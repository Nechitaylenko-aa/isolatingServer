//
// Created by artem on 25.05.24.
//

#ifndef NYM_PROJECT_CDRINKWATERTOR_H
#define NYM_PROJECT_CDRINKWATERTOR_H

//#include "saver_export.h"
#include "IGeneralTor.h"
#include "CLimits.h"
#include "../../app-project/forms/tor/tor-structs/tor-content.h"

static const char * drink_water_source[DWS_COUNT]
        {"Скважина", "Привозная", "Природный водный объёкт", "Не установлен"};

/**
 * @brief Техническое задание на установку водоподготовки питьевой воды
 */
class /*SAVER_EXPORT*/ CDrinkWaterTor : public IGeneralTor
{
public:
    explicit CDrinkWaterTor(CSubProject *parent);
    CDrinkWaterTor(const CDrinkWaterTor &) = delete;
    ~CDrinkWaterTor() override;

    void    Delete() override;
    CSubProject  * parent() override;
    [[nodiscard]] NCore::E_PROJECT_TYPE   type() const override;


    [[nodiscard]] bool    is_ready()   const override;

    std::vector<NCore::COperatingBody *> * operating_bodies() override;
    NCore::COperatingBody*  general_working_body() override;
    NCore::COperatingBody*  reference_body() override;

    [[nodiscard]] Tstring     name() const override;
    void        set_name(const Tstring &name) override;

    [[nodiscard]] DRINK_WATER_SOURCE      water_source() const;
    void                    set_water_source(const DRINK_WATER_SOURCE &water_source);

    [[nodiscard]] float                   clean_water_reserve_hour() const;
    void                    set_clean_water_reserve(const float &hours);

    [[nodiscard]] float                   chemical_warehouse() const;
    void                    set_chemical_warehouse(const float &month);

    [[nodiscard]] bool                    is_waste_chanel() const;
    void                    set_waste_chanel(const bool &yes_no);

    [[nodiscard]] float                   water_in_temper() const;
    void                    set_water_in_temper(const float &celsius);

    [[nodiscard]] float                   water_in_press() const;
    void                    set_water_in_press(const float &press_meter);

    [[nodiscard]] float                   consumption_in_hour_max() const;
    void                    set_consumption_in_hour_max(const float &cubic_m_hour);

    [[nodiscard]] float                   consumption_in_day() const;
    void                    set_consumption_in_day(const float &cubic_m_day);

    [[nodiscard]] float                   consumption_out_hour_max() const;
    void                    set_consumption_out_hour_max(const float &cubic_m_hour);

    [[nodiscard]] float                   water_out_press() const;
    void                    set_water_out_press(const float &press_meter);
    s_drink_water * parameters(); // конкретная структура ТЗ
    std::vector<CLimits>    * required_limits();

    CLimits*    limit_at(const uint8_t & index) override;
    [[nodiscard]] uint8_t     limits_count() const override;

    [[nodiscard]] uint8_t base_inputs() const override;
    void  set_base_inputs(uint8_t inputs) override;

    [[nodiscard]] uint8_t  base_outputs() const override;
    void set_base_outputs(uint8_t outputs) override;

    /// serialization ----------------------------
    bool     set_parameters(CContainer &container) override;
    void     get_parameters(CContainer & container) override;



protected:

private:
    s_drink_water       * m_station_params;
    std::vector<CLimits>* m_limits;
    CSubProject         * m_parent;
    Tstring               m_name;

    uint32_t limit_am;
    uint32_t w_body_am;
    std::vector<uint32_t> w_body_params;
    std::vector<uint8_t>  w_body_types;
    uint32_t r_body_am;
    std::vector<uint32_t> r_body_params;
    std::vector<uint8_t>  r_body_types;
    uint32_t limits_am;



    void fill_working_body(); //!< create parameters and setup them
    void serialize_bodies(CContainer &container);
    void deserialize_bodies(CContainer &container);
};

#endif //NYM_PROJECT_CDRINKWATERTOR_H

namespace WT
{
    static std::vector<std::tuple<E_MEASURE_UNITS, VSubtypes, float, EStandardPrefix, Tstring>> water_drink_requirements
    {
            {EMU_TEMPERATURE, EMUTEM::emt_celsius, 7, ESP_NONE, ""},
            {EMU_SMELL, EMUSML::ESML_UNIT, 2, ESP_NONE, ""},
            {EMU_FLAVOR, EMUFLV::EFLV_UNIT, 2, ESP_NONE, ""},
            {EMU_CHROMATICITY, EMUCHR::ECHR_COLORNESS, 20, ESP_NONE, ""},
            {EMU_TURBIDITY, EMUTUR::ETR_MG_L, 2.6f, ESP_NONE, ""},
            {EMU_HYDROGEN_INDEX, EMUHYD::HYD_PH, 6.5f, ESP_NONE, ""},
            {EMU_HARDNESS, EMUHRD::EHRD_DEGREE, 7, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, -1000, ESP_NONE, ""},
            {EMU_ALKALINITY, EMUALK::MILL_EQ_LITER, -1000, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.3, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.1, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, -1000, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, -1000, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, -1000, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 200, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 7, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 1, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.03f, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.0005, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.1, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 10, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 2, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 3.5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.003, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 350, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 500, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 45, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 3, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.035, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.1, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, 0.5, ESP_NONE, ""},
            {EMU_CONCENTRATION, EMUCONC::con_mg_liter, -1000, ESP_NONE, ""},
            {EMU_OXIDIZABILITY, EMUOXI::EO_HPK, 5, ESP_NONE, ""}
    };

    /*{PU_CONCENTRATION, VC_MG_LITER, "Гидрокарбонат"},
                        {PU_ALKALINITY, EPU_ALKALINITY::EA_MILLI_EQUIV_LITER, ""},
                        {PU_CONCENTRATION, VC_MG_LITER, "Fe"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Mn"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Ca"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Mg"},
                        {PU_CONCENTRATION, VC_MG_LITER, "K"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Na"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Sr"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Cu"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Zn"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Pb"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Hg"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Cr"},
                        {PU_CONCENTRATION, VC_MG_LITER, "Ba"},
                        {PU_CONCENTRATION, VC_MG_LITER, "B"},
                        {PU_CONCENTRATION, VC_MG_LITER, "кремния (кисл.)"},
                        {PU_CONCENTRATION, VC_MG_LITER, "ионов аммония"},
                        {PU_CONCENTRATION, VC_MG_LITER, "ионов фосфатов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "сероводорода"},
                        {PU_CONCENTRATION, VC_MG_LITER, "хлоридов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "сульфатов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "фторидов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "нитратов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "нитритов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "цианидов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "нефтепродуктов"},
                        {PU_CONCENTRATION, VC_MG_LITER, "АПВ"},
                        {PU_CONCENTRATION, VC_MG_LITER, "сухого остат. (общая минерализация)"},
                        {PU_OXIDIZABILITY, EO_MGO2_LITER, "перманганатная"}*/

    static std::vector<std::tuple<double, E_ConditionOpers, double, E_ConditionOpers>> wt_limits{
            {4,      ECO_GE,      100, ECO_LE},
            {0,      ECO_GE,      2, ECO_LE},
            {0,      ECO_GE,      2, ECO_LE},
            {0,     ECO_GE,      20, ECO_LE},
            {0,    ECO_GE,      2.6, ECO_LE},
            {6.5,    ECO_GE,      8.5, ECO_LE},
            {0,      ECO_GE,      7, ECO_LE},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {0,    ECO_GE,      0.3, ECO_LE},
            {0,    ECO_GE,      0.1, ECO_LE},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {0,    ECO_GE,      200, ECO_LE},
            {0,      ECO_GE,      7, ECO_LE},
            {0,      ECO_GE,      1, ECO_LE},
            {0,      ECO_GE,      5, ECO_LE},
            {0,   ECO_GE,      0.03, ECO_LE},
            {0, ECO_GE,      0.0005, ECO_LE},
            {0,    ECO_GE,      0.5, ECO_LE},
            {0,    ECO_GE,      0.1, ECO_LE},
            {0,    ECO_GE,      0.5, ECO_LE},
            {0,     ECO_GE,      10, ECO_LE},
            {0,      ECO_GE,      2, ECO_LE},
            {0,    ECO_GE,      3.5, ECO_LE},
            {0,  ECO_GE,      0.003, ECO_LE},
            {0,    ECO_GE,      350, ECO_LE},
            {0,    ECO_GE,      500, ECO_LE},
            {0,    ECO_GE,      0.5, ECO_LE},
            {0,     ECO_GE,      45, ECO_LE},
            {0,      ECO_GE,      3, ECO_LE},
            {0,  ECO_GE,      0.035, ECO_LE},
            {0,    ECO_GE,      0.1, ECO_LE},
            {0,    ECO_GE,      0.5, ECO_LE},
            {-1000,  ECO_COUNT,      0, ECO_COUNT},
            {0,      ECO_GE,      5, ECO_LE}
    };
    static Tstring wt_source = "https://eng-eco.ru/upload/iblock/f62/f62518fef27847ef31fcc40c3543b2a5.pdf";
}
