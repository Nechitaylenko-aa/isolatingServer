//
// Created by artem on 22.08.26.
//

#ifndef NYM_PROJECT_IWATERTREATMENTTEMPLATE_H
#define NYM_PROJECT_IWATERTREATMENTTEMPLATE_H

#include "IInstallationTemplate.h"

namespace NCore
{
    struct SBackwashCandidate
    {
        std::vector<NComponent*> filters;
        NComponent* tank_before{nullptr};
        NComponent* tank_after{nullptr};
        NComponent* recirculation_pump{nullptr};
        NComponent* filler_pump{nullptr};
        NComponent* filter_pump{nullptr};
        NComponent* flow_reverser{nullptr};
        NComponent* air_blower{nullptr};
        std::vector<NComponent*> valves_air;
        std::vector<NComponent*> valves_water;

        [[nodiscard]] bool is_ready() const {
            return  !filters.empty() && tank_before && tank_after &&
                    recirculation_pump && filler_pump &&
                    filter_pump && flow_reverser;
        }
    };

    struct SStageOverride
    {
        NComponent*  owner{nullptr};
        SCommandRole role{};
        bool         activate{true};
    };

    /**
     * @brief Данные для чередования двух источников (насос/воздуходувка) с попарно
     * управляемыми группами клапанов. НЕ является общим механизмом — специфично для
     * air-cut backwash-рецепта, живёт только внутри SBackwashRecipe. Первый (primary)
     * стартует первым при входе в BACKWASH (offset=0), secondary — через period_sec (offset=period_sec).
     * @note Клапаны реагируют на СТАТУС актуатора-источника (TRIG_EVENT), а не на время —
     * это даёт устойчивость к любому дрейфу таймера между насосом/воздуходувкой и клапанами.
     */
    struct SAlternatingAgent
    {
        NComponent*   primary{nullptr};          // НС39 — первый по условию задачи
        SCommandRole  primary_role{};
        NComponent*   secondary{nullptr};        // ВС98
        SCommandRole  secondary_role{};

        uint32_t      period_sec{0};             // длительность одной фазы (60с в текущем случае)

        std::vector<std::pair<NComponent*, SCommandRole>> primary_valves;   // КО256/262/268
        std::vector<std::pair<NComponent*, SCommandRole>> secondary_valves; // КО259/265/271
    };


    struct SBackwashRecipe
    {

        std::vector<SCondition> stages_conditions;
        std::vector<SStageOverride> prep_overrides1{};
        std::vector<SStageOverride> prep_overrides2{};
        std::vector<SStageOverride> backwash_overrides{};
        std::vector<SStageOverride> restore_overrides{};
        std::optional<SAlternatingAgent> backwash_alternation{};
    };

    class IWaterTreatmentTemplate : public IInstallationTemplate
    {
    public:
        IWaterTreatmentTemplate();
        ~IWaterTreatmentTemplate() override;

        NComponent* find_first_boundary_upstream(CCap *from_input_cap, std::vector<CCell*> &visited) override;
        NComponent* find_first_boundary_downstream(CCap *from_output_cap, std::vector<CCell*> &visited) override;

        std::vector<SBackwashCandidate> wire_filter_backwash_pattern(
                CSubProject *proj, const SCollectedAutomationData &data);

        uint16_t emit_backwash_sequence(
                CAutomationModel *model, uint16_t last_id,
                const SCollectedAutomationData &data,
                const SBackwashRecipe &r,
                const SBackwashCandidate &c, uint32_t trigger_period_sec);

        void emit_alternation_atoms(
                CAutomationModel *model, uint16_t stage_set_id,
                const SCollectedAutomationData &data,
                const SAlternatingAgent &agent);

        void emit_stage_entry_snapshot(
                CAutomationModel *model, uint16_t stage_set_id,
                const SCollectedAutomationData &data,
                const std::vector<SStageOverride> &recipe_overrides);

        std::vector<SAutomationAtom> build_alternation_atoms(
                const SAlternatingAgent &agent);

        std::vector<SBackwashRecipe>    collectRecipes(const std::vector<SBackwashCandidate> & backwashAgents);
        void    collect_air_cut_recipe(SBackwashCandidate &candidate, SBackwashRecipe & ricipe);

    };

} // NCore

#endif //NYM_PROJECT_IWATERTREATMENTTEMPLATE_H
