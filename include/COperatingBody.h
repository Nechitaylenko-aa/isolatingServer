//
// Created by artem on 04.04.24.
//

#ifndef LIBCORED_COPERATINGBODY_H
#define LIBCORED_COPERATINGBODY_H

#include "core-types.h"
#include "CBodyParam.h"


namespace NCore
{

    class COperatingBody
    {
    public:
        COperatingBody() = delete;
        COperatingBody(const COperatingBody &);

        COperatingBody(COperatingBody &&) noexcept;
        explicit COperatingBody(const E_BODY_TYPE &body_type);
        virtual ~COperatingBody();

        friend bool operator==(const COperatingBody &lhs, const COperatingBody &rhs);

        COperatingBody& operator=(const COperatingBody &rhs);
        COperatingBody& operator=(COperatingBody && tmp) noexcept;


        COperatingBody operator+(const COperatingBody &rhs);

        /**
         * @brief разделяет поток объёмом this на потоки volume и оставшийся this->volume - volume
         * @param[in] volume
         * @return новое РТ объёмом volume, при этом this уменьшен на соответствующий объём или 0.
         */
        COperatingBody operator-(const long double &volume);
        COperatingBody& operator+=(const COperatingBody &rhs);
        COperatingBody operator*(const float &value);
        COperatingBody& operator *=(const float &value);


        [[nodiscard]] CBodyParam* get_temperature() const;
        [[nodiscard]] float get_si_temperature() const;
        void  set_si_temperature(const float &temper);

        [[nodiscard]] CBodyParam* get_pressure() const;
        [[nodiscard]] float get_si_pressure() const;
        void set_si_pressure(const float &pressure);

        [[nodiscard]] CBodyParam* get_volume() const;
        [[nodiscard]] float get_si_volume() const;
        void set_si_volume(const float &volume);

        [[nodiscard]] E_BODY_TYPE  body_type() const;



        CBodyParam* add_parameter(CBodyParam *parameter);
        CBodyParam* get_parameter(const uint16_t &index);
        CBodyParam* remove_parameter(const uint16_t &index);
        CBodyParam* remove_parameter(CBodyParam *parameter);
        [[nodiscard]] uint16_t    parameters_count() const;

        [[nodiscard]] bool is_test() const;
        void  set_test(const bool &yes_no);

        void reset_body();
    protected:



    private:
        CBodyParam  * m_temperature;
        CBodyParam  * m_pressure;
        CBodyParam  * m_volume;

        bool          m_is_test{false};

        // COperatingBody  *m_tmp{nullptr};

        E_BODY_TYPE  m_body_type;

        std::vector<CBodyParam*>    * m_body_parameters;
        static CBodyParam * get_same_param(CBodyParam *local, std::vector<CBodyParam*> *alien);

        [[nodiscard]] CBodyParam  add(CBodyParam *local, CBodyParam *alien, const COperatingBody &rhs);

        /**
         * @brief добавляет свойства в body0 из body1 которых нет в body0
         * @param[in] body0 COperatingBody
         * @param[in] body1 COperatingBody
         */
        void    fill_missing_items(COperatingBody *body0, const COperatingBody *body1);

    };

}

#endif //LIBCORED_COPERATINGBODY_H
