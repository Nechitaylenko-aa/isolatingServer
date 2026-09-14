//
// Created by artem on 14.08.26.
//

#ifndef APP_SERVERD_CWORKER_H
#define APP_SERVERD_CWORKER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "worker_inbox.h"
#include "../protocol//request_pipeline.h"      // CRequestPipeline (Группа 2)
#include "../protocol/packet_serializer.h"      // CPacketSerializer (Группа 2)
#include "CActiveClientRegistry.h"
#include "../admission/admission_control.h"       // CAdmissionControl (Группа 5)
#include "../database-layer/db_thread.h"                 // CDBThread (Группа 3)
#include "../equip-match/equip_query_dispatcher.h"     // CEquipQueryDispatcher (Группа 4)
#include "../logger-common/wire_types.h"
#include "../logger-common/limits.h"                    // MAX_ACTIVE_CLIENTS

class CMainCoordinator;

class CWorker
{
public:
    CWorker(uint32_t workerId,
            CRequestPipeline&        pipeline,
            CPacketSerializer&        serializer,
            CActiveClientRegistry&     activeClients,
            CAdmissionControl&          admission,
            CDBThread&                    dbThread,
            CEquipQueryDispatcher&          equipDispatcher);

    void set_coordinator(CMainCoordinator * coordinator) noexcept { m_coordinator = coordinator; }
    void run();   // event-loop потока
    void stop();

    void assignConnection(int fd); // вызывается из CAcceptor, потокобезопасно

    CWorkerInbox& inbox() { return m_inbox; }
    uint32_t      id() const noexcept { return m_workerId; }
    size_t        activeConnections() const noexcept { return m_activeConnCount.load(); }

private:
    struct SConnectionCtx
    {
        int       fd{-1};
        uint16_t  id_client{0};
        bool      admitted{false};
    };

    void onNewConnection(int fd);
    void onRequestReady(SConnectionCtx& ctx, std::vector<uint8_t> rawData);
    void dispatchToBackend(SConnectionCtx& ctx, const SClientPacket& header,
                           TParsedPayload payload, bool wasEncrypted);
    void pollInbox();

    void writeAndClose(SConnectionCtx& ctx, std::vector<uint8_t> responseBytes);
    void closeConnection(SConnectionCtx& ctx);

    std::vector<SEquipProxyRespItem> buildEquipProxyItems(const std::vector<SEquipRow>& rows);
    static std::pair<std::vector<uint16_t>, size_t> filterOrgIdsToUint16(const std::vector<uint32_t>& ids);

    std::optional<std::vector<uint8_t>> readMessage(int fd); // применяет лимиты 500кБ/200кБ

    uint32_t                m_workerId;
    CRequestPipeline&        m_pipeline;
    CPacketSerializer&        m_serializer;
    CActiveClientRegistry&     m_activeClients;
    CAdmissionControl&          m_admission;
    CDBThread&                    m_dbThread;
    CEquipQueryDispatcher&          m_equipDispatcher;
    CMainCoordinator*                m_coordinator;

    CWorkerInbox               m_inbox;
    std::atomic<uint32_t>       m_nextRequestId{0};
    std::atomic<size_t>          m_activeConnCount{0};

    std::unordered_map<uint32_t, SConnectionCtx> m_pendingByRequestId;

    std::mutex           m_newConnMutex;
    std::vector<int>     m_newConnFds;

    std::atomic<bool>    m_running{false};

    static constexpr uint32_t WORKER_INBOX_POLL_MS = 5;
};


#endif //APP_SERVERD_CWORKER_H
