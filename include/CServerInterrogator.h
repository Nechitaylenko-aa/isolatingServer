#ifndef NYM_PROJECT_CSERVERINTERROGATOR_H
#define NYM_PROJECT_CSERVERINTERROGATOR_H

#include <vector>
#include <mutex>
#include <atomic>

#include "NComponent.h"
#include "CThreadPool.h"
#include "../../saver/include/CCompany.h"
#include "../../server/sources/logger-common/wire_types.h"

struct SOrganizationRequest
{
    std::vector<uint8_t>    organizationTypes;
    std::vector<Tstring>    possibleNames;
    std::vector<uint64_t>   idOrg;
};



namespace NCore
{
    struct SEquipmentRequest;
    struct SEquipmentResponse;
}

class CThreadPool;

class CServerInterrogator
{
public:
    CServerInterrogator(
            const std::string& host,
            uint16_t port,
            CThreadPool* pool = nullptr
    );

    ~CServerInterrogator();

    // Запускает асинхронную обработку запросов
    void process(std::vector<NCore::SEquipmentRequest>&& requests);

    // Проверяет готовность результата
    [[nodiscard]] bool isReady() const;

    // Забирает результат (вызывать только когда isReady() == true)
    //std::vector<NCore::SEquipmentResponse> getResponses();

    void processOrgQuery(SOrganizationRequest request);
    std::vector<CCompany> getOrgResult();
    [[nodiscard]] bool is_readyOrgQuery() const;

private:
    // Задача для пула потоков
    void executeGetEquip();

    // Сериализация запросов в бинарный буфер
    [[nodiscard]] std::vector<uint8_t> serializeRequests(
            const std::vector<NCore::SEquipmentRequest>& requests
    ) const;

    // Отправка и приём через сокет
    bool sendRequest(const std::vector<uint8_t>& buffer);
    bool receiveResponse(std::vector<uint8_t>& buffer);

    // Разбор заголовка ответа
    bool parseResponseHeader(SClientPacket &responsePacket, const std::vector<uint8_t>& buffer);

    void execGetOrg();

    struct Impl;
    Impl* pimpl;

    static std::vector<NCore::SComponentProxy> getProxies(NCore::SEquipmentRequest &request, std::vector<NCore::SComponentProxy> &response);
};


#endif //NYM_PROJECT_CSERVERINTERROGATOR_H
