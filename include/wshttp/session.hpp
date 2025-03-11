#pragma once

#include "listener.hpp"

namespace wshttp
{
    class stream;
    class endpoint;

    namespace deleters
    {
        struct _bufferevent
        {
            inline void operator()(::bufferevent* b) const { bufferevent_free(b); }
        };

        struct _session
        {
            inline void operator()(nghttp2_session* s) const { nghttp2_session_del(s); }
        };
    }  // namespace deleters

    using bufferevent_ptr = std::unique_ptr<::bufferevent, deleters::_bufferevent>;

    using session_ptr = std::shared_ptr<::nghttp2_session>;

    class session_base
    {
        friend class stream;
        friend class listener;
        friend struct session_callbacks;

      protected:
        session_base(endpoint& e, evutil_socket_t f, path _p, bool d) :
                _ep{e}, _fd{f}, _path{std::move(_p)}, _is_outbound{d}
        {}

        endpoint& _ep;

        evutil_socket_t _fd;

        path _path;

        ssl_ptr _ssl;
        bufferevent_ptr _bev;

        session_ptr _session;
        std::unordered_map<int32_t, std::shared_ptr<stream>> _streams;

        bool _is_outbound{false};

        std::shared_ptr<stream> get_stream(int32_t id);

        void read_session_data();

        void write_session_data();

        void send_session_data();

        nghttp2_ssize send_hook(uspan data);

        void config_send_initial();

        virtual void initialize_session() = 0;

        virtual void send_initial() = 0;

        virtual int begin_headers_hook(const nghttp2_frame* frame) = 0;

        virtual int frame_recv_hook(const nghttp2_frame* frame) = 0;

        virtual int recv_header_hook(const nghttp2_frame* frame, uspan name, uspan value) = 0;

        virtual int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) = 0;

        virtual void close_session() = 0;

        bool is_inbound() const { return !_is_outbound; }
        bool is_outbound() const { return _is_outbound; }

      public:
        virtual ~session_base() = default;

        const ip_address& local() const { return _path.local(); }
        const ip_address& remote() const { return _path.remote(); }
        const path& session_path() const { return _path; }

        template <typename T>
            requires std::same_as<T, nghttp2_session>
        operator const T*() const
        {
            return _session.get();
        }

        template <typename T>
            requires std::same_as<T, nghttp2_session>
        operator T*()
        {
            return _session.get();
        }
    };

    class inbound_session final : public session_base
    {
        // friend class stream;
        friend class listener;
        friend struct session_callbacks;

      public:
        inbound_session() = delete;

        inbound_session(listener& l, ip_address remote, evutil_socket_t fd) :
                session_base{l._ep, fd, path{{}, std::move(remote)}, false}, _lst{l}
        {
            _init_internals();
        }

        static std::shared_ptr<inbound_session> make(listener& l, ip_address remote, evutil_socket_t fd);

        // No copy, no move; always hold in shared_ptr using static ::make()
        inbound_session(const inbound_session&) = delete;
        inbound_session& operator=(const inbound_session&) = delete;
        inbound_session(inbound_session&&) = delete;
        inbound_session& operator=(inbound_session&&) = delete;

        ~inbound_session() override;

      protected:
        listener& _lst;

        void _init_internals();

        void initialize_session() override;

        void send_initial() override;

        int begin_headers_hook(const nghttp2_frame* frame) override;

        int recv_header_hook(const nghttp2_frame* frame, uspan name, uspan value) override;

        int frame_recv_hook(const nghttp2_frame* frame) override;

        // int frame_send_hook(const nghttp2_frame* frame);

        int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) override;

        std::shared_ptr<stream> make_stream(int32_t stream_id);

        void close_session() override;
    };

    class outbound_node final : public local_node, public session_base
    {
        outbound_node(endpoint& ep, uri _u, evutil_socket_t fd, ip_address local = ip_address{}) :
                local_node{std::move(local)}, session_base{ep, fd, path{_local, {}}, true}, _uri{_u}
        {
            _init_internals();
        }

      private:
        uri _uri;

      public:
        outbound_node() = delete;

        ~outbound_node() override;

        SSL* new_ssl() override;

        void close() override;

        void _init_internals() override;

        void initialize_session() override;

        void send_initial() override;

        int begin_headers_hook(const nghttp2_frame* frame) override;

        int recv_header_hook(const nghttp2_frame* frame, uspan name, uspan value) override;

        int frame_recv_hook(const nghttp2_frame* frame) override;

        int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) override;

        void on_connect();

        void close_session() override;
    };
}  //  namespace wshttp
