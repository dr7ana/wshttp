#pragma once

#include "types.hpp"

#include <tuple>
#include <utility>

namespace wshttp
{
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

    using request_data_cb = std::function<void(std::vector<char>)>;

    template <typename F, typename... Arg>
    concept void_func = std::invocable<F, Arg...> && std::is_void_v<std::invoke_result_t<F, Arg...>>;

    template <typename F>
    concept request_func = void_func<F, std::vector<char>>;

    using inbound_generic_handler = std::function<void(METHOD, struct evhttp_request*)>;
    using inbound_method_handler = std::function<void(struct evhttp_request*)>;

    namespace detail
    {
        template <typename T, typename E>
        concept is_scoped_enum_or_underlying_uint =
                (std::is_scoped_enum_v<T> && std::is_same_v<T, E>) || std::is_unsigned_v<T>;

        template <typename T, typename... Arg>
        concept require_one_of_is = (0 + ... + std::is_same_v<std::remove_cv_t<T>, std::remove_cvref_t<Arg>>) == 1;

        // is an option exclusive to sessions
        template <typename Arg>
        concept session_only_opt =
                (request_func<std::remove_cvref_t<Arg>> ||
                 is_scoped_enum_or_underlying_uint<std::remove_cvref_t<Arg>, content_type>);

        // is an option exclusive to requests
        template <typename Arg>
        concept request_only_opt = (is_scoped_enum_or_underlying_uint<std::remove_cvref_t<Arg>, hdr_flags>);

    }  // namespace detail

    template <typename... Arg>
    concept session_opt_types = ((detail::session_only_opt<Arg> && !detail::request_only_opt<Arg>), ...);

    template <typename... Arg>
    concept request_opt_types = ((detail::request_only_opt<Arg> || detail::session_only_opt<Arg>), ...);

    struct inbound_opts final
    {
        template <typename... Arg>
        inbound_opts(Arg&&... args)
        {
            ((void)handle_iopt(std::forward<Arg>(args)...));
        }

        std::unordered_map<METHOD, inbound_method_handler> handlers;
        inbound_generic_handler generic_handler;

        void handle_iopt(std::pair<METHOD, inbound_method_handler> method_handler)
        {
            handlers.emplace(std::move(method_handler));
        }

        void handle_iopo(inbound_generic_handler generic) { generic_handler = std::move(generic); }

        friend class listener;
        friend struct inbound_session;
    };

    struct session_opts
    {
        friend class outbound_session;
        friend struct http_request;

        template <typename... Arg>
        session_opts(Arg... args)
        {
            ((void)handle_sopt(std::forward<Arg>(args)), ...);
        }

        virtual ~session_opts() = default;

        // template <typename... Arg>
        // static std::unique_ptr<session_opts> make(Arg... args)
        // {
        //     return std::make_unique<session_opts>(std::forward<Arg>(args)...);
        // }

        std::optional<content_type> media_type{};
        std::optional<request_data_cb> data_cb;

        virtual void handle_sopt(content_type t) { media_type = t; }
        virtual void handle_sopt(request_data_cb cb) { data_cb = std::move(cb); }

      protected:
        uint8_t flags{};
        virtual void handle_sopt(uint8_t f) { flags |= f; }
        virtual void handle_sopt(hdr_flags f) { flags |= std::to_underlying(f); }
    };

    struct request_opts : public session_opts
    {
        friend class outbound_session;

        using session_opts::session_opts;

        // template <typename... Arg>
        // static std::unique_ptr<request_opts> make(Arg... args)
        // {
        //     return std::make_unique<request_opts>(std::forward<Arg>(args)...);
        // }
    };

    // struct request_opts
    // {
    //     template <typename... Arg>
    //     request_opts(Arg... args)
    //     {
    //         ((void)handle_sopt(std::forward<Arg>(args)), ...);
    //     }

    //     template <typename... Arg>
    //     static std::unique_ptr<request_opts> make(Arg... args)
    //     {
    //         return std::make_unique<request_opts>(std::forward<Arg>(args)...);
    //     }

    //     std::optional<content_type> media_type{};
    //     std::optional<request_data_cb> data_cb;
    //     uint8_t flags{};

    //     void handle_sopt(content_type t) { media_type = t; }
    //     void handle_sopt(request_data_cb cb) { data_cb = std::move(cb); }
    //     void handle_sopt(uint8_t f) { flags = f; }
    //     void handle_sopt(hdr_flags f) { flags = std::to_underlying(f); }
    // };

}  // namespace wshttp
