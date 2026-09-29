#pragma once
#include "../IPlant.h"
#include "CThreadPool.h"
#include "CServerInterrogator.h"


namespace NCore
{
    class CAutomationModel;

    // Первая установка — питьевая вода. Остальные водные пока переиспользуют её логику.
    class CPlantDrinkWater : public IPlant {
    public:
        CPlantDrinkWater(NCore::CCell* cell, IGeneralTor* tor);
        ~CPlantDrinkWater() override;

        [[nodiscard]] NCore::CCell* projectCell() const override { return m_project_cell; }
        [[nodiscard]] NCore::CAutomationModel* automationModel() const override { return m_automation_model; }
        [[nodiscard]] NCore::E_PROJECT_TYPE type() const override { return m_type; }

        std::vector<NCore::SReagentRequirement> collectReagents() override;
        std::vector<NCore::SEquipmentRequest> aggregateDosingStations() override;
        SPlantValidationResult validateHydraulics() override;
        bool buildAutomation() override;
        void prepareReagentsRequests(std::vector<NCore::SEquipmentRequest>& out);// override;
        SPlantValidationResult audit() override;

        // В варианте Б автоматизация пока остаётся в CSubProject — сюда прокидываем
        // уже существующую модель, чтобы не ломать bind(). На шаге 3 перенесём владение.
        // void attachAutomationModel(NCore::CAutomationModel* m) { m_automationModel = m; }

        void requestEquipment(std::vector<NCore::SEquipmentRequest>&& requests) override;
        [[nodiscard]] bool isEquipReady() const override;
        bool pollEquipResult() override;
        bool tryRequestEquipment() override;

    private:

        NCore::E_PROJECT_TYPE m_type{NCore::pt_undef};
        // NCore::CAutomationModel* m_automationModel{nullptr}; // не владеет на шаге 1


        CThreadPool m_pool{6};
        CServerInterrogator m_interrogator{"127.0.0.1", 40000, &m_pool};
        std::vector<NCore::SEquipmentRequest> m_lastRequest;
    };
}


