#include <cstdlib>
#include <csignal>
#include <memory>
#include "sources/entry-point/server.h"
#include "../libmeasures/measures/CMeasureManager.h"
#include <CMultiLanguage.h>


E_NYM_LANG  current_language;
CMeasureManager *mes_creator;

static std::unique_ptr<CServer> g_server;

extern "C" void onSignal(int)
{
    if (g_server) g_server->stop();
}

int main(int argc, char** argv)
{
    uint16_t port = 40000; // server.md: "если пусто - 40000"
    if (argc > 1)
    {
        int parsed = std::atoi(argv[1]);
        if (parsed > 0 && parsed <= 65535) port = static_cast<uint16_t>(parsed);
    }

    g_server = std::make_unique<CServer>(port);

    mes_creator = new CMeasureManager();

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    g_server->run();

    delete mes_creator;

    return 0;
}
