#pragma once

#include "types.hpp"

namespace wshttp
{
    class endpoint;

    namespace auth
    {
        enum class ALPN { NONE, UNVALIDATED, VALIDATED };
    }  //  namespace auth

    struct ssl_creds
    {
        explicit ssl_creds(const std::string_view& keyfile, const std::string_view& certfile)
            : _keyfile{keyfile}, _certfile{certfile}
        {
            if (_keyfile.empty() or _certfile.empty())
                throw std::invalid_argument{"Empty paths"};
        }

        static std::shared_ptr<ssl_creds> make(const std::string_view& keyfile, const std::string_view& certfile)
        {
            return std::shared_ptr<ssl_creds>{new ssl_creds{keyfile, certfile}};
        }

        const fs::path _keyfile;
        const fs::path _certfile;
    };

    class app_context;
    using ctx_pair = std::pair<std::shared_ptr<app_context>, std::shared_ptr<app_context>>;

    class app_context
    {
        friend struct ctx_callbacks;
        friend class endpoint;
        friend class node_old;
        friend class listener;

        app_context() = default;

        app_context(IO dir, std::shared_ptr<ssl_creds>& c) : _dir{dir}, _creds{c} { _init(); }

        static std::shared_ptr<app_context> make(IO dir, std::shared_ptr<ssl_creds>& c)
        {
            return std::shared_ptr<app_context>{new app_context{dir, c}};
        }

      public:
        app_context(IO dir) : _dir{dir} { _init(); }

        static ctx_pair make_pair(std::shared_ptr<ssl_creds> c)
        {
            return ctx_pair{make(IO::INBOUND, c), make(IO::OUTBOUND, c)};
        }

        template <typename T>
            requires std::is_same_v<T, SSL_CTX>
        operator T*()
        {
            return _ctx.get();
        }

        template <typename T>
            requires std::is_same_v<T, SSL_CTX>
        operator const T*() const
        {
            return _ctx.get();
        }

      private:
        IO _dir;
        std::shared_ptr<ssl_creds> _creds;
        ssl_ctx_ptr _ctx;

        void _init();

        void _init_inbound();
        void _init_inbound(const char* _keyfile, const char* _certfile);

        void _init_outbound();
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
