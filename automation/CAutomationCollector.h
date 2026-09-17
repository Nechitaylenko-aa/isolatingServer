//
// Первый приёмник хуков слоя 2 автоматизации.
// bind()/IInstallationTemplate (слой 3) — следующая сессия, сюда не входит.
//

#ifndef NYM_PROJECT_CAUTOMATIONCOLLECTOR_H
#define NYM_PROJECT_CAUTOMATIONCOLLECTOR_H

//#include "NComponent.h"
#include <vector>
#include "CAutomationAtoms.h"


namespace NCore
{
    class NComponent;
    class CCap;
    class CCell;

    /**
     * @brief Плоское, привязанное к владельцу объявление сигнала. cap уже резолвлен в
     * реальный CCap* — эта резолюция нужна только владеющий компонент (слой 2), а не
     * знание ToR, поэтому её можно сделать уже сейчас, до bind().
     * @note Это НЕ Instrument. Тег/source(auto|manual)/io_address появляются только
     * после bind() — здесь только сырая декларация "что физически есть у компонента".
     */
    struct SDeclaredSignal
    {
        NComponent* owner{nullptr};
        SSignalRole role{};
        CCap*       anchor_cap{nullptr}; // nullptr означает ошибку конфигурации хука — см. CAutomationCollector::print
    };

    struct SDeclaredCommand
    {
        NComponent*  owner{nullptr};
        SCommandRole role{};
        CCap*        anchor_cap{nullptr};
    };

    struct SDeclaredAutomation
    {
        NComponent*     owner{nullptr};
        SAutomationSpec spec{};
    };

    struct SDeclaredConstraint
    {
        NComponent*            owner{nullptr};
        SDesignConstraintSpec  spec{};
    };

    struct SCollectedAutomationData
    {
        std::vector<SDeclaredSignal>     signals;
        std::vector<SDeclaredCommand>    commands;
        std::vector<SDeclaredAutomation> automations;
        std::vector<SDeclaredConstraint> constraints;
    };

    /**
     * @brief Обходит все компоненты подпроекта, вызывает у каждого все четыре хука слоя 2,
     * резолвит cap_index/is_input в реальный CCap*, аггрегирует в плоские owner-tagged списки.
     *
     * Осознанно НЕ делает: не создаёт Instrument/Actuator/CControlLoop/CInterlock, не читает
     * ToR, не решает какие из объявленных возможностей реально нужны данному проекту —
     * это задача bind()/IInstallationTemplate (слой 3), следующая сессия.
     *
     * @note subproject_root принимается как CCell*, а не как конкретный CSubProject*, т.к.
     * для обхода дерева нужен только CCell::get_components(). Если у CSubProject есть более
     * подходящий публичный метод для получения списка компонентов — заменить сигнатуру на него.
     */
    class CAutomationCollector
    {
    public:
        static SCollectedAutomationData collect(CCell *subproject_root);

        //!< отладочный дамп в консоль, по образцу CCell::printCell
        static void print(const SCollectedAutomationData &data);

    private:
        static CCap* resolve_anchor(NComponent *owner, bool is_input, uint16_t cap_index);
    };
}

#endif //NYM_PROJECT_CAUTOMATIONCOLLECTOR_H
