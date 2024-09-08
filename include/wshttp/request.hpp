#pragma once

#include "types.hpp"

namespace wshttp
{
    namespace req
    {
        namespace fields
        {
            inline constexpr auto method = ":method"_usv;
            inline constexpr auto scheme = ":scheme"_usv;
            inline constexpr auto authority = ":authority"_usv;
            inline constexpr auto path = ":path"_usv;
            inline constexpr auto status = ":status"_usv;
        }  // namespace fields

        struct headers
        {
            std::vector<nghttp2_nv> _hdrs;

            headers(ustring_view name, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);
            headers(FIELD f, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            void add_field(FIELD f, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            void add_pair(ustring_view name, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            template <concepts::nghttp2_nv_type T>
            operator const T*() const
            {
                return &_hdrs;
            }

            template <concepts::nghttp2_nv_type T>
            operator T*()
            {
                return &_hdrs;
            }
        };

        struct settings
        {
            std::vector<nghttp2_settings_entry> _settings;

            void add_setting(int32_t id, uint32_t val);

            size_t size() const { return _settings.size(); }

            template <concepts::nghttp2_settings_type T>
            operator const T*() const
            {
                return _settings.data();
            }

            template <concepts::nghttp2_settings_type T>
            operator T*()
            {
                return _settings.data();
            }
        };
    }  // namespace req
}  //  namespace wshttp
