#pragma once

#include <algorithm>

#include "core-types.h"
#include <vector>

namespace NCore
{
    class CCell;
    class CAutomationModel;
}

class IGeneralTor;

namespace NCore
{
    struct SReagentRequirement;
    struct SEquipmentRequest;
}

struct SPlantValidationIssue
{
    Tstring message;
    uint64_t targetId{0};
    bool isError{true};
};

struct SPlantValidationResult
{
    std::vector<SPlantValidationIssue> issues;
    [[nodiscard]] bool hasErrors() const
    {
        if (std::any_of(issues.begin(),issues.end(), [](const SPlantValidationIssue& issue)
        {
            return issue.isError;
        }))
            return true;
        return false;
    }
};

namespace NCore
{
    /** @brief IPlant — доменный фасад установки. Зона ответственности: 1. Цикл запросов сервера на подбор оборудования
     * и останов цикла опроса. 2. автоматизация установки - генерация атомов "протоязыка" и симуляция (пока текстовая)
     * на базе генерации*/
    class IPlant {
    public:
        static IPlant* create(NCore::CCell* cell, IGeneralTor* tor);
        virtual ~IPlant() = default;
        [[nodiscard]] virtual NCore::CCell* projectCell() const = 0;
        [[nodiscard]] virtual NCore::CAutomationModel* automationModel() const = 0;
        [[nodiscard]] virtual NCore::E_PROJECT_TYPE type() const = 0;
        virtual std::vector<NCore::SReagentRequirement> collectReagents() = 0;
        virtual std::vector<NCore::SEquipmentRequest> aggregateDosingStations() = 0;
        virtual SPlantValidationResult validateHydraulics() = 0;
        virtual bool buildAutomation() = 0;
        virtual bool resolveTopology() { return true; }
        [[nodiscard]] CAutomationModel*   model() const { return m_automation_model; }
        CCell           *   project_cell() {return m_project_cell; }
        IGeneralTor     *   general_tor() { return m_general_tor; }

        virtual void  start_equipment_query();

        // virtual void prepareReagentsRequests(std::vector<NCore::SEquipmentRequest>& out) = 0;

        // Вызывается в CResourceHandler::slot_processPendingTasks ПОСЛЕ processResponses().
        virtual SPlantValidationResult audit() = 0;

        /** @brief отправляет запрос на сервер подбора оборудования (не блокирует). */
        virtual void requestEquipment(std::vector<NCore::SEquipmentRequest>&& requests) = 0;
        /** @brief true, если предыдущий запрос уже обработан (или запросов не было). */
        virtual bool isEquipReady() const = 0;
        /** @brief вызывать по таймеру, когда isEquipReady() == true. Компоненты уже
         * получили set_equipmentProxy() внутри сетевого воркера — здесь только
         * дозаписывается признак, что новых данных для put_ob() достаточно.
         * @return true если был обработан реальный ответ (не пустая проверка)
         */
        virtual bool pollEquipResult() = 0;

        /** @brief Вызывается из CResourceHandler::callbackOnFlowComplete (колбэк CCap,
         * который сработал по завершении обхода). Вся доменная логика — здесь, а не в GUI:
         * дренирует SEquipmentRequest/SReagentRequirement из CInfoBus своей клетки (projectCell()),
         * дописывает дозаторы через prepareReagentsRequests(), при непустом батче — requestEquipment().
         * GUI (Qt) знает только "нужно ли включить QTimer", доменных деталей не видит.
         * @return true если реально ушёл новый запрос на сервер (GUI должен запустить m_pendingTimer)
         */
        virtual bool tryRequestEquipment() = 0;

    protected:
        CAutomationModel * m_automation_model{nullptr};
        CCell            * m_project_cell;
        IGeneralTor      * m_general_tor;
    };
}
