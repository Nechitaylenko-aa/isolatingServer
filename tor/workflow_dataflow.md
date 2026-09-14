# Workflow / Dataflow — ServerD

Дополняет `architecture.mdj` (StarUML). Sequence-диаграммы StarUML вручную в JSON надёжно не собрать (хрупкая схема view/interaction) — поэтому здесь Mermaid: это тоже нотация UML sequence/activity, рендерится сразу, при желании воссоздаётся в StarUML вручную по этой же схеме.

Как открыть `architecture.mdj`: File → Open Project в StarUML. Диаграмм внутри нет намеренно (не рискнул битой геометрией view) — весь класс/пакет/связи уже в Model Explorer; чтобы получить визуальный Class Diagram, ПКМ на пакете (`Group1_Infrastructure` и т.д.) → Add Diagram → Class Diagram → перетащить нужные классы из Model Explorer на холст → ПКМ на холсте → Layout Diagram (автоматическая расстановка).

---

## 1. Workflow — полный цикл запроса `EPT_EQUIP_REQ`

```mermaid
sequenceDiagram
    participant Client
    participant CAcceptor
    participant CWorker
    participant CRequestPipeline
    participant CActiveClientRegistry as ActiveClients
    participant CAdmissionControl as Admission
    participant CEquipQueryDispatcher as Dispatcher
    participant CDBThread as DBThread
    participant CThreadPool as MatchPool
    participant IEquipMatchHandler as Handler
    participant CMainCoordinator as Coordinator
    participant CWorkerInbox as Inbox

    Client->>CAcceptor: TCP connect
    CAcceptor->>CWorker: assignConnection(fd)  [round-robin]
    CWorker->>CWorker: readMessage(fd) [лимит 500кБ/200кБ]
    CWorker->>CRequestPipeline: process(rawData)
    CRequestPipeline->>CRequestPipeline: CHeaderValidator::validate() [magic/version/query_type/size/length/encryption/isActive-ro/timeCheck/signature]
    CRequestPipeline->>CRequestPipeline: CPacketParser::parse() -> vector<SComponentReq>
    CRequestPipeline-->>CWorker: SPipelineResult{action=PROCEED, header, payload, wasEncrypted}

    CWorker->>ActiveClients: tryAdmit(id_client, MAX_ACTIVE_CLIENTS)
    ActiveClients-->>CWorker: true

    CWorker->>Admission: isAcceptingConnections()
    Admission-->>CWorker: true  [DB healthy - критическая правка №1]

    CWorker->>Dispatcher: dispatch(components, onComplete)
    Dispatcher->>DBThread: executeEquipQuery(uniqueTypeKeys, cb)
    Note over DBThread: единственный поток доступа к CDatabaseModel
    DBThread-->>Dispatcher: cb(vector<SEquipRow>, DATABSE_OK)
    Dispatcher->>Dispatcher: groupEquipRowsByType(rows)

    loop на каждый SComponentReq
        Dispatcher->>MatchPool: submit(matchTask)
        MatchPool->>Handler: match(equipPool, request)
        Handler-->>MatchPool: vector<SEquipRespItem>
        MatchPool->>Dispatcher: finishOne(ctx)
    end

    Dispatcher-->>Coordinator: publish(SPendingResult{worker_id, request_id, bytes})
    Note over Dispatcher: bytes уже сериализованы CPacketSerializer::serializeEquipReqResponse внутри onComplete

    loop раз в REDISTRIBUTE_PERIOD_MS (CQueue)
        Coordinator->>Coordinator: redistribute() [из общего буфера в inbox воркера по worker_id]
        Coordinator->>Inbox: push_back(result)
    end

    loop раз в WORKER_INBOX_POLL_MS (event-loop воркера)
        CWorker->>Inbox: pollInbox()
        Inbox-->>CWorker: vector<SPendingResult>
    end

    CWorker->>CWorker: m_pendingByRequestId.find(request_id) -> ctx
    CWorker->>ActiveClients: release(id_client)
    CWorker->>Client: write(responseBytes) + close()
```

### Ветки отказа (тот же вход, другой путь)

```mermaid
sequenceDiagram
    participant Client
    participant CWorker
    participant CRequestPipeline
    participant ActiveClients as CActiveClientRegistry
    participant Admission as CAdmissionControl

    Client->>CWorker: rawData (битый magic / просроченный timestamp / и т.п.)
    CWorker->>CRequestPipeline: process(rawData)
    alt DISCONNECT (magic/размер/decrypt fail/id_client active/timeCheck fail)
        CRequestPipeline-->>CWorker: action=DISCONNECT
        CWorker->>Client: close() [без ответа]
    else RESPOND (version/query_type/length/encryption-flag/signature)
        CRequestPipeline-->>CWorker: action=RESPOND, responseBytes
        CWorker->>Client: write(responseBytes) + close()
    else PROCEED, но лимит клиентов исчерпан
        CWorker->>ActiveClients: tryAdmit(...)
        ActiveClients-->>CWorker: false
        CWorker->>Client: close() [без ответа]
    else PROCEED, но БД недоступна (критическая правка №1)
        CWorker->>Admission: isAcceptingConnections()
        Admission-->>CWorker: false
        CWorker->>Client: write(serializeError(ERS_DATABASE_QUERY_ERROR)) + close()
        Note over CWorker: CDBThread/Dispatcher НЕ вызываются вообще - нет риска зависания
    end
```

---

## 2. Dataflow — трансформация данных `EPT_EQUIP_REQ` (запрос → ответ)

```mermaid
flowchart TD
    A["сырые байты сокета\n[flag][SClientPacket][payload]"] -->|CPacketParser::parseEquipReq| B["vector&lt;SComponentReq&gt;\n(id_component, natureType, componentType, parameters)"]
    B -->|dedup по natureType+componentType| C["vector&lt;SEquipTypeKey&gt;"]
    C -->|CDBThread::executeEquipQuery| D["vector&lt;SEquipRow&gt;\n(плоские строки: 1 строка = 1 поле шаблона)"]
    D -->|groupEquipRowsByType\nгруппировка natureType→componentType→idEquip| E["map&lt;SEquipTypeKey, vector&lt;SEquipGrouped&gt;&gt;"]
    B --> F{для каждого SComponentReq}
    E --> F
    F -->|CEquipHandlerRegistry::find по SEquipTypeKey| G["IEquipMatchHandler::match(pool, request)"]
    G -->|алгоритм пользователя| H["vector&lt;SEquipRespItem&gt;\n(SResponseEquip + params)"]
    H -->|собрать в SEquipReqRespBlock по component_id| I["vector&lt;SEquipReqRespBlock&gt;"]
    I -->|CPacketSerializer::serializeEquipReqResponse| J["готовые байты ответа\n[flag][SClientPacket][SReqHeader+SResponseEquip+floats]*"]
    J -->|SPendingResult| K["CMainCoordinator -> CWorkerInbox -> CWorker"]
    K --> L["write(fd) + close()"]

    style A fill:#f5f5f5
    style D fill:#f5f5f5
    style J fill:#f5f5f5
    style L fill:#f5f5f5
```

### Dataflow — прямые запросы (без диспетчера подбора)

```mermaid
flowchart LR
    subgraph EPT_EQIP_ID
        A1["vector&lt;uint16_t&gt; ids"] --> B1["CDBThread::executeEquipIdQuery"]
        B1 --> C1["vector&lt;SEquipRow&gt;"]
        C1 -->|groupEquipRowsByType, flatten по всем ключам| D1["vector&lt;SEquipGrouped&gt;"]
        D1 -->|"manufacturer name - ГЭП, см. критич. правку №3"| E1["vector&lt;SEquipProxyRespItem&gt;"]
        E1 --> F1["serializeEquipIdResponse"]
    end
    subgraph EPT_ORGS_REQ
        A2["vector&lt;uint16_t&gt; types"] --> B2["CDBThread::executeOrgsByType"] --> C2["vector&lt;SOrgProxy&gt;"] --> D2["serializeOrgsResponse"]
    end
    subgraph EPT_ORGS_ID
        A3["vector&lt;uint32_t&gt; ids"] -->|"filterOrgIdsToUint16\n(правка §0.1 ТЗ-3, отброс >65535)"| B3["vector&lt;uint16_t&gt;"]
        B3 --> C3["CDBThread::executeOrgByID"] --> D3["vector&lt;SOrgProxy&gt;"] --> E3["serializeOrgsResponse"]
    end
```

---

## 3. Карта потоков (thread map) — для дебага гонок

```mermaid
flowchart TD
    subgraph "Acceptor-поток (1)"
        ACC["CAcceptor::run()\naccept() loop"]
    end
    subgraph "Worker-потоки (N, по числу ядер)"
        W0["CWorker#0::run()"]
        W1["CWorker#1::run()"]
        WN["CWorker#N::run()"]
    end
    subgraph "DB-поток (1)"
        DB["CDBThread::threadLoop()\nFIFO очередь, healthCheckTick раз в 30с"]
    end
    subgraph "CThreadPool (M потоков)"
        P0["pool thread"]
        P1["pool thread"]
    end
    subgraph "CQueue-задачи (выполняются на CThreadPool)"
        RQ["CMainCoordinator::redistribute()\nраз в 5мс"]
        AQ["CAdmissionControl::checkHealth()\nраз в 1с"]
    end

    ACC -->|assignConnection fd, thread-safe queue| W0
    ACC --> W1
    ACC --> WN

    W0 -->|executeXxx callback| DB
    W1 --> DB
    DB -->|publish SPendingResult| RQ
    RQ -->|push в CWorkerInbox воркера| W0
    RQ --> W1

    W0 -->|dispatch equip| P0
    P0 -->|handler->match| P0
    P0 -->|finishOne -> publish| RQ

    AQ -->|читает isHealthy атомарно| DB
    W0 -->|читает isAcceptingConnections атомарно| AQ

    style DB fill:#ffe0e0
    style ACC fill:#e0f0ff
```

Единственные объекты с состоянием, к которым обращаются **несколько** потоков одновременно (точки, где искать гонки при дебаге): `CActiveClientRegistry` (мьютекс), `CTimeCheckRegistry` (мьютекс), `CWorkerInbox` (мьютекс, на воркер), `CMainCoordinator::m_results` (мьютекс), `CDBThread::m_queue` (мьютекс), `CThreadPool` internal queue (мьютекс), плюс три atomic-флага без мьютекса: `CDBThread::m_healthy`, `CAdmissionControl::m_dbHealthy`, `CWorker::m_nextRequestId`.
