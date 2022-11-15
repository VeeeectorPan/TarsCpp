#include "GameServiceImp.h"
#include "servant/Application.h"

using namespace std;

//////////////////////////////////////////////////////
void GameServiceImp::initialize()
{
    //initialize servant here:
    //...
}

//////////////////////////////////////////////////////
void GameServiceImp::destroy()
{
    //destroy servant here:
    //...
}

int GameServiceImp::testHello(const string &req, string &rsp, tars::TarsCurrentPtr current) {
    rsp = "Hello " + req + "!";
    return 0;
}

