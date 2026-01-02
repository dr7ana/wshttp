#pragma once

#include "address.hpp"
#include "loop.hpp"

extern "C" {
#include <event2/dns.h>
#include <event2/dns_struct.h>
}

namespace wshttp {
    struct dns_callbacks;
    class endpoint;

    namespace dns {
        using request_id = uint32_t;
        using dns_request_cb = std::function<void()>;

        struct dns_request {
            friend class server;
            friend struct dnsreq_ptr_hash;
            friend struct dnsreq_ptr_comp;

            static std::unique_ptr<dns_request> make(request_id rid, dns_request_cb cb) {
                return std::unique_ptr<dns_request>(new dns_request{rid, std::move(cb)});
            }

          protected:
            explicit dns_request(request_id rid, dns_request_cb cb) : id{rid}, hook{std::move(cb)} {}

          public:
            request_id id{};
            dns_request_cb hook = nullptr;

            auto operator<=>(const dns_request& r) const { return id <=> r.id; }
            bool operator==(const dns_request& r) const { return id == r.id; }
            bool operator==(request_id rid) const { return id == rid; }
        };

        struct dnsreq_ptr_comp {
            using is_transparent = void;

            bool operator()(
                    const std::unique_ptr<dns_request>& lhs, const std::unique_ptr<dns_request>& rhs) const noexcept {
                return *lhs == *rhs;
            }

            bool operator()(const std::unique_ptr<dns_request>& lhs, request_id rhs) const noexcept {
                return *lhs == rhs;
            }

            bool operator()(request_id lhs, const std::unique_ptr<dns_request>& rhs) const noexcept {
                return *rhs == lhs;
            }
        };

        struct dnsreq_ptr_hash {
            using is_transparent = void;
            using transparent_key_eq = dnsreq_ptr_comp;

            size_t operator()(const std::unique_ptr<dns_request>& r) const noexcept { return std::hash<int>{}(r->id); }

            size_t operator()(request_id id) const noexcept { return std::hash<int>{}(id); }
        };

        // Holds unique pointers to dns_request objects, which are searchable by unique request id's as transparent keys
        using dnsreq_ptr_set =
                std::unordered_set<std::unique_ptr<dns_request>, dnsreq_ptr_hash, dnsreq_ptr_hash::transparent_key_eq>;

        class server final {
            friend class wshttp::endpoint;
            friend struct wshttp::dns_callbacks;

          public:
            server() = delete;
            server(wshttp::endpoint& e);

            [[nodiscard]] static std::unique_ptr<server> make(wshttp::endpoint& e);

            ~server() = default;

          private:
            wshttp::endpoint& _ep;

            std::shared_ptr<evdns_base> _evdns;

            request_id next_request_id{};

            dnsreq_ptr_set _requests;

            void _dns_request_init(dns_request_cb hook, domain_host host);

            void _close_request(request_id req_id);

          protected:
            void gai_request_result(int err, struct evutil_addrinfo* ai);

          public:
            template <typename T>
                requires std::same_as<T, ::evdns_base>
            operator const T*() const {
                return _evdns.get();
            }

            template <typename T>
                requires std::same_as<T, ::evdns_base>
            operator T*() {
                return _evdns.get();
            }

            const std::shared_ptr<evdns_base>& dns() const { return _evdns; }

            // template <typename Callable>
            // void initiate_dns_request(Callable&& cb, domain_host host)
            // {
            //     return _dns_request_init(std::forward<Callable>(cb), std::move(host));
            // }
        };
    }  //  namespace dns
}  // namespace wshttp
