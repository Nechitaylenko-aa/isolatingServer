//
// Created by artem on 14.08.26.
//

#include "CAcceptor.h"

#include <cerrno>
#include <cstring>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../logger-common/logger.h"


CAcceptor::CAcceptor(uint16_t port, std::vector<CWorker*> workers)
        : m_port(port)
          , m_workers(std::move(workers))
{
}

size_t CAcceptor::pickWorker()
{
    size_t idx = m_rrCounter.fetch_add(1, std::memory_order_relaxed);
    return idx % m_workers.size();
}

void CAcceptor::run()
{
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
    {
        CLogger::instance().critical("CAcceptor", "socket() failed: {}", std::strerror(errno));
        return;
    }

    int opt = 1;
    ::setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_port);

    if (::bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        CLogger::instance().critical("CAcceptor", "bind() failed on port {}: {}", m_port, std::strerror(errno));
        ::close(m_listenFd);
        m_listenFd = -1;
        return;
    }

    if (::listen(m_listenFd, SOMAXCONN) < 0)
    {
        CLogger::instance().critical("CAcceptor", "listen() failed: {}", std::strerror(errno));
        ::close(m_listenFd);
        m_listenFd = -1;
        return;
    }

    CLogger::instance().info("CAcceptor", "listening on port {}", m_port);
    m_running.store(true, std::memory_order_release);

    while (m_running.load(std::memory_order_acquire))
    {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        int fd = ::accept(m_listenFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (fd < 0)
        {
            if (!m_running.load(std::memory_order_acquire))
            {
                break;
            }
            CLogger::instance().warn("CAcceptor", "accept() failed: {}", std::strerror(errno));
            continue;
        }

        // КРИТИЧЕСКАЯ ПРАВКА №2: приём безусловный, здоровье БД здесь не проверяется.
        size_t idx = pickWorker();
        m_workers[idx]->assignConnection(fd);
    }
}

void CAcceptor::stop()
{
    m_running.store(false, std::memory_order_release);
    if (m_listenFd >= 0)
    {
        ::shutdown(m_listenFd, SHUT_RDWR);
        ::close(m_listenFd);
        m_listenFd = -1;
    }
}
