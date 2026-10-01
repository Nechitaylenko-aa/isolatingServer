#include "queries.h"
#include "compile_commands.h"
#include "build_graph.h"
#include "orchestrator.h"
#include "model_broker.h"
#include "main_model_broker.h"
#include "execute_service.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <map>

using json = nlohmann::json;

namespace {

void printUsage() {
    std::cerr <<
        "Использование:\n"
        "  Отладочные запросы слоя 0 (без модели):\n"
        "    cli_agent locate_symbol <file> <line> <col> [--cc <compile_commands.json>] [-- <clang-flags...>]\n"
        "    cli_agent class_outline <file> <class_name> [--cc <compile_commands.json>] [-- <clang-flags...>]\n"
        "    cli_agent symbol_refs <usr> <file> [--cc <compile_commands.json>] [-- <clang-flags...>]\n"
        "    cli_agent file_flags <compile_commands.json> <file>\n"
        "    cli_agent build_graph <build_dir>\n"
        "\n"
        "  Запуск агента (Intake -> Ground -> Clarify|Report -> Plan -> Execute -> Verify):\n"
        "    cli_agent run <task.json|-> --cc <compile_commands.json> [--model-url <url>] [--model-name <name>]\n"
        "                                 [--main-model-url <url>] [--main-model-name <name>]\n"
        "\n"
        "  Откат правок по checkpointId:\n"
        "    cli_agent undo <checkpointId>\n"
        "\n"
        "  Прогон классификатора light прямо в бинаре (без subprocess/tmp):\n"
        "    cli_agent classify <phrases.txt> [--model-url <url>] [--model-name <n>] [--repeat N] [--temperature T] [--json]\n"
        "      phrases.txt — одна фраза/строку, '#' комментарий. --repeat N — стабильность. --temperature T (по умолчанию 0.1).\n"
        "\n"
        "  Без --model-url используется StubModelBroker (light 8GiB).\n"
        "  Без --main-model-url используется StubMainModelBroker (plan-заглушка).\n"
        "\n"
        "  Бенчмарк классификатора по датасету с expected-метками:\n"
        "    cli_agent benchmark <dataset.json> [--model-url <url>] [--model-name <n>] [--temperature T] [--json]\n"
        "      dataset.json — массив объектов {id, goal_text, expected}. Выводит accuracy по примитивам.\n";
}

// --- разбор аргументов для отладочных query-команд (как раньше в cpp-tool) ---
struct QueryArgs {
    std::vector<std::string> positional;
    std::vector<std::string> clangFlags;
    std::string compileCommandsPath;
};

QueryArgs parseQueryArgs(int argc, char** argv, int startAt) {
    QueryArgs out;
    bool afterDashDash = false;
    for (int i = startAt; i < argc; ++i) {
        std::string a = argv[i];
        if (!afterDashDash && a == "--cc" && i + 1 < argc) { out.compileCommandsPath = argv[++i]; continue; }
        if (!afterDashDash && a == "--") { afterDashDash = true; continue; }
        if (afterDashDash) out.clangFlags.push_back(a);
        else out.positional.push_back(a);
    }
    return out;
}

int runQueryCommand(const std::string& command, int argc, char** argv) {
    if (command == "file_flags") {
        QueryArgs a = parseQueryArgs(argc, argv, 2);
        if (a.positional.size() != 2) { printUsage(); return 2; }
        json result = cpptool::queryFileFlags(a.positional[0], a.positional[1]);
        std::cout << result.dump(2) << std::endl;
        return result.value("ok", false) ? 0 : 1;
    }

    if (command == "build_graph") {
        QueryArgs a = parseQueryArgs(argc, argv, 2);
        if (a.positional.size() != 1) { printUsage(); return 2; }
        json result = cpptool::queryBuildGraph(a.positional[0]);
        std::cout << result.dump(2) << std::endl;
        return result.value("ok", false) ? 0 : 1;
    }

    QueryArgs a = parseQueryArgs(argc, argv, 2);

    std::string fileForFlags;
    if (command == "symbol_refs") {
        if (a.positional.size() >= 2) fileForFlags = a.positional[1];
    } else if (!a.positional.empty()) {
        fileForFlags = a.positional[0];
    }

    std::vector<std::string> flags;
    json flagsWarning;
    if (!a.compileCommandsPath.empty()) {
        if (fileForFlags.empty()) { printUsage(); return 2; }
        json ff = cpptool::queryFileFlags(a.compileCommandsPath, fileForFlags);
        if (!ff.value("ok", false)) { std::cout << ff.dump(2) << std::endl; return 1; }
        for (auto& f : ff["flags"]) flags.push_back(f.get<std::string>());
        if (ff.contains("warning")) flagsWarning = ff["warning"];
    } else {
        flags.push_back("-std=c++17");
    }
    flags.insert(flags.end(), a.clangFlags.begin(), a.clangFlags.end());

    json result;
    if (command == "locate_symbol") {
        if (a.positional.size() != 3) { printUsage(); return 2; }
        result = cpptool::queryLocateSymbol(a.positional[0], std::stoul(a.positional[1]), std::stoul(a.positional[2]), flags);
    } else if (command == "class_outline") {
        if (a.positional.size() != 2) { printUsage(); return 2; }
        result = cpptool::queryClassOutline(a.positional[0], a.positional[1], flags);
    } else if (command == "symbol_refs") {
        if (a.positional.size() != 2) { printUsage(); return 2; }
        result = cpptool::querySymbolRefs(a.positional[0], a.positional[1], flags);
    } else {
        printUsage();
        return 2;
    }

    if (!flagsWarning.is_null() && result.value("ok", false) && !result.contains("warning")) {
        result["warning"] = flagsWarning;
    }
    std::cout << result.dump(2) << std::endl;
    return result.value("ok", false) ? 0 : 1;
}

// --- команда classify: прогон LightModelBroker в процессе (без subprocess/tmp) ---
int classifyCommand(int argc, char** argv) {
    if (argc < 3) { printUsage(); return 2; }
    std::string phrasesPath = argv[2];
    std::string modelUrl, modelName;
    int repeat = 1;
    bool jsonOut = false;
    double temperature = 0.1;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model-url" && i + 1 < argc) modelUrl = argv[++i];
        else if (a == "--model-name" && i + 1 < argc) modelName = argv[++i];
        else if (a == "--repeat" && i + 1 < argc) repeat = std::stoi(argv[++i]);
        else if (a == "--temperature" && i + 1 < argc) temperature = std::stod(argv[++i]);
        else if (a == "--json") jsonOut = true;
    }
    if (repeat < 1) repeat = 1;

    std::ifstream in(phrasesPath);
    if (!in) { std::cerr << "не удалось открыть " << phrasesPath << "\n"; return 2; }
    std::vector<std::string> phrases;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back()=='\r' || line.back()=='\n' || line.back()==' ')) line.pop_back();
        size_t b = 0; while (b < line.size() && line[b]==' ') ++b;
        line = line.substr(b);
        if (line.empty() || line[0]=='#') continue;
        phrases.push_back(line);
    }

    std::unique_ptr<cppagent::ModelBroker> model;
    if (modelUrl.empty()) model = std::make_unique<cppagent::StubModelBroker>();
    else model = std::make_unique<cppagent::LightModelBroker>(modelUrl, modelName, temperature);

    json allOut = json::array();
    int unstableCount = 0;
    for (auto& goal : phrases) {
        std::map<std::string,int> counter;
        std::vector<double> confs;
        std::vector<std::string> alts;
        std::vector<std::string> reasons;
        for (int i = 0; i < repeat; ++i) {
            cppagent::PrimitiveGuess g = model->classifyIntent(goal);
            std::string key = g.modelUnavailable ? ("UNAVAILABLE:"+g.rationale) : g.primitiveStr;
            counter[key]++;
            confs.push_back(g.confidence);
            if (!g.alternativePrimitiveStr.empty()) alts.push_back(g.alternativePrimitiveStr);
            reasons.push_back(g.rationale);
        }
        bool stable = counter.size() == 1;
        if (!stable) ++unstableCount;
        double confAvg = 0.0; if (!confs.empty()) { for (double c : confs) confAvg += c; confAvg /= confs.size(); }

        json one;
        one["goal"] = goal;
        one["stable"] = stable;
        one["repeat"] = repeat;
        one["distribution"] = json::object();
        for (auto& [k,v] : counter) one["distribution"][k] = v;
        one["confidence_avg"] = confAvg;
        one["confidence_all"] = confs;
        one["alternatives"] = alts;
        one["rationale_sample"] = reasons.empty() ? "" : reasons.front();

        if (jsonOut) { allOut.push_back(one); }
        else {
            std::cout << (stable ? "[STABLE  ]" : "[UNSTABLE]") << " "
                      << "conf_avg=" << confAvg << "  "
                      << "dist=";
            bool first = true;
            for (auto& [k,v] : counter) {
                if (!first) std::cout << ",";
                std::cout << k << "x" << v;
                first = false;
            }
            std::cout << "\n           alt=" << (alts.empty() ? "-" : alts.front());
            std::cout << "\n           | " << goal << "\n";
            std::cout << "           rationale: " << (reasons.empty() ? "" : reasons.front()) << "\n\n";
        }
    }
    if (jsonOut) std::cout << allOut.dump(2) << std::endl;
    else std::cout << "=== unstable: " << unstableCount << " / " << phrases.size() << " ===\n";
    return 0;
}

// --- команда benchmark: прогон по датасету с expected-метками ---
int benchmarkCommand(int argc, char** argv) {
    if (argc < 3) { printUsage(); return 2; }
    std::string datasetPath = argv[2];
    std::string modelUrl, modelName;
    bool jsonOut = false;
    double temperature = 0.1;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model-url" && i + 1 < argc) modelUrl = argv[++i];
        else if (a == "--model-name" && i + 1 < argc) modelName = argv[++i];
        else if (a == "--temperature" && i + 1 < argc) temperature = std::stod(argv[++i]);
        else if (a == "--json") jsonOut = true;
    }

    std::ifstream in(datasetPath);
    if (!in) { std::cerr << "не удалось открыть " << datasetPath << "\n"; return 2; }
    json dataset;
    try { in >> dataset; }
    catch (const std::exception& e) { std::cerr << "dataset.json не распарсился: " << e.what() << "\n"; return 2; }
    if (!dataset.is_array()) { std::cerr << "dataset.json должен быть массивом [{id,goal_text,expected}]\n"; return 2; }

    std::unique_ptr<cppagent::ModelBroker> model;
    if (modelUrl.empty()) model = std::make_unique<cppagent::StubModelBroker>();
    else model = std::make_unique<cppagent::LightModelBroker>(modelUrl, modelName, temperature);

    std::map<std::string,int> perClassCorrect, perClassTotal;
    int totalCorrect = 0, totalRun = 0;
    int unavailableCount = 0;
    json rows = json::array();

    for (auto& item : dataset) {
        if (!item.contains("goal_text") || !item.contains("expected")) continue;
        std::string goalText = item["goal_text"].get<std::string>();
        std::string expected = item["expected"].get<std::string>();
        std::string id = item.value("id", "");

        cppagent::PrimitiveGuess g = model->classifyIntent(goalText);
        std::string got = g.modelUnavailable ? "UNAVAILABLE" : g.primitiveStr;

        if (g.modelUnavailable) {
            ++unavailableCount;
            if (!jsonOut)
                std::cerr << "[UNAVAIL] " << id << ": " << g.rationale << "\n";
            if (unavailableCount >= 3) {
                std::cerr << "Модель недоступна (3 подряд), прерываем.\n";
                break;
            }
            continue;
        }

        bool correct = (got == expected);
        ++totalRun;
        if (correct) ++totalCorrect;
        perClassTotal[expected]++;
        if (correct) perClassCorrect[expected]++;

        json row;
        row["id"] = id;
        row["goal_text"] = goalText;
        row["expected"] = expected;
        row["got"] = got;
        row["correct"] = correct;
        row["confidence"] = g.confidence;
        row["alternative"] = g.alternativePrimitiveStr;
        row["rationale"] = g.rationale;
        rows.push_back(row);

        if (!jsonOut) {
            std::cout << (correct ? "[OK ] " : "[ERR] ")
                      << std::left << std::setw(16) << id
                      << " expected=" << std::left << std::setw(14) << expected
                      << " got=" << std::left << std::setw(14) << got
                      << " conf=" << std::fixed << std::setprecision(2) << g.confidence
                      << "\n";
        }
    }

    json summary;
    summary["total"] = totalRun;
    summary["correct"] = totalCorrect;
    summary["accuracy"] = totalRun > 0 ? double(totalCorrect) / double(totalRun) : 0.0;
    summary["unavailable"] = unavailableCount;
    json perClass = json::object();
    for (auto& [prim, tot] : perClassTotal) {
        int cor = perClassCorrect.count(prim) ? perClassCorrect[prim] : 0;
        perClass[prim] = {
            {"correct", cor},
            {"total", tot},
            {"accuracy", tot > 0 ? double(cor)/double(tot) : 0.0}
        };
    }
    summary["per_class"] = perClass;

    if (jsonOut) {
        json out;
        out["summary"] = summary;
        out["rows"] = rows;
        std::cout << out.dump(2) << std::endl;
    } else {
        std::cout << "\n=== ИТОГО: " << totalCorrect << "/" << totalRun
                  << " (" << std::fixed << std::setprecision(1)
                  << (totalRun > 0 ? 100.0*totalCorrect/totalRun : 0.0) << "%) ===";
        if (unavailableCount > 0) std::cout << " unavailable=" << unavailableCount;
        std::cout << "\n";
        std::cout << "\nПо примитивам:\n";
        for (auto& [prim, tot] : perClassTotal) {
            int cor = perClassCorrect.count(prim) ? perClassCorrect[prim] : 0;
            std::cout << "  " << std::left << std::setw(16) << prim
                      << cor << "/" << tot
                      << " (" << std::fixed << std::setprecision(0)
                      << (tot > 0 ? 100.0*cor/tot : 0.0) << "%)\n";
        }
    }
    return (totalRun > 0 && double(totalCorrect)/double(totalRun) >= 0.8) ? 0 : 1;
}

// --- команда run: запуск оркестратора ---
int runAgentCommand(int argc, char** argv) {
    if (argc < 3) { printUsage(); return 2; }
    std::string taskPath = argv[2];
    std::string ccPath, modelUrl, modelName, mainUrl, mainName;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cc" && i + 1 < argc) ccPath = argv[++i];
        else if (a == "--model-url" && i + 1 < argc) modelUrl = argv[++i];
        else if (a == "--model-name" && i + 1 < argc) modelName = argv[++i];
        else if (a == "--main-model-url" && i + 1 < argc) mainUrl = argv[++i];
        else if (a == "--main-model-name" && i + 1 < argc) mainName = argv[++i];
    }
    if (ccPath.empty()) { printUsage(); return 2; }

    std::string raw;
    if (taskPath == "-") {
        std::ostringstream ss; ss << std::cin.rdbuf(); raw = ss.str();
    } else {
        std::ifstream in(taskPath);
        if (!in) { std::cerr << "не удалось открыть " << taskPath << "\n"; return 2; }
        std::ostringstream ss; ss << in.rdbuf(); raw = ss.str();
    }

    json turn;
    try { turn = json::parse(raw); }
    catch (const std::exception& e) { std::cerr << "task.json не распарсился: " << e.what() << "\n"; return 2; }

    if (!turn.contains("task")) { std::cerr << "ожидался объект с полем \"task\" (см. ТЗ §7.1)\n"; return 2; }

    std::unique_ptr<cppagent::ModelBroker> model;
    if (modelUrl.empty()) model = std::make_unique<cppagent::StubModelBroker>();
    else model = std::make_unique<cppagent::LightModelBroker>(modelUrl, modelName);

    std::unique_ptr<cppagent::MainModelBroker> mainModel;
    if (!mainUrl.empty()) mainModel = std::make_unique<cppagent::HttpMainModelBroker>(mainUrl, mainName);
    // если mainUrl пустой — orchestrator использует StubMainModelBroker внутри handlers

    cppagent::Orchestrator orchestrator(*model, ccPath, mainModel.get());
    json outcome = orchestrator.run(turn["task"]);

    std::cout << outcome.dump(2) << std::endl;
    return outcome.value("type", "") == "error" ? 1 : 0;
}

int undoCommand(int argc, char** argv) {
    if (argc < 3) { printUsage(); return 2; }
    std::string checkpointId = argv[2];
    cppagent::ExecuteService svc;
    std::string error;
    bool ok = svc.undo(checkpointId, error);
    json result;
    result["ok"] = ok;
    result["checkpointId"] = checkpointId;
    if (!ok) result["error"] = error;
    else result["message"] = "откат выполнен, файлы восстановлены из checkpoint";
    std::cout << result.dump(2) << std::endl;
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(); return 2; }
    std::string command = argv[1];

    if (command == "run") return runAgentCommand(argc, argv);
    if (command == "undo") return undoCommand(argc, argv);
    if (command == "classify") return classifyCommand(argc, argv);
    if (command == "benchmark") return benchmarkCommand(argc, argv);

    static const std::vector<std::string> queryCommands =
        {"locate_symbol", "class_outline", "symbol_refs", "file_flags", "build_graph"};
    for (auto& c : queryCommands) {
        if (command == c) return runQueryCommand(command, argc, argv);
    }

    printUsage();
    return 2;
}
