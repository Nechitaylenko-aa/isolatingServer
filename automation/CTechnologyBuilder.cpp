
#include "CTechnologyBuilder.h"
#include <CConductor.h>
#include <NComponent.h>

namespace NCore
{

    std::vector<CCell*> CTechnologyBuilder::build_parallel_branches(CCell *owner, uint16_t count,
                                                                    const std::function<CCell*(CCell*)> &branch_factory)
    {
        std::vector<CCell*> branches;

        // очистка компонента
        {
            auto *comps = owner->get_components();
            auto *tubes = owner->get_tubes();

            if (!comps->empty())
            {
                for (auto &comp: *comps)
                    delete comp;
                comps->clear();
            }

            if (!tubes->empty())
            {
                for (auto &tube: *tubes)
                    delete tube;
                tubes->clear();
            }
            owner->clear_base_connections();
        }

        CCap * in{nullptr}, *out{nullptr};

        // pump station has 2 caps in/out
        for (auto & cap : owner->get_base_caps())
        {
            if (cap->get_direction(owner) == CD_INPUT) // param: owner is the same as nullptr in this case
                in = cap;
            else
                out = cap;
        }

        assert(in && out);

        for (uint16_t i = 0; i < count; ++i)
        {
            auto *branch = dynamic_cast<NComponent*>(branch_factory(owner));

            CCap * brIn = branch->input(0);
            CCap * brOut = branch->output(0);

            SConnectingResult result = owner->connect_caps(in, brIn);
            assert(result.result_conductor != nullptr);
            result = owner->connect_caps(out, brOut);
            assert(result.result_conductor != nullptr);

            branches.push_back(branch);
        }
        return branches;
    }
}
