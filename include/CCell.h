//
// Created by artem on 04.04.24.
//

#ifndef NYM_PROJECT_CCELL_H
#define NYM_PROJECT_CCELL_H

#include "core-types.h"
#include "CCap.h"
#include "CConductor.h"
#include "CInfoBus.h"
#include "COperatingBody.h"
#include "serial_interfaces.h"


class CSubProject;


namespace NCore
{
    class CComponent;
    typedef struct s_body_set
    {
        CCap * cap_in{nullptr};
        CCap * cap_out{nullptr};
        long  body_id{-1};
        //bool  is_belongs_cell{false};
    }SBodySet;

    typedef struct s_connection{
        CConductor *    result_conductor{nullptr};
        CConductor *    trash{nullptr};
    }SConnectingResult;

    /** @brief basic class of the whole system. See  libUML.graphml */
    class CCell : public ISerializable
    {
        friend class CConductor;    //!< строго для получения ID трубами. может объявить дружественную функцию?
                                    //!< или трубам выдавать ID?
    public:
        CCell(const CCell &&) = delete;
        CCell(const CCell &) = delete;
        explicit CCell(CCell *owner = nullptr);
        ~CCell() override;

        /// general data and methods

        [[nodiscard]] Tuint64 get_id() const;

        void    set_project_type(const E_PROJECT_TYPE &project_type);
        [[nodiscard]] E_PROJECT_TYPE    get_project_type() const;
        CInfoBus    * info_bus();

        /** @brief применимо только для компонентов */
        [[nodiscard]] TComponentType  get_component_type() const;

        /**@brief может изменяться только у главной клетки при интеграции проекта во внешний, более высокий по иерархии */
        void    set_owner(CCell *owner);
        CCell*  get_owner();

        CCell*  remove_component(CCell *comp);
        std::vector<NCore::CCell *> *   get_components();
        std::vector<NCore::CConductor*> * get_tubes();

        /// ------------------------------------------------------------------------------------------------------------
        /// schematic methods
        /// ------------------------------------------------------------------------------------------------------------

        /** @brief insert component to the tube after the first found output CCap. */
        // CConductor*     insert_component(); //!< TODO: продумать
        SConnectingResult    connect_caps(CCap *cap_src, CCap *cap_dst);

        CConductor* remove_tube(CConductor *conductor);
        [[nodiscard]] CCell*  add_component(CCell *component);
        CConductor* add_tube(CConductor * conductor);

        /**
         * @brief Используется при работе сети. Когда рабочее тело передается по трубам от компонента к компоненту через
         * наконечники.
         * @details Вызывается, если у кепки внутри компонента нет трубы. Иначе компонент не в курсе))
         * @param body [in] IOperatingBody* собственно рабочее тело для передачи/обработки
         * @param sender [in] CCap* передаётся для того, что бы определить противоположный наконечник,
         * а не вернуть РТ обратно
         * @return IOperatingBody* возвратка, после долгого пути, на много больше данной клетки. Цепочка на стеке может быть
         * очень большой. Проблема размера стека в будущем
         */
        virtual COperatingBody* put_ob(COperatingBody *body, CCap* sender);

        /**
        * @brief Используется при настройке либо главной клетки, либо в конструкторах компонентов: добавляет  пару
        * наконечников (входной и выходной), Всё созданное помещает в соответствующее хранилище и возвращает
         * в выходной кортеж.
        * @param[in] ob (ваше, не в юрисдикции CCell)
        * @return std::tuple\<CCap*, CCap*, COperatingBody*> CCap* (input_cap), CCap* (output cap),
        * COperatingBody*  operating body instance, the same as input.
        * @attention рабочее тело alien удаляются там где создано, т.е. на вашей совести
        * @note если предполагается только вход (выход) эти методы 'remove_input' и 'remove_output' могут изъять лишнее
        * (не удалить из памяти)
        */
        std::tuple<CCap*, CCap*, COperatingBody*>    add_ob(COperatingBody *alien, E_CONTOUR_TYPE contour = E_CONTOUR_TYPE::CT_MAIN);
        /** @brief не удаляет из памяти */
        CCap*   remove_input(CCap *input);
        /** @brief не удаляет из памяти */
        CCap*   remove_output(CCap *output);

        /** @brief base working body input-caps */
        std::vector<CCap*>  get_base_inputs();
        /** @brief base working body output-caps */
        std::vector<CCap*>  get_base_outputs();
        /** @brief base working body all caps */
        std::vector<CCap*>  get_base_caps();
        void  clear_base_caps();
        COperatingBody * get_base_ob();

        std::vector<CCap*>   inputs(); //!< get inputs ANY working body and any contour
        std::vector<CCap*>   outputs(); //!< get outputs ANY working body  any contour


        /// специальные методы на вырост (т.е. не сейчас). нет эти методы ДОЛЖНЫ быть внешними, но оставлю к
        /// ак напоминалку.

        /// интеграция младшего по иерархии проекта в старший. Возвращать должен два вектора незадействованных кепок
        /// со стороны высшего проекта и подпроекта (в идеале они пустые)
        // bool integrate_subproject(CCell *cell);

        /// экстракция ТЗ из пустого компонент, пока технология неясна. На будущее
        //bool  extract_tor_for_cell(CCell *cell);

        /// выделение интегрированного проекта в самостоятельный, или иначе из подпроекта сделать самостоятельный
        /// проект. Похоже это одно и тоже с предыдущим
        //bool  move_subproject_to_standalone(CCell *cell);

        /** @brief When project is loading this method updates OB in inputs and vector*/
        void  loadingUpdateOperationBody(COperatingBody *ob, uint ins, uint outs);

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] EObjectType   get_type() const override { return OT_Cell; }
        [[nodiscard]] TComponentType get_subtype() const override {return {};}
        [[nodiscard]] Tstring  get_description() const override;

        void  clear_base_connections();

        //!< debug method. Print current schema in console
        void     printCell(int depth = 0);
        /** @brief проверка на одинаковые ID
 * @returns true - ok, false - нашлись одинаковые ID */
        bool    debug_check_id();
        ECellRole   cell_role() const { return m_cell_role; }

    protected:

        /** @brief т.к. главная клетка (проектная) отвечает за раздачу ID всем участникам, то наследники (компоненты)
         * выполняют эту функцию в конце (после инициализации кепок) своего конструктора обязательно, тут идет
         * раздача ID, трубы получают ID по отдельному распоряжению */
        void setup_id(CConductor *cond = nullptr);

        /** @brief может изменяться только у главной клетки при интеграции проекта во внешний, более высокий по иерархии */
        void    set_id(const Tuint64 &id) {} // try to make setup ID automatically

        void            clear_cell();   //!< for components deserialize
        void printCaps(const std::string& indent);
        void printCapInfo(CCap* cap, const std::string& indent, const std::string& direction);
        void printTubes(const std::string& indent);


    protected:
        Tuint64   m_id{0};
        CInfoBus* m_info_bus;
        TComponentType  m_successor_component_type{};
        ECellRole       m_cell_role;
        Tstring         m_successor_schematic_name;



        friend CSubProject;


        Tuint64   m_last_id{0};
        Tuint64 * p_last_id;
        CCell   * m_owner;

        E_PROJECT_TYPE    m_project_type{pt_undef};

        std::vector<CCap*>          * m_additional_caps;
        std::vector<CConductor*>    * m_tubes;
        std::vector<CCell*>         * m_components;
        std::vector<SBodySet*>      * m_bodies_caps;

        // сериализация неявных данных
        uint8_t           m_ser_project_type;
        uint16_t          m_owner_id;

        std::vector<COperatingBody*>  m_bodies;

        uint8_t     m_caps_amount{0};

    private:
        void  clean_cell();
        CCap *find_cap(const E_BODY_TYPE &body_type, const E_CAP_DIRECTION &direction);
        bool  is_mine_cap(CCap *cap);
        COperatingBody * get_body_instance(const E_BODY_TYPE &type);
        CCap *get_cap_by_id(const Tsize &id);

        void serialize_caps(CContainer &container);

        void deserialize_caps(CContainer &container);

        void remove_from_tube(CCap *cap);
    };

}
#endif //NYM_PROJECT_CCELL_H
