#pragma once

#include "types.hpp"
#include "utils.hpp"

namespace wshttp
{
    class Endpoint;
    class app_context;

    using ssl_ptr = std::unique_ptr<::SSL_CTX, decltype(ssl_deleter)>;
    using ctx_ptr = std::unique_ptr<app_context>;

    struct ssl_creds
    {
        explicit ssl_creds(const std::string_view& keyfile, const std::string_view& certfile)
            : _keyfile{keyfile}, _certfile{certfile}
        {
            // if (_keyfile.empty() or _certfile.empty())
            //     throw std::invalid_argument{"Empty paths"};
        }

        static std::shared_ptr<ssl_creds> make(const std::string_view& keyfile, const std::string_view& certfile)
        {
            return std::shared_ptr<ssl_creds>{new ssl_creds{keyfile, certfile}};
        }

        const fs::path _keyfile;
        const fs::path _certfile;
    };

    class app_context
    {
        friend class Endpoint;

      public:
        app_context() = delete;

        template <typename... Opt>
        app_context(IO dir, Opt... opts) : _dir{dir}
        {
            handle_io_opt(std::forward<Opt>(opts)...);
            _init();
        }

      private:
        const IO _dir;
        std::shared_ptr<ssl_creds> _creds;
        ssl_ptr _ctx;

        void _init();

        void handle_io_opt(std::shared_ptr<ssl_creds> c);

        void _init_inbound(const char* _keyfile, const char* _certfile);
        void _init_outbound(const char* _keyfile, const char* _certfile);
    };
}  //  namespace wshttp

namespace std
{
    template <>
    struct hash<wshttp::app_context>
    {
        size_t operator()(const wshttp::app_context& c) const noexcept
        {
            // TODO:
            (void)c;
            return {};
        };
    };
}  //  namespace std
