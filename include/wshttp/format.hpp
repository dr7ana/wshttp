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
    // Types can opt-in to being fmt-formattable by ensuring they have a ::to_string() method
    // defined
    template <typename T>
    concept to_string_formattable = T::to_string_formattable && requires(T a) {
        {
            a.to_string()
        } -> std::convertible_to<std::string_view>;
    };
}  // namespace wshttp

namespace fmt
{
    template <wshttp::to_string_formattable T>
    struct formatter<T, char> : formatter<std::string_view>
    {
        template <typename FormatContext>
        auto format(const T& val, FormatContext& ctx) const
        {
            return formatter<std::string_view>::format(val.to_string(), ctx);
        }
    };

    template <wshttp::const_span_type T>
    struct formatter<T, char> : formatter<std::string_view>
    {
        template <typename FormatContext>
        auto format(const T& val, FormatContext& ctx) const
        {
            return formatter<std::string_view>::format(
                    std::string_view{reinterpret_cast<const char*>(val.data()), val.size()}, ctx);
        }
    };
}  // namespace fmt

namespace wshttp
{
    inline const auto PATTERN_COLOR = "[%H:%M:%S.%e] [%*] [\x1b[1m%n\x1b[0m:%^%l%$] >> %v"s;
    inline const auto PATTERN_COLOR2 = "[%H:%M:%S.%e] [%*] [\x1b[1m%n\x1b[0m:%^%l%$|\x1b[3m%g:%#\x1b[0m] >> %v"s;

    namespace detail
    {
        template <size_t N>
        struct string_literal
        {
            consteval string_literal(const char (&s)[N]) { std::copy(s, s + N, str.begin()); }

            consteval std::string_view sv() const { return {str.data(), N - 1}; }
            std::array<char, N> str;
        };

        template <string_literal Format>
        struct fmt_wrapper
        {
            consteval fmt_wrapper() = default;

            /// Calling on this object forwards all the values to fmt::format, using the format
            /// string as provided during type definition (via the "..."_format user-defined
            /// function).
            template <typename... T>
            constexpr auto operator()(T&&... args) &&
            {
                return fmt::format(Format.sv(), std::forward<T>(args)...);
            }
        };
    }  //  namespace detail

    template <detail::string_literal Format>
    inline consteval auto operator""_format()
    {
        return detail::fmt_wrapper<Format>{};
    }

    class Logger
    {
      public:
        explicit Logger(std::string sink = "stderr", std::string level = "trace");

        static std::shared_ptr<Logger> make_logger();
        static std::shared_ptr<spdlog::sinks::dist_sink_mt> make_sink();

        template <typename T>
        void trace(const T& msg)
        {
            _logger->trace(msg);
        }

        template <typename... Args>
        void trace(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->trace(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename T>
        void debug(const T& msg)
        {
            _logger->debug(msg);
        }

        template <typename... Args>
        void debug(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->debug(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename T>
        void info(const T& msg)
        {
            _logger->info(msg);
        }

        template <typename... Args>
        void info(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->info(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename T>
        void warn(const T& msg)
        {
            _logger->warn(msg);
        }

        template <typename... Args>
        void warn(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->warn(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename T>
        void critical(const T& msg)
        {
            _logger->critical(msg);
        }

        template <typename... Args>
        void critical(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->critical(std::move(fmt), std::forward<Args>(args)...);
        }

        template <typename T>
        void error(const T& msg)
        {
            _logger->error(msg);
        }

        template <typename... Args>
        void error(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->error(std::move(fmt), std::forward<Args>(args)...);
        }

        void set_level(std::string level);

        ~Logger();

      private:
        std::shared_ptr<spdlog::logger> _logger;

        void _logger_init(std::string sink, std::string level);
        spdlog::level::level_enum _translate_level(std::string_view level);
    };

    class Logger;

    // global logger
    extern std::shared_ptr<Logger> log;
    extern std::shared_ptr<spdlog::sinks::dist_sink_mt> sink;

    template <typename T>
    concept string_view_convertible =
            std::convertible_to<T, std::string_view> || std::convertible_to<T, std::basic_string_view<unsigned char>> ||
            std::convertible_to<T, std::basic_string_view<std::byte>>;

    struct buffer_printer
    {
        std::basic_string_view<std::byte> buf;

        template <typename T>
            requires const_span_convertible<T> || string_view_convertible<T>
        explicit buffer_printer(T buf) : buffer_printer{buf.data(), buf.size()}
        {}

        // Constructed from any type of string_view<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.)
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string_view<T> buf) :
                buf{reinterpret_cast<const std::byte*>(buf.data()), buf.size()}
        {}

        // Constructed from any type of lvalue string<T> for a single-byte T (char, std::byte,
        // uint8_t, etc.
        template <enc::basic_char T>
        explicit buffer_printer(const std::basic_string<T>& buf) : buffer_printer(std::basic_string_view<T>{buf})
        {}

        // *Not* constructable from a string<T> rvalue (no taking ownership)
        template <enc::basic_char T>
        explicit buffer_printer(std::basic_string<T>&& buf) = delete;

        // Constructable from a (T*, size) argument pair, for byte-sized T's.
        template <enc::basic_char T>
        explicit buffer_printer(const T* data, size_t size) : buffer_printer(std::basic_string_view<T>{data, size})
        {}

        std::string to_string() const;

        static constexpr bool to_string_formattable = true;
    };

}  //  namespace wshttp
