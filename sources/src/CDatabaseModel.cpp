
#include "../include/CDatabaseModel.h"
// Реализация будет похже
CDatabaseModel::CDatabaseModel(CAbstractConnection *connection)
    : ADatabaseModel("countries", connection)
{
    prepareConnection();
}

CDatabaseModel::~CDatabaseModel()
= default;

int CDatabaseModel::socket() const
{
    return m_connection->socket();
}

bool CDatabaseModel::simpleTest()
{
    if (!prepareConnection())
        return false;

    auto count = read(true);
    return count > 0;
}

void CDatabaseModel::executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys,
                                       const CDatabaseModel::TQueryEquipParamCallback& callback)
{
    if (!prepareConnection())
    {
        callback({}, EDatabaseError::DATABASE_CONNECTION_FAILED);
        return;
    }

    if (uniqueTypeKeys.empty())
    {
        callback({}, EDatabaseError::DATABSE_OK);
        return;
    }

    std::stringstream query;

    query << "WITH RECURSIVE ";
    query << "roots AS (";
    query << "    SELECT ";
    query << "        etm.id_tree_i AS root_node,";
    query << "        etm.nature_type_i AS nature_type_i,";
    query << "        etm.component_en_i AS component_en_i";
    query << "    FROM equip_tree_mapper etm";
    query << "    WHERE (etm.nature_type_i, etm.component_en_i) IN (";

    for (size_t i = 0; i < uniqueTypeKeys.size(); ++i)
    {
        auto & key = uniqueTypeKeys.at(i);
        if (i > 0) query << ", ";
        // natureType - enum int8_t, componentType - uint16_t; оба целочисленные,
        // подстановка напрямую безопасна (не строковые значения, экранирование не требуется)
        query << "(" << static_cast<int>(key.natureType) << ", " << key.componentType << ")";
    }

    query << ")";
    query << "),";
    query << "tree_nodes AS (";
    query << "    SELECT ";
    query << "        tt.id AS node_id,";
    query << "        tt.id_template_i AS id_template_i,";
    query << "        r.nature_type_i AS nature_type_i,";
    query << "        r.component_en_i AS component_en_i";
    query << "    FROM template_tree tt";
    query << "    JOIN roots r ON tt.id = r.root_node";
    query << "    UNION ALL";
    query << "    SELECT ";
    query << "        tt.id,";
    query << "        tt.id_template_i,";
    query << "        tn.nature_type_i,";
    query << "        tn.component_en_i";
    query << "    FROM template_tree tt";
    query << "    JOIN tree_nodes tn ON tt.parent_id = tn.node_id";
    query << "),";
    query << "templates AS (";
    query << "    SELECT DISTINCT";
    query << "        id_template_i,";
    query << "        nature_type_i,";
    query << "        component_en_i";
    query << "    FROM tree_nodes";
    query << "    WHERE id_template_i <> 0";
    query << ")";
    query << "SELECT ";
    query << "    t.nature_type_i AS natureType,";
    query << "    t.component_en_i AS componentEnum,";
    query << "    eq.id AS idEquip,";
    query << "    eq.id_template AS idTemplate,";
    query << "    etf.id AS idTemplateField,";
    query << "    efn.name_s AS templFieldNameI,";
    query << "    eq.sku_s AS equipName,";
    query << "    eq.manufacturer_id AS id_manufacturer,";
    query << "    etf.measure_set_i AS templFieldMeasureSetI,";
    query << "    ef.measure_i AS equipFieldMeasureI,";
    query << "    ef.prefix_i AS equipFieldPrefixI,";
    query << "    ef.value_f AS equipFieldValueF";
    query << " FROM templates t";
    query << " JOIN equipment eq ON eq.id_template = t.id_template_i AND eq.is_active = 1";
    query << " JOIN equip_fields ef ON ef.id_equip = eq.id";
    query << " JOIN equip_templ_fields etf ON etf.id = ef.id_template_field";
    query << " JOIN equip_field_names efn ON efn.id = etf.name_i";
    query << " ORDER BY t.nature_type_i, t.component_en_i, eq.id, ef.id;";

    Tstring sql = query.str();
    auto rows = execSQL_read(sql);

    if (rows == 0)
    {
        callback({}, EDatabaseError::DATABSE_OK);
        return;
    }

    std::vector<SEquipRow> result;
    result.reserve(rows);

    for (auto & row : *answer)
    {
        SEquipRow equipRow;

        equipRow.natureType         = static_cast<uint8_t>(row->at(0)->integer());
        equipRow.componentEnum      = static_cast<uint16_t>(row->at(1)->integer());
        equipRow.idEquip             = static_cast<uint16_t>(row->at(2)->integer());
        equipRow.idTemplate           = static_cast<uint16_t>(row->at(3)->integer());
        equipRow.idTemplateField       = static_cast<uint16_t>(row->at(4)->integer());
        equipRow.templFieldNameI        = row->at(5)->string();
        equipRow.equipName                = row->at(6)->string();
        equipRow.id_manufacturer           = static_cast<uint16_t>(row->at(7)->integer());
        equipRow.templFieldMeasureSetI      = static_cast<uint16_t>(row->at(8)->integer());
        equipRow.equipFieldMeasureI          = static_cast<uint16_t>(row->at(9)->integer());
        equipRow.equipFieldPrefixI            = static_cast<uint8_t>(row->at(10)->integer());
        equipRow.equipFieldValueF               = static_cast<float>(row->at(11)->decimal());

        result.push_back(equipRow);
    }

    callback(result, EDatabaseError::DATABSE_OK);
}

void CDatabaseModel::executeEquipIdQuery(std::vector<uint16_t> idList, CDatabaseModel::TQueryEquipIdCallback callback)
{

}

void CDatabaseModel::executeOrgByID(std::vector<uint16_t> idList, CDatabaseModel::TQueryOrgsByID callback)
{

}

void CDatabaseModel::executeOrgsByType(std::vector<uint16_t> typeList, CDatabaseModel::TQueryOrgsByType callback)
{

}

bool CDatabaseModel::prepareConnection()
{
    if (!m_connection->is_opened())
    {
        if (!m_connection->open())
            return false;
    }
    clear_all();
    return true;
}
