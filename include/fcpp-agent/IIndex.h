/**
 * @file IIndex.h
 * @brief Интерфейс индексатора C++ (SQLite + libclang).
 *
 * Индекс является единственным детерминированным источником знаний о C++
 * структуре проекта. Реализация обязана следовать принципам Zero-Search
 * (никакого поиска по дереву) и Zero-Window (в LLM передаются только
 * минимальные срезы кода).
 */

#ifndef FCPP_AGENT_IINDEX_H
#define FCPP_AGENT_IINDEX_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fcpp
{

class IClang;
struct SCompileCommand;

/**
 * @brief Состояние персистентного индекса проекта.
 *
 * Преобразование в текст выполняется статическим массивом в .cpp
 * (Section 3 rules.md), без switch-case.
 */
enum class EIndexState
{
    NotReady, ///< Индекс отсутствует.
    Building, ///< Идёт построение или обновление.
    Ready,    ///< Индекс актуален.
    Failed,   ///< Построение завершилось ошибкой.
    Count     ///< Маркер количества для инициализации массива строк.
};

/**
 * @brief Категория индексируемой сущности C++.
 *
 * Преобразование в текст выполняется статическим массивом в .cpp
 * (Section 3 rules.md), без switch-case.
 */
enum class ESymbolKind
{
    Namespace,     ///< Пространство имён.
    Class,         ///< Класс.
    Struct,        ///< Структура.
    Union,         ///< Объединение.
    Enum,          ///< Перечисление.
    EnumConstant,  ///< Константа перечисления.
    Typedef,       ///< typedef / using / alias.
    Function,      ///< Свободная функция.
    Method,        ///< Метод класса.
    Constructor,   ///< Конструктор.
    Destructor,    ///< Деструктор.
    Variable,      ///< Глобальная/статическая переменная.
    Field,         ///< Поле класса.
    Parameter,     ///< Параметр функции.
    Count          ///< Маркер количества для инициализации массива строк.
};

/**
 * @brief Диапазон исходного кода: координаты libclang 1-базовые, конечная позиция не включается.
 */
struct SSourceRange
{
    std::uint32_t m_line = 0;       ///< Начальная строка.
    std::uint32_t m_column = 0;     ///< Начальная колонка.
    std::uint32_t m_end_line = 0;   ///< Конечная строка.
    std::uint32_t m_end_column = 0; ///< Конечная колонка.
};

/**
 * @brief Позиция сущности в конкретном файле проекта.
 */
struct SSymbolLocation
{
    std::string m_file;   ///< Путь файла относительно корня проекта.
    SSourceRange m_range; ///< Диапазон исходного кода.
};

/**
 * @brief Индексируемая сущность C++.
 */
struct SSymbol
{
    std::string m_usr;             ///< Стабильный идентификатор clang (USR).
    ESymbolKind m_kind;            ///< Категория сущности.
    std::string m_name;            ///< Короткое имя.
    std::string m_qualified_name;  ///< Полное имя с квалификацией.
    std::string m_signature;       ///< Сигнатура (без тела).
    std::string m_access;          ///< public/protected/private.
    std::string m_parent_usr;      ///< USR семантического родителя.
    bool m_is_definition = false;  ///< Является ли определением.
    bool m_is_virtual = false;     ///< Виртуальный метод.
    bool m_is_pure_virtual = false;///< Чисто виртуальный метод.
    bool m_is_static = false;      ///< Статический член/функция.
    SSymbolLocation m_location;    ///< Позиция объявления/определения.
};

/**
 * @brief Место использования символа (только позиция).
 */
struct SReference
{
    std::string m_file;         ///< Путь к файлу.
    std::uint32_t m_line = 0;   ///< Строка.
    std::uint32_t m_column = 0; ///< Колонка.
};

/**
 * @brief Снимок состояния индекса (без загрузки данных).
 */
struct SIndexStatus
{
    EIndexState m_state = EIndexState::NotReady; ///< Текущее состояние.
    std::uint64_t m_generation = 0;  ///< Активная generation индекса.
    std::uint64_t m_file_count = 0;  ///< Число проиндексированных файлов.
    std::uint64_t m_symbol_count = 0;///< Число проиндексированных сущностей.
    std::uint64_t m_stale_files = 0; ///< Число потенциально устаревших файлов.
};

/**
 * @brief Минимальный срез исходного кода (Zero-Window).
 *
 * Передаётся в LLM вместо файла целиком: только тело запрошенного метода
 * либо сигнатура класса/enum.
 */
struct SSourceSlice
{
    std::string m_usr;    ///< Идентификатор сущности, к которой относится срез.
    std::string m_text;   ///< Текст среза.
    SSourceRange m_range; ///< Диапазон среза в исходном файле.
};

/**
 * @brief Интерфейс индексатора C++ проекта.
 *
 * Все операции чтения обслуживаются персистентным индексом (SQLite).
 * Обновление индекса выполняется детерминированно, без сканирования дерева
 * проекта за пределами известных translation units.
 */
class IIndex
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IIndex() = default;

    /**
     * @brief Открыть индекс для проекта.
     * @param project_root Канонический путь CMake-корня проекта.
     * @return true при успешном открытии; false при ошибке конфигурации.
     */
    virtual bool open(const std::filesystem::path& project_root) = 0;

    /**
     * @brief Закрыть индекс и освободить ресурсы.
     */
    virtual void close() = 0;

    /**
     * @brief Получить снимок состояния индекса.
     * @return Структура состояния без загрузки данных.
     */
    virtual SIndexStatus status() const = 0;

    /**
     * @brief Обновить индекс в рамках активной generation.
     * @param clang Backend доступа к libclang.
     * @return true при успешном обновлении; false при ошибке.
     */
    virtual bool refresh(IClang& clang) = 0;

    /**
     * @brief Полностью перестроить индекс в новую generation.
     * @param clang Backend доступа к libclang.
     * @param commands Команды компиляции для новой generation.
     * @return true при успешном построении; false при ошибке.
     */
    virtual bool rebuild(IClang& clang, const std::vector<SCompileCommand>& commands) = 0;

    /**
     * @brief Найти сущность по стабильному идентификатору clang (USR).
     * @param usr Идентификатор сущности.
     * @return Сущность либо пустое значение.
     */
    virtual std::optional<SSymbol> find_symbol_by_usr(const std::string& usr) const = 0;

    /**
     * @brief Найти сущности по короткому/полному имени.
     * @param name Имя сущности.
     * @return Список совпавших сущностей.
     */
    virtual std::vector<SSymbol> find_symbols_by_name(const std::string& name) const = 0;

    /**
     * @brief Получить структуру сущности (для класса — методы и поля).
     * @param usr Идентификатор сущности.
     * @return Дочерние сущности.
     */
    virtual std::vector<SSymbol> outline(const std::string& usr) const = 0;

    /**
     * @brief Получить функции/методы, вызывающие указанную сущность.
     * @param usr Идентификатор сущности.
     * @return Список вызывающих сущностей.
     */
    virtual std::vector<SSymbol> callers(const std::string& usr) const = 0;

    /**
     * @brief Получить сущности, вызываемые указанной функцией/методом.
     * @param usr Идентификатор сущности.
     * @return Список вызываемых сущностей.
     */
    virtual std::vector<SSymbol> callees(const std::string& usr) const = 0;

    /**
     * @brief Получить места использования сущности.
     * @param usr Идентификатор сущности.
     * @return Список позиций использования.
     */
    virtual std::vector<SSymbolLocation> references(const std::string& usr) const = 0;

    /**
     * @brief Получить минимальный срез исходного кода сущности.
     * @param usr Идентификатор сущности.
     * @return Срез кода либо пустое значение.
     */
    virtual std::optional<SSourceSlice> get_source_slice(const std::string& usr) const = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_IINDEX_H
