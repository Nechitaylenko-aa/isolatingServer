/**
 * @file main.cpp
 * @brief Точка входа CLI fcpp-agent — тонкий клиент Agent API (§93).
 *
 * CLI не вызывает Agent Core напрямую: каждая команда транслируется в
 * типизированный SApiRequest и выполняется через CAgentApi.
 */

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cli/CProtocolServer.h"
#include "core/CAgentApi.h"
#include "core/CConfigManager.h"
#include "fcpp-agent/IAgentApi.h"
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
    std::cout << "  open <path>                     Открыть проект\n";
    std::cout << "  close                           Закрыть проект\n";
    std::cout << "  projects                        Список проектов\n";
    std::cout << "  status                          Состояние проекта и индекса\n";
    std::cout << "  index <status|refresh|rebuild> [<path>]\n";
    std::cout << "  build                           Сборка (cmake --build)\n";
    std::cout << "  test                            Тесты (ctest)\n";
    std::cout << "  patch <file|-> [--yes] [--build]  Применить unified diff (+проверка сборкой)\n";
    std::cout << "\nДетерминированные запросы (tools):\n";
    std::cout << "  outline|callers|callees|refs|includes|overrides|dispatch\n";
    std::cout << "  def|ancestors|derived|search|source <symbol> [<path>]\n";
    std::cout << "  ask \"<text>\" --session <id> [--file F --line L --col C] [--stream]\n";
    std::cout << "                                  Запрос в диалоге (сессия обязательна)\n";
    std::cout << "  session open                    Создать отдельную сессию\n";
    std::cout << "  session resume [project-path]   Восстановить сессию проекта\n";
    std::cout << "  session close <id>              Закрыть сессию\n";
    std::cout << "\nОпции: --config PATH --project PATH --compile-commands PATH --yes --json --verbose\n";
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
    if (!response.m_session_id.empty())
    {
        std::cout << "session_id:  " << response.m_session_id << '\n';
    }
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

    if (command == "serve")
    {
        // Протокольный режим: JSON-RPC поверх stdio (для IDE/плагина).
        // Опционально: --project <root> — открыть проект до начала цикла.
        std::string project_root;
        for (int index = 2; index < argc; ++index)
        {
            const std::string arg = argv[index];
            if (arg == "--project" && index + 1 < argc)
            {
                project_root = argv[index + 1];
            }
        }

        fcpp::CConfigManager protocol_config;
        protocol_config.load();
        fcpp::CProtocolServer server(protocol_config, std::cin, std::cout);
        if (!project_root.empty())
        {
            server.open_project(project_root);
        }
        return server.run();
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
    if (command == "session")
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: session требует подкоманду (open|resume|close)\n";
            return 2;
        }
        const std::string subcommand = argv[2];

        fcpp::SApiRequest request;
        if (subcommand == "open")
        {
            request.m_method = fcpp::EApiMethod::SessionOpen;
        }
        else if (subcommand == "resume")
        {
            // CLI is one-shot, unlike the long-lived IDE protocol server.
            // Open the supplied project in this process, or restore the last
            // project address before asking the API to resume its session.
            if (argc >= 4)
            {
                fcpp::SApiRequest open_request;
                open_request.m_method = fcpp::EApiMethod::ProjectOpen;
                open_request.m_path = argv[3];
                const fcpp::SApiResponse opened = api.execute(open_request);
                if (!opened.m_ok)
                {
                    std::cerr << "fcpp-agent: cannot open project for session resume: "
                              << opened.m_message << '\n';
                    return 1;
                }
            }
            else
            {
                fcpp::SApiRequest status_request;
                status_request.m_method = fcpp::EApiMethod::ProjectStatus;
                const fcpp::SApiResponse status = api.execute(status_request);
                if (!status.m_ok)
                {
                    std::cerr << "fcpp-agent: session resume requires a project path "
                                 "or a previously opened project; use 'session resume <path>'\n";
                    return 1;
                }
            }
            request.m_method = fcpp::EApiMethod::SessionResume;
        }
        else if (subcommand == "close")
        {
            if (argc < 4)
            {
                std::cerr << "fcpp-agent: session close требует id сессии\n";
                return 2;
            }
            request.m_method = fcpp::EApiMethod::SessionClose;
            request.m_session_id = argv[3];
        }
        else
        {
            std::cerr << "fcpp-agent: неизвестная подкоманда session: " << subcommand << '\n';
            return 2;
        }

        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: " << fcpp::error_kind_name(response.m_error)
                      << ": " << response.m_message << '\n';
            return 1;
        }
        std::cout << response.m_session_id << '\n';
        return 0;
    }
    if (command == "ask")
    {
        if (argc < 3)
        {
            std::cerr << "fcpp-agent: ask требует текст запроса\n";
            return 2;
        }

        // Тело запроса §94: только text + editor{file,cursor}. --stream — режим вывода.
        // Адрес диалога принадлежит агенту: активную сессию проекта создаёт
        // project.open и возвращает её id. --session необязателен и служит
        // проверкой, что клиент говорит с той же сессией, а не адресом.
        fcpp::SApiRequest request;
        request.m_method = fcpp::EApiMethod::Ask;
        request.m_text = argv[2];
        for (int index = 3; index < argc; ++index)
        {
            const std::string arg = argv[index];
            auto next = [&argc, &argv, index]() -> std::string
            {
                return (index + 1 < argc) ? std::string(argv[index + 1]) : std::string();
            };
            if (arg == "--stream")
            {
                request.m_stream = true;
            }
            else if (arg == "--session")
            {
                request.m_session_id = next();
            }
            else if (arg == "--file")
            {
                request.m_editor_file = next();
            }
            else if (arg == "--line")
            {
                request.m_editor_line = static_cast<std::uint32_t>(std::strtoul(next().c_str(), nullptr, 10));
            }
            else if (arg == "--col")
            {
                request.m_editor_column = static_cast<std::uint32_t>(std::strtoul(next().c_str(), nullptr, 10));
            }
        }
        bool streamed_output = false;
        if (request.m_stream)
        {
            request.m_on_stream_chunk = [&streamed_output](const std::string& chunk) -> bool
            {
                std::cout << chunk;
                std::cout.flush();
                if (!chunk.empty())
                {
                    streamed_output = true;
                }
                return std::cout.good();
            };
        }

        const fcpp::SApiResponse response = api.execute(request);
        if (streamed_output)
        {
            std::cout << '\n';
        }
        else if (!response.m_text.empty())
        {
            std::cout << response.m_text << '\n';
        }
        if (!response.m_ok && !response.m_message.empty())
        {
            std::cerr << "fcpp-agent: " << response.m_message << '\n';
        }
        return response.m_ok ? 0 : 1;
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
            const std::string arg = argv[index];
            if (arg == "--yes")
            {
                request.m_assume_yes = true;
            }
            else if (arg == "--build")
            {
                request.m_build_after = true;
            }
        }

        const fcpp::SApiResponse response = api.execute(request);
        if (!response.m_ok)
        {
            std::cerr << "fcpp-agent: " << fcpp::error_kind_name(response.m_error) << ": " << response.m_message << '\n';
            return 1;
        }
        std::cout << response.m_text << '\n';
        if (request.m_build_after)
        {
            std::cout << "сборка успешна\n";
        }
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
