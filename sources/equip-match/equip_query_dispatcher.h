// equip/equip_query_dispatcher.h
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "../include/CDatabaseModel.h"
#include "../include/CThreadPool.h"
#include "../database-layer/db_thread.h"
#include "../protocol/parsed_types.h"        // SComponentReq
#include "../protocol/packet_serializer.h"   // SEquipReqRespBlock, SEquipRespItem
#include "../logger-common/wire_types.h"          // EResponseStatus, SReqHeader
#include "../database-layer/equip_row_grouping.h"  // SEquipGrouped, groupEquipRowsByType
#include "equip_handler_registry.h"


using TDispatchComplete = std::function<void(EResponseStatus status, std::vector<SEquipReqRespBlock> blocks)>;

class CEquipQueryDispatcher
{
public:
    CEquipQueryDispatcher(CEquipHandlerRegistry& registry, CThreadPool& pool, CDBThread& dbThread);

    // components - результат парсинга EPT_EQUIP_REQ (Группа 2), блоки с
    //              param_amount=0 уже отфильтрованы вызывающей стороной.
    // onComplete - вызывается РОВНО ОДИН РАЗ: синхронно внутри dispatch()
    //              (components.empty()), на DB-потоке (немедленная ошибка БД)
    //              либо на потоке CThreadPool (последний завершившийся компонент).
    void dispatch(std::vector<SComponentReq> components, TDispatchComplete onComplete);

private:
    struct SDispatchContext
    {
        std::vector<SEquipReqRespBlock> blocks;
        std::atomic<size_t>             remaining{0};
        TDispatchComplete               onComplete;
    };

    void onDbResult(const std::vector<SEquipRow>&     rows,
                     EDatabaseError             error,
                     std::vector<SComponentReq> components,
                     TDispatchComplete          onComplete);

    static void finishOne(const std::shared_ptr<SDispatchContext>& ctx);

    CEquipHandlerRegistry& m_registry;
    CThreadPool&            m_pool;
    CDBThread&              m_dbThread;
};
