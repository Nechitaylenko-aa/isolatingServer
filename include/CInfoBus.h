//
// Created by artem on 04.04.24.
//

#ifndef LIBCORED_CINFOBUS_H
#define LIBCORED_CINFOBUS_H

#include "core-types.h"
#include <types.h>   // явно — за E_MEASURE_UNITS, см. примечание в чате

namespace NCore
{
    class NComponent;

    struct SEquipmentRequest
    {
        NCore::NComponent* sender = nullptr;
        uint64_t id_equip = 0;              // 0 = подбор, >0 = прямой запрос
        std::vector<float> params;          // для подбора или уточнения
        friend bool operator==(const SEquipmentRequest &lhs, const SEquipmentRequest &rhs)
        {
            return lhs.id_equip == rhs.id_equip && lhs.sender == rhs.sender && lhs.params == rhs.params;
        }
    };
    struct SEquipLight
    {
        uint16_t comp_id{0};
        uint16_t equip_id{0};
        uint16_t id_manufacturer{0};
        char     equip_name[30];
        char     manufacturer[30];
        std::vector<float> params;
    };


    struct SEquipmentResponse
    {
        NCore::NComponent* target = nullptr;
        uint64_t id_equip = 0;
        std::vector<uint8_t> rawProxyData;  // сырые байты прокси для компонента
    };

    /** @brief Тип реагента, требуемый компонентом. Решение о типе принимает ТЕНЬ компонента-источника
     *  (см. IShadow) — станция реагент не выбирает, только агрегирует уже принятые решения по типу. */
    enum class EReagentType : uint8_t
    {
        Coagulant,   // коагулянт (соли алюминия/железа) — осаждение взвеси, снижение мутности и цветности
    };

    /** @brief Требование на реагентную подготовку от компонента-источника (например, фильтра) в адрес
     *  станции/сборщика топологии. В отличие от SEquipmentRequest это НЕ запрос "подбери оборудование мне" —
     *  ответа компоненту-источнику не предполагается: станция сама решает, из скольких таких требований
     *  собрать один узел дозирования (агрегируя по reagent_type и суммируя required_dose).
     *  @details получатель определяется станцией по типу source (component->get_subtype()), отдельного
     *  поля-тега не заводим, чтобы не дублировать то, что уже узнаваемо через sender. */
    struct SReagentRequirement
    {
        NCore::NComponent* source = nullptr;
        EReagentType        reagent_type{};
        float               required_dose{0.f};    // концентрация — юрисдикция компонента-источника,
                                                     // пересчёт в расход/типоразмер дозатора — вне неё
        E_MEASURE_UNITS     trigger_parameter{};    // причина требования — для отчёта/трассировки, станции не нужна
    };


    class CCell;
/**@brief Этот класс (точнее его указатели есть абсолютно во всех элементах проекта. Пока это лишь заготовка,
 * но в дальнейшем это будет "нервной системой" проекта собирать транспорт данных и команд */
    class CInfoBus
    {
    public:
        CInfoBus() = delete;
        CInfoBus(const CInfoBus &) = delete;
        CInfoBus(CInfoBus &&) = delete;
        explicit CInfoBus(CCell *owner);
        virtual ~CInfoBus();

        [[nodiscard]] bool is_requestEmpty() const;
        void addRequest(SEquipmentRequest&& req);
        std::vector<SEquipmentRequest> extractPendingRequests();

        void  addResponse(const SEquipmentResponse &response);
        void  processResponses();

        // отдельный, независимый от equipment-запроса канал: требования на реагентную подготовку
        [[nodiscard]] bool is_requirementEmpty() const;
        void addRequirement(SReagentRequirement && requirement);
        std::vector<SReagentRequirement> extractPendingRequirements();

    protected:
    private:
        std::vector<SEquipmentRequest> m_pendingRequests;
        std::vector<SEquipmentResponse> m_responses;
        std::vector<SReagentRequirement> m_pendingRequirements;
    };
}

#endif //LIBCORED_CINFOBUS_H
