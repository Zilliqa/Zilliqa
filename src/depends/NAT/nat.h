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

	Reference for miniupnp: http://upnp.org/specs/gw/UPnP-gw-WANIPConnection-v2-Service.pdf
*/
/** @file UPnP.h
 * @authors:
 *   Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <memory>
#include <set>
#include <string>

struct UPNPUrls;
struct IGDdatas;

class NAT
{
public:
    NAT();
    ~NAT();

	void init();
    std::string externalIP();
    int addRedirect(int port);
    void removeRedirect(int port);
    bool isIntialized() const { return m_initialized; }

private:

    std::set<unsigned int> m_reg;
    bool m_initialized;
    std::string m_lanAddress;
    std::shared_ptr<UPNPUrls> m_urls;
    std::shared_ptr<IGDdatas> m_data;
};