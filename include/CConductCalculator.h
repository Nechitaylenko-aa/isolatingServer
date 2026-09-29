//
// Created by artem on 30.06.24.
//

#ifndef NYM_PROJECT_CCONDUCTCALCULATOR_H
#define NYM_PROJECT_CCONDUCTCALCULATOR_H


#include <map>

namespace NCore
{
    class CConductor;
    class COperatingBody;
    class CCap;


    typedef struct s_cap_data
    {
        bool      is_ob{false};     //!< РТ от кепки получено (true)
        float     weight{false};    //!< весовой коэффициент от входящей кепки
    }s_cap_data;

    class CConductCalculator
    {
    public:
        explicit CConductCalculator(CConductor *tube);
        ~CConductCalculator();

        COperatingBody *    calculate_weights(const COperatingBody *body, CCap *sender);
        COperatingBody *    put_ob(const COperatingBody *ob, CCap *sender);

        void                remove_cap(CCap *cap);
        void                add_cap(CCap *cap);
        void                clear();

    private:
        std::map<CCap*, s_cap_data>   * caps_in_state();
        std::map<CCap*, float>        * weights_map();

        CConductor          * m_conductor;
        COperatingBody      * tmp_body{nullptr};
        bool                  m_income_weights_complete{false};
        std::map<CCap*, s_cap_data>   * m_caps_in_state;  //!< состояние входных кепок - отдали РТ или нет
        std::map<CCap*, float>        * m_weight_map;     //!< весовые коэффициенты исходящих наконечников

    private:
        bool                check_tube_income(const COperatingBody *ob, CCap *sender);
        void                reset_caps_state();
        void                reset_caps_weights();
    };

}
#endif //NYM_PROJECT_CCONDUCTCALCULATOR_H
