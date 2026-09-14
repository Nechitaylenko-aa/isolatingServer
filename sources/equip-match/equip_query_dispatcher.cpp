// equip/equip_query_dispatcher.cpp
#include "equip_query_dispatcher.h"

#include <exception>
#include <unordered_set>
//#include <utility>

#include "../logger-common/logger.h"
//#include "../include/CThreadPool.h"
//#include "../database-layer/db_thread.h"
//#include "../equip-match/equip_query_dispatcher.h"


CEquipQueryDispatcher::CEquipQueryDispatcher(CEquipHandlerRegistry& registry, CThreadPool& pool, CDBThread& dbThread)
    : m_registry(registry)
    , m_pool(pool)
    , m_dbThread(dbThread)
{
}

void CEquipQueryDispatcher::dispatch(std::vector<SComponentReq> components, TDispatchComplete onComplete)
{
    if (components.empty())
    {
        onComplete(ERS_SUCCESS, {});
        return;
    }

    std::vector<SEquipTypeKey> uniqueKeys;
    std::unordered_set<SEquipTypeKey, SEquipTypeKeyHash> seen;
    uniqueKeys.reserve(components.size());
    for (const auto& c : components)
    {
        SEquipTypeKey key{c.natureType, c.componentType};
        if (seen.insert(key).second)
        {
            uniqueKeys.push_back(key);
        }
    }

    m_dbThread.executeEquipQuery(
        std::move(uniqueKeys),
        [this, components = std::move(components), onComplete = std::move(onComplete)](
            const std::vector<SEquipRow>& rows, EDatabaseError error) mutable
        {
            onDbResult(rows, error, std::move(components), std::move(onComplete));
        });
}

void CEquipQueryDispatcher::onDbResult(const std::vector<SEquipRow>&     rows,
                                        EDatabaseError             error,
                                        std::vector<SComponentReq> components,
                                        TDispatchComplete          onComplete)
{
    if (error != DATABSE_OK)
    {
        CLogger::instance().warn("CEquipQueryDispatcher", "db query failed: error={}", static_cast<int>(error));
        onComplete(ERS_DATABASE_QUERY_ERROR, {});
        return;
    }

    auto grouped = groupEquipRowsByType(rows);

    auto ctx = std::make_shared<SDispatchContext>();
    ctx->blocks.resize(components.size());
    ctx->remaining.store(components.size(), std::memory_order_relaxed);
    ctx->onComplete = std::move(onComplete);

    for (size_t i = 0; i < components.size(); ++i)
    {
        const SComponentReq& comp = components[i];
        SEquipTypeKey key{comp.natureType, comp.componentType};
        IEquipMatchHandler* handler = m_registry.find(key);

        if (handler == nullptr)
        {
            CLogger::instance().warn("CEquipQueryDispatcher",
                                      "handler not found for natureType={} componentType={}",
                                      static_cast<int>(comp.natureType), comp.componentType);
            ctx->blocks[i] = SEquipReqRespBlock{ SReqHeader{ comp.id_component, 0 }, {} };
            finishOne(ctx);
            continue;
        }

        auto poolIt = grouped.find(key);
        std::vector<SEquipGrouped> pool = (poolIt != grouped.end()) ? poolIt->second : std::vector<SEquipGrouped>{};

        m_pool.submit(this,
            [ctx, i, comp, handler, pool = std::move(pool)]() mutable
            {
                std::vector<SEquipRespItem> items;
                try
                {
                    items = handler->match(pool, comp);
                }
                catch (const std::exception& e)
                {
                    CLogger::instance().error("CEquipQueryDispatcher",
                                               "handler threw for natureType={} componentType={}: {}",
                                               static_cast<int>(comp.natureType), comp.componentType, e.what());
                    items.clear();
                }
                catch (...)
                {
                    CLogger::instance().error(
                        "CEquipQueryDispatcher",
                        "handler threw non-std exception for natureType={} componentType={}",
                        static_cast<int>(comp.natureType), comp.componentType);
                    items.clear();
                }

                ctx->blocks[i] = SEquipReqRespBlock{
                    SReqHeader{ comp.id_component, static_cast<uint16_t>(items.size()) },
                    std::move(items)
                };
                finishOne(ctx);
            });
    }
}

void CEquipQueryDispatcher::finishOne(const std::shared_ptr<SDispatchContext>& ctx)
{
    if (ctx->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        // это был последний компонент - ровно один вызов onComplete
        ctx->onComplete(ERS_SUCCESS, std::move(ctx->blocks));
    }
}
