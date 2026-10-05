#include "task_router.h"

namespace cppagent {

std::string executorToString(Executor e) {
    switch (e) {
    case Executor::NONE: return "NONE";
    case Executor::LOCAL: return "LOCAL";
    case Executor::SPLIT: return "SPLIT";
    case Executor::REMOTE: return "REMOTE";
    case Executor::REFUSE_LOCAL_INCAPABLE: return "REFUSE_LOCAL_INCAPABLE";
    }
    return "UNKNOWN";
}

namespace {
bool isReadOnlyDetail(Detail d) {
    switch (d) {
    case Detail::GENERAL_QUESTION:
    case Detail::SIMPLE_READ:
        return true;
    default:
        return false;
    }
}
bool isWriteDetail(Detail d) {
    switch (d) {
    case Detail::LOCAL_FIX:
    case Detail::RENAME:
    case Detail::SIGNATURE_CHANGE:
    case Detail::ADD_OVERRIDE:
    case Detail::EXTRACT_METHOD:
    case Detail::INLINE_METHOD:
    case Detail::EXTRACT_CLASS:
    case Detail::MOVE_METHOD:
    case Detail::DEAD_CODE_REMOVAL:
    case Detail::REMOVE_UNUSED_INCL:
    case Detail::ADD_METHOD:
    case Detail::ADD_CLASS:
    case Detail::NEW_INTERFACE:
    case Detail::API_DESIGN:
    case Detail::APPLY_PATTERN:
    case Detail::SPLIT_CLASS:
    case Detail::DESIGN_HIERARCHY:
    case Detail::UNIT_TEST:
    case Detail::INTEGRATION_TEST:
    case Detail::GEN_MOCKS:
    case Detail::COVERAGE_GAP:
    case Detail::PROTOTYPE:
    case Detail::BENCHMARK:
    case Detail::TRY_ALTERNATIVE:
    case Detail::BUILD_FIX:
    case Detail::BUILD_CONFIG:
    case Detail::DEPS_UPDATE:
    case Detail::CI_PIPELINE:
        return true;
    default:
        return false;
    }
}
// Все запросы, которые решаются детерминированно (libclang/кэш) — без модели.
bool isDeterministicDetail(Detail d) {
    switch (d) {
    case Detail::FIND_IMPLEMENTATIONS:
    case Detail::CALL_CHAIN:
    case Detail::CALL_GRAPH_SUBTREE:
    case Detail::DEPENDENCY_MAP:
    case Detail::TYPE_USAGE:
    case Detail::TODO_SCAN:
    case Detail::DOC_CHECK:
    case Detail::CODE_SMELL:
    case Detail::COMPLEXITY_HOTSPOT:
    case Detail::DUPLICATE_FIND:
    case Detail::ANALYZE_INTERFACE:
    case Detail::DIFF_INTERFACE_IMPL:
        return true;
    default:
        return false;
    }
}
// Категория C — всегда за пределами локальной модели.
bool isCategoryC(Detail d) {
    switch (d) {
    case Detail::SECURITY_AUDIT:
    case Detail::INPUT_VALIDATION:
    case Detail::THREADING_SAFE:
    case Detail::MODERNIZE:
    case Detail::API_MIGRATION:
    case Detail::PORT_PLATFORM:
    case Detail::EXCEEDS_LOCAL_AI:
        return true;
    default:
        return false;
    }
}
// Задачи, где масштаб заведомо превышает «одну правку» -> сразу remote.
bool isLargeRefactor(Detail d) {
    switch (d) {
    case Detail::EXTRACT_CLASS:
    case Detail::SPLIT_CLASS:
    case Detail::DESIGN_HIERARCHY:
    case Detail::NEW_INTERFACE:
    case Detail::API_DESIGN:
    case Detail::APPLY_PATTERN:
    case Detail::MOVE_METHOD:
        return true;
    default:
        return false;
    }
}
} // namespace

RouteDecision routeTask(const RouteInput& in) {
    RouteDecision r;

    // (1) Детерминированное — вообще без модели.
    if (isDeterministicDetail(in.detail)) {
        r.executor = Executor::NONE;
        r.reason = "запрос решается libclang/кэшом, модель не нужна";
        return r;
    }

    // (2) Категория C — только remote.
    if (isCategoryC(in.detail)) {
        if (in.remoteAvailable) { r.executor = Executor::REMOTE; r.reason = "Категория C: превышает локальную модель"; }
        else { r.executor = Executor::REFUSE_LOCAL_INCAPABLE; r.reason = "Категория C, remote недоступен — локальная не потянет"; }
        return r;
    }

    // (3) Крупный рефакторинг/архитектура — remote (не «ждать ошибки»).
    if (isLargeRefactor(in.detail) || in.primitiveStr == "QUARRY_DESIGN") {
        if (in.remoteAvailable) { r.executor = Executor::REMOTE; r.reason = "крупный рефакторинг/архитектура -> remote"; }
        else { r.executor = Executor::REFUSE_LOCAL_INCAPABLE; r.reason = "крупный рефакторинг, remote недоступен"; }
        return r;
    }

    // (4) Чтение-обзор (GENERAL_QUESTION/SIMPLE_READ) — локальная справляется.
    if (isReadOnlyDetail(in.detail)) {
        if (in.localCapable) { r.executor = Executor::LOCAL; r.reason = "чтение/обзор — локальная модель"; }
        else if (in.remoteAvailable) { r.executor = Executor::REMOTE; r.reason = "чтение, но local не способен -> remote"; }
        else { r.executor = Executor::REFUSE_LOCAL_INCAPABLE; r.reason = "чтение, local не способен, remote нет"; }
        return r;
    }

    // (5) Запись. Оцениваем масштаб: профиль local + blast radius.
    if (isWriteDetail(in.detail)) {
        bool tooWide = (in.distinctFiles > in.localMaxEditFiles) ||
                       (in.totalFiles > 0 && in.distinctFiles > 0 &&
                        in.distinctFiles * 4 >= in.totalFiles && in.totalFiles > 10);
        bool localFits = in.localCapable && !tooWide;
        if (localFits) {
            r.executor = Executor::LOCAL;
            r.reason = "точечная правка в пределах профиля локальной модели";
        } else if (in.remoteAvailable) {
            r.executor = Executor::SPLIT;
            r.reason = tooWide ? "правка широкая -> remote-план + local-исполнение"
                               : "local не способен -> remote-план + local-исполнение";
        } else {
            r.executor = Executor::REFUSE_LOCAL_INCAPABLE;
            r.reason = "правка шире возможностей local, remote недоступен — скажу честно";
        }
        return r;
    }

    // (6) Всё прочее — локальная, при отсутствии способности честный отказ.
    if (in.localCapable) { r.executor = Executor::LOCAL; r.reason = "по умолчанию локальная модель"; }
    else if (in.remoteAvailable) { r.executor = Executor::REMOTE; r.reason = "local не способен -> remote"; }
    else { r.executor = Executor::REFUSE_LOCAL_INCAPABLE; r.reason = "local не способен, remote нет"; }
    return r;
}

} // namespace cppagent
