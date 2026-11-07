/**
 * @file CGitSnapshotStore.h
 * @brief Теневой git-снапшот проекта для отката правок (decisions.md №22).
 *
 * Снапшот не копирует дерево проекта — отдельный --git-dir
 * (`<snapshot_root>/<project-id>/snapshots.git`) работает поверх реального
 * каталога проекта как --work-tree. В /tmp попадают только git-объекты
 * изменившихся файлов, не копия всего дерева; .gitignore проекта
 * соблюдается автоматически (git ищет его в work-tree, не в --git-dir).
 */

#ifndef FCPP_AGENT_CGITSNAPSHOTSTORE_H
#define FCPP_AGENT_CGITSNAPSHOTSTORE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/CGitProcess.h"
#include "fcpp-agent/SSnapshot.h"

namespace fcpp
{

class CGitSnapshotStore
{
public:
    /**
     * @param snapshot_root Корень всех теневых репозиториев (CConfig::m_snapshot_root).
     */
    explicit CGitSnapshotStore(std::filesystem::path snapshot_root);

    /**
     * @brief Зафиксировать текущее состояние каталога проекта как снапшот.
     *
     * Инициализирует теневой репозиторий при первом вызове для этого
     * project_id (git init --bare + info/exclude с базовым исключением
     * .git). Коммит делается с --allow-empty — операция детерминирована
     * и не зависит от того, реально ли что-то изменилось с прошлого раза.
     *
     * @param project_root Каталог проекта (work-tree).
     * @param project_id Идентификатор проекта — определяет путь теневого репо.
     * @param message Сообщение коммита (request_id и т.п., для списка истории).
     * @param[out] error Причина сбоя, если вернул false.
     * @return true при успехе.
     */
    bool snapshot(const std::filesystem::path& project_root,
                 const std::string& project_id,
                 const std::string& message,
                 std::string& error) const;

    /**
     * @brief Восстановить каталог проекта к состоянию снапшота.
     *
     * `checkout <ref> -- .` — обновляет только рабочее дерево и индекс
     * теневого репозитория, не трогает HEAD/branch (не создаёт detached
     * HEAD — это не тот checkout). ref — любая ссылка, которую понимает git:
     * полный/короткий SHA или выражение вида HEAD~N (см. resolve_steps_back).
     *
     * @param project_root Каталог проекта (work-tree).
     * @param project_id Идентификатор проекта.
     * @param ref Целевая ссылка.
     * @param[out] error Причина сбоя, если вернул false.
     * @return true при успехе.
     */
    bool restore(const std::filesystem::path& project_root,
                const std::string& project_id,
                const std::string& ref,
                std::string& error) const;

    /**
     * @brief Разрешить «N шагов назад» в конкретный SHA.
     *
     * Возвращает стабильный SHA, а не оставляет это как относительное
     * выражение HEAD~N — HEAD уходит вперёд с каждым новым снапшотом,
     * относительное выражение означало бы разное в разные моменты.
     *
     * @param project_root Каталог проекта.
     * @param project_id Идентификатор проекта.
     * @param steps Сколько снапшотов назад (1 — последний перед текущим).
     * @param[out] error Причина сбоя (в т.ч. «шагов больше, чем снапшотов»).
     * @return SHA либо nullopt при сбое.
     */
    std::optional<std::string> resolve_steps_back(const std::filesystem::path& project_root,
                                                   const std::string& project_id,
                                                   unsigned steps,
                                                   std::string& error) const;

    /**
     * @brief Список последних снапшотов, от новых к старым.
     * @param project_root Каталог проекта.
     * @param project_id Идентификатор проекта.
     * @param limit Максимум записей.
     * @param[out] error Причина сбоя, если вернул false (например, снапшотов ещё нет).
     * @return Список снапшотов; пустой список при отсутствии истории — не ошибка.
     */
    std::vector<SSnapshotEntry> list(const std::filesystem::path& project_root,
                                     const std::string& project_id,
                                     unsigned limit,
                                     std::string& error) const;

private:
    std::filesystem::path m_snapshot_root;
    CGitProcess m_git;

    /** @brief Путь --git-dir теневого репозитория проекта. */
    [[nodiscard]] std::filesystem::path git_dir(const std::string& project_id) const;

    /**
     * @brief Создать теневой репозиторий, если его ещё нет.
     * @return true если репозиторий существует или успешно создан.
     */
    bool ensure_initialized(const std::filesystem::path& project_root,
                            const std::string& project_id,
                            std::string& error) const;
};

} // namespace fcpp

#endif // FCPP_AGENT_CGITSNAPSHOTSTORE_H
