#include "stream.hpp"

#include "endpoint.hpp"
#include "internal.hpp"
#include "session.hpp"

namespace wshttp
{
    ssize_t stream_callbacks::file_read_callback(
        nghttp2_session* /* session */,
        int32_t /* stream_id */,
        uint8_t* buf,
        size_t length,
        uint32_t* data_flags,
        nghttp2_data_source* source,
        void* /* user_data */)
    {
        auto fd = source->fd;
        ssize_t ret{-1};

        while (ret == -1 and errno == EINTR)
            ret = read(fd, buf, length);

        if (ret == -1)
        {
            log->critical("stream file read returning 'NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE'");
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
        if (ret == 0)
        {
            log->critical("stream file read returning 'NGHTTP2_DATA_FLAG_EOF'");
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            return NGHTTP2_DATA_FLAG_EOF;
        }

        return ret;
    }

    stream::stream(inbound_session& s, const session_ptr& sess, int32_t id)
        : _s{s}, _session{sess.get(), deleters::_session{}}, dir{IO::INBOUND}, _id{id}
    {
        log->debug("Inbound stream (ID: {}) created!", _id);
    }

    stream::stream(outbound_session& s, const session_ptr& sess, int32_t id)
        : _s{s}, _session{sess.get(), deleters::_session{}}, dir{IO::OUTBOUND}, _id{id}
    {
        log->debug("Outbound stream (ID: {}) created!", _id);
    }

    int stream::recv_data(ustring data)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        log->info("Stream (ID:{}) received data: {}", _id, buffer_printer{data});
        return 0;
    }

    int stream::recv_path_header(uspan path)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        _req = uri::parse(path);
        return _req ? 0 : NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    int stream::recv_header(req::headers hdr)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        hdr.print();
        return 0;
    }

    int stream::recv_frame()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (not _req)
        {
            log->warn("Stream received request before header!");
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }

        _req.print_contents();

        auto path = _req.path();
        while (path.starts_with('/'))
            path.remove_prefix(1);

        auto rv = open(path.data(), O_RDONLY);
        if (rv == -1)
            return send_error();

        _fd = rv;

        rv = send_response(req::headers::make_status(req::CODE::_200));
        if (rv != 0)
            close(_fd);

        return rv;
    }

    int stream::send_error()
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        if (pipe(_pipes.data()) != 0)
        {
            log->warn("Failed to create pipes to send error! Resetting stream");
            if (nghttp2_submit_rst_stream(_session.get(), NGHTTP2_FLAG_NONE, _id, NGHTTP2_INTERNAL_ERROR) != 0)
            {
                log->critical("Could not submit stream reset! Fatal error!");
                return NGHTTP2_ERR_FATAL;
            }
            return 0;
        }

        ssize_t errlen = sizeof(req::errors::HTML) - 1;

        auto len = write(_pipes[1], &req::errors::HTML, errlen);
        close(_pipes[1]);

        if (len != errlen)
        {
            log->critical("Read {}B (expecting: {})", len, errlen);
            close(_pipes[0]);
            return NGHTTP2_ERR_FATAL;
        }

        _fd = _pipes[0];

        auto rv = send_response(req::headers::make_status(req::CODE::_404));
        if (rv != 0)
            close(_pipes[0]);

        return rv;
    }

    int stream::send_response(req::headers hdrs)
    {
        log->trace("{} called", __PRETTY_FUNCTION__);

        nghttp2_data_provider2 _prv{.source = {_fd}, .read_callback = stream_callbacks::file_read_callback};

        if (auto rv = nghttp2_submit_response2(_session.get(), _id, hdrs, hdrs.size(), &_prv); rv != 0)
        {
            log->critical("Fatal 'nghttp2_submit_response2' error: {}", nghttp2_strerror(rv));
            return NGHTTP2_ERR_FATAL;
        }

        return 0;
    }

}  //  namespace wshttp
