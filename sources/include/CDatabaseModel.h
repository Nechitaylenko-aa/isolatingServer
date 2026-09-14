#ifndef EQUIPQUERY_SERVER_CDATABASEMODEL_H
#define EQUIPQUERY_SERVER_CDATABASEMODEL_H

#include <ADatabaseModel.h>
#include "../logger-common/nc_component_types.h"
#include "../logger-common/wire_types.h"
struct SEquipRow
{
    uint8_t     natureType{0};
    uint16_t    componentEnum{0};
    uint16_t    idEquip{0};
    uint16_t    idTemplate{0};
    uint16_t    idTemplateField{0};
    std::string templFieldNameI;
    std::string equipName;
    uint16_t    id_manufacturer{0};
    uint16_t    templFieldMeasureSetI{0};
    uint16_t    equipFieldMeasureI{0};
    uint8_t     equipFieldPrefixI{0};
    float       equipFieldValueF{0.f};
};

/** это не wire-структура */
/*
struct SOrgProxy
{
    uint32_t id{0};
    char     name[30]{};
    uint16_t id_type{0};
    uint16_t id_ownership{0};
    uint16_t id_country{0};
    char     inn[12]{};
};*/

struct SEquipTypeKey
{
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water};
    uint16_t                componentType{0};

    bool operator==(const SEquipTypeKey& other) const
    {
        return natureType == other.natureType && componentType == other.componentType;
    }
};

// Определён в architecture.md (§4) как хэшер для SEquipTypeKey в
// unordered_map - используется и в этой группе (equip_row_grouping.h).
struct SEquipTypeKeyHash
{
    size_t operator()(const SEquipTypeKey& k) const noexcept
    {
        return (static_cast<size_t>(k.natureType) << 16) ^ k.componentType;
    }
};

enum EDatabaseError
{
    DATABSE_OK,
    DATABASE_CONNECTION_FAILED,
    DATABASE_QUERY_FAILED,
    DATABASE_TIMEOUT,
    DATABASE_INVALID_RESULT,
};


class CDatabaseModel : public ADatabaseModel
{
    using TQueryEquipParamCallback = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryEquipIdCallback = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryOrgsByType = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;
    using TQueryOrgsByID = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;

public:
    explicit CDatabaseModel(CAbstractConnection *connection);
    ~CDatabaseModel() override;

    int socket() const;
    bool  simpleTest(); // простая проверка, что БД жива и дееспособна

    void executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys, const TQueryEquipParamCallback& callback); // запрос подбора оборудования
    void executeEquipIdQuery(std::vector<uint16_t> idList, TQueryEquipIdCallback callback); // запрос оборудования по ID
    void executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback);         // запрос организаций по ID
    void executeOrgsByType(std::vector<uint16_t> typeList, TQueryOrgsByType callback);      // запрос организаций по типам

private:
    bool  prepareConnection();

    //TQueryEquipParamCallback  m_callbackEquipParam;
    //TQueryEquipIdCallback     m_cbEquipId;
    //TQueryOrgsByType          m_cbOrgsTypes;
    //TQueryOrgsByID            m_cbOrgsId;

};


#endif //EQUIPQUERY_SERVER_CDATABASEMODEL_H
