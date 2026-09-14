//
// Created by artem on 15.08.26.
//

#include "CWaterFilterLightHandler.h"
#include "CParameter.h"
#include <types.h>

std::vector<SEquipRespItem>
CWaterFilterLightHandler::match(const std::vector<SEquipGrouped> &equipPool, const SComponentReq &request)
{
    std::vector<SEquipRespItem> res;

    if (equipPool.empty())
        return res;

    /** @brief Тут следующая логика, от БД к нам приходят именно сюда все светлые фильтры из БД и собственно сам запрос
     * в СИ Самое веселое, что мы обязаны знать что именно и в каком порядке к нам сюда приходит, т.к. приходят голые
     * float в том порядке, что на клиенте положили. Т.о. в этой точке сырые float из запроса клиента и более менее
     * внятные данные из БД:
     * */

    for(auto & item : equipPool)
    {
        std::vector<CParameter> fields;

        // form fields from database representation
        for (auto &field : item.fields)
        {
            VSubtypes st = subtypesFromInt((E_MEASURE_UNITS)field.templFieldMeasureSetI, field.equipFieldMeasureI);
            CParameter param((E_MEASURE_UNITS)field.templFieldMeasureSetI, st, 0,
                             (EStandardPrefix)field.equipFieldPrefixI, field.templFieldNameI);
            param.set_value(field.equipFieldValueF);
            fields.push_back(param);
        }

        float Q_si = fields.at(0).si_value();
        float P_si = fields.at(1).si_value();

        SEquipRespItem resp_item;

        SResponseEquip response_equip;
        response_equip.equip_id = item.idEquip;
        memcpy(response_equip.name, item.equipName.data(), 29);
        response_equip.name[29] = '\0';
        response_equip.param_amount = item.fields.size();

        resp_item.meta = response_equip;

        for (auto &filed : item.fields)
        {
            resp_item.params.push_back(filed.equipFieldValueF);
        }

        res.push_back(resp_item);
    }

    return res;
}
