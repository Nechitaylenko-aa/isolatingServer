/**
 * @file CModelCascade.cpp
 * @brief Реализация детерминированного каскада моделей (ADR-10).
 */

#include "core/CModelCascade.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fcpp
{

SCascadeResult CModelCascade::run(const std::vector<EModelLevel>& levels,
                                  const AttemptFn& attempt,
                                  std::uint32_t max_attempts)
{
    SCascadeResult result;
    result.m_level = levels.empty() ? EModelLevel::Lite : levels.front();
    result.m_glitch = EGlitchKind::Provider;
    result.m_message = "no model levels available";

    std::string carried_context;

    for (const EModelLevel level : levels)
    {
        result.m_level = level;

        // На каждом уровне: 1 попытка + 1 авто-ретрай (с контекстом ошибки).
        for (std::uint32_t attempt_index = 0; attempt_index < 2; ++attempt_index)
        {
            if (result.m_attempts >= max_attempts)
            {
                // Жёсткий бюджет исчерпан — hard-stop.
                result.m_ok = false;
                result.m_glitch = EGlitchKind::Loop;
                result.m_message = "model cascade budget exhausted";
                return result;
            }

            const SCascadeAttempt single = attempt(level, carried_context);
            ++result.m_attempts;

            if (single.m_valid)
            {
                result.m_ok = true;
                result.m_glitch = EGlitchKind::None;
                result.m_text = single.m_result_text;
                result.m_message = "ok";
                return result;
            }

            // Попытка невалидна — фиксируем причину для ретрая/эскалации.
            result.m_glitch = single.m_glitch;
            result.m_message = single.m_error_context.empty()
                                   ? "invalid model response"
                                   : single.m_error_context;
            carried_context = single.m_error_context;

            if (single.m_glitch == EGlitchKind::Provider)
            {
                // Провайдер недоступен — ретрай бессмыслен, сразу на следующий уровень.
                break;
            }
        }
    }

    // Все уровни исчерпаны за 1 ретрай каждый — hard-stop.
    return result;
}

} // namespace fcpp
