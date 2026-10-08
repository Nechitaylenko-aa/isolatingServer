/**
 * @file main.cpp
 * @brief Точка входа CLI fcpp-agent — тонкий клиент Agent API (§93).
 *
 * CLI не вызывает Agent Core напрямую: каждая команда транслируется в
 * типизированный SApiRequest и выполняется через CAgentApi.
 */

#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/CAgentApi.h"
#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "core/CRequestPipeline.h"
#include "fcpp-agent/IAgentApi.h"
#include "fcpp-agent/SIntent.h"
#include "fcpp-agent/SRequestOutcome.h"
#include "fcpp-agent/ITool.h"
#include "fcpp-agent/SError.h"

namespace
{

/** @brief Имена состояний индекса, индексированные по EIndexState. */
static const char* const m_index_state_names[static_cast<std::size_t>(fcpp::EIndexState::Count)]
{
    "NotReady",
    "Building",
    "Ready",
    "Failed"
};

/**
 * @brief Вывести лаконичную справку по командам (ADR-01).
 * @param program Имя исполняемого файла (argv[0]).
 */
void print_usage(const std::string& program)
{
    std::cout << "fcpp-agent: детерминированный C++/CMake-агент (CLI = клиент Agent API)\n\n";
    std::cout << "Использование: " << program << " <команда> [опции]\n\n";
    std::cout << "Агент и проекты:\n";
    std::cout << "  init                            Инициализировать каталоги и конфиг\n";
    std::cout << "  reload                          Перечитать agent.conf\n";
    std::cout << "  doctor                          Проверка инструментов (ADR-06)\n";
    std::cout << "  route --area A --relation R --projection P [--subject S] [--project PATH]\n";
    std::cout << "  open <path>                     Открыть проект\n";
    std::cout << "  close                           Закрыть проект\n";
    std::cout << "  projects                        Список проектов\n";
    std::cout << "  status                          Состояние проекта и индекса\n";
    std::cout << "  index <status|refresh|rebuild> [<path>]\n";
    std::cout << "  build                           Сборка (cmake --build)\n";
    std::cout << "  test                            Тесты (ctest)\n";
    std::cout << "  patch <file|-> [--yes]          Применить unified diff\n";
    std::cout << "\nДетерминированные запросы (tools):\n";
    std::cout << "  outline|callers|callees|refs|includes|overrides|dispatch\n";
    std::cout << "  def|ancestors|derived|search|source <symbol> [<path>]\n";
    std::cout << "\nОпции: --config PATH --project PATH --compile-commands PATH --yes --json --verbose\n";
    std::cout << "Не реализовано: ask\n";
}

/**
 * @brief Обработать команду init.
 * @return Код возврата процесса.
 */
int run_init()
{
    fcpp::CConfigManager manager;
    if (!manager.bootstrap())
    {
        std::cerr << "fcpp-agent: ошибка инициализации конфигурации\n";
        return 1;
    }

    if (!manager.load())
    {
        std::cerr << "fcpp-agent: ошибка чтения конфигурации: " << manager.config_path().string() << '\n';
        return 1;
    }

    std::cout << "fcpp-agent: инициализация завершена\n";
    std::cout << "  каталог агента: " << manager.agent_dir().string() << '\n';
    std::cout << "  конфигурация:   " << manager.config_path().string() << '\n';

    return 0;
}

/**
 * @brief Прочитать текст патча из файла или stdin ("-").
 * @param source Путь к файлу либо "-".
 * @return Текст патча.
 */
std::string read_patch_text(const std::string& source)
{
    if (source == "-")
    {
        std::ostringstream buffer;
        buffer << std::cin.rdbuf();
        return buffer.str();
    }

    std::ifstream input(source, std::ios::binary);
    if (!input.is_open())
    {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

/**
 * @brief Напечатать результат инструмента.
 * @param response Ответ API.
 * @return Код возврата.
 */
int print_tool_response(const fcpp::SApiResponse& response)
{
    if (!response.m_ok)
    {
        std::cerr << "fcpp-agent: " << fcpp::error_kind_name(response.m_error)
                  << ": " << response.m_message << '\n';
        return 1;
    }

    bool printed = false;
    for (const fcpp::SSymbol& symbol : response.m_symbols)
    {
        std::cout << fcpp::symbol_kind_name(symbol.m_kind) << "  " << symbol.m_qualified_name;
        if (!symbol.m_location.m_file.empty())
        {
            std::cout << "  [" << symbol.m_location.m_file << "]";
        }
        std::cout << '\n';
        printed = true;
    }
    for (const std::string& name : response.m_names)
    {
        std::cout << name << '\n';
        printed = true;
    }
    for (const fcpp::SSymbolLocation& location : response.m_locations)
    {
        std::cout << location.m_file << ":" << location.m_range.m_line << ":"
                  << location.m_range.m_column << '\n';
        printed = true;
    }
    if (!response.m_text.empty())
    {
        std::cout << response.m_text;
        printed = true;
    }
    if (!printed)
    {
        std::cout << "(пусто)\n";
    }
    return 0;
}

/**
 * @brief Напечатать состояние проекта/индекса.
 * @param response Ответ API.
 */
void print_status(const fcpp::SApiResponse& response)
{
    if (!response.m_ok)
    {
        std::cout << (response.m_error == fcpp::EErrorKind::ProjectNotOpen ? "проект не открыт\n" : "(нет)\n");
        return;
    }

    const std::size_t state_index = static_cast<std::size_t>(response.m_index_status.m_state);
    std::cout << "project_id:  " << response.m_project_id << '\n';
    std::cout << "state:       " << m_index_state_names[state_index] << '\n';
    std::cout << "generation:  " << response.m_index_status.m_generation << '\n';
    std::cout << "files:       " << response.m_index_status.m_file_count << '\n';
    std::cout << "symbols:     " << response.m_index_status.m_symbol_count << '\n';
}

} // namespace

/**
 * @brief Точка входа приложения.
 * @param argc Число аргументов командной строки.
 * @param argv Аргументы командной строки.
 * @return Код возврата: 0 — успех; 1 — ошибка; 2 — ошибка использования.
 */
int main(int argc, char* argv[])
{
    const std::string program = (argc > 0) ? argv[0] : "fcpp-agent";

    if (argc < 2)
    {
        print_usage(program);
        return 2;
    }

    const std::string command = argv[1];

    if (command == "init")
    {
        return run_init();
    }

    if (command == "doctor")
    {
        fcpp::CConfigManager config;
        config.load();
        fcpp::CAgentApi api(config);
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::Doctor;
        return api.execute(request).m_ok ? 0 : 1;
    }

    if (command == "reload")
    {
        fcpp::CConfigManager config;
        fcpp::CAgentApi api(config);
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::ConfigReload;
        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: " << response.m_message << '\n';
            return 1;
        }
        std::cout << "конфигурация перечитана\n";
        return 0;
    }

    if (command == "route")
    {
        // Детерминированная проверка конвейера без LLM: route --area ... [--relation ...]
        // [--projection ...] [--subject <name>] [--project <path>]
        fcpp::SIntent intent;
        std::string project_path;
        for (int index = 2; index < argc; ++index)
        {
            const std::string arg = argv[index];
            auto next = [&argc, &argv, index]() -> std::string
            {
                return (index + 1 < argc) ? std::string(argv[index + 1]) : std::string();
            };
            if (arg == "--area")
            {
                fcpp::parse_area(next(), intent.m_area);
            }
            else if (arg == "--relation")
            {
                fcpp::parse_relation(next(), intent.m_relation);
            }
            else if (arg == "--projection")
            {
                fcpp::parse_projection(next(), intent.m_projection);
            }
            else if (arg == "--subject")
            {
                intent.m_subject.m_name = next();
            }
            else if (arg == "--project")
            {
                project_path = next();
            }
        }

        fcpp::CConfigManager config;
        config.load();
        fcpp::CProjectManager manager(config);
        if (!project_path.empty())
        {
            manager.open(project_path);
        }
        else
        {
            manager.restore_session();
        }

        fcpp::CRequestPipeline pipeline(&manager, &config);
        const fcpp::SRequestOutcome outcome = pipeline.run(intent);

        std::cout << "intent:  " << fcpp::area_name(intent.m_area) << " "
                  << fcpp::relation_name(intent.m_relation) << " "
                  << fcpp::projection_name(intent.m_projection) << '\n';
        std::cout << "stage:   " << fcpp::stage_name(outcome.m_stage) << '\n';
        std::cout << "outcome: " << fcpp::outcome_kind_name(outcome.m_kind) << '\n';
        std::cout << "message: " << outcome.m_message << '\n';
        if (!outcome.m_options.empty())
        {
            std::cout << "options:\n";
            for (const std::string& option : outcome.m_options)
            {
                std::cout << "  - " << option << '\n';
            }
        }
        std::cout << "plan:\n";
        for (const fcpp::EToolName tool : outcome.m_plan.m_tools)
        {
            std::cout << "  - " << fcpp::tool_name(tool) << '\n';
        }

        return (outcome.m_kind == fcpp::EOutcomeKind::NotUnderstood) ? 2 : 0;
    }

    fcpp::CConfigManager config;
    if (!config.load())
    {
        std::cerr << "fcpp-agent: конфигурация не найдена или повреждена; выполните '"
                  << program << " init'\n";
        return 1;
    }

    fcpp::CAgentApi api(config);

    if (command == "open")
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: open требует путь к проекту\n";
            return 2;
        }
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::ProjectOpen;
        request.m_path = argv[2];
        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: FAIL: " << response.m_message << '\n';
            return 1;
        }
        print_status(response);
        return 0;
    }
    if (command == "close")
    {
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::ProjectClose;
        api.execute(request);
        std::cout << "проект закрыт\n";
        return 0;
    }
    if (command == "projects")
    {
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::ProjectList;
        const fcpp::SApiResponse response = api.execute(request);
        if (response.m_names.empty())
        {
            std::cout << "(нет зарегистрированных проектов)\n";
        }
        for (const std::string& name : response.m_names)
        {
            std::cout << name << '\n';
        }
        return 0;
    }
    if (command == "status")
    {
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::ProjectStatus;
        print_status(api.execute(request));
        return 0;
    }
    if (command == "index")
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: index требует подкоманду (status|refresh|rebuild)\n";
            return 2;
        }
        const std::string subcommand = argv[2];

        fcpp::SApiRequest request;
        if (subcommand == "status")
        {
            request.m_method = fcpp::EApiMethod::IndexStatus;
        }
        else if (subcommand == "refresh")
        {
            request.m_method = fcpp::EApiMethod::IndexRefresh;
        }
        else if (subcommand == "rebuild")
        {
            request.m_method = fcpp::EApiMethod::IndexRebuild;
        }
        else
        {
            std::cerr << "fcpp-agent: неизвестная подкоманда index: " << subcommand << '\n';
            return 2;
        }

        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: FAIL: " << response.m_message << '\n';
            return 1;
        }
        print_status(response);
        return 0;
    }
    if (command == "build" || command == "test")
    {
        fcpp::SApiRequest request;
        request.m_method = (command == "build") ? fcpp::EApiMethod::Build : fcpp::EApiMethod::Tests;
        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: " << fcpp::error_kind_name(response.m_error) << ": " << response.m_message << '\n';
            return 1;
        }
        std::cout << response.m_text;
        std::cout << (command == "build" ? "сборка успешна\n" : "тесты пройдены\n");
        return 0;
    }
    if (command == "patch")
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: patch требует путь к патчу (или '-' для stdin)\n";
            return 2;
        }
        const std::string text = read_patch_text(argv[2]);
        if (text.empty())
        {
            std::cerr << "fcpp-agent: пустой патч или не удалось прочитать источник\n";
            return 1;
        }

        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::Patch;
        request.m_payload = text;
        for (int index = 3; index < argc; ++index)
        {
            if (std::string(argv[index]) == "--yes")
            {
                request.m_assume_yes = true;
            }
        }

        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: " << fcpp::error_kind_name(response.m_error) << ": " << response.m_message << '\n';
            return 1;
        }
        std::cout << response.m_text << '\n';
        return 0;
    }

    if (fcpp::api_is_tool_command(command))
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: " << command << " требует имя сущности\n";
            return 2;
        }
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::Tool;
        request.m_tool = command;
        request.m_target = argv[2];
        return print_tool_response(api.execute(request));
    }

    std::cerr << "fcpp-agent: неизвестная команда: " << command << "\n\n";
    print_usage(program);
    return 2;
}
