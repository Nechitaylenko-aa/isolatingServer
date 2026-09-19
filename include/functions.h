//
// Created by nechi on 07.03.2022.
//

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <cmath>
#include "../../interfaces/include/core-types.h"
#include <utility>
#include <vector>

typedef std::string Tstring;

#define  SALT "salt-string"



enum EBoolTypes{
    EBT_DIGIT,
    EBT_LOWSTR,
    EBT_UPSTR
};

class CFunctions{

private:
    static Tuint64 StringToDigits(const Tstring & str);
    static Tstring DigitsToString(const Tuint64 & dig);


public:

    static Tint32 ToInt32(const Tstring & str);
    static double ToDouble(const Tstring & str, const Tsize & precision = 4);
    static long double ToLongDouble(const Tstring & str, const Tsize & precision = 4);
    static bool   ToBool(const Tstring & str);


    static Tstring ToString(const unsigned & value);
    static Tstring ToString(const long long & value);
    static Tstring ToString(const unsigned long & value);
    static Tstring ToString(const long & value);
    static Tstring ToString(const Tint32 & value);
    static Tstring ToString(const double & value, const Tsize & prec = 4, const bool & ispoint = true);
    static Tstring ToString(const long double & value, const Tsize & prec = 4, const bool & ispoint = true);
    static Tstring ToString(const bool & value, const EBoolTypes & type = EBT_DIGIT);

    static Tint32 fromChar4(const char *raw_data);

    static Tstring get_file_extension(const Tstring &file_name);


    /**
     * @brief trims string from start
     */
    static Tstring &ltrim(Tstring &s);

    /**
     * @brief trims string from end
     */
    static Tstring &rtrim(Tstring &s);
    /** @brief trim from both ends*/
    static Tstring &trim(Tstring &s);

    static bool is_integer(const Tstring &text);
    static bool is_float(const Tstring &text);

    //static bool is_floats_equal(const long double &val1, const long double &val2);


    [[nodiscard]]static Tstring  md5_hash(const Tstring &message);

    static Tstring remove_filename_from_path(Tstring &path);

    /** @brief removed all spaces and 'TABs'*/
    static Tstring trim_inner_spaces(Tstring &string);
};




#endif //FUNCTIONS_H
