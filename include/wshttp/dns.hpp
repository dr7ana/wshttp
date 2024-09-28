#pragma once

#include "concepts.hpp"
#include "loop.hpp"

namespace wshttp
{
    class endpoint;

    namespace dns
    {
        class server final
        {
            friend class wshttp::endpoint;

          public:
            server() = delete;
            explicit server(wshttp::endpoint& e);

            [[nodiscard]] static std::unique_ptr<server> make(wshttp::endpoint& e);

            ~server() = default;

            int main_lookup(struct evdns_server_request* req, struct evdns_server_question* q);

            template <concepts::dns_base T>
            operator const T*() const
            {
                return _evdns.get();
            }

            template <concepts::dns_base T>
            operator T*()
            {
                return _evdns.get();
            }

          private:
            wshttp::endpoint& _ep;

            std::shared_ptr<evdns_server_port> _udp_bind;
            evutil_socket_t _udp_sock;
            std::shared_ptr<evdns_base> _evdns;

            const std::shared_ptr<evdns_base>& dns() const { return _evdns; }

            void initialize();

            void register_nameserver(uint16_t port);
        };
    }  //  namespace dns
}  // namespace wshttp
