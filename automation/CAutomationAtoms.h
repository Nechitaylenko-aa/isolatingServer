
#ifndef NYM_PROJECT_CAUTOMATIONATOMS_H
#define NYM_PROJECT_CAUTOMATIONATOMS_H


#include <types.h>
#include <core-types.h>

namespace NCore
{
    class NComponent;
    // ============================================================================
    // Слой 2 — декларативный (без изменений, объявляется в NComponent-хуках)
    // ============================================================================

    enum ESignalRole : uint16_t {
        SR_PRESSURE_PV,
        SR_LEVEL_HH,
        SR_LEVEL_LL,
        SR_LEVEL_UPPER,
        SR_LEVEL_MID,
        SR_LEVEL_BOTT,
        SR_PUMP_STATUS,
        SR_DP_FILTER,
        SR_PUMP_STATION_FAIL,
        SR_PUMP_STATION_RUNNING,
        SR_TIME_SINCE_SET_ACTIVE,
        SR_PRESSURE_LOW,
        SR_PRESSURE_HIGH,
        SR_FLOW_LOW,
        SR_FLOW_HIGH,
        SR_GAS_DETECT,
        SR_QUALITY_FAIL,
        SR_QUALITY_OK,
        SR_JUST_DO_IT,
        SR_MOTO_HOURS,
        SR_TEMPERATURE_PV,
        SR_FLOW_RATE,
        SR_FLOW_TOTAL,
        SR_VALVE_STATUS,
        SR_LAMP_STATUS,
        // ... по мере появления новых типов компонентов, растёт вместе с каталогом
        SR_COUNT
    };

    static Tstring  signal_roles_str[SR_COUNT]
    {
        "SR_PRESSURE_PV",
        "SR_LEVEL_HH",
        "SR_LEVEL_LL",
        "SR_LEVEL_UPPER",
        "SR_LEVEL_MID",
        "SR_LEVEL_BOTT",
        "SR_PUMP_STATUS",
        "SR_DP_FILTER",
        "SR_PUMP_STATION_FAIL",
        "SR_PUMP_STATION_RUNNING",
        "SR_TIME_SINCE_SET_ACTIVE",
        "SR_PRESSURE_LOW",
        "SR_PRESSURE_HIGH",
        "SR_FLOW_LOW",
        "SR_FLOW_HIGH",
        "SR_GAS_DETECT",
        "SR_QUALITY_FAIL",
        "SR_QUALITY_OK",
        "SR_JUST_DO_IT",
        "SR_MOTO_HOURS",
        "SR_TEMPERATURE_PV",
        "SR_FLOW_RATE",
        "SR_FLOW_TOTAL",
        "SR_VALVE_STATUS"
    };

    enum ECommandRole : uint16_t {
        CR_PUMP_START_STOP,
        CR_VALVE_OPEN_CLOSE,
        CR_FLOW_DIRECTION_VALVE,
        CR_BLOWER_START_STOP,
        CR_PUMP_FAULT_RESET,        // НОВОЕ: агрегатная команда станции, какой конкретно
        CR_LAMP_ON_OFF,
        CR_COUNT
    };

    static Tstring command_roles_str[CR_COUNT]
            {
                    "CR_PUMP_START_STOP", "CR_VALVE_OPEN_CLOSE",
                    "CR_FLOW_DIRECTION_VALVE", "CR_BLOWER_START_STOP",
                    "CR_PUMP_FAULT_RESET", "CR_LAMP_ON_OFF"
            };


    /** @brief Обещание компонента "как я хочу быть триггернут" — слой 2, декларативный.
     *  НЕ путать с STrigger ниже: тот — исполняемая форма слоя 4, заполняется bind()'ом. */
    enum ETriggerType     { TT_CONTINUOUS_PV, TT_CONDITION, TT_SCHEDULE };
    enum EControlAlgorithm{ EA_NONE, EA_PID, EA_ON_OFF };

    /** @brief Чтобы validation мог осмысленно объяснить что конфликтует (не просто "два атома"), стоит завести
     * лёгкий тег происхождения на атоме —  источник:
     * добавить полем в SAutomationAtom (или в SAtomCommand). Он не участвует в разрешении конфликта автоматически —
     * только в диагностике ("boundary-атом от Е82 конфликтует с schedule-атомом на НС108") и в будущем даёт человеку
     * зацепку, где именно руками расписать явную развязку (например: расписание работает, только пока уровень
     * не в аварийной зоне — тогда это уже осознанно написанный TRIG_EVENT-guard, а не implicit порядок вставки).*/
    enum EAtomOrigin { AO_BOUNDARY, AO_SCHEDULE, AO_MANUAL, AO_INTERNAL /* и т.п. */ }; // происхождение атома


    /**
     * @brief Различение "важна ли идентичность конкретного Cap для смысла сигнала".
     * Физически Cap-якорь есть всегда — разница не в наличии Cap, а в том, несёт ли
     * его идентичность смысловую нагрузку. Свойство роли, фиксируется в декларации хука.
     */
    enum ECapSemantics : uint8_t {
        CS_FLOW_SPECIFIC,     //!< идентичность Cap — часть смысла сигнала (давление на входе != на выходе)
        CS_COMPONENT_SCOPED   //!< Cap — просто физический якорь; смысл сигнала — про компонент целиком
    };

    /** @brief Декларация "что физически измеряется". cap_index/is_input компонент обязан
     *  выбирать детерминированно — не "любой доступный". */
    struct SSignalRole  {
        ESignalRole      role;
        uint16_t         index{0};
        E_MEASURE_UNITS  unit{E_MEASURE_UNITS::EMU_AMOUNT};
        VSubtypes        subtype{EMUAMO::EA_ENUM};
        EStandardPrefix  prefix{EStandardPrefix::ESP_NONE};
        ECapSemantics    cap_semantics{CS_COMPONENT_SCOPED};
        bool             is_input{false};   //!< значим при CS_FLOW_SPECIFIC: искать в input() или output()
        uint16_t         cap_index{0};      //!< индекс внутри input()/output()
    };

    /** @brief Декларация "чем физически управляют". */
    struct SCommandRole {
        ECommandRole     role{ECommandRole::CR_COUNT};
        uint16_t         index{0};
        E_MEASURE_UNITS  unit{E_MEASURE_UNITS::EMU_AMOUNT};
        VSubtypes        subtype{EMUAMO::EA_ENUM};
        ECapSemantics    cap_semantics{CS_COMPONENT_SCOPED};
        bool             is_input{false};
        uint16_t         cap_index{0};
        //bool             default_activate{false};
    };

    enum EScheduleNature : uint8_t {
        SN_DUTY_CYCLE,   // мигает актуатором внутри текущего набора, топологию не меняет — генерируется механически
        SN_PROCEDURE     // меняет топологию потока / требует своей цепочки SAtomSet — доменная сборка, не механическая
    };
    enum ESetpointSource  { ESS_TOR_OUT_PRESSURE, ESS_TOR_OUT_TURBIDITY, ESS_FIXED, ESS_SIGNAL };

    struct SAutomationSpec {
        ETriggerType trigger;
        EControlAlgorithm algorithm{EA_NONE};
        ESignalRole  input_role{};        // значим только для TT_CONTINUOUS_PV / TT_CONDITION
        uint16_t     input_index{0};
        uint32_t     period_seconds{0};   // значим только для TT_SCHEDULE
        uint32_t     duration_seconds{0};
        EScheduleNature schedule_nature{SN_DUTY_CYCLE};  // значим только при TT_SCHEDULE
        std::vector<SCommandRole> output_roles;
        ESetpointSource setpoint_source{ESS_FIXED};
        float fixed_setpoint{0};          // значим только при ESS_FIXED
    };

    enum EDesignConstraintType : uint16_t {
        DC_FILTRATION_VELOCITY_VS_PRESSURE,
        DC_FLOW_CAPACITY_VS_NEIGHBOR,
    };

    struct SDesignConstraintSpec {
        EDesignConstraintType type;    // строгий идентификатор — резолвер/Shadow знает, что считать по нему
        Tstring display_name;          // ТОЛЬКО для GUI/отчёта, не участвует в логике
    };

    // ============================================================================
    // Слой 4 — исполняемая форма (заполняется bind()'ом, читается раннером и кодогеном)
    // ============================================================================

    enum ECompareOp : uint8_t
    {
        CMP_LT, CMP_LE, CMP_GT, CMP_GE, CMP_EQ, CMP_NE
    };

    /** @brief READ+COMPARE над одной ролью. Используется внутри STrigger при TRIG_EVENT.
     *  Открытый вопрос (automation_layer_timeline_revision.md §3): различать ли структурно
     *  "готовое событие компонента" и "сравнение continuous PV" — решено НЕ различать,
     *  пока не вылезет неудобство на реальном каталоге ролей. Для готовых событий op
     *  вырождается в CMP_EQ true. */

    struct SCondition
    {
        NComponent*      signal_owner{nullptr};
        ESignalRole      role{};
        uint16_t         index{0};
        ECompareOp       op{CMP_EQ};
        ESetpointSource  threshold_source{ESS_FIXED};
        float            fixed_threshold{0};

        // НОВОЕ: при threshold_source == ESS_SIGNAL сравниваем не с константой/ToR,
        // а с другим сигналом (fixed_threshold в этом случае не используется).
        NComponent*      compare_owner{nullptr};
        ESignalRole      compare_role{};
        uint16_t         compare_index{0};
    };

    enum ETriggerKind : uint8_t
    {
        TRIG_EVENT,  //!< сравнение по SCondition
        TRIG_NO_CONDITIONS,
        TRIG_TIME    //!< время от входа в набор, разовое или периодическое
    };

    /** @brief offset=0 закрывает случай "сразу при входе в набор" — отдельного on_enter
     *  механизма нет, это обычный time-triggered атом/переход с offset=0.
     *  period=0 — разовое действие; period>0 — периодическое, пока набор активен
     *  (НЕ разворачивается в список точек — параметр остаётся до этапа генерации/раннера,
     *  на кодогене превращается в пару FB-таймеров TON/TOF). */
    struct STimeTrigger
    {
        float offset_from_entry_sec{0};
        float period_sec{0};
        float duration_sec{0};
    };

    /** @brief Единая исполняемая форма триггера — и для команды внутри набора
     *  (в составе SAutomationAtom), и для перехода между наборами (в составе STransition). */
    struct STrigger
    {
        ETriggerKind  kind{TRIG_EVENT};
        std::vector<SCondition>  conditions{};   //!< TRIG_EVENT: все элементы AND; пусто == TRIG_NO_CONDITIONS
        STimeTrigger  time{};    //!< значимо при kind == TRIG_TIME
    };

    struct SAtomCommand
    {
        NComponent * target_owner{nullptr};
        SCommandRole command;
        bool         activate{true};  // true = старт/открыть, false = стоп/закрыть
    };


    enum EBoundaryReactionScope : uint8_t { BRS_SUPERVISORY, BRS_NORMAL };

    /** @brief Неделимая единица алгоритма. Действие — обычная команда (SCommandRole),
     *  без обёртки: переход между наборами больше не действие атома, см. STransition.
     *  actions — вектор: одно условие может вызывать несколько независимых команд разом. */
    struct SAutomationAtom
    {
        STrigger                    trigger{};
        std::vector<SAtomCommand>   actions;
        EAtomOrigin                 origin{AO_MANUAL};
        EBoundaryReactionScope      scope{BRS_SUPERVISORY};   // НОВОЕ — переносится из исходного SBoundaryInterlockCandidate
    };

    /** @brief Переход между наборами. from_set_id НЕ хранится — структурно неявен:
     *  transition лежит внутри SAtomSet, из которого выполняется переход. to_set_id
     *  обязателен и явен. */
    struct STransition
    {
        uint16_t   to_set_id{0};
        STrigger   trigger{};
    };


    enum EAtomSetScope : uint8_t { SCOPE_INSTALLATION, SCOPE_COMPONENT_INTERNAL };

    struct SAtomSet
    {
        uint16_t                      id{0};
        Tstring                       display_name;
        std::vector<SAutomationAtom>  atoms;
        std::vector<STransition>      transitions;
        EAtomSetScope                 scope{SCOPE_INSTALLATION};
        NComponent*                   owner{nullptr};              // НОВОЕ, значим только при SCOPE_COMPONENT_INTERNAL
    };

    /** @brief Длительность набора для общестанционного total_cycle_duration —
     *  вспомогательная проектная величина, НЕ участвует в диспетчеризации переходов
     *  (это делают STransition::trigger независимо). Не реализуется в этой сессии —
     *  структура зафиксирована заранее, чтобы не пересматривать форму позже. */
    enum EDurationSource : uint8_t
    {
        EDS_TOR_SPECIFIED,     //!< задано ТЗ напрямую
        EDS_COMPUTED_FORMULA,  //!< объём/расход соседей, требует подтверждённого DC_FLOW_CAPACITY_VS_NEIGHBOR
        EDS_COMPUTED_SHADOW,   //!< через существующий путь IShadow
        EDS_NOT_ESTIMABLE      //!< честно неизвестно — потребитель обязан явно это учитывать
    };

    struct SAtomSetDuration
    {
        uint16_t          set_id{0};
        EDurationSource    source{EDS_NOT_ESTIMABLE};
        float              duration_sec{0};  //!< валидно, если source != EDS_NOT_ESTIMABLE
    };

    // ============================================================================
    // wire_boundary_patterns — общий каталог пар ролей (не per-компонент, не per-E_PROJECT_TYPE)
    // ============================================================================
    enum EBoundarySearchDirection : uint8_t { BSD_UPSTREAM, BSD_DOWNSTREAM, BSD_BIDIRECTIONAL };


    /** @brief Пара "что физически измеряется" -> "чем это гасится по умолчанию".
     *  Не пара типов компонентов — обход идёт по РОЛИ, не по типу, иначе поиск
     *  ломается, если между источником сигнала и актуатором стоят пассивные компоненты. */
    struct SBoundaryReactionRule
    {
        ESignalRole              trigger_role;
        EBoundarySearchDirection direction;
        ECommandRole             reaction_role;
        bool                     activate;
        EBoundaryReactionScope   scope;
    };

    /** @brief Растёт по мере появления новых пар "опасность -> стандартная реакция".
     *  Каждая запись — кандидат на автоматический вывод safety-интерлока, не
     *  гарантированное решение (см. IInstallationTemplate::wire_boundary_patterns —
     *  результат всегда кандидат, автоподтверждение не реализовано, открытый вопрос). */
    inline const std::vector<SBoundaryReactionRule> kBoundaryReactionCatalog = {
            // аварийные случаи
            {SR_LEVEL_HH,   BSD_UPSTREAM,   CR_PUMP_START_STOP,  false, BRS_SUPERVISORY },
            {SR_LEVEL_HH,   BSD_UPSTREAM,   CR_VALVE_OPEN_CLOSE,  true, BRS_SUPERVISORY },
            {SR_LEVEL_LL,   BSD_DOWNSTREAM, CR_PUMP_START_STOP,  false, BRS_SUPERVISORY },

            // ============================================
            // УРОВНИ (штатное управление насосами)
            // ============================================
            {SR_LEVEL_UPPER,   BSD_UPSTREAM,   CR_PUMP_START_STOP, false,  BRS_NORMAL },
            {SR_LEVEL_MID,     BSD_UPSTREAM,   CR_PUMP_START_STOP, true,  BRS_NORMAL },
            {SR_LEVEL_BOTT,    BSD_UPSTREAM,   CR_PUMP_START_STOP, true,  BRS_NORMAL },


            {SR_LEVEL_UPPER,   BSD_DOWNSTREAM,   CR_PUMP_START_STOP, true,  BRS_NORMAL },
            {SR_LEVEL_MID,     BSD_DOWNSTREAM,   CR_PUMP_START_STOP, true,  BRS_NORMAL },
            {SR_LEVEL_BOTT,    BSD_DOWNSTREAM,   CR_PUMP_START_STOP, false,  BRS_NORMAL },
    };

    // ============================================================================
    // Разделение ролей на flow-derived / state-derived (для Instrument::value())
    // ============================================================================

    /**
     * @brief Роли, значение которых НЕ выводимо из COperatingBody на анкер-кепке —
     * это состояние оборудования (уровень в ёмкости, статус насоса), не параметр
     * протекающего рабочего тела. Компоненты, объявляющие такую роль в
     * required_signals()/required_commands(), при bind() в режиме SIMULATION должны
     * получить CSimulationShadow — сам класс ещё не реализован (следующий шаг после
     * этого), здесь только маркер данных, чтобы разделение не размывалось внутри
     * реализации CPhysicalInstrument::value() веткой по role.
     * @note Роли ВНЕ этого списка (SR_PRESSURE_PV, SR_DP_FILTER) уже работают —
     * читаются напрямую из тела на анкер-кепке через CPhysicalInstrument.
     */
    // CAutomationAtoms.h, строки 333-338 — заменить существующий блок на:

    inline const std::vector<ESignalRole> kStateDerivedSignalRoles = {
            SR_LEVEL_HH,
            SR_LEVEL_UPPER,
            SR_LEVEL_MID,
            SR_LEVEL_BOTT,
            SR_LEVEL_LL,
            SR_PUMP_STATUS,
            SR_PUMP_STATION_FAIL,
            SR_FLOW_RATE,
            SR_VALVE_STATUS,
            SR_PUMP_STATION_RUNNING
    };

    inline bool is_state_derived_role(const ESignalRole &role)
    {
        return std::find(kStateDerivedSignalRoles.begin(), kStateDerivedSignalRoles.end(), role)
               != kStateDerivedSignalRoles.end();
    }

    // ---- Внутренняя persistent/non-persistent переменная компонента (не Instrument,
    // у нас есть система измерений, и тут переменную мы и будем хранить как CMeasureUnit (составляющие),
    // но УЖЕ знаем что за размерность: абстрактное количество, знаем что это
    // E_MEASURE_UNITS unit{E_MEASURE_UNITS::EMU_COUNT, EMUAMO::EA_ITEMS} float value
    // поэтому специально хранить unit и проч. не имеет смысла но E_TYPE_VARIABLE НУЖЕН

    struct SInternalVariable
    {
        NComponent*     owner{nullptr};
        Tstring         name;              // логическое имя переменной, не роль из каталога
        E_TYPE_VARIABLE type_variable{ETV_INT16};
        bool            retain{false};     // per-переменная: true только там, где реально нужно
        float           initial_value{0};
    };

    // известные заранее шаблоны поведения над SInternalVariable — конечный список,
    // не произвольный код
    enum EVariableBehavior : uint8_t
    {
        VB_NONE,
        VB_ACCUMULATE_WHILE_TRUE,   // растёт пока condition_role истинна, до max_value, сброс по reset_role
        VB_ACCUMULATE_RATE          // НОВОЕ: растёт на value(condition_role)*dt каждый тик — condition_role
        // здесь переиспользуется как "источник МГНОВЕННОЙ СКОРОСТИ", не булево условие
        // (тотализатор расхода узла учёта — первый и пока единственный потребитель)
    };

    struct SVariableBehaviorSpec
    {
        SInternalVariable variable;
        EVariableBehavior behavior{VB_NONE};
        ESignalRole       condition_role{};
        uint16_t          condition_index{0};
        bool              has_reset{false};   // без этого флага reset_role неотличим от "забыли задать"
        ECommandRole      reset_role{};
        uint16_t          reset_index{0};
        float             max_value{0};
        ESignalRole       exposed_role{};      // под какой ролью переменная читается через SCondition
        uint16_t          exposed_index{0};     // и под каким index

    };

    // ---- Агрегация N однотипных сигналов в один внешний (AND/OR) ------------------
    enum EAggregateOp : uint8_t { AGG_ALL, AGG_ANY };

    struct SAggregateSignalSpec
    {
        SSignalRole              result_role;      // напр. SR_PUMP_STATION_FAIL
        EAggregateOp             op{AGG_ALL};
        std::vector<SSignalRole> source_roles;     // SR_PUMP_STATUS[0..N-1] и т.п.
    };

    // ---- Известный шаблон "N однотипных агрегатов + ротация" ----------------------
    enum EInternalPattern : uint8_t { IP_NONE, IP_N_UNIT_ROTATION };
    /*enum ERotationAlgorithm : uint8_t { RA_BY_HOURS, RA_BY_LAST_STOP };

    struct SRotationGroupSpec
    {
        uint16_t            unit_count{0};
        uint16_t            working_count{0};   // НОВОЕ: [0, working_count) — обычная ротация;
        // [working_count, unit_count) — резерв, включается
        // только фолт-промоушном внутри Shadow, не статическими атомами
        ERotationAlgorithm  algorithm{RA_BY_HOURS};
        SCommandRole        enable_role{};
        SCommandRole        fault_reset_role{};
        SSignalRole         hours_role{};
        SSignalRole         status_role{};
    };*/
}

#endif //NYM_PROJECT_CAUTOMATIONATOMS_H
