#include "utils.hpp"

#include "format.hpp"

namespace wshttp
{
    namespace detail
    {
        using namespace wshttp::literals;

        std::chrono::steady_clock::time_point get_time()
        {
            return std::chrono::steady_clock::now();
        }

        std::string translate_req_type(int t)
        {
            switch (t)
            {
                case EVDNS_TYPE_A:
                    return "IPV4(A)-REQUEST"s;
                case EVDNS_TYPE_CNAME:
                    return "CNAME-REQUEST"s;
                case EVDNS_TYPE_PTR:
                    return "PTR-REQUEST"s;
                case EVDNS_TYPE_AAAA:
                    return "IPV6(AAAA)-REQUEST"s;
                case EVDNS_TYPE_SOA:
                    return "SOA-REQUEST"s;
                default:
                    return "UNKNOWN-REQUEST"s;
            }
        }

        void print_req(struct evdns_server_request* req)
        {
            auto msg = "\n----- INCOMING REQUEST -----\nFlags:{}\nNum questions:{}\n"_format(
                req->flags, req->nquestions ? req->nquestions : 0);

            if (req->nquestions)
            {
                for (int i = 0; i < req->nquestions; ++i)
                {
                    auto* q = req->questions[i];
                    msg += "Question #{}\nName: {} -- Type: {} -- Class: {}"_format(
                        i + 1, q->name, translate_req_type(q->type), translate_req_class(q->dns_question_class));
                }
            }

            log->critical("{}", msg);
        }

        std::string translate_req_class(int t)
        {
            switch (t)
            {
                case EVDNS_CLASS_INET:
                    return "CLASS-INET"s;
                default:
                    return "CLASS-UNKNOWN"s;
            }
        }

        std::string localhost_ip(uint16_t port)
        {
            return "{}:{}"_format(localhost, port);
        }
    }  //  namespace detail
}  //  namespace wshttp
