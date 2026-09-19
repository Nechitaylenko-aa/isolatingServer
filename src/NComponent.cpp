//
// Created by artem on 15.11.25.
//

#include "../include/NComponent.h"
//#include "CEquipment.h"

NCore::NComponent::NComponent(IGeneralTor *tor, NCore::CCell *owner, TComponentType type)
    : CCell(owner)
    , m_generalTor(tor)
{
    m_successor_component_type = type;
}

NCore::NComponent::~NComponent()
= default;

uint16_t NCore::NComponent::countIn() const
{
    return m_inputs.size();
}

uint16_t NCore::NComponent::countOut() const
{
    return m_outputs.size();
}

NCore::CCap *NCore::NComponent::input(const uint16_t &index)
{
    if (index >= m_inputs.size())
    {
        return nullptr;
    }
    return m_inputs.at(index);
}

NCore::CCap *NCore::NComponent::output(const uint16_t &index)
{
    if (index >= m_outputs.size())
        return nullptr;
    return m_outputs.at(index);
}

std::vector<CParameter> &NCore::NComponent::parameters()
{
    return m_parameters;
}

Tstring NCore::NComponent::get_imageSource() const
{
    return m_imgSource;
}

EObjectType NCore::NComponent::get_type() const
{
    return OT_Component;
}

NCore::TComponentType NCore::NComponent::get_subtype() const
{
    return m_successor_component_type;
}

void NCore::NComponent::set_schematicName(const Tstring &schName)
{
    m_schName = schName;
}

Tstring NCore::NComponent::schematicName() const
{
    return m_schName;
}

void NCore::NComponent::set_callbackUpdateColor(std::function<void()> handler)
{
    m_cbUpdateColor = std::move(handler);
}

Tstring NCore::NComponent::warnings() const
{
    return m_warning_message;
}

EComponentState NCore::NComponent::component_state() const
{
    return m_componentState;
}

EComponentError NCore::NComponent::component_error() const
{
    return m_error;
}

void NCore::NComponent::set_warnMessage(const Tstring &msg)
{
    m_warning_message = msg;
}

void NCore::NComponent::set_error(EComponentError error)
{
    m_error = error;
    if (m_error == EComponentError::ESE_ERROR || m_error == EComponentError::ECE_CRITICAL)
    {
        m_componentState = EComponentState::ECS_ERROR;
    }

    if (m_cbUpdateColor)
    {
        m_cbUpdateColor();
    }
}

void NCore::NComponent::set_state(EComponentState state)
{
    m_componentState = state;
}

void NCore::NComponent::set_state_signal(const NCore::SSignalRole &role, uint16_t index, float value)
{
    m_state_signals[{ role.role, index }] = value;
}

float NCore::NComponent::read_state_signal(const NCore::SSignalRole &role, uint16_t index) const
{
    auto it = m_state_signals.find({ role.role, index });
    return it != m_state_signals.end() ? it->second : -1.0f;
}
