# ТЗ — Группа 2: Протокольный слой (валидация, парсинг, сериализация)

Статус: вторая группа в порядке реализации. Зависит **только** от Группы 1 (`logger/`, `protocol/wire_types.h`). От Группы 3 (БД), Группы 4 (подбор оборудования) и Группы 6 (сеть) — не зависит: там, где протокольному слою логически нужны данные/решения из более поздних групп, определяется **интерфейс-контракт**, который эта группа потребляет как абстракцию, а более поздняя группа — реализует.

Собирается и тестируется полностью изолированно: юнит-тесты подают на вход байтовые буферы (`std::vector<uint8_t>` / `uint8_t*`+`size_t`), никакой сети и БД не требуется.

Источники: `protocol.md`, `server.md` (раздел "Инвалидация заголовка"), плюс уже реализованные `logger/logger.h`, `logger/log_level.h`, `protocol/wire_types.h`.

---

## 0. Состав группы

```
protocol/
  limits.h                 — константы протокола (переиспользуются из Группы 1, см. §0.1)
  parsed_types.h            — логические (не wire) типы: результат парсинга
  packet_cipher.h            — интерфейс шифрования + заглушка (задел на будущее)
  signature_verifier.h        — интерфейс проверки подписи + заглушка
  active_client_registry.h    — интерфейс "клиент уже активен" (реализуется в Группе 6)
  time_check_registry.h/.cpp   — CTimeCheckRegistry (полностью самостоятельный, см. §4)
  header_validator.h/.cpp      — CHeaderValidator
  packet_parser.h/.cpp          — CPacketParser
  packet_serializer.h/.cpp       — CPacketSerializer
  request_pipeline.h/.cpp        — CRequestPipeline (фасад: валидация+парсинг за один вызов)
```

### 0.1 `limits.h` — контракт (если уже создан в Группе 1 — используется как есть, ничего не меняем)

```cpp
constexpr uint32_t MAX_INCOMING_ENCRYPTED_BYTES   = 500 * 1024;
constexpr uint32_t MAX_INCOMING_UNENCRYPTED_BYTES = 200 * 1024;
constexpr uint32_t PROTOCOL_MAGIC                 = 0xDEADBEEF; // uint32_t, не uint16_t — см. server.md "magic = 0xDEADBEEF"
constexpr uint16_t PROTOCOL_SUPPORTED_VERSION      = 1;
constexpr size_t   MAX_ACTIVE_CLIENTS              = 50;   // используется Группой 6
constexpr size_t   TIME_CHECK_REGISTRY_CAPACITY    = 500;  // используется §4 этой группы
```
Примечание: в ТЗ Группы 1 константа магического числа была по ошибке типизирована как `uint16_t PROTOCOL_MAGIC`, но `SClientPacket::magic` — это `uint32_t` (см. `wire_types.h`). В этой группе используется исправленный тип `uint32_t`. Если файл `limits.h` уже физически создан с `uint16_t` — поправить при реализации, это не смена контракта, а исправление опечатки.

---

## 1. Логические типы результата парсинга (`parsed_types.h`)

Это **не** wire-структуры — они не сериализуются напрямую, это удобная форма для передачи в бизнес-логику (Группы 3/4) и для тестов этой группы.

```cpp
#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include "wire_types.h"

// Логическое представление одного компонента из EPT_EQUIP_REQ
// (появляется после десериализации SEquipRequestData + чтения его float'ов)
struct SComponentReq
{
    uint16_t                 id_component{0};
    uint16_t                 componentType{0};
    NCore::EComponentTypes   natureType{NCore::EComponentTypes::ect_water};
    std::vector<float>       parameters;
};

struct SEquipIdRequest { std::vector<uint16_t> ids; };    // EPT_EQIP_ID
struct SOrgTypeRequest { std::vector<uint16_t> types; };   // EPT_ORGS_REQ
struct SOrgIdRequest   { std::vector<uint32_t> ids; };     // EPT_ORGS_ID

using TParsedPayload = std::variant<
    std::vector<SComponentReq>, // EPT_EQUIP_REQ
    SEquipIdRequest,             // EPT_EQIP_ID
    SOrgTypeRequest,              // EPT_ORGS_REQ
    SOrgIdRequest                 // EPT_ORGS_ID
>;
```

`SComponentReq` дословно совпадает по смыслу со структурой из `database.md` — Группа 3 переиспользует именно этот тип (не заводит свой), инклудит `protocol/parsed_types.h`.

**Правило дропа**: блок с `param_amount == 0` **не** попадает в результирующий `std::vector<SComponentReq>` (см. `protocol.md`: "Если param_amount=0 компонент отбрасывается из запроса клиента"), но байты (сам заголовок блока, 0 float'ов) обязаны быть корректно потреблены парсером, чтобы не сбить смещение для следующего блока.

---

## 2. Интерфейсы-контракты для будущих групп

### 2.1 `packet_cipher.h` — шифрование (реальная реализация — вне ТЗ, см. `protocol.md`: "при разработке шифрования не будет")

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

class IPacketCipher
{
public:
    virtual ~IPacketCipher() = default;

    // true, если подключена реальная реализация (сейчас — всегда false, см. CNullPacketCipher)
    virtual bool isAvailable() const noexcept = 0;

    // Расшифровка. Возвращает nullopt при ошибке/недоступности — источник ERS_ENCRYPTION_ERROR
    // либо разрыва соединения без ответа, см. §3.
    virtual std::optional<std::vector<uint8_t>> decrypt(const uint8_t* data, size_t len) const = 0;

    // Шифрование ответа. Вызывается только когда isAvailable() == true.
    virtual std::vector<uint8_t> encrypt(const uint8_t* data, size_t len) const = 0;
};

// Заглушка на время разработки — шифрования нет вообще.
class CNullPacketCipher final : public IPacketCipher
{
public:
    bool isAvailable() const noexcept override { return false; }
    std::optional<std::vector<uint8_t>> decrypt(const uint8_t*, size_t) const override { return std::nullopt; }
    std::vector<uint8_t> encrypt(const uint8_t*, size_t) const override
    {
        // не должен вызываться, пока isAvailable() == false — вызывающая сторона обязана это проверять
        throw std::logic_error("CNullPacketCipher::encrypt called while isAvailable() == false");
    }
};
```

Интеграция реального шифрования (`octo-encryption-cpp`) в будущем — это просто ещё одна реализация `IPacketCipher`, подставляемая в `CRequestPipeline`/`CPacketSerializer` без изменения их кода.

### 2.2 `signature_verifier.h` — проверка подписи (заглушка по прямому указанию `protocol.md`)

```cpp
#pragma once
#include "wire_types.h"

class ISignatureVerifier
{
public:
    virtual ~ISignatureVerifier() = default;
    // messageBytes — SClientPacket+payload (незашифрованные, без SSignature и без самой подписи)
    virtual bool verify(const uint8_t* messageBytes, size_t messageLen,
                          const SSignature& sig, const uint8_t* sigBytes, size_t sigLen,
                          ESignatureKeyType keyType) const = 0;
};

// "Подпись если есть, игнорируется. Она на будущее, нужна заглушка" — protocol.md
class CNullSignatureVerifier final : public ISignatureVerifier
{
public:
    bool verify(const uint8_t*, size_t, const SSignature&, const uint8_t*, size_t,
                 ESignatureKeyType) const override { return true; }
};
```

### 2.3 `active_client_registry.h` — "id_client уже активен" (полноценная реализация — Группа 6)

Эта проверка требует знания о **текущих** живых соединениях по всем воркерам — данных, которых в протокольном слое нет и не должно быть. Группа 2 только определяет интерфейс и тестовую заглушку.

```cpp
#pragma once
#include <cstdint>

class IActiveClientRegistry
{
public:
    virtual ~IActiveClientRegistry() = default;
    virtual bool isActive(uint16_t id_client) const = 0;
};

// Для юнит-тестов Группы 2 — считает, что активных клиентов никогда нет.
class CAlwaysInactiveRegistry final : public IActiveClientRegistry
{
public:
    bool isActive(uint16_t) const override { return false; }
};
```

Группа 6 реализует настоящий `CWorkerPoolActiveClientRegistry` (или аналог) поверх реального реестра соединений и подставит его в `CRequestPipeline` вместо заглушки — без изменения кода Группы 2.

---

## 3. `CHeaderValidator` — порядок проверок (дословно по `server.md`, с уточнениями)

```cpp
#pragma once
#include <cstdint>
#include "wire_types.h"
#include "active_client_registry.h"
#include "time_check_registry.h"
#include "signature_verifier.h"

enum class EValidationOutcome : uint8_t
{
    OK,                 // всё прошло, можно парсить payload
    RESPOND_WITH_ERROR,  // нужно отправить [flag][SClientPacket{status=...}] и закрыть соединение
    DROP_SILENTLY         // разорвать соединение без ответа
};

struct SValidationResult
{
    EValidationOutcome outcome{EValidationOutcome::DROP_SILENTLY};
    EResponseStatus     status{ERS_INTERNAL_ERROR}; // валиден только при outcome == RESPOND_WITH_ERROR
};

class CHeaderValidator
{
public:
    CHeaderValidator(IActiveClientRegistry& activeClients,
                       CTimeCheckRegistry& timeCheck,
                       ISignatureVerifier& sigVerifier);

    // header       — уже извлечённый из (расшифрованного, если нужно) буфера SClientPacket
    // totalMsgSize — размер всего сообщения ПОСЛЕ снятия флага шифрования и ПОСЛЕ расшифровки
    //                (т.е. sizeof(SClientPacket) + length + хвост, если есть)
    // wasEncrypted — исходное значение encrypted_flag (для проверки шага 5)
    SValidationResult validate(const SClientPacket& header, size_t totalMsgSize, bool wasEncrypted) const;

private:
    IActiveClientRegistry& m_activeClients;
    CTimeCheckRegistry&      m_timeCheck;
    ISignatureVerifier&        m_sigVerifier;
};
```

### Порядок проверок (строго в этом порядке, первая же неудача — возврат)

| № | Проверка | При неудаче |
|---|---|---|
| 1 | `header.magic == PROTOCOL_MAGIC` | `RESPOND_WITH_ERROR`, `ERS_INVALID_MAGIC` |
| 2 | `header.version == PROTOCOL_SUPPORTED_VERSION` | `RESPOND_WITH_ERROR`, `ERS_UNSUPPORTED_VERSION` |
| 3 | `header.query_type < EPT_COUNT` | `RESPOND_WITH_ERROR`, `ERS_INVALID_QUERY_TYPE` |
| 4 | `totalMsgSize ≤ (wasEncrypted ? MAX_INCOMING_ENCRYPTED_BYTES : MAX_INCOMING_UNENCRYPTED_BYTES)` | `DROP_SILENTLY` |
| 5 | `totalMsgSize == sizeof(SClientPacket) + header.length + tailSize`, где `tailSize = header.is_signature ? (≥ sizeof(SSignature)) : 0` | `RESPOND_WITH_ERROR`, `ERS_INVALID_LENGTH` |
| 6 | `!(wasEncrypted && header.is_signature == 0)` | `RESPOND_WITH_ERROR`, `ERS_ENCRYPTION_ERROR` |
| 7 | `!m_activeClients.isActive(header.id_client)` | `DROP_SILENTLY` |
| 8 | `m_timeCheck.checkAndUpdate(header.id_client, header.timestamp)` | `DROP_SILENTLY` |
| 9 | если `header.is_signature == 1` — `m_sigVerifier.verify(...)` (в текущей заглушке всегда `true`) | `RESPOND_WITH_ERROR`, `ERS_INVALID_SIGNATURE` |

Пункты 3 (валидность `query_type`) и 4/5 (расчёт `totalMsgSize`/`tailSize`) в `server.md` явно не упорядочены — порядок зафиксирован здесь как решение этой группы: query_type проверяется сразу после version (по аналогии с остальными "структурными" проверками заголовка), проверки размера — до содержательных проверок (encryption/active/time/signature), т.к. без корректного размера сообщения нет смысла проверять содержимое.

**Открытое решение (прошу подтвердить или скорректировать):** размер тела подписи (`tailSize` при `is_signature==1`) не фиксируется конкретным значением, зависящим от `keyType` (RSA-1024/2048/4096) — проверяется только нижняя граница `≥ sizeof(SSignature)`, всё, что сверх, считается непрозрачным блоком подписи и не валидируется побайтово (согласуется с тем, что верификация подписи — заглушка). Если впоследствии понадобится проверять точную длину подписи по `keyType`, это добавится в `CHeaderValidator` без слома интерфейса.

---

## 4. `CTimeCheckRegistry` — самостоятельный модуль (полностью в этой группе)

```cpp
#pragma once
#include <cstdint>
#include <list>
#include <unordered_map>
#include <mutex>

class CTimeCheckRegistry
{
public:
    explicit CTimeCheckRegistry(size_t capacity = TIME_CHECK_REGISTRY_CAPACITY);

    // true  — timestamp принят, состояние обновлено
    // false — timestamp отклонён (см. политику ниже), состояние НЕ изменено
    bool checkAndUpdate(uint16_t id_client, uint64_t timestamp);

private:
    struct SEntry { uint64_t lastTimestamp; };

    mutable std::mutex                        m_mutex;
    std::unordered_map<uint16_t, SEntry>       m_entries;
    std::list<uint16_t>                        m_lruOrder; // front = самый свежий
    std::unordered_map<uint16_t, std::list<uint16_t>::iterator> m_lruIndex;
    size_t                                      m_capacity;
};
```

### Политика (открытое решение — реализуется по умолчанию так, требует вашего OK)

`protocol.md`/`server.md` не описывают точную политику сравнения — только "валидация по timestamp". Принято:

1. Если `id_client` встречается впервые — принимается всегда, заводится запись.
2. Если `id_client` уже известен — новый `timestamp` должен быть **строго больше** ранее сохранённого (`timestamp > lastTimestamp`). Иначе — отказ (защита от replay/повторной отправки старого пакета). Проверка "не из будущего" (допустимый скос часов) сейчас **не** делается — если понадобится, добавляется отдельным параметром без слома интерфейса.
3. Хранилище ограничено `capacity` (по умолчанию 500, см. `server.md`: "клиентов ≤ 500"). При достижении лимита и приходе **нового** `id_client` — вытесняется наименее недавно использованная запись (LRU по времени последнего успешного обращения, а не по значению timestamp клиента).
4. Не переживает рестарт сервера (только in-memory), как и указано в `server.md`.
5. Полностью потокобезопасен (`std::mutex`) — вызывается из воркер-потоков параллельно.

### Критерии готовности
- Тест: первый пакет клиента — всегда `true`.
- Тест: убывающий/равный timestamp — `false`, состояние не меняется (повторный вызов с тем же старым значением — снова `false`).
- Тест: возрастающие timestamp подряд — все `true`.
- Тест LRU: заполнить `capacity` разными `id_client`, добавить ещё один новый — самый старый по LRU вытесняется, что проверяется через то, что его следующий (даже корректный, растущий) timestamp снова считается "первым визитом" (т.е. принимается как новый клиент).
- Конкурентный тест: N потоков одновременно дергают `checkAndUpdate` для разных `id_client` — нет гонок/крашей (проверяется TSAN/ASAN при наличии).

---

## 5. `CPacketParser`

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "wire_types.h"
#include "parsed_types.h"

struct SParseOutcome
{
    bool             ok{false};
    EResponseStatus  status{ERS_SUCCESS}; // валиден только при ok == false; в этой группе — всегда ERS_INVALID_LENGTH при ошибке
    TParsedPayload   payload;              // валиден только при ok == true
};

class CPacketParser
{
public:
    // payload     — указатель на начало payload (сразу после SClientPacket), длина == header.length
    // queryType   — header.query_type (уже провалидирован CHeaderValidator'ом на этапе > этого вызова)
    static SParseOutcome parse(EQueryType queryType, const uint8_t* payload, uint32_t payloadLen);

private:
    static SParseOutcome parseEquipReq(const uint8_t* p, uint32_t len);   // EPT_EQUIP_REQ
    static SParseOutcome parseEquipId(const uint8_t* p, uint32_t len);    // EPT_EQIP_ID
    static SParseOutcome parseOrgsReq(const uint8_t* p, uint32_t len);    // EPT_ORGS_REQ
    static SParseOutcome parseOrgsId(const uint8_t* p, uint32_t len);     // EPT_ORGS_ID
};
```

### 5.1 `EPT_EQUIP_REQ` — алгоритм

```
offset = 0
result = []
while offset < len:
    if len - offset < sizeof(SEquipRequestData): FAIL(ERS_INVALID_LENGTH)
    block = read<SEquipRequestData>(p + offset); offset += sizeof(SEquipRequestData)
    needed = block.param_amount * sizeof(float)
    if len - offset < needed: FAIL(ERS_INVALID_LENGTH)
    params = read_floats(p + offset, block.param_amount); offset += needed
    if block.param_amount > 0:
        result.push_back(SComponentReq{block.id_component, block.componentType, block.natureType, params})
    # param_amount == 0 -> блок потреблён, в result НЕ попадает
return OK(result)
```
После цикла `offset` обязан точно равняться `len` (уже гарантировано проверкой `CHeaderValidator` шаг 5 на уровне заголовка — но здесь дополнительно эта же арифметика используется, чтобы отловить рассинхронизацию внутри тела, если `length` в заголовке был "правильным" по сумме, но блоки внутри битые — тогда `while` завершится с `offset != len` в какой-то итерации раньше и упадёт в `FAIL` на недостатке байт для следующего блока, либо, в редком случае точного совпадения по байтам при испорченной структуре, тест на это добавляется отдельно, см. критерии готовности).

### 5.2 `EPT_EQIP_ID`

```
if len < 2: FAIL(ERS_INVALID_LENGTH)
requestAmount = read<uint16_t>(p); offset = 2
if len - offset != requestAmount * sizeof(uint16_t): FAIL(ERS_INVALID_LENGTH)
ids = read_uint16_array(p + offset, requestAmount)
return OK(SEquipIdRequest{ids})
```

### 5.3 `EPT_ORGS_REQ`

Аналогично 5.2, но семантика — типы организаций: `OK(SOrgTypeRequest{types})`.

### 5.4 `EPT_ORGS_ID`

```
if len < 2: FAIL(ERS_INVALID_LENGTH)
requestAmount = read<uint16_t>(p); offset = 2
if len - offset != requestAmount * sizeof(uint32_t): FAIL(ERS_INVALID_LENGTH)
ids = read_uint32_array(p + offset, requestAmount)
return OK(SOrgIdRequest{ids})
```

### Замечание по чтению из буфера
Порядок байт — нативный (x86/x64, FreeBSD/Linux — см. `protocol.md`), поэтому чтение полей выполняется прямым `std::memcpy` в POD-переменную нужного типа (без `ntohs`/`ntohl`). Каждое обращение к буферу обязано быть через явную проверку `offset + sizeof(T) <= len` **до** `memcpy`, без исключений — весь модуль работает без выбрасывания исключений на "плохих" входных данных (только `FAIL(...)`, возврат структуры с `ok=false`); `throw` допустим только на явных программных ошибках (`assert`/`std::logic_error` при нарушении инвариантов самого кода, не пользовательских данных).

### Критерии готовности
- Для каждого из 4 типов запроса: тест на корректный буфер → верный результат.
- Тест "обрезанный буфер" на каждой стадии (не хватает байт на заголовок блока / на float'ы / на id) → `ERS_INVALID_LENGTH`.
- `EPT_EQUIP_REQ`: тест с `param_amount=0` в одном из блоков → блок не в результате, но следующий блок распознан корректно.
- `EPT_EQUIP_REQ`: тест с 0 блоков (`len == 0`) → `OK(пустой вектор)`.
- Фаззинг-тест (опционально, если есть время): случайные байты произвольной длины на вход всех `parseXxx` — не должно быть падений/UB (только `FAIL` или `OK`), гоняется под ASAN/UBSAN.

---

## 6. `CPacketSerializer`

Строит **готовые к отправке байтовые буферы** ответа. Работает только с "строительными блоками" протокольного уровня — не знает ничего про БД/подбор оборудования; входные данные ему подготавливают более поздние группы.

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "wire_types.h"
#include "packet_cipher.h"

struct SEquipRespItem                    // одна единица оборудования в ответе на EPT_EQUIP_REQ / EPT_EQIP_ID
{
    SResponseEquip      meta;
    std::vector<float>  params;
};

struct SEquipReqRespBlock                 // один SReqHeader + список найденного оборудования под него
{
    SReqHeader                     header;
    std::vector<SEquipRespItem>    items;   // items.size() ДОЛЖЕН == header.response_amount
};

struct SEquipProxyRespItem                // для EPT_EQIP_ID (использует SEquipProxy вместо SResponseEquip)
{
    SEquipProxy          meta;
    std::vector<float>   params;
};

class CPacketSerializer
{
public:
    explicit CPacketSerializer(IPacketCipher& cipher);

    // Ответ с ошибкой: [flag][SClientPacket{query_type=req.query_type, length=0, status=status}]
    // encryptResponse — зеркалит encrypted_flag исходного запроса; ЕСЛИ cipher.isAvailable() == false
    //                   и encryptResponse == true — форсируется НЕзашифрованный ответ (см. §6.1)
    std::vector<uint8_t> serializeError(const SClientPacket& requestHeader,
                                          EResponseStatus status,
                                          bool encryptResponse) const;

    std::vector<uint8_t> serializeEquipReqResponse(const SClientPacket& requestHeader,
                                                       const std::vector<SEquipReqRespBlock>& blocks,
                                                       bool encryptResponse) const;

    std::vector<uint8_t> serializeEquipIdResponse(const SClientPacket& requestHeader,
                                                      const std::vector<SEquipProxyRespItem>& items,
                                                      bool encryptResponse) const;

    std::vector<uint8_t> serializeOrgsResponse(const SClientPacket& requestHeader,
                                                   const std::vector<SOrgProxy>& orgs,
                                                   bool encryptResponse) const; // EPT_ORGS_REQ и EPT_ORGS_ID — идентичный формат

    // Безопасно копирует src в fixed-size char буфер: обрезает по dstSize-1 и гарантирует '\0' в конце,
    // остаток заполняет нулями. При обрезке — WARN в CLogger (component="CPacketSerializer").
    static void copyFixedString(char* dst, size_t dstSize, const std::string& src);

private:
    IPacketCipher& m_cipher;

    SClientPacket buildResponseHeader(const SClientPacket& req, EResponseStatus status, uint32_t payloadLength) const;
    std::vector<uint8_t> finalize(const SClientPacket& respHeader, const std::vector<uint8_t>& payload, bool encryptResponse) const;
};
```

### 6.1 `buildResponseHeader`
```
resp.magic       = PROTOCOL_MAGIC
resp.version     = req.version
resp.query_type  = req.query_type
resp.id_client   = req.id_client
resp.length      = payloadLength
resp.timestamp   = <текущее unix-время сервера>
resp.keyType     = KT_NONE      // подпись ответа сейчас не реализуется (сервер не подписывает — заглушка)
resp.is_signature = 0
resp.status       = status
```

### 6.2 `finalize` (общая точка сборки байтов на выходе)
```
bytes = [ ] // сериализованный SClientPacket (memcpy, native order) + сериализованный payload
if encryptResponse && cipher.isAvailable():
    encrypted = cipher.encrypt(bytes.data(), bytes.size())
    return [0x01] + encrypted
else:
    if encryptResponse && !cipher.isAvailable():
        CLogger::instance().warn("CPacketSerializer",
            "запрошено шифрование ответа, но cipher недоступен - отправляется незашифрованный ответ");
    return [0x00] + bytes
```

**Открытое решение:** нет отдельного кода ошибки/статуса для "клиент прислал зашифрованный запрос, а зашифровать ответ не можем" — по факту это может произойти только пока шифрование не реализовано вообще (текущий этап разработки, `encrypted_flag` от клиента всегда `0x00`). Как только реальный `IPacketCipher` подключат, `isAvailable()` станет `true` и ситуация исчезнет сама собой. Считаю это приемлемым для текущего этапа, но фиксирую как решение, а не как забытый кейс.

### Критерии готовности
- Для каждого из 4 типов ответа — сериализация корректного набора данных, обратное чтение через `CPacketParser`-подобную ручную проверку в тесте (побайтовое сравнение смещений, а не полноценный "round-trip" — парсер ответов на клиентской стороне вне зоны этого ТЗ) даёт исходные значения.
- `serializeError`: проверка, что `length == 0` и после `SClientPacket` в буфере больше ничего нет.
- `copyFixedString`: тест обрезки длинной строки (проверить WARN в логе через инъекцию тестового sink'а/перехват, если `CLogger` это позволяет; иначе — проверка по факту отсутствия падения и корректности итоговых байт), тест короткой строки (нулевой "хвост" после `\0` заполнен нулями).
- Тест `encryptResponse=true` при `CNullPacketCipher` (недоступен) → в логе WARN, ответ фактически невалидированно уходит с `flag=0x00`.

---

## 7. `CRequestPipeline` — фасад для Группы 6

Единая точка входа: получает сырые байты соединения (уже прочитанные Группой 6 из сокета целиком — до этого места байты не режутся), возвращает либо "нужно ответить вот этим", либо "нужно молча закрыть соединение", либо "всё ок, вот распарсенные данные — передавай в бизнес-логику".

```cpp
#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include "wire_types.h"
#include "parsed_types.h"
#include "header_validator.h"
#include "packet_parser.h"
#include "packet_cipher.h"
#include "active_client_registry.h"
#include "time_check_registry.h"
#include "signature_verifier.h"

enum class EPipelineAction : uint8_t
{
    PROCEED,        // валидно, парсинг успешен -> header/payload валидны, передавать в бизнес-логику
    RESPOND,         // отправить responseBytes и закрыть соединение
    DISCONNECT        // закрыть соединение без ответа
};

struct SPipelineResult
{
    EPipelineAction        action{EPipelineAction::DISCONNECT};
    std::vector<uint8_t>  responseBytes;  // валиден только при action == RESPOND
    SClientPacket           header;          // валиден при action == PROCEED (и как источник для error-ответов выше по стеку)
    TParsedPayload           payload;          // валиден только при action == PROCEED
};

class CRequestPipeline
{
public:
    CRequestPipeline(IActiveClientRegistry& activeClients,
                       CTimeCheckRegistry& timeCheck,
                       IPacketCipher& cipher,
                       ISignatureVerifier& sigVerifier);

    // rawData/rawLen — ВЕСЬ буфер, полученный от сокета, включая первый байт encrypted_flag
    SPipelineResult process(const uint8_t* rawData, size_t rawLen);

private:
    IActiveClientRegistry& m_activeClients;
    CTimeCheckRegistry&      m_timeCheck;
    IPacketCipher&             m_cipher;
    ISignatureVerifier&          m_sigVerifier;

    CHeaderValidator             m_validator;
    CPacketSerializer             m_serializer; // используется ТОЛЬКО для сборки error-ответов внутри process()
};
```

### Алгоритм `process`

```
if rawLen < 1: DISCONNECT   // даже флага нет

flag = rawData[0]
wasEncrypted = (flag == 0x01)
# flag не из {0x00, 0x01} - протокол не описывает такой случай явно; трактуется как DISCONNECT
if flag != 0x00 && flag != 0x01: DISCONNECT

body = rawData + 1, bodyLen = rawLen - 1

if wasEncrypted:
    decrypted = cipher.decrypt(body, bodyLen)
    if !decrypted.has_value(): DISCONNECT   // нет валидного заголовка, отвечать нечем
    workBuf = *decrypted
else:
    workBuf = body (без копирования, просто указатель+длина)

if workBuf.size() < sizeof(SClientPacket): DISCONNECT   // даже на заголовок не хватает

header = memcpy_read<SClientPacket>(workBuf.data())

validation = m_validator.validate(header, workBuf.size(), wasEncrypted)

if validation.outcome == DROP_SILENTLY: DISCONNECT

if validation.outcome == RESPOND_WITH_ERROR:
    resp = m_serializer.serializeError(header, validation.status, wasEncrypted)
    return { RESPOND, resp, header, {} }

# validation.outcome == OK
payloadPtr = workBuf.data() + sizeof(SClientPacket)
parseResult = CPacketParser::parse(header.query_type, payloadPtr, header.length)

if !parseResult.ok:
    resp = m_serializer.serializeError(header, parseResult.status, wasEncrypted)
    return { RESPOND, resp, header, {} }

return { PROCEED, {}, header, parseResult.payload }
```

Это единственное место в Группе 2, где валидатор, парсер и сериализатор используются совместно — всё, что дальше (бизнес-логика по `payload`, финальный успешный ответ с данными) собирается в более поздних группах и вызывает `CPacketSerializer` напрямую (не через `CRequestPipeline`).

### Критерии готовности
- Полный набор интеграционных тестов "сырые байты → `SPipelineResult`" на все ветки: `DISCONNECT` (каждая причина отдельно), `RESPOND` с каждым кодом ошибки, `PROCEED` для каждого из 4 типов запроса.
- Тест на `wasEncrypted=true` при `CNullPacketCipher` → всегда `DISCONNECT` (поскольку `decrypt` всегда `nullopt`) — зафиксировать этим тестом текущее поведение явно, чтобы при подключении реального шифрования регрессия была заметна по упавшему тесту.

---

## 8. Что передаётся в Группу 3 / Группу 4 / Группу 6

- **Группе 3 (БД):** тип `SComponentReq` (`protocol/parsed_types.h`) как единственный источник входных параметров для сборки `SEquipTypeKey`; типы `SEquipIdRequest::ids`, `SOrgTypeRequest::types`, `SOrgIdRequest::ids` — как готовые списки для соответствующих `CDatabaseModel::executeXxx`.
- **Группе 4 (подбор оборудования):** `SComponentReq` как вход хендлеров, `SEquipRespItem`/`SEquipReqRespBlock` (`protocol/packet_serializer.h`) как ожидаемая форма результата, который дальше отдаётся в `CPacketSerializer::serializeEquipReqResponse`.
- **Группе 6 (сеть):** `CRequestPipeline` как единственная точка входа для сырых байт соединения; интерфейсы `IActiveClientRegistry` (обязана быть реализована реальным реестром активных клиентов) и `IPacketCipher`/`ISignatureVerifier` (пока — заглушки `CNullPacketCipher`/`CNullSignatureVerifier`, передаются как есть до появления реальных реализаций); `CPacketSerializer` — для сборки успешных ответов после получения результата от Группы 3/4.

---

## 9. Сводка открытых решений этой группы (просьба подтвердить)

1. Порядок проверок `query_type`/размера сообщения в `CHeaderValidator`, явно не заданный в `server.md` — зафиксирован в §3.
2. Длина хвоста подписи при `is_signature==1` — проверяется только нижняя граница (`≥ sizeof(SSignature)`), не привязана к `keyType`.
3. Политика `CTimeCheckRegistry`: строго возрастающий timestamp на клиента, без окна допустимого расхождения часов; LRU-вытеснение при заполнении на 500 записей.
4. Ошибка расшифровки (`cipher.decrypt() == nullopt`) → всегда `DISCONNECT` (не `RESPOND`), так как валидного заголовка для ответа ещё нет.
5. Если для зашифрованного запроса шифрование ответа недоступно (`cipher.isAvailable()==false`) — ответ уходит незашифрованным с WARN в лог, а не блокируется отдельным кодом ошибки.
6. Исправлен тип `PROTOCOL_MAGIC` на `uint32_t` (см. §0.1) — приведение в соответствие с `wire_types.h`.
