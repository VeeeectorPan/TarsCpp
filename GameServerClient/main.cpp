#include "servant/Communicator.h"
#include "servant/ServantProxy.h"
#include "GameService.h"
#include "iostream"

static const string GameServiceObj = "XG.GameServer.GameServiceObj";


struct HelloCB : XG::GameServicePrxCallback{
    void callback_testHello(int iRet, const string& sRsp) override {
        cout << "Call Successfully: " << sRsp << endl;
    }

    void callback_testHello_exception(tars::Int32 iRet) override {
        cout << "callback exception: " << iRet << endl;
    }
};

int main(int argc, char** argv){
    CommunicatorPtr comm = new Communicator();
    try{
        TC_Config conf;
        conf.parseFile("/root/src/TarsCpp/GameServerClient/config.conf");
        comm->setProperty(conf);

        auto pGameServicePrx = comm->stringToProxy<XG::GameServicePrx>(GameServiceObj);

        XG::GameServicePrxCallbackPtr cb = new HelloCB;
        pGameServicePrx->async_testHello(cb, "vecpan");
        cout << "call testHello async!" << endl;
        sleep(1);
    }catch (exception &e){
        cerr << "error: " << e.what() << endl;
    }catch(...){
        cerr << "Unknown Error!" << endl;
    }
    return 0;
}
