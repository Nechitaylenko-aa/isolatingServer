#include "blast_radius.h"
#include "tu_cache.h"
#include "queries.h"
#include <filesystem>

namespace cpptool {

json queryBlastRadius(const std::string& file,
                      const std::string& className,
                      const std::string& methodUSR,
                      const std::vector<std::string>& flags) {
    json outline = json{{"ok", false}};
    json refs = json{{"ok", false}};
    if (!className.empty()) outline = queryClassOutline(file, className, flags);
    if (!methodUSR.empty()) refs = querySymbolRefs(methodUSR, file, flags);
    int pm=0, refsInTU=0;
    if(outline.value("ok",false) && outline.contains("public_methods")) pm=(int)outline["public_methods"].size();
    if(refs.value("ok",false) && refs.contains("refs")) refsInTU=(int)refs["refs"].size();
    bool outlineOk=outline.value("ok",false), refsOk=refs.value("ok",false);
    if(className.empty() && methodUSR.empty())
        return json{{"ok",false},{"error",{{"code","blast_radius_no_inputs"},{"message","ни className ни methodUSR не заданы"}}}};
    if(!outlineOk && !refsOk && !className.empty() && !methodUSR.empty()){
        if(outline.contains("error")) return outline;
        if(refs.contains("error")) return refs;
    }
    // pm (методы класса) — характеристика класса, не правки. Убираем как
    // самостоятельный критерий: 15-методный класс с 1 ссылкой — это sandbox.
    // Single-TU: нет данных о размере проекта, используем только абсолютный порог refs.
    bool quarry=(refsInTU>=8);
    return json{{"ok",true},{"suggested_mode",quarry?"quarry":"sandbox"},{"facts",json{{"public_methods_in_class",pm},{"refs_found_in_same_tu",refsInTU},{"class_outline_ok",outlineOk},{"symbol_refs_ok",refsOk},{"scope","single_tu"},{"note","blast_radius v1 single-TU"}}},{"outline",outline},{"refs",refs}};
}

json queryBlastRadiusCross(const std::string& compileCommandsPath,
                           const std::string& file,
                           const std::string& className,
                           const std::string& methodUSR,
                           const std::vector<std::string>& flags,
                           const std::string& cachePath) {
    // public_methods — всё равно из одного TU (класс один)
    json outline = json{{"ok", false}};
    if(!className.empty()) outline = queryClassOutline(file, className, flags);
    int pm=0;
    if(outline.value("ok",false) && outline.contains("public_methods")) pm=(int)outline["public_methods"].size();
    bool outlineOk=outline.value("ok",false);

    int totalRefs=0;
    int distinctFiles=0;
    int fromCache=0, scanned=0, total=0;
    bool refsOk=true;
    std::string refsError;
    if(!methodUSR.empty() && !compileCommandsPath.empty()){
        // Кэш живёт рядом с compile_commands.json: build-каталог проекта.
        std::filesystem::path ccDir = std::filesystem::path(compileCommandsPath).parent_path();
        std::string cp = cachePath;
        if(cp.empty())
            cp = (ccDir / ".cpp-tool-cache" / "index.db").string();
        CTuCache cache(cp);
        if(!cache.opened()){
            refsOk=false;
            refsError="не удалось открыть кэш " + cp + ": " + cache.open_error();
        } else {
            auto res = cache.get_refs_for_usr(compileCommandsPath, methodUSR);
            if(!res.ok){ refsOk=false; refsError=res.error; }
            else { totalRefs=res.totalRefs; distinctFiles=res.distinctFilesWithRefs; fromCache=res.fromCache; scanned=res.filesScanned; total=res.filesTotal; }
        }
    } else if(!methodUSR.empty()){
        // нет compile_commands — фолбэк на single TU
        json refs = querySymbolRefs(methodUSR, file, flags);
        refsOk=refs.value("ok",false);
        if(refsOk && refs.contains("refs")) totalRefs=(int)refs["refs"].size();
        else if(!refsOk && refs.contains("error")) refsError=refs["error"].value("message","symbol_refs failed");
    }

    if(className.empty() && methodUSR.empty())
        return json{{"ok",false},{"error",{{"code","blast_radius_no_inputs"},{"message","ни className ни methodUSR не заданы"}}}};

    // Адаптивные пороги — зеркало логики оркестратора (defaultAbs/defaultRel).
    // pm (методы класса) убран: это свойство класса, а не масштаб конкретной правки.
    // quarry = правка затронет много мест В ПРОЕКТЕ: абсолютно много refs ИЛИ
    // относительно много файлов (доля затронутых файлов от всех).
    int absThreshold;
    if(total <= 5)        absThreshold = 8;
    else if(total <= 20)  absThreshold = 15;
    else if(total <= 100) absThreshold = 30;
    else                  absThreshold = 60;
    double relThreshold;
    if(total <= 3)        relThreshold = 1.01; // отключаем relative на крошечных проектах
    else if(total <= 10)  relThreshold = 0.5;
    else if(total <= 50)  relThreshold = 0.3;
    else                  relThreshold = 0.15;
    double frac = (total > 0) ? (double)distinctFiles / total : 0.0;
    bool quarry = (totalRefs >= absThreshold) || (total > 3 && frac >= relThreshold);
    json facts=json{
        {"public_methods_in_class",pm},
        {"refs_found_total",totalRefs},
        {"distinct_files_with_refs",distinctFiles},
        {"total_files_in_project",total},
        {"refs_from_cache",fromCache},{"refs_files_scanned",scanned},{"refs_files_total",total},
        {"class_outline_ok",outlineOk},{"symbol_refs_ok",refsOk},{"scope","cross_tu_cached"},{"note","blast_radius cross-TU via CTuCache (SQLite)"}
    };
    if(!refsError.empty()) facts["refs_error"]=refsError;

    return json{{"ok",true},{"suggested_mode",quarry?"quarry":"sandbox"},{"facts",facts},{"outline",outline}};
}

} // namespace cpptool
