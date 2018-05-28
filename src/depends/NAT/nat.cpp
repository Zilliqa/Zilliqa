#include "nat.h"
#include "libUtils/Logger.h"
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <cstring>
#include <iostream>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

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

#if MINIUPNPC_API_VERSION >= 14
    devlist = upnpDiscover(2000, NULL, NULL, 0, 0, 2, &error);
#else
    devlist = upnpDiscover(2000, NULL, NULL, 0, 0, &error);
#endif

    if (devlist == NULL || error != 0)
    {
        LOG_GENERAL(WARNING, "devlist empty or error in discovery");
        freeUPNPDevlist(devlist);
        return;
    }
    char lan_address[INET6_ADDRSTRLEN];

    int status = UPNP_GetValidIGD(devlist, m_urls.get(), m_data.get(),
                                  lan_address, sizeof(lan_address));

    if (status != 1)
    {
        LOG_GENERAL(WARNING, "Unable to get Valid IGD");
        freeUPNPDevlist(devlist);
        return;
    }

    m_lanAddress = lan_address;
    m_initialized = true;
    freeUPNPDevlist(devlist);
}

string NAT::externalIP()
{
    if (!m_initialized)
        return "0.0.0.0";
    char addr[16];

    if (!UPNP_GetExternalIPAddress(m_urls->controlURL,
                                   m_data->first.servicetype, addr))
    {
        return addr;
    }
    else
    {
        LOG_GENERAL(WARNING, "Unable to get external IP");
        return "0.0.0.0";
    }
}

int NAT::addRedirect(int _port)
{
    if (!m_initialized)
        return -1;

    char port_str[16];
    sprintf(port_str, "%d", _port);
    int error = UPNP_AddPortMapping(
        m_urls->controlURL, m_data->first.servicetype, port_str, port_str,
        m_lanAddress.c_str(), "zilliqa", "TCP", NULL, NULL);

    if (!error)
    {
        m_reg.insert(_port);
        return _port;
    }
    else
    {
        LOG_GENERAL(INFO, "Failed to map same port in router");
    }

    if (UPNP_AddPortMapping(m_urls->controlURL, m_data->first.servicetype,
                            port_str, NULL, m_lanAddress.c_str(), "zilliqa",
                            "TCP", NULL, NULL))
    {
        LOG_GENERAL(WARNING, "Failed to map Port");
        return 0;
    }

    unsigned num = 0;
    UPNP_GetPortMappingNumberOfEntries(m_urls->controlURL,
                                       m_data->first.servicetype, &num);
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
        UPNP_GetGenericPortMappingEntry(
            m_urls->controlURL, m_data->first.servicetype, to_string(i).c_str(),
            extPort, intClient, intPort, protocol, desc, enabled, rHost,
            duration);
        if (string("zilliqa") == desc)
        {
            m_reg.insert(atoi(extPort));
            return atoi(extPort);
        }
    }

    LOG_GENERAL(WARNING, "Failed to find port");

    return -1;
}

void NAT::removeRedirect(int _port)
{
    if (!m_initialized)
    {
        LOG_GENERAL(WARNING, "Unitialized");
        return;
    }
    char port_str[16];
    sprintf(port_str, "%d", _port);
    UPNP_DeletePortMapping(m_urls->controlURL, m_data->first.servicetype,
                           port_str, "TCP", NULL);
    m_reg.erase(_port);
}

NAT::~NAT()
{
    auto r = m_reg;
    for (auto i : r)
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
