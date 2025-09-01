#pragma once

#include "types.hpp"

#include <fmt/format.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <source_location>

using namespace std::literals;

namespace wshttp
{
    struct buffer_printer
    {
      private:
        bspan buf;

      public:
        template <enc::basic_char T>
        explicit buffer_printer(const T* data, size_t datalen) : buf{reinterpret_cast<const std::byte*>(data), datalen}
        {}

        // Constructed from any type of string_view<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.)
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string_view<T> data) : buffer_printer{data.data(), data.size()}
        {}

        // Constructed from any type of lvalue string<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.)
        template <enc::basic_char T>
        explicit buffer_printer(const std::basic_string<T>& data) : buffer_printer{data.data(), data.size()}
        {}

        // *Not* constructable from a string<T> rvalue (because we only hold a view and do not take
        // ownership).
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string<T>&& buf) = delete;

        // Constructed from any type of span
        template <const_span_type T>
        explicit buffer_printer(const T& data) : buffer_printer{data.data(), data.size()}
        {}

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  //  namespace wshttp
