#include "CSimGraphManager.h"
#include "shadows/include/ISimulationShadow.h"
#include "CSubProject.h"
#include "CCell.h"
#include "CCap.h"
#include "CConductor.h"
#include "NComponent.h"
#include "shadows/include/CSimValveTwoDirectional.h"
#include "shadows/include/CSimValveCut.h"


namespace NCore
{
    CSimGraphManager::CSimGraphManager(CSubProject *project, std::map<Tuint64, ISimulationShadow*> *shadows)
        : m_project(project), m_shadows(shadows) {}

    Tuint64 CSimGraphManager::component_id(NComponent *c)
    {
        return c->get_id();
    }

    void CSimGraphManager::mark_dirty()
    {
        m_dirty = true;
    }

    // ---- построение базового графа -------------------------------------------------------
    //
    // Тот же принцип обхода, что и в IInstallationTemplate::find_components_*_matching:
    // идём от кепки через её CConductor, перебираем ВСЕХ соседей на трубе нужного
    // направления, для каждого — либо он сам узел с тенью (останов, кладём в результат),
    // либо boundary без тени (останов, ничего не кладём — за водоразделом не ищем),
    // либо прозрачный узел без тени (идём сквозь ВСЕ его выходы/входы дальше).
    //
    // Отличие от оригинала: не агрегируем находки со всех output() компонента в один
    // список — каждый физический выход/вход даёт свой отдельный вектор, чтобы клапан
    // мог впоследствии выбрать нужный индекс как активную ветку.

    static void collect_shadow_neighbors(CCap *from_cap, bool downstream,
                                          const std::map<Tuint64, ISimulationShadow*> &shadows,
                                          std::vector<CCell*> &visited,
                                          std::vector<NComponent*> &out)
    {
        if (!from_cap) return;

        CConductor *pipe = from_cap->get_conductor(nullptr);
        if (!pipe) return;

        E_CAP_DIRECTION want = downstream ? CD_INPUT : CD_OUTPUT;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);
            if (!neighbor_cap || neighbor_cap == from_cap)
                continue;
            if (neighbor_cap->get_contour() == E_CONTOUR_TYPE::CT_ADDITIONAL)
                continue;
            if (neighbor_cap->get_direction(nullptr) != want)
                continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell)
                continue;
            if (std::find(visited.begin(), visited.end(), neighbor_cell) != visited.end())
                continue;
            visited.push_back(neighbor_cell);

            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp)
                continue;

            if (shadows.count(neighbor_comp->get_id()))
            {
                out.push_back(neighbor_comp);   // нашли тень — по этой ветке дальше не идём
                continue;
            }

            if (neighbor_comp->is_flow_boundary())
                continue;   // за водоразделом без тени — обрыв, глубже не идём

            // прозрачный узел без тени — продолжаем сквозь ВСЕ его выходы/входы
            uint16_t count = downstream ? neighbor_comp->countOut() : neighbor_comp->countIn();
            for (uint16_t k = 0; k < count; ++k)
            {
                CCap *next_cap = downstream ? neighbor_comp->output(k) : neighbor_comp->input(k);
                if (!next_cap || next_cap->get_contour() == E_CONTOUR_TYPE::CT_ADDITIONAL)
                    continue;
                collect_shadow_neighbors(next_cap, downstream, shadows, visited, out);
            }
        }
    }

    void CSimGraphManager::rebuild_base()
    {
        m_base.clear();

        CCell *root = m_project->project_cell();
        if (!root) return;

        for (auto *cell : *root->get_components())
        {
            auto *comp = dynamic_cast<NComponent*>(cell);
            if (!comp) continue;
            if (!m_shadows->count(comp->get_id())) continue; // не узел графа теней вообще

            SEdges edges;

            for (uint16_t out = 0; out < comp->countOut(); ++out)
            {
                std::vector<CCell*> visited{ comp };
                std::vector<NComponent*> found;
                collect_shadow_neighbors(comp->output(out), true, *m_shadows, visited, found);
                edges.downstream.push_back(std::move(found));
            }

            for (uint16_t in = 0; in < comp->countIn(); ++in)
            {
                std::vector<CCell*> visited{ comp };
                std::vector<NComponent*> found;
                collect_shadow_neighbors(comp->input(in), false, *m_shadows, visited, found);
                edges.upstream.push_back(std::move(found));
            }

            m_base[comp] = std::move(edges);
        }

        m_dirty = true; // актуальный граф больше не соответствует новой базе
    }

    // ---- актуальный граф: "наложение" клапанов на базовый ---------------------------------

    void CSimGraphManager::rebuild_actual_if_dirty() const
    {
        if (!m_dirty) return;

        m_actual_downstream_cache.clear();
        m_actual_upstream_cache.clear();

        for (auto &[comp, edges] : m_base)
        {
            auto sh_it = m_shadows->find(comp->get_id());
            ISimulationShadow *shadow = (sh_it != m_shadows->end()) ? sh_it->second : nullptr;

            bool blocked = false;
            uint16_t active_out = 0;
            uint16_t active_in  = 0;

            if (auto *tw = dynamic_cast<CSimValveTwoDirectional*>(shadow))
            {
                active_out = tw->active_branch();
                active_in  = tw->active_branch();
            }
            else if (auto *cut = dynamic_cast<CSimValveCut*>(shadow))
            {
                blocked = !cut->is_open();
            }

            if (!blocked)
            {
                if (active_out < edges.downstream.size())
                    m_actual_downstream_cache[comp] = edges.downstream[active_out];
                if (active_in < edges.upstream.size())
                    m_actual_upstream_cache[comp] = edges.upstream[active_in];
            }
            // blocked == true — узел временно не проецирует рёбра в актуальный граф вообще
        }

        m_dirty = false;
    }

    std::vector<NComponent*> CSimGraphManager::actual_neighbors(NComponent *from, bool downstream) const
    {
        auto &cache = downstream ? m_actual_downstream_cache : m_actual_upstream_cache;
        auto it = cache.find(from);
        return it != cache.end() ? it->second : std::vector<NComponent*>{};
    }

    NComponent* CSimGraphManager::advance_to_nearest_shadow(NComponent *from, bool downstream,
                                                             std::vector<NComponent*> &visited) const
    {
        // база уже хранит только ближайших соседей С ТЕНЬЮ (collect_shadow_neighbors
        // не проходит сквозь тени вглубь) — значит один шаг actual_neighbors() либо
        // сразу даёт искомых кандидатов, либо (для узла-клапана без открытой ветки)
        // не даёт ничего, и это законный "не найдено", а не повод искать глубже.
        auto neighbors = actual_neighbors(from, downstream);

        for (auto *n : neighbors)
        {
            if (std::find(visited.begin(), visited.end(), n) != visited.end())
                continue;
            return n; // первый найденный — водораздел для этого направления
        }

        return nullptr;
    }
}
