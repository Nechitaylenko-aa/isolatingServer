#include "ground_service.h"
#include "blast_radius.h"
#include "compile_commands.h"
#include "queries.h"

namespace cppagent {

bool GroundService::resolveFlags(const std::string& file, std::vector<std::string>& outFlags, json& outError) {
    json ff = cpptool::queryFileFlags(compileCommandsPath_, file);
    if (!ff.value("ok", false)) { outError = ff; return false; }
    if (ff.contains("flags") && ff["flags"].is_array())
        for (auto& f : ff["flags"]) outFlags.push_back(f.get<std::string>());
    return true;
}

GroundResult GroundService::buildGround(const std::string& file, int line, int col) {
    GroundResult g;
    std::vector<std::string> flags;
    json flagsErr;
    if (!resolveFlags(file, flags, flagsErr)) { g.ok=false; g.error=flagsErr; return g; }
    g.locate = cpptool::queryLocateSymbol(file, line, col, flags);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    std::string usr = g.enclosingMethod.is_null()?"":g.enclosingMethod.value("usr","");
    std::string className;
    if(!g.enclosingClass.is_null()) className=g.enclosingClass.value("name","");
    // S3 cross-TU с кэшем: ProjectIndex (L1 mem + L2 JsonFileStore)
    json br = cpptool::queryBlastRadiusCross(compileCommandsPath_, file, className, usr, flags);
    if(br.value("ok",false)){
        g.suggestedMode = br.value("suggested_mode","sandbox");
        g.scaleFacts = br.value("facts", json::object());
        if(br.contains("outline")) g.classOutline = br["outline"];
        // refs не храним целиком — в кэше они по всем файлам, в facts уже агрегат
    } else {
        g.suggestedMode="sandbox";
        g.scaleFacts=json{{"error",br.value("error",json::object())},{"note","blast_radius failed, fallback sandbox"}};
    }
    return g;
}

GroundResult GroundService::buildGroundLight(const std::string& file, int line, int col) {
    GroundResult g;
    std::vector<std::string> flags;
    json flagsErr;
    if (!resolveFlags(file, flags, flagsErr)) { g.ok=false; g.error=flagsErr; return g; }
    g.locate = cpptool::queryLocateSymbol(file, line, col, flags);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    g.suggestedMode="sandbox";
    g.scaleFacts=json{{"note","light ground: blast_radius not computed"}};
    return g;
}

} // namespace cppagent
