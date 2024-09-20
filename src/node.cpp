#include "node.hpp"

#include "context.hpp"
#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    void node_old::_init_internals()
    {
        assert(_ep.in_event_loop());
    }

    SSL *node_old::new_ssl()
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            SSL *_ssl;
            _ssl = SSL_new(_ctx->_ctx.get());

            if (!_ssl)
                throw std::runtime_error{"Failed to create SSL/TLS for inbound: {}"_format(detail::current_error())};

            return _ssl;
        });
    }

}  //  namespace wshttp
