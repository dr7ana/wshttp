#pragma once

#include "address.hpp"
#include "context.hpp"
#include "listener.hpp"
#include "node.hpp"
#include "request.hpp"

namespace wshttp
{
    class node;
    class stream;
    class endpoint;

    class session_base
    {
        friend class stream;
        friend class listener;
        friend struct session_callbacks;

      protected:
        session_base(endpoint& e, evutil_socket_t f, path _p, bool d)
            : _ep{e}, _fd{f}, _path{std::move(_p)}, _is_outbound{d}
        {}

        endpoint& _ep;

        evutil_socket_t _fd;

        path _path;

        ssl_ptr _ssl;
        bufferevent_ptr _bev;

        session_ptr _session;
        std::unordered_map<uint32_t, std::shared_ptr<stream>> _streams;

        bool _is_outbound{false};

        void read_session_data();

        void write_session_data();

        void send_session_data();

        nghttp2_ssize send_hook(ustring data);

        void config_send_initial();

        virtual void initialize_session() = 0;

        virtual void send_initial() = 0;

        virtual int begin_headers_hook(const nghttp2_frame* frame) = 0;

        virtual int recv_header_hook(const nghttp2_frame* frame, ustring_view name, ustring_view value) = 0;

        virtual int frame_recv_hook(const nghttp2_frame* frame) = 0;

        virtual int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) = 0;

        virtual void close_session() = 0;

        bool is_inbound() const { return !_is_outbound; }
        bool is_outbound() const { return _is_outbound; }

      public:
        virtual ~session_base() = default;

        const ip_address& local() const { return _path.local(); }
        const ip_address& remote() const { return _path.remote(); }
        const path& session_path() const { return _path; }

        template <concepts::nghttp2_session_type T>
        operator const T*() const
        {
            return _session.get();
        }

        template <concepts::nghttp2_session_type T>
        operator T*()
        {
            return _session.get();
        }
    };

    namespace concepts
    {
        template <typename T>
        concept session_type = std::derived_from<T, session_base>;
    }  //  namespace concepts

    class inbound_session final : public session_base
    {
        friend class stream;
        friend class listener;
        friend struct session_callbacks;

      public:
        inbound_session() = delete;

        inbound_session(listener& l, ip_address remote, evutil_socket_t fd)
            : session_base{l._ep, fd, path{{}, std::move(remote)}, false}, _lst{l}
        {
            _init_internals();
        }

        static std::shared_ptr<inbound_session> make(listener& l, ip_address remote, evutil_socket_t fd);

        // No copy, no move; always hold in shared_ptr using static ::make()
        inbound_session(const inbound_session&) = delete;
        inbound_session& operator=(const inbound_session&) = delete;
        inbound_session(inbound_session&&) = delete;
        inbound_session& operator=(inbound_session&&) = delete;

        ~inbound_session();

      protected:
        listener& _lst;

        void _init_internals();

        void initialize_session() override;

        void send_initial() override;

        int begin_headers_hook(const nghttp2_frame* frame) override;

        int recv_header_hook(const nghttp2_frame* frame, ustring_view name, ustring_view value) override;

        int frame_recv_hook(const nghttp2_frame* frame) override;

        int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) override;

        std::shared_ptr<stream> make_stream(int32_t stream_id);

        void close_session() override;
    };

    class outbound_session final : public session_base
    {
        friend class stream;
        friend class node;
        friend struct session_callbacks;

      public:
        outbound_session(node& n, evutil_socket_t fd, std::optional<ip_address> local = std::nullopt)
            : session_base{n._ep, fd, path{local ? std::move(*local) : ip_address{}, {}}, true},
              _n{n},
              _host{_n._uri.host()}
        {
            _init_internals();
        }

        ~outbound_session();

      protected:
        node& _n;
        std::string _host;

        void _init_internals();

        void initialize_session() override;

        void send_initial() override;

        int begin_headers_hook(const nghttp2_frame* frame) override;

        int recv_header_hook(const nghttp2_frame* frame, ustring_view name, ustring_view value) override;

        int frame_recv_hook(const nghttp2_frame* frame) override;

        int stream_close_hook(int32_t stream_id, uint32_t error_code = 0) override;

        void on_connect();

        void close_session() override;

      private:
        uri& get_uri() { return _n._uri; }
    };

}  //  namespace wshttp
