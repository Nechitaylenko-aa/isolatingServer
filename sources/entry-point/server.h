#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "../logger-common/logger.h"
#include "../include/CThreadPool.h"
#include "../include/CQueue.h"
#include "../protocol/packet_cipher.h"
#include "../protocol/signature_verifier.h"
#include "../protocol/time_check_registry.h"
#include "../protocol/request_pipeline.h"
#include "../protocol/packet_serializer.h"
#include "../database-layer/db_config.h"
#include "../database-layer/db_thread.h"
#include "../equip-match/equip_handler_registry.h"
#include "../equip-match/equip_query_dispatcher.h"
#include "../admission/admission_control.h"
#include "../network-layer/CActiveClientRegistry.h"
#include "../network-layer/CMainCoordinator.h"
#include "../network-layer/CWorker.h"
#include "../network-layer/CAcceptor.h"

class CServer
{
public:
    CServer(uint16_t port, size_t workerCount = std::thread::hardware_concurrency());
    ~CServer();

    CServer(const CServer&) = delete;
    CServer& operator=(const CServer&) = delete;

    void run();  // блокирует вызывающий поток (acceptor-цикл), возвращается после stop()
    void stop(); // потокобезопасно, идемпотентно; можно звать из сигнал-хендлера

private:
    static SDBConnection buildDBConfig(); // §fix-2: заполняется ДО конструирования m_dbThread,
                                            // т.к. member-init-list выполняется раньше тела ctor

    void buildPipeline();   // §3.2
    void buildWorkers();    // §3.3 - двухфазная сборка (coordinator=nullptr -> set_coordinator())
    void startAll();        // §3.4
    void stopAll();          // §3.5, строго обратный startAll() порядок

    uint16_t     m_port;
    size_t        m_workerCount;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    // --- Группа 1 ---
    CThreadPool   m_matchPool;
    CQueue          m_systemQueue{&m_matchPool};

    // --- Группа 2 (стейтлесс/разделяемые компоненты, по одному экземпляру на сервер) ---
    CNullPacketCipher       m_cipher;
    CNullSignatureVerifier   m_sigVerifier;
    CTimeCheckRegistry         m_timeCheck;
    std::unique_ptr<CRequestPipeline>  m_pipeline;   // строится в buildPipeline() - нужен m_activeClients
    std::unique_ptr<CPacketSerializer>  m_serializer; // отдельный экземпляр для прямого использования CWorker'ом

    // --- Группа 3 ---
    SDBConnection   m_dbConfig;
    CDBThread          m_dbThread;

    // --- Группа 4 ---
    CEquipHandlerRegistry     m_equipRegistry;
    std::unique_ptr<CEquipQueryDispatcher> m_equipDispatcher; // нужен m_dbThread+m_matchPool, строится в конструкторе

    // --- Группа 5 ---
    std::unique_ptr<CAdmissionControl> m_admissionControl; // нужен m_systemQueue+m_dbThread

    // --- Группа 6 ---
    CActiveClientRegistry                        m_activeClients;
    std::vector<std::unique_ptr<CWorker>>          m_workers;
    std::vector<std::thread>                         m_workerThreads;
    std::unique_ptr<CMainCoordinator>                  m_coordinator;
    std::unique_ptr<CAcceptor>                           m_acceptor;
};
