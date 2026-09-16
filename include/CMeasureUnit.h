//
// Created by artem on 8/17/24.
//

#ifndef MEASURE_SYSD_CMEASUREUNIT_H
#define MEASURE_SYSD_CMEASUREUNIT_H

#include <initializer_list>


#define ADD_LENGTH 12   //!< acceptable length of the additional unit name

#include "CMeasureSet.h"
//#include "../../interfaces/include/core-types.h"

/** @brief class for representation of the measure unit*/
class CMeasureUnit
{
public:
    CMeasureUnit() = delete;
    CMeasureUnit(const CMeasureUnit &);
    CMeasureUnit(CMeasureUnit &&) noexcept;
    CMeasureUnit(const float &value, CMeasureSet* measure, const Tstring &add);
    virtual ~CMeasureUnit();

    CMeasureUnit& operator=(const CMeasureUnit &src);
    CMeasureUnit & operator=(CMeasureUnit && tmp) noexcept;
    friend bool operator==(const CMeasureUnit &lhs, const CMeasureUnit &rhs);
    friend bool operator!=(const CMeasureUnit &lhs, const CMeasureUnit &rhs);

    [[nodiscard]] CMeasureSet *  measure_set() const;

    [[nodiscard]] Tstring     unit_name() const;
    [[nodiscard]] Tstring     additional() const;
    [[nodiscard]] Tstring     dimension_user_name() const;
    [[nodiscard]] Tstring     dimension_si_name() const;

    [[nodiscard]] E_MEASURE_UNITS measure_unit() const;
    [[nodiscard]] VSubtypes       subtype() const;
    [[nodiscard]] EStandardPrefix prefix() const;

    [[nodiscard]] virtual float     value() const;
    [[nodiscard]] virtual float     si_value() const = 0;

    virtual void      set_value(const float &value);
    virtual void      set_si_value(const float &si_value) = 0;

    [[nodiscard]] bool    acceptable(const CMeasureUnit & other) const;
    [[nodiscard]] uint16_t    serial_version() const;

    friend class CVariantPtr;
protected:
    CMeasureSet                 * m_measure_set{nullptr};
    Tstring                       m_additional;//[ADD_LENGTH]{};
    float                         m_value{0.0f};
};


#endif //MEASURE_SYSD_CMEASUREUNIT_H
