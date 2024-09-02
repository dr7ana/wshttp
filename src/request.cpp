#include "request.hpp"

#include "internal.hpp"

namespace wshttp::req
{
    constexpr ustring_view field(req::FIELD f)
    {
        switch (f)
        {
            case req::FIELD::method:
                return fields::method;
            case req::FIELD::scheme:
                return fields::scheme;
            case req::FIELD::authority:
                return fields::authority;
            case req::FIELD::path:
                return fields::path;
            case req::FIELD::status:
                return fields::status;
        }
    }

    nghttp2_nv make_header(req::FIELD f, ustring_view& v, nghttp2_nv_flag flags)
    {
        return nghttp2_nv{
            const_cast<uint8_t*>(field(f).data()),
            const_cast<uint8_t*>(v.data()),
            field(f).size(),
            v.size(),
            static_cast<uint8_t>(flags | NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE)};
    }

    void headers::add_field(FIELD f, ustring_view val, nghttp2_nv_flag flags)
    {
        _hdrs.push_back(make_header(f, val, flags));
    }
}  // namespace wshttp::req
