/**
 * @file ILogger.h
 * @brief Logging interface for structured events.
 */
#ifndef FCPP_AGENT_ILOGGER_H
#define FCPP_AGENT_ILOGGER_H

#include <string>
#include "fcpp-agent/SLogEvent.h"

namespace fcpp
{
enum class ELogLevel { Error, Warn, Info, Debug, Trace, Count };

class ILogger
{
public:
    virtual ~ILogger() = default;
    virtual void log(ELogLevel level, const std::string& message) = 0;
    virtual void log(ELogLevel level, const SLogEvent& event)
    {
        log(level, event.m_name);
    }
    static const char* level_name(ELogLevel level);
};
} // namespace fcpp
#endif
