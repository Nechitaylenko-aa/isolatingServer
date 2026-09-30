#include "handlers.h"
#include <filesystem>
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

std::string buildDirFromCC(const std::string& cc){ namespace fs=std::filesystem; fs::path p(cc); if(p.has_parent_path()) return p.parent_path().string(); return ""; }

json callMain(const HandlerContext& ctx, const std::string& file, const std::string& sig, const std::string& usr){
    GenerateRequest req{ctx.primitiveStr, ctx.task.value("goal_text",""), file, sig, usr, ctx.ground.scaleFacts, ctx.explicitMode};
    GenerateResult res;
    if(ctx.mainModel) res = ctx.mainModel->generate(req);
    else { StubMainModelBroker stub; res = stub.generate(req); }
    json j;
    if(res.ok){ j["plan"]=res.planText; j["rationale"]=res.rationale; j["actions"]=res.actions; }
    else { j["error"]=res.error; j["modelUnavailable"]=res.modelUnavailable; j["actions"]=json::array(); }
    return j;
}
std::vector<EditAction> actionsFromPlan(const json& plan){
    std::vector<EditAction> out;
    if(!plan.contains("actions") || !plan["actions"].is_array()) return out;
    for(auto& a: plan["actions"]){
        if(!a.contains("file") || !a["file"].is_string()) continue;
        EditAction e; e.file=a["file"].get<std::string>();
        e.oldText=a.value("oldText",""); e.newText=a.value("newText","");
        out.push_back(std::move(e));
    }
    return out;
}
json runExecute(const std::vector<EditAction>& edits){
    ExecuteService svc;
    auto r = svc.apply(edits);
    json j={{"ok",r.ok},{"appliedFiles",r.appliedFiles},{"checkpointId",r.checkpointId}};
    if(!r.error.empty()) j["error"]=r.error;
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
json UnderstandHandler::handle(const HandlerContext& ctx) {
    const auto& ground = ctx.ground;
    if (ground.enclosingMethod.is_null()) {
        return json{
            {"protocol_version","1.0"},
            {"type","answer"},
            {"answered_by", answeredBy(ctx)},
            {"primitive", ctx.primitiveStr},
            {"answer", {{"text", "UNDERSTAND: под курсором не нашёлся метод/функция. rationale: " + ctx.guess.rationale}, {"refs", json::array()}}},
            {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine", "UNDERSTAND"}}}
        };
    }
    std::string sig = ground.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    json refs = json::array({ json{{"usr", ground.enclosingMethod.value("usr", "")}, {"file", file}, {"line", ground.enclosingMethod.value("line", 0)}} });
    return json{
        {"protocol_version","1.0"},
        {"type","answer"},
        {"answered_by", answeredBy(ctx)},
        {"primitive", ctx.primitiveStr},
        {"answer", {{"text", "UNDERSTAND: " + sig + ". Код на диске не меняется. rationale: " + ctx.guess.rationale}, {"refs", refs}}},
        {"debug_facts", {{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine","UNDERSTAND"}, {"enclosing_class", ground.enclosingClass}}}
    };
}

json SandboxFixHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    // TODO(S2): GroundService; TODO(S4): Plan(local) via MainModel (12 GiB) -> Execute -> Verify(compile)
    std::string text = "SANDBOX_FIX: " + sig + ". Plan(local) -> Verify(compile) [TODO: MainModel not yet wired, S4]. rationale: " + ctx.guess.rationale;
    if (g.suggestedMode != "sandbox") text += " [scale расхождение: " + g.suggestedMode + "]";
    json plan = callMain(ctx, file, sig, usr);
    auto edits = actionsFromPlan(plan);
    json execJson = runExecute(edits);
    json answer;
    answer["text"] = text;
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    answer["plan"] = plan;
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
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
    std::string text = "QUARRY_DESIGN: " + sig + ". Режим: " + finalMode + " (Plan multi-file S4.1). rationale: " + ctx.guess.rationale;
    json answer;
    answer["text"] = text;
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    answer["plan"] = callMain(ctx, file, sig, usr);
    auto edits = actionsFromPlan(answer["plan"]);
    json execJson = runExecute(edits);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",debugFacts}};
}

json TestGenHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    std::string text = "TEST_GEN: " + sig + ". Plan(tests) S4.1. rationale: " + ctx.guess.rationale;
    json answer;
    answer["text"] = text;
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    answer["plan"] = callMain(ctx, file, sig, usr);
    auto edits = actionsFromPlan(answer["plan"]);
    json execJson = runExecute(edits);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyTests("");
    json verify = {{"tests_ok", vres.ok}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",{{"primitive",ctx.primitiveStr},{"rationale",ctx.guess.rationale},{"state_machine","TEST_GEN"},{"scale",g.scaleFacts},{"plan_note","Plan+Execute+Verify(ctest) S4.1"}}}};
}

json ExperimentHandler::handle(const HandlerContext& ctx) {
    const auto& g = ctx.ground;
    std::string sig = g.enclosingMethod.is_null() ? "?" : g.enclosingMethod.value("signature", "?");
    std::string file = ctx.task.contains("cursor") ? ctx.task["cursor"].value("file", "") : "";
    std::string usr = g.enclosingMethod.is_null() ? "" : g.enclosingMethod.value("usr", "");
    std::string text = "EXPERIMENT: " + sig + ". Plan + checkpoint S4.1. rationale: " + ctx.guess.rationale;
    json answer;
    answer["text"] = text;
    answer["refs"] = json::array({ json{{"usr", usr}, {"file", file}, {"line", g.enclosingMethod.is_null()?0:g.enclosingMethod.value("line",0)}} });
    answer["plan"] = callMain(ctx, file, sig, usr);
    auto edits = actionsFromPlan(answer["plan"]);
    json execJson = runExecute(edits);
    std::string buildDir = buildDirFromCC(ctx.compileCommandsPath);
    VerifyService verifier(buildDir);
    auto vres = verifier.verifyCompile(file);
    json verify = {{"compile_ok", vres.ok}, {"log", vres.log.substr(0,800)}};
    if(!vres.ok) verify["error"] = vres.error;
    json checkpoint = {{"id", execJson.value("checkpointId","")}, {"undo_available", !execJson.value("checkpointId","").empty()}};
    return json{{"protocol_version","1.0"},{"type","answer"},{"answered_by",answeredBy(ctx)},{"primitive",ctx.primitiveStr},{"answer",answer},{"execute",execJson},{"checkpoint",checkpoint},{"verify",verify},{"debug_facts",{{"primitive",ctx.primitiveStr},{"rationale",ctx.guess.rationale},{"state_machine","EXPERIMENT"},{"scale",g.scaleFacts},{"plan_note","Plan+Execute+checkpoint+Verify S4.1 (бекапы по checkpointId, undo доступен)"}}}};
}

json InfraHandler::handle(const HandlerContext& ctx) {
    // INFRA — без Ground по коду, код на диске не меняется
    // TODO(S2): exec инструмента (cmake/ninja/compile_commands) как отдельный сервис
    return json{
        {"protocol_version","1.0"},
        {"type","answer"},
        {"answered_by", answeredBy(ctx)},
        {"primitive", ctx.primitiveStr},
        {"answer", json{{"text", "INFRA: команда окружения/сборки. Логика .h/.cpp не меняется. rationale: " + ctx.guess.rationale}, {"refs", json::array()}, {"todo", "exec cmake/ninja заглушка — S2"}}},
        {"debug_facts", json{{"primitive", ctx.primitiveStr}, {"rationale", ctx.guess.rationale}, {"state_machine","INFRA"}}}
    };
}

} // namespace cppagent
