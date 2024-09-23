#pragma once

#include "encoding.hpp"
#include "format.hpp"
#include "utils.hpp"

#include <ada.h>

namespace wshttp
{
    using namespace wshttp::literals;
    class stream;

    using url_result = ada::result<ada::url_aggregator>;

    static constexpr auto https_proto = "https"sv;

    struct uri;

    class url_parser
    {
        std::unique_ptr<std::string> _data{};

        std::unique_ptr<url_result> _res;

        url_parser() = default;

      public:
        static std::shared_ptr<url_parser> make();

        bool read(std::string input);

        void print_aggregates();

        uri extract();

        url_result& url();

        std::string href_str();
        std::string_view href_sv();

      private:
        url_result& _url();  // does no safety checking
        bool _parse();
        void _reset();
    };

    // global parser
    extern std::shared_ptr<url_parser> parser;

    namespace detail
    {
        template <typename SV>
            requires std::same_as<SV, ustring_view> || std::same_as<SV, bstring_view>
        inline std::string_view to_sv(SV x)
        {
            return {reinterpret_cast<const char*>(x.data()), x.size()};
        }

        inline const char* current_error()
        {
            return ERR_error_string(ERR_get_error(), NULL);
        }

        inline void parse_addr(int af, void* dest, const std::string& from)
        {
            auto rv = inet_pton(af, from.c_str(), dest);

            if (rv == 0)  // inet_pton returns this on invalid input
                throw std::invalid_argument{"Unable to parse IP address!"};
            if (rv < 0)
                throw std::system_error{errno, std::system_category()};
        }

        inline constexpr std::string_view translate_dns_req_class(int t)
        {
            switch (t)
            {
                case EVDNS_CLASS_INET:
                    return "CLASS-INET"sv;
                default:
                    return "CLASS-UNKNOWN"sv;
            }
        }

        inline void print_dns_req(struct evdns_server_request* req)
        {
            auto msg = "\n----- INCOMING REQUEST -----\nFlags: {}\nNum questions: {}\n"_format(
                req->flags, req->nquestions ? req->nquestions : 0);

            if (req->nquestions)
            {
                for (int i = 0; i < req->nquestions; ++i)
                {
                    auto* q = req->questions[i];
                    msg += "Question #{}:\nName: {} -- Type: {} -- Class: {}"_format(
                        i + 1, q->name, translate_req_type(q->type), translate_dns_req_class(q->dns_question_class));
                }
            }

            log->critical("{}", msg);
        }
    }  // namespace detail

    struct loop_callbacks
    {
        static void exec_oneshot(evutil_socket_t fd, short, void* user_arg);
        static void exec_iterative(evutil_socket_t fd, short, void* user_arg);
    };

    struct ctx_callbacks
    {
        static int server_select_alpn_proto_cb(
            SSL*,
            const unsigned char** out,
            unsigned char* outlen,
            const unsigned char* in,
            unsigned int inlen,
            void* arg);
    };

    struct dns_callbacks
    {
        static void server_cb(struct evdns_server_request* req, void* user_data);
    };

    struct listen_callbacks
    {
        static void accept_cb(
            struct evconnlistener* evconn, evutil_socket_t fd, struct sockaddr* addr, int addrlen, void* user_arg);
        static void error_cb(struct evconnlistener* evconn, void* user_arg);
    };

    struct session_callbacks
    {
        // static void server_event_cb(struct bufferevent* bev, short events, void* user_arg);
        static void event_cb(struct bufferevent* bev, short events, void* user_arg);
        static void read_cb(struct bufferevent* bev, void* user_arg);
        static void write_cb(struct bufferevent* bev, void* user_arg);

        static nghttp2_ssize send_callback(
            nghttp2_session* session, const uint8_t* data, size_t length, int flags, void* user_arg);
        static int on_frame_send_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_data);
        static int on_data_chunk_recv_callback(
            nghttp2_session* session,
            uint8_t flags,
            int32_t stream_id,
            const uint8_t* data,
            size_t len,
            void* user_data);
        static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_arg);
        static int on_stream_close_callback(
            nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_arg);
        static int on_header_callback(
            nghttp2_session* session,
            const nghttp2_frame* frame,
            const uint8_t* name,
            size_t namelen,
            const uint8_t* value,
            size_t valuelen,
            uint8_t flags,
            void* user_arg);
        static int on_begin_headers_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_arg);
    };

    struct stream_callbacks
    {
        static ssize_t file_read_callback(
            nghttp2_session* session,
            int32_t stream_id,
            uint8_t* buf,
            size_t length,
            uint32_t* data_flags,
            nghttp2_data_source* source,
            void* user_data);
    };

    struct buffer_printer
    {
        std::basic_string_view<std::byte> buf;

        // Constructed from any type of string_view<T> for a single-byte T (char, std::byte, uint8_t, etc.)
        template <concepts::basic_char T>
        explicit buffer_printer(std::basic_string_view<T> buf)
            : buf{reinterpret_cast<const std::byte*>(buf.data()), buf.size()}
        {}

        // Constructed from any type of lvalue string<T> for a single-byte T (char, std::byte, uint8_t, etc.
        template <concepts::basic_char T>
        explicit buffer_printer(const std::basic_string<T>& buf) : buffer_printer(std::basic_string_view<T>{buf})
        {}

        // *Not* constructable from a string<T> rvalue (no taking ownership)
        template <concepts::basic_char T>
        explicit buffer_printer(std::basic_string<T>&& buf) = delete;

        // Constructable from a (T*, size) argument pair, for byte-sized T's.
        template <concepts::basic_char T>
        explicit buffer_printer(const T* data, size_t size) : buffer_printer(std::basic_string_view<T>{data, size})
        {}

        std::string to_string() const
        {
            auto& b = buf;
            std::string out;
            auto ins = std::back_inserter(out);
            fmt::format_to(ins, "Buffer[{}/{:#x} bytes]:", b.size(), b.size());

            for (size_t i = 0; i < b.size(); i += 32)
            {
                fmt::format_to(ins, "\n{:04x} ", i);

                size_t stop = std::min(b.size(), i + 32);
                for (size_t j = 0; j < 32; j++)
                {
                    auto k = i + j;
                    if (j % 4 == 0)
                        out.push_back(' ');
                    if (k >= stop)
                        out.append("  ");
                    else
                        fmt::format_to(ins, "{:02x}", std::to_integer<uint_fast16_t>(b[k]));
                }
                out.append("  ┃");
                for (size_t j = i; j < stop; j++)
                {
                    auto c = std::to_integer<char>(b[j]);
                    if (c == 0x00)
                        out.append("∅");
                    else if (c < 0x20 || c > 0x7e)
                        out.append("·");
                    else
                        out.push_back(c);
                }
                out.append("┃");
            }
            return out;
        }

        static constexpr bool to_string_formattable = true;
    };

}  // namespace wshttp
