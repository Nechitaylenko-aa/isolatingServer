//
// Created by artem on 04.04.24.
//

#ifndef NYM_PROJECT_CCAP_H
#define NYM_PROJECT_CCAP_H

#include "core-types.h"
#include "COperatingBody.h"
#include <functional>

class CContainer;

namespace NCore
{
    class CCell;
    class CConductor;

    /** @brief Наконечник любого CCell или все CCell имеют CCap (два и больше) */
    class CCap
    {
    public:
        CCap() = delete;
        CCap(const CCap &) = delete;
        CCap(CCap &&) = delete;
        CCap(CCell *owner,
             const E_BODY_TYPE &body_type,
             const E_CAP_DIRECTION &external_direction,
             const E_CONTOUR_TYPE &contour_type);

        virtual ~CCap();

        void    set_contour(const E_CONTOUR_TYPE & contour);

        void    set_id(const Tuint64 &id);
        [[nodiscard]] Tuint64 get_id() const;

        CCell   * get_owner();

        [[nodiscard]] E_CONTOUR_TYPE    get_contour() const;

        E_CAP_DIRECTION   get_direction(CCell *watcher = nullptr);

        void    set_conductor(CConductor *conductor, CCell *inquirer);
        CConductor*  get_conductor(CCell *inquirer);

        [[nodiscard]] E_BODY_TYPE     get_body_type() const;

        /** @brief this callback for base outputs of the project cell only */
        void set_callbackOnFlowComplete(std::function<void(NCore::CCap*)> handler);

        /**
         * @brief injecting OB from outer source, for example - test or run schema
         * @param[in] ob COperatingBody
         * @return излишки
         */
        COperatingBody* inject_ob(COperatingBody *ob);

        COperatingBody* put_ob(const COperatingBody *ob, CConductor *sender);
        COperatingBody* get_ob();

        void  get_parameters(CContainer & container);
        void  set_parameters(const CContainer & container);

        // for tunning
        void set_inner_conductor(CConductor *conductor);
        void set_outer_conductor(CConductor *conductor);


    protected:



    private:
        CCell           * m_owner;
        CConductor      * m_external_conductor{nullptr};
        CConductor      * m_internal_conductor{nullptr};
        COperatingBody  * m_operating_body;
        COperatingBody  * m_finish_body{nullptr};
        E_CONTOUR_TYPE    m_contour;
        E_BODY_TYPE       m_body_type;
        E_CAP_DIRECTION   m_external_direction;
        E_CAP_DIRECTION   m_internal_direction;
        Tuint64           m_id{0};

        std::function<void(NCore::CCap*)> m_cbFlowComplete;

        uint8_t           ser_contour;
        uint8_t           ser_body_type;
        uint8_t           ser_extern_dir;
        uint8_t           ser_intern_dir;
        uint32_t          ser_ext_tube_id;
        uint32_t          ser_intern_tube_id;
        Tuint64           ser_owner_id;

        bool set_ob(const COperatingBody *p_body);
    };

}
#endif //NYM_PROJECT_CCAP_H
