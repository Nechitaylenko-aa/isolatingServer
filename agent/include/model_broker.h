#pragma once
#include <string>
#include "task_classification.h"

namespace cppagent {

enum class Primitive { UNDERSTAND, SANDBOX_FIX, QUARRY_DESIGN, TEST_GEN, EXPERIMENT, INFRA };

// Строковые имена для JSON-контракта / логов
std::string primitiveToString(Primitive p);
Primitive primitiveFromString(const std::string& s, bool& ok);

struct PrimitiveGuess {
    Primitive primitive;
    std::string primitiveStr; // "UNDERSTAND" | ... — каноническое имя для JSON
    std::string rationale;    // кратко по-русски, от модели
    bool modelUnavailable = false; // true если модель недоступна / вернула мусор
    // Тип B (needs_input): модель сама оценивает уверенность в выборе примитива.
    // confidence=1.0 у Stub (детерминированная эвристика не умеет сомневаться).
    // У Light — то что вернула модель в JSON, по умолчанию 1.0 если не прислала.
    double confidence = 1.0;
    std::string alternativePrimitiveStr; // второй по вероятности примитив, если confidence низкая (может быть пустым)
    // Отдельное от confidence измерение: confidence — уверенность в ВЫБОРЕ примитива,
    // goalIsSpecific — есть ли в goal_text конкретный объект правки и критерий "готово".
    // Модель может быть на 100% уверена, что "сделай лучше" — это SANDBOX_FIX (правка),
    // и одновременно там нет ЧТО улучшать — это и есть дыра: неполный запрос исполняется как полный.
    // true по умолчанию — чтобы Stub (не умеет оценивать) не блокировал ничего сам по себе.
    bool goalIsSpecific = true;

    // Слой 1: детализация задачи — что конкретно хочет юзер.
    // Классифицируется МОДЕЛЬЮ в том же вызове, что и primitive (отдельного похода нет).
    Detail detail = Detail::GENERAL_QUESTION;
    std::string detailStr = "GENERAL_QUESTION";

    // Слой 0: привязана ли задача к курсору вообще. Если NOT_NEEDED — Ground
    // не вызывается совсем (экономия libclang); если NEEDED_MISSING — сразу
    // needs_input, без бессмысленной попытки парсить несуществующий курсор.
    CursorBinding cursorBinding = CursorBinding::NEEDED_PRESENT;
    std::string cursorBindingStr = "NEEDED_PRESENT";
};

// Абстракция роли "модель, которая классифицирует намерение". Реальная
// реализация (роль light/main из ТЗ §6) будет ходить в llama.cpp по HTTP —
// см. ТЗ §6.2. Здесь — заглушка, чтобы обкатать оркестратор (Intake→Ground→
// Clarify/Report) БЕЗ настоящей модели, что и было целью этого шага.
class ModelBroker {
public:
    virtual ~ModelBroker() = default;
    virtual PrimitiveGuess classifyIntent(const std::string& goalText) = 0;
    virtual std::string roleName() const = 0; // попадает в answered_by.role контракта
};

// Обратная совместимость: старый IntentGuess больше не используется,
// оставлен как alias чтобы не ломать внешние тесты, если они его включат.
using IntentGuess = PrimitiveGuess;

// Ключевые слова, а не суждение — намеренно тупая эвристика. Настоящая
// лёгкая модель (Qwen2.5-Coder-7B) заменит это позже реализацией того же
// интерфейса, без изменений в orchestrator.cpp.
class StubModelBroker : public ModelBroker {
public:
    PrimitiveGuess classifyIntent(const std::string& goalText) override;
    std::string roleName() const override { return "stub"; }
};

// Настоящая реализация: HTTP-запрос к серверу llama.cpp (OpenAI-совместимый
// /v1/chat/completions). При недоступности сети/сервера, HTTP-ошибке или
// невалидном JSON в ответе возвращает safety_mode="" — оркестратор трактует
// это как "модель недоступна" и откатывается на детерминированную оценку
// (см. ТЗ §6.3, цепочки отступления).
class LightModelBroker : public ModelBroker {
public:
    // baseUrl без завершающего слэша, например "http://127.0.0.1:8080"
    explicit LightModelBroker(std::string baseUrl, std::string modelName = "", double temperature = 0.1)
        : baseUrl_(std::move(baseUrl)), modelName_(std::move(modelName)), temperature_(temperature) {}

    PrimitiveGuess classifyIntent(const std::string& goalText) override;
    std::string roleName() const override { return "light"; }
    void setTemperature(double t) { temperature_ = t; }
    double temperature() const { return temperature_; }

private:
    std::string baseUrl_;
    std::string modelName_;
    double temperature_;
};

} // namespace cppagent
