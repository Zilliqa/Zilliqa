
#include <set>
#include <string>
#include <memory>

struct UPNPUrls;
struct IGDdatas;

class NAT
{
public:
	NAT();
	~NAT();

	std::string externalIP();
	int addRedirect(int port);
	void removeRedirect(int port);
	bool isIntialized() const {return m_initialized;}

private:
	std::set<unsigned int> m_reg;
	bool m_initialized;
	std::string m_lanAddress;
	std::shared_ptr<UPNPUrls> m_urls;
	std::shared_ptr<IGDdatas> m_data;

};