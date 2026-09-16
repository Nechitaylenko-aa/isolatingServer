//
// Created by artem on 04.04.24.
//

#ifndef LIBCORED_CINFOBUS_H
#define LIBCORED_CINFOBUS_H

#include "core-types.h"

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

    protected:
    private:
        std::vector<SEquipmentRequest> m_pendingRequests;
        std::vector<SEquipmentResponse> m_responses;
    };
}

#endif //LIBCORED_CINFOBUS_H
