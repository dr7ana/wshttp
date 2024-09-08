#include "stream.hpp"

#include "session.hpp"

namespace wshttp
{
    stream::stream(session& s, int32_t id) : _s{s}, _id{id}
    {}

    int stream::recv_header(req::headers hdr)
    {
        (void)hdr;
        return {};
    }

    int stream::recv_request()
    {
        return 0;
    }
}  //  namespace wshttp
