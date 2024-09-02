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

            void add_field(FIELD f, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            template <concepts::nghttp2_session_type T>
            operator const T*() const
            {
                return &_hdrs;
            }

            template <concepts::nghttp2_session_type T>
            operator T*()
            {
                return &_hdrs;
            }
        };
    }  // namespace req
}  //  namespace wshttp
