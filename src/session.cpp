#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "request.hpp"
#include "stream.hpp"

namespace wshttp
{
    static constexpr auto OUTPUT_BLOCK_THRESHOLD{1 << 16};

    static wshttp::stream *_get_stream(struct nghttp2_session *s, int32_t id)
    {
        return static_cast<wshttp::stream *>(nghttp2_session_get_stream_user_data(s, id));
    }

    void session_callbacks::event_cb(struct bufferevent * /* bev */, short events, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = *static_cast<session *>(user_arg);

        auto msg = "Inbound session (path: {})"_format(s.path());

        if (events & BEV_EVENT_CONNECTED)
        {
            const unsigned char *alpn = nullptr;
            unsigned int alpn_len = 0;

            SSL_get0_alpn_selected(s._ssl.get(), &alpn, &alpn_len);
            if (not alpn or alpn_len != 2 or alpn != defaults::ALPN)
            {
                log->warn(
                    "{} failed to negotiate 'h2' alpn! Received: {}",
                    msg,
                    std::string_view{reinterpret_cast<const char *>(alpn), alpn_len});
            }
            else
            {
                log->info("{} successfully negotiated 'h2' alpn, initializing...", msg);
                // initialize session
                return s.config_send_initial();
            }
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
        auto &s = *static_cast<session *>(user_arg);
        s.recv_session_data();
    }

    void session_callbacks::write_cb(struct bufferevent * /* bev */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = *static_cast<session *>(user_arg);
        s.recv_session_data();
    }

    nghttp2_ssize session_callbacks::send_callback(
        nghttp2_session * /* session */, const uint8_t *data, size_t length, int /* flags */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto &s = *static_cast<session *>(user_arg);
        return s.send_hook(ustring_view{data, length});
    }

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
        auto &s = *static_cast<session *>(user_arg);

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
        auto &s = *static_cast<session *>(user_arg);

        if (frame->hd.type != NGHTTP2_HEADERS or frame->headers.cat != NGHTTP2_HCAT_REQUEST)
        {
            log->debug("Ignoring non-header and non-hcat-req frames...");
            return 0;
        }

        return s.begin_headers_hook(frame->hd.stream_id);
    }

    std::shared_ptr<session> session::make(listener &l, ip_address remote, evutil_socket_t fd)
    {
        return l._ep.template make_shared<session>(l, std::move(remote), fd);
    }

    session::session(listener &l, ip_address remote, evutil_socket_t fd)
        : _lst{l}, _ep{_lst._ep}, _fd{fd}, _path{{}, std::move(remote)}
    {
        _init_internals();
    }

    void session::_init_internals()
    {
        assert(_ep.in_event_loop());
        _ep.call_get([&]() {
            _ssl.reset(_lst.new_ssl());

            if (not _ssl)
                throw std::runtime_error{"Failed to emplace SSL pointer for new session"};

            _bev.reset(bufferevent_openssl_socket_new(
                _ep._loop->loop().get(),
                _fd,
                _ssl.get(),
                BUFFEREVENT_SSL_ACCEPTING,
                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE));

            if (not _bev)
                throw std::runtime_error{
                    "Failed to create bufferevent socket for inbound TLS session: {}"_format(detail::current_error())};

            bufferevent_openssl_set_allow_dirty_shutdown(_bev.get(), 1);

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
        return _ep.call_get([this]() {
            initialize_session();
            send_server_initial();
            send_session_data();
        });
    }

    void session::initialize_session()
    {
        nghttp2_session *_sess;
        nghttp2_session_callbacks *callbacks;

        nghttp2_session_callbacks_new(&callbacks);

        nghttp2_session_callbacks_set_send_callback2(callbacks, session_callbacks::send_callback);

        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, session_callbacks::on_frame_recv_callback);

        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, session_callbacks::on_stream_close_callback);

        nghttp2_session_callbacks_set_on_header_callback(callbacks, session_callbacks::on_header_callback);

        nghttp2_session_callbacks_set_on_begin_headers_callback(
            callbacks, session_callbacks::on_begin_headers_callback);

        if (nghttp2_session_server_new(&_sess, callbacks, this) != 0)
            throw std::runtime_error{"Failed to initialize inbound session!"};

        _session = _ep.template shared_ptr<nghttp2_session>(_sess, deleters::_session{});

        log->info("Inbound session initialized and set callbacks!");
    }

    void session::send_server_initial()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        req::settings _settings;
        _settings.add_setting(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100);
        _settings.add_setting(NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES, 1);

        if (nghttp2_submit_settings(_session.get(), NGHTTP2_FLAG_NONE, _settings, _settings.size()) != 0)
            throw std::runtime_error{"Failed to submit session settings!"};

        log->info("Inbound session successfully submitted nghttp2 settings!");
    }

    void session::send_session_data()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        if (nghttp2_session_send(_session.get()) != 0)
            throw std::runtime_error{"Failed to dispatch session data to remote: {}"_format(remote())};

        log->info("Inbound session successfully dispatched session data to remote: {}", remote());
    }

    void session::recv_session_data()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        std::array<uint8_t, 4096> buf{};

        auto nwrite = bufferevent_read(_bev.get(), buf.data(), buf.size());
        auto recv_len = nghttp2_session_mem_recv2(_session.get(), buf.data(), nwrite);

        if (recv_len < 0)
            throw std::runtime_error{"Fatal error reading from bufferevent: {}"_format(nghttp2_strerror(nwrite))};

        send_session_data();
    }

    void session::write_session_data()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);

        if (evbuffer_get_length(bufferevent_get_output(_bev.get())) > 0)
        {
            // TODO:
            log->info("Figure out why nghttp2 returns here!");
            return;
        }

        if (nghttp2_session_want_read(_session.get()) or nghttp2_session_want_write(_session.get()))
        {
            log->warn("We have more IO to go!");
            return;
        }

        send_session_data();
    }

    nghttp2_ssize session::send_hook(ustring_view data)
    {
        if (auto outlen = evbuffer_get_length(bufferevent_get_output(_bev.get())); outlen >= OUTPUT_BLOCK_THRESHOLD)
        {
            log->warn("Cannot send data (size:{}) with output buffer of size:{}", data.size(), outlen);
            return NGHTTP2_ERR_WOULDBLOCK;
        }
        bufferevent_write(_bev.get(), data.data(), data.size());
        return data.size();
    }

    int session::stream_close_hook(int32_t stream_id, uint32_t error_code)
    {
        if (auto it = _streams.find(stream_id); it != _streams.end())
        {
            log->info("Closing stream (ID:{}) with error code: {}", stream_id, error_code);
            _streams.erase(it);
        }
        else
            log->warn("Could not find stream (ID:{}); received error code: {}", stream_id, error_code);
        return 0;
    }

    int session::recv_frame_hook(int32_t stream_id)
    {
        auto *str = static_cast<stream *>(nghttp2_session_get_stream_user_data(_session.get(), stream_id));

        if (not str)
        {
            log->warn(
                "Stream (ID: {}) could not be queried from nghttp2; {} held locally!",
                stream_id,
                _streams.count(stream_id) ? "ERROR it is" : "it is not");
            return 0;
        }

        assert(_streams.count(stream_id));

        return {};
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

    int session::stream_recv_header(int32_t stream_id, req::headers hdr)
    {
        assert(_ep.in_event_loop());
        return _ep.call_get([&]() {
            if (auto itr = _streams.find(stream_id); itr != _streams.end())
            {
                return itr->second->recv_header(std::move(hdr));
            }
            else
            {
                log->critical("Could not find stream of id:{} to process incoming header!", stream_id);
            }

            return 0;
        });
    }

    std::shared_ptr<stream> session::make_stream(int32_t stream_id)
    {
        assert(_ep.in_event_loop());
        return _ep.template shared_ptr<stream>(new stream{*this, _session, stream_id}, deleters::stream_d);
    }
}  //  namespace wshttp
