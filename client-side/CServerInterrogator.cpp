#include "../include/CServerInterrogator.h"

#include "../../libcore/equipment/water/include/CEquipmentLightFilterProxy.h"
#include "../../server/sources/logger-common/limits.h"

#include "CInfoBus.h"
#include "../../server/sources/logger-common/logger.h"


#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <utility>
#include <sys/poll.h>

struct CServerInterrogator::Impl
{
    std::string host;
    uint16_t port;
    CThreadPool* pool = nullptr;

    std::vector<NCore::SEquipmentRequest> requests;
    std::vector<NCore::SEquipmentResponse> responses;
    mutable std::mutex mutex;
    std::atomic<bool> ready{true};

    int sockfd = -1;

    Impl(std::string  h, uint16_t p, CThreadPool* pl)
            : host(std::move(h)), port(p), pool(pl)
    {
    }

    ~Impl()
    {
        if (sockfd >= 0)
        {
            close(sockfd);
        }
    }

    bool connectToServer()
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            fprintf(stderr, "CServerInterrogator: socket() failed\n");
            return false;
        }

        struct sockaddr_in serverAddr{};
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0)
        {
            fprintf(stderr, "CServerInterrogator: invalid address %s\n", host.c_str());
            close(sockfd);
            sockfd = -1;
            return false;
        }

        if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        {
            CLogger::instance().error("CServerInterrogator", "connect() to {}:{} failed", host.c_str(), port);
            close(sockfd);
            sockfd = -1;
            return false;
        }

        CLogger::instance().info("CServerInterrogator", "Connected to {}:{}", host.c_str(), port);
        return true;
    }

    void disconnect()
    {
        if (sockfd >= 0)
        {
            close(sockfd);
            sockfd = -1;
        }
    }
};

CServerInterrogator::CServerInterrogator(
        const std::string& host,
        uint16_t port,
        CThreadPool* pool)
        : pimpl(new Impl(host, port, pool))
{
}

CServerInterrogator::~CServerInterrogator()
{
    delete pimpl;
}

void CServerInterrogator::process(std::vector<NCore::SEquipmentRequest>&& requests)
{
    {
        std::lock_guard<std::mutex> lock(pimpl->mutex);
        pimpl->requests = std::move(requests);
        pimpl->ready = false;
        pimpl->responses.clear();
    }

    if (pimpl->pool)
    {
        pimpl->pool->submit(this, [this]()
        {
            executeGetEquip();
        });
    }
    else
    {
        executeGetEquip();
    }
}

bool CServerInterrogator::isReady() const
{
    return pimpl->ready.load(std::memory_order_acquire);
}

std::vector<NCore::SEquipmentResponse> CServerInterrogator::getResponses()
{
    if (!pimpl->ready.load())
    {
        CLogger::instance().error("CServerInterrogator", "CServerInterrogator: getResponses() on busy {}");
        return {};
    }

    std::lock_guard<std::mutex> lock(pimpl->mutex);
    return pimpl->responses;
}

std::vector<uint8_t> CServerInterrogator::serializeRequests(
        const std::vector<NCore::SEquipmentRequest>& requests) const
{
    std::vector<uint8_t> buffer;

    SClientPacket queryPacket;
    queryPacket.magic = PROTOCOL_MAGIC;
    queryPacket.version = PROTOCOL_SUPPORTED_VERSION;
    queryPacket.query_type = EQueryType::EPT_EQUIP_REQ;
    queryPacket.id_client = 1;
    queryPacket.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
    ).count();

    queryPacket.keyType = (ESignatureKeyType)0;


    for (auto &req : requests)
    {
        SEquipRequestData packet;
        packet.id_component = req.sender->get_id();
        packet.componentType = NCore::ComponentConverter::get_int_TComponentType(req.sender->get_component_type());
        packet.natureType = (NCore::EComponentTypes)req.sender->get_component_type().index();
        packet.param_amount = req.params.size();

        std::vector<uint8_t> request;
        request.resize(sizeof(SEquipRequestData) + sizeof(float)*req.params.size());
        memcpy(request.data(), &packet, sizeof(SEquipRequestData));
        memcpy(request.data() + sizeof(SEquipRequestData),
               req.params.data(),
               sizeof(float) * req.params.size());

        buffer.insert(buffer.end(), request.begin(), request.end());
    }

    //fprintf(stderr, "Serialized %zu components\n", requests.size());

    queryPacket.length = buffer.size();

    uint8_t encription = 0;
    const auto* packetData = reinterpret_cast<const uint8_t*>(&queryPacket);
    const auto* encrData = reinterpret_cast<const uint8_t*>(&encription);
    buffer.insert(buffer.begin(), packetData, packetData + sizeof(SClientPacket));
    buffer.insert(buffer.begin(), encrData, encrData + 1);

    CLogger::instance().info("CServerInterrogator", "serialized buffers:{} requests into {} bytes", requests.size(), buffer.size());

    return buffer;
}

bool CServerInterrogator::sendRequest(const std::vector<uint8_t>& buffer)
{
    if (pimpl->sockfd < 0)
    {
        return false;
    }

    ssize_t sent = send(pimpl->sockfd, buffer.data(), buffer.size(), 0);
    if (sent != static_cast<ssize_t>(buffer.size()))
    {
        CLogger::instance().error("CServerInterrogator::sendRequest", "send() failed, sent {} of {}",
                                 sent, buffer.size());
        return false;
    }

    CLogger::instance().info("CServerInterrogator", "sent {} bytes", buffer.size());
    return true;
}

bool CServerInterrogator::receiveResponse(std::vector<uint8_t>& buffer)
{
    if (pimpl->sockfd < 0)
    {
        return false;
    }

    buffer.clear();

    constexpr int TOTAL_TIMEOUT_MS = 25000;
    constexpr int CHUNK_TIMEOUT_MS  = 5000;
    constexpr size_t CHUNK_SIZE     = 4096;

    auto start = std::chrono::steady_clock::now();

    while (true)
    {
        // 1. Ждём данные или закрытие соединения
        struct pollfd fds[1];
        fds[0].fd = pimpl->sockfd;
        fds[0].events = POLLIN;

        // тут я не уверен... наверное лучше.. линукс мля
        int ret = poll(fds, 1, CHUNK_TIMEOUT_MS);

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll");
            return false;
        }

        if (ret == 0) // Порция данных не пришла вовремя
        {
            CLogger::instance().error("CServerInterrogator", "receive timeout (no data chunk)");
            return false;
        }

        if (!(fds[0].revents & (POLLIN | POLLHUP | POLLERR)))
        {
            continue;
        }

        // 2. Читаем доступные данные
        std::array<uint8_t, CHUNK_SIZE> tmp{};
        ssize_t n = recv(pimpl->sockfd, tmp.data(), tmp.size(), 0);

        if (n == 0)
        {
            CLogger::instance().info("CServerInterrogator", "received {} bytes (connection closed)", buffer.size());
            return !buffer.empty();
        }

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            perror("recv");
            return false;
        }

        // 3. Склеиваем буфер
        buffer.insert(buffer.end(), tmp.begin(), tmp.begin() + n);

        // 4. Защита от общего таймаута на весь приём
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();

        if (elapsed > TOTAL_TIMEOUT_MS)
        {
            fprintf(stderr, "CServerInterrogator: total receive timeout (%zu bytes read)\n",
                    buffer.size());
            return false;
        }
    }
}


bool CServerInterrogator::parseResponseHeader(SClientPacket &responsePacket, const std::vector<uint8_t>& buffer)
{
    memcpy(&responsePacket, buffer.data()+1, sizeof(SClientPacket));

    return  responsePacket.query_type == EQueryType::EPT_EQUIP_REQ ||
            responsePacket.query_type == EQueryType::EPT_ORGS_REQ;
}

// ---------------------------------------------------------------------------
// Основная задача в пуле потоков
// ---------------------------------------------------------------------------

void CServerInterrogator::executeGetEquip()
{
    auto &logger = CLogger::instance();

    std::vector<NCore::SEquipmentRequest> requests;
    {
        std::lock_guard<std::mutex> lock(pimpl->mutex);
        requests = pimpl->requests;  // копия для работы
    }

    if (requests.empty())
    {
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }
    logger.info("CServerInterrogator", "processing {} requests", requests.size());

    // 1. Подключаемся к серверу
    if (!pimpl->connectToServer())
    {
        logger.error("CServerInterrogator", "connection failed");
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    // 2. Сериализуем запросы
    std::vector<uint8_t> outBuffer = serializeRequests(requests);
    if (outBuffer.empty())
    {
        logger.error("CServerInterrogator", "serialization failed");
        pimpl->disconnect();
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    // 3. Отправляем
    if (!sendRequest(outBuffer))
    {
        logger.error("CServerInterrogator", "send failed");
        pimpl->disconnect();
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    // 4. Получаем ответ
    std::vector<uint8_t> inBuffer;
    if (!receiveResponse(inBuffer))
    {
        logger.error("CServerInterrogator", "receive failed");
        pimpl->disconnect();
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    // disconnect any case
    pimpl->disconnect();


    // 5. Проверяем encrypted_flag и извлекаем заголовок
    //    Минимальный буфер: 1 (flag) + sizeof(SClientPacket)
    if (inBuffer.size() < 1 + sizeof(SClientPacket))
    {
        logger.error("CServerInterrogator", "response too short ({} bytes)", inBuffer.size());
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    SClientPacket inPacket{};
    memcpy(&inPacket, inBuffer.data() + 1, sizeof(SClientPacket));

    if (inPacket.magic != PROTOCOL_MAGIC)
    {
        logger.error("CServerInterrogator", "bad magic {}", inPacket.magic);
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    if (inPacket.status != ERS_SUCCESS)
    {
        fprintf(stderr, "CServerInterrogator: server returned error status %u\n",
                static_cast<unsigned>(inPacket.status));
        pimpl->ready.store(true, std::memory_order_release);
        return;
    }

    std::vector<uint8_t>  equipRawData;
    equipRawData.insert(equipRawData.end(), inBuffer.begin() + sizeof(SClientPacket),
                        inBuffer.end());

    std::vector<SEquipProxy> proxies;

    // 6. Парсим payload: смещение сразу после flag + SClientPacket
    if (inPacket.query_type == EQueryType::EPT_EQUIP_REQ)
    {
        // payload начинается с байта 1 + sizeof(SClientPacket)
        const size_t payloadStart = 1 + sizeof(SClientPacket);
        const size_t payloadEnd   = payloadStart + inPacket.length;

        if (inBuffer.size() < payloadEnd)
        {
            fprintf(stderr, "CServerInterrogator: truncated payload "
                            "(have %zu, need %zu)\n", inBuffer.size(), payloadEnd);
            pimpl->ready.store(true, std::memory_order_release);
            return;
        }

        const uint8_t* p   = inBuffer.data() + payloadStart;
        const uint8_t* end = inBuffer.data() + payloadEnd;

        // Собираем SEquipLight, привязанные к component_id из SReqHeader
        std::vector<NCore::SEquipLight> result;

        while (p + sizeof(SReqHeader) <= end)
        {
            SReqHeader reqHdr{};
            memcpy(&reqHdr, p, sizeof(SReqHeader));
            p += sizeof(SReqHeader);

            for (uint16_t i = 0; i < reqHdr.response_amount; ++i)
            {
                if (p + sizeof(SResponseEquip) > end)
                {
                    fprintf(stderr, "CServerInterrogator: SResponseEquip out of bounds\n");
                    goto parse_done;
                }

                SResponseEquip re{};
                memcpy(&re, p, sizeof(SResponseEquip));
                p += sizeof(SResponseEquip);

                if (p + re.param_amount * sizeof(float) > end)
                {
                    fprintf(stderr, "CServerInterrogator: float params out of bounds\n");
                    goto parse_done;
                }

                NCore::SEquipLight light{};
                light.comp_id  = reqHdr.component_id;   // привязка к компоненту из запроса
                light.equip_id = re.equip_id;
                // id_manufacturer и manufacturer недоступны в SResponseEquip
                // (они есть только в SEquipProxy — ответ на EPT_EQIP_ID);
                // заполняем пустыми значениями, данные придут отдельным запросом EPT_EQIP_ID если нужны
                light.id_manufacturer = 0;
                memset(light.manufacturer, 0, sizeof(light.manufacturer));
                memcpy(light.equip_name, re.name, sizeof(re.name));

                light.params.resize(re.param_amount);
                if (re.param_amount > 0)
                    memcpy(light.params.data(), p, re.param_amount * sizeof(float));
                p += re.param_amount * sizeof(float);

                result.push_back(std::move(light));
            }
        }

        parse_done:
        logger.info("CServerInterrogator", "parsed {} equipment entries", result.size());

        std::lock_guard<std::mutex> lock(pimpl->mutex);

        for (auto &req : requests)
        {
            req.sender->set_equipmentProxy(getProxies(req, result));
        }

        pimpl->responses.clear();
        pimpl->requests.clear();
        pimpl->ready.store(true, std::memory_order_release);
    }
    else
    {
        fprintf(stderr, "CServerInterrogator: unexpected query_type %u in response\n",
                static_cast<unsigned>(inPacket.query_type));
        pimpl->ready.store(true, std::memory_order_release);
    }
    logger.info("CServerInterrogator", "processing completed");

}


void CServerInterrogator::processOrgQuery(SOrganizationRequest request)
{
    /*
    {
        std::lock_guard<std::mutex> lock(pimpl->mutexOrg);
        pimpl->requestOrg = std::move(request);
        pimpl->readyOrg = false;
        pimpl->responseOrg.clear();
    }

    if (pimpl->pool)
    {
        pimpl->pool->submit(this, [this]()
        {
            execGetOrg();
        });
    }
    else
    {
        execGetOrg();
    }*/
}

bool CServerInterrogator::is_readyOrgQuery() const
{
    return true;//pimpl->readyOrg.load(std::memory_order_acquire);
}

std::vector<CCompany> CServerInterrogator::getOrgResult()
{
    std::vector<CCompany> res;

    CCompany comp1;
    comp1.set_name("Test 0");
    comp1.set_id(1);
    comp1.set_address("nowhere");
    comp1.set_id_country(1);
    comp1.set_id_ownership(1);
    comp1.set_id_type(4);
    comp1.set_email("Some email");

    CCompany comp2(comp1);
    comp2.set_name("Test 1");
    comp2.set_id(2);
    comp2.set_id_type(5);

    res.push_back(comp1);
    res.push_back(comp2);

    return res;
}

void CServerInterrogator::execGetOrg()
{
    // Имитация задержки сети
    /*
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::lock_guard<std::mutex> lock(pimpl->mutexOrg);

    int id0 = pimpl->requestOrg.idOrg.empty() ? 1 : pimpl->requestOrg.idOrg.at(0);
    int id1 = pimpl->requestOrg.idOrg.empty() ? 2 : pimpl->requestOrg.idOrg.at(1);

    nlohmann::json comp0 = {
            {"id", id0},
            {"id_ownership", 2},
            {"id_type", 4},
            {"id_country", 4},
            {"is_active", true},
            {"is_banned", false},
            {"name", "Tech Corp"},
            {"ownership", "Private"},
            {"type", "LLC"},
            {"inn", "1234567890"},
            {"phone", "+1234567890"},
            {"email", "info@techcorp.com"},
            {"address", "123 Main St, City"},
            {"country", "USA"},
            {"site", "https://techcorp.com"},
            {"employees", {101, 102, 103, 104}}
    };

    nlohmann::json comp1 = {
            {"id", id1},
            {"id_ownership", 2},
            {"id_type", 5},
            {"id_country", 4},
            {"is_active", true},
            {"is_banned", false},
            {"name", "Evel Corp"},
            {"ownership", "Private"},
            {"type", "LLC"},
            {"inn", "1234567890"},
            {"phone", "+1234567890"},
            {"email", "info@techcorp.com"},
            {"address", "123 Main St, City"},
            {"country", "USA"},
            {"site", "https://techcorp.com"},
            {"employees", {101, 102, 103, 104}}
    };

    CCompany company_0, company_1;
    company_0.initFromJson(comp0);
    company_1.initFromJson(comp1);

    pimpl->responseOrg.clear();
    pimpl->responseOrg.push_back(company_0);
    pimpl->responseOrg.push_back(company_1);
    pimpl->readyOrg.store(true, std::memory_order_release);
    */
}

std::vector<NCore::SEquipLight>
CServerInterrogator::getProxies(NCore::SEquipmentRequest &request, std::vector<NCore::SEquipLight> &response)
{
    std::vector<NCore::SEquipLight> result;

    for (auto &item : response)
    {
        if (item.comp_id == request.sender->get_id())
        {
            result.push_back(item);
        }
    }

    return result;
}
