#include "session.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "request.hpp"
#include "stream.hpp"

namespace wshttp
{
    static constexpr auto OUTPUT_BLOCK_THRESHOLD{1 << 16};

    void session_callbacks::event_cb(struct bufferevent */* bev */, short events, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);

        auto msg = "Inbound session (path: {})"_format(s.path());

        if (events & BEV_EVENT_CONNECTED)
        {
            const unsigned char* alpn = nullptr;
            unsigned int alpn_len = 0;

            SSL_get0_alpn_selected(s._ssl.get(), &alpn, &alpn_len);
            if (not alpn || alpn_len != 2 || alpn != defaults::ALPN)
            {
                log->warn("{} failed to negotiate 'h2' alpn!", msg);
            }
            else
            {
                log->info("{} successfully negotiated 'h2' alpn, initializing...", msg);
                // initialize session
                s.initialize_session();
                s.send_server_initial();
                s.send_session_data();
            }
        }
        else if (events & BEV_EVENT_EOF)
        {
            log->warn("{} EOF!", msg);
        }
        else if (events & BEV_EVENT_ERROR)
        {
            log->warn("{} network error!", msg);
        }
        else if (events & BEV_EVENT_TIMEOUT)
        {
            log->warn("{} timed out!", msg);
        }

        // TODO: close the connection
    }
    
    void session_callbacks::read_cb(struct bufferevent */* bev */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);
        s.recv_session_data();
    }

    void session_callbacks::write_cb(struct bufferevent */* bev */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);
        s.recv_session_data();
    }

    nghttp2_ssize session_callbacks::send_callback(nghttp2_session */* session */,
                                const uint8_t *data, size_t length,
                                int /* flags */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);
        return s.send_hook(ustring_view{data, length});
    }

    int session_callbacks::on_frame_recv_callback(nghttp2_session */* session */,
                                const nghttp2_frame *frame, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);

        switch (frame->hd.type)
        {
            case NGHTTP2_DATA:
            case NGHTTP2_HEADERS:
                return s.recv_frame_hook(frame->hd.stream_id);  // TODO:
                break;
            default:
                break;
        }

        return 0;
    }

    int session_callbacks::on_stream_close_callback(nghttp2_session */* session */, int32_t stream_id,
                                uint32_t error_code, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);
        (void)s;
        (void)stream_id;
        (void)error_code;
        return 0;
    }

    // called when nghttp2 emits single header name/value pair
    int session_callbacks::on_header_callback(nghttp2_session */* session */,
                            const nghttp2_frame *frame, const uint8_t *name,
                            size_t /* namelen */, const uint8_t *value,
                            size_t valuelen, uint8_t /* flags */, void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);

        if (frame->hd.type == NGHTTP2_HEADERS and frame->headers.cat == NGHTTP2_HCAT_REQUEST)
        {
            if (req::fields::path.compare(name) == 0)
            {
                return s.stream_recv_header(frame->hd.stream_id, req::headers{req::FIELD::path, ustring_view{value, valuelen}});
            }
        }

        return 0;
    }

    int session_callbacks::on_begin_headers_callback(nghttp2_session */* session */,
                                    const nghttp2_frame *frame,
                                    void *user_arg)
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        auto& s = *static_cast<session *>(user_arg);

        if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) 
        {
            log->debug("Ignoring non-header and non-hcat-req frames...");
            return 0;
        }

        return s.begin_headers_hook(frame->hd.stream_id);
    }

    std::shared_ptr<session> session::make(listener& l, ip_address remote, evutil_socket_t fd)
    {
        return l._ep.template make_shared<session>(l, std::move(remote), fd);
    }

    session::session(listener& l, ip_address remote, evutil_socket_t fd) : _ep{l._ep}, _fd{fd}
    {
        _ssl.reset(SSL_new(l._ctx->ctx().get()));

        if (not _ssl)
            throw std::runtime_error{
                "Failed to create SSL/TLS for inbound: {}"_format(ERR_error_string(ERR_get_error(), NULL))};

        _bev.reset(bufferevent_openssl_socket_new(
            _ep._loop->loop().get(),
            _fd,
            _ssl.get(),
            BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE));

        if (not _bev)
            throw std::runtime_error{"Failed to create bufferevent socket for inbound TLS session: {}"_format(
                ERR_error_string(ERR_get_error(), NULL))};

        bufferevent_enable(_bev.get(), EV_READ | EV_WRITE);

        sockaddr _laddr{};
        socklen_t len;

        if (getsockname(_fd, &_laddr, &len) < 0)
            throw std::runtime_error{"Failed to get local socket address for incoming (remote: {})"_format(remote)};

        _path = {std::move(remote), ip_address{&_laddr}};

        bufferevent_setcb(_bev.get(), session_callbacks::read_cb, session_callbacks::write_cb, session_callbacks::event_cb, this);

        log->info("Successfully configured inbound session; path: {}", _path);
    }

    void session::initialize_session()
    {
        nghttp2_session* _sess;
        nghttp2_session_callbacks* callbacks;

        nghttp2_session_callbacks_new(&callbacks);
                
        nghttp2_session_callbacks_set_send_callback2(callbacks, session_callbacks::send_callback);

        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                            session_callbacks::on_frame_recv_callback);

        nghttp2_session_callbacks_set_on_stream_close_callback(
            callbacks, session_callbacks::on_stream_close_callback);

        nghttp2_session_callbacks_set_on_header_callback(callbacks,
                                                       session_callbacks:: on_header_callback);

        nghttp2_session_callbacks_set_on_begin_headers_callback(
            callbacks, session_callbacks::on_begin_headers_callback);

        if (nghttp2_session_server_new(&_sess, callbacks, this) != 0)
            throw std::runtime_error{"Failed to initialize inbound session!"};

        _session.reset(_sess);

        log->info("Inbound session initialized and set callbacks!");
    }

    void session::send_server_initial()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        req::settings _settings;
        _settings.add_setting(NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100);

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

    void session::stream_close_hook()
    {}

    int session::recv_frame_hook(int32_t stream_id)
    {
        auto* stream_data = nghttp2_session_get_stream_user_data(_session.get(), stream_id);

        if (not stream_data)
        {
            // nghttp2 says DATA and HEADERS frames can invoke this cb after on_stream_close
            return 0;
        }

        return {};
    }

    int session::begin_headers_hook(int32_t stream_id)
    {
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

        nghttp2_session_set_stream_user_data(_session.get(), stream_id, &itr->second);

        return 0;
    }

    int session::stream_recv_header(int32_t stream_id, req::headers hdr)
    {
        if (auto itr = _streams.find(stream_id); itr != _streams.end())
        {
            return itr->second->recv_header(std::move(hdr));
        }
        else
        {
            log->critical("Could not find stream of id:{} to process incoming header!", stream_id);
        }

        return 0;
    }

    std::shared_ptr<stream> session::make_stream(int32_t stream_id)
    {
        return _ep.template make_shared<stream>(*this, stream_id);
    }
}  //  namespace wshttp
