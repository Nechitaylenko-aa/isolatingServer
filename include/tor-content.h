//
// Created by artem on 8/8/24.
//

#ifndef NYM_PROJECT_TOR_CONTENT_H
#define NYM_PROJECT_TOR_CONTENT_H

#include "CParameter.h"


/// Источник питьевой воды
enum DRINK_WATER_SOURCE
{
    DWS_WELL, DWS_IMPORTED, DWS_NATURE_RESERVE, DWS_UNDEF, DWS_COUNT
};

/// drink water treatment plant data -----------------------------------------------------------------------------------
struct s_drink_water
{
    DRINK_WATER_SOURCE  water_source{DWS_UNDEF};    //!< источник исходной воды
    CParameter   drink_water_source{E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ITEMS};
    CParameter   time_clean_water_reserve_hour{EMU_TIME, EMUTIM::emit_hour};    //!< больше 0 если требуется запас очищенной воды на срок в часах
    CParameter   time_chemical_warehouse_mon{EMU_TIME, EMUTIM::emit_hour};      //!< больше нуля если требуется склад реагентов на срок в месяцах
    CParameter   is_waste_chanel{EMU_AMOUNT, EMUAMO::EA_ITEMS, 0};         //!< предусмотрена ли канализация
    CParameter   in_temper{E_MEASURE_UNITS::EMU_TEMPERATURE, EMUTEM::emt_celsius};       //!< температура воды на входе, град Ц
    CParameter   in_pressure{EMU_PRESSURE, EMUPRE::emp_pascale, 0, ESP_MEGA};         //!< давление воды на входе, МПа
    CParameter   consumption_in_max_hour{EMU_CONSUMPTION, EMUCONS::emcs_meter_hour};                 //!< максимальный часовой приток, м3/ч
    CParameter   consumption_in_day{EMU_CONSUMPTION, EMUCONS::emcs_meter_day};                       //!< суточный приток м3/сут
    CParameter   consumption_out_max_hour{EMU_CONSUMPTION, EMUCONS::emcs_meter_hour};                //!< Максимальная часовая потребность, м3/ч
    CParameter   out_press{EMU_PRESSURE, EMUPRE::emp_pascale, 0, ESP_MEGA};           //!< требуемое давление на выходе, МПа
    CParameter   amount_inputs_{EMU_AMOUNT, EMUAMO::EA_ITEMS}; //!< amount of inputs
    CParameter   amount_outputs{EMU_AMOUNT, EMUAMO::EA_ITEMS}; //!< amount of inputs
};

#endif //NYM_PROJECT_TOR_CONTENT_H
