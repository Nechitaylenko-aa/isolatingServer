//
// Created by artem on 8/11/24.
//

#ifndef MEASURE_SYSD_TYPES_H
#define MEASURE_SYSD_TYPES_H

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <functional>

#ifndef WIN32
typedef  uint8_t BYTE;
#else
typedef unsigned char BYTE;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#endif

enum E_OPER
{
    EO_NONE,
    EO_PLUS,
    EO_MINUS,
    EO_MULTI,
    EO_DIV,
    EO_COUNT
};

enum EMUDIS
{
    emd_meter,
    emd_foot,
    emd_inch,
    emd_mile,
    emd_count
};

static float ADistCoeffs[EMUDIS::emd_count]
{
    1,
    0.3048,
    0.0254,
    1609.34
};

enum EMUVEL
{
    vel_meter_sec,
    vel_km_sec,
    vel_km_hour,
    vel_count
};

static float AVelocityCoeff[EMUVEL::vel_count]
{
    1,
    0.001,
    0.2777777778
};

enum EMUTEM
{
    emt_kelvin,
    emt_celsius,
    emt_fahrengeit,
    emt_count
};

static float ATemperCoeffs[emt_count]
{
    1,
    -273.15,
    0   // formula
};

enum EMUTIM
{
    emit_sec,
    emit_min,
    emit_hour,
    emit_day,
    emit_count
};

static float ATimeCoeffs[emit_count]
{
    1,
    60,
    3600,
    84600
};

enum EMUPRE
{
    emp_pascale,
    emp_bar,
    emp_atmosphere_phys,
    emp_atmosphere_tech,
    emp_mm_mercury,
    emp_mm_water,
    emp_psi,
    emp_kgs_mm_sq,
    emp_kgs_m_sq
};

enum EMUVOL
{
    emv_meter,
    emv_liter
};

enum EMUCONS
{
    emcs_meter_sec,
    emcs_meter_hour,
    emcs_meter_day,
    emcs_liter_sec,
    emcs_liter_hour
};

enum EMUMAS
{
    EMM_GRAM,
    EMM_TON,
    EMM_STONE,
    EMM_PUD
};

enum EMUCONC
{
    con_kg_meter_cube,
    con_mg_liter,
    con_gram_cubic_meter,
    con_mg_cubic_decimetr,
    con_mg_ml
};

enum EStandardPrefix : uint8_t
{
    ESP_NANO,
    ESP_MICRO,
    ESP_MILLI,
    ESP_SANTI,
    ESP_DECI,
    ESP_NONE,
    ESP_DECA,
    ESP_HECTO,
    ESP_KILO,
    ESP_MEGA,
    ESP_GIGA
};

enum EMUTUR
{
    ETR_EMF,
    ETR_MG_L
};

enum EMUSML
{
    ESML_UNIT
};

enum EMUFLV
{
    EFLV_UNIT
};

enum EMUNOS
{
    ENOS_BELL
};

enum EMUCHR
{
    ECHR_COLORNESS
};

enum EMUHRD
{
    EHRD_DEGREE
};

enum EMUALK
{
    MILL_EQ_LITER
};

enum EMUOXI
{
    EO_PERMANGANATE,
    EO_HPK
};

enum EMUHYD
{
    HYD_PH
};

enum EMUAMO
{
    EA_ITEMS,   //!< штуки
    EA_PACKS,   //!< упаковки
    EA_UNITS,    //!< единицы
    EA_ENUM     //!< enum в параметры
};

enum EMUPOW
{
    EPW_WATT
};

enum EMUVOLT
{
    EV_VOLT
};

enum EMUCURR
{
    EMC_AMPER
};

enum E_MEASURE_UNITS : uint8_t
{
    EMU_DISTANCE,
    EMU_TIME,
    EMU_VELOCITY,
    EMU_TEMPERATURE,
    EMU_PRESSURE,
    EMU_MASS,
    EMU_CONSUMPTION,
    EMU_CONCENTRATION,
    EMU_VOLUME,
    EMU_TURBIDITY,
    EMU_SMELL,
    EMU_FLAVOR,
    EMU_NOISE,
    EMU_CHROMATICITY,
    EMU_HARDNESS,
    EMU_ALKALINITY,
    EMU_OXIDIZABILITY,
    EMU_HYDROGEN_INDEX,
    EMU_AMOUNT,
    EMU_ELECTRIC_POWER,
    EMU_VOLTAGE,
    EMU_ELECTRIC_CURRENT,

    EMU_COUNT
};

typedef std::string Tstring;
typedef std::variant<EMUDIS, EMUTIM, EMUVEL, EMUTEM, EMUPRE, EMUMAS, EMUCONS, EMUCONC, EMUVOL, EMUTUR, EMUSML,
                     EMUFLV, EMUNOS, EMUCHR, EMUHRD, EMUALK, EMUOXI, EMUHYD, EMUAMO, EMUPOW, EMUVOLT, EMUCURR> VSubtypes;

static  VSubtypes subtypesFromInt(E_MEASURE_UNITS unit, int ival)
{
    VSubtypes subtype;
    switch (unit)
    {
        case EMU_DISTANCE:
            subtype = (EMUDIS)ival;
            break;
        case EMU_TIME:
            subtype = (EMUTIM)ival;
            break;
        case EMU_VELOCITY:
            subtype = (EMUVEL)ival;
            break;
        case EMU_TEMPERATURE:
            subtype = (EMUTEM)ival;
            break;
        case EMU_PRESSURE:
            subtype = (EMUPRE)ival;
            break;
        case EMU_MASS:
            subtype = (EMUMAS)ival;
            break;
        case EMU_CONSUMPTION:
            subtype = (EMUCONS)ival;
            break;
        case EMU_CONCENTRATION:
            subtype = (EMUCONC)ival;
            break;
        case EMU_VOLUME:
            subtype = (EMUVOL)ival;
            break;
        case EMU_TURBIDITY:
            subtype = (EMUTUR)ival;
            break;
        case EMU_SMELL:
            subtype = (EMUSML)ival;
            break;
        case EMU_FLAVOR:
            subtype = (EMUFLV)ival;
            break;
        case EMU_NOISE:
            subtype = (EMUNOS)ival;
            break;
        case EMU_CHROMATICITY:
            subtype = (EMUCHR)ival;
            break;
        case EMU_HARDNESS:
            subtype = (EMUHRD)ival;
            break;
        case EMU_ALKALINITY:
            subtype = (EMUALK)ival;
            break;
        case EMU_OXIDIZABILITY:
            subtype = (EMUOXI)ival;
            break;
        case EMU_HYDROGEN_INDEX:
            subtype = (EMUHYD)ival;
            break;
        case EMU_AMOUNT:
            subtype = (EMUAMO)ival;
            break;
        case EMU_ELECTRIC_POWER:
            subtype = (EMUPOW)ival;
            break;
        case EMU_VOLTAGE:
            subtype = (EMUVOLT)ival;
            break;
        case EMU_ELECTRIC_CURRENT:
            subtype = (EMUCURR)ival;
            break;
        case EMU_COUNT:
            subtype = {};
            break;
    }
    return subtype;
}

enum class EChemicalElement : uint8_t
{
    Fe, Mn, Ca, Mg, K, Na, Sr, Cu, Zn, Pb, Hg, Cr, Ba, B, None
    // таблица Менделеева
};

enum class EGeneralizedSubstance : uint8_t
{
    Ammonium, Phosphates, HydrogenSulfide, Chlorides, Sulfates,
    Fluorides, Nitrates, Nitrites, Cyanides, PetroleumProducts,
    Surfactants, DryResidue, None
    // ионы/соединения не отдельные элементы, а "обобщённые"
};

//struct SOtherSubstance { Tstring label; };  // раньше addition

//typedef std::variant<EChemicalElement, EGeneralizedSubstance, SOtherSubstance> V_SUBSTANCE;
typedef std::variant<EChemicalElement, EGeneralizedSubstance, Tstring> V_SUBSTANCE;


#endif //MEASURE_SYSD_TYPES_H
