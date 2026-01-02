#pragma once

#include "types.hpp"

#include <unlog.hpp>
#include <unlog/format.hpp>

#include <ranges>
#include <source_location>

using namespace std::literals;

namespace wshttp {
    namespace detail {
        inline constexpr char char_tolower(char in) {
            if (in >= 'A' && in <= 'Z') {
                return in - ('A' - 'a');
            }
            return in;
        }

        inline constexpr bool str_case_eq(std::string_view ref, std::string_view cmp) {
            return std::ranges::equal(ref, cmp | std::views::transform(char_tolower));
        }

    }  // namespace detail

    inline constexpr unlog::LogLevel parse_log_level(std::string_view level) {
        if (detail::str_case_eq("trace"sv, level)) {
            return unlog::LogLevel::trace;
        }
        else if (detail::str_case_eq("debug"sv, level)) {
            return unlog::LogLevel::debug;
        }
        else if (detail::str_case_eq("info"sv, level)) {
            return unlog::LogLevel::info;
        }
        else if (detail::str_case_eq("warn"sv, level)) {
            return unlog::LogLevel::warn;
        }
        else if (detail::str_case_eq("err"sv, level) || detail::str_case_eq("error"sv, level)) {
            return unlog::LogLevel::err;
        }
        else if (detail::str_case_eq("critical"sv, level)) {
            return unlog::LogLevel::critical;
        }

        throw std::invalid_argument{"Could not recognize log level input: {}"_format(level)};
    }

    struct buffer_printer {
      private:
        bspan buf;

      public:
        template <enc::basic_char T>
        explicit buffer_printer(const T* data, size_t datalen) :
                buf{reinterpret_cast<const std::byte*>(data), datalen} {}

        // Constructed from any type of string_view<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.)
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string_view<T> data) : buffer_printer{data.data(), data.size()} {}

        // Constructed from any type of lvalue string<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.)
        template <enc::basic_char T>
        explicit buffer_printer(const std::basic_string<T>& data) : buffer_printer{data.data(), data.size()} {}

        // *Not* constructable from a string<T> rvalue (because we only hold a view and do not take
        // ownership).
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string<T>&& buf) = delete;

        // Constructed from any type of span
        template <unlog::const_span_type T>
        explicit buffer_printer(const T& data) : buffer_printer{data.data(), data.size()} {}

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  //  namespace wshttp
