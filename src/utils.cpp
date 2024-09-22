#include "utils.hpp"

#include "internal.hpp"

namespace wshttp
{
    namespace detail
    {
        using namespace wshttp::literals;

        std::chrono::steady_clock::time_point get_time()
        {
            return std::chrono::steady_clock::now();
        }

        std::string_view translate_req_type(int t)
        {
            switch (t)
            {
                case EVDNS_TYPE_A:
                    return "IPV4(A)-REQUEST"sv;
                case EVDNS_TYPE_CNAME:
                    return "CNAME-REQUEST"sv;
                case EVDNS_TYPE_PTR:
                    return "PTR-REQUEST"sv;
                case EVDNS_TYPE_AAAA:
                    return "IPV6(AAAA)-REQUEST"sv;
                case EVDNS_TYPE_SOA:
                    return "SOA-REQUEST"sv;
                default:
                    return "UNKNOWN-REQUEST"sv;
            }
        }

        std::string localhost_ip(uint16_t port)
        {
            return "{}:{}"_format(localhost, port);
        }
    }  //  namespace detail
}  //  namespace wshttp
