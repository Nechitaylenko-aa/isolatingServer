#include "handlers.h"
#include "queries.h"
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "main_model_broker.h"
#include "execute_service.h"
#include "verify_service.h"

namespace cppagent {

namespace {

json answeredBy(const HandlerContext& ctx) {
    std::string role = ctx.guess.modelUnavailable ? "deterministic_fallback" : "stub";
    // guess не хранит roleName, но handlers вызываются из Orchestrator где есть ModelBroker.
    // Пока возвращаем generic; Orchestrator перезапишет answered_by.role на model_.roleName().
    return json{{"role", role}};
}

std::string buildDirFromCC(const std::string& cc){ namespace fs=std::filesystem; if(cc.empty()) return ""; fs::path p=fs::absolute(fs::path(cc)); if(p.has_parent_path()) return p.parent_path().string(); return ""; }

// Фрагмент файла вокруг курсора с нумерацией строк. Без него main-модель не видит исходник
// и не может выдать oldText, точно совпадающий с содержимым (выдумывает или вставляет в конец).
std::string readFileSnippet(const std::string& file, int line, int radius){
    if(file.empty() || line <= 0) return "";
    std::ifstream in(file);
    if(!in) return "";
    std::vector<std::string> lines;
    std::string l;
    while(std::getline(in, l)) lines.push_back(l);
    if(lines.empty()) return "";
    size_t ln = static_cast<size_t>(line);
    if(ln > lines.size()) ln = lines.size();
    size_t from = (ln > static_cast<size_t>(radius)) ? ln - radius : 1;
    size_t to = std::min(lines.size(), ln + radius);
    std::ostringstream os;
    for(size_t i = from; i <= to; ++i)
        os << i << ": " << lines[i-1] << "\n";
    return os.str();
}

// Точный диапазон строк [fromLine..toLine] с нумерацией — extent метода из
// libclang-кэша. В отличие от окна ±radius не пере/недо-добирает.
std::string readFileLines(const std::string& file, int fromLine, int toLine){
    if(file.empty() || fromLine <= 0 || toLine < fromLine) return "";
    std::ifstream in(file);
    if(!in) return "";
    std::vector<std::string> lines;
    std::string l;
    while(std::getline(in, l)) lines.push_back(l);
    if(lines.empty()) return "";
    size_t from = static_cast<size_t>(fromLine);
    size_t to = std::min(lines.size(), static_cast<size_t>(toLine));
    if(from > lines.size()) return "";
    std::ostringstream os;
    for(size_t i = from; i <= to; ++i)
        os << i << ": " << lines[i-1] << "\n";
    return os.str();
}

// Компактная структура класса (объявление + базы + публичные сигнатуры) для
// read-only reasoning в DELEGATE_TO_MAIN. Это сигнатуры и иерархия, БЕЗ тел —
// ровно то, что нужно для вопроса "как устроено", и дешевле по токенам, чем
// локальное окно вокруг курсора.
// Детерминированная эвристика качества по телу метода (детерминизм — сильная
// сторона агента: не гадаем, а называем конкретные факты). Это НЕ глубокий
// анализ: длина, вложенность, длина строк, "голые" new/delete, catch(...).
json codeSmellHeuristic(const HandlerContext& ctx){
    json findings = json::array();
    const auto& m = ctx.ground.enclosingMethod;
    if(m.is_null()) return findings;
    int s = m.value("start_line", 0), e = m.value("end_line", 0);
    std::string f = m.value("file", "");
    if(s <= 0 || e < s || f.empty()) return findings;
    std::ifstream in(f);
    if(!in) return findings;
    std::vector<std::string> lines; std::string l;
    while(std::getline(in, l)) lines.push_back(l);
    if((size_t)s > lines.size()) return findings;
    size_t from = s, to = std::min(lines.size(), (size_t)e);
    int bodyLines = 0, maxIndent = 0, longLines = 0, rawNew = 0, rawDelete = 0, catchAll = 0;
    for(size_t i = from; i <= to; ++i){
        const std::string& t = lines[i-1];
        size_t first = t.find_first_not_of(" \t");
        if(first == std::string::npos) continue;
        ++bodyLines;
        int indent = (int)first; if(indent > maxIndent) maxIndent = indent;
        if(t.size() > 120) ++longLines;
        if(t.find("new ") != std::string::npos) ++rawNew;
        if(t.find("delete ") != std::string::npos) ++rawDelete;
        if(t.find("catch(...)") != std::string::npos || t.find("catch (...)") != std::string::npos) ++catchAll;
    }
    if(bodyLines > 50) findings.push_back({{"kind","long_method"},{"detail","тело ~" + std::to_string(bodyLines) + " строк (>50) — кандидат на декомпозицию"}});
    if(maxIndent >= 20) findings.push_back({{"kind","deep_nesting"},{"detail","глубокий отступ (" + std::to_string(maxIndent) + ") — вероятна сложная вложенность"}});
    if(longLines > 0)   findings.push_back({{"kind","long_lines"},{"detail",std::to_string(longLines) + " строк длиннее 120 символов"}});
    if(rawNew > 0 || rawDelete > 0) findings.push_back({{"kind","raw_memory"},{"detail","ручное new/delete (" + std::to_string(rawNew) + "/" + std::to_string(rawDelete) + ") — рассмотреть RAII/умные указатели"}});
    if(catchAll > 0)    findings.push_back({{"kind","catch_all"},{"detail","catch(...) глушит исключения (" + std::to_string(catchAll) + ")"}});
    return findings;
}

std::string classStructureFor(const HandlerContext& ctx){
    if(ctx.ground.enclosingClass.is_null()) return "";
    std::string cname = ctx.ground.enclosingClass.value("name","");
    if(cname.empty()) return "";
    json co = cpptool::queryClassOutline(cname, ctx.compileCommandsPath);
    if(!co.value("ok", false)) return "";
    std::string out = "class " + cname;
    auto bases = co.value("base_classes", json::array());
    if(bases.is_array() && !bases.empty()){
        out += " : ";
        for(size_t i=0;i<bases.size();++i){ if(i) out += ", "; out += bases[i].get<std::string>(); }
    }
    out += " {\npublic:\n";
    for(auto& m : co.value("public_methods", json::array()))
        out += "    " + m.value("signature", "?") + ";\n";
    out += "};\n";
    return out;
}

int cursorLineOf(const HandlerContext& ctx){ return ctx.task.contains("cursor") ? ctx.task["cursor"].value("line", 0) : 0; }

// Курсор мог устареть между постановкой задачи и текущим состоянием файла: locate не находит метод,
// sig становится "?", blast_radius теряет вход и откатывается в sandbox. Раньше это было видно
// только как "?" в тексте — добавляем явное предупреждение (и в текст, и структурно).
bool groundLostMethod(const HandlerContext& ctx){
    // Слой 0: если задача по природе не привязана к месту (NOT_NEEDED),
    // отсутствие метода под курсором — норма, а не деградация: не шумим
    // предупреждением "курсор устарел".
    if (ctx.guess.cursorBinding == CursorBinding::NOT_NEEDED) return false;
    return ctx.ground.enclosingMethod.is_null();
}
std::string groundNote(const HandlerContext& ctx){
    if(groundLostMethod(ctx))
        return " [warning: метод под курсором не разрешён — возможно, устарел cursor.line; правка идёт по oldText]";
    return "";
}
json groundWarnings(const HandlerContext& ctx){
    json w = json::array();
    if(groundLostMethod(ctx))
        w.push_back("enclosing_method_not_resolved: курсор не попал в метод (устаревший cursor.line или промах locate); окружение метода не проверено, blob blast_radius не применён");
    return w;
}

json callMain(const HandlerContext& ctx, const std::string& file, const std::string& sig, const std::string& usr,
              const std::string& contextOverride = ""){
    GenerateRequest req{ctx.primitiveStr, ctx.task.value("goal_text",""), file, sig, usr, ctx.ground.scaleFacts, ctx.explicitMode};
    if(!contextOverride.empty()){
        // Явно заданный контекст (напр. структура класса для read-only reasoning).
        req.fileContext = contextOverride;
    } else if(!ctx.ground.enclosingMethod.is_null()){
        // Точный extent метода из кэша (start_line..end_line) — ровно тело,
        // без переизбытка/недобора фиксированного окна.
        int s = ctx.ground.enclosingMethod.value("start_line", 0);
        int e = ctx.ground.enclosingMethod.value("end_line", 0);
        std::string mfile = ctx.ground.enclosingMethod.value("file", file);
        if(s > 0 && e >= s) req.fileContext = readFileLines(mfile, s, e);
    }
    if(req.fileContext.empty())
        req.fileContext = readFileSnippet(file, cursorLineOf(ctx), 40);
    GenerateResult res;
    if(ctx.mainModel) res = ctx.mainModel->generate(req);
    else { StubMainModelBroker stub; res = stub.generate(req); }
    json j;
    if(res.ok){ j["plan"]=res.planText; j["rationale"]=res.rationale; j["actions"]=res.actions; }
    else { j["error"]=res.error; j["modelUnavailable"]=res.modelUnavailable; j["actions"]=json::array(); }
    return j;
}
bool planFailed(const json& plan){
    return plan.contains("error") || plan.contains("modelUnavailable");
}
json mainPlanFailedError(const HandlerContext& ctx, const json& plan){
    return json{
        {"protocol_version","1.0"},
        {"type","error"},
        {"error", {
            {"code","main_plan_failed"},
            {"message", plan.value("error", "main model plan generation failed")},
            {"domain","model"}
        }},
        {"primitive", ctx.primitiveStr}
    };
}
// Fail-closed: контракт считается НАРУШЕННЫМ, пока сборка не подтвердит обратное.
// Значит success — это ТОЛЬКО vres.ok==true (реально собралось). Провал компиляции
// и пропуск верификации (skipped: buildDir не разрешился) — оба ведут в type:"error",
// а не в "answer" с тихим verify.compile_ok=false внутри.
json verifyFailedError(const HandlerContext& ctx, const json& execJson, const json& checkpoint, const json& verify){
    return json{
        {"protocol_version","1.0"},
        {"type","error"},
        {"error", {
            {"code", verify.value("skipped", false) ? "verify_skipped" : "verify_failed"},
            {"message", verify.value("skipped", false)
                ? std::string("верификация пропущена (buildDir не разрешился) — правка применена, но НЕ подтверждена сборкой")
                : verify.value("error", json("compile/tests failed")).get<std::string>()},
            {"domain","build"}
        }},
        {"primitive", ctx.primitiveStr},
        {"execute", execJson},
        {"checkpoint", checkpoint},
        {"verify", verify}
    };
}

// Провал применения правок (oldText не найден, файл недоступен) — тоже ошибка, а не
// "answer с вложенным execute.ok=false": клиент читает type и verify, не вложенность.
json executeFailedError(const HandlerContext& ctx, const json& execJson){
    return json{
        {"protocol_version","1.0"},
        {"type","error"},
        {"error", {
            {"code","execute_failed"},
            {"message", execJson.value("error", "apply failed")},
            {"domain","filesystem"}
        }},
        {"primitive", ctx.primitiveStr},
        {"execute", execJson}
    };
}
// hintLine — строка курсора: при нескольких вхождениях oldText apply выберет ближайшее.
// Путь файла — НЕ от модели: canonicalFile приходит из Ground (cursor.file). Модель
// генерирует только oldText/newText. Поле "file" в actions (если модель его всё же
// прислала) игнорируется — иначе main-модель выдумывает несуществующие пути.
std::vector<EditAction> actionsFromPlan(const json& plan, const std::string& canonicalFile, int hintLine = 0){
    std::vector<EditAction> out;
    if(canonicalFile.empty()) return out; // без канонического пути писать некуда
    if(!plan.contains("actions") || !plan["actions"].is_array()) return out;
    for(auto& a: plan["actions"]){
        if(!a.contains("oldText") && !a.contains("newText")) continue;
        EditAction e; e.file = canonicalFile;
        e.oldText=a.value("oldText",""); e.newText=a.value("newText","");
        e.hintLine=a.value("hintLine", hintLine);
        out.push_back(std::move(e));
    }
    return out;
}
json runExecute(const std::vector<EditAction>& edits){
    ExecuteService svc;
    auto r = svc.apply(edits);
    json j={{"ok",r.ok},{"appliedFiles",r.appliedFiles},{"checkpointId",r.checkpointId}};
    if(!r.error.empty()) j["error"]=r.error;
    if(!r.matchInfo.empty()) j["matchInfo"]=r.matchInfo;
    return j;
}

} // anon

std::unique_ptr<PrimitiveHandler> makeHandler(Primitive p) {
    switch (p) {
        case Primitive::UNDERSTAND:   return std::make_unique<UnderstandHandler>();
        case Primitive::SANDBOX_FIX:  return std::make_unique<SandboxFixHandler>();
        case Primitive::QUARRY_DESIGN:return std::make_unique<QuarryDesignHandler>();
        case Primitive::TEST_GEN:     return std::make_unique<TestGenHandler>();
        case Primitive::EXPERIMENT:   return std::make_unique<ExperimentHandler>();
        case Primitive::INFRA:        return std::make_unique<InfraHandler>();
    }
    return std::make_unique<UnderstandHandler>();
}

// UNDERSTAND: Intake -> Ground -> Report (read-only, запрет на запись)
// Поведение ветвится по Action = selectAction(primitive, Detail, CursorContext) —
// слои 1-4 из arch.md, см. task_classification.h. Раньше ветвилось по
// DetailKind (4 значения, только virtual/pure-virtual/header-fallback) и
// игнорировало, что именно просил юзер (Detail) — отсюда и была дыра:
// "покажи метод" на виртуальном методе показывало все overrides, даже если
// юзера интересовала только сигнатура.
json UnderstandHandler::handle(const HandlerContext& ctx) {
    const auto& ground = ctx.ground;
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";

    // Раньше здесь был ранний return при NO_CONTEXT — он МАСКИРОВАЛ слой 3:
    // computeRelevance для NO_CONTEXT всегда UNRELATED, которое по arch.md
    // обязано превратиться в NEEDS_INPUT_RELEVANCE. Теперь NO_CONTEXT проходит
    // через ту же таблицу, что и все остальные контексты (arch.md: слой 3 -> 4).
    std::string enclosingUsr = ground.enclosingMethod.is_null() ? "" : ground.enclosingMethod.value("usr", "");
    std::string sig = ground.enclosingMethod.is_null() ? "?" : ground.enclosingMethod.value("signature", "?");
    json refs = json::array();
    if (!ground.enclosingMethod.is_null()) {
        refs.push_back(json{
            {"usr",  enclosingUsr},
            {"file", file},
            {"line", ground.enclosingMethod.value("line", 0)}
        });
    }

    // Слой 0 важнее слоя 1 для решений о месте: если задача NOT_NEEDED (курсор
    // в принципе не нужен), любое "место" нерелевантно, и method-bound Detail
    // (модель иногда возвращает SIMPLE_READ на общий вопрос) не должен форсить
    // needs_input. Трактyeм detail как GENERAL_QUESTION -> NEARBY -> DELEGATE_TO_MAIN.
    Detail effectiveDetail = ctx.guess.detail;
    if (ctx.guess.cursorBinding == CursorBinding::NOT_NEEDED && effectiveDetail != Detail::GENERAL_QUESTION)
        effectiveDetail = Detail::GENERAL_QUESTION;
    Relevance relevance = computeRelevance(effectiveDetail, ground.cursorContext,
                                           ctx.task.value("goal_text",""), enclosingUsr);
    Action action = selectAction(ctx.primitiveStr, effectiveDetail, ground.cursorContext, relevance);
    std::string ctxStr = cursorContextToString(ground.cursorContext);
    std::string text;

    // Общий кусок "показать overrides" нужен трём разным Action — вынесен,
    // чтобы не плодить копии одного и того же текста.
    auto appendOverrides = [&](std::string& t) {
        if (ground.virtualOverridesChecked && ground.virtualOverrides.value("ok", false)) {
            auto& overrides = ground.virtualOverrides["overrides"];
            if (overrides.is_array() && !overrides.empty()) {
                t += " Найдено реализаций: " + std::to_string(overrides.size()) + ":";
                for (auto& ov : overrides) {
                    std::string ovSig  = ov.value("signature", "?");
                    std::string ovFile = ov.value("file", "");
                    int         ovLine = ov.value("line", 0);
                    t += " [" + ovSig + " @ " + ovFile + ":" + std::to_string(ovLine) + "]";
                    refs.push_back(json{{"usr", ov.value("usr","")}, {"file", ovFile}, {"line", ovLine}});
                }
                int scanned = ground.virtualOverrides.value("files_scanned", 0);
                int total   = ground.virtualOverrides.value("files_total",   0);
                if (scanned > 0 || total > 0)
                    t += " (" + std::to_string(scanned) + "/" + std::to_string(total) + " TU просканировано)";
            } else {
                int scanned = ground.virtualOverrides.value("files_scanned", 0);
                int total   = ground.virtualOverrides.value("files_total",   0);
                t += " Реализаций в проекте не найдено (" + std::to_string(scanned) + "/" + std::to_string(total) + " TU просканировано).";
            }
        } else if (ground.virtualOverridesChecked) {
            t += " Поиск реализаций завершился с ошибкой: " + ground.virtualOverrides.value("error", json::object()).value("message", "unknown");
        }
    };

    switch (action) {

    case Action::QUERY_LOCATE_ONLY:
        if (ground.enclosingMethod.is_null())
            text = "UNDERSTAND: под курсором не нашёлся метод/функция (" + ctxStr + "). "
                   "Код на диске не меняется. rationale: " + ctx.guess.rationale;
        else
            text = "UNDERSTAND: " + sig + ". Код на диске не меняется. rationale: " + ctx.guess.rationale;
        break;

    case Action::QUERY_FIND_IMPLEMENTORS:
    case Action::QUERY_VIRTUAL_OVERRIDES:
        text = "UNDERSTAND: " + sig + ". Код на диске не меняется. rationale: " + ctx.guess.rationale;
        appendOverrides(text);
        break;

    case Action::QUERY_CALL_CHAIN:
    case Action::QUERY_CALL_SUBTREE: {
        bool incoming = (action == Action::QUERY_CALL_CHAIN);
        json res = incoming
            ? cpptool::queryCallChain(enclosingUsr, ctx.compileCommandsPath)
            : cpptool::queryCallSubtree(enclosingUsr, ctx.compileCommandsPath);
        std::string head = incoming ? "UNDERSTAND (кто вызывает " + sig + ")"
                                    : "UNDERSTAND (кого вызывает " + sig + ")";
        if (res.value("ok", false)) {
            auto& list = res["calls"];
            if (list.is_array() && !list.empty()) {
                text = head + ": найдено " + std::to_string(list.size()) + ":";
                for (auto& c : list) {
                    std::string cSig = c.value("signature", "?");
                    std::string cFile = c.value("file", "");
                    int cLine = c.value("line", 0);
                    int callLine = c.value("call_line", 0);
                    text += " [" + cSig + " @ " + cFile + ":" + std::to_string(cLine);
                    if (callLine > 0) text += " (вызов на " + std::to_string(callLine) + ")";
                    text += "]";
                    refs.push_back(json{{"usr", c.value("usr","")}, {"file", cFile}, {"line", cLine}});
                }
                text += ". Код на диске не меняется.";
            } else {
                text = head + ": в проекте не найдено. Код на диске не меняется.";
            }
        } else {
            std::string msg = res.value("error", json::object()).value("message", "ошибка query");
            text = head + ": ошибка — " + msg;
        }
        break;
    }

    case Action::QUERY_DEPENDENCY_MAP: {
        json res = cpptool::queryDependencyMap(file, ctx.compileCommandsPath);
        if (res.value("ok", false)) {
            auto deps = res.value("dependencies", json::array());
            auto rev  = res.value("dependents", json::array());
            if (!ground.enclosingClass.is_null()) {
                text = "UNDERSTAND: зависимости. Класс " + ground.enclosingClass.value("name","") +
                       ": подключается " + std::to_string(rev.size()) + " файл(ами), подключает " +
                       std::to_string(deps.size()) + " файл(ов). Код на диске не меняется.";
            } else {
                text = "UNDERSTAND: зависимости файла " + file + ": подключается " + std::to_string(rev.size()) +
                       ", подключает " + std::to_string(deps.size()) + ". Код на диске не меняется.";
            }
            auto refsAdd = [&](const json& arr){ for (auto& f : arr) refs.push_back(json{{"file", f.get<std::string>()}, {"line", 0}}); };
            refsAdd(rev); refsAdd(deps);
        } else {
            text = "UNDERSTAND: не удалось построить карту зависимостей — " +
                   res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::QUERY_TODO_SCAN: {
        std::string classUsr = ground.enclosingClass.is_null() ? "" : ground.enclosingClass.value("usr","");
        // Если курсор в файле — сканируем файл, иначе весь класс.
        std::string scopeFile = (!file.empty() && ground.enclosingClass.is_null()) ? file : "";
        json res = cpptool::queryTodoScan(ctx.compileCommandsPath, scopeFile, classUsr);
        if (res.value("ok", false)) {
            auto& markers = res["markers"];
            text = "UNDERSTAND: TODO/FIXME/HACK/XXX — найдено " + std::to_string(markers.size()) + ".";
            for (auto& m : markers) {
                text += " [" + m.value("marker","?") + " " + m.value("file","") + ":" +
                        std::to_string(m.value("line",0)) + "]";
                refs.push_back(json{{"file", m.value("file","")}, {"line", m.value("line",0)}});
            }
            text += " Код на диске не меняется.";
        } else {
            text = "UNDERSTAND: TODO-скан не удался — " + res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::QUERY_DOC_CHECK: {
        std::string classUsr = ground.enclosingClass.is_null() ? "" : ground.enclosingClass.value("usr","");
        std::string scopeFile = (!file.empty() && ground.enclosingClass.is_null()) ? file : "";
        json res = cpptool::queryDocCheck(ctx.compileCommandsPath, scopeFile, classUsr);
        if (res.value("ok", false)) {
            auto& und = res["undocumented"];
            auto& chk = res["checked"];
            text = "UNDERSTAND: doc-комментарии — проверено " + std::to_string(chk.size()) +
                   " файл(ов), без заголовочного doc: " + std::to_string(und.size()) + ".";
            for (auto& u : und) {
                text += " [" + u.value("file","") + "]";
                refs.push_back(json{{"file", u.value("file","")}, {"line", u.value("line",0)}});
            }
            text += " Код на диске не меняется.";
        } else {
            text = "UNDERSTAND: DOC_CHECK не удался — " + res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::QUERY_CODE_SMELL: {
        json findings = codeSmellHeuristic(ctx);
        // Плюс метрика тела (длина/вложенность/строки) через queryComplexityHotspot
        // для точного extent метода — единый источник метрик.
        if (!ground.enclosingMethod.is_null()) {
            std::string mfile = ground.enclosingMethod.value("file", file);
            json ch = cpptool::queryComplexityHotspot(ctx.compileCommandsPath, mfile, "");
            if (ch.value("ok", false)) {
                for (auto& h : ch["hotspots"]) {
                    if (h.value("usr", "") != enclosingUsr) continue;
                    int nest = h.value("max_nesting", 0);
                    int bl = h.value("body_lines", 0);
                    if (nest >= 4) findings.push_back({{"kind","nesting"},{"detail","вложенность скобок до " + std::to_string(nest)}});
                    if (bl > 50) findings.push_back({{"kind","length"},{"detail","тело ~" + std::to_string(bl) + " строк"}});
                    break;
                }
            }
        }
        if (findings.empty()) {
            text = "UNDERSTAND: " + sig + " — явных эвристических запахов не найдено.";
        } else {
            text = "UNDERSTAND: " + sig + " — эвристические находки (" + std::to_string(findings.size()) + "):";
            for (auto& f : findings) text += " [" + f.value("kind","?") + ": " + f.value("detail","") + "]";
        }
        text += " Код на диске не меняется.";
        break;
    }

    case Action::QUERY_TYPE_USAGE: {
        // Тип/класс под курсором: используем USR класса, если он есть.
        std::string typeUsr = !ground.enclosingClass.is_null()
                                ? ground.enclosingClass.value("usr","") : enclosingUsr;
        json res = cpptool::queryTypeUsage(typeUsr, ctx.compileCommandsPath);
        if (res.value("ok", false)) {
            auto locs = res.value("locations", json::array());
            text = "UNDERSTAND: тип используется в " + std::to_string(locs.size()) + " мест(ах).";
            int shown = 0;
            for (auto& l : locs) {
                if (shown++ >= 20) { text += " …"; break; }
                text += " [" + l.value("file","") + ":" + std::to_string(l.value("line",0)) + "]";
                refs.push_back(json{{"file", l.value("file","")}, {"line", l.value("line",0)}});
            }
            text += " Код на диске не меняется.";
        } else {
            text = "UNDERSTAND: TYPE_USAGE не удался — " + res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::QUERY_COMPLEXITY: {
        std::string scopeFile = (!file.empty() && ground.enclosingClass.is_null()) ? file : "";
        std::string classUsr = ground.enclosingClass.is_null() ? "" : ground.enclosingClass.value("usr","");
        json res = cpptool::queryComplexityHotspot(ctx.compileCommandsPath, scopeFile, classUsr);
        if (res.value("ok", false)) {
            auto& hs = res["hotspots"];
            text = "UNDERSTAND: хотспоты сложности (" + std::to_string(hs.size()) + "):";
            int shown = 0;
            for (auto& h : hs) {
                if (shown++ >= 5) break;
                text += " [" + h.value("signature","?") + " score=" + std::to_string(h.value("score",0)) +
                        " lines=" + std::to_string(h.value("body_lines",0)) +
                        " nest=" + std::to_string(h.value("max_nesting",0)) +
                        " @" + h.value("file","") + ":" + std::to_string(h.value("line",0)) + "]";
                refs.push_back(json{{"file", h.value("file","")}, {"line", h.value("line",0)}});
            }
            text += " Код на диске не меняется.";
        } else {
            text = "UNDERSTAND: COMPLEXITY не удался — " + res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::QUERY_DUPLICATES: {
        std::string scopeFile = (!file.empty() && ground.enclosingClass.is_null()) ? file : "";
        std::string classUsr = ground.enclosingClass.is_null() ? "" : ground.enclosingClass.value("usr","");
        json res = cpptool::queryDuplicateFind(ctx.compileCommandsPath, scopeFile, classUsr);
        if (res.value("ok", false)) {
            auto& g = res["groups"];
            text = "UNDERSTAND: групп с одинаковой сигнатурой (кандидаты в дубли): " + std::to_string(g.size()) + ".";
            for (auto& grp : g) {
                text += " [" + grp.value("signature","?") + " — " + std::to_string(grp["members"].size()) + " шт]";
                for (auto& mem : grp["members"])
                    refs.push_back(json{{"file", mem.value("file","")}, {"line", mem.value("line",0)}});
            }
            text += " Код на диске не меняется.";
        } else {
            text = "UNDERSTAND: DUPLICATE_FIND не удался — " + res.value("error", json::object()).value("message", "ошибка");
        }
        break;
    }

    case Action::NOT_IMPLEMENTED_YET:
        // Честно: Detail распознан, механизм для него ещё не написан — не
        // угадываем и не подменяем сигнатурой, которая не отвечает на вопрос.
        return json{
            {"protocol_version","1.0"},
            {"type","answer"},
            {"answered_by", answeredBy(ctx)},
            {"primitive", ctx.primitiveStr},
            {"answer", {{"text", "UNDERSTAND: запрос понят как " + detailToString(ctx.guess.detail) +
                                  ", но механизм для него ещё не реализован (Категория B, см. arch.md). "
                                  "Сигнатура под курсором: " + sig}, {"refs", refs}}},
            {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine", "UNDERSTAND"}, {"detail", detailToString(ctx.guess.detail)}, {"cursor_context", ctxStr}, {"action", "NOT_IMPLEMENTED_YET"}}}
        };

    case Action::REFUSE_TOO_BROAD:
        return json{
            {"protocol_version","1.0"},
            {"type","answer"},
            {"answered_by", answeredBy(ctx)},
            {"primitive", ctx.primitiveStr},
            {"answer", {{"text", "UNDERSTAND: " + detailToString(ctx.guess.detail) + " признан превышающим возможности "
                                  "текущего анализа (Категория C, см. arch.md) — явный отказ вместо недостоверного ответа."}, {"refs", json::array()}}},
            {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine", "UNDERSTAND"}, {"detail", detailToString(ctx.guess.detail)}, {"cursor_context", ctxStr}, {"action", "REFUSE_TOO_BROAD"}}}
        };

    case Action::DELEGATE_TO_MAIN: {
        // Read-only reasoning: зовём main-модель БЕЗ Execute/Verify — ни один
        // файл на диске не меняется. Раньше была честная заглушка "не
        // реализовано"; теперь слой 4 реально дотягивается до main-модели.
        // Сюда попадают GENERAL_QUESTION / VIRTUAL_REFACTOR_SIM / FEASIBILITY_CHECK.
        // Для вопроса "как устроено" структура класса (сигнатуры+иерархия) важнее
        // одного тела — и дешевле по токенам, чем локальное окно.
        json plan = callMain(ctx, file, sig, enclosingUsr, classStructureFor(ctx));
        json answer;
        answer["warnings"] = groundWarnings(ctx);
        answer["refs"] = refs;
        // Роль отвечающего — "main" (не light/stub): ответ даёт основная модель
        // (или deterministic_fallback, если она недоступна).
        bool delegatedOk = !planFailed(plan);
        std::string byRole = ctx.guess.modelUnavailable ? "deterministic_fallback" : "main";
        if (delegatedOk) {
            answer["text"] = plan.value("plan", "");
            answer["rationale"] = plan.value("rationale", "");
        } else {
            answer["text"] = "UNDERSTAND: " + detailToString(ctx.guess.detail) +
                             " требует reasoning основной модели, но она не справилась: " +
                             plan.value("error", "main model unavailable") +
                             ". Сигнатура под курсором: " + sig;
            answer["error"] = plan.value("error", "main model unavailable");
        }
        json out = json{
            {"protocol_version","1.0"},
            {"type","answer"},
            {"answered_by", json{{"role", byRole}}},
            {"primitive", ctx.primitiveStr},
            {"answer", answer},
            {"debug_facts", {
                {"primitive", ctx.primitiveStr},
                {"rationale", ctx.guess.rationale},
                {"state_machine", "UNDERSTAND"},
                {"detail", detailToString(ctx.guess.detail)},
                {"cursor_context", ctxStr},
                {"relevance", relevanceToString(relevance)},
                {"action", "DELEGATE_TO_MAIN"},
                {"read_only", true},
                {"model_ok", delegatedOk},
                {"delegated_role", "main"}
            }}
        };
        if (!delegatedOk)
            out["debug_facts"]["model_error"] = plan.value("error", "main model unavailable");
        return out;
    }

    case Action::NEEDS_INPUT_RELEVANCE:
    case Action::NEEDS_INPUT_CURSOR:
        // ВАЖНО (сказано честно, не спрятано только в код-комментарии):
        // этот needs_input НЕ проходит через orchestrator::decorateNeedsInput
        // (он оборачивает только то, что рождается внутри run(), не то, что
        // возвращает handler->handle()) — значит у него нет history_entry/
        // depth-лимита, и orchestrator не распознает ответ на него через
        // clarification_history. Рабочий прототип, не законченная интеграция.
        return json{
            {"protocol_version","1.0"},
            {"type","needs_input"},
            {"needs_input", {
                {"question_id", "understand-relevance-1"},
                {"text", "Место под курсором (" + ctxStr + ") не похоже на то, что описывает задача (" +
                         detailToString(ctx.guess.detail) + "). Уточните, что нужно."},
                {"options", json::array()},
                {"free_text_allowed", true}
            }},
            {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine", "UNDERSTAND"}, {"detail", detailToString(ctx.guess.detail)}, {"cursor_context", ctxStr}, {"relevance", relevanceToString(relevance)}, {"action", actionToString(action)}, {"integration_note", "needs_input из handler не декорируется orchestrator'ом — см. комментарий в коде"}}}
        };

    default:
        text = "UNDERSTAND: " + sig + ". Код на диске не меняется. rationale: " + ctx.guess.rationale;
        break;
    }

    return json{
        {"protocol_version","1.0"},
        {"type","answer"},
        {"answered_by", answeredBy(ctx)},
        {"primitive", ctx.primitiveStr},
        {"answer", {{"text", text}, {"refs", refs}}},
        {"debug_facts", {
            {"primitive",      ctx.primitiveStr},
            {"rationale",      ctx.guess.rationale},
            {"state_machine",  "UNDERSTAND"},
            {"detail",         detailToString(ctx.guess.detail)},
            {"cursor_context", ctxStr},
            {"relevance",      relevanceToString(relevance)},
            {"action",         actionToString(action)},
            {"enclosing_class", ground.enclosingClass}
        }}
    };
}

json SandboxFixHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    std::string text = "SANDBOX_FIX: " + sig + ". Plan(local) -> Verify(compile). rationale: " + ctx.guess.rationale + groundNote(ctx);
    if (g.suggestedMode != "sandbox") text += " [scale расхождение: " + g.suggestedMode + "]";
    json plan = callMain(ctx, file, sig, usr);
    if (planFailed(plan)) return mainPlanFailedError(ctx, plan);
    auto edits = actionsFromPlan(plan, file, cursorLineOf(ctx));
    json execJson = runExecute(edits);
    if (!execJson.value("ok", false)) return executeFailedError(ctx, execJson);
    json answer;
    answer["text"] = text;
    answer["warnings"] = groundWarnings(ctx);
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    answer["plan"] = plan;
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"skipped", vres.skipped}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    if(!vres.ok) return verifyFailedError(ctx, execJson, checkpoint, verify);
    return json{
        {"protocol_version","1.0"},
        {"type","answer"},
        {"answered_by", answeredBy(ctx)},
        {"primitive", ctx.primitiveStr},
        {"answer", answer},
        {"execute", execJson},
        {"checkpoint", checkpoint},
        {"verify", verify},
        {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine","SANDBOX_FIX"}, {"expected_mode","sandbox"}, {"final_mode", ctx.explicitMode.empty()?"sandbox":ctx.explicitMode}, {"scale", g.scaleFacts}, {"plan_note","Plan+Execute+Verify S4.1 (actions от MainModel, git ветки — твои)"}}}
    };
}

json QuarryDesignHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    // S4.1: ветки теперь на тебе (пользователе). Агент не спрашивает, просто Plan multi-file + Execute/Verify.
    // Никакого needs_input / branch_required — ты уже на нужной ветке.
    std::string finalMode = ctx.explicitMode.empty() ? "quarry" : ctx.explicitMode;
    json debugFacts = json{{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine","QUARRY_DESIGN"}, {"expected_mode","quarry"}, {"final_mode", finalMode}, {"scale", g.scaleFacts}, {"plan_note","Plan(multi-file)+Verify S4.1 (Execute ждёт actions, git ветки — твои)"}};
    std::string text = "QUARRY_DESIGN: " + sig + ". Режим: " + finalMode + " (Plan multi-file S4.1). rationale: " + ctx.guess.rationale + groundNote(ctx);
    json answer;
    answer["text"] = text;
    answer["warnings"] = groundWarnings(ctx);
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    json qPlan = callMain(ctx, file, sig, usr);
    if (planFailed(qPlan)) return mainPlanFailedError(ctx, qPlan);
    answer["plan"] = qPlan;
    auto edits = actionsFromPlan(answer["plan"], file, cursorLineOf(ctx));
    json execJson = runExecute(edits);
    if (!execJson.value("ok", false)) return executeFailedError(ctx, execJson);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"skipped", vres.skipped}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    if(!vres.ok) return verifyFailedError(ctx, execJson, checkpoint, verify);
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",debugFacts}};
}

json TestGenHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    std::string text = "TEST_GEN: " + sig + ". Plan(tests) S4.1. rationale: " + ctx.guess.rationale + groundNote(ctx);
    json answer;
    answer["text"] = text;
    answer["warnings"] = groundWarnings(ctx);
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    json tPlan = callMain(ctx, file, sig, usr);
    if (planFailed(tPlan)) return mainPlanFailedError(ctx, tPlan);
    answer["plan"] = tPlan;
    auto edits = actionsFromPlan(answer["plan"], file, cursorLineOf(ctx));
    json execJson = runExecute(edits);
    if (!execJson.value("ok", false)) return executeFailedError(ctx, execJson);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyTests("");
    json verify = {{"tests_ok", vres.ok}, {"skipped", vres.skipped}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    if(!vres.ok) return verifyFailedError(ctx, execJson, checkpoint, verify);
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",{{"primitive",ctx.primitiveStr},{"rationale",ctx.guess.rationale},{"state_machine","TEST_GEN"},{"scale",g.scaleFacts},{"plan_note","Plan+Execute+Verify(ctest) S4.1"}}}};
}

json ExperimentHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    std::string text = "EXPERIMENT: " + sig + ". Plan + checkpoint S4.1. rationale: " + ctx.guess.rationale + groundNote(ctx);
    json answer;
    answer["text"] = text;
    answer["warnings"] = groundWarnings(ctx);
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    json ePlan = callMain(ctx, file, sig, usr);
    if (planFailed(ePlan)) return mainPlanFailedError(ctx, ePlan);
    answer["plan"] = ePlan;
    auto edits = actionsFromPlan(answer["plan"], file, cursorLineOf(ctx));
    json execJson = runExecute(edits);
    if (!execJson.value("ok", false)) return executeFailedError(ctx, execJson);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"skipped", vres.skipped}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    if(!vres.ok) return verifyFailedError(ctx, execJson, checkpoint, verify);
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",{{"primitive",ctx.primitiveStr},{"rationale",ctx.guess.rationale},{"state_machine","EXPERIMENT"},{"scale",g.scaleFacts},{"plan_note","Plan+Execute+checkpoint+Verify S4.1 (бекапы по checkpointId, undo доступен)"}}}};
}

namespace {

// Определяем команду по goal_text: cmake --build, ninja, cmake (конфигурация), или compile_commands
struct InfraCmd {
    std::string command;   // shell-команда БЕЗ пайпов вида | tail — статус чужой команды не должен подменять $?
    std::string kind;      // "build" | "configure" | "compile_commands" | "graph" | "unknown"
    std::string label;     // человекочитаемое описание
    int tailLines = 0;     // сколько последних строк лога оставить (0 = весь лог)
};

std::string toLowerInfra(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
    return r;
}

InfraCmd resolveInfraCmd(const std::string& goalText, const std::string& buildDir) {
    std::string t = toLowerInfra(goalText);
    // compile_commands.json — нужен отдельный cmake re-configure.
    // buildDir уже сконфигурирован (там лежит CMakeCache.txt), поэтому -B без
    // отдельного source-dir корректно берёт source из кэша этой же директории.
    if(t.find("compile_commands") != std::string::npos) {
        std::string cmd = buildDir.empty() ? "" :
            "cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B " + buildDir;
        return {cmd, "compile_commands", "cmake re-configure (EXPORT_COMPILE_COMMANDS=ON)", 40};
    }
    // граф зависимостей / таргетов
    if(t.find("граф") != std::string::npos || t.find("target") != std::string::npos || t.find("таргет") != std::string::npos) {
        std::string cmd = buildDir.empty() ? "" : "cmake --build " + buildDir + " --target help";
        return {cmd, "graph", "cmake --target help (список таргетов)", 60};
    }
    // пересборка: ninja если есть build.ninja, иначе cmake --build
    if(!buildDir.empty()) {
        namespace fs = std::filesystem;
        bool hasNinja = fs::exists(fs::path(buildDir) / "build.ninja");
        if(hasNinja) return {"ninja -C " + buildDir + " -j4", "build", "ninja -j4", 80};
        return {"cmake --build " + buildDir + " -j4", "build", "cmake --build -j4", 80};
    }
    return {"", "unknown", "buildDir неизвестен — команда не определена", 0};
}

// ВАЖНО: cmd не должен содержать собственный '| tail'/'| head' — иначе $? в fullCmd
// будет кодом возврата ПОСЛЕДНЕЙ команды пайпа, а не самой сборки (ловили баг:
// упавший ninja через '| tail' всегда отдавал exit 0). Обрезка лога — отдельно, ниже.
std::string execCaptureInfra(const std::string& cmd, int tailLines, int& exitCode) {
    std::array<char, 4096> buf{};
    std::string out;
    // { cmd; } без сабшелла — $? это статус самой команды, а не обёртки
    std::string fullCmd = "{ " + cmd + "; } 2>&1; echo __EXIT__:$?";
    FILE* p = popen(fullCmd.c_str(), "r");
    if (!p) { exitCode = -1; return "popen failed"; }
    while (fgets(buf.data(), static_cast<int>(buf.size()), p)) out += buf.data();
    pclose(p);
    exitCode = 0;
    auto pos = out.rfind("__EXIT__:");
    if (pos != std::string::npos) {
        std::string codeStr = out.substr(pos + 9);
        while (!codeStr.empty() && (codeStr.back() == '\n' || codeStr.back() == '\r' || codeStr.back() == ' '))
            codeStr.pop_back();
        try { exitCode = std::stoi(codeStr); } catch(...) { exitCode = 0; }
        out = out.substr(0, pos);
    }
    if (tailLines > 0) {
        std::vector<std::string> lines;
        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) lines.push_back(line);
        if ((int)lines.size() > tailLines) lines.erase(lines.begin(), lines.end() - tailLines);
        out.clear();
        for (auto& l : lines) { out += l; out += '\n'; }
    }
    return out;
}

} // anon (infra helpers)

json InfraHandler::handle(const HandlerContext& ctx) {
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    std::string goalText = ctx.task.value("goal_text", "");
    InfraCmd infra = resolveInfraCmd(goalText, buildDir);

    json result;
    result["kind"] = infra.kind;
    result["label"] = infra.label;
    result["buildDir"] = buildDir;

    if (infra.command.empty()) {
        result["ok"] = false;
        result["error"] = "buildDir не определён — передайте --cc с путём к compile_commands.json рядом с build-директорией";
        result["log"] = "";
    } else {
        int exitCode = 0;
        std::string log = execCaptureInfra(infra.command, infra.tailLines, exitCode);
        // Эвристика успеха: exit code 0 И нет "Error:" / "error:" в логе
        bool hasError = (log.find("error:") != std::string::npos || log.find("Error:") != std::string::npos);
        bool ok = (exitCode == 0) && !hasError;
        result["ok"] = ok;
        result["exit_code"] = exitCode;
        // Ограничиваем лог чтобы не раздувать JSON
        result["log"] = log.size() > 3000 ? log.substr(log.size() - 3000) : log;
        if (!ok) {
            result["error"] = exitCode != 0 
                ? ("команда завершилась с кодом " + std::to_string(exitCode))
                : "в выводе обнаружены ошибки";
        }
    }

    std::string answerText = "INFRA [" + infra.kind + "]: " + infra.label;
    if (result.value("ok", false))
        answerText += " — успешно";
    else
        answerText += " — " + result.value("error", "ошибка");
    answerText += ". rationale: " + ctx.guess.rationale;

    return json{
        {"protocol_version", "1.0"},
        {"type", "answer"},
        {"answered_by", answeredBy(ctx)},
        {"primitive", ctx.primitiveStr},
        {"answer", {
            {"text", answerText},
            {"refs", json::array()},
            {"infra", result}
        }},
        {"debug_facts", {
            {"primitive", ctx.primitiveStr},
            {"rationale", ctx.guess.rationale},
            {"state_machine", "INFRA"},
            {"buildDir", buildDir},
            {"command", infra.command},
            {"kind", infra.kind}
        }}
    };
}

} // namespace cppagent
