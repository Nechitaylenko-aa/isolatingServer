//
// Created by artem on 8/29/24.
//

#ifndef NYM_PROJECT_CLIMITS_H
#define NYM_PROJECT_CLIMITS_H


//#include "measure_EXPORTS.h"
#include <nlohmann/json.hpp>
#include <pugixml.hpp>
#include "types.h"


enum E_ConditionOpers : uint8_t
{
    ECO_GE,
    ECO_GT,
    ECO_LE,
    ECO_LT,
    ECO_EQ,
    ECO_NE,
    ECO_COUNT
};

static Tstring T_ConditionOpers[E_ConditionOpers::ECO_COUNT]
{
    ">=",
    ">",
    "<=",
    "<",
    "=",
    "!="
};

typedef struct s_range
{
    E_ConditionOpers left_cond;
    float       left_limit;
    E_ConditionOpers right_cond;
    float       right_limit;
    s_range() = default;
    s_range(const E_ConditionOpers &l_cond, const float &l_val, const E_ConditionOpers &r_cond, const float &r_val)
    {
        left_cond = l_cond;
        left_limit = l_val;
        right_cond = r_cond;
        right_limit = r_val;
    }
} SRange;

// SRangeSerializer.h
class SRangeSerializer {
public:
    // JSON serialization
    static nlohmann::json to_json(const SRange& range);
    static bool from_json(SRange& range, const nlohmann::json& j);

    // XML serialization (using pugixml)
    static pugi::xml_node to_xml(const SRange& range, pugi::xml_document& doc);
    static bool from_xml(SRange& range, const pugi::xml_node& node);

    // Binary serialization
    static std::vector<uint8_t> to_binary(const SRange& range);
    static size_t from_binary(SRange& range, const uint8_t* data, size_t& offset);
};

/**
 * @brief Класс ограничений значения параметров. Причина: есть параметры, которые могут принимать значения из нескольуих
 * кусков пределов, например частота вращения асинхронника может принимать значения от 0 до 7 и от 11 до 50, т.е.
 * на диапазоне от 0 до 50 Гц есть "дырка" от 7 до 11 Гц, на которых двигателю лучше не работать (конструктивные
 * особенности), а есть параметры, которые могут принимать значения в "дырках" всего диаппазона
 * (минус бескон - плюс бескон).
 * @details можно добавлять только "позитивные" диаппазоны, которые указывают допустимые значения
 * @attention допустимо добавлять диаппазоны только "включительные", т.е. (>= 2.5 ; \<= 8)
 * и НЕЛЬЗЯ "исключительные" (\<= 2.5 ; >= 8)
 */
class  CLimits
{
public:
    CLimits();
    CLimits(const E_ConditionOpers &l_c, const float &left_lim, const E_ConditionOpers &r_c, const float &right_lim);
    CLimits(const E_ConditionOpers &r_c, const float &right_lim);
    CLimits(const CLimits &);
    CLimits(CLimits &&) noexcept;
    ~CLimits();

    CLimits& operator=(const CLimits &rhs);

    bool    add_positive_range(const E_ConditionOpers &left_cond, const float &left_limit,
                               const E_ConditionOpers &right_cond, const float &right_limit);
    [[nodiscard]] bool    is_acceptable(const float &value) const;
    [[nodiscard]] Tstring get_limits_str() const;

    std::vector<SRange>  * limits() {return m_limits; }

    [[nodiscard]] pugi::xml_node to_xml() const;
    void from_xml (const pugi::xml_node &xml);
    [[nodiscard]] nlohmann::json to_json() const;
    void from_json(const nlohmann::json &j);
    size_t from_binary(const std::vector<uint8_t> &bytes);
    [[nodiscard]] std::vector<uint8_t> to_binary() const;


private:
    std::vector<SRange>  * m_limits;
    [[nodiscard]] static Tstring  float_to_str(const float &val) ;
    [[nodiscard]] int is_inside_ranges(const float &value) const;
    [[nodiscard]] static bool is_range_side_ok(const E_ConditionOpers &oper, const float &limit, const float &value) ;
};


#endif //NYM_PROJECT_CLIMITS_H
