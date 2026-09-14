//
// Created by artem on 15.08.26.
//

#ifndef NYM_PROJECT_CWATERMEMBARNEOSMOS_H
#define NYM_PROJECT_CWATERMEMBARNEOSMOS_H

#include "../equip-match/equip_match_handler.h"

class CWaterMembarneOsmos : public IEquipMatchHandler
{
public:
    std::vector<SEquipRespItem> match(const std::vector<SEquipGrouped>& equipPool,
                                      const SComponentReq&              request) override;
};


#endif //NYM_PROJECT_CWATERMEMBARNEOSMOS_H
