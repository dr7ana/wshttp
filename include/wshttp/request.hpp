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

        namespace status
        {
            inline constexpr auto HTTP_200 = "200"_usv;
            inline constexpr auto HTTP_404 = "404"_usv;
        }  //  namespace status

        namespace errors
        {
            inline constexpr std::array<const char*, 2> HTML{
                "<html><head><title>404</title></head>", "<body><h1>404 Not Found</h1></body></html>"};
        }  //  namespace errors

        struct headers
        {
            std::vector<nghttp2_nv> _hdrs;

          private:
            size_t _index{};

          public:
            headers(ustring_view name, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);
            headers(FIELD f, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            static headers make_status(STATUS s);

            std::pair<ustring_view, ustring_view> current();
            std::pair<ustring_view, ustring_view> next();

            size_t size() const { return _hdrs.size(); }
            bool end() const { return _index == size() - 1; }

            void add_field(FIELD f, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            void add_pair(ustring_view name, ustring_view val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            template <concepts::nghttp2_nv_type T>
            operator const T*() const
            {
                return _hdrs.data();
            }

            template <concepts::nghttp2_nv_type T>
            operator T*()
            {
                return _hdrs.data();
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
