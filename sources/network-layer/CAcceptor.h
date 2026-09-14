#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "CWorker.h"

class CAcceptor
{
public:
    CAcceptor(uint16_t port, std::vector<CWorker*> workers);

    void run();
    void stop();

private:
    size_t pickWorker();

    uint16_t                m_port;
    std::vector<CWorker*>   m_workers;
    std::atomic<size_t>     m_rrCounter{0};
    int                     m_listenFd{-1};
    std::atomic<bool>       m_running{false};
};
