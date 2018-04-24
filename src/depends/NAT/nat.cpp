#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include "nat.h"

using namespace std;

NAT::NAT()
{
	m_urls = make_shared<UPNPUrls>();
	m_data = make_shared<IGDdatas>();

	memset(m_urls.get(), 0, sizeof(struct UPNPUrls));
	memset(m_data.get(), 0, sizeof(struct IGDdatas));

	m_initialized = false;

	struct UPNPDev* devlist;
	int error = 0;

	devlist = upnpDiscover(2000,NULL,NULL,0,0,&error);

	if(devlist == NULL || error != 0)
	{
		//[LOG ERROR]
		freeUPNPDevlist(devlist);
		return;

	}
	char lan_address[INET6_ADDRSTRLEN];

	int status = UPNP_GetValidIGD(devlist, m_urls.get(), m_data.get(), lan_address, sizeof(lan_address));

	if(status != 1)
	{
		//[LOG Error]
		freeUPNPDevlist(devlist);
		return;
	}
	
	m_lanAddress = lan_address;
	m_initialized = true;
	freeUPNPDevlist(devlist);
}

string NAT::externalIP()
{
	if(!m_initialized)
		return "0.0.0.0";
	char addr[16];

	if (!UPNP_GetExternalIPAddress(m_urls->controlURL, m_data->first.servicetype, addr))
	{
		return addr;
	}
	else 
	{
		//[LOG Error]
		return "0.0.0.0";
	}
}

int NAT::addRedirect(int _port)
{
	if(!m_initialized)
		return -1;

	char port_str[16];
	sprintf(port_str, "%d", _port);
	int error = UPNP_AddPortMapping(m_urls->controlURL, m_data->first.servicetype, port_str, port_str, m_lanAddress.c_str(), "zilliqa", "TCP", NULL, NULL);

	if(!error)
	{
		m_reg.insert(_port);
		return _port;
	}

	if (UPNP_AddPortMapping(m_urls->controlURL, m_data->first.servicetype, port_str, NULL, m_lanAddress.c_str(), "zilliqa", "TCP", NULL, NULL))
		return 0;

	unsigned num = 0;
	UPNP_GetPortMappingNumberOfEntries(m_urls->controlURL, m_data->first.servicetype, &num);
	for (unsigned i = 0; i < num; ++i)
	{
		char extPort[16];
		char intClient[16];
		char intPort[6];
		char protocol[4];
		char desc[80];
		char enabled[4];
		char rHost[64];
		char duration[16];
		UPNP_GetGenericPortMappingEntry(m_urls->controlURL, m_data->first.servicetype, to_string(i).c_str(), extPort, intClient, intPort, protocol, desc, enabled, rHost, duration);
		if (string("zilliqa") == desc)
		{
			m_reg.insert(atoi(extPort));
			return atoi(extPort);
		}
	}

	//[LOG Error]
	
	return -1;

}

void NAT::removeRedirect(int _port)
{
	if(!m_initialized)
	{
		//[LOG]
		return;
	}
	char port_str[16];
	sprintf(port_str, "%d", _port);
	UPNP_DeletePortMapping(m_urls->controlURL, m_data->first.servicetype, port_str, "TCP", NULL);
	m_reg.erase(_port);
}

NAT::~NAT()
{
	auto r = m_reg;
	for (auto i: r)
		removeRedirect(i);
}


//For testing

/*int main()
{
	NAT nt;

	cout<<nt.externalIP()<<endl<<nt.addRedirect(2020)<<endl;

	string s = "";
	while(1)
	{
		cin>>s;
		if(s=="break")
		{
			break;
		}
	}

	return 0;
}*/ 
