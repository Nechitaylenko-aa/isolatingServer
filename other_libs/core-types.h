//
// Created by nechi on 17.10.2023.
//

#ifndef NYM_PROJECT_CORE_TYPES_H
#define NYM_PROJECT_CORE_TYPES_H

#ifdef FreeBSD
#include <cstdint>
#endif


#include <cmath>
#include <string>
#include <vector>

#ifdef Linux
#include <cstdint>
#endif

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else

#define PATH_SEPARATOR "/"
#endif

#define FLOAT_ACCURACY  0.0001 //!< максимальная  погрешность всех вещественных чисел при сравнении,

#include <variant>

enum DiagramType {
    TECH,
    SFC,
    LD,
    FBD
};

/**@brief Загрузка фильтров*/
enum EFilterMediaType
{
    EMF_QUARTZ_SAND,
    EMF_ANTHRACITE,
    EMF_EXPANDED_CLAY,
    EMF_ZEOLITE
};


enum ECellRole
{
    ECR_SUBPROJECT,
    ECR_COMPONENT
};

enum    E_TYPE_VARIABLE
{
    ETV_INT8,
    ETV_INT16,
    ETV_INT32,
    ETV_INT64,
    ETV_UINT8,
    ETV_UINT16,
    ETV_UINT32,
    ETV_UINT64,
    ETV_LONG_LONG_INT,
    ETV_LONG_LONG_UINT,
    ETV_FLOAT,
    ETV_DOUBLE,
    ETV_LONG_DOUBLE,
    ETV_BOOL,
    ETV_STRING,
    ETV_BODY_PARAM,
    EVT_PARAMETER,
    EVT_OPERATING_BODY,
    ETV_NDATE,
    EVT_CAP,
    EVT_CELL,
    EVT_CONDUCTOR,
    EVT_MEASURE_SET,
    EVT_MEASURE_UNIT
};


    typedef std::string Tstring;
    typedef size_t Tsize;
    typedef unsigned char BYTE;
    typedef float  TValueType;

#ifdef WIN32

//# include "stdint-gcc.h"

    typedef uint8_t Tuint8;     // aka our uint8
    typedef uint16_t Tuint16;     // aka our uint
    typedef int16_t Tint16;     // aka our int
    typedef int32_t Tint32;
    typedef uint64_t Tuint64;     // aka our uint64

    typedef unsigned int uint;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0)

#else

    typedef uint8_t Tuint8;     // aka our uint8
    typedef uint16_t Tuint16;   // aka our uint
    typedef int16_t Tint16;     // aka our int
    typedef int32_t Tint32;     //
    typedef uint64_t Tuint64;   // aka our uint64


#endif

namespace NCore
{
    enum E_PROJECT_TYPE : uint8_t
    {
        pt_water_drink,
        pt_water_tech,
        pt_water_waste,
        pt_water_industrial,
        pt_water_surface,
        pt_water_recycle,
        pt_water_dehydration_sludge,
        pt_water_pump_station_II,
        pt_water_well_pump_station,
        pt_all_water,
        pt_gas_gnks,
        pt_gas_grs_np,
        pt_gas_boiler,
        pt_gas_upig,
        pt_all_gas,
        pt_elect_plant_supply,
        pt_elect_city_supply,
        pt_undef
    };

    enum EProjectDivider : uint8_t
    {
        epd_water,
        epd_gas = pt_gas_gnks,
        epd_electric = pt_elect_plant_supply
    };

    enum E_BODY_TYPE
    {
        BT_WATER,
        BT_GAS,
        BT_ELECTRICITY,
        BT_UNDEF,
    };

    static E_BODY_TYPE subprojects_base_body(const E_PROJECT_TYPE &subproject_type)
    {
        if (subproject_type < E_PROJECT_TYPE::pt_all_water)
        {
            return NCore::BT_WATER;
        }

        if (subproject_type > E_PROJECT_TYPE::pt_all_water &&
            subproject_type < E_PROJECT_TYPE::pt_all_gas)
        {
            return NCore::BT_GAS;
        }

        if (subproject_type > pt_all_gas && subproject_type < pt_undef)
        {
            return BT_ELECTRICITY;
        }
        return BT_UNDEF;
    }

    /** @brief контур у клеток - главный контур и дополнительный. Главный - как правило базовое рабочее тело, например
     * теплообменник для газа - главный контур газовый, дополнительный - греющий агент */
    enum E_CONTOUR_TYPE
    {
        CT_MAIN,
        CT_ADDITIONAL
    };

    enum E_CAP_DIRECTION : uint8_t
    {
        CD_INPUT,
        CD_OUTPUT
    };

    enum  E_WATER_COMPONENTS : int8_t
    {
        EWB_PROJECT_CELL = -1,
        EWB_WATER_FILTER_LIGHT = 0,
        EWB_WATER_FILTER_ION_EXCHANGE,
        EWB_WATER_FILTER_SORPTION,
        EWB_MEMBRANE_OSMOS,
        EWB_MEMBRANE_NANO,
        EWB_MEMBRANE_ULTRA,
        EWB_CAPACITY,
        EWB_PUMP_STATION,
        EWB_ACCOUNT_NODE,
        EWB_VALVE_WATER_THREE_WAY,

        EWB_AERATOR,  // аэратор. относительно сложная но полностью пассивная (самостоятельная) установка
        EWB_DOZER,    // насос-дозатор. управляем мы (импульсное управление)
        EWB_DOZER_PREP, // бывает ручное, бывает автомат (зависит от выбранного оборуд. по умолчанию ручн. Это как насосная станция, обязательная генерация кода.
        EWB_SNAKE, //!< NOT component
        EWB_UF_LAMP, // вкл/выкл
        EWB_PACKET_DEWATER, // обяз. генерация кода
        EWB_VALVE_CUT, //вкл./выкл
        EWB_AIR_BLOWER, //не видел воздуходувок объединенных в станции, но мысль интересная. Сделаем копию насосной станции

        EWB_GENERIC_COMP,
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
        EEC_PROJECT_CELL = -1,
        EEC_POWER_SUPPLY,
        EEC_COUNT
    };

    enum EComponentTypes : uint8_t
    {
        ect_water,
        ect_gas,
        ect_electric
    };

    static  const Tstring  component_water_names[EWB_COUNT]
    {
        "Filter light",
        "Ion exchange filter",
        "Sorption filter",
        "Membrane osmose",
        "Membrane nano",
        "Membrane ultra",
        "Capacity",
        "Pump station",
        "EWB_WATER_ACCOUNT_NODE",
        "EWB_WATER_TWO_WAY_VALVE"
    };

    static  const Tstring  component_gas_names[EWB_COUNT]
    {
        "Filter",
        "REDUCING_DEV",
        "COMPRESSOR",
    };

    static  const Tstring  component_electric_names[EWB_COUNT]
    {
        "EEC_POWER_SUPPLY"
    };

    typedef std::variant<E_WATER_COMPONENTS, E_GAS_COMPONENTS, E_ELECTRIC_COMPONENTS> TComponentType;

    template<class> inline constexpr bool always_false = false;
    class ComponentConverter {
    public:
        static uint16_t get_int_TComponentType(const TComponentType& type)
        {
            return std::visit([](auto&& arg) -> uint16_t
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_enum_v<T>)
                {
                    return static_cast<uint16_t>(arg);
                }

                /*
                if constexpr (std::is_same_v<T, E_WATER_COMPONENTS> ||
                              std::is_same_v<T, E_GAS_COMPONENTS> ||
                              std::is_same_v<T, E_ELECTRIC_COMPONENTS>) {
                    return static_cast<uint16_t>(arg);
                }*/
                else
                {
                    static_assert(always_false<T>, "Non-exhaustive visitor!");
                    return 0;
                }
            }, type);
        }

        static TComponentType get_TComponentType_from_int(const EComponentTypes& env_type, const int16_t value)
        {
            switch (env_type) {
                case ect_water:
                    return static_cast<E_WATER_COMPONENTS>(value);
                case ect_gas:
                    return static_cast<E_GAS_COMPONENTS>(value);
                case ect_electric:
                    return static_cast<E_ELECTRIC_COMPONENTS>(value);
                default:
                    throw std::invalid_argument("Unknown component type: " + std::to_string(static_cast<int>(env_type)));
            }
        }
        static TComponentType get_TComponentType_from_int(const E_PROJECT_TYPE & project_type, const uint16_t &value)
        {
            EComponentTypes component_type;
            if (project_type <= pt_all_water)
                component_type = ect_water;
            else if (project_type > pt_all_water && project_type <= pt_all_gas)
                component_type = ect_gas;
            else
                component_type = ect_electric;

            return  get_TComponentType_from_int(component_type, value);
        }
    };


    static bool is_float_equal(const float &left, const float &right)
    {
        return std::fabs(left - right) < FLOAT_ACCURACY;
    }

}

#endif //NYM_PROJECT_CORE_TYPES_H
