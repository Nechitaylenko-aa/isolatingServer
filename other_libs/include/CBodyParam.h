//
// Created by artem on 8/19/24.
//

#ifndef NYM_PROJECT_CBODYPARAM_H
#define NYM_PROJECT_CBODYPARAM_H

#include "CParameter.h"

//typedef CParameter CBodyParam;

/**@brief параметр рабочего тела. Отличается от параметра наличием двух полей:
 * крупность включений MIN и MAX, мкм (микрометры) */
class CBodyParam : public CParameter
{
    friend class CVariantPtr;
public:
    CBodyParam();
    CBodyParam(const E_MEASURE_UNITS &measure_unit,
               const VSubtypes &subtype,
               const float &value = 0,
               const float &seed_size_min = 0,
               const float &seed_size_max = 0,
               const EStandardPrefix &prefix = ESP_NONE,
               const Tstring &additional_name = "");
    CBodyParam(const E_MEASURE_UNITS &measure_unit,
               const VSubtypes &subtype,
               const float &value,
               const EStandardPrefix &prefix,
               const V_SUBSTANCE &subs,
               float seed_min = 0, float seed_max = 0);
    CBodyParam(const CBodyParam &src);
    CBodyParam(CBodyParam &&tmp) noexcept ;
    ~CBodyParam() override;

    friend bool operator==(const CBodyParam & lhs, const CBodyParam &rhs);
    friend bool operator!=(const CBodyParam & lhs, const CBodyParam &rhs);

    CBodyParam & operator=(const CBodyParam &src);
    CBodyParam & operator=(CBodyParam&& tmp) noexcept;

    CBodyParam& operator += (const CBodyParam &rhs) ;
    CBodyParam& operator -= (const CBodyParam &rhs) ;
    CBodyParam& operator *= (const CBodyParam &rhs) ;
    CBodyParam& operator /= (const CBodyParam &rhs) ;

    CBodyParam operator + (const CBodyParam &rhs) ;
    CBodyParam operator - (const CBodyParam &rhs) ;
    CBodyParam operator * (const CBodyParam &rhs) ;
    CBodyParam operator / (const CBodyParam &rhs) ;

    [[nodiscard]] bool    is_acceptable(const CBodyParam &other) const;

    [[nodiscard]] bool    is_coarseness() const;
    [[nodiscard]] float   coarse_min() const;
    void    set_coarse_min(const float &value);
    [[nodiscard]] float   coarse_max() const;
    void    set_coarse_max(const float &value);

    void set_after_deserialize(CMeasureUnit *unit, const float &min, const float &max);

protected:
    CMeasureUnit   *m_seed_min;
    CMeasureUnit   *m_seed_max;

};


#endif //NYM_PROJECT_CBODYPARAM_H
