#pragma once

#include "concepts.hpp"
#include "types.hpp"

namespace wshttp
{
    using namespace wshttp::literals;

    namespace req
    {
        namespace fields
        {
            inline constexpr auto method = ":method"_usp;
            inline constexpr auto scheme = ":scheme"_usp;
            inline constexpr auto authority = ":authority"_usp;
            inline constexpr auto path = ":path"_usp;
            inline constexpr auto status = ":status"_usp;
        }  // namespace fields

        namespace types
        {
            inline constexpr auto get = "GET"_usp;
        }   //  namespace types

        namespace code
        {
            inline constexpr auto HTTP_200 = "200"_usp;
            inline constexpr auto HTTP_404 = "404"_usp;
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
            headers(uspan name, uspan val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);
            headers(FIELD f, uspan val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            static headers make_request();

            static headers make_status(CODE s);

            std::pair<uspan, uspan> current();
            std::pair<uspan, uspan> next();

            size_t size() const { return _hdrs.size(); }
            bool end() const { return _index == size() - 1; }

            void print() const;

            void add_field(FIELD f, uspan val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            void add_pair(uspan name, uspan val, nghttp2_nv_flag flags = NGHTTP2_NV_FLAG_NONE);

            std::pair<uspan, uspan> operator[](size_t i)
            {
                assert(i < _hdrs.size());
                auto& h = _hdrs[i];
                return {uspan{h.name, h.namelen}, uspan{h.value, h.valuelen}};
            }

            const std::pair<uspan, uspan> operator[](size_t i) const
            {
                assert(i < _hdrs.size());
                auto& h = _hdrs[i];
                return {uspan{h.name, h.namelen}, uspan{h.value, h.valuelen}};
            }

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
