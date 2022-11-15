#ifndef _GameServiceImp_H_
#define _GameServiceImp_H_

#include "servant/Application.h"
#include "GameService.h"

/**
 *
 *
 */
class GameServiceImp : public XG::GameService
{
public:
    /**
     *
     */
    virtual ~GameServiceImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    /**
     *
     */
    int testHello(const string& req,string& rsp, tars::TarsCurrentPtr current) override;
};
/////////////////////////////////////////////////////
#endif
