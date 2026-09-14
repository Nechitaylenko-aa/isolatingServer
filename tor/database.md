# База данных

> библиотека базы данных не потокобезопасна, поэтому общение с ней из одного потока. Отдельно (не в пуле) создать поток и в нем обращаться к БД



## структуры для работы c БД

```cpp
struct SEquipRow {
    uint8_t     natureType;
    uint16_t    componentEnum;    
    uint16_t    idEquip;
    uint16_t    idTemplate;
    uint16_t    idTemplateField;
    std::string templFieldNameI;
    std::string equipName;              // new
    uint16_t    id_manufacturer;        // new
    uint16_t    templFieldMeasureSetI;
    uint16_t    equipFieldMeasureI;
    uint8_t     equipFieldPrefixI;
    float       equipFieldValueF;
};

/** это не wire-структура */
struct SOrgProxy
{
    uint32_t id{0};
    char     name[30];
    uint16_t id_type{0};
    uint16_t id_ownership{0};
    uint16_t id_country{0};
    char     inn[12];
};

struct SEquipTypeKey {
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water};
    uint16_t componentType{0};
    bool operator==(const SEquipTypeKey& other) const
    {
        return natureType == other.natureType && componentType == other.componentType;
    }
};

enum EDatabaseError
{
    DATABSE_OK,
    DATABASE_CONNECTION_FAILED,
    DATABASE_QUERY_FAILED,
    DATABASE_TIMEOUT,
    DATABASE_INVALID_RESULT,
}
```

## интерфейс

```cpp
class CDatabaseModel : public ADatabaseModel
{
    using TQueryEquipParamCallback = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryEquipIdCallback = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryOrgsByType = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;
    using TQueryOrgsByID = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;
public:
    explicit CDatabaseModel(CAbstractConnection * connection);
    ~CDatabaseModel() override;
    
    int socket() const;
    bool  simpleTest() const; // простая проверка, что БД жива и дееспособна

    void executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys, TQueryEquipParamCallback callback); // запрос подбора оборудования
    void executeEquipIdQuery(std::vector<uint16_t> idList, TQueryEquipIdCallback callback); // запрос оборудования по ID
    void executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback);         // запрос организаций по ID  
    void executeOrgsByType(std::vector<uint16_t> typeList, TQueryOrgsByType callback);      // запрос организаций по типам

private:
    bool  prepareConnection();
    TQueryEquipParamCallback  m_callbackEquipParam;
};
```

В сервере должна быть заполнена структура:
```cpp

typedef struct ssl_conf {
    std::string key_file;
    std::string cert_file;
    std::string ca_file;
    std::string ciphers;
    bool is_enabled{false};
}SSslConf;

/**@brief to connect with MySQL/MariaDB servers */
typedef struct db_connection
{
    std::string host;
    std::string dbname;
    std::string user;
    std::string pass;
    int port{3306};
    SSslConf    ssl_config{};
    bool    is_data_encrypt_enabled{false};

    db_connection& operator=(const s_database_config &db_conf)
    {
        host = db_conf.server;
        port = db_conf.port;
        dbname = db_conf.name;
        user = db_conf.login;
        pass = db_conf.password;
        is_data_encrypt_enabled = db_conf.is_encrypted_txt_data; // not used
        ssl_config.ca_file = db_conf.ca_pem;  // not used
        ssl_config.key_file = db_conf.client_key_pem;  // not used
        ssl_config.cert_file = db_conf.client_pem; // not used
        //db_conn.ssl_config.ciphers = db_conf.cipher;  // not used
        ssl_config.is_enabled = db_conf.is_encrypted_txt_data; // not used - always false
        return *this;
    }

}SDBConnection;
```
для этого нужно:
```cpp
#include <db_types.h>

void setupDBconfig(SDBConnection & config)
{
// заполняем конфиг

}

// для создания самого соединения

static CAbstractConnection * createConnection(const SDBConnection & connect)
{
    return CAbstractConnection::createDatabaseInstance(E_DB_TYPE::EDT_MYSQL, &connect);
}

```

> Пока библиотека не оптимизирована под множественные соединения, поэтому сокет будет один

## Логика запроса подбора оборудования
```cpp
// логическая структура для работы в памяти. Эта структура появляется после десериализации SEquipRequestData и заполнения ее float (смотри описание протокола)
struct SComponentReq
{
    uint16_t id_component{0};        // идентификатор компонента на клиенте
    uint16_t componentType{0};       // тип компонента
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water};
    std::vector<float> parameters;
};
```
> для КАЖДОГО оборудования из запроса подбор происходит индивидуальный

Обобщенно: нам происходит от клиента набор компонентов с параметрами, а в ответ мы высылаем отобранное оборудование (привязанное к запросившему компоненту)

после получения запроса от клиента и его десериализации у нас есть вектор SComponentReq. В запросе может быть несколько одинакового оборудования, например два оасветлителных фильтра, но для каждого фильтра уникальные параметры подбора.
наша задача перед подачей запроса вычленить из вектора SComponentReq уникальные типы оборудования и составить вектор SEquipTypeKey, т.е. осветлительных фильтров два, но в вектор SEquipTypeKey попадет только один, без параметров
Это позволит нам одним запросом получить все оборудование подпадающее под нужное.

Оборудование из базы к нам попадает плоским вектором `std::vector<SEquipRow>` этот вектор будет иметь тройную группировку по:
1. natureType
2. componentEnum
3. idEquip

т.е. если оборудование имеет 5 полей, будет 5 строк SEquipRow

и так мы получили оборудование, в том числе из раздела `осветлительные фильтры` допустим 15 единиц фильтров (хотя может быть ниодного)

в запросе 2 фильтра и мы передаем в хандлер подбора фильтров:
1. подборку из этих 15 фильтров
2. SComponentReq

на выходе хандлера данные для сериализации в
```cpp
struct SResponseEquip
{
    uint16_t equip_id{0};
    char     name[30];
    uint16_t param_amount{0};
};
// Затем param_amount штук float
```

в этом хандлере производится отбор фильтра по параметрам, но хандлеры ДОЛЖНЫ быть индивидуальные для каждого типа оборудования, т.к.
порядок паметров и сам подбор уникальны для каждого типа компонента.

на текущий момент у нас есть такие компоненты:
```cpp
namespace NCore {

...
enum  E_WATER_COMPONENTS : int8_t
    {
        EWB_PROJECT_CELL = -1,
        EWB_WATER_FILTER_LIGHT = 0,
        EWB_WATER_CAPACITY,
        EWB_PUMP_STATION,
        EWB_WATER_ACCOUNT_NODE,
        EWB_COUNT
    };

    enum E_GAS_COMPONENTS : int8_t
    {
        EGC_PROJECT_CELL = -1,
        EGC_FILTER = 0,
        EGC_REDUCING_DEV,
        EGC_COMPRESSOR,
        EGC_COUNT
    };

    enum E_ELECTRIC_COMPONENTS : int8_t
    {
        EEC_POWER_SUPPLY,
        EEC_COUNT
    };
...    

}
```













