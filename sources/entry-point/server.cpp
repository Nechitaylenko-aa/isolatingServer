#include "server.h"
#include "bootstrap.h"

SDBConnection CServer::buildDBConfig()
{
    SDBConnection cfg{};
    setupDBconfig(cfg);
    return cfg;
}

CServer::CServer(uint16_t port, size_t workerCount)
    : m_port(port)
    , m_workerCount(workerCount == 0 ? 1 : workerCount)
    , m_matchPool(m_workerCount)
    , m_dbConfig(buildDBConfig())   // §fix-2: заполнен ДО того, как m_dbThread его скопирует
    , m_dbThread(m_dbConfig)
{
    // logger.md: init() — до старта любых потоков, которые реально шлют логи
    // (потоки CThreadPool/CQueue/CDBThread ещё не запущены — start() ниже).
    CLogger::instance().init("server.log", ELogLevel::LL_INFO);

    m_equipDispatcher = std::make_unique<CEquipQueryDispatcher>(m_equipRegistry, m_matchPool, m_dbThread);
    registerEquipHandlers(m_equipRegistry);
    m_admissionControl = std::make_unique<CAdmissionControl>(m_systemQueue, m_dbThread);

    buildPipeline();
    buildWorkers();
}

CServer::~CServer()
{
    stop();
}

void CServer::buildPipeline()
{
    m_pipeline   = std::make_unique<CRequestPipeline>(m_activeClients, m_timeCheck, m_cipher, m_sigVerifier);
    m_serializer = std::make_unique<CPacketSerializer>(m_cipher);
}

void CServer::buildWorkers()
{
    // Фаза 1: создать воркеров (реальная сигнатура CWorker - 7 параметров, БЕЗ coordinator).
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_workerCount); ++i)
    {
        m_workers.push_back(std::make_unique<CWorker>(
            i, *m_pipeline, *m_serializer, m_activeClients,
            *m_admissionControl, m_dbThread, *m_equipDispatcher));
    }

    // Фаза 2: собрать raw-указатели, создать координатор.
    std::vector<CWorker*> rawWorkers;
    rawWorkers.reserve(m_workers.size());
    for (auto& w : m_workers) rawWorkers.push_back(w.get());
    m_coordinator = std::make_unique<CMainCoordinator>(m_systemQueue, rawWorkers);

    // Фаза 3: связать воркеров с координатором постфактум (метод называется set_coordinator).
    for (auto& w : m_workers) w->set_coordinator(m_coordinator.get());

    // Фаза 4: акцептор - воркеры уже существуют, циклической зависимости нет.
    m_acceptor = std::make_unique<CAcceptor>(m_port, rawWorkers);
}

void CServer::startAll()
{
    m_dbThread.start();              // соединение с БД поднимается первым
    m_admissionControl->start();
    m_systemQueue.start();            // periodic tasks: admission control health-poll, coordinator redistribute
    m_coordinator->start();
    for (auto& w : m_workers)
    {
        CWorker* raw = w.get();
        m_workerThreads.emplace_back([raw] { raw->run(); });
    }
    // acceptor НЕ стартуется здесь - см. run()
}

void CServer::run()
{
    if (m_running.exchange(true)) return; // повторный run() - no-op

    if (m_stopRequested.load())
    {
        // stop() был вызван до run() - ничего не запускаем.
        m_running.store(false);
        return;
    }

    startAll();
    m_acceptor->run();   // БЛОКИРУЕТ вызывающий поток (accept()-цикл) - "acceptor-поток" из server.md
    // возврат сюда происходит только после m_acceptor->stop() (вызван из stop(), с другого потока)
    stopAll();
    m_running.store(false);
}

void CServer::stop()
{
    if (m_stopRequested.exchange(true)) return; // идемпотентно

    if (m_acceptor)
        m_acceptor->stop();  // разблокирует accept() в run(), дальше stopAll() вызывается из run()
}

void CServer::stopAll()
{
    for (auto& w : m_workers) w->stop();
    for (auto& t : m_workerThreads) if (t.joinable()) t.join();

    m_coordinator->stop();
    m_systemQueue.stop();
    m_admissionControl->stop();
    m_dbThread.stop();

    CLogger::instance().shutdown(); // строго последним, logger.md
}
