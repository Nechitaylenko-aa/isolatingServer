//
// CSimGraphManager — граф теней для симуляционного раннера.
//
// Базовый граф строится ОДИН РАЗ реальным traversal'ом CCap/CConductor (тот же
// принцип обхода, что и find_components_downstream_matching в IInstallationTemplate),
// с предикатом остановки "есть тень в m_shadows" вместо "тип компонента совпадает".
// У узла-разветвителя (клапан с несколькими выходами) хранится ПО ОДНОМУ вектору
// найденных соседей НА КАЖДЫЙ физический выход/вход — не агрегированный список,
// иначе невозможно "наложить" активную ветку клапана поверх.
//
// Актуальный граф — производная проекция базового: для каждого узла-клапана
// выбирается один активный индекс выхода (CSimValveTwoDirectional::active_branch())
// либо узел блокируется целиком (CSimValveCut::is_open() == false). Пересобирается
// ЛЕНИВО, по флагу m_dirty, который клапаны выставляют сами через mark_dirty()
// при apply_command — то есть только тогда, когда состояние клапана реально
// изменилось, не на каждый тик раннера.
//
// is_flow_boundary() — условие остановки обхода даже при отсутствии тени
// (сейчас true только у CWaterCapacity; поскольку тень ёмкости обязательна —
// на практике boundary и "тень найдена" сейчас всегда совпадают, но код должен
// проверять оба условия независимо, на случай будущих boundary-типов без тени).
//
// Мы сознательно НЕ ищем сквозь ёмкость дальше неё самой: ёмкость — водораздел
// графа теней на сегодняшний день, find_nearest всегда останавливается на первой
// найденной тени, глубже неё не идёт.
//

#ifndef NYM_PROJECT_CSIMGRAPHMANAGER_H
#define NYM_PROJECT_CSIMGRAPHMANAGER_H

#include <map>
#include <vector>
#include <algorithm>
//
#include "core-types.h"

class CSubProject;

namespace NCore
{
    class NComponent;
    class CCap;
    class CCell;
    class ISimulationShadow;

    class CSimGraphManager
    {
    public:
        CSimGraphManager() = delete;
        CSimGraphManager(const CSimGraphManager &) = delete;
        CSimGraphManager(CSimGraphManager &&) = delete;

        /**
         * @param project не владеющий, источник project_cell() для обхода компонентов верхнего уровня
         * @param shadows не владеющий, актуальная карта (id компонента -> тень), заполненная
         *                CSimShadowsManager к моменту вызова rebuild_base()
         */
        CSimGraphManager(CSubProject *project, std::map<Tuint64, ISimulationShadow*> *shadows);

        /** @brief полный проход по реальному CCap/CConductor. Вызывать ПОСЛЕ того, как
         *  m_shadows заполнена тенями всех компонентов проекта (иначе предикат
         *  "есть тень" даст неполную картину и часть узлов графа потеряется). */
        void rebuild_base();

        /** @brief дёргается тенью клапана (CSimValveTwoDirectional/CSimValveCut) из
         *  apply_command, когда её состояние реально изменилось. Актуальный граф
         *  пересобирается лениво, при следующем find_nearest. */
        void mark_dirty();

        /**
         * @brief ищет ближайшую тень заданного типа T по направлению потока от from.
         *  Обход прекращается на первом найденном узле, у которого есть тень
         *  (независимо от её типа) — глубже не идёт. Если найденная тень не
         *  приводится к T — результат "не найдено" (мы не ищем "следующую за
         *  первой" тень; первая найденная тень и есть водораздел для этого пути).
         * @return nullptr, если по этому направлению нет ни одной тени, либо
         *         найденная тень не соответствует типу T.
         */
        template<typename T>
        T* find_nearest(NComponent *from, bool downstream) const
        {
            rebuild_actual_if_dirty();

            std::vector<NComponent*> visited{ from };
            NComponent *current = from;

            while (true)
            {
                NComponent *next = advance_to_nearest_shadow(current, downstream, visited);
                if (!next) return nullptr; // путь оборвался — ни разу не встретили T

                visited.push_back(next);

                auto it = m_shadows->find(component_id(next));
                if (it != m_shadows->end())
                    if (auto *t = dynamic_cast<T*>(it->second))
                        return t; // нашли искомый тип — стоп

                current = next; // тень другого типа (клапан и т.п.) — продолжаем ОТ неё дальше
            }
        }


    private:
        //!< по одному вектору соседей НА КАЖДЫЙ физический выход/вход компонента-узла;
        //!< для обычного (не ветвящегося) узла — вектор из одного элемента на индекс
        struct SEdges
        {
            std::vector<std::vector<NComponent*>> downstream; //!< downstream[out_index] = все теневые соседи с output(out_index)
            std::vector<std::vector<NComponent*>> upstream;    //!< upstream[in_index]    = все теневые соседи с input(in_index)
        };

        static Tuint64 component_id(NComponent *c);

        //!< один логический переход по АКТУАЛЬНОМУ графу: для клапана — единственный
        //!< активный сосед (по active_branch()/is_open()), для обычного узла — все
        //!< соседи данного направления сразу (за узлом нет ветвления по смыслу,
        //!< только физическое разветвление трубы без арбитра)
        std::vector<NComponent*> actual_neighbors(NComponent *from, bool downstream) const;

        //!< BFS до первого узла, для которого в m_shadows есть запись, независимо от
        //!< того, к какому конкретно классу тень приводится
        NComponent* advance_to_nearest_shadow(NComponent *from, bool downstream,
                                               std::vector<NComponent*> &visited) const;

        void rebuild_actual_if_dirty() const;

        CSubProject *m_project;
        std::map<Tuint64, ISimulationShadow*> *m_shadows;

        std::map<NComponent*, SEdges> m_base;

        //!< выбранный (активный) сосед на каждый узел-клапан; для обычных узлов
        //!< актуальный граф не отличается от базового — actual_neighbors читает
        //!< m_base напрямую в этом случае, кэш здесь не нужен
        mutable std::map<NComponent*, std::vector<NComponent*>> m_actual_downstream_cache;
        mutable std::map<NComponent*, std::vector<NComponent*>> m_actual_upstream_cache;
        mutable bool m_dirty{true};
    };
}

#endif //NYM_PROJECT_CSIMGRAPHMANAGER_H
