//
// Created by artem on 8/8/24.
//

#ifndef NYM_PROJECT_FLOAT_COMPARISON_H
#define NYM_PROJECT_FLOAT_COMPARISON_H

#define ACC_LIMIT 0.0001

class floats
{
public:
    static bool is_floats_equal(const float &val1, const float &val2);
    static bool is_floats_equal(const double &val1, const double &val2);
    static bool is_float_less(const float &val1, const float &val2);
    static bool is_float_less_equal(const float &val1, const float &val2);
    static bool is_float_grater(const float &val1, const float &val2);
    static bool is_float_grater_equal(const float &val1, const float &val2);

};


#endif //NYM_PROJECT_FLOAT_COMPARISON_H
