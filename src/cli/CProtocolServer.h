/**
 * @file CProtocolServer.h
 * @brief Протокольный диспетчер агента: JSON-RPC поверх stdio (§41, §89.4).
 *
 * Отделяет транспорт (JSON-строки) от ядра (CAgentApi): принимает JSON-запрос,
 * маппит method/params в SApiRequest, выполняет и сериализует ответ в JSON.
 * Ядро не знает про транспорт; плагин работает только с JSON.
 */

#ifndef FCPP_AGENT_CPROTOCOLSERVER_H
#define FCPP_AGENT_CPROTOCOLSERVER_H

#include <iosfwd>

#include "core/CAgentApi.h"
#include "fcpp-agent/IAgentApi.h"

namespace fcpp
{

/**
 * @brief Сервер протокола (JSON-RPC поверх потоков).
 */
class CProtocolServer
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Менеджер конфигурации (должен пережить сервер).
     * @param input Входной поток (JSON-запросы, по одному в строке).
     * @param output Выходной поток (JSON-ответы).
     */
    CProtocolServer(const CConfigManager& config_manager,
                    std::istream& input,
                    std::ostream& output);

    /**
     * @brief Деструктор.
     */
    ~CProtocolServer() = default;

    /**
     * @brief Обработать один JSON-запрос.
     *
     * Разбирает строку, выполняет метод через CAgentApi и печатает JSON-ответ.
     * Ошибка разбора не прерывает цикл.
     * @param request_line Строка JSON-запроса.
     */
    void handle_line(const std::string& request_line);

    /**
     * @brief Основной цикл: читать строки stdin до EOF.
     * @return Код возврата процесса.
     */
    int run();

    /**
     * @brief Открыть проект до начала цикла (serve --project <root>).
     * @param project_root Корень проекта.
     * @return true при успехе.
     */
    bool open_project(const std::string& project_root);

    /**
     * @brief Разобрать JSON-запрос в типизированный SApiRequest.
     *
     * Экспонируется для тестов (без транспорта).
     * @param document Разобранный JSON.
     * @param out_request Выходной запрос.
     * @param out_id Выходной id (повторяется в ответе).
     * @param out_error Сообщение об ошибке разбора.
     * @return true при успехе.
     */
    static bool parse_request(const std::string& text,
                              SApiRequest& out_request,
                              std::string& out_id,
                              std::string& out_error);

    /**
     * @brief Сериализовать ответ в JSON-строку.
     * @param id Идентификатор запроса.
     * @param response Ответ ядра.
     * @return JSON-строка.
     */
    static std::string serialize_response(const std::string& id, const SApiResponse& response);

private:
    CAgentApi m_api;         ///< Ядро API.
    std::istream& m_input;   ///< Вход.
    std::ostream& m_output;  ///< Выход.
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROTOCOLSERVER_H
