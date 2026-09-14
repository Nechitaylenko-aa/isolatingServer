// logger/logger.cpp

#include "logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>

namespace
{
constexpr size_t LOG_FILE_MAX_BYTES   = 50 * 1024 * 1024; // 50 МБ
constexpr int    LOG_FILE_MAX_BACKUPS = 5;

const char* levelToString(ELogLevel level)
{
    switch (level)
    {
        case ELogLevel::LL_TRACE:    return "TRACE";
        case ELogLevel::LL_DEBUG:    return "DEBUG";
        case ELogLevel::LL_INFO:     return "INFO ";
        case ELogLevel::LL_WARN:     return "WARN ";
        case ELogLevel::LL_ERROR:    return "ERROR";
        case ELogLevel::LL_CRITICAL: return "CRIT ";
    }
    return "?????";
}
} // namespace

CLogger& CLogger::instance()
{
    static CLogger logger;
    return logger;
}

CLogger::~CLogger()
{
    shutdown();
}

uint64_t CLogger::currentUnixTimeSeconds()
{
    // unix epoch, секунды - как в protocol.md. Намеренно time(nullptr),
    // а не steady_clock (тот используется в CQueue для планирования, это
    // другая единица измерения времени с другим назначением).
    return static_cast<uint64_t>(std::time(nullptr));
}

void CLogger::init(const std::string& logFilePath, ELogLevel minLevel)
{
    std::lock_guard<std::mutex> lock(m_initMutex);

    // повторный init() - идемпотентен, второй и последующие вызовы игнорируются
    if (m_initialized.load(std::memory_order_acquire))
        return;

    m_logFilePath = logFilePath;
    m_minLevel = minLevel;
    m_currentFileBytes = 0;

    {
        std::lock_guard<std::mutex> fileLock(m_fileMutex);
        if (!m_logFilePath.empty())
            m_fileSinkBroken = !openFileSink();
    }

    m_running.store(true, std::memory_order_release);
    m_writerThread = std::thread(&CLogger::writerLoop, this);

    m_initialized.store(true, std::memory_order_release);
}

void CLogger::shutdown()
{
    // не был инициализирован либо уже остановлен - no-op
    if (!m_running.exchange(false, std::memory_order_acq_rel))
        return;

    m_queueCv.notify_all();

    if (m_writerThread.joinable())
        m_writerThread.join();

    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_fileSink.is_open())
        m_fileSink.flush();
}

bool CLogger::openFileSink()
{
    m_fileSink.open(m_logFilePath, std::ios::out | std::ios::app);
    if (!m_fileSink.is_open())
    {
        std::cerr << "[CLogger] не удалось открыть файл лога '" << m_logFilePath
                  << "', логирование продолжится только в stderr" << '\n';
        return false;
    }

    std::error_code ec;
    m_currentFileBytes = std::filesystem::exists(m_logFilePath, ec)
                              ? std::filesystem::file_size(m_logFilePath, ec)
                              : 0;
    return true;
}

void CLogger::writerLoop()
{
    while (true)
    {
        std::deque<SLogRecord> batch;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this] {
                return !m_queue.empty() || !m_running.load(std::memory_order_acquire);
            });

            if (m_queue.empty() && !m_running.load(std::memory_order_acquire))
                break; // остановка запрошена и очередь пуста - можно выходить

            batch.swap(m_queue);
        }

        for (const auto& record : batch)
            writeToSinks(record);
    }
}

void CLogger::writeToSinks(const SLogRecord& record)
{
    std::time_t t = static_cast<std::time_t>(record.timestamp);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif

    std::ostringstream line;
    line << '[' << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << ']'
         << '[' << levelToString(record.level) << ']'
         << "[thread:0x" << std::hex << std::hash<std::thread::id>{}(record.threadId) << std::dec << ']'
         << '[' << record.component << "] "
         << record.message;

    const std::string formatted = line.str();

    // консоль - всегда, независимо от состояния файлового синка
    std::cerr << formatted << '\n';

    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_fileSinkBroken)
        return; // писать в файл некуда - в stderr уже записано выше

    rotateIfNeeded();

    if (!m_fileSink.is_open())
        return;

    m_fileSink << formatted << '\n';
    m_fileSink.flush();
    m_currentFileBytes += formatted.size() + 1;
}

void CLogger::rotateIfNeeded()
{
    if (m_fileSinkBroken || m_currentFileBytes < LOG_FILE_MAX_BYTES)
        return;

    m_fileSink.close();

    std::error_code ec;

    // удаляем самый старый бэкап, если он есть
    const std::string oldest = m_logFilePath + "." + std::to_string(LOG_FILE_MAX_BACKUPS);
    if (std::filesystem::exists(oldest, ec))
        std::filesystem::remove(oldest, ec);

    // сдвигаем остальные: .(N-1) -> .N, ..., .1 -> .2
    for (int i = LOG_FILE_MAX_BACKUPS - 1; i >= 1; --i)
    {
        const std::string from = m_logFilePath + "." + std::to_string(i);
        const std::string to   = m_logFilePath + "." + std::to_string(i + 1);
        if (std::filesystem::exists(from, ec))
            std::filesystem::rename(from, to, ec);
    }

    // текущий файл -> .1
    if (std::filesystem::exists(m_logFilePath, ec))
        std::filesystem::rename(m_logFilePath, m_logFilePath + ".1", ec);

    m_currentFileBytes = 0;
    m_fileSinkBroken = !openFileSink();
}
