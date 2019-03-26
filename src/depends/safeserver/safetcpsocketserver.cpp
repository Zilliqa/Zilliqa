/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    tcpsocketserver.cpp
 * @date    17.07.2015
 * @author  Alexandre Poirot <alexandre.poirot@legrand.fr>
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "safetcpsocketserver.h"
#ifdef __WIN32__
#include "jsonrpccpp/server/connectors/windowstcpsocketserver.h"
#elif __unix__
#include "safelinuxtcpsocketserver.h"
#endif
#include <string>

using namespace jsonrpc;
using namespace std;

SafeTcpSocketServer::SafeTcpSocketServer(const std::string& ipToBind, const unsigned int &port) :
	AbstractServerConnector()
{
#ifdef __WIN32__
	this->realSocket = new WindowsTcpSocketServer(ipToBind, port);
#elif __unix__
	this->realSocket = new SafeLinuxTcpSocketServer(ipToBind, port);
#else
	this->realSocket = NULL;
#endif
}

SafeTcpSocketServer::~SafeTcpSocketServer()
{
	if(this->realSocket != NULL) 
	{
		delete this->realSocket;
		this->realSocket = NULL;
	}
}

bool SafeTcpSocketServer::StartListening()
{
	if(this->realSocket != NULL)
	{
		this->realSocket->SetHandler(this->GetHandler());
		return this->realSocket->StartListening();
	}
	else
		return false;
}

bool SafeTcpSocketServer::StopListening()
{
	if(this->realSocket != NULL)
		return this->realSocket->StopListening();
	else
		return false;
}

bool SafeTcpSocketServer::SendResponse(const string& response, void* addInfo)
{
	if(this->realSocket != NULL)
		return this->realSocket->SendResponse(response, addInfo);
	else
		return false;
}
