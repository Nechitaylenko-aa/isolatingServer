# Логирование

Единый компонент логирования, используемый всеми частями сервера (см. `architecture.md`, `server.md`, `database.md`).

## Требования

- Многопоточная среда: N воркеров + DB-поток + главный поток + пул матчинга + admission control — все могут писать в лог одновременно.
- Запись в лог не должна блокировать воркер/DB-поток на диске — используется асинхронная запись через отдельный поток-писатель.
- Уровни важности, вывод в консоль (stderr) и в файл с ротацией по размеру.
- Простой API, без внешних зависимостей (либо допускается использование готовой header-only библиотеки типа spdlog — уточняется на этапе реализации).

## Уровни

```cpp
enum class ELogLevel : uint8_t
{
    LL_TRACE,
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRITICAL
};
```

Минимальный уровень вывода настраивается на старте (по умолчанию `LL_INFO`, в отладке — `LL_DEBUG`).

## Интерфейс

```cpp
class CLogger
{
public:
    static CLogger& instance();

    void init(const std::string& logFilePath, ELogLevel minLevel = ELogLevel::LL_INFO);
    void shutdown(); // дожидается сброса очереди и останавливает поток-писатель

    template <typename... Args>
    void log(ELogLevel level, std::string_view component, std::string_view fmt, Args&&... args);

    // удобные обёртки
    template <typename... Args> void trace(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void debug(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void info(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void warn(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void error(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void critical(std::string_view component, std::string_view fmt, Args&&... args);

private:
    CLogger() = default;

    struct SLogRecord
    {
        ELogLevel        level;
        std::string      component;
        std::string      message;
        std::thread::id  threadId;
        uint64_t         timestamp; // unix epoch, как в protocol.md
    };

    void writerLoop();       // работает в отдельном потоке, вычитывает очередь и пишет в sink'и
    void writeToSinks(const SLogRecord&);

    std::mutex               m_queueMutex;
    std::condition_variable   m_queueCv;
    std::deque<SLogRecord>   m_queue;
    std::thread              m_writerThread;
    std::atomic<bool>        m_running{false};

    ELogLevel                 m_minLevel{ELogLevel::LL_INFO};
    std::ofstream              m_fileSink;
    std::mutex                 m_fileMutex; // на случай прямой записи вне очереди при shutdown
};
```

`instance()` — синглтон, инициализируется один раз в `CServer::run()` до старта остальных потоков; `shutdown()` вызывается последним, чтобы не потерять сообщения из воркеров/DB-потока при завершении.

## Формат записи

```
[YYYY-MM-DD HH:MM:SS][LEVEL][thread:0x...][component] message
```

Пример:
```
[2026-08-12 14:03:11][ERROR][thread:0x7f2a1c][CThreadPool] unhandled exception in task: bad_alloc
[2026-08-12 14:03:41][WARN ][thread:0x7f2a03][CEquipHandlerRegistry] handler not found for natureType=0 componentType=0
[2026-08-12 14:04:00][INFO ][thread:0x7f2a00][CAdmissionControl] db health check: OK
```

## Ротация файла

Простая ротация по размеру (без внешних зависимостей на первом этапе):

```cpp
constexpr size_t LOG_FILE_MAX_BYTES = 50 * 1024 * 1024; // 50 МБ
constexpr int    LOG_FILE_MAX_BACKUPS = 5;
```

При достижении лимита текущий файл переименовывается (`server.log` → `server.log.1`, сдвиг остальных), открывается новый `server.log`.

## Точки интеграции (кто и что логирует)

| Компонент | События |
|---|---|
| `CAcceptor` | приём/отказ в приёме соединения (WARN при отказе из-за admission control) |
| `CHeaderValidator` | каждая невалидация заголовка — с указанием причины (DEBUG/WARN) |
| `CEquipQueryDispatcher` / `CEquipHandlerRegistry` | отсутствие хендлера для типа компонента (WARN) |
| `CThreadPool` | необработанное исключение в задаче (ERROR) |
| `CDBThread` / `CDatabaseModel` | ошибки запроса, потеря соединения, успешное переподключение (ERROR/WARN/INFO) |
| `CAdmissionControl` | смена состояния БД healthy/unhealthy (WARN при переходе в unhealthy, INFO при восстановлении) |
| `CServer` | старт/остановка сервера, порт, число воркеров (INFO) |

## Открытые вопросы

- Финальный выбор: собственная реализация vs. header-only библиотека (spdlog) — не критично для архитектуры, решается на этапе реализации.
- Формат ротации файлов на FreeBSD/Linux не отличается — специфики ОС не требует `#ifdef`.
