#ifndef _GameServer_H_
#define _GameServer_H_

#include <iostream>
#include "servant/Application.h"

using namespace tars;

/**
 *
 **/
class GameServer : public Application
{
public:
    /**
     *
     **/
    virtual ~GameServer() {};

    /**
     *
     **/
    virtual void initialize();

    /**
     *
     **/
    virtual void destroyApp();
};

extern GameServer g_app;

////////////////////////////////////////////
#endif
