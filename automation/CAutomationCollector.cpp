//
// Первый приёмник хуков слоя 2 автоматизации.
//

#include "CAutomationCollector.h"
//#include "../include/CCell.h"
#include "NComponent.h"
#include <iostream>

namespace NCore
{
    CCap* CAutomationCollector::resolve_anchor(NComponent *owner, bool is_input, uint16_t cap_index)
    {
        // Резолюция одинакова для CS_FLOW_SPECIFIC и CS_COMPONENT_SCOPED — физический Cap
        // нужен в обоих случаях (см. automation_layer_summary.md, п.5). Разница не здесь,
        // а в том, вправе ли потребитель (будущий bind()) трактовать ИДЕНТИЧНОСТЬ этого Cap
        // как часть смысла сигнала — это решается по cap_semantics на стороне вызывающего.

        return is_input ? owner->input(cap_index) : owner->output(cap_index);
    }

    SCollectedAutomationData CAutomationCollector::collect(CCell *subproject_root)
    {
        SCollectedAutomationData result;

        if (!subproject_root)
            return result;

        for (auto *cell : *subproject_root->get_components())
        {
            auto *component = dynamic_cast<NComponent*>(cell);
            if (!component)
                continue; // не NComponent (например, вложенный подпроект) — пропускаем на этом шаге


            for (auto &sig : component->required_signals())
            {
                SDeclaredSignal declared;
                declared.owner      = component;
                declared.role       = sig;
                declared.anchor_cap = resolve_anchor(component, sig.is_input, sig.cap_index);
                result.signals.push_back(declared);
            }

            for (auto &cmd : component->required_commands())
            {
                SDeclaredCommand declared;
                declared.owner      = component;
                declared.role       = cmd;
                declared.anchor_cap = resolve_anchor(component, cmd.is_input, cmd.cap_index);
                result.commands.push_back(declared);
            }

            for (auto &spec : component->automation())
            {
                result.automations.push_back({component, spec});
            }

            for (auto &constraint : component->design_constraints())
            {
                result.constraints.push_back({component, constraint});
            }

            // result.componentContent.push_back(component_data);
        }

        print(result);

        return result;
    }

    void CAutomationCollector::print(const SCollectedAutomationData &data)
    {
        std::cout << "=== Automation layer-2 declarations ===\n";

        std::cout << "-- Signals (" << data.signals.size() << ") --\n";
        for (auto &s : data.signals)
        {
            std::cout << "  component[" << s.owner->get_id() << "] role=" << s.role.role
                       << " index=" << s.role.index
                       << " cap=" << (s.anchor_cap ? std::to_string(s.anchor_cap->get_id()) : "MISSING")
                       << " semantics=" << (s.role.cap_semantics == CS_FLOW_SPECIFIC ? "FLOW_SPECIFIC" : "COMPONENT_SCOPED")
                       << "\n";
        }

        std::cout << "-- Commands (" << data.commands.size() << ") --\n";
        for (auto &c : data.commands)
        {
            std::cout << "  component[" << c.owner->get_id() << "] role=" << c.role.role
                       << " index=" << c.role.index
                       << " cap=" << (c.anchor_cap ? std::to_string(c.anchor_cap->get_id()) : "MISSING")
                       << " semantics=" << (c.role.cap_semantics == CS_FLOW_SPECIFIC ? "FLOW_SPECIFIC" : "COMPONENT_SCOPED")
                       << "\n";
        }

        std::cout << "-- Automation specs (" << data.automations.size() << ") --\n";
        for (auto &a : data.automations)
        {
            std::cout << "  component[" << a.owner->get_id() << "] trigger=" << a.spec.trigger
                       << " algorithm=" << a.spec.algorithm
                       << " commands=" << a.spec.output_roles.size() << "\n";
        }

        std::cout << "-- Design constraints (" << data.constraints.size() << ") --\n";
        for (auto &d : data.constraints)
        {
            std::cout << "  component[" << d.owner->get_id() << "] type=" << d.spec.type
                       << " (" << d.spec.display_name << ")\n";
        }
    }
}
