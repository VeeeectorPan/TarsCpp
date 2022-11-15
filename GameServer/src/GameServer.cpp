#include "GameServer.h"
#include "GameServiceImp.h"

using namespace std;

GameServer g_app;

/////////////////////////////////////////////////////////////////
void
GameServer::initialize()
{
    //initialize application here:
    //...

    addServant<GameServiceImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".GameServiceObj");

    addConfig("GameServer.conf");
    TC_Config conf;
    conf.parseFile(ServerConfig::BasePath + "GameServer.conf");
    TLOG_DEBUG("out: " << conf.get("/root<message>"));
}
/////////////////////////////////////////////////////////////////
void
GameServer::destroyApp()
{
    //destroy application here:
    //...
}
/////////////////////////////////////////////////////////////////
int
main(int argc, char* argv[])
{
    try
    {
        g_app.main(argc, argv);
        g_app.waitForShutdown();
    }
    catch (std::exception& e)
    {
        cerr << "std::exception:" << e.what() << std::endl;
    }
    catch (...)
    {
        cerr << "unknown exception." << std::endl;
    }
    return -1;
}
/////////////////////////////////////////////////////////////////
