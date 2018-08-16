#include "IPConverter.h"
#include <arpa/inet.h>

const std::string
IPConverter::ToStrFromNumercialIP(const boost::multiprecision::uint128_t& ip)
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = ip.convert_to<unsigned long>();
    return inet_ntoa(serv_addr.sin_addr);
}

const boost::multiprecision::uint128_t
IPConverter::ToNumercialIPFromStr(std::string ipStr)
{
    struct in_addr ip_addr;
    inet_aton(ipStr.c_str(), &ip_addr);
    return (boost::multiprecision::uint128_t)ip_addr.s_addr;
}