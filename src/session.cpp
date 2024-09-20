#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "request.hpp"
#include "stream.hpp"

namespace wshttp
{
    static constexpr auto OUTPUT_BLOCK_THRESHOLD{1 << 16};

    static stream *_get_stream(struct nghttp2_session *s, int32_t id)
    {
        return static_cast<stream *>(nghttp2_session_get_stream_user_data(s, id));
    }

    static session &_get_session(void *user_arg)
    {
        return *static_cast<session *>(user_arg);
    }

    void session_callbacks::event_cb(struct bufferevent * /* bev */, short events, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);

        auto msg = "Inbound session (path: {})"_format(s.path());

        if (events & BEV_EVENT_CONNECTED)
        {
            const unsigned char *_alpn = nullptr;
            unsigned int _alpn_len = 0;

            SSL_get0_alpn_selected(s._ssl.get(), &_alpn, &_alpn_len);

            if (not _alpn_len or defaults::ALPN.compare({_alpn, _alpn_len}) == 0)
            {
                log->info(
                    "{} {} alpn; initializing...", msg, _alpn_len ? "successfully negotiated" : "did not negotiate");
                return s.config_send_initial();
            }

            log->warn(
                "{} failed to negotiate 'h2' alpn! Received: {}",
                msg,
                std::string_view{reinterpret_cast<const char *>(_alpn), _alpn_len});
        }
        else if (events & BEV_EVENT_EOF)
            msg += " EOF!";
        else if (events & BEV_EVENT_ERROR)
            msg += " network error!";
        else if (events & BEV_EVENT_TIMEOUT)
            msg += " timed out!";

        log->warn("{}: {}", msg, detail::current_error());

        s.close_session();
    }

    void session_callbacks::read_cb(struct bufferevent * /* bev */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);
        s.read_session_data();
    }

    void session_callbacks::write_cb(struct bufferevent * /* bev */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);
        s.write_session_data();
    }

    nghttp2_ssize session_callbacks::send_callback(
        nghttp2_session * /* session */, const uint8_t *data, size_t length, int /* flags */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);
        return s.send_hook(ustring_view{data, length});
    }

    // int session_callbacks::on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame, void
    // *user_arg)
    // {
    //     log->debug("{} called", __PRETTY_FUNCTION__);
    //     auto &s = _get_session(user_arg);
    // }

    // int session_callbacks::on_data_chunk_recv_callback(
    //     nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_arg)
    // {
    //     log->debug("{} called", __PRETTY_FUNCTION__);
    //     auto &s = _get_session(user_arg);
    // }

    int session_callbacks::on_frame_recv_callback(nghttp2_session *s, const nghttp2_frame *frame, void * /* user_arg */)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &stream_id = frame->hd.stream_id;

        switch (frame->hd.type)
        {
            case NGHTTP2_DATA:
            case NGHTTP2_HEADERS:
                if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)
                {
                    auto *str = _get_stream(s, stream_id);

                    if (not str)
                    {
                        log->critical("Could not find stream of id:{} to process incoming frame!", stream_id);
                        return NGHTTP2_ERR_CALLBACK_FAILURE;
                    }

                    return str->recv_request();
                }

                break;
            default:
                break;
        }

        log->debug("Received nghttp2_frame_type value: {}", static_cast<int>(frame->hd.type));
        return 0;
    }

    int session_callbacks::on_stream_close_callback(
        nghttp2_session * /* session */, int32_t stream_id, uint32_t error_code, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);

        return s.stream_close_hook(stream_id, error_code);
    }

    // called when nghttp2 emits single header name/value pair
    int session_callbacks::on_header_callback(
        nghttp2_session *s,
        const nghttp2_frame *frame,
        const uint8_t *name,
        size_t /* namelen */,
        const uint8_t *value,
        size_t valuelen,
        uint8_t /* flags */,
        void * /* user_arg */)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &stream_id = frame->hd.stream_id;

        if (frame->hd.type == NGHTTP2_HEADERS and frame->headers.cat == NGHTTP2_HCAT_REQUEST
            and not req::fields::path.compare(name))
        {
            auto *str = _get_stream(s, stream_id);

            if (not str)
            {
                log->critical("Could not find stream of id:{} to process incoming header!", stream_id);
                return NGHTTP2_ERR_CALLBACK_FAILURE;
            }

            return str->recv_header(req::headers{req::FIELD::path, ustring_view{value, valuelen}});
        }

        return 0;
    }

    int session_callbacks::on_begin_headers_callback(
        nghttp2_session * /* session */, const nghttp2_frame *frame, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = _get_session(user_arg);

        if (frame->hd.type != NGHTTP2_HEADERS or frame->headers.cat != NGHTTP2_HCAT_REQUEST)
        {
            log->debug("Ignoring non-header and non-hcat-req frames...");
            return 0;
        }

        return s.begin_headers_hook(frame->hd.stream_id);
    }

    std::shared_ptr<session> session::make(IO dir, listener &l, ip_address remote, evutil_socket_t fd)
    {
        return l._ep.template make_shared<session>(dir, l, std::move(remote), fd);
    }

    session::session(IO dir, listener &l, ip_address remote, evutil_socket_t fd)
        : _dir{dir}, _lst{l}, _ep{_lst._ep}, _fd{fd}, _path{{}, std::move(remote)}
    {
        _init_internals();
    }

    void session::_init_internals()
    {
        assert(_ep.in_event_loop());
        _ep.call_get([&]() {
            _ssl.reset(_lst.new_ssl(_dir));

            if (not _ssl)
                throw std::runtime_error{"Failed to emplace SSL pointer for new session"};

            _bev.reset(bufferevent_openssl_socket_new(
                _ep._loop->loop().get(),
                _fd,
                _ssl.get(),
                is_inbound() ? BUFFEREVENT_SSL_ACCEPTING : BUFFEREVENT_SSL_CONNECTING,
                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE));

            if (not _bev)
                throw std::runtime_error{"Failed to create bufferevent socket for {}bound TLS session: {}"_format(
                    is_inbound() ? "in" : "out", detail::current_error())};

            bufferevent_ssl_set_flags(_bev.get(), BUFFEREVENT_SSL_DIRTY_SHUTDOWN);

            _fd = bufferevent_getfd(_bev.get());
            int val = 1;
            if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0)
                throw std::runtime_error{
                    "Failed to set TCP_NODELAY on inbound TLS session socket: {}"_format(detail::current_error())};

            log->debug("Inbound session has fd: {}", _fd);

            sockaddr _laddr{};
            socklen_t len;

            if (getsockname(_fd, &_laddr, &len) < 0)
                throw std::runtime_error{"Failed to get local socket address for incoming (remote: {}): {}, {}"_format(
                    _path.remote(), detail::current_error(), errno)};

            _path._local = ip_address{&_laddr};

            bufferevent_setcb(
                _bev.get(), session_callbacks::read_cb, session_callbacks::write_cb, session_callbacks::event_cb, this);

            bufferevent_enable(_bev.get(), EV_READ | EV_WRITE);

            log->info("Successfully configured inbound session; path: {}", _path);
        });
    }

    void session::close_session()
    {
        _ep.call_soon([&]() {
            log->info("Session (path: {}) signalling listener to close connection...", _path);
            _lst.close_session(_path.remote());
        });
    }

    void session::config_send_initial()
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([this]() {
            initialize_session();
            send_server_initial();
            send_session_data();
        });
    }

    void session::initialize_session()
    {
        assert(_ep.in_event_loop());
        nghttp2_option *opt;
        nghttp2_session *_sess;
        nghttp2_session_callbacks *callbacks;

        nghttp2_session_callbacks_new(&callbacks);

        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, session_callbacks::on_stream_close_callback);

        nghttp2_session_callbacks_set_send_callback2(callbacks, session_callbacks::send_callback);

        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, session_callbacks::on_frame_recv_callback);

        nghttp2_session_callbacks_set_on_begin_headers_callback(
            callbacks, session_callbacks::on_begin_headers_callback);

        nghttp2_session_callbacks_set_on_header_callback(callbacks, session_callbacks::on_header_callback);

        // nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, nullptr);

        // nghttp2_session_callbacks_set_before_frame_send_callback(callbacks, nullptr);

        // nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, nullptr);

        // nghttp2_session_callbacks_set_on_frame_not_send_callback(callbacks, nullptr);

        if (auto rv = nghttp2_option_new(&opt); rv != 0)
            throw std::runtime_error{"Failed to create nghttp2 session option struct: {}"_format(nghttp2_strerror(rv))};

        nghttp2_option_set_no_recv_client_magic(opt, 1);

        if (auto rv = nghttp2_session_server_new2(&_sess, callbacks, this, opt); rv != 0)
            throw std::runtime_error{"Failed to initialize inbound session: {}"_format(nghttp2_strerror(rv))};

        _session = _ep.template shared_ptr<nghttp2_session>(_sess, deleters::_session{});

        log->info("Inbound session initialized and set callbacks!");
    }

    void session::send_server_initial()
    {
        assert(_ep.in_event_loop());
        log->debug("{} called", __PRETTY_FUNCTION__);
        req::settings _settings;
        _settings.add_setting(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100);

        if (auto rv = nghttp2_submit_settings(_session.get(), NGHTTP2_FLAG_NONE, _settings, _settings.size()); rv != 0)
            throw std::runtime_error{"Failed to submit session settings: {}"_format(nghttp2_strerror(rv))};

        log->info("Inbound session successfully submitted nghttp2 settings!");
    }

    void session::send_session_data()
    {
        assert(_ep.in_event_loop());
        log->debug("{} called", __PRETTY_FUNCTION__);
        if (nghttp2_session_send(_session.get()) != 0)
            throw std::runtime_error{"Failed to dispatch session data to remote: {}"_format(remote())};

        log->info("Inbound session successfully dispatched session data to remote: {}", remote());
    }

    void session::read_session_data()
    {
        assert(_ep.in_event_loop());
        log->debug("{} called", __PRETTY_FUNCTION__);

        evbuffer *input = bufferevent_get_input(_bev.get());
        auto inlen = evbuffer_get_length(input);

        auto recv_len = nghttp2_session_mem_recv2(_session.get(), evbuffer_pullup(input, -1), inlen);

        if (recv_len < 0)
        {
            log->critical("Fatal error reading {}B from bufferevent: {}", inlen, nghttp2_strerror(recv_len));
            return close_session();
        }

        send_session_data();
    }

    void session::write_session_data()
    {
        assert(_ep.in_event_loop());
        log->debug("{} called", __PRETTY_FUNCTION__);

        if (evbuffer_get_length(bufferevent_get_output(_bev.get())) > 0)
        {
            log->info("Figure out why nghttp2 returns here!");
            return;
        }

        if (not nghttp2_session_want_read(_session.get()) and not nghttp2_session_want_write(_session.get()))
        {
            log->warn("No more IO to go, closing session...");
            return close_session();
        }

        send_session_data();
    }

    nghttp2_ssize session::send_hook(ustring_view data)
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() -> nghttp2_ssize {
            if (auto outlen = evbuffer_get_length(bufferevent_get_output(_bev.get())); outlen >= OUTPUT_BLOCK_THRESHOLD)
            {
                log->warn("Cannot send data (size:{}) with output buffer of size:{}", data.size(), outlen);
                return NGHTTP2_ERR_WOULDBLOCK;
            }

            bufferevent_write(_bev.get(), data.data(), data.size());
            return data.size();
        });
    }

    int session::stream_close_hook(int32_t stream_id, uint32_t error_code)
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            if (auto it = _streams.find(stream_id); it != _streams.end())
            {
                log->info("Closing stream (ID:{}) with error code: {}", stream_id, error_code);
                _streams.erase(it);
            }
            else
                log->warn("Could not find stream (ID:{}); received error code: {}", stream_id, error_code);
            return 0;
        });
    }

    int session::begin_headers_hook(int32_t stream_id)
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            // create stream
            auto [itr, b] = _streams.try_emplace(stream_id, nullptr);

            if (not b)
            {
                log->error("Already have stream with stream-id:{}!", stream_id);
                // TODO: something...
                return 0;
            }

            itr->second = make_stream(stream_id);

            if (not itr->second)
                throw std::runtime_error{"Failed to construct stream (id: {})"_format(stream_id)};

            nghttp2_session_set_stream_user_data(_session.get(), stream_id, itr->second.get());

            return 0;
        });
    }

    std::shared_ptr<stream> session::make_stream(int32_t stream_id)
    {
        assert(_ep.in_event_loop());
        return _ep.template shared_ptr<stream>(new stream{*this, _session, stream_id}, deleters::stream_d);
    }
}  //  namespace wshttp
