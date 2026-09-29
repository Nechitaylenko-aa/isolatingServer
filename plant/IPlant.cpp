#include "IPlant.h"
#include "water/CPlantDrinkWater.h"
#include "../../saver/include/IGeneralTor.h"

namespace NCore
{
    IPlant* IPlant::create(NCore::CCell* cell, IGeneralTor* tor)
    {
        // Пока только питьевая вода — первая. Остальные типы — заглушка через неё же,
        const auto type = cell->get_project_type();
        switch (type)
        {
            case NCore::pt_water_drink:
                return new CPlantDrinkWater(cell, tor);
            case NCore::pt_water_tech:
            case NCore::pt_water_waste:
            case NCore::pt_water_industrial:
            case NCore::pt_water_surface:
            case NCore::pt_water_recycle:
            case NCore::pt_water_dehydration_sludge:
            case NCore::pt_water_pump_station_II:
            case NCore::pt_water_well_pump_station:
            default:
                return nullptr;
        }
    }

    void IPlant::start_equipment_query()
    {
        NCore::COperatingBody *ob = m_general_tor->general_working_body();
        ob->set_test(true);

        for (auto &in: m_project_cell->get_base_inputs())
        {
            in->put_ob(ob, nullptr);
        }
        ob->set_test(false);

    }
}
