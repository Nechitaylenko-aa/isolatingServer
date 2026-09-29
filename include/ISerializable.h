//
// Created by artem on 03.07.26.
//

#ifndef NYM_PROJECT_ISERIALIZABLE_H
#define NYM_PROJECT_ISERIALIZABLE_H

#include "CContainer.h"
//#include "serial_interfaces.h"

enum EObjectType {
    OT_ElectricTor,
    OT_GeneralTor,
    OT_BaseProject,
    OT_Subproject,
    OT_Cell,
    OT_Component,
    OT_Visual,
    OT_Simple
};

class  ISerializable
{
public:
    virtual ~ISerializable() = default;
    virtual	bool     set_parameters(CContainer &container) = 0;
    virtual	void     get_parameters(CContainer & container) = 0;
    [[nodiscard]] virtual Tstring  get_description() const = 0;
    [[nodiscard]] virtual EObjectType   get_type() const = 0;
    [[nodiscard]] virtual NCore::TComponentType get_subtype() const = 0;
};

#endif //NYM_PROJECT_ISERIALIZABLE_H
