#include "stream.hpp"

#include "session.hpp"

namespace wshttp
{
    stream::stream(session& s, int32_t id) : _s{s}, _id{id} {}

    int stream::recv_header(req::headers hdr)
    {
        _req = uri::parse(hdr.current().second);
        return _req ? 0 : -1;
    }

    int stream::recv_request()
    {
        return 0;
    }
}  //  namespace wshttp
