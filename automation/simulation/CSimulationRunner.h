#ifndef NYM_PROJECT_CSIMULATIONRUNNER_H
#define NYM_PROJECT_CSIMULATIONRUNNER_H

#include <vector>
#include <map>
#include "../CAutomationAtoms.h"


namespace NCore
{
    class CAutomationModel;
    class Instrument;
    class Actuator;
    class ISimulationShadow;
    class CSimGraphManager;

    /** @brief Снимок одной грани — для табличного/диагностического отображения.
     *  Полноценный CSimulationTrace с кольцевым буфером — отдельная задача,
     *  здесь только минимум, чтобы раннер уже был к чему подключать. */
    struct SSimulationGrain
    {
        float    time_sec{0};
        Tstring  description;   // "atom[N] condition[K] flipped: ..." — заполняется по месту диффа
    };

    class CSimulationRunner
    {
    public:
        explicit CSimulationRunner(CAutomationModel *model);
        ~CSimulationRunner();

        /** @brief (пере)инициализирует всё рантайм-состояние с нуля из текущего
         *  содержимого модели. Вызывается сразу конструктором и повторно —
         *  автоматически, колбэком модели при каждом успешном bind(). */
        void rebuild();

        /** @brief один шаг тонкого внутреннего шага (granularity), возвращает true,
         *  если на этом шаге зафиксирована "грань" (диф условий). */
        bool step();

        void set_granularity(float sec) { m_granularity = sec; }
        [[nodiscard]] float time() const { return m_t; }

        [[nodiscard]] const std::vector<SSimulationGrain>& grains() const { return m_grains; }

        /** @brief печатает всю накопленную к данному моменту хронику в консоль */
        void print_grains() const;

        /** @brief прогоняет N секунд модельного времени и сразу печатает
         *  каждую грань в консоль по мере появления (не постфактум) */
        void run_and_print(float duration_sec);

    private:
        // --- индексация Layer 4, строится в rebuild() -----------------------------
        using TInstrumentKey = std::tuple<NComponent*, ESignalRole, uint16_t>;
        using TActuatorKey   = std::tuple<NComponent*, ECommandRole, uint16_t>;

        std::map<TInstrumentKey, Instrument*> m_instrument_index;
        std::map<TActuatorKey,   Actuator*>   m_actuator_index;

        // --- рантайм-состояние internal variables, index-aligned с
        //     model->internal_variables() ------------------------------------------
        std::vector<float> m_variable_values;

        // --- рантайм-состояние atom set ---------------------------------------------
        uint16_t m_active_set_id{0};
        float    m_time_since_entry{0};

        // --- предыдущий снимок условий, для диффа -----------------------------------
        std::vector<uint8_t> m_prev_supervisory_results;
        std::vector<uint8_t> m_prev_active_set_results; // атомы + переходы активного набора

        float m_t{0};
        float m_granularity{1.0f};

        CAutomationModel *m_model;
        std::vector<SSimulationGrain> m_grains;
        std::vector<std::unique_ptr<Instrument>> m_service_instruments; // владение CServiceInstrument для переменных

        std::map<uint64_t ,ISimulationShadow*> m_simShadowsMap;
        CSimGraphManager * m_graphManager{nullptr};

        void tick_all_shadows();

        // --- вычисления --------------------------------------------------------------
        float read_signal(NComponent *owner, ESignalRole role, uint16_t index) const;
        float resolve_threshold(const SCondition &c) const;
        bool  evaluate_condition(const SCondition &c) const;
        bool  evaluate_trigger(const STrigger &trig) const;

        void  advance_variables();
        void  apply_actions(const std::vector<SAtomCommand> &actions);
        void  apply_single_command(const SAtomCommand &cmd);

        // возвращает true, если где-то был диф относительно m_prev_*
        bool  evaluate_and_diff();

        static Tstring describe_command(const SAtomCommand &cmd);
        static Tstring describe_transition(const SAtomSet &from, const SAtomSet &to);
        const SAtomSet* find_set(uint16_t id) const;

        void process_command(const SAtomCommand &command);
    };
}

#endif //NYM_PROJECT_CSIMULATIONRUNNER_H
