//
// Created by artem on 24.08.26.
//

#ifndef NYM_PROJECT_CTECHNOLOGYBUILDER_H
#define NYM_PROJECT_CTECHNOLOGYBUILDER_H

#include <core-types.h>
#include <functional>

namespace NCore
{
    class CCell;

    class CTechnologyBuilder
    {
    public:

        /** @brief создаёt count одинаковых веток через branch_factory, разводит их параллельно между входной и выходной
         * кепкой owner (owner->input(0)/output(0))
         * @attention owner returns branches as internal content (tubes inside to)*/
        static std::vector<CCell*> build_parallel_branches(
                CCell *owner,
                uint16_t count,
                const std::function<CCell*(CCell*)> &branch_factory);
    };
}

#endif //NYM_PROJECT_CTECHNOLOGYBUILDER_H
