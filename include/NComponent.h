//
// Created by artem on 15.11.25.
//

#ifndef NYM_PROJECT_NCOMPONENT_H
#define NYM_PROJECT_NCOMPONENT_H

#include "CCell.h"
#include "../automation/CAutomationAtoms.h"
#include "../automation/IInstallationTemplate.h"


enum EComponentError : uint8_t
{
    ECE_NORM,
    ECE_CRITICAL,
    ESE_ERROR,
    ECE_WARNING,
    ESE_ACTUATOR
};



enum EComponentState
{
    ECS_DEFAULT,
    ECS_CONNECTED_NO_EQUIP,
    ECS_CONNECTED_EQUIP_IS_ONE,
    ECS_CONNECTED_EQUIP_CHOICE,
    ECS_ERROR
};

namespace equip {
    class CEquipment;
}

namespace NCore
{

    class NComponent : public CCell
    {
    public:
        NComponent(IGeneralTor *tor, CCell *owner, TComponentType type);
        ~NComponent() override;

        [[nodiscard]] uint16_t  countIn() const;
        [[nodiscard]] uint16_t  countOut() const;
        CCap*     input(const uint16_t &index);
        CCap*     output(const uint16_t &index);
        std::vector<CParameter> & parameters();
        [[nodiscard]] Tstring get_imageSource() const;
        [[nodiscard]] EObjectType get_type() const final;
        [[nodiscard]] TComponentType get_subtype() const final;
        void  set_schematicName(const Tstring & schName);
        [[nodiscard]] Tstring  schematicName() const;

        virtual void set_equipment(equip::CEquipment *equip) = 0;
        virtual void set_equipmentProxy(std::vector<SEquipLight> && items) = 0;
        void set_callbackUpdateColor(std::function<void()> handler);
        [[nodiscard]] Tstring  warnings() const;
        [[nodiscard]] EComponentState  component_state() const;
        [[nodiscard]] EComponentError  component_error() const;
        virtual void rebuild_internal_topology() {}

        // set from shadows while calculating
        void set_warnMessage(const Tstring & msg);
        void set_error(EComponentError error);

        void set_state(EComponentState state);

        // automation
        [[nodiscard]] virtual std::vector<SSignalRole>           required_signals()      const  = 0;
        [[nodiscard]] virtual std::vector<SCommandRole>          required_commands()     const  = 0;
        [[nodiscard]] virtual std::vector<SAutomationSpec>       automation()            const  = 0;
        [[nodiscard]] virtual std::vector<SDesignConstraintSpec> design_constraints()    const  = 0;
        [[nodiscard]] virtual bool is_flow_boundary() const { return false; }
        [[nodiscard]] virtual std::vector<SSignalRole> readable_state_roles() const { return {}; }
        // automation possible variables (e.g. moto-hours for pump station)
        [[nodiscard]] virtual std::vector<SVariableBehaviorSpec>   variable_behaviors()     const { return {}; }
        // email box
        void set_state_signal(const SSignalRole &role, uint16_t index, float value);
        [[nodiscard]] float read_state_signal(const SSignalRole &role, uint16_t index) const;


    protected:

        uint32_t        m_id_equip{0};
        SEquipLight     m_equipProxy;

        std::vector<CCap*> m_inputs;  // not owning
        std::vector<CCap*> m_outputs; // not owning
        IGeneralTor     * m_generalTor;   // not owning
        COperatingBody  * m_body{nullptr}; // not owning

        std::vector<SEquipLight> m_equipmentChoice; // не уверен, возможно просто параметры в согласованном порядке и id оборуд.

        std::vector<CParameter>  m_parameters;

        Tstring     m_imgSource;
        Tstring     m_schName;
        Tstring     m_descript;
        Tstring     m_warning_message;


        // для сериализации
        uint16_t    m_ser_comp_type{0},
                    m_ser_project_type{0};
        Tuint64 id_in{0}, id_out{0};

        EComponentState m_componentState{EComponentState::ECS_DEFAULT};
        EComponentError m_error{EComponentError::ECE_NORM};

        // для GUI
        std::function<void()>   m_cbUpdateColor;

        // для автоматизации (почтовый ящик)
        std::map<std::pair<ESignalRole, uint16_t>, float> m_state_signals;




    private:

    };
}


#endif //NYM_PROJECT_NCOMPONENT_H
