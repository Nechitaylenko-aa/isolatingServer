//
// Created by artem on 16.06.26.
//

#ifndef NYM_PROJECT_LOGGER_H
#define NYM_PROJECT_LOGGER_H


#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>


enum ELogLevel : uint32_t {
    LOG_NONE     = 0,
    LOG_CRITICAL = 1 << 0,  // 0x01
    LOG_ERROR    = 1 << 1,  // 0x02
    LOG_WARNING  = 1 << 2,  // 0x04
    LOG_INFO     = 1 << 3,  // 0x08
    LOG_DEBUG    = 1 << 4,  // 0x10
    LOG_TRACE    = 1 << 5,  // 0x20

    LOG_QUIET    = LOG_CRITICAL | LOG_ERROR,
    LOG_NORMAL   = LOG_CRITICAL | LOG_ERROR | LOG_WARNING | LOG_INFO,
    LOG_VERBOSE  = LOG_CRITICAL | LOG_ERROR | LOG_WARNING | LOG_INFO | LOG_DEBUG,
    LOG_ALL      = 0xFFFFFFFF
};


inline ELogLevel operator|(ELogLevel a, ELogLevel b) {
    return static_cast<ELogLevel>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ELogLevel operator&(ELogLevel a, ELogLevel b) {
    return static_cast<ELogLevel>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool isLevelEnabled(ELogLevel mask, ELogLevel level) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(level)) != 0;
}

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Инициализация с битовой маской
    void init(const std::string& log_file = "log.txt",
              ELogLevel enabled_levels = LOG_NORMAL,   // <-- маска
              bool output_to_console = true,
              bool output_to_file = true)
    {
        if (m_running || m_worker.joinable())
        {
            return;
        }
        m_log_file = log_file;
        m_enabled_levels = enabled_levels;
        m_output_to_console = output_to_console;
        m_output_to_file = output_to_file;
        m_running = true;
        m_worker = std::thread(&Logger::workerThread, this);
    }

    void shutdown()
    {
        m_running = false;
        m_cv.notify_one();
        if (m_worker.joinable()) {
            m_worker.join();
        }
        flush();
    }

    // Основной метод логирования
    void log(ELogLevel level, const std::string& message) {
        // Проверяем, разрешен ли этот уровень
        if (!isLevelEnabled(m_enabled_levels, level)) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_queue.emplace(level, getTimestamp(), message, getThreadId());
        m_cv.notify_one();
    }

    // обертки
    void critical(const std::string& msg) { log(LOG_CRITICAL, msg); }
    void error(const std::string& msg)    { log(LOG_ERROR, msg); }
    void warn(const std::string& msg)     { log(LOG_WARNING, msg); }
    void info(const std::string& msg)     { log(LOG_INFO, msg); }
    void debug(const std::string& msg)    { log(LOG_DEBUG, msg); }
    void trace(const std::string& msg)    { log(LOG_TRACE, msg); }

    // Управление выводом
    void setConsoleOutput(bool enable) { m_output_to_console = enable; }
    void setFileOutput(bool enable)    { m_output_to_file = enable; }
    void setEnabledLevels(ELogLevel levels) { m_enabled_levels = levels; }
    ELogLevel getEnabledLevels() const { return m_enabled_levels; }

private:
    Logger() = default;
    ~Logger() { shutdown(); }

    struct LogEntry {
        ELogLevel level;
        std::string timestamp;
        std::string thread_id;
        std::string message;

        LogEntry(ELogLevel lvl, std::string  ts, const std::string& msg, const std::string& tid)
                : level(lvl), timestamp(std::move(ts)), thread_id(tid), message(msg) {}
    };

    std::queue<LogEntry> m_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_running{false};

    std::string m_log_file;
    ELogLevel m_enabled_levels{LOG_NORMAL};
    std::atomic<bool> m_output_to_console{true};
    std::atomic<bool> m_output_to_file{true};

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string getThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }

    std::string levelToString(ELogLevel level) {
        switch (level)
        {
            case LOG_CRITICAL: return "CRIT";
            case LOG_ERROR:    return "ERROR";
            case LOG_WARNING:  return "WARN";
            case LOG_INFO:     return "INFO";
            case LOG_DEBUG:    return "DEBUG";
            case LOG_TRACE:    return "TRACE";
            default:           return "UNKN";
        }
    }

    std::string levelToColor(ELogLevel level) {
        switch (level) {
            case LOG_TRACE:    return "\033[37m";   // Белый
            case LOG_DEBUG:    return "\033[36m";   // Циан
            case LOG_INFO:     return "\033[32m";   // Зеленый
            case LOG_WARNING:  return "\033[33m";   // Желтый
            case LOG_ERROR:    return "\033[31m";   // Красный
            case LOG_CRITICAL: return "\033[35m";   // Магента
            default:           return "\033[0m";
        }
    }

    void writeToConsole(const LogEntry& entry)
    {
        if (!m_output_to_console) return;

        std::string color = levelToColor(entry.level);
        std::string reset = "\033[0m";

        std::cout << color
                  << entry.timestamp << " ["
                  << levelToString(entry.level) << "] "
                  << "[T:" << entry.thread_id << "] "
                  << entry.message
                  << reset << std::endl;
    }

    void writeToFile(const LogEntry& entry) {
        if (!m_output_to_file || m_log_file.empty()) return;

        std::ofstream file(m_log_file, std::ios::app);
        if (file.is_open()) {
            file << entry.timestamp << " ["
                 << levelToString(entry.level) << "] "
                 << "[T:" << entry.thread_id << "] "
                 << entry.message << std::endl;
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_queue.empty())
        {
            const auto& entry = m_queue.front();
            writeToConsole(entry);
            writeToFile(entry);
            m_queue.pop();
        }
    }

    void workerThread()
    {
        while (m_running)
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(100), [this]()
            {
                return !m_queue.empty() || !m_running;
            });

            if (!m_queue.empty())
            {
                std::queue<LogEntry> batch;
                std::swap(batch, m_queue);
                lock.unlock();

                while (!batch.empty())
                {
                    const auto& entry = batch.front();
                    writeToConsole(entry);
                    writeToFile(entry);
                    batch.pop();
                }
            }
        }
        flush();
    }
};

// Макросы с проверкой уровня (для отключения в Release)
#define LOG_CRITICAL(msg) Logger::instance().critical(msg)
#define LOG_ERROR(msg)    Logger::instance().error(msg)
#define LOG_WARN(msg)     Logger::instance().warn(msg)
#define LOG_INFO(msg)     Logger::instance().info(msg)
#define LOG_DEBUG(msg)    Logger::instance().debug(msg)
#define LOG_TRACE(msg)    Logger::instance().trace(msg)


#endif //NYM_PROJECT_LOGGER_H
