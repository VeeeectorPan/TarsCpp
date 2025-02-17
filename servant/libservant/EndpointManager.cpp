﻿/**
 * Tencent is pleased to support the open source community by making Tars available.
 *
 * Copyright (C) 2016THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except 
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed 
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the 
 * specific language governing permissions and limitations under the License.
 */
#include "util/tc_port.h"
#include "servant/EndpointManager.h"
#include "servant/ObjectProxy.h"
#include "servant/RemoteLogger.h"
#include "servant/AppCache.h"
#include "servant/Application.h"
#include "servant/CommunicatorEpoll.h"
#include "servant/StatReport.h"

namespace tars
{
/////////////////////////////////////////////////////////////////////////////
QueryEpBase::QueryEpBase(Communicator * pComm, bool bFirstNetThread,bool bInterfaceReq)
: _communicator(pComm)
, _firstNetThread(bFirstNetThread)
, _interfaceReq(bInterfaceReq)
, _direct(false)
, _objName("")
, _invokeSetId("")
, _locator("")
, _valid(false)
, _weightType(E_LOOP)
, _rootServant(true)
, _requestRegistry(false)
, _requestTimeout(0)
, _timeoutInterval(5*1000)
, _refreshTime(0)
, _refreshInterval(60*1000)
, _activeEmptyInterval(10*1000)
, _failInterval(2*1000)
, _manyFailInterval(30*1000)
, _failTimesLimit(3)
, _failTimes(0)
{
    _refreshInterval = TC_Common::strto<int>(_communicator->getProperty("refresh-endpoint-interval", "60*1000"));

    if(_refreshInterval < 5*1000)
    {
        _refreshInterval = 5 * 1000;
    }
    setNoDelete(true);
}

void QueryEpBase::callback_findObjectById4All(Int32 ret, const vector<EndpointF>& activeEp, const vector<EndpointF>& inactiveEp)
{
    TLOGTARS("[callback_findObjectById4All _objName:" << _objName << "|ret:" << ret
            << ",active:" << activeEp.size()
            << ",inactive:" << inactiveEp.size() << "]" << endl);

    doEndpoints(activeEp,inactiveEp,ret);
}

void QueryEpBase::callback_findObjectById4All_exception(Int32 ret)
{
    TLOGERROR("[callback_findObjectById4All_exception _objName:" << _objName << "|ret:" << ret << "]" << endl);

    doEndpointsExp(ret);
}

void QueryEpBase::callback_findObjectById4Any(Int32 ret, const vector<EndpointF>& activeEp, const vector<EndpointF>& inactiveEp)
{
    TLOGTARS("[callback_findObjectById4Any _objName:" << _objName << "|ret:" << ret
            << ",active:" << activeEp.size()
            << ",inactive:" << inactiveEp.size() << "]" << endl);

    doEndpoints(activeEp,inactiveEp,ret);
}

void QueryEpBase::callback_findObjectById4Any_exception(Int32 ret)
{
    TLOGERROR("[callback_findObjectById4Any_exception _objName:" << _objName << "|ret:" << ret << "]" << endl);

    doEndpointsExp(ret);
}

void QueryEpBase::callback_findObjectByIdInSameGroup(Int32 ret, const vector<EndpointF>& activeEp, const vector<EndpointF>& inactiveEp)
{
    TLOGTARS("[callback_findObjectByIdInSameGroup _objName:" << _objName << "|ret:"<<ret
            << ",active:" << activeEp.size()
               << ",inactive:" << inactiveEp.size() << "]" << endl);

    doEndpoints(activeEp,inactiveEp,ret);
}

void QueryEpBase::callback_findObjectByIdInSameGroup_exception(Int32 ret)
{
    TLOGERROR("[callback_findObjectByIdInSameGroup_exception _objName:" << _objName << "|ret:" << ret << "]" << endl);

    doEndpointsExp(ret);
}

void QueryEpBase::callback_findObjectByIdInSameSet( Int32 ret, const vector<EndpointF> &activeEp, const vector<EndpointF> & inactiveEp)
{
    TLOGTARS("[callback_findObjectByIdInSameSet _objName:" << _objName << "|ret:" << ret
            << ",active:" << activeEp.size()
            << ",inactive:" << inactiveEp.size() << "]" << endl);

    doEndpoints(activeEp,inactiveEp,ret);
}

void QueryEpBase::callback_findObjectByIdInSameSet_exception( Int32 ret)
{
   TLOGERROR("[callback_findObjectByIdInSameSet_exception _objName:" << _objName << "|ret:" << ret << "]" << endl);

    doEndpointsExp(ret);
}

void QueryEpBase::callback_findObjectByIdInSameStation( Int32 ret, const vector<EndpointF> &activeEp, const vector<EndpointF> &inactiveEp)
{
    TLOGTARS("[callback_findObjectByIdInSameStation _objName:" << _objName << "|ret:" << ret
            << ",active:" << activeEp.size()
            << ",inactive:" << inactiveEp.size() << "]" << endl);

    doEndpoints(activeEp,inactiveEp,ret);
}

void QueryEpBase::callback_findObjectByIdInSameStation_exception( Int32 ret)
{
    TLOGERROR("[callback_findObjectByIdInSameStation_exception _objName:" << _objName << "|ret:" << ret << "]" << endl);

    doEndpointsExp(ret);
}

int QueryEpBase::setLocatorPrx(QueryFPrx prx)
{
    _queryFPrx = prx;

    return 0;
}

bool QueryEpBase::init(const string & sObjName, const string& setName, bool rootServant)
{
    _locator = _communicator->getProperty("locator");

    TLOGTARS("QueryEpBase::init sObjName:" << sObjName << ", sLocator:" << _locator << ", setName:" << setName << ", rootServant: " << rootServant << endl);
//	LOG_CONSOLE_DEBUG << "QueryEpBase::init sObjName:" << sObjName << ", sLocator:" << _locator << ", setName:" << setName << ", rootServant: " << rootServant << endl;

    _invokeSetId = setName;

    _rootServant = rootServant;

    setObjName(sObjName);

    return true;
}

void QueryEpBase::setObjName(const string & sObjName)
{
    string::size_type pos = sObjName.find_first_of('@');

    string sEndpoints;
    string sInactiveEndpoints;

    if (pos != string::npos)
    {
        //[直接连接]指定服务的IP和端口列表
        _objName = sObjName.substr(0,pos);

        sEndpoints = sObjName.substr(pos + 1);

	    pos = _objName.find_first_of("#");

	    if(pos != string::npos)
	    {
		    _objName        = _objName.substr(0, pos);
	    }

	    _direct = true;

        _valid = true;
    }
    else
    {
        //[间接连接]通过registry查询服务端的IP和端口列表

        _direct = false;
        _valid = false;

        _objName = sObjName;

        if(_locator.find_first_not_of('@') == string::npos)
        {
            TLOGERROR("[QueryEpBase::setObjName locator is not valid,_locator:" << _locator << "]" << endl);
            throw TarsRegistryException("locator is not valid,_locator:" + _locator);
        }

	    pos = _objName.find_first_of("#");
	    if(pos != string::npos)
	    {
		    _objName = _objName.substr(0, pos);
	    }

        _queryFPrx = _communicator->stringToProxy<QueryFPrx>(_locator);

        string sLocatorKey = _locator;

        //如果启用set，则获取按set分组的缓存
        if(ClientConfig::SetOpen)
        {
            sLocatorKey += "_" + ClientConfig::SetDivision;
        }

        string objName = _objName + string(_invokeSetId.empty() ? "" : ":") + _invokeSetId;

        //[间接连接]第一次使用cache，如果是接口级请求则不从缓存读取
        if(!_interfaceReq)
        {
            sEndpoints = AppCache::getInstance()->get(objName,sLocatorKey);
            sInactiveEndpoints = AppCache::getInstance()->get("inactive_"+objName,sLocatorKey);
        }
    }

    setEndpoints(sEndpoints,_activeEndpoints);
    setEndpoints(sInactiveEndpoints,_inactiveEndpoints);

    if(!_activeEndpoints.empty())
    {
        _valid = true;
    }

	if((!_activeEndpoints.empty() || !_inactiveEndpoints.empty()))
	{
    	//非直接指定端口, 且从cache中能查到服务端口的, 不需要通知所有ObjectProxy更新地址
        notifyEndpoints(_activeEndpoints,_inactiveEndpoints,true);
    }
}

void QueryEpBase::setEndpoints(const string & sEndpoints, set<EndpointInfo> & setEndpoints)
{
    if(sEndpoints == "")
    {
        return ;
    }

    bool         bSameWeightType  = true;
    bool         bFirstWeightType = true;
    unsigned int iWeightType      = 0;

    vector<string>  vEndpoints = TC_Endpoint::sepEndpoint(sEndpoints);

    for (size_t i = 0; i < vEndpoints.size(); ++i)
    {
        try
        {
            TC_Endpoint ep(vEndpoints[i]);

            string sSetDivision;
            //解析set分组信息
            if (!_direct)
            {
                string sep = " -s ";
                size_t pos = vEndpoints[i].rfind(sep);
                if (pos != string::npos)
                {
                    sSetDivision = TC_Common::trim(vEndpoints[i].substr(pos+sep.size()));

                    size_t endPos = sSetDivision.find(" ");

                    if (endPos != string::npos)
                    {
                        sSetDivision = sSetDivision.substr(0, endPos);
                    }
                }
            }

            if(bFirstWeightType)
            {
                bFirstWeightType = false;
                iWeightType = ep.getWeightType();
            }
            else
            {
                if(ep.getWeightType() != iWeightType)
                {
                    bSameWeightType = false;
                }
            }

	        EndpointInfo epi(ep, sSetDivision);

            setEndpoints.insert(epi);
        }
        catch (exception &ex)
        {
            TLOGERROR("[QueryEpBase::setEndpoints parse error,objname:" << _objName << ",endpoint:" << vEndpoints[i] << "]" << endl);
        }
    }

    if(bSameWeightType)
    {    
        if(iWeightType == 1)
        {
            _weightType = E_STATIC_WEIGHT;
        }
        else
        {
            _weightType = E_LOOP;
        }
    }
    else
    {
        _weightType = E_LOOP;
    }
}

void QueryEpBase::refreshReg(GetEndpointType type, const string & sName)
{
    onUpdateOutter();

    if(_direct)
    {
        return;
    }

    int64_t iNow = TNOWMS;
    //正在请求状态 而且请求超时了，或者第一次
    if(_requestRegistry && _requestTimeout < iNow)
    {
        doEndpointsExp(0);
    }

    //如果是间接连接，通过registry定时查询服务列表
    //正在请求状态 而且请求超时了
    //非请求状态 到了下一个刷新时间了
    if( (!_requestRegistry) && (_refreshTime <= iNow))
    {
        _requestRegistry = true;

        //一定时间不回调就算超时了
        _requestTimeout = iNow + _timeoutInterval;

        TLOGTARS("[QueryEpBase::refresh," << _objName << "]" <<endl);

        if(_valid && !_rootServant && _valid)
        {
            return;
        }
        //判断是同步调用还是异步调用
        //内部请求主控都是异步请求
        //接口请求主控第一次是同步请求
        bool bSync = (!_valid && _interfaceReq);

	    //如果是异步且不是根servant(通过#1创建的servant, 不主动更新主控信息)
        if(!bSync && !_rootServant)
	        return;

        try
        {
            if(bSync)
            {
                vector<EndpointF> activeEp;
                vector<EndpointF> inactiveEp;
                int iRet = 0;
                switch(type)
                {
                    case E_ALL:
                        {
                            iRet = _queryFPrx->findObjectById4Any(_objName,activeEp,inactiveEp, ServerConfig::Context);
                            break;
                        }
                    case E_STATION:
                        {
                            iRet = _queryFPrx->findObjectByIdInSameStation(_objName,sName,activeEp,inactiveEp, ServerConfig::Context);
	                        break;
                        }
                    case E_SET:
                        {
                            iRet = _queryFPrx->findObjectByIdInSameSet(_objName,sName,activeEp,inactiveEp, ServerConfig::Context);
                            break;
                        }
                    case E_DEFAULT:
                    default:
                        {
                            if(ClientConfig::SetOpen || !_invokeSetId.empty())
                            {
                                   //指定set调用时，指定set的优先级最高
                                string setId = _invokeSetId.empty()?ClientConfig::SetDivision:_invokeSetId;
                                iRet = _queryFPrx->findObjectByIdInSameSet(_objName,setId,activeEp,inactiveEp, ServerConfig::Context);
                            }
                            else
                            {
                                iRet = _queryFPrx->findObjectByIdInSameGroup(_objName,activeEp,inactiveEp, ServerConfig::Context);
                            }
                            break;
                        }
                }
                doEndpoints(activeEp, inactiveEp, iRet, true);
            }
            else
            {
                switch(type)
                {
                    case E_ALL:
                        {
                            _queryFPrx->async_findObjectById4Any(this,_objName, ServerConfig::Context);
                            break;
                        }
                    case E_STATION:
                        {
                            _queryFPrx->async_findObjectByIdInSameStation(this,_objName,sName, ServerConfig::Context);
                            break;
                        }
                    case E_SET:
                        {
                            _queryFPrx->async_findObjectByIdInSameSet(this,_objName,sName, ServerConfig::Context);
                            break;
                        }
                    case E_DEFAULT:
                    default:
                        {
                            if(ClientConfig::SetOpen || !_invokeSetId.empty())
                            {
                                //指定set调用时，指定set的优先级最高
                                string setId = _invokeSetId.empty()?ClientConfig::SetDivision:_invokeSetId;
                                _queryFPrx->async_findObjectByIdInSameSet(this,_objName,setId, ServerConfig::Context);
                            }
                            else
                            {
                                _queryFPrx->async_findObjectByIdInSameGroup(this,_objName, ServerConfig::Context);
                            }
                            break;
                        }
                }//end switch
            }
        }
        catch(TC_Exception & ex)
        {
            TLOGERROR("[QueryEpBase::refreshReg obj:"<<_objName<<"exception:"<<ex.what() << "]"<<endl);
            doEndpointsExp(TARSSERVERUNKNOWNERR);
        }
        catch(...)
        {
            TLOGERROR("[QueryEpBase::refreshReg obj:"<<_objName<<"unknown exception]" <<endl);
            doEndpointsExp(TARSSERVERUNKNOWNERR);
        }
    }
}

void QueryEpBase::doEndpoints(const vector<EndpointF>& activeEp, const vector<EndpointF>& inactiveEp, int iRet, bool bSync)
{
    if(iRet != 0)
    {
        doEndpointsExp(iRet);
        return ;
    }

    _failTimes = 0;
    _requestRegistry = false;

    int64_t iNow = TNOWMS;

    //有返回成功的结点，按照正常的频率
    //如果返回空列表或者返回失败 2s去刷新一次
    //接口请求主控的方式 不管是不是空都要去刷新
    if(activeEp.empty() && (!_interfaceReq) )
    {
        _refreshTime = iNow + _activeEmptyInterval;

        //如果registry返回Active服务列表为空，不做更新
        TLOGERROR("[QueryEpBase::doEndpoints, callback activeEps is empty,objname:"<< _objName << "]" << endl);
        return;
    }
    else
    {
        _refreshTime = iNow + _refreshInterval;
    }

    bool bNeedNotify = false;
    bool bSameWeightType = true;
    bool bFirstWeightType = true;
    int iWeightType = 0;

    set<string>    sActiveEndpoints;
    set<string> sInactiveEndpoints;
    set<EndpointInfo> activeEps;
    set<EndpointInfo> inactiveEps;

    //生成active set 用于比较
    for (uint32_t i = 0; i < activeEp.size(); ++i)
    {
        if(bFirstWeightType)
        {
            bFirstWeightType = false;
            iWeightType = activeEp[i].weightType;
        }
        else
        {
            if(activeEp[i].weightType != iWeightType)
            {
                bSameWeightType = false;
            }
        }

        //  taf istcp意思和这里枚举值对应
        activeEps.insert(EndpointInfo(activeEp[i]));
    }

    //生成inactive set 用于比较
    for (uint32_t i = 0; i < inactiveEp.size(); ++i)
    {
        //  taf istcp意思和这里枚举值对应
        inactiveEps.insert(EndpointInfo(inactiveEp[i]));
    }

    if(bSameWeightType)
    {    
        if(iWeightType == 1)
        {
            _weightType = E_STATIC_WEIGHT;
        }
        else
        {
            _weightType = E_LOOP;
        }
    }
    else
    {
        _weightType = E_LOOP;
    }

    if(activeEps != _activeEndpoints)
    {
        bNeedNotify = true;
        _activeEndpoints = activeEps;

        if(_firstNetThread)
        {
            setEndPointToCache(false);
        }
    }

    if(inactiveEps != _inactiveEndpoints)
    {
        bNeedNotify = true;
        _inactiveEndpoints = inactiveEps;

        if(_firstNetThread)
        {
            setEndPointToCache(true);
        }
    }

    if(bNeedNotify)
    {
        notifyEndpoints(_activeEndpoints,_inactiveEndpoints,bSync);
    }

    if(!_valid)
    {
        _valid = true;
        doNotify();
    }
}

void QueryEpBase::doEndpointsExp(int iRet)
{
    _failTimes++;
    _requestRegistry = false;

    int64_t iNow = TNOWMS;

    //频率控制获取主控失败 2秒钟再更新
    _refreshTime = iNow + _failInterval;

    //获取主控连续失败3次就等30s再更新一次
    //连续失败 强制设成数据是有效的
    if(_failTimes > _failTimesLimit)
    {
        if(!_valid)
        {
            _valid = true;
            doNotify();
        }
        _refreshTime = iNow + _manyFailInterval;
    }
}

void QueryEpBase::setEndPointToCache(bool bInactive)
{
    //如果是接口级请求则不缓存到文件
    if(_interfaceReq)
    {
        return;
    }

    string sEndpoints;

    set<EndpointInfo> doEndpoints;
    if(!bInactive)
    {
        doEndpoints = _activeEndpoints;
    }
    else
    {
        doEndpoints = _inactiveEndpoints;
    }
    set<EndpointInfo>::iterator iter;
    iter = doEndpoints.begin();

    for (; iter != doEndpoints.end(); ++iter)
    {
        //这里的超时时间 只是对服务端有效。这里的值无效。所以默认用3000了
        TC_Endpoint ep = iter->getEndpoint();

        if (!sEndpoints.empty())
        {
            sEndpoints += ":";
        }

        sEndpoints += ep.toString();

        if (!iter->setDivision().empty())
        {
            sEndpoints += " -s " + iter->setDivision();
        }
    }

    //如果启用set，则按set分组保存
    string sLocatorKey = _locator;
    if(ClientConfig::SetOpen)
    {
        sLocatorKey += "_" + ClientConfig::SetDivision;
    }
    string objName = _objName + string(_invokeSetId.empty()?"":":") + _invokeSetId;
    if(bInactive)
    {
        AppCache::getInstance()->set("inactive_"+objName,sEndpoints,sLocatorKey);
    }
    else
    {
        AppCache::getInstance()->set(objName,sEndpoints,sLocatorKey);
    }

    TLOGTARS("[setEndPointToCache,obj:" << _objName << ",invokeSetId:" << _invokeSetId << ",endpoint:" << sEndpoints << "]" << endl);
}

EndpointManager::EndpointManager(ObjectProxy * pObjectProxy, Communicator* pComm, bool bFirstNetThread)
: QueryEpBase(pComm, bFirstNetThread, false)
,_objectProxy(pObjectProxy)
,_lastRoundPosition(0)
,_update(true)
,_updateWeightInterval(60)
,_lastSWeightPosition(0)
,_consistentHashWeight(E_TC_CONHASH_KETAMAHASH)
,_consistentHash(E_TC_CONHASH_KETAMAHASH)
{
    setNetThreadProcess(true);
}

EndpointManager::~EndpointManager()
{
    map<string,AdapterProxy*>::iterator iterAdapter;
    for(iterAdapter = _allProxys.begin();iterAdapter != _allProxys.end();iterAdapter++)
    {
        if(iterAdapter->second)
        {
            delete iterAdapter->second;
            iterAdapter->second = NULL;
        }
    }
	_allProxys.clear();
}

void EndpointManager::onUpdateOutter()
{
//	LOG_CONSOLE_DEBUG << this->_objectProxy << ", valid:" << _valid << ", " << _outterUpdate.get() << endl;
	assert(this->_objectProxy->getCommunicatorEpoll()->getThreadId() == this_thread::get_id());

    lock_guard<mutex> l(_outterLocker);
    if(_outterUpdate)
    {
		shared_ptr<OutterUpdate> outterUpdate = _outterUpdate;

        updateEndpoints(outterUpdate->active, outterUpdate->inactive);

		_valid = true;

		_outterUpdate.reset();
    }
}

void EndpointManager::updateEndpointsOutter(const set<EndpointInfo> & active, const set<EndpointInfo> & inactive)
{
//	LOG_CONSOLE_DEBUG << this->_objectProxy << ", " << active.begin()->desc() << endl;
	//创新新对象, 避免线程冲突
    shared_ptr<OutterUpdate> outterUpdate = std::make_shared<OutterUpdate>();
	outterUpdate->active    = active;
	outterUpdate->inactive  = inactive;

    //更新时间
	_refreshTime = TNOWMS + _refreshInterval;

    lock_guard<mutex> l(_outterLocker);
	_outterUpdate = outterUpdate;
}

void EndpointManager::updateEndpoints(const set<EndpointInfo> & active, const set<EndpointInfo> & inactive)
{
    TLOGTARS("[EndpointManager::updateEndpoints obj:" << this->_objName << ", active:" << active.size() << ", inactive size:" << inactive.size() << endl);

	pair<map<string,AdapterProxy*>::iterator,bool> result;

	_activeProxys.clear();
	_regProxys.clear();
	_indexActiveProxys.clear();
	_sortActivProxys.clear();

	if(!active.empty())
	{
		//先把服务都设置为非活跃
		for (auto iter = _allProxys.begin(); iter != _allProxys.end(); ++iter)
		{
			iter->second->setActiveInReg(false);
		}
	}

	//更新active
	for(auto iter = active.begin(); iter != active.end(); ++iter)
	{
		if(!_direct && _weightType == E_STATIC_WEIGHT && iter->weight() <= 0)
		{
			continue;
		}

//		LOG_CONSOLE_DEBUG << std::this_thread::get_id() << ", allProxys size:" << _allProxys.size() << ", " << iter->cmpDesc() << endl;

		auto iterAdapter = _allProxys.find(iter->cmpDesc());
		if(iterAdapter == _allProxys.end())
		{
			AdapterProxy* ap = new AdapterProxy(_objectProxy, *iter, _communicator);

			result = _allProxys.insert(make_pair(iter->cmpDesc(),ap));

			iterAdapter = result.first;

			_vAllProxys.push_back(ap);
		}

		//该节点在主控的状态为active
		iterAdapter->second->setActiveInReg(true);

		_activeProxys.push_back(iterAdapter->second);

		_regProxys.insert(make_pair(iter->cmpDesc(),iterAdapter->second));

		const string &host = iterAdapter->second->endpoint().host();
		_indexActiveProxys.insert(make_pair(host, iterAdapter->second));
		_sortActivProxys.insert(make_pair(host, iterAdapter->second));

		//设置该节点的静态权重值
		iterAdapter->second->setWeight(iter->weight());
	}

	//更新inactive
	for(auto iter = inactive.begin(); iter != inactive.end(); ++iter)
	{
		if(!_direct && _weightType == E_STATIC_WEIGHT && iter->weight() <= 0)
		{
			continue;
		}

		auto iterAdapter = _allProxys.find(iter->cmpDesc());
		if(iterAdapter == _allProxys.end())
		{
			AdapterProxy* ap = new AdapterProxy(_objectProxy, *iter, _communicator);

			result = _allProxys.insert(make_pair(iter->cmpDesc(),ap));
			assert(result.second);

			iterAdapter = result.first;

			_vAllProxys.push_back(ap);
		}

		//该节点在主控的状态为inactive
		iterAdapter->second->setActiveInReg(false);

		_regProxys.insert(make_pair(iter->cmpDesc(),iterAdapter->second));

		//设置该节点的静态权重值
		iterAdapter->second->setWeight(iter->weight());
	}

	//_vRegProxys 需要按顺序来 重排
	_vRegProxys.clear();
	auto iterAdapter = _regProxys.begin();
	for(;iterAdapter != _regProxys.end();++iterAdapter)
	{
		_vRegProxys.push_back(iterAdapter->second);
	}

	_update = true;
}

void EndpointManager::notifyEndpoints(const set<EndpointInfo> & active,const set<EndpointInfo> & inactive,bool bNotify)
{
	updateEndpoints(active, inactive);
	//丢给外层统一做
    _objectProxy->onNotifyEndpoints(active, inactive);
}

void EndpointManager::doNotify()
{
    _objectProxy->doInvoke();
}

bool EndpointManager::selectAdapterProxy(ReqMessage * msg,AdapterProxy * & pAdapterProxy)
{
    pAdapterProxy = NULL;

    //刷新主控
    refreshReg(E_DEFAULT, "");

    //无效的数据 返回true
    if (!_valid) 
    {
        return true;
    }

    //如果有hash，则先使用hash策略
    if (msg->data._hash)
    {
        pAdapterProxy = getHashProxy(msg->data._hashCode, msg->data._conHash);
        return false;
    }
    
    if(_weightType == E_STATIC_WEIGHT)
    {
        //权重模式
        bool bStaticWeighted = false;
        if(_weightType == E_STATIC_WEIGHT || msg->eType == ReqMessage::ONE_WAY)
            bStaticWeighted = true;

        pAdapterProxy = getWeightedProxy(bStaticWeighted);
    }
    else
    {
        //普通轮询模式
        pAdapterProxy = getNextValidProxy();
    }

    return false;
}

AdapterProxy * EndpointManager::getNextValidProxy()
{
    if (_activeProxys.empty())
    {
        TLOGERROR("[EndpointManager::getNextValidProxy activeEndpoints is empty][obj:"<<_objName<<"]" << endl);
        return NULL;
    }

	vector<AdapterProxy*> conn;

    for(size_t i=0;i<_activeProxys.size();i++)
    {
        ++_lastRoundPosition;
        if(_lastRoundPosition >= _activeProxys.size())
        {
            _lastRoundPosition = 0;
        }

	    if(_activeProxys[_lastRoundPosition]->checkActive(false))
        {
	        return _activeProxys[_lastRoundPosition];
        }

        if(!_activeProxys[_lastRoundPosition]->isConnTimeout() && !_activeProxys[_lastRoundPosition]->isConnExc()) {
	        conn.push_back(_activeProxys[_lastRoundPosition]);
        }
    }

    if(conn.size() > 0)
    {
        //都有问题, 随机选择一个没有connect超时或者链接异常的发送
        AdapterProxy * adapterProxy = conn[((uint32_t)rand() % conn.size())];

        //该proxy可能已经被屏蔽,需重新连一次
        adapterProxy->checkActive(true);
        return adapterProxy;
    }

    //所有adapter都有问题 选不到结点,随机找一个重试
    AdapterProxy * adapterProxy = _activeProxys[((uint32_t)rand() % _activeProxys.size())];

	adapterProxy->resetRetryTime(false);

    //该proxy可能已经被屏蔽,需重新连一次
    adapterProxy->checkActive(true);

    return adapterProxy;
}

AdapterProxy* EndpointManager::getHashProxy(uint32_t hashCode, bool bConsistentHash)
{
    if(_weightType == E_STATIC_WEIGHT)
    {
        if(bConsistentHash)
        {
            return getConHashProxyForWeight(hashCode, true);
        }
        else
        {
            return getHashProxyForWeight(hashCode, true, _hashStaticRouterCache);
        }
    }
    else
    {
        if(bConsistentHash)
        {
            return getConHashProxyForNormal(hashCode);
        }
        else
        {
            return getHashProxyForNormal(hashCode);
        }
    }
}

AdapterProxy* EndpointManager::getHashProxyForWeight(uint32_t hashCode, bool bStatic, vector<size_t> &vRouterCache)
{
    if(_vRegProxys.empty())
    {
        TLOGERROR("[EndpointManager::getHashProxyForWeight _vRegProxys is empty], bStatic:" << bStatic << endl);
        return NULL;
    }

    if(checkHashStaticWeightChange(bStatic))
    {
        int64_t iBegin = TNOWMS;

        updateHashProxyWeighted(bStatic);

        int64_t iEnd = TNOWMS;

        TLOGTARS("[EndpointManager::getHashProxyForWeight update bStatic:" << bStatic << "|_objName:" << _objName << "|timecost(ms):" << (iEnd - iBegin) << endl);
    }

    if(vRouterCache.size() > 0)
    {
        size_t hash = hashCode % vRouterCache.size();

        //这里做判断的原因是：32位系统下，如果hashCode为负值，hash经过上面的计算会是一个超大值，导致越界
        if(hash >= vRouterCache.size())
        {
            hash = hash % vRouterCache.size();
        }

        size_t iIndex = vRouterCache[hash];

        if(iIndex >= _vRegProxys.size())
        {
            iIndex = iIndex % _vRegProxys.size();
        }

        //被hash到的节点在主控是active的才走在流程
        if (_vRegProxys[iIndex]->isActiveInReg() && _vRegProxys[iIndex]->checkActive(true))
        {
            return _vRegProxys[iIndex];
        }
        else
        {
            TLOGWARN("[EndpointManager::getHashProxyForWeight, hash not active," << _objectProxy->name() << "@" << _vRegProxys[iIndex]->endpoint().desc() << endl);
            if(_activeProxys.empty())
            {
                TLOGERROR("[EndpointManager::getHashProxyForWeight _activeEndpoints is empty], bStatic:" << bStatic << endl);
                return NULL;
            }

            //在active节点中再次hash
            vector<AdapterProxy*> thisHash = _activeProxys;
            vector<AdapterProxy*> conn;

            do
            {
                hash = hashCode % thisHash.size();

                if (thisHash[hash]->checkActive(true))
                {
                    return thisHash[hash];
                }
                if(!thisHash[hash]->isConnTimeout() &&
                   !thisHash[hash]->isConnExc())
                {
                    conn.push_back(thisHash[hash]);
                }
                thisHash.erase(thisHash.begin() + hash);
            }
            while(!thisHash.empty());

            if(conn.size() > 0)
            {
                hash = hashCode % conn.size();

                //都有问题, 随机选择一个没有connect超时或者链接异常的发送
                AdapterProxy *adapterProxy = conn[hash];

                //该proxy可能已经被屏蔽,需重新连一次
                adapterProxy->checkActive(true);
                return adapterProxy;
            }

            //所有adapter都有问题 选不到结点,随机找一个重试
            AdapterProxy * adapterProxy = _activeProxys[((uint32_t)rand() % _activeProxys.size())];

	        adapterProxy->resetRetryTime(false);

	        //该proxy可能已经被屏蔽,需重新连一次
            adapterProxy->checkActive(true);

            return adapterProxy;
        }
    }

    return getHashProxyForNormal(hashCode);
}


AdapterProxy* EndpointManager::getConHashProxyForWeight(uint32_t hashCode, bool bStatic)
{
    if(_vRegProxys.empty())
    {
        TLOGERROR("[EndpointManager::getConHashProxyForWeight _vRegProxys is empty], bStatic:" << bStatic << endl);
        return NULL;
    }

    if(checkConHashChange(bStatic, _lastConHashWeightProxys))
    {
        int64_t iBegin = TNOWMS;

        updateConHashProxyWeighted(bStatic, _lastConHashWeightProxys, _consistentHashWeight);

        int64_t iEnd = TNOWMS;

        TLOGTARS("[EndpointManager::getConHashProxyForWeight update bStatic:" << bStatic << "|_objName:" << _objName << "|timecost(ms):" << (iEnd - iBegin) << endl);
    }

    while(_consistentHashWeight.size() > 0)
    {
        string sNode;

        // 通过一致性hash取到对应的节点
        _consistentHashWeight.getNodeName(hashCode, sNode);

        auto it = _indexActiveProxys.find(sNode);
        // 节点不存在，可能是下线或者服务不可用
        if (it == _indexActiveProxys.end())
        {
            updateConHashProxyWeighted(bStatic, _lastConHashWeightProxys, _consistentHashWeight);
            continue;
        }

        //被hash到的节点在主控是active的才走在流程
        if (it->second->isActiveInReg() && it->second->checkActive(true))
        {
            return it->second;
        }
        else
        {
            TLOGWARN("[EndpointManager::getHashProxyForWeight, hash not active," << _objectProxy->name() << "@" << it->second->endpoint().desc() << endl);
            // 剔除节点再次hash
            if (!it->second->isActiveInReg())
            {
                // 如果在主控的注册状态不是active直接删除，如果状态有变更由updateEndpoints函数里重新添加
                _indexActiveProxys.erase(sNode);
            }
            // checkConHashChange里重新加回到_sortActivProxys重试
            _sortActivProxys.erase(sNode);
            updateConHashProxyWeighted(bStatic, _lastConHashWeightProxys, _consistentHashWeight);

            if (_indexActiveProxys.empty())
            {
                TLOGERROR("[EndpointManager::getConHashProxyForNormal _activeEndpoints is empty]" << endl);
                return NULL;
            }
        }
    }

    return getHashProxyForNormal(hashCode);
}

bool EndpointManager::checkHashStaticWeightChange(bool bStatic)
{
    if(bStatic)
    {
        if(_lastHashStaticProxys.size() != _vRegProxys.size())
        {
            return true;
        }

        for(size_t i = 0; i < _vRegProxys.size(); i++)
        {
            //解决服务权重更新时哈希表不更新的问题
            if((_lastHashStaticProxys[i]->endpoint().desc() != _vRegProxys[i]->endpoint().desc()) || _vRegProxys[i]->checkWeightChanged(true))
            {
                return true;
            }
        }
    }

    return false;
}

bool EndpointManager::checkConHashChange(bool bStatic, const map<string, AdapterProxy*> &mLastConHashProxys)
{
    // 将之前故障临时剔除的节点重新加回来重试
    if (_indexActiveProxys.size() != _sortActivProxys.size())
    {
        for (auto &it : _indexActiveProxys)
        {
            _sortActivProxys[it.first] = it.second;
        }
    }

    if(mLastConHashProxys.size() != _sortActivProxys.size())
    {
        return true;
    }

    auto itLast = mLastConHashProxys.begin();
    auto itSort = _sortActivProxys.begin();
    for (; itLast!=mLastConHashProxys.end() && itSort!=_sortActivProxys.end(); ++itLast,++itSort)
    {
        if (itLast->first != itSort->first)
        {
            return true;
        }

        //解决服务权重更新时一致性哈希环不更新的问题
        if(bStatic && itSort->second->checkWeightChanged(true))
        {
            return true;
        }
    }
/*
    if(vLastConHashProxys.size() != _vRegProxys.size())
    {
        return true;
    }

    for(size_t i = 0; i < _vRegProxys.size(); i++)
    {
        if(vLastConHashProxys[i]->endpoint().desc() != _vRegProxys[i]->endpoint().desc())
        {
            return true;
        }

        //解决服务权重更新时一致性哈希环不更新的问题
        if(bStatic && _vRegProxys[i]->checkWeightChanged(true))
        {
            return true;
        }
    }
*/

    return false;
}

void EndpointManager::updateHashProxyWeighted(bool bStatic)
{
    if(_vRegProxys.size() <= 0)
    {    
        TLOGERROR("[EndpointManager::updateHashProxyWeighted _vRegProxys is empty], bStatic:" << bStatic << endl);
        return ;
    }

    if(bStatic)
    {
        _lastHashStaticProxys = _vRegProxys;
        _hashStaticRouterCache.clear();
    }

    vector<AdapterProxy*> vRegProxys;
    vector<size_t> vIndex;
    for(size_t i = 0; i < _vRegProxys.size(); ++i)
    {
        if(_vRegProxys[i]->getWeight() > 0)
        {
            vRegProxys.push_back(_vRegProxys[i]);
            vIndex.push_back(i);
        }
        //防止多个服务节点权重同时更新时哈希表多次更新
        _vRegProxys[i]->resetWeightChanged();
    }

    if(vRegProxys.size() <= 0)
    {
        TLOGERROR("[EndpointManager::updateHashProxyWeighted vRegProxys is empty], bStatic:" << bStatic << endl);
        return ;
    }

    size_t                        iHashStaticWeightSize =    vRegProxys.size();
    map<size_t, int>              mIdToWeight;
    multimap<int, size_t>         mWeightToId;
    size_t                        iMaxR = 0;
    size_t                        iMaxRouterR = 0;

    size_t iMaxWeight = vRegProxys[0]->getWeight();
    size_t iMinWeight = vRegProxys[0]->getWeight();
    size_t iTempWeight = 0;

    for(size_t i = 1;i < iHashStaticWeightSize; i++)
    {
        iTempWeight = vRegProxys[i]->getWeight();
        
        if(iTempWeight > iMaxWeight)
        {
            iMaxWeight = iTempWeight;
        }

        if(iTempWeight < iMinWeight)
        {
            iMinWeight = iTempWeight;
        }
    }

    if(iMinWeight > 0)
    {
        iMaxR = iMaxWeight / iMinWeight;

        if(iMaxR < iMinWeightLimit)
            iMaxR = iMinWeightLimit;

        if(iMaxR > iMaxWeightLimit)
            iMaxR = iMaxWeightLimit;
    }
    else
    {
        iMaxR = 1;
        iMaxWeight = 1;
    }

    for(size_t i = 0;i < iHashStaticWeightSize; i++)
    {
        int iWeight = (vRegProxys[i]->getWeight() * iMaxR) / iMaxWeight;

        if(iWeight > 0)
        {
            iMaxRouterR += iWeight;

            mIdToWeight.insert(map<size_t, int>::value_type(vIndex[i], iWeight));

            mWeightToId.insert(make_pair(iWeight, vIndex[i]));
        }
        else
        {
            if(bStatic)
            {
                _hashStaticRouterCache.push_back(vIndex[i]);
            }
        }

        TLOGTARS("EndpointManager::updateHashProxyWeighted bStatic:" << bStatic << "|_objName:" << _objName << "|endpoint:" << vRegProxys[i]->endpoint().desc() << "|iWeight:" << vRegProxys[i]->getWeight() << "|iWeightR:" << iWeight << "|iIndex:" << vIndex[i] << endl);
    }

    for(size_t i = 0; i < iMaxRouterR; i++)
    {
        bool bFirst = true;
        multimap<int, size_t> mulTemp;
        multimap<int, size_t>::reverse_iterator mIter = mWeightToId.rbegin();
        while(mIter != mWeightToId.rend())
        {
            if(bFirst)
            {
                bFirst = false;
                if(bStatic)
                {
                    _hashStaticRouterCache.push_back(mIter->second);
                }

                mulTemp.insert(make_pair((mIter->first - iMaxRouterR + mIdToWeight[mIter->second]), mIter->second));
            }
            else
            {
                mulTemp.insert(make_pair((mIter->first + mIdToWeight[mIter->second]), mIter->second));
            }

            mIter++;
        }

        mWeightToId.clear();
        mWeightToId.swap(mulTemp);
    }
}

void EndpointManager::updateConHashProxyWeighted(bool bStatic, map<string, AdapterProxy*> &mLastConHashProxys, TC_ConsistentHashNew &conHash)
{
    conHash.clear();
    if(_sortActivProxys.empty())
    {    
        TLOGERROR("[EndpointManager::updateHashProxyWeighted _indexActiveProxys is empty], bStatic:" << bStatic << endl);
        return ;
    }

    mLastConHashProxys = _sortActivProxys;

    for (auto it = _sortActivProxys.begin(); it != _sortActivProxys.end(); ++it)
    {
        int iWeight = (bStatic ? (it->second->getWeight()) : 100);
        if(iWeight > 0)
        {
            iWeight = iWeight / 4;
            if(iWeight <= 0)
            {
                iWeight = 1;
            }
            // 同一服务有多个obj的情况
            // 同一hash值调用不同的obj会hash到不同的服务器
            // 因为addNode会根据desc(ip+port)计算md5,导致顺序不一致
            // 一致性hash用host进行索引，不使用index，这里传0
            conHash.addNode(it->second->endpoint().host(), 0, iWeight);
        }
        //防止多个服务节点权重同时更新时一致性哈希环多次更新
        it->second->resetWeightChanged();
    }
/*
    if(_vRegProxys.size() <= 0)
    {
        TLOGERROR("[EndpointManager::updateHashProxyWeighted _vRegProxys is empty], bStatic:" << bStatic << endl);
        return ;
    }

    vLastConHashProxys = _vRegProxys;
    conHash.clear();

    for(size_t i = 0; i < _vRegProxys.size(); ++i)
    {
        int iWeight = (bStatic ? (_vRegProxys[i]->getWeight()) : 100);
        if(iWeight > 0)
        {
            iWeight = iWeight / 4;
            if(iWeight <= 0)
            {
                iWeight = 1;
            }
            // 同一服务有多个obj的情况
            // 同一hash值调用不同的obj会hash到不同的服务器
            // 因为addNode会根据desc(ip+port)计算md5,导致顺序不一致
            conHash.addNode(_vRegProxys[i]->endpoint().host(), i, iWeight);
        }
        //防止多个服务节点权重同时更新时一致性哈希环多次更新
        _vRegProxys[i]->resetWeightChanged();
    }
*/
    conHash.sortNode();
}

AdapterProxy* EndpointManager::getHashProxyForNormal(uint32_t hashCode)
{
    if(_vRegProxys.empty())
    {
        TLOGERROR("[EndpointManager::getHashProxyForNormal _vRegProxys is empty]" << endl);
        return NULL;
    }

    // 1 _vRegProxys从客户端启动之后，就不会再改变，除非有节点增加
    // 2 如果有增加节点，则_vRegProxys顺序会重新排序,之前的hash会改变
    // 3 节点下线后，需要下次启动客户端后,_vRegProxys内容才会生效
    size_t hash = hashCode % _vRegProxys.size();

    //被hash到的节点在主控是active的才走在流程
    if (_vRegProxys[hash]->isActiveInReg() && _vRegProxys[hash]->checkActive(true))
    {
        return _vRegProxys[hash];
    }
    else
    {
        TLOGWARN("[EndpointManager::getHashProxyForNormal, hash not active," << _objectProxy->name() << "@" << _vRegProxys[hash]->endpoint().desc() << endl);
        if(_activeProxys.empty())
        {
            TLOGERROR("[EndpointManager::getHashProxyForNormal _activeEndpoints is empty]" << endl);
            return NULL;
        }

        //在active节点中再次hash
        vector<AdapterProxy*> thisHash = _activeProxys;
        vector<AdapterProxy*> conn;

        do
        {
            hash = hashCode % thisHash.size();

            if (thisHash[hash]->checkActive(true))
            {
                return thisHash[hash];
            }
            if(!thisHash[hash]->isConnTimeout() &&
               !thisHash[hash]->isConnExc())
            {
                conn.push_back(thisHash[hash]);
            }
            thisHash.erase(thisHash.begin() + hash);
        }
        while(!thisHash.empty());

        if(conn.size() > 0)
        {
            hash = hashCode % conn.size();

            //都有问题, 随机选择一个没有connect超时或者链接异常的发送
            AdapterProxy *adapterProxy = conn[hash];

            //该proxy可能已经被屏蔽,需重新连一次
            adapterProxy->checkActive(true);
            return adapterProxy;
        }

        //所有adapter都有问题 选不到结点,随机找一个重试
        AdapterProxy * adapterProxy = _activeProxys[((uint32_t)rand() % _activeProxys.size())];
	    adapterProxy->resetRetryTime(false);

	    //该proxy可能已经被屏蔽,需重新连一次
        adapterProxy->checkActive(true);

        return adapterProxy;
    }
}

AdapterProxy* EndpointManager::getConHashProxyForNormal(uint32_t hashCode)
{
    if(_vRegProxys.empty())
    {
        TLOGERROR("[EndpointManager::getConHashProxyForNormal _vRegProxys is empty]" << endl);
        return NULL;
    }

    if(checkConHashChange(false, _lastConHashProxys))
    {
        int64_t iBegin = TNOWMS;

        updateConHashProxyWeighted(false, _lastConHashProxys, _consistentHash);

        int64_t iEnd = TNOWMS;

        TLOGTARS("[EndpointManager::getConHashProxyForNormal update _objName:" << _objName << "|timecost(ms):" << (iEnd - iBegin) << endl);
    }

    while(_consistentHash.size() > 0)
    {
        string sNode;

        // 通过一致性hash取到对应的节点
        _consistentHash.getNodeName(hashCode, sNode);

        auto it = _indexActiveProxys.find(sNode);
        // 节点不存在，可能是下线或者服务不可用
        if (it == _indexActiveProxys.end())
        {
            updateConHashProxyWeighted(false, _lastConHashProxys, _consistentHash);
            continue;
        }

        //被hash到的节点在主控是active的才走在流程
        if (it->second->isActiveInReg() && it->second->checkActive(true))
        {
            return it->second;
        }
        else
        {
            TLOGWARN("[EndpointManager::getConHashProxyForNormal, hash not active," << _objectProxy->name() << "@" << it->second->endpoint().desc() << endl);
            // 剔除节点再次hash
            if (!it->second->isActiveInReg())
            {
                // 如果在主控的注册状态不是active直接删除，如果状态有变更由updateEndpoints函数里重新添加
                _indexActiveProxys.erase(sNode);
            }
            // checkConHashChange里重新加回到_sortActivProxys重试
            _sortActivProxys.erase(sNode);
            updateConHashProxyWeighted(false, _lastConHashProxys, _consistentHash);

            if (_indexActiveProxys.empty())
            {
                TLOGERROR("[EndpointManager::getConHashProxyForNormal _activeEndpoints is empty]" << endl);
                return NULL;
            }
        }
    }

    return getHashProxyForNormal(hashCode);
}

AdapterProxy* EndpointManager::getWeightedProxy(bool bStaticWeighted)
{
    return getWeightedForNormal(bStaticWeighted);
}

AdapterProxy* EndpointManager::getWeightedForNormal(bool bStaticWeighted)
{
    if (_activeProxys.empty())
    {
        TLOGERROR("[EndpointManager::getWeightedForNormal activeEndpoints is empty][obj:"<<_objName<<"]" << endl);
        return NULL;
    }

    int64_t iNow = TNOW;
    if(_lastBuildWeightTime <= iNow)
    {
        updateProxyWeighted();

        if(!_first)
        {
            _lastBuildWeightTime = iNow + _updateWeightInterval;
        }
        else
        {    
            _first = false;
            _lastBuildWeightTime = iNow + _updateWeightInterval + 5;
        }
    }

    bool bEmpty = false;
    int iActiveSize = _activeWeightProxy.size();
    
    if(iActiveSize > 0)
    {
        size_t iProxyIndex = 0;
        set<AdapterProxy*> sConn;

        if(_staticRouterCache.size() > 0)
        {
            for(size_t i = 0;i < _staticRouterCache.size(); i++)
            {
                ++_lastSWeightPosition;

                if(_lastSWeightPosition >= _staticRouterCache.size())
                    _lastSWeightPosition = 0;

                iProxyIndex = _staticRouterCache[_lastSWeightPosition];

                if(_activeWeightProxy[iProxyIndex]->checkActive(false))
                {
                    return _activeWeightProxy[iProxyIndex];
                }

                if(!_activeWeightProxy[iProxyIndex]->isConnTimeout() &&
                    !_activeWeightProxy[iProxyIndex]->isConnExc())
                {
                    sConn.insert(_activeWeightProxy[iProxyIndex]);
                }
            }
        }
        else
        {
            bEmpty = true;
        }

        if(!bEmpty)
        {
            if(sConn.size() > 0)
            {
                vector<AdapterProxy*> conn;
                set<AdapterProxy*>::iterator it_conn = sConn.begin();
                while(it_conn != sConn.end())
                {
                    conn.push_back(*it_conn);
                    ++it_conn;
                }

                //都有问题, 随机选择一个没有connect超时或者链接异常的发送
                AdapterProxy * adapterProxy = conn[((uint32_t)rand() % conn.size())];

                //该proxy可能已经被屏蔽,需重新连一次
                adapterProxy->checkActive(true);
                return adapterProxy;
            }

            //所有adapter都有问题 选不到结点,随机找一个重试
            AdapterProxy * adapterProxy = _activeWeightProxy[((uint32_t)rand() % iActiveSize)];

	        adapterProxy->resetRetryTime(false);

	        //该proxy可能已经被屏蔽,需重新连一次
            adapterProxy->checkActive(true);

            return adapterProxy;
        }
    }

    vector<AdapterProxy*> conn;

    for(size_t i=0;i<_activeProxys.size();i++)
    {
        ++_lastRoundPosition;
        if(_lastRoundPosition >= _activeProxys.size())
            _lastRoundPosition = 0;

        if(_activeProxys[_lastRoundPosition]->checkActive(false))
        {
            return _activeProxys[_lastRoundPosition];
        }

        if(!_activeProxys[_lastRoundPosition]->isConnTimeout() &&
            !_activeProxys[_lastRoundPosition]->isConnExc())
            conn.push_back(_activeProxys[_lastRoundPosition]);
    }

    if(conn.size() > 0)
    {
        //都有问题, 随机选择一个没有connect超时或者链接异常的发送
        AdapterProxy * adapterProxy = conn[((uint32_t)rand() % conn.size())];

        //该proxy可能已经被屏蔽,需重新连一次
        adapterProxy->checkActive(true);
        return adapterProxy;
    }

    //所有adapter都有问题 选不到结点,随机找一个重试
    AdapterProxy * adapterProxy = _activeProxys[((uint32_t)rand() % _activeProxys.size())];

	adapterProxy->resetRetryTime(false);

	//该proxy可能已经被屏蔽,需重新连一次
    adapterProxy->checkActive(true);

    return adapterProxy;
}

void EndpointManager::updateProxyWeighted()
{
    size_t iWeightProxySize = _activeProxys.size();

    if(iWeightProxySize <= 0)
    {
        TLOGERROR("[EndpointManager::updateProxyWeighted _objName:" << _objName << ", activeProxys.size() <= 0]" << endl);
        return ;
    }

    vector<AdapterProxy*>    vProxy;

    for(size_t i = 0; i < _activeProxys.size(); ++i)
    {
        if(_activeProxys[i]->getWeight() > 0)
        {
            vProxy.push_back(_activeProxys[i]);
        }
    }

    iWeightProxySize = vProxy.size();

    if(iWeightProxySize <= 0)
    {
        TLOGERROR("[EndpointManager::updateProxyWeighted _objName:" << _objName << ", vProxy.size() <= 0]" << endl);
        return ;
    }

    if(_update)
    {
        _activeWeightProxy = vProxy;

        updateStaticWeighted();
    }

    _update = false;
}

void EndpointManager::updateStaticWeighted()
{
    size_t        iWeightProxySize =    _activeWeightProxy.size();
    vector<int> vWeight;

    vWeight.resize(iWeightProxySize);

    for(size_t i = 0; i < iWeightProxySize; i++)
    {
        vWeight[i] = _activeWeightProxy[i]->getWeight();
    }

    dispatchEndpointCache(vWeight);
}

void EndpointManager::dispatchEndpointCache(const vector<int> &vWeight)
{
    if(vWeight.size() <= 0)
    {
        TLOGERROR("[EndpointManager::dispatchEndpointCache vWeight.size() < 0]" << endl);
        return ;
    }

    size_t                        iWeightProxySize =    vWeight.size();
    map<size_t, int>            mIdToWeight;
    multimap<int, size_t>        mWeightToId;
    size_t                        iMaxR = 0;
    size_t                        iMaxRouterR = 0;
    size_t                        iMaxWeight = 0;
    size_t                        iMinWeight = 0;
    size_t                        iTotalCapacty = 0;
    size_t                        iTempWeight = 0;

    for(size_t i = 0; i < vWeight.size(); ++i)
    {
        iTotalCapacty += vWeight[i];
    }

    _staticRouterCache.clear();
    _lastSWeightPosition = 0;
    _staticRouterCache.reserve(iTotalCapacty+100);

    iMaxWeight = vWeight[0];
    iMinWeight = vWeight[0];

    for(size_t i = 1;i < iWeightProxySize; i++)
    {
        iTempWeight = vWeight[i];
        
        if(iTempWeight > iMaxWeight)
        {
            iMaxWeight = iTempWeight;
        }

        if(iTempWeight < iMinWeight)
        {
            iMinWeight = iTempWeight;
        }
    }

    if(iMinWeight > 0)
    {
        iMaxR = iMaxWeight / iMinWeight;

        if(iMaxR < iMinWeightLimit)
            iMaxR = iMinWeightLimit;

        if(iMaxR > iMaxWeightLimit)
            iMaxR = iMaxWeightLimit;
    }
    else
    {
        iMaxR = 1;
        iMaxWeight = 1;
    }

    for(size_t i = 0;i < iWeightProxySize; i++)
    {
        int iWeight = (vWeight[i] * iMaxR) / iMaxWeight;

        if(iWeight > 0)
        {
            iMaxRouterR += iWeight;

            mIdToWeight.insert(map<size_t, int>::value_type(i, iWeight));

            mWeightToId.insert(make_pair(iWeight, i));
        }
        else
        {
            _staticRouterCache.push_back(i);
        }
        
        TLOGTARS("EndpointManager::dispatchEndpointCache _objName:" << _objName << "|endpoint:" << _activeWeightProxy[i]->endpoint().desc() << "|iWeightR:" << iWeight << endl);
    }

    for(size_t i = 0; i < iMaxRouterR; i++)
    {
        bool bFirst = true;
        multimap<int, size_t> mulTemp;
        multimap<int, size_t>::reverse_iterator mIter = mWeightToId.rbegin();
        while(mIter != mWeightToId.rend())
        {
            if(bFirst)
            {
                bFirst = false;

                _staticRouterCache.push_back(mIter->second);

                mulTemp.insert(make_pair((mIter->first - iMaxRouterR + mIdToWeight[mIter->second]), mIter->second));
            }
            else
            {
                mulTemp.insert(make_pair((mIter->first + mIdToWeight[mIter->second]), mIter->second));
            }

            mIter++;
        }

        mWeightToId.clear();
        mWeightToId.swap(mulTemp);
    }
}

/////////////////////////////////////////////////////////////////////////////
EndpointThread::EndpointThread(Communicator* pComm, const string & sObjName, GetEndpointType type, const string & sName, bool bFirstNetThread)
: QueryEpBase(pComm,bFirstNetThread,true)
, _type(type)
, _name(sName)
{
    init(sObjName, "", true);
}

void EndpointThread::getEndpoints(vector<EndpointInfo> &activeEndPoint, vector<EndpointInfo> &inactiveEndPoint)
{
    //直连调用这个接口无效
    if(_direct)
    {
        return ;
    }

    {
        TC_LockT<TC_ThreadMutex> lock(_mutex);

        refreshReg(_type,_name);
    
        activeEndPoint = _activeEndPoint;
        inactiveEndPoint = _inactiveEndPoint;
    }
}

void EndpointThread::getTCEndpoints(vector<TC_Endpoint> &activeEndPoint, vector<TC_Endpoint> &inactiveEndPoint)
{
    //直连调用这个接口无效
    if(_direct)
    {
        return ;
    }

    {
    
        TC_LockT<TC_ThreadMutex> lock(_mutex);
    
        refreshReg(_type,_name);
    
        activeEndPoint = _activeTCEndPoint;
        inactiveEndPoint = _inactiveTCEndPoint;
    }
}

void EndpointThread::notifyEndpoints(const set<EndpointInfo> & active,const set<EndpointInfo> & inactive,bool bSync)
{
    if(!bSync)
    {
        TC_LockT<TC_ThreadMutex> lock(_mutex);

        update(active, inactive);
    }
    else
    {
        update(active, inactive);
    }
}

void EndpointThread::update(const set<EndpointInfo> & active, const set<EndpointInfo> & inactive)
{
    _activeEndPoint.clear();
    _inactiveEndPoint.clear();

    _activeTCEndPoint.clear();
    _inactiveTCEndPoint.clear();

    set<EndpointInfo>::iterator iter= active.begin();
    for(;iter != active.end(); ++iter)
    {
//        TC_Endpoint ep = (iter->host(), iter->port(), 3000, iter->type(), iter->grid());

        _activeTCEndPoint.push_back(iter->getEndpoint());

        _activeEndPoint.push_back(*iter);
    }

    iter = inactive.begin();
    for(;iter != inactive.end(); ++iter)
    {
//        TC_Endpoint ep(iter->host(), iter->port(), 3000, iter->type(), iter->grid());

        _inactiveTCEndPoint.push_back(iter->getEndpoint());

        _inactiveEndPoint.push_back(*iter);
    }
}
/////////////////////////////////////////////////////////////////////////////
EndpointManagerThread::EndpointManagerThread(Communicator * pComm,const string & sObjName)
:_communicator(pComm)
,_objName(sObjName)
{
}
EndpointManagerThread::~EndpointManagerThread()
{
    map<string,EndpointThread*>::iterator iter;
    for(iter=_info.begin();iter != _info.end();iter++)
    {
        if(iter->second)
        {
            delete iter->second;
            iter->second = NULL;
        }
    }
}

void EndpointManagerThread::getEndpoint(vector<EndpointInfo> &activeEndPoint, vector<EndpointInfo> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_DEFAULT,"");

    pThread->getEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getEndpointByAll(vector<EndpointInfo> &activeEndPoint, vector<EndpointInfo> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_ALL,"");

    pThread->getEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getEndpointBySet(const string &sName, vector<EndpointInfo> &activeEndPoint, vector<EndpointInfo> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_SET,sName);

    pThread->getEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getEndpointByStation(const string &sName, vector<EndpointInfo> &activeEndPoint, vector<EndpointInfo> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_STATION,sName);

    pThread->getEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getTCEndpoint(vector<TC_Endpoint> &activeEndPoint, vector<TC_Endpoint> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_DEFAULT,"");

    pThread->getTCEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getTCEndpointByAll(vector<TC_Endpoint> &activeEndPoint, vector<TC_Endpoint> &inactiveEndPoint)
{

    EndpointThread * pThread  = getEndpointThread(E_ALL,"");

    pThread->getTCEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getTCEndpointBySet(const string &sName, vector<TC_Endpoint> &activeEndPoint, vector<TC_Endpoint> &inactiveEndPoint)
{
    EndpointThread * pThread  = getEndpointThread(E_SET,sName);

    pThread->getTCEndpoints(activeEndPoint,inactiveEndPoint);
}

void EndpointManagerThread::getTCEndpointByStation(const string &sName, vector<TC_Endpoint> &activeEndPoint, vector<TC_Endpoint> &inactiveEndPoint)
{

    EndpointThread * pThread  = getEndpointThread(E_STATION,sName);

    pThread->getTCEndpoints(activeEndPoint,inactiveEndPoint);
}

EndpointThread * EndpointManagerThread::getEndpointThread(GetEndpointType type,const string & sName)
{
    TC_LockT<TC_SpinLock> lock(_mutex);

    string sAllName = TC_Common::tostr((int)type) + ":" +  sName;

    map<string,EndpointThread*>::iterator iter;
    iter = _info.find(sAllName);
    if(iter != _info.end())
    {
        return iter->second;
    }

    EndpointThread * pThread = new EndpointThread(_communicator, _objName, type, sName);
    _info[sAllName] = pThread;

    return pThread;
}

}


