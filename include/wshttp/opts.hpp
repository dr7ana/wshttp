#pragma once

#include "types.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace wshttp {
    enum class METHOD : uint8_t {
        UNSUPPORTED = 0,
        GET = 1,
        POST = 2,
        HEAD = 3,
        PUT = 4,
        DELETE = 5,
        OPTIONS = 6,
        TRACE = 7,
        CONNECT = 8,
        PATCH = 9
    };

    template <typename T>
    concept supported_method = std::is_same_v<T, METHOD> && requires(T a) { std::to_underlying(a) > 0; };

    enum class hdr_flags : uint8_t { CLOSE = 1 << 0 };

    // Media types for request body Content-Type.
    // Defaults to WILDCARD unless overridden.
    enum class content_type : uint8_t {
        WILDCARD,  //  */*
        APP_WC,    //  application/*
        JSON,      //  application/json
        MISC,      //  application/misc
        U8STREAM,  //  application/octet-stream
        XURL,      //  application/x-www-form-urlencoded
        XML,       //  application/xml
        TEXT_WC,   //  text/*
        CSS,       //  text/css
        STREAM,    //  text/event-stream
        HTML,      //  text/html
        PLAIN      //  text/plain
    };

    // Accept header override (independent from Content-Type).
    struct accept_type {
        content_type value;
    };

    // User-Agent header override; if unset, no User-Agent is sent.
    struct user_agent {
        std::string value;
    };

    // Connection-level timeout (applies to outbound session only).
    struct connection_timeout {
        std::chrono::seconds value;
    };

    // Connection-level retry count for outbound sessions (-1 = infinite).
    struct retry_count {
        int value;
    };

    // IP family hint for outbound DNS/connection resolution.
    enum class ip_family : uint8_t { ANY = 0, IPV4 = 4, IPV6 = 6 };

    using request_data_cb = std::function<void(std::vector<char>)>;

    template <typename F, typename... Arg>
    concept void_func = std::invocable<F, Arg...> && std::is_void_v<std::invoke_result_t<F, Arg...>>;

    template <typename F>
    concept request_func = void_func<F, std::vector<char>>;

    using inbound_generic_handler = std::function<void(METHOD, struct evhttp_request*)>;
    using inbound_method_handler = std::function<void(struct evhttp_request*)>;

    namespace detail {
        template <typename T, typename E>
        concept is_scoped_enum_or_underlying_uint =
                (std::is_scoped_enum_v<T> && std::is_same_v<T, E>) || std::is_unsigned_v<T>;

        template <typename T, typename... Arg>
        concept require_one_of_is = (0 + ... + std::is_same_v<std::remove_cv_t<T>, std::remove_cvref_t<Arg>>) == 1;

        // is an option exclusive to sessions
        template <typename Arg>
        concept session_only_opt =
                (request_func<std::remove_cvref_t<Arg>> ||
                 is_scoped_enum_or_underlying_uint<std::remove_cvref_t<Arg>, content_type> ||
                 std::same_as<std::remove_cvref_t<Arg>, accept_type> ||
                 std::same_as<std::remove_cvref_t<Arg>, user_agent> ||
                 std::same_as<std::remove_cvref_t<Arg>, connection_timeout> ||
                 std::same_as<std::remove_cvref_t<Arg>, retry_count> ||
                 std::same_as<std::remove_cvref_t<Arg>, ip_family>);

        // is an option exclusive to requests
        template <typename Arg>
        concept request_only_opt = (is_scoped_enum_or_underlying_uint<std::remove_cvref_t<Arg>, hdr_flags>);

    }  // namespace detail

    template <typename... Arg>
    concept session_opt_types = ((detail::session_only_opt<Arg> && !detail::request_only_opt<Arg>), ...);

    template <typename... Arg>
    concept request_opt_types = ((detail::request_only_opt<Arg> || detail::session_only_opt<Arg>), ...);

    struct inbound_opts final {
        template <typename... Arg>
        inbound_opts(Arg&&... args) {
            ((void)handle_iopt(std::forward<Arg>(args)), ...);
        }

        std::unordered_map<METHOD, inbound_method_handler> handlers;
        inbound_generic_handler generic_handler;

        void handle_iopt(std::pair<METHOD, inbound_method_handler> method_handler) {
            handlers.emplace(std::move(method_handler));
        }

        void handle_iopt(inbound_generic_handler generic) { generic_handler = std::move(generic); }

        friend class listener;
        friend struct inbound_session;
    };

    struct session_opts {
        friend class outbound_session;
        friend struct http_request;

        template <typename... Arg>
        session_opts(Arg... args) {
            ((void)handle_sopt(std::forward<Arg>(args)), ...);
        }

        virtual ~session_opts() = default;

        std::optional<content_type> media_type{};
        std::optional<content_type> accept{};
        std::optional<std::string> ua{};
        std::optional<std::chrono::seconds> timeout{};
        std::optional<int> retries{};
        std::optional<ip_family> family{};
        std::optional<request_data_cb> data_cb;

        virtual void handle_sopt(content_type t) { media_type = t; }
        virtual void handle_sopt(accept_type t) { accept = t.value; }
        virtual void handle_sopt(user_agent a) { ua = std::move(a.value); }
        virtual void handle_sopt(connection_timeout t) { timeout = t.value; }
        virtual void handle_sopt(retry_count r) { retries = r.value; }
        virtual void handle_sopt(ip_family f) { family = f; }
        virtual void handle_sopt(request_data_cb cb) { data_cb = std::move(cb); }

      protected:
        uint8_t flags{};
        virtual void handle_sopt(uint8_t f) { flags |= f; }
        virtual void handle_sopt(hdr_flags f) { flags |= std::to_underlying(f); }
    };

    struct request_opts : public session_opts {
        friend class outbound_session;

        template <typename... Arg>
        request_opts(Arg&&... args) : session_opts() {
            ((void)handle_ropt(std::forward<Arg>(args)), ...);
        }

        std::optional<std::vector<char>> body{};

        void handle_ropt(content_type t) { session_opts::handle_sopt(t); }
        void handle_ropt(accept_type t) { session_opts::handle_sopt(t); }
        void handle_ropt(user_agent a) { session_opts::handle_sopt(std::move(a)); }
        void handle_ropt(connection_timeout t) { session_opts::handle_sopt(t); }
        void handle_ropt(retry_count r) { session_opts::handle_sopt(r); }
        void handle_ropt(ip_family f) { session_opts::handle_sopt(f); }
        void handle_ropt(request_data_cb cb) { session_opts::handle_sopt(std::move(cb)); }
        void handle_ropt(uint8_t f) { session_opts::handle_sopt(f); }
        void handle_ropt(hdr_flags f) { session_opts::handle_sopt(f); }
        void handle_ropt(std::string_view payload) { body = std::vector<char>(payload.begin(), payload.end()); }
        void handle_ropt(std::vector<char> payload) { body = std::move(payload); }
    };

}  // namespace wshttp
