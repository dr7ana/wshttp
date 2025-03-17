#include "ws.hpp"

#include "endpoint.hpp"
#include "internal.hpp"

namespace wshttp
{
    namespace detail
    {
        static ws_session_base* _get_session(void* user_arg)
        {
            return static_cast<ws_session_base*>(user_arg);
        }
    }  // namespace detail

    void ws_callbacks::msg_cb(
            struct evws_connection* /* evws */, int type, const unsigned char* data, size_t len, void* user_arg)
    {
        return detail::_get_session(user_arg)->recv_msg(type, uspan{data, len});
    }

    void ws_callbacks::close_cb(struct evws_connection* /* evws */, void* user_arg)
    {
        return detail::_get_session(user_arg)->close_session();
    }

    ws_session_base::ws_session_base(listener& l, ip_address remote, evhttp_request* req) :
            _l{l}, _path{ip_address{}, std::move(remote)}, _fd{detail::get_request_fd(req)}
    {
        log->debug("New WS session has fd: {}", _fd);

        _ws.reset(evws_new_session(req, ws_callbacks::msg_cb, this, 0));

        if (not _ws)
            throw std::runtime_error{"Failed to create new WS session!"};

        _path._local = ip_address::from_socket(_fd);

        log->info("Successfully configured WS session; path: {}", _path);
    }

    static constexpr auto close_ws_msg = "/quit"_usp;

    void ws_session_base::recv_msg(int /* type */, uspan data)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (data == close_ws_msg)
        {
            log->info("Received close for WS session (remote:{})", _path.remote());
            evws_close(_ws.get(), WS_CR_NORMAL);
        }
        else
            log->info("Received WS session data: {}", buffer_printer{data});
    }

    void ws_session_base::close_session()
    {
        log->info("Signalling listener to close WS session (remote:{})", _path.remote());

        _l._ep.loop()->call_soon([this, remote = _path.remote()]() mutable { _l.close_ws_session(remote); });
    }
}  // namespace wshttp
