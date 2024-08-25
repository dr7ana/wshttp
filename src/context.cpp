#include "context.hpp"

#include <openssl/err.h>

namespace wshttp
{
    std::unique_ptr<app_context> app_context::make_context()
    {
        return std::unique_ptr<app_context>{new app_context{}};
    }

    app_context::app_context() {}

    void app_context::_init_internals()
    {
        auto ssl = SSL_CTX_new(TLS_server_method());
        if (not ssl)
            throw std::runtime_error{
                "Failed to create SSL Context: {}"_format(ERR_error_string(ERR_get_error(), NULL))};
        _ctx = std::unique_ptr<::SSL_CTX, decltype(ssl_deleter)>{ssl, ssl_deleter};
    }
}  //  namespace wshttp
