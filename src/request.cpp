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

    static nghttp2_nv make_pair(
        const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen, nghttp2_nv_flag flags)
    {
        return nghttp2_nv{
            const_cast<uint8_t*>(name),
            const_cast<uint8_t*>(value),
            namelen,
            valuelen,
            static_cast<uint8_t>(flags | NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE)};
    }

    static nghttp2_nv make_header(req::FIELD f, ustring_view& v, nghttp2_nv_flag flags)
    {
        return nghttp2_nv{
            const_cast<uint8_t*>(field(f).data()),
            const_cast<uint8_t*>(v.data()),
            field(f).size(),
            v.size(),
            static_cast<uint8_t>(flags | NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE)};
    }

    static nghttp2_settings_entry make_setting(int32_t id, uint32_t val)
    {
        return nghttp2_settings_entry{id, val};
    }

    headers::headers(ustring_view name, ustring_view val, nghttp2_nv_flag flags)
    {
        make_pair(name.data(), name.size(), val.data(), val.size(), flags);
    }

    headers::headers(FIELD f, ustring_view val, nghttp2_nv_flag flags)
    {
        add_field(f, val, flags);
    }

    std::pair<ustring_view, ustring_view> headers::current()
    {
        auto& h = _hdrs[_index];
        return {{h.name, h.namelen}, {h.value, h.valuelen}};
    }

    std::pair<ustring_view, ustring_view> headers::next()
    {
        _index += 1;
        _index %= _hdrs.size();
        return current();
    }

    void headers::add_field(FIELD f, ustring_view val, nghttp2_nv_flag flags)
    {
        _hdrs.push_back(make_header(f, val, flags));
    }

    void headers::add_pair(ustring_view name, ustring_view val, nghttp2_nv_flag flags)
    {
        _hdrs.push_back(make_pair(name.data(), name.size(), val.data(), val.size(), flags));
    }

    void settings::add_setting(int32_t id, uint32_t val)
    {
        _settings.push_back(make_setting(id, val));
    }
}  // namespace wshttp::req
