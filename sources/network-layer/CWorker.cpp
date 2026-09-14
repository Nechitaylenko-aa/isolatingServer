//
// Created by artem on 14.08.26.
//

#include "CWorker.h"


#include <algorithm>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>

#include "../database-layer/equip_row_grouping.h"
#include "CMainCoordinator.h"
#include "../logger-common/logger.h"

namespace {
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    constexpr size_t MAX_ENCRYPTED_MSG_BYTES = 500 * 1024;
    constexpr size_t MAX_PLAIN_MSG_BYTES     = 200 * 1024;
} // namespace

CWorker::CWorker(uint32_t workerId,
                 CRequestPipeline&      pipeline,
                 CPacketSerializer&     serializer,
                 CActiveClientRegistry& activeClients,
                 CAdmissionControl&     admission,
                 CDBThread&             dbThread,
                 CEquipQueryDispatcher& equipDispatcher)
        : m_workerId(workerId)
          , m_pipeline(pipeline)
          , m_serializer(serializer)
          , m_activeClients(activeClients)
          , m_admission(admission)
          , m_dbThread(dbThread)
          , m_equipDispatcher(equipDispatcher)
{
}

void CWorker::assignConnection(int fd)
{
    m_activeConnCount.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_newConnMutex);
    m_newConnFds.push_back(fd);
}

void CWorker::run()
{
    m_running.store(true, std::memory_order_release);
    std::vector<int> pendingFds;

    while (m_running.load(std::memory_order_acquire))
    {
        {
            std::lock_guard<std::mutex> lock(m_newConnMutex);
            for (int fd : m_newConnFds)
                pendingFds.push_back(fd);
            m_newConnFds.clear();
        }

        std::vector<pollfd> pfds;
        pfds.reserve(pendingFds.size());
        for (int fd : pendingFds)
            pfds.push_back(pollfd{fd, POLLIN, 0});

        int rc = pfds.empty() ? 0 : ::poll(pfds.data(), pfds.size(), WORKER_INBOX_POLL_MS);
        if (rc > 0)
        {
            std::vector<int> ready;
            for (auto& p : pfds)
                if (p.revents & (POLLIN | POLLHUP | POLLERR))
                    ready.push_back(p.fd);

            for (int fd : ready)
            {
                onNewConnection(fd); // читает целиком, обрабатывает, закрывает
                pendingFds.erase(std::remove(pendingFds.begin(), pendingFds.end(), fd), pendingFds.end());
            }
        }
        else if (pfds.empty())
        {
            // нечего опрашивать - просто ждём следующего тика для pollInbox
            struct timespec ts{0, WORKER_INBOX_POLL_MS * 1000000L};
            ::nanosleep(&ts, nullptr);
        }

        pollInbox();
    }
}

void CWorker::stop()
{
    m_running.store(false, std::memory_order_release);
}

size_t signatureByteSize(ESignatureKeyType t)
{
    switch (t)
    {
        case ESignatureKeyType::KT_RSA_1024: return 128;  // 1024 бит / 8
        case ESignatureKeyType::KT_RSA_2048: return 256;
        case ESignatureKeyType::KT_RSA_4096: return 512;
        case ESignatureKeyType::KT_NONE:
        default: return 0;
    }
}

std::optional<std::vector<uint8_t>> CWorker::readMessage(int fd)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);
    uint8_t chunk[4096];
    size_t limit = MAX_ENCRYPTED_MSG_BYTES; // уточняется после первого байта
    bool lengthDefined = false;

    size_t  minSize = sizeof(SClientPacket) + 1;
    size_t totalSize = 0;

    while (true)
    {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n < 0)
        {
            return std::nullopt;
        }

        if (n == 0 )
        {
            return std::nullopt;
        }

        buffer.insert(buffer.end(), chunk, chunk + n);

        if (buffer.size() >= minSize && !lengthDefined)
        {
            SClientPacket packet;
            size_t signatureLen = 0;
            memcpy(&packet, buffer.data()+1, sizeof(SClientPacket));
            totalSize = minSize + packet.length;

            if (packet.is_signature == 1)
            {
                totalSize += sizeof(SSignature) + signatureByteSize(packet.keyType);
            }
            lengthDefined = true;
        }

        if (lengthDefined && buffer.size() >= totalSize)
        {
            break;
        }

        if (buffer.size() >= 1)
            limit = (buffer[0] == 0x01) ? MAX_ENCRYPTED_MSG_BYTES : MAX_PLAIN_MSG_BYTES;

        if (buffer.size() > limit)
            return std::nullopt; // превышен лимит - разрыв без ответа (server.md)
    }

    if (buffer.empty())
        return std::nullopt;
    CLogger::instance().info("CWorker", "Received bytes: {}", buffer.size());
    return buffer;
}

void CWorker::onNewConnection(int fd)
{
    SConnectionCtx ctx;
    ctx.fd = fd;

    auto raw = readMessage(fd);
    if (!raw.has_value())
    {
        closeConnection(ctx);
        return;
    }

    onRequestReady(ctx, std::move(*raw));
}

void CWorker::onRequestReady(SConnectionCtx& ctx, std::vector<uint8_t> rawData)
{
    SPipelineResult result = m_pipeline.process(rawData.data(), rawData.size());

    switch (result.action)
    {
        case EPipelineAction::DISCONNECT:
            closeConnection(ctx);
            return;

        case EPipelineAction::RESPOND:
            writeAndClose(ctx, std::move(result.responseBytes)); // ctx.admitted == false
            return;

        case EPipelineAction::PROCEED:
        {
            uint16_t id_client = result.header.id_client;
            /*if (!m_activeClients.tryAdmit(id_client, MAX_ACTIVE_CLIENTS))
            {
                closeConnection(ctx);
                return;
            }*/
            ctx.id_client = id_client;
            ctx.admitted  = true;

            // КРИТИЧЕСКАЯ ПРАВКА №1: гейт по здоровью БД до обращения к CDBThread/диспетчеру
            if (!m_admission.isAcceptingConnections())
            {
                auto resp = m_serializer.serializeError(result.header, ERS_DATABASE_QUERY_ERROR, result.wasEncrypted);
                writeAndClose(ctx, std::move(resp));
                return;
            }

            dispatchToBackend(ctx, result.header, std::move(result.payload), result.wasEncrypted);
            return;
        }
    }
}

void CWorker::dispatchToBackend(SConnectionCtx& ctx, const SClientPacket& header,
                                TParsedPayload payload, bool wasEncrypted)
{
    uint32_t requestId = m_nextRequestId.fetch_add(1, std::memory_order_relaxed);
    m_pendingByRequestId[requestId] = ctx; // контекст сохраняется ДО ухода в БД

    std::visit(overloaded{
            [&](const std::vector<SComponentReq>& components) {
                m_equipDispatcher.dispatch(components,
                                           [this, workerId = m_workerId, requestId, header, wasEncrypted]
                                                   (EResponseStatus status, std::vector<SEquipReqRespBlock> blocks) {
                                               auto bytes = (status == ERS_SUCCESS)
                                                            ? m_serializer.serializeEquipReqResponse(header, blocks, wasEncrypted)
                                                            : m_serializer.serializeError(header, status, wasEncrypted);
                                               m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                                           });
            },
            [&](const SEquipIdRequest& req) {
                m_dbThread.executeEquipIdQuery(req.ids,
                                               [this, workerId = m_workerId, requestId, header, wasEncrypted]
                                                       (std::vector<SEquipRow> rows, EDatabaseError error) {
                                                   std::vector<uint8_t> bytes;
                                                   if (error != DATABSE_OK) {
                                                       bytes = m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, wasEncrypted);
                                                   } else {
                                                       auto items = buildEquipProxyItems(rows);
                                                       bytes = m_serializer.serializeEquipIdResponse(header, items, wasEncrypted);
                                                   }
                                                   m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                                               });
            },
            [&](const SOrgTypeRequest& req) {
                m_dbThread.executeOrgsByType(req.types,
                                             [this, workerId = m_workerId, requestId, header, wasEncrypted]
                                                     (std::vector<SOrgProxy> orgs, EDatabaseError error) {
                                                 auto bytes = (error == DATABSE_OK)
                                                              ? m_serializer.serializeOrgsResponse(header, orgs, wasEncrypted)
                                                              : m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, wasEncrypted);
                                                 m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                                             });
            },
            [&](const SOrgIdRequest& req) {
                auto [filtered, droppedCount] = filterOrgIdsToUint16(req.ids);
                if (droppedCount > 0)
                    CLogger::instance().warn("CWorker", "EPT_ORGS_ID: {} id отброшено (>65535)", droppedCount);
                m_dbThread.executeOrgByID(filtered,
                                          [this, workerId = m_workerId, requestId, header, wasEncrypted]
                                                  (std::vector<SOrgProxy> orgs, EDatabaseError error) {
                                              auto bytes = (error == DATABSE_OK)
                                                           ? m_serializer.serializeOrgsResponse(header, orgs, wasEncrypted)
                                                           : m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, wasEncrypted);
                                              m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                                          });
            },
    }, payload);
}

void CWorker::pollInbox()
{
    std::vector<SPendingResult> results;
    {
        std::lock_guard<std::mutex> lock(m_inbox.m_mutex);
        results = std::move(m_inbox.m_results);
        m_inbox.m_results.clear();
    }

    for (auto& r : results)
    {
        auto it = m_pendingByRequestId.find(r.request_id);
        if (it == m_pendingByRequestId.end())
        {
            CLogger::instance().error("CWorker", "inbox: неизвестный request_id={}", r.request_id);
            continue;
        }
        SConnectionCtx ctx = it->second;
        m_pendingByRequestId.erase(it);
        writeAndClose(ctx, std::move(r.responseBytes));
    }
}

void CWorker::writeAndClose(SConnectionCtx& ctx, std::vector<uint8_t> responseBytes)
{
    if (ctx.fd >= 0)
    {
        size_t sent = 0;
        while (sent < responseBytes.size())
        {
            ssize_t n = ::send(ctx.fd, responseBytes.data() + sent, responseBytes.size() - sent, 0);
            if (n <= 0)
                break; // клиент уже не получит ответ - соединение всё равно закрываем ниже
            sent += static_cast<size_t>(n);
        }

        if (sent > 0)
        {
            CLogger::instance().info("CWorker::writeAndClose", "sent {} bytes to sock {}", sent, ctx.fd);
        }
    }
    closeConnection(ctx);
}

void CWorker::closeConnection(SConnectionCtx& ctx)
{
    if (ctx.admitted)
    {
        m_activeClients.release(ctx.id_client);
        ctx.admitted = false;
    }
    if (ctx.fd >= 0)
    {
        ::close(ctx.fd);
        ctx.fd = -1;
    }
    if (m_activeConnCount.load(std::memory_order_relaxed) > 0)
        m_activeConnCount.fetch_sub(1, std::memory_order_relaxed);
}

std::vector<SEquipProxyRespItem> CWorker::buildEquipProxyItems(const std::vector<SEquipRow>& rows)
{
    auto grouped = groupEquipRowsByType(rows); // группировка по SEquipTypeKey, здесь не важна - flatten
    std::vector<SEquipProxyRespItem> items;
    for (auto& [key, list] : grouped)
        for (auto& g : list)
        {
            SEquipProxyRespItem item;
            item.meta.id              = g.idEquip;
            item.meta.id_manufacturer = g.idManufacturer;
            CPacketSerializer::copyFixedString(item.meta.equip_name, sizeof(item.meta.equip_name), g.equipName);
            // КРИТИЧЕСКАЯ ПРАВКА №3: имени производителя нет в SEquipRow - поле пустое
            CPacketSerializer::copyFixedString(item.meta.manufacturer, sizeof(item.meta.manufacturer), "");
            for (auto& f : g.fields) item.params.push_back(f.equipFieldValueF);
            item.meta.param_amount = static_cast<uint16_t>(item.params.size());
            items.push_back(std::move(item));
        }
    if (!items.empty())
        CLogger::instance().warn("CWorker",
                                 "SEquipProxy::manufacturer не заполняется - нет источника имени в SEquipRow (ТЗ-6, правка №3)");
    return items;
}

std::pair<std::vector<uint16_t>, size_t> CWorker::filterOrgIdsToUint16(const std::vector<uint32_t>& ids)
{
    std::vector<uint16_t> out;
    size_t dropped = 0;
    for (auto id : ids)
        if (id <= 0xFFFF) out.push_back(static_cast<uint16_t>(id));
        else dropped++;
    return {out, dropped};
}
