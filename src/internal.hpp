#pragma once

#include "format.hpp"
#include "utils.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    class _parser;

    extern std::shared_ptr<_parser> parser;

    class _parser
    {
      protected:
        std::optional<std::string> _data;

        virtual bool _parse() = 0;

      public:
        virtual ~_parser() = default;

        static std::shared_ptr<_parser> make();

        virtual bool read(const std::string &input)
        {
            _data.emplace(input);
            log->critical("{}", _data.value_or("YOU FUKED IT"));
            if (_parse())
                return true;
            _data.reset();
            return false;
        }

        virtual void print_aggregates() = 0;
    };

    namespace concepts
    {
        template <typename T>
        concept ParserType = std::is_base_of_v<_parser, T>;
    }  // namespace concepts

    namespace detail
    {
        template <concepts::ParserType parser_t>
        std::shared_ptr<parser_t> make_parser()
        {
            static std::shared_ptr<parser_t> p;
            if (not p)
                p = std::static_pointer_cast<parser_t>(_parser::make());
            return p;
        }

        template <std::integral T>
        T *increment_ptr(T *ptr, int inc)
        {
            return ptr + (inc * sizeof(decltype(*ptr)));
        }

        // Wrapper around inet_pton that throws an exception on error
        inline void parse_addr(int af, void *dest, const std::string &from)
        {
            auto rv = inet_pton(af, from.c_str(), dest);

            if (rv == 0)  // inet_pton returns this on invalid input
                throw std::invalid_argument{"Unable to parse IP address!"};
            if (rv < 0)
                throw std::system_error{errno, std::system_category()};
        }
    }  // namespace detail

    struct callbacks
    {
        static int server_select_alpn_proto_cb(
            SSL *,
            const unsigned char **out,
            unsigned char *outlen,
            const unsigned char *in,
            unsigned int inlen,
            void *arg);

        static void dns_server_cb(struct evdns_server_request *req, void *user_data);
    };

}  // namespace wshttp
