//
// Created by artem on 04.04.24.
//

#ifndef NYM_PROJECT_CCONDUCTOR_H
#define NYM_PROJECT_CCONDUCTOR_H

#include <map>
#include "../../interfaces/include/core-types.h"
#include "CCap.h"
#include "CConductCalculator.h"

namespace NCore
{
    class CCell;
    class CCap;
    class CConductCalculator;

    class CConductor
    {
        friend class CCell;
    public:
        CConductor() = delete;
        CConductor(const CConductor &) = delete;
        CConductor(CConductor &&) = delete;
        explicit CConductor(CCell *owner);
        virtual ~CConductor();

        [[nodiscard]] Tuint64 get_id() const;
        void                  set_id(const Tuint64 & id) { m_id = id; }

        // ========== caps management ============================
        [[nodiscard]] bool    add_cap(CCap *cap);
        CCap*   remove_cap(CCap *cap);
        [[nodiscard]] uint16_t     caps_amount() const;
        CCap*   cap(const uint16_t & index);
        void    clear_caps(); //!< not delete caps from memory, just disconnecting

        /**
         * @brief перемещает все кепки в указанную трубу и переназначает трубу в самих кепках
         * @param dst_tube
         */
        void    move_caps_to_tube(CConductor *dst_tube);
        CCell * get_owner();

        [[nodiscard]] bool  is_connection_ok() const;
        COperatingBody *  put_ob(const COperatingBody *ob, CCap *sender);

        void print(const std::string& indent = "");
    protected:



    private:
        friend  CConductCalculator;

        CConductCalculator  * m_calculator;

        Tuint64           m_id{0};
        E_BODY_TYPE       m_body_type;
        CCell           * m_owner;

        std::vector<CCap*>  * m_caps;           //!< stores pointers to CCap, not instances
        std::vector<Tsize>    d_caps_id;        //!< сериализуются ID вместо кепок


        bool  is_existing_cap(CCap *cap);   //!< true if existing
        bool  check_correct();              //!< check that tube has income end output caps
        std::vector<Tsize>  connected_caps_id();
    };

}
#endif //NYM_PROJECT_CCONDUCTOR_H
