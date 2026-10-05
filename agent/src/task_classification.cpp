#include "task_classification.h"

#include <cctype>
#include <string>
#include <vector>

namespace cppagent {

std::string detailToString(Detail d) {
    switch (d) {
    case Detail::SIMPLE_READ: return "SIMPLE_READ";
    case Detail::FIND_IMPLEMENTATIONS: return "FIND_IMPLEMENTATIONS";
    case Detail::ANALYZE_INTERFACE: return "ANALYZE_INTERFACE";
    case Detail::CALL_CHAIN: return "CALL_CHAIN";
    case Detail::CALL_GRAPH_SUBTREE: return "CALL_GRAPH_SUBTREE";
    case Detail::DEPENDENCY_MAP: return "DEPENDENCY_MAP";
    case Detail::DATA_FLOW: return "DATA_FLOW";
    case Detail::TYPE_USAGE: return "TYPE_USAGE";
    case Detail::LIFECYCLE_TRACE: return "LIFECYCLE_TRACE";
    case Detail::COMPARE_IMPLS: return "COMPARE_IMPLS";
    case Detail::DIFF_INTERFACE_IMPL: return "DIFF_INTERFACE_IMPL";
    case Detail::VIRTUAL_REFACTOR_SIM: return "VIRTUAL_REFACTOR_SIM";
    case Detail::FEASIBILITY_CHECK: return "FEASIBILITY_CHECK";
    case Detail::GENERAL_QUESTION: return "GENERAL_QUESTION";
    case Detail::LOCAL_FIX: return "LOCAL_FIX";
    case Detail::RENAME: return "RENAME";
    case Detail::SIGNATURE_CHANGE: return "SIGNATURE_CHANGE";
    case Detail::ADD_OVERRIDE: return "ADD_OVERRIDE";
    case Detail::EXTRACT_METHOD: return "EXTRACT_METHOD";
    case Detail::INLINE_METHOD: return "INLINE_METHOD";
    case Detail::EXTRACT_CLASS: return "EXTRACT_CLASS";
    case Detail::MOVE_METHOD: return "MOVE_METHOD";
    case Detail::DEAD_CODE_REMOVAL: return "DEAD_CODE_REMOVAL";
    case Detail::REMOVE_UNUSED_INCL: return "REMOVE_UNUSED_INCL";
    case Detail::ADD_METHOD: return "ADD_METHOD";
    case Detail::ADD_CLASS: return "ADD_CLASS";
    case Detail::NEW_INTERFACE: return "NEW_INTERFACE";
    case Detail::API_DESIGN: return "API_DESIGN";
    case Detail::APPLY_PATTERN: return "APPLY_PATTERN";
    case Detail::SPLIT_CLASS: return "SPLIT_CLASS";
    case Detail::DESIGN_HIERARCHY: return "DESIGN_HIERARCHY";
    case Detail::UNIT_TEST: return "UNIT_TEST";
    case Detail::INTEGRATION_TEST: return "INTEGRATION_TEST";
    case Detail::GEN_MOCKS: return "GEN_MOCKS";
    case Detail::COVERAGE_GAP: return "COVERAGE_GAP";
    case Detail::PROTOTYPE: return "PROTOTYPE";
    case Detail::BENCHMARK: return "BENCHMARK";
    case Detail::TRY_ALTERNATIVE: return "TRY_ALTERNATIVE";
    case Detail::BUILD_FIX: return "BUILD_FIX";
    case Detail::BUILD_CONFIG: return "BUILD_CONFIG";
    case Detail::DEPS_UPDATE: return "DEPS_UPDATE";
    case Detail::CI_PIPELINE: return "CI_PIPELINE";
    case Detail::CODE_SMELL: return "CODE_SMELL";
    case Detail::DUPLICATE_FIND: return "DUPLICATE_FIND";
    case Detail::COMPLEXITY_HOTSPOT: return "COMPLEXITY_HOTSPOT";
    case Detail::TODO_SCAN: return "TODO_SCAN";
    case Detail::DOC_GEN: return "DOC_GEN";
    case Detail::DOC_CHECK: return "DOC_CHECK";
    case Detail::DOC_SUMMARY: return "DOC_SUMMARY";
    case Detail::MODERNIZE: return "MODERNIZE";
    case Detail::API_MIGRATION: return "API_MIGRATION";
    case Detail::PORT_PLATFORM: return "PORT_PLATFORM";
    case Detail::SECURITY_AUDIT: return "SECURITY_AUDIT";
    case Detail::INPUT_VALIDATION: return "INPUT_VALIDATION";
    case Detail::THREADING_SAFE: return "THREADING_SAFE";
    case Detail::EXCEEDS_LOCAL_AI: return "EXCEEDS_LOCAL_AI";
    }
    return "UNKNOWN";
}


std::string cursorContextToString(CursorContext c) {
    switch (c) {
    case CursorContext::NO_CONTEXT: return "NO_CONTEXT";
    case CursorContext::PURE_VIRTUAL_METHOD: return "PURE_VIRTUAL_METHOD";
    case CursorContext::VIRTUAL_METHOD_WITH_BODY: return "VIRTUAL_METHOD_WITH_BODY";
    case CursorContext::CONCRETE_METHOD_IMPL: return "CONCRETE_METHOD_IMPL";
    case CursorContext::NON_VIRTUAL_METHOD: return "NON_VIRTUAL_METHOD";
    case CursorContext::PIMPL_IMPL: return "PIMPL_IMPL";
    case CursorContext::INTERFACE_CLASS_DECL: return "INTERFACE_CLASS_DECL";
    case CursorContext::CC_FOREST: return "CC_FOREST";
    case CursorContext::PLAIN_HEADER: return "PLAIN_HEADER";
    case CursorContext::FREE_FUNCTION: return "FREE_FUNCTION";
    }
    return "UNKNOWN";
}

std::string cursorBindingToString(CursorBinding b) {
    switch (b) {
    case CursorBinding::NOT_NEEDED: return "NOT_NEEDED";
    case CursorBinding::NEEDED_PRESENT: return "NEEDED_PRESENT";
    case CursorBinding::NEEDED_MISSING: return "NEEDED_MISSING";
    }
    return "UNKNOWN";
}

std::string actionToString(Action a) {
    switch (a) {
    case Action::QUERY_LOCATE_ONLY: return "QUERY_LOCATE_ONLY";
    case Action::QUERY_FIND_IMPLEMENTORS: return "QUERY_FIND_IMPLEMENTORS";
    case Action::QUERY_VIRTUAL_OVERRIDES: return "QUERY_VIRTUAL_OVERRIDES";
    case Action::QUERY_CALL_CHAIN: return "QUERY_CALL_CHAIN";
    case Action::QUERY_CALL_SUBTREE: return "QUERY_CALL_SUBTREE";
    case Action::QUERY_DEPENDENCY_MAP: return "QUERY_DEPENDENCY_MAP";
    case Action::QUERY_TODO_SCAN: return "QUERY_TODO_SCAN";
    case Action::QUERY_DOC_CHECK: return "QUERY_DOC_CHECK";
    case Action::QUERY_CODE_SMELL: return "QUERY_CODE_SMELL";
    case Action::QUERY_TYPE_USAGE: return "QUERY_TYPE_USAGE";
    case Action::QUERY_COMPLEXITY: return "QUERY_COMPLEXITY";
    case Action::QUERY_DUPLICATES: return "QUERY_DUPLICATES";
    case Action::NOT_IMPLEMENTED_YET: return "NOT_IMPLEMENTED_YET";
    case Action::DELEGATE_TO_MAIN: return "DELEGATE_TO_MAIN";
    case Action::REFUSE_TOO_BROAD: return "REFUSE_TOO_BROAD";
    case Action::NEEDS_INPUT_RELEVANCE: return "NEEDS_INPUT_RELEVANCE";
    case Action::NEEDS_INPUT_CURSOR: return "NEEDS_INPUT_CURSOR";
    }
    return "UNKNOWN";
}

std::string relevanceToString(Relevance r) {
    switch (r) {
    case Relevance::ENTRY_POINT_EXACT: return "ENTRY_POINT_EXACT";
    case Relevance::ENTRY_POINT_NEARBY: return "ENTRY_POINT_NEARBY";
    case Relevance::UNRELATED: return "UNRELATED";
    case Relevance::AMBIGUOUS: return "AMBIGUOUS";
    }
    return "UNKNOWN";
}


Detail detailFromString(const std::string& s, bool& ok) {
    ok = true;
    if (s == "SIMPLE_READ") return Detail::SIMPLE_READ;
    if (s == "FIND_IMPLEMENTATIONS") return Detail::FIND_IMPLEMENTATIONS;
    if (s == "ANALYZE_INTERFACE") return Detail::ANALYZE_INTERFACE;
    if (s == "CALL_CHAIN") return Detail::CALL_CHAIN;
    if (s == "CALL_GRAPH_SUBTREE") return Detail::CALL_GRAPH_SUBTREE;
    if (s == "DEPENDENCY_MAP") return Detail::DEPENDENCY_MAP;
    if (s == "DATA_FLOW") return Detail::DATA_FLOW;
    if (s == "TYPE_USAGE") return Detail::TYPE_USAGE;
    if (s == "LIFECYCLE_TRACE") return Detail::LIFECYCLE_TRACE;
    if (s == "COMPARE_IMPLS") return Detail::COMPARE_IMPLS;
    if (s == "DIFF_INTERFACE_IMPL") return Detail::DIFF_INTERFACE_IMPL;
    if (s == "VIRTUAL_REFACTOR_SIM") return Detail::VIRTUAL_REFACTOR_SIM;
    if (s == "FEASIBILITY_CHECK") return Detail::FEASIBILITY_CHECK;
    if (s == "GENERAL_QUESTION") return Detail::GENERAL_QUESTION;
    if (s == "LOCAL_FIX") return Detail::LOCAL_FIX;
    if (s == "RENAME") return Detail::RENAME;
    if (s == "SIGNATURE_CHANGE") return Detail::SIGNATURE_CHANGE;
    if (s == "ADD_OVERRIDE") return Detail::ADD_OVERRIDE;
    if (s == "EXTRACT_METHOD") return Detail::EXTRACT_METHOD;
    if (s == "INLINE_METHOD") return Detail::INLINE_METHOD;
    if (s == "EXTRACT_CLASS") return Detail::EXTRACT_CLASS;
    if (s == "MOVE_METHOD") return Detail::MOVE_METHOD;
    if (s == "DEAD_CODE_REMOVAL") return Detail::DEAD_CODE_REMOVAL;
    if (s == "REMOVE_UNUSED_INCL") return Detail::REMOVE_UNUSED_INCL;
    if (s == "ADD_METHOD") return Detail::ADD_METHOD;
    if (s == "ADD_CLASS") return Detail::ADD_CLASS;
    if (s == "NEW_INTERFACE") return Detail::NEW_INTERFACE;
    if (s == "API_DESIGN") return Detail::API_DESIGN;
    if (s == "APPLY_PATTERN") return Detail::APPLY_PATTERN;
    if (s == "SPLIT_CLASS") return Detail::SPLIT_CLASS;
    if (s == "DESIGN_HIERARCHY") return Detail::DESIGN_HIERARCHY;
    if (s == "UNIT_TEST") return Detail::UNIT_TEST;
    if (s == "INTEGRATION_TEST") return Detail::INTEGRATION_TEST;
    if (s == "GEN_MOCKS") return Detail::GEN_MOCKS;
    if (s == "COVERAGE_GAP") return Detail::COVERAGE_GAP;
    if (s == "PROTOTYPE") return Detail::PROTOTYPE;
    if (s == "BENCHMARK") return Detail::BENCHMARK;
    if (s == "TRY_ALTERNATIVE") return Detail::TRY_ALTERNATIVE;
    if (s == "BUILD_FIX") return Detail::BUILD_FIX;
    if (s == "BUILD_CONFIG") return Detail::BUILD_CONFIG;
    if (s == "DEPS_UPDATE") return Detail::DEPS_UPDATE;
    if (s == "CI_PIPELINE") return Detail::CI_PIPELINE;
    if (s == "CODE_SMELL") return Detail::CODE_SMELL;
    if (s == "DUPLICATE_FIND") return Detail::DUPLICATE_FIND;
    if (s == "COMPLEXITY_HOTSPOT") return Detail::COMPLEXITY_HOTSPOT;
    if (s == "TODO_SCAN") return Detail::TODO_SCAN;
    if (s == "DOC_GEN") return Detail::DOC_GEN;
    if (s == "DOC_CHECK") return Detail::DOC_CHECK;
    if (s == "DOC_SUMMARY") return Detail::DOC_SUMMARY;
    if (s == "MODERNIZE") return Detail::MODERNIZE;
    if (s == "API_MIGRATION") return Detail::API_MIGRATION;
    if (s == "PORT_PLATFORM") return Detail::PORT_PLATFORM;
    if (s == "SECURITY_AUDIT") return Detail::SECURITY_AUDIT;
    if (s == "INPUT_VALIDATION") return Detail::INPUT_VALIDATION;
    if (s == "THREADING_SAFE") return Detail::THREADING_SAFE;
    if (s == "EXCEEDS_LOCAL_AI") return Detail::EXCEEDS_LOCAL_AI;
    ok = false;
    return Detail::GENERAL_QUESTION;
}


namespace {

bool isMethodContext(CursorContext c) {
    switch (c) {
    case CursorContext::PURE_VIRTUAL_METHOD:
    case CursorContext::VIRTUAL_METHOD_WITH_BODY:
    case CursorContext::CONCRETE_METHOD_IMPL:
    case CursorContext::NON_VIRTUAL_METHOD:
    case CursorContext::PIMPL_IMPL:
    case CursorContext::FREE_FUNCTION:
        return true;
    default:
        return false;
    }
}

// Detail-ы, которые по смыслу привязаны к КОНКРЕТНОМУ методу под курсором.
// Для них "место = другой метод" уже сигнал несогласованности (слой 3).
bool detailRequiresMethod(Detail d) {
    switch (d) {
    case Detail::SIMPLE_READ:
    case Detail::FIND_IMPLEMENTATIONS:
    case Detail::CALL_CHAIN:
    case Detail::CALL_GRAPH_SUBTREE:
    case Detail::DATA_FLOW:
    case Detail::LIFECYCLE_TRACE:
    case Detail::COMPARE_IMPLS:
    case Detail::VIRTUAL_REFACTOR_SIM:
    case Detail::FEASIBILITY_CHECK:
    case Detail::LOCAL_FIX:
    case Detail::RENAME:
    case Detail::SIGNATURE_CHANGE:
    case Detail::ADD_OVERRIDE:
    case Detail::EXTRACT_METHOD:
    case Detail::INLINE_METHOD:
    case Detail::MOVE_METHOD:
    case Detail::DEAD_CODE_REMOVAL:
    case Detail::BENCHMARK:
    case Detail::THREADING_SAFE:
        return true;
    default:
        return false;
    }
}

// "c:@S@Calculator@F@add#I#I#" -> "add". Пустая строка, если USR без @F@.
std::string methodNameFromUsr(const std::string& usr) {
    const std::string kF = "@F@";
    auto p = usr.find(kF);
    if (p == std::string::npos) return "";
    p += kF.size();
    auto h = usr.find('#', p);
    if (h == std::string::npos) h = usr.size();
    return usr.substr(p, h - p);
}

// Имена, использованные в goal_text как вызов/reference: identifier, сразу за
// которым (после пробелов) идёт '('. Сознательно узко: слова без скобок
// (int/long/имена классов) НЕ считаются упоминанием метода, поэтому ложных
// UNRELATED почти нет. Это приближение, а не разбор естественного языка.
std::vector<std::string> callLikeMethodNames(const std::string& goalText) {
    static const char* kKeywords[] = {
        "if", "for", "while", "switch", "return", "sizeof", "catch", "throw",
        "new", "delete", "decltype", "static_cast", "dynamic_cast", "const_cast",
        "reinterpret_cast", "typeid", "alignof", "noexcept", "operator"
    };
    std::vector<std::string> out;
    size_t i = 0, n = goalText.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(goalText[i]);
        if (std::isalpha(c) || c == '_') {
            size_t s = i;
            while (i < n) {
                unsigned char d = static_cast<unsigned char>(goalText[i]);
                if (std::isalnum(d) || d == '_') ++i;
                else break;
            }
            std::string name = goalText.substr(s, i - s);
            size_t j = i;
            while (j < n && goalText[j] == ' ') ++j;
            if (j < n && goalText[j] == '(') {
                bool kw = false;
                for (const char* k : kKeywords) if (name == k) { kw = true; break; }
                if (!kw) out.push_back(name);
            }
        } else {
            ++i;
        }
    }
    return out;
}

} // namespace

Relevance computeRelevance(Detail detail, CursorContext ctx,
                           const std::string& goalText,
                           const std::string& enclosingMethodUsr) {
    // arch.md: GENERAL_QUESTION — курсор неважен НЕЗАВИСИМО от контекста,
    // поэтому этот приоритет ВЫШЕ проверки NO_CONTEXT (общий вопрос "как
    // работает подсистема X" не должен требовать курсора).
    if (detail == Detail::GENERAL_QUESTION) return Relevance::ENTRY_POINT_NEARBY;
    if (ctx == CursorContext::NO_CONTEXT) return Relevance::UNRELATED;
    if (isMethodContext(ctx)) {
        // Детерминированный сигнал "не то место": задача явно вызывает/называет
        // ДРУГОЙ метод, чем тот, под которым стоит курсор. Если имя текущего
        // метода в тексте тоже присутствует — место считаем релевантным.
        if (!enclosingMethodUsr.empty() && detailRequiresMethod(detail)) {
            const std::string current = methodNameFromUsr(enclosingMethodUsr);
            std::vector<std::string> named = callLikeMethodNames(goalText);
            if (!current.empty() && !named.empty()) {
                bool mentionsCurrent = false;
                for (const auto& nm : named) if (nm == current) { mentionsCurrent = true; break; }
                if (!mentionsCurrent) return Relevance::UNRELATED;
            }
        }
        return Relevance::ENTRY_POINT_EXACT;
    }
    if (ctx == CursorContext::INTERFACE_CLASS_DECL ||
        ctx == CursorContext::CC_FOREST ||
        ctx == CursorContext::PLAIN_HEADER)
        return Relevance::ENTRY_POINT_NEARBY;
    return Relevance::AMBIGUOUS;
}

Action selectAction(const std::string& primitiveStr, Detail detail, CursorContext ctx,
                    Relevance relevance) {
    // Слой 3 как модификатор слоя 4 (arch.md): если место не относится к задаче —
    // спрашиваем юзера, что бы ни дала таблица (primitive, detail) ниже.
    if (relevance == Relevance::UNRELATED || relevance == Relevance::AMBIGUOUS)
        return Action::NEEDS_INPUT_RELEVANCE;
    if (primitiveStr == "UNDERSTAND" || primitiveStr == "EXPERIMENT") {
        switch (detail) {
        case Detail::SIMPLE_READ:
            return isMethodContext(ctx) ? Action::QUERY_LOCATE_ONLY : Action::NEEDS_INPUT_RELEVANCE;
        case Detail::FIND_IMPLEMENTATIONS:
            if (ctx == CursorContext::PURE_VIRTUAL_METHOD) return Action::QUERY_FIND_IMPLEMENTORS;
            if (ctx == CursorContext::VIRTUAL_METHOD_WITH_BODY ||
                ctx == CursorContext::CONCRETE_METHOD_IMPL) return Action::QUERY_VIRTUAL_OVERRIDES;
            return Action::NEEDS_INPUT_RELEVANCE;
        case Detail::ANALYZE_INTERFACE:
        case Detail::DIFF_INTERFACE_IMPL:
            return Action::QUERY_FIND_IMPLEMENTORS;
        case Detail::VIRTUAL_REFACTOR_SIM:
        case Detail::FEASIBILITY_CHECK:
            return Action::DELEGATE_TO_MAIN;
        case Detail::GENERAL_QUESTION:
            return Action::DELEGATE_TO_MAIN;
        case Detail::CALL_CHAIN:
            return isMethodContext(ctx) ? Action::QUERY_CALL_CHAIN : Action::NEEDS_INPUT_RELEVANCE;
        case Detail::CALL_GRAPH_SUBTREE:
            return isMethodContext(ctx) ? Action::QUERY_CALL_SUBTREE : Action::NEEDS_INPUT_RELEVANCE;
        case Detail::DEPENDENCY_MAP:
            // Зависимости — всегда есть: файл (курсор) или headless по цели.
            return Action::QUERY_DEPENDENCY_MAP;
        case Detail::TODO_SCAN:
            return Action::QUERY_TODO_SCAN;
        case Detail::DOC_CHECK:
            return Action::QUERY_DOC_CHECK;
        case Detail::CODE_SMELL:
            return isMethodContext(ctx) ? Action::QUERY_CODE_SMELL : Action::NEEDS_INPUT_RELEVANCE;
        case Detail::TYPE_USAGE:
            // Тип/класс -> refs по USR класса; на методе — по USR метода.
            return (isMethodContext(ctx) ||
                    ctx == CursorContext::INTERFACE_CLASS_DECL) ? Action::QUERY_TYPE_USAGE
                                                                : Action::NEEDS_INPUT_RELEVANCE;
        case Detail::COMPLEXITY_HOTSPOT:
            return Action::QUERY_COMPLEXITY;
        case Detail::DUPLICATE_FIND:
            return Action::QUERY_DUPLICATES;
        // Смысловой анализ (трассировка потока данных/жизненного цикла, сравнение
        // реализаций, генерация доков) — read-only reasoning основной модели.
        case Detail::DATA_FLOW:
        case Detail::LIFECYCLE_TRACE:
        case Detail::COMPARE_IMPLS:
        case Detail::DOC_GEN:
        case Detail::DOC_SUMMARY:
            return Action::DELEGATE_TO_MAIN;
        // Категория C: задачи, требующие широкого/глубокого анализа. Раньше все
        // они отказывались (REFUSE_TOO_BROAD). Теперь отдаём read-only основной
        // модели (DELEGATE_TO_MAIN) — она честно пробует, а при неудаче явно
        // сообщает, что не справилась (modelfail), вместо молчаливого отказа.
        case Detail::SECURITY_AUDIT:
        case Detail::INPUT_VALIDATION:
        case Detail::THREADING_SAFE:
        case Detail::MODERNIZE:
        case Detail::API_MIGRATION:
        case Detail::PORT_PLATFORM:
        case Detail::EXCEEDS_LOCAL_AI:
            return Action::DELEGATE_TO_MAIN;
        default:
            return Action::NOT_IMPLEMENTED_YET;
        }
    }
    if (primitiveStr == "SANDBOX_FIX") {
        if (detail == Detail::LOCAL_FIX) return Action::QUERY_LOCATE_ONLY;
        if (detail == Detail::ADD_OVERRIDE) return Action::QUERY_FIND_IMPLEMENTORS;
        return Action::NOT_IMPLEMENTED_YET;
    }
    if (primitiveStr == "QUARRY_DESIGN") return Action::DELEGATE_TO_MAIN;
    if (primitiveStr == "TEST_GEN") return Action::NOT_IMPLEMENTED_YET;
    if (primitiveStr == "INFRA") return Action::QUERY_LOCATE_ONLY;
    return Action::NOT_IMPLEMENTED_YET;
}

} // namespace cppagent
