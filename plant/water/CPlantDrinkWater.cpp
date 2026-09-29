#include "CPlantDrinkWater.h"
#include "CInfoBus.h"
#include "CCell.h"
#include <map>
#include "../../../server/sources/logger-common/logger.h"
#include "../../automation/CAutomationModel.h"
#include "../../saver/include/IGeneralTor.h"

namespace NCore
{
    CPlantDrinkWater::CPlantDrinkWater(NCore::CCell* cell, IGeneralTor* tor)
    {
        m_automation_model = new CAutomationModel(this);
        m_project_cell = cell;
        m_general_tor = tor;
        m_type = m_project_cell->get_project_type();
    }

    CPlantDrinkWater::~CPlantDrinkWater() = default;

    std::vector<NCore::SReagentRequirement> CPlantDrinkWater::collectReagents() {
        if (!m_project_cell || !m_project_cell->info_bus()) return {};
        // Пока без потребления — лишь опрос. Потребление (extract) — в aggregateDosingStations.
        // Шаг 2: здесь будет extractPendingRequirements + группировка.
        return m_project_cell->info_bus()->extractPendingRequirements();
    }

    std::vector<NCore::SEquipmentRequest> CPlantDrinkWater::aggregateDosingStations()
    {
        // Шаг 2: группировка по EReagentType, суммирование required_dose,
        // формирование SEquipmentRequest на станцию приготовления.
        // Сейчас заглушка — лишь дренирует шину, чтобы не копилось.
        auto reqs = collectReagents();
        if (reqs.empty()) return {};
        // TODO: aggregate по reagent_type -> один узел дозирования
        // Пока возвращаем пусто, но требования уже сняты с шины.
        return {};
    }

    SPlantValidationResult CPlantDrinkWater::validateHydraulics() {
        SPlantValidationResult res;
        // Шаг 2: проход по tubes, сравнение P/Q насоса vs фильтра, выставить NComponent::set_error.
        // Заглушка — без ошибок.
        return res;
    }

    bool CPlantDrinkWater::buildAutomation()
    {
        auto res = m_automation_model->bind();
        return res;
    }

    // Фильтры могут запросить установку добавить реагентное хозяйство. Пока я не готов к этому
    void CPlantDrinkWater::prepareReagentsRequests(std::vector<NCore::SEquipmentRequest>& out)
    {
        if (!m_project_cell || !m_project_cell->info_bus())
        {
            return;
        }

        const auto reqs = m_project_cell->info_bus()->extractPendingRequirements();
        if (reqs.empty())
        {
            return;
        }

        // Группировка по типу реагента -> один узел дозирования на тип.
        std::map<NCore::EReagentType, float> sum;
        for (auto &r : reqs)
        {
            sum[r.reagent_type] += r.required_dose;
        }

        for (auto &[type, dose] : sum)
        {
            NCore::SEquipmentRequest rq;
            rq.sender = nullptr;
            rq.sender_conductor = nullptr;
            rq.id_equip = 0;
            rq.params = { dose };
            (void)type;
            out.push_back(std::move(rq));
        }
    }

    SPlantValidationResult CPlantDrinkWater::audit()
    {
        SPlantValidationResult hydraulic = validateHydraulics();
        SPlantValidationResult stage1;

        return hydraulic;
    }

    void CPlantDrinkWater::requestEquipment(std::vector<NCore::SEquipmentRequest>&& requests)
    {
        if (requests.empty()) return;
        if (!m_lastRequest.empty() && requests == m_lastRequest) return;
        m_lastRequest = requests;
        m_interrogator.process(std::move(requests));
    }

    bool CPlantDrinkWater::isEquipReady() const {
        return m_interrogator.isReady();
    }

    bool CPlantDrinkWater::pollEquipResult()
    {
        if (!m_interrogator.isReady())
            return false;

        if (!audit().hasErrors())
        {
            //
            return true;
        }

        for (auto &in : m_project_cell->get_base_inputs())
        {
            in->put_ob(m_general_tor->general_working_body(), nullptr);
        }

        // set_equipmentProxy() уже вызван внутри CServerInterrogator::executeGetEquip()
        // до ready.store(release). Осталось только очистить дедупликатор,
        // чтобы следующий идентичный batch снова ушёл на сервер если понадобится.
        // Не очищаем m_lastRequest полностью — оставляем для дедупликации,
        // но сигнал о готовности возвращаем.
        CLogger::instance().info("CPlantDrinkWater", "pollEquipResult: equipment ready, count cached {}",
                                 m_lastRequest.size());
        return false;
    }

    bool CPlantDrinkWater::tryRequestEquipment()
    {
        NCore::CInfoBus* busInfo = m_project_cell->info_bus();

        // Установка смотрит, заказали ли фильтры реагенты и если да, дописывает дозаторы в тот же batch,
        // поэтому "пусто" значит нет ни equipment-запросов, ни реагентных требований.
        // bool noReagent = busInfo->is_requirementEmpty(); // но пока рано

        bool noEquip = busInfo->is_requestEmpty();

        if (noEquip) return false;

        std::vector<NCore::SEquipmentRequest> tmp = busInfo->extractPendingRequests();

        // Дозаторы: группировка по EReagentType -> SEquipmentRequest на EWB_DOZER_PREP.
        // prepareReagentsRequests(tmp);

        if (tmp.empty()) return false;

        if (!m_lastRequest.empty() && tmp == m_lastRequest)
        {
            return false;
        }

        requestEquipment(std::move(tmp));
        return true;
    }
}
