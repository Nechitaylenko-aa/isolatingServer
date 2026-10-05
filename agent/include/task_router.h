#pragma once

#include <string>
#include "task_classification.h"

namespace cppagent {

// Кто исполняет задачу. Решение принимается ДО вызова модели (проактивно),
// по классу задачи + масштабу + доступности remote, а не «после того, как
// локальная заглючила». LITE здесь СОЗНАТЕЛЬНО отсутствует: lite — фоновый
// помощник (классификация/сущности/сжатие), в исполнении задач не участвует.
enum class Executor {
    NONE,                     // чистый libclang/кэш: граф, зависимости, качество
    LOCAL,                    // локальная main-модель
    SPLIT,                    // remote планирует, local исполняет узкие правки
    REMOTE,                   // удалённая модель (сильнейший игрок по умолчанию)
    REFUSE_LOCAL_INCAPABLE,   // класс выше профиля local, remote недоступен -> честный отказ
};

std::string executorToString(Executor e);

struct RouteInput {
    std::string primitiveStr;
    Detail detail{Detail::GENERAL_QUESTION};
    int refsFound{0};
    int distinctFiles{0};
    int totalFiles{0};
    bool remoteAvailable{false};
    // Профиль локальной модели: тянет ли она класс этой задачи.
    // FAIL-CLOSED: по умолчанию НЕ способна, пока проба не подтвердит обратное.
    // Spec: «модель считается неспособной, пока не подтверждено обратное».
    bool localCapable{false};
    // Сколько файлов правки локальная main тянет уверенно (из профиля/конфига).
    int localMaxEditFiles{3};
};

struct RouteDecision {
    Executor executor{Executor::NONE};
    std::string reason;
};

// Детерминированная таблица решений (unit-тестируемая, без сети).
RouteDecision routeTask(const RouteInput& in);

} // namespace cppagent
