# Протокол обмена (полная версия)
## Общие положения

    порядок байт соответствует FreeBSD и Linux

    Все пакеты имеют бинарный формат.

    Используется побайтовая упаковка (#pragma pack(push, 1)).

    Первый байт любого сообщения (как запроса, так и ответа) — флаг шифрования:

        0x00 — данные не зашифрованы

        0x01 — данные зашифрованы (шифруется всё, что идёт после этого байта)

    Если данные зашифрованы, после расшифровки получается структура SClientPacket + далее payload + SSignature + подпись.
    
    В незашифрованном виде `SSignature + подпись` может не быть. В зашифрованном виде SSignature + подпись ДОЛЖНЫ быть
    
    Шифруется ВСЁ сообщение (естественно кроме 1-го байта), но при разработке шифрования не будет
    
    Шифрование производится с помощью https://github.com/ofiriluz/octo-encryption-cpp.git
    
    В запросе клиента status в заголовке игнорируется
    
    При десериализации строк char[XX] \0 уже внутри
    
    Входящие сообщения на сервер ограничены 500 кБ зашифрованные и 200 кБ незашифрованные
    
    timestamp -  Unix-эпоха
    
    TCP-соединение = ровно 1 запрос-ответ. Keep-alive не поддерживается
    
    

## Базовые структуры

### Флаг шифрования

```cpp
// Первый байт ВСЕГДА в каждом сообщении
uint8_t encrypted_flag; // 0x00 - открыто, 0x01 - зашифровано
```

### Типы запросов клиентов

```cpp
enum EQueryType : uint8_t
{
    EPT_EQUIP_REQ,  // запрос подбора оборудования по параметрам
    EPT_ORGS_REQ,   // запрос организаций по типам
    EPT_EQIP_ID,    // запрос конкретного оборудования по ID
    EPT_ORGS_ID,    // запрос конкретных организаций по ID
    EPT_COUNT
};
```

### Алгоритмы шифрования

```cpp
enum class ESignatureKeyType : uint8_t {
    KT_NONE,        // на время разработки но предусмотреть хранилище для ключей привязанных к id_client
    KT_RSA_1024,
    KT_RSA_2048,
    KT_RSA_4096,
    KT_COUNT
};
```

### Подпись (находится в конце пакета, если есть)

```cpp
struct SSignature
{
    uint32_t hash{0};    // хеш незашифрованного сообщения CRC-32 SClientPacket + payload
    uint32_t length{0};  // дубликат SClientPacket::length
};
```
На подпись SClientPacket + payload
Подпись если есть, игнорируется. Она на будущее, нужна заглушка

### Заголовок пакета

```cpp
#pragma pack(push, 1)
struct SClientPacket
{
    uint32_t    magic{0};
    uint16_t    version{0};
    EQueryType  query_type{EPT_COUNT};
    uint16_t    id_client{0}; // уникальный ID клиента
    uint32_t    length{0}; // длина payload в байтах ПОСЛЕ SClientPacket ДО SSignature (если есть)
    uint64_t    timestamp{0};
    ESignatureKeyType     keyType{0}; // ESignatureKeyType
    uint8_t     is_signature{0}; // 0/1 — есть ли SSignature+подпись в хвосте
    EResponseStatus status{ERS_INVALID_MAGIC};
};
#pragma pack(pop)
```


## Формирование пакета

    Без шифрования:

```text
    [0x00][SClientPacket][payload][SSignature(опционально)][подпись(опционально)]
```
    С шифрованием:
    
```text
    [0x01][зашифрованные данные: SClientPacket + payload + SSignature + подпись]
```

    Поле length:

        Указывает размер только payload (данных после SClientPacket).

        Не включает размер SSignature и самой подписи.

        length = строго размер всего payload.

    Подпись:

        Находится строго в конце пакета после SSignature.

        Наличие определяется флагом is_signature = 1.

        Подписывается незашифрованное сообщение.

        Размер самой подписи не фиксирован и зависит от алгоритма.

### 1. Запрос подбора оборудования по параметрам
Тип запроса: `EPT_EQUIP_REQ`
#### Запрос

```text
[encrypted_flag][SClientPacket][блок_1][блок_2]...[блок_N]
```

Где каждый блок:

```cpp
struct SEquipRequestData
{
    uint16_t id_component{0};        // идентификатор компонента на клиенте
    uint16_t componentType{0};       // тип компонента
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water}; //  natureType берется из внешних данных оттуда же и возмется размер (он uint8_t)
    uint16_t param_amount{0};        // количество параметров (float)
};
// Затем идут param_amount штук float
```

Важно: блоков может быть сколько угодно. Сервер читает param_amount и пропускает соответствующее число float, чтобы перейти к следующему блоку. Если param_amount=0 компонент отбрасывается из запроса клиента
#### Ответ

```text
[encrypted_flag][SClientPacket][SReqHeader_1][SReqHeader_2]...[SReqHeader_N]
```

Где каждый SReqHeader соответствует одному компоненту из запроса:

```cpp
struct SReqHeader
{
    uint16_t component_id{0};        // = id_component из запроса
    uint16_t response_amount{0};     // количество найденного оборудования
};
```

За каждым SReqHeader следуют пары:

```cpp
struct SResponseEquip
{
    uint16_t equip_id{0};
    char     name[30];
    uint16_t param_amount{0};
};
// Затем param_amount штук float
```

Структура ответа:

```text
[SClientPacket]
  [SReqHeader_1]
    [SResponseEquip_1][float...]
    [SResponseEquip_2][float...]
    ...
  [SReqHeader_2]
    [SResponseEquip_1][float...]
    ...
```

Количество SReqHeader = количеству компонентов в запросе.
Возможны случаи когда SReqHeader::response_amount=0 это когда оборуд. не найдено. За таким SReqHeader сразу следует следующий SReqHeader

### 2. Запрос конкретного оборудования по ID
Тип запроса: EPT_EQIP_ID
#### Запрос

```text
[encrypted_flag][SClientPacket][requestAmount][id_1][id_2]...[id_N]
```

Где:

    requestAmount (uint16_t) — количество ID оборудования

    Далее перечисляются ID (uint16_t) в количестве requestAmount

#### Ответ

```text
[encrypted_flag][SClientPacket][equipCount][SEquipProxy_1][float...][SEquipProxy_2][float...]...
```
Где:

    equipCount (uint16_t) — количество найденного оборудования

    Для каждого оборудования:

```cpp
struct SEquipProxy
{
    uint16_t id;
    uint16_t id_manufacturer;
    char     equip_name[30];
    char     manufacturer[30];
    uint16_t param_amount{0};
};
// Затем param_amount штук float
```
### 3. Запрос организаций по типам
Тип запроса: `EPT_ORGS_REQ`
#### Запрос

```text
[encrypted_flag][SClientPacket][requestAmount][type_1][type_2]...[type_N]
```

Где:

    requestAmount (uint16_t) — количество типов организаций

    Далее перечисляются id_type (uint16_t) в количестве requestAmount

#### Ответ

```text
[encrypted_flag][SClientPacket][orgCount][SOrgProxy_1][SOrgProxy_2]...
```

Где:

    orgCount (uint16_t) — количество найденных организаций

    Для каждой организации:

```cpp
struct SOrgProxy
{
    uint32_t id{0};
    char     name[30];
    uint16_t id_type{0};
    uint16_t id_ownership{0};
    uint16_t id_country{0};
    char     inn[12];
};
```

Примечание: в этом запросе нет параметров float — только uint16_t идентификаторы типов.
### 4. Запрос конкретных организаций по ID
Тип запроса: `EPT_ORGS_ID`
#### Запрос

```text
[encrypted_flag][SClientPacket][requestAmount][id_1][id_2]...[id_N]
```
Где:

    requestAmount (uint16_t) — количество ID организаций

    Далее перечисляются ID организаций (uint32_t) в количестве requestAmount

#### Ответ

```text
[encrypted_flag][SClientPacket][orgCount][SOrgProxy_1][SOrgProxy_2]...
```

Формат ответа полностью идентичен ответу на EPT_ORGS_REQ (структура SOrgProxy).
## Коды ошибок

В ответе на любой запрос сервер может вернуть код ошибки вместо данных:

```cpp
enum EResponseStatus : uint16_t
{
    ERS_SUCCESS = 0,          // успешный ответ
    ERS_INVALID_MAGIC,        // неверное магическое число
    ERS_UNSUPPORTED_VERSION,  // неподдерживаемая версия протокола
    ERS_INVALID_QUERY_TYPE,   // неизвестный тип запроса
    ERS_INVALID_LENGTH,       // несоответствие длины пакета
    ERS_INVALID_SIGNATURE,    // неверная подпись
    ERS_ENCRYPTION_ERROR,     // ошибка шифрования/дешифрования
    ERS_NOT_FOUND,            // данные не найдены
    ERS_INTERNAL_ERROR,       // внутренняя ошибка сервера
    ERS_DATABASE_QUERY_ERROR, // ошибка запроса БД
    ERS_COUNT
};
```

Формат ответа с ошибкой:

```text
[encrypted_flag][SClientPacket]
```

При этом:

    query_type = повторяется тип запроса

    length = 0

    payload тсутствует
