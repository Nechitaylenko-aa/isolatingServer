//
// Created by artem on 5/23/25.
//

#ifndef NYM_PROJECT_CCONTAINER_H
#define NYM_PROJECT_CCONTAINER_H

//#include "measure_EXPORTS.h"
#include <typeindex>
#include "core-types.h"
#include "CBodyParam.h"


// ============ SUPPORTING STRUCTURES ============

struct MemberInfo {
    void* ptr{nullptr};
    std::type_index type;
    Tstring name;
    bool is_pointer;
};

struct VectorInfo {
    void* ptr;
    Tstring name;
    size_t size;
    std::type_index element_type;
    bool is_pointer_type; // true if vector contains pointers
};

struct DynamicVectorInfo {
    void* ptr;
    std::type_index element_type;
    std::string name;
    bool is_pointer_type; // true if vector contains pointers
};

struct ParameterInfo {
    CParameter* parameter;
    std::string measure_unit_id; // Store unit identifier for restoration
    Tstring parameter_name;
};

class CLimits;

/**@brief When object is about to be serialized, Its stores its members in this container, then this container is going
 * to serializer and saving members in the files */

class  CContainer {
public:
    CContainer()  =  default;
    ~CContainer();

    // Delete copy/move operations
    CContainer(const         CContainer&);
    CContainer&              operator              =             (const         CContainer&);
    CContainer(CContainer&&) noexcept;
    CContainer&              operator              =             (CContainer&&) noexcept;
    [[nodiscard]]            Tstring               object_data() const;
    void                     set_object_data(const Tstring       &              object_data);
    //  ============  BASIC TYPES  ===========
    // const Tstring& name below for serializing to XML/Json
    void    add_member(bool&      var,          const Tstring& name =    "");
    void    add_member(int8_t&    var,          const Tstring& name =    "");
    void    add_member(int16_t&   var,          const Tstring& name =    "");
    void    add_member(int32_t&   var,          const Tstring& name =    "");
    void    add_member(int64_t&   var,          const Tstring& name =    "");
    void    add_member(uint8_t&   var,          const Tstring& name =    "");
    void    add_member(uint16_t&  var,          const Tstring& name =    "");
    void    add_member(uint32_t&  var,          const Tstring& name =    "");
    void    add_member(uint64_t&  var,          const Tstring& name =    "");
    void    add_member(float&     var,          const Tstring& name =    "");
    void    add_member(double&    var,          const Tstring& name =    "");
    void    add_member(Tstring&   var,          const Tstring& name =    "");


    // ============ COMPLEX TYPES ============
    void add_member(CParameter&  var,           const Tstring& name  = "");
    void add_member(CBodyParam&  var,           const Tstring& name  = "");
    void add_member(CLimits&     var,           const Tstring& name  = "");

    // ============ STREAM INTERFACE ============
    template<typename T>
    CContainer& operator<<(T& var) {
        add_member(var);
        return *this;
    }

    // Add vector support
    template<typename T>
    void add_member(std::vector<T>& vec, const Tstring & name = "")
    {
        m_vectors.emplace_back(VectorInfo
        {
                .ptr          = &vec,
                .name         = name,
                .size         = vec.size(),
                .element_type = typeid(T),
        });
    }

    // Add support for variadic struct array via flags
    bool is_deserialize() const;
    void set_deserialize(bool deserialize);
    uint8_t total_times() const;
    void    set_total_times(uint8_t times);
    uint8_t current_time() const;
    void    set_current_time(uint8_t time);
    bool is_contains_variadik() const;
    void set_variadik(bool variadik);

    // Add support for dynamic arrays (unknown size)
    template<typename T>
    void add_dynamic_member(std::vector<T>& vec, const Tstring & name = "")
    {
        m_dynamic_vectors.emplace_back(DynamicVectorInfo
        {
                .ptr = &vec,
                .element_type = typeid(T),
                .name = name
        });
    }

    // ============ DATA ACCESS ============
    [[nodiscard]] const auto& vectors() const { return m_vectors; }
    [[nodiscard]] const auto& dynamic_vectors() const { return m_dynamic_vectors; }
    [[nodiscard]] const auto& members() const { return m_members; }

private:


    std :: vector<MemberInfo>       m_members;
    std :: string                   m_object_data;
    std :: vector<VectorInfo>       m_vectors;
    std::vector<DynamicVectorInfo>  m_dynamic_vectors;

    bool m_is_deserialize{false};
    bool m_is_second{false};
    bool m_variadik{false};
    uint8_t m_total_times{0};
    uint8_t m_current_time{0};

    static void swap(CContainer& first, CContainer& second) noexcept;
};




#endif //NYM_PROJECT_CCONTAINER_H
