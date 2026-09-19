//
// Слой 4 автоматизации как отдельная сущность — не часть CSubProject.
// По аналогии с паттерном Shadow: CSubProject создаёт этот класс в конструкторе
// и дальше не занимается его внутренностями.
//
// ОБНОВЛЕНО (эта сессия, п.2 из плана): добавлен add_transition() — SAtomSet теперь
// несёт transitions отдельным списком (см. CAutomationAtoms.h, STransition), раньше
// переход был действием атома (AAK_ACTIVATE_SET) — эта форма выведена из атомов,
// набор больше не заполняется через один только add_atom_to_set.
// m_active_set_id сохранён: даже при цели "raннер = вычислитель для кодогена",
// он остаётся нужен как рантайм-состояние для дебаг-симуляции (см. обсуждение
// CSimulationShadow) — просто больше не единственный смысл этого поля.
//

#ifndef NYM_PROJECT_CAUTOMATIONMODEL_H
#define NYM_PROJECT_CAUTOMATIONMODEL_H

#include <vector>
#include <functional>
#include "CAutomationAtoms.h"

class CSubProject;

namespace NCore
{
    class CCap;
    class Instrument;
    class Actuator;
    class IInstallationTemplate;

    struct SModelValidationIssue
    {
        NComponent*  target_owner{nullptr};
        ECommandRole role{};
        uint16_t     index{0};
        Tstring      description;
    };


    class CAutomationModel
    {
    public:
        CAutomationModel() = delete;
        CAutomationModel(const CAutomationModel &) = delete;
        CAutomationModel(CAutomationModel &&) = delete;

        explicit CAutomationModel(CSubProject *owner);
        ~CAutomationModel();

        /** @brief пересобирает Instrument/Actuator/SAtomSet с нуля через
         *  IInstallationTemplate::bind(). Явная GUI-команда — не связана с refresh_cell(),
         *  вызывается отдельно. */
        bool bind();

        [[nodiscard]] std::vector<SModelValidationIssue> validate() const;

        [[nodiscard]] std::vector<Instrument*> instruments() const;
        [[nodiscard]] std::vector<Actuator*>   actuators() const;

        [[nodiscard]] const std::vector<SAtomSet>&        atom_sets() const;
        [[nodiscard]] const std::vector<SAutomationAtom>& supervisory_atoms() const;
        [[nodiscard]] uint16_t                            active_set_id() const;

        static bool evaluate_time(const STimeTrigger &t, float t_since_entry);

        [[nodiscard]] const std::vector<SVariableBehaviorSpec>& internal_variables() const;
        [[nodiscard]] std::vector<SAggregateSignalSpec>  aggregate_signals() const { return {}; }


        //!< отладочный дамп в консоль, по образцу CAutomationCollector::print
        void print() const;

        IInstallationTemplate*  installation_template() const { return m_tmpl; }
        void set_callback_on_bind(std::function<void()> handler) { m_cbOnBind = std::move(handler); }



    private:
        //!< единственный, кому разрешено населять модель — резолвит роли из хуков в конкретные target'ы
        friend class IInstallationTemplate;

        void add_internal_variable(const SVariableBehaviorSpec &spec);
        void add_aggregate_signal(NComponent *owner, const SAggregateSignalSpec &spec);

        std::vector<SVariableBehaviorSpec> m_variables;
        struct SModelAggregate { NComponent* owner{nullptr}; SAggregateSignalSpec spec; };
        std::vector<SModelAggregate> m_aggregates;

        Instrument* add_instrument(CCap *anchor, const SSignalRole &role);              // -> CPhysicalInstrument
        Instrument* add_service_instrument(const SSignalRole &role, std::function<float()> source); // -> CServiceInstrument
        Actuator*   add_actuator(CCap *anchor, const SCommandRole &role);

        // CAutomationModel.h, приватная секция рядом с add_service_instrument
        Instrument* add_internal_instrument(NComponent *owner, const SSignalRole &role);
        Actuator*   add_internal_actuator(NComponent *owner, const SCommandRole &role);

        // новое: принять уже готовый SAtomSet целиком (внутренние наборы не строятся
        // через add_atom_to_set по одному атому — компонент отдаёт готовый набор)
        void        adopt_atom_set(SAtomSet &&set);

        //!< создаёт набор и возвращает его id — НЕ указатель: m_atom_sets реаллоцируется
        //!< при последующих add_atom_set, любой ранее выданный SAtomSet* стал бы мёртвым.
        uint16_t    add_atom_set(uint16_t id, const std::string &display_name);
        //!< дозаполнение атомами уже существующего набора — ищет по id каждый раз,
        //!< безопасно при любом порядке вызовов и любом количестве наборов.
        void        add_atom_to_set(uint16_t set_id, const SAutomationAtom &atom);
        //!< НОВОЕ: дозаполнение переходами уже существующего набора — тот же принцип
        //!< id-lookup, что и add_atom_to_set, по той же причине (реаллокация вектора).
        void        add_transition(uint16_t set_id, const STransition &transition);
        void        add_supervisory_atom(const SAutomationAtom &atom);
        void        set_initial_active_set(uint16_t id);

        //!< находит набор по id для внутреннего использования add_atom_to_set/add_transition.
        //!< nullptr, если набор с таким id ещё не создан add_atom_set — вызывающий обязан
        //!< создать набор раньше, чем в него что-либо добавлять.
        SAtomSet*   find_atom_set(uint16_t set_id);

        static void print_atom(const SAutomationAtom &atom, const std::string &indent);

        void        clear();

        CSubProject                     * m_owner; //!< не владеющий; доступ к general_tor()/project_cell() для bind()
        std::vector<Instrument*>          m_instruments;
        std::vector<Actuator*>            m_actuators;

        IInstallationTemplate           * m_tmpl{nullptr};
        std::function<void()> m_cbOnBind;

        std::vector<SAtomSet>             m_atom_sets;
        std::vector<SAutomationAtom>      m_supervisory_atoms;
        uint16_t                          m_active_set_id{0};
    };
}

#endif //NYM_PROJECT_CAUTOMATIONMODEL_H
