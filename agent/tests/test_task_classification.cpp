#include <catch2/catch_test_macros.hpp>
#include "task_classification.h"

using namespace cppagent;

// === Слой 3: computeRelevance(Detail, CursorContext, goalText, usr) ===

TEST_CASE("computeRelevance: NO_CONTEXT unrelated for method-bound details",
          "[classify][unit]") {
    for (Detail d : {Detail::SIMPLE_READ, Detail::LOCAL_FIX, Detail::CALL_CHAIN})
        CHECK(computeRelevance(d, CursorContext::NO_CONTEXT) == Relevance::UNRELATED);
}

TEST_CASE("computeRelevance: GENERAL_QUESTION is cursor-irrelevant (priority over NO_CONTEXT)",
          "[classify][unit]") {
    // arch.md: "если Detail::GENERAL_QUESTION — курсор неважен (ENTRY_POINT_NEARBY
    // независимо от контекста)" — значит этот приоритет выше NO_CONTEXT -> UNRELATED.
    CHECK(computeRelevance(Detail::GENERAL_QUESTION, CursorContext::NO_CONTEXT)
          == Relevance::ENTRY_POINT_NEARBY);
}

TEST_CASE("computeRelevance: method context is EXACT unless goal names another method",
          "[classify][unit]") {
    const std::string addUsr = "c:@S@Calculator@F@add#I#I#";
    // Без текста цели — чисто позиционное суждение: место метод -> EXACT.
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL)
          == Relevance::ENTRY_POINT_EXACT);
    // Цель называет ТОТ ЖЕ метод -> EXACT.
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL,
                           "Что делает метод add(int, int)?", addUsr)
          == Relevance::ENTRY_POINT_EXACT);
    // Цель называет ДРУГОЙ метод -> UNRELATED (детерминированный сигнал слоя 3).
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL,
                           "Что делает метод multiply(int, int)?", addUsr)
          == Relevance::UNRELATED);
    // Упомянуты оба — текущий присутствует, считаем релевантным.
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL,
                           "сравни add(int,int) и multiply(int,int)", addUsr)
          == Relevance::ENTRY_POINT_EXACT);
}

TEST_CASE("computeRelevance: GENERAL_QUESTION is never forced UNRELATED by method name",
          "[classify][unit]") {
    const std::string addUsr = "c:@S@Calculator@F@add#I#I#";
    // Общий вопрос не привязан к конкретному методу -> NEARBY даже если назван другой.
    CHECK(computeRelevance(Detail::GENERAL_QUESTION, CursorContext::CONCRETE_METHOD_IMPL,
                           "расскажи про multiply(int, int)", addUsr)
          == Relevance::ENTRY_POINT_NEARBY);
}

TEST_CASE("computeRelevance: non-method contexts are NEARBY", "[classify][unit]") {
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::INTERFACE_CLASS_DECL)
          == Relevance::ENTRY_POINT_NEARBY);
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::CC_FOREST)
          == Relevance::ENTRY_POINT_NEARBY);
    CHECK(computeRelevance(Detail::SIMPLE_READ, CursorContext::PLAIN_HEADER)
          == Relevance::ENTRY_POINT_NEARBY);
}

TEST_CASE("computeRelevance: bare words without '(' are not method mentions",
          "[classify][unit]") {
    const std::string addUsr = "c:@S@Calculator@F@add#I#I#";
    // "int"/"long" без скобок — не вызовы метода, ложного UNRELATED быть не должно.
    CHECK(computeRelevance(Detail::LOCAL_FIX, CursorContext::CONCRETE_METHOD_IMPL,
                           "Замени int на long", addUsr)
          == Relevance::ENTRY_POINT_EXACT);
}

// === Слой 4: selectAction с модификатором Relevance ===

TEST_CASE("selectAction: UNRELATED/AMBIGUOUS force NEEDS_INPUT_RELEVANCE",
          "[classify][unit]") {
    CHECK(selectAction("UNDERSTAND", Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::UNRELATED) == Action::NEEDS_INPUT_RELEVANCE);
    CHECK(selectAction("SANDBOX_FIX", Detail::LOCAL_FIX, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::UNRELATED) == Action::NEEDS_INPUT_RELEVANCE);
    CHECK(selectAction("UNDERSTAND", Detail::GENERAL_QUESTION, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::AMBIGUOUS) == Action::NEEDS_INPUT_RELEVANCE);
}

TEST_CASE("selectAction: decision table with non-blocking relevance", "[classify][unit]") {
    CHECK(selectAction("UNDERSTAND", Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_LOCATE_ONLY);
    CHECK(selectAction("UNDERSTAND", Detail::SIMPLE_READ, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::NEEDS_INPUT_RELEVANCE);
    CHECK(selectAction("UNDERSTAND", Detail::FIND_IMPLEMENTATIONS,
                       CursorContext::PURE_VIRTUAL_METHOD, Relevance::ENTRY_POINT_EXACT)
          == Action::QUERY_FIND_IMPLEMENTORS);
    CHECK(selectAction("UNDERSTAND", Detail::FIND_IMPLEMENTATIONS,
                       CursorContext::VIRTUAL_METHOD_WITH_BODY, Relevance::ENTRY_POINT_EXACT)
          == Action::QUERY_VIRTUAL_OVERRIDES);
    CHECK(selectAction("UNDERSTAND", Detail::GENERAL_QUESTION,
                       CursorContext::CONCRETE_METHOD_IMPL, Relevance::ENTRY_POINT_NEARBY)
          == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::CALL_CHAIN, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_CALL_CHAIN);
    // Категория C (пункт 3): теперь read-only delegate основной модели, а
    // не молчаливый отказ — модель честно пробует и может сообщить о неудаче.
    CHECK(selectAction("UNDERSTAND", Detail::SECURITY_AUDIT, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::EXCEEDS_LOCAL_AI, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::MODERNIZE, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("SANDBOX_FIX", Detail::LOCAL_FIX, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_LOCATE_ONLY);
    CHECK(selectAction("QUARRY_DESIGN", Detail::NEW_INTERFACE, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
}

TEST_CASE("selectAction: default relevance arg keeps backward-compatible behaviour",
          "[classify][unit]") {
    // Вызов без relevance (старый код) не должен внезапно уходить в needs_input.
    CHECK(selectAction("UNDERSTAND", Detail::SIMPLE_READ, CursorContext::CONCRETE_METHOD_IMPL)
          == Action::QUERY_LOCATE_ONLY);
}

TEST_CASE("selectAction: call-graph details map to call actions", "[classify][unit]") {
    CHECK(selectAction("UNDERSTAND", Detail::CALL_CHAIN, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_CALL_CHAIN);
    CHECK(selectAction("UNDERSTAND", Detail::CALL_GRAPH_SUBTREE, CursorContext::NON_VIRTUAL_METHOD,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_CALL_SUBTREE);
    // на классе (не метод) — спросить, а не отвечать наугад
    CHECK(selectAction("UNDERSTAND", Detail::CALL_CHAIN, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::NEEDS_INPUT_RELEVANCE);
}

TEST_CASE("selectAction: dependency/quality details map to their actions", "[classify][unit]") {
    CHECK(selectAction("UNDERSTAND", Detail::DEPENDENCY_MAP, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::QUERY_DEPENDENCY_MAP);
    CHECK(selectAction("UNDERSTAND", Detail::TODO_SCAN, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::QUERY_TODO_SCAN);
    CHECK(selectAction("UNDERSTAND", Detail::DOC_CHECK, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::QUERY_DOC_CHECK);
    CHECK(selectAction("UNDERSTAND", Detail::CODE_SMELL, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_CODE_SMELL);
    // CODE_SMELL на классе (не метод) — спросить
    CHECK(selectAction("UNDERSTAND", Detail::CODE_SMELL, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::NEEDS_INPUT_RELEVANCE);
    // DOC_SUMMARY — обзор, уходит read-only в main-модель
    CHECK(selectAction("UNDERSTAND", Detail::DOC_SUMMARY, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
}

TEST_CASE("selectAction: type-usage/complexity/duplicates mapping", "[classify][unit]") {
    CHECK(selectAction("UNDERSTAND", Detail::TYPE_USAGE, CursorContext::INTERFACE_CLASS_DECL,
                       Relevance::ENTRY_POINT_NEARBY) == Action::QUERY_TYPE_USAGE);
    CHECK(selectAction("UNDERSTAND", Detail::COMPLEXITY_HOTSPOT, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_COMPLEXITY);
    CHECK(selectAction("UNDERSTAND", Detail::DUPLICATE_FIND, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::QUERY_DUPLICATES);
    // смысловой анализ -> read-only delegate
    CHECK(selectAction("UNDERSTAND", Detail::DATA_FLOW, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::LIFECYCLE_TRACE, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::COMPARE_IMPLS, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
    CHECK(selectAction("UNDERSTAND", Detail::DOC_GEN, CursorContext::CONCRETE_METHOD_IMPL,
                       Relevance::ENTRY_POINT_EXACT) == Action::DELEGATE_TO_MAIN);
}

TEST_CASE("relevanceToString covers all values", "[classify][unit]") {
    CHECK(relevanceToString(Relevance::ENTRY_POINT_EXACT) == "ENTRY_POINT_EXACT");
    CHECK(relevanceToString(Relevance::ENTRY_POINT_NEARBY) == "ENTRY_POINT_NEARBY");
    CHECK(relevanceToString(Relevance::UNRELATED) == "UNRELATED");
    CHECK(relevanceToString(Relevance::AMBIGUOUS) == "AMBIGUOUS");
}
