/*
	This file is part of cpp-ethereum.
	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/// @file
/// UPnP port forwarding support.

#include "nat.h"
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <cstring>
#include <iostream>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

using namespace std;

namespace
{
    const int DISCOVERY_TIME_OUT = 2000;

#if MINIUPNPC_API_VERSION >= 14
    const int TTL = 2;
#endif //MINIUPNPC_API_VERSION

}

NAT::NAT()
{
    m_urls = make_shared<UPNPUrls>();
    m_data = make_shared<IGDdatas>();

    memset(m_urls.get(), 0, sizeof(struct UPNPUrls));
    memset(m_data.get(), 0, sizeof(struct IGDdatas));

    m_initialized = false;
}

NAT::~NAT()
{
    auto regCopySafeToIterateDuringDelete = m_reg;
    for (auto i : regCopySafeToIterateDuringDelete)
    {
        removeRedirect(i);
    }
}

void NAT::init()
{
    shared_ptr<struct UPNPDev> devlist;
    int error = 0;
    
#if MINIUPNPC_API_VERSION >= 14
        devlist.reset(upnpDiscover(DISCOVERY_TIME_OUT, NULL/*multicast interface*/, NULL/*minissdpd socket path*/, 0/*sameport*/, 0/*ipv6*/, TTL, &error));
#else
        devlist.reset(upnpDiscover(DISCOVERY_TIME_OUT, NULL/*multicast interface*/, NULL/*minissdpd socket path*/, 0/*sameport*/, 0/*ipv6*/, &error));
#endif //MINIUPNPC_API_VERSION

    if (devlist.get() == NULL || error != 0)
    {
        return;
    }

    char lan_address[INET6_ADDRSTRLEN];

    int status = UPNP_GetValidIGD(devlist.get(), m_urls.get(), m_data.get(),
                                  lan_address, sizeof(lan_address));

    if (status != 1)
    {
        return;
    }

    m_lanAddress = lan_address;
    m_initialized = true;
}


string NAT::externalIP()
{
    if (!m_initialized)
    {
        return "0.0.0.0";
    }

    char addr[16];

    if (!UPNP_GetExternalIPAddress(m_urls->controlURL,
                                   m_data->first.servicetype, addr))
    {
        return addr;
    }
    else
    {
        return "0.0.0.0";
    }
}

int NAT::addRedirect(int _port)
{
    if (!m_initialized)
    {
        return -1;
    }

    if ((_port < 0) || (_port > 65535))
    {
        return -1;
    }

    string port_str = to_string(_port);

    // Remove any dangling mapping
    UPNP_DeletePortMapping(m_urls->controlURL, m_data->first.servicetype,
                           port_str.c_str(), "TCP", NULL);

	// 1) Try direct mapping first (port external, port internal).
    int error = UPNP_AddPortMapping(
        m_urls->controlURL, m_data->first.servicetype, port_str.c_str(), port_str.c_str(),
        m_lanAddress.c_str(), "zilliqa", "TCP", NULL, NULL);

    if (!error)
    {
        m_reg.insert(_port);
        return _port;
    }
    
    // 2) Failed - now try (random external, port internal) and cycle up to 10 times.
	srand(static_cast<unsigned int>(time(nullptr)));
	for (unsigned i = 0; i < 10; ++i)
	{
		_port = rand() % (32768 - 1024) + 1024;
		string ext_port_str = to_string(_port);

        UPNP_DeletePortMapping(m_urls->controlURL, m_data->first.servicetype,
                           ext_port_str.c_str(), "TCP", NULL);

		if (!UPNP_AddPortMapping(m_urls->controlURL, m_data->first.servicetype, ext_port_str.c_str(), port_str.c_str(), m_lanAddress.c_str(), "zilliqa", "TCP", NULL, NULL))
        {
            m_reg.insert(_port);
            return _port;
        }
	}

	// 3) Failed. Try asking the router to give us a free external port.
    // This may not work on some routers.
    char reservedPort[6];
    int result = UPNP_AddAnyPortMapping(m_urls->controlURL, m_data->first.servicetype,
                            port_str.c_str(), port_str.c_str(), m_lanAddress.c_str(), "zilliqa",
                            "TCP", NULL, NULL, reservedPort);

    if (result)
    {
        return -1;
    }
    else
    {   int obtainedPort = atoi(reservedPort);
        m_reg.insert(obtainedPort);
        return obtainedPort;
    }
}

void NAT::removeRedirect(int _port)
{
    if (!m_initialized)
    {
        return;
    }
    
    if ((_port < 0) || (_port > 65535))
    {
        return;
    }

    string port_str = to_string(_port);
    UPNP_DeletePortMapping(m_urls->controlURL, m_data->first.servicetype,
                           port_str.c_str(), "TCP", NULL);
    m_reg.erase(_port);
}

