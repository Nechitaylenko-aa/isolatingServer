/**
 * @file CEscalatingIntentAnalyzer.cpp
 * @brief Реализация анализа Intent с эскалацией (6c).
 */

#include "core/CEscalatingIntentAnalyzer.h"

#include <memory>
#include <string>
#include <vector>

#include "core/CIntentAnalyzer.h"
#include "core/CModelCascade.h"
#include "fcpp-agent/IModelApi.h"
#include "llm/CModelFactory.h"

namespace fcpp
{

namespace
{

/** @brief Жёсткий бюджет попыток каскада. */
constexpr std::uint32_t k_max_attempts = 5;

} // namespace

CEscalatingIntentAnalyzer::CEscalatingIntentAnalyzer(const SConfig& config)
    : m_config(config)
{
}

std::vector<EModelLevel> CEscalatingIntentAnalyzer::available_levels() const
{
    std::vector<EModelLevel> levels;

    std::unique_ptr<IModelApi> lite = CModelFactory::create(m_config, EModelRoleKind::Light);
    if (lite != nullptr && lite->is_available())
    {
        levels.push_back(EModelLevel::Lite);
    }

    std::unique_ptr<IModelApi> main_model = CModelFactory::create(m_config, EModelRoleKind::Main);
    if (main_model != nullptr && main_model->is_available())
    {
        levels.push_back(EModelLevel::Main);
    }

    std::unique_ptr<IModelApi> remote = CModelFactory::create(m_config, EModelRoleKind::Remote);
    if (remote != nullptr && remote->is_available())
    {
        levels.push_back(EModelLevel::Remote);
    }

    return levels;
}

bool CEscalatingIntentAnalyzer::analyze(const std::string& user_text,
                                        const std::string& cursor_hint,
                                        SIntent& out_intent,
                                        std::string& out_error) const
{
    const std::vector<EModelLevel> levels = available_levels();
    if (levels.empty())
    {
        out_error = "no models configured";
        return false;
    }

    const CIntentAnalyzer analyzer;

    const SCascadeResult result = CModelCascade::run(
        levels,
        [&](EModelLevel level, const std::string& extra_context) -> SCascadeAttempt
        {
            SCascadeAttempt attempt;

            EModelRoleKind role = EModelRoleKind::Light;
            if (level == EModelLevel::Main)
            {
                role = EModelRoleKind::Main;
            }
            else if (level == EModelLevel::Remote)
            {
                role = EModelRoleKind::Remote;
            }

            std::unique_ptr<IModelApi> model = CModelFactory::create(m_config, role);
            if (model == nullptr || !model->is_available())
            {
                attempt.m_valid = false;
                attempt.m_glitch = EGlitchKind::Provider;
                attempt.m_error_context = "model unavailable";
                return attempt;
            }

            SIntent intent;
            std::string reason;
            const std::string text_with_ctx =
                user_text + (cursor_hint.empty() ? "" : "\n" + cursor_hint)
                + (extra_context.empty() ? "" : "\n" + extra_context);
            if (analyzer.analyze(*model, text_with_ctx, {}, intent, reason))
            {
                attempt.m_valid = true;
                out_intent = intent;
                return attempt;
            }

            attempt.m_valid = false;
            attempt.m_glitch = EGlitchKind::Syntax;
            attempt.m_error_context = "syntax fail: " + reason + "; fix the XML format";
            return attempt;
        },
        k_max_attempts);

    if (!result.m_ok)
    {
        out_error = std::string(glitch_kind_name(result.m_glitch)) + ": " + result.m_message
                    + " (levels exhausted)";
        return false;
    }

    return true;
}

} // namespace fcpp
