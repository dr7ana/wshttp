#include "request.hpp"

#include "internal.hpp"

namespace wshttp::req
{
    constexpr uspan _field(FIELD f)
    {
        switch (f)
        {
            case FIELD::method:
                return fields::method;
            case FIELD::scheme:
                return fields::scheme;
            case FIELD::authority:
                return fields::authority;
            case FIELD::path:
                return fields::path;
            case FIELD::status:
                return fields::status;
        }
    }

    constexpr auto _code(CODE s)
    {
        switch (s)
        {
            case CODE::_200:
                return code::HTTP_200;
            case CODE::_404:
                return code::HTTP_404;
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

    static nghttp2_nv make_header(FIELD f, uspan& v, nghttp2_nv_flag flags)
    {
        return nghttp2_nv{
            const_cast<uint8_t*>(_field(f).data()),
            const_cast<uint8_t*>(v.data()),
            _field(f).size(),
            v.size(),
            static_cast<uint8_t>(flags | NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE)};
    }

    static nghttp2_settings_entry make_setting(int32_t id, uint32_t val)
    {
        return nghttp2_settings_entry{id, val};
    }

    headers::headers(uspan name, uspan val, nghttp2_nv_flag flags)
    {
        make_pair(name.data(), name.size(), val.data(), val.size(), flags);
    }

    headers::headers(FIELD f, uspan val, nghttp2_nv_flag flags)
    {
        add_field(f, val, flags);
    }

    headers headers::make_status(CODE s)
    {
        return headers{fields::status, _code(s)};
    }

    std::pair<uspan, uspan> headers::current()
    {
        auto& h = _hdrs[_index];
        return {{h.name, h.namelen}, {h.value, h.valuelen}};
    }

    std::pair<uspan, uspan> headers::next()
    {
        _index += 1;
        _index %= _hdrs.size();
        return current();
    }

    void headers::print() const
    {
        for (size_t i = 0; i < _hdrs.size(); ++i)
        {
            auto [n, v] = (*this)[i];
            log->info("Type: {}, Value: {}", n, buffer_printer{v});
        }
    }

    void headers::add_field(FIELD f, uspan val, nghttp2_nv_flag flags)
    {
        _hdrs.push_back(make_header(f, val, flags));
    }

    void headers::add_pair(uspan name, uspan val, nghttp2_nv_flag flags)
    {
        _hdrs.push_back(make_pair(name.data(), name.size(), val.data(), val.size(), flags));
    }

    void settings::add_setting(int32_t id, uint32_t val)
    {
        _settings.push_back(make_setting(id, val));
    }
}  // namespace wshttp::req
