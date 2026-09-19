#include "../include/functions.h"
#include <algorithm>

#include <openssl/evp.h>



Tuint64 CFunctions::StringToDigits(const Tstring & str)
{
    Tuint64 res = 0;

    if (str.empty())
        return res;

    Tstring template_str = "0123456789";

    //for (Tstring::const_iterator it = str.begin(); it != str.end(); ++it)
    for (char it : str)
    {
        Tuint64 pos = template_str.find(it);

        if (Tstring::npos == pos)
        {
            return 0;
        }

        res *= 10;
        res += pos;
    }

    return res;

}

Tstring CFunctions::DigitsToString(const Tuint64 & dig)
{
    Tuint64 d = dig;
    Tstring res;

    if (!d)
        return "0";

    while(d)
    {
        res = std::to_string(d % 10).append(res);
        d /= 10;
    }

    return res;
}


Tint32 CFunctions::ToInt32(const Tstring & str)
{

    Tstring s = str;

    if(s.empty())
    {
        return 0;
    }

    bool isMinus = false;
    Tint32 dec;


    // начинаем поиск в строке
    // если есть - в начале, то число отрицательное

    Tsize pos = s.find('-');

    if (0 == pos)
    {
        isMinus = true;
        s.erase(0, 1); // удалим первый элемент
    }

    pos = s.find_first_of('.'); // ищем первую
    if (Tstring::npos == pos) // если не нашли
    {
        pos = s.find_first_of(',');    // поищем первую запятую
    }

    if (Tstring::npos != pos){  // если что-то нашли
        s.erase(pos);                   // удалим все элементы после разделителя и разедлитель
    }


    dec = static_cast<int>(CFunctions::StringToDigits(s));              // получим числовое значение целой части
    return (isMinus ? -1 : 1) * dec;
}



double CFunctions::ToDouble(const Tstring & str, const Tsize & precision)
{
    Tstring s = str;
    if(s.empty())
    {
        return 0;
    }

    auto val = static_cast<Tuint8>(precision);

    struct digit_s
    {
        bool isFrac {false} ;
        bool isMinus {false};
        Tuint64 dec {0};
        Tuint64 frac {0};
        Tuint8 prec  {};

    } digit_s;

    digit_s.prec = val;


    // начинаем поиск в строке
    // если есть - в начале, то число отрицательное
    Tsize pos = s.find('-');
    if (0 == pos)
    {
        digit_s.isMinus = true;
        s.erase(0, 1); // удалим первый элемент
    }

    // теперь поищем разделители
    pos = s.find_first_of('.'); // ищем первую .

    if (Tstring::npos == pos) // если не нашли
    {
        pos = s.find_first_of(',');    // поищем первую запятую
    }

    if (Tstring::npos != pos) // если что-то нашли
    {
        digit_s.isFrac = true;    // укажем, что у нас есть разделитель
        Tstring dec = s.substr(0, pos); // возьмём целую часть
        s.erase(0, pos + 1);                   // удалим все элементы до разделителя и разедлитель
        Tstring frac = s.substr(0, precision);         // возьмём дробную часть значение в соответствие с точностью

        digit_s.dec = CFunctions::StringToDigits(dec);            // получим числовое значение целой части
        digit_s.frac = CFunctions::StringToDigits(frac);          // и дробной
        digit_s.prec = frac.length();                 // узнаем количество знаков после запятой

        if (0 == digit_s.prec)
        {
            return (digit_s.isMinus ? -1 : 1) * (double)digit_s.dec;
        }

        return (digit_s.isMinus ? -1 : 1) * (static_cast<double>(digit_s.dec) + static_cast<double>(digit_s.frac) /
                pow(10, digit_s.prec)); // преобразуем в одно число и вернём
    }

    else
    {                                               // если не было запятых и точек, значит число целое

        digit_s.dec = CFunctions::StringToDigits(s);              // получим числовое значение целой части
        return (digit_s.isMinus ? -1 : 1) * (double) digit_s.dec;
    }
}

long double CFunctions::ToLongDouble(const Tstring &str, const Tsize &precision)
{
    Tstring s = str;
    if(s.empty())
    {
        return 0;
    }

    auto val = static_cast<Tuint8>(precision);

    struct digit_s
    {
        bool isFrac {false} ;
        bool isMinus {false};
        Tuint64 dec {0};
        Tuint64 frac {0};
        Tuint8 prec  {};

    } digit_s;

    digit_s.prec = val;


    // начинаем поиск в строке
    // если есть - в начале, то число отрицательное
    Tsize pos = s.find('-');
    if (0 == pos)
    {
        digit_s.isMinus = true;
        s.erase(0, 1); // удалим первый элемент
    }

    // теперь поищем разделители
    pos = s.find_first_of('.'); // ищем первую .

    if (Tstring::npos == pos) // если не нашли
    {
        pos = s.find_first_of(',');    // поищем первую запятую
    }

    if (Tstring::npos != pos) // если что-то нашли
    {
        digit_s.isFrac = true;    // укажем, что у нас есть разделитель
        Tstring dec = s.substr(0, pos); // возьмём целую часть
        s.erase(0, pos + 1);                   // удалим все элементы до разделителя и разедлитель
        Tstring frac = s.substr(0, precision);         // возьмём дробную часть значение в соответствие с точностью

        digit_s.dec = CFunctions::StringToDigits(dec);            // получим числовое значение целой части
        digit_s.frac = CFunctions::StringToDigits(frac);          // и дробной
        digit_s.prec = frac.length();                 // узнаем количество знаков после запятой

        if (0 == digit_s.prec)
        {
            return (digit_s.isMinus ? -1 : 1) * (long double)digit_s.dec;
        }

        return (digit_s.isMinus ? -1 : 1) * (static_cast<long double>(digit_s.dec) + static_cast<long double>(digit_s.frac) /
                                                                                pow(10, digit_s.prec)); // преобразуем в одно число и вернём
    }

    else
    {                                               // если не было запятых и точек, значит число целое

        digit_s.dec = CFunctions::StringToDigits(s);              // получим числовое значение целой части
        return (digit_s.isMinus ? -1 : 1) * (long double) digit_s.dec;
    }
}


bool CFunctions::ToBool(const Tstring & str)
{
    Tstring s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

    if(s == "TRUE")
        return true;

    if (s == "FALSE")
        return false;

    return (bool)CFunctions::ToInt32(str);
}

/*Tstring CFunctions::ToString(const Tint16 & value)
{
    Tstring res = CFunctions::DigitsToString(abs(value));

    if (value < 0)
        res = "-" + res;

    return res;
}*/

Tstring CFunctions::ToString(const Tint32 &value) {
    Tstring res = CFunctions::DigitsToString(abs(value));
    if (value < 0)
        res = "-" + res;

    return res;
}

Tstring CFunctions::ToString(const long &value)
{
    Tstring res = CFunctions::DigitsToString(abs(value));
    if (value < 0)
        res = "-" + res;

    return res;
}

Tstring CFunctions::ToString(const unsigned long &value)
{
    return CFunctions::DigitsToString(value);
}

Tstring CFunctions::ToString(const unsigned int &value)
{
    return CFunctions::DigitsToString(value);
}

Tstring CFunctions::ToString(const long long &value)
{
    return CFunctions::DigitsToString(value);
}

Tstring CFunctions::ToString(const double & value, const Tsize & prec, const bool & ispoint)
{

    if (!prec) // если точность - 0
        return ToString((Tint16)value); // вернем целое число

    struct digit_s{
        bool isMinus {false};
        Tuint64 dec {0};
        Tuint64 frac {0};
    } digit;

    if (value < 0)
        digit.isMinus = true;


    // разложим число на целую и дробную часть в соответствии с точностью prec
    auto p = static_cast<Tint16 >(pow(10, prec));
    digit.frac = (Tuint64)abs(value * p) % p;
    digit.dec = (Tuint64) abs(value);


    Tstring res = CFunctions::DigitsToString(digit.dec); // записали целую часть

    if (ispoint)        // добавили точку
        res += ".";
    else
        res += ",";

    if (!digit.frac){ // если дробная часть отстутствует (равна нулю)
        for (int i = 0; i < prec; ++i)
            res += "0"; // дополним до точности нулями
    }
    else   // если норм, то
        res += CFunctions::DigitsToString(digit.frac); // добавили дробную

    if (digit.isMinus) // добавим минус
        res = "-" + res;

    return res;
}

Tstring CFunctions::ToString(const long double &value, const Tsize &prec, const bool &ispoint) {
    if (!prec) // если точность - 0
        return ToString((Tint16)value); // вернем целое число

    struct digit_s{
        bool isMinus {false};
        Tuint64 dec {0};
        Tuint64 frac {0};
    } digit;

    if (value < 0)
        digit.isMinus = true;


    // разложим число на целую и дробную часть в соответствии с точностью prec
    auto p = static_cast<Tint16 >(pow(10, prec));
    digit.frac = (Tuint64)abs(value * p) % p;
    digit.dec = (Tuint64) abs(value);


    Tstring res = CFunctions::DigitsToString(digit.dec); // записали целую часть

    if (ispoint)        // добавили точку
        res += ".";
    else
        res += ",";

    if (!digit.frac){ // если дробная часть отстутствует (равна нулю)
        for (int i = 0; i < prec; ++i)
            res += "0"; // дополним до точности нулями
    }
    else   // если норм, то
        res += CFunctions::DigitsToString(digit.frac); // добавили дробную

    if (digit.isMinus) // добавим минус
        res = "-" + res;

    return res;
}

Tstring CFunctions::ToString(const bool & value, const EBoolTypes & type)
{
    switch(type){
        case EBoolTypes::EBT_DIGIT:
            return (value ? "1" : "0");
        case EBoolTypes::EBT_LOWSTR:
            return (value ? "true" : "false");
        case EBoolTypes::EBT_UPSTR:
            return (value ? "TRUE" : "FALSE");
    }
    return "";
}

Tint32 CFunctions::fromChar4(const char *buff) {
    Tint32 res = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | (buff[3]);
    return res;
}

Tstring CFunctions::get_file_extension(const Tstring &file_name) {
    std::string::size_type idx;
    Tstring ext;
    idx = file_name.rfind('.');
    if(idx != std::string::npos)
        ext = file_name.substr(idx+1);

    return ext;
}

Tstring &CFunctions::ltrim(Tstring &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int c){ return !std::isspace(c); }));
    return s;
}

Tstring &CFunctions::rtrim(Tstring &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](int c){ return !std::isspace(c); }).base(), s.end());
    return s;
}

Tstring CFunctions::trim_inner_spaces(Tstring &string)
{
    if (string.empty())
        return "";

    auto str = string;

    std::string delim = " \t";
    auto pred = [delim](std::string::value_type c){ return delim.find(c) != std::string::npos;};

    auto new_end = std::remove_if(str.begin(), str.end(), pred);

    str.erase(new_end, str.end());

    auto pos = str.find('#');

    if (pos != std::string::npos && pos > 0)
    {
        str.erase(pos);
    }

    return str;
}

Tstring &CFunctions::trim(std::string &s) {
    return ltrim(rtrim(s));
}

bool CFunctions::is_integer(const Tstring &text) {
    if ( text.empty() )
        return false ;

    if ( !(std::isdigit(text[0]) && text[0] != '-' && text[0] != '+') )
        return false ;

    for ( unsigned i=1 ;  i<text.size();  ++i )
        if ( !std::isdigit(text[i]) )
            return false ;

    return true ;
}

bool CFunctions::is_float(const Tstring &text) {
    if (text.empty())
        return false;

    if ( !(std::isdigit(text[0]) || text[0] == '-') )
    {
        return false;
    }

    for ( unsigned i=1 ;  i<text.size();  ++i )
    {
        if (!std::isdigit(text[i]) && text[i] != '.' && text[i] != ',')
            return false;
    }

    return true ;
}

/*bool CFunctions::is_floats_equal(const long double &val1, const long double &val2)
{
    if (fabs(val1 - val2) < 0.0000001)
        return true;
    return false;
}*/

Tstring CFunctions::md5_hash(const Tstring &message)
{
    EVP_MD_CTX*   context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_md5();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int  md_len;
    Tstring       output;

    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, message.c_str(), message.length());
    EVP_DigestFinal_ex(context, md_value, &md_len);
    EVP_MD_CTX_free(context);

    output.resize(md_len * 2);
    for (unsigned int i = 0 ; i < md_len ; ++i)
        std::sprintf(&output[i * 2], "%02x", md_value[i]);
    return output;
}

Tstring CFunctions::remove_filename_from_path(Tstring &path)
{
    return path.substr(0, path.find_last_of("\\/"));
}
