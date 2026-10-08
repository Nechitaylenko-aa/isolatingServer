/**
 * @file CPatchValidator.h
 * @brief Валидация и планирование применения патча (§46, §77).
 *
 * Проверяет патч против текущего состояния файлов проекта и вычисляет итоговое
 * содержимое. Никаких изменений на диске не производит. Если хотя бы один hunk
 * не применим — весь патч отклоняется (никакого частичного применения).
 */

#ifndef FCPP_AGENT_CPATCHVALIDATOR_H
#define FCPP_AGENT_CPATCHVALIDATOR_H

#include <filesystem>

#include "fcpp-agent/SPatch.h"

namespace fcpp
{

/**
 * @brief Валидатор патча.
 */
class CPatchValidator
{
public:
    /**
     * @brief Конструктор.
     * @param project_root Канонический корень проекта (для разрешения путей).
     */
    explicit CPatchValidator(std::filesystem::path project_root);

    /**
     * @brief Деструктор.
     */
    ~CPatchValidator() = default;

    /**
     * @brief Проверить и запланировать применение патча.
     * @param patch Разобранный патч.
     * @param out_validation Выходной результат.
     * @return true если патч валиден; false иначе.
     */
    bool validate(const SPatchSet& patch, SPatchValidation& out_validation) const;

private:
    /**
     * @brief Применить hunk'и к тексту файла.
     * @param content Исходное содержимое.
     * @param file_patch Патч файла.
     * @param out_updated Выходное новое содержимое.
     * @param out_message Сообщение об ошибке.
     * @return true при успехе.
     */
    static bool apply_hunks(const std::string& content,
                            const SFilePatch& file_patch,
                            std::string& out_updated,
                            std::string& out_message);

    std::filesystem::path m_project_root; ///< Корень проекта.
};

} // namespace fcpp

#endif // FCPP_AGENT_CPATCHVALIDATOR_H
