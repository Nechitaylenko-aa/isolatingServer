//
// Created by artem on 8/19/24.
//

#ifndef NYM_PROJECT_CPARAMETER_H
#define NYM_PROJECT_CPARAMETER_H

#include "../measures/CMeasureUnit.h"


class CParameter
{
public:
    CParameter(const CParameter &src);
    CParameter();
    CParameter(const E_MEASURE_UNITS &measure_unit,
               const VSubtypes &subtype,
               const float &value,
               const EStandardPrefix &prefix,
               const Tstring &additional_name);
    CParameter(const E_MEASURE_UNITS &measure_unit,
               const VSubtypes &subtype,
               const float &value,
               const EStandardPrefix &prefix,
               const V_SUBSTANCE &subs);
    CParameter(CParameter &&tmp) noexcept;
    CParameter & operator=(CParameter && tmp) noexcept;
    virtual ~CParameter();

    CParameter& operator=(const CParameter &src);

    [[nodiscard]] CMeasureUnit * measure_unit() const;

    [[nodiscard]] Tstring     unit_name() const;
    [[nodiscard]] Tstring     additional() const;
    [[nodiscard]] Tstring     dimension_user_name() const;
    [[nodiscard]] Tstring     dimension_si_name() const;

    [[nodiscard]] float     value() const;
    [[nodiscard]] float     si_value() const;

    void      set_value(const float &value);
    void      set_si_value(const float &si_value);


    CParameter& operator += (const CParameter &rhs);
    CParameter& operator -= (const CParameter &rhs);
    CParameter& operator *= (const CParameter &rhs);
    CParameter& operator /= (const CParameter &rhs);

    CParameter operator + (const CParameter &rhs);
    CParameter operator - (const CParameter &rhs);
    CParameter operator * (const CParameter &rhs);
    CParameter operator / (const CParameter &rhs);

    friend bool operator > (const CParameter &lhs, const CParameter &rhs);
    friend bool operator >= (const CParameter &lhs, const CParameter &rhs);
    friend bool operator < (const CParameter &lhs, const CParameter &rhs);
    friend bool operator <= (const CParameter &lhs, const CParameter &rhs);
    friend bool operator == (const CParameter &lhs, const CParameter &rhs);
    friend bool operator != (const CParameter &lhs, const CParameter &rhs);

    void set_after_deserialize(CMeasureUnit *unit);

protected:
    CMeasureUnit    * m_measure_unit{nullptr};
};


#endif //NYM_PROJECT_CPARAMETER_H
