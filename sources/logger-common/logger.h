// logger/logger.h
//
// Публичный интерфейс — реализован буквально по ТЗ. Приватные детали
// расширены (m_initialized, m_fileMutex, m_fileSinkBroken, m_initMutex) —
// это не часть контракта, а внутренняя реализация принятых решений:
//
//  - log()/trace()/... до init()            -> молча игнорируется
//  - повторный init()                        -> идемпотентен (второй и
//                                                последующие вызовы игнорируются)
//  - ошибка открытия файла лога              -> логирование продолжается
//                                                только в stderr, начиная
//                                                с момента этой ошибки
//  - SLogRecord::timestamp                   -> unix epoch в секундах,
//                                                через time(nullptr)
//                                                (не путать с steady_clock
//                                                в CQueue — это разные часы
//                                                для разных целей)

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "log_level.h"

class CLogger
{
public:
    static CLogger& instance();

    void init(const std::string& logFilePath, ELogLevel minLevel = ELogLevel::LL_INFO);
    void shutdown(); // дожидается опустошения очереди, останавливает поток-писатель

    template <typename... Args>
    void log(ELogLevel level, std::string_view component, std::string_view fmt, Args&&... args)
    {
        // не инициализирован (init() ещё не вызывался или уже прошёл shutdown()) -> молча игнорируем
        if (!m_running.load(std::memory_order_acquire))
            return;

        if (level < m_minLevel)
            return;

        SLogRecord record;
        record.level     = level;
        record.component = std::string(component);
        record.message   = formatMessage(fmt, std::forward<Args>(args)...);
        record.threadId  = std::this_thread::get_id();
        record.timestamp = currentUnixTimeSeconds();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_queue.push_back(std::move(record));
        }
        m_queueCv.notify_one();
    }

    template <typename... Args>
    void trace(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_TRACE, component, fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    void debug(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_DEBUG, component, fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    void info(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_INFO, component, fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    void warn(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_WARN, component, fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    void error(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_ERROR, component, fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    void critical(std::string_view component, std::string_view fmt, Args&&... args)
    { log(ELogLevel::LL_CRITICAL, component, fmt, std::forward<Args>(args)...); }

    CLogger(const CLogger&) = delete;
    CLogger& operator=(const CLogger&) = delete;

private:
    CLogger() = default;
    ~CLogger();

    struct SLogRecord
    {
        ELogLevel        level{ELogLevel::LL_INFO};
        std::string      component;
        std::string      message;
        std::thread::id  threadId{};
        uint64_t         timestamp{0}; // unix epoch, секунды
    };

    void writerLoop();
    void writeToSinks(const SLogRecord& record);
    void rotateIfNeeded();
    bool openFileSink(); // false, если файл не открылся - тогда пишем только в stderr

    static uint64_t currentUnixTimeSeconds();

    template <typename T>
    static void formatOne(std::ostringstream& oss, std::string_view& fmt, const T& value)
    {
        const auto pos = fmt.find("{}");
        if (pos == std::string_view::npos)
        {
            oss << fmt;
            fmt = std::string_view{};
            return;
        }
        oss << fmt.substr(0, pos) << value;
        fmt.remove_prefix(pos + 2);
    }

    template <typename... Args>
    static std::string formatMessage(std::string_view fmt, Args&&... args)
    {
        std::ostringstream oss;
        (formatOne(oss, fmt, args), ...); // левый-направо, C++17 fold
        oss << fmt;                        // остаток строки после последнего "{}"
        return oss.str();
    }

    std::mutex               m_queueMutex;
    std::condition_variable   m_queueCv;
    std::deque<SLogRecord>   m_queue;
    std::thread              m_writerThread;
    std::atomic<bool>        m_running{false}; // true строго между init() и shutdown()

    ELogLevel                 m_minLevel{ELogLevel::LL_INFO};
    std::ofstream              m_fileSink;
    std::string                m_logFilePath;
    size_t                      m_currentFileBytes{0};


    std::mutex   m_initMutex;
    std::atomic<bool> m_initialized{false};
    std::mutex   m_fileMutex;
    bool         m_fileSinkBroken{false};
};
