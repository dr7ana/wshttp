#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/cfg/env.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <concepts>

using namespace std::literals;

namespace wshttp3
{
    inline const auto PATTERN_COLOR = "[%H:%M:%S.%e] >> [\x1b[1m%n\x1b[0m:%^%l%$] >> %v"s;

    // Types can opt-in to being fmt-formattable by ensuring they have a ::to_string() method defined
    template <typename T>
    concept ToStringFormattable = requires(T a) {
        {
            a.to_string()
        } -> std::convertible_to<std::string_view>;
    };

    class Logger
    {
      public:
        explicit Logger(std::string sink = "stderr", std::string level = "trace");

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
        void error(const T& msg)
        {
            _logger->error(msg);
        }

        template <typename... Args>
        void error(fmt::format_string<Args...> fmt, Args&&... args)
        {
            _logger->error(std::move(fmt), std::forward<Args>(args)...);
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

        void set_level(std::string level);

        ~Logger();

      private:
        std::shared_ptr<spdlog::logger> _logger;

        void _logger_init(std::string sink, std::string level);
        spdlog::level::level_enum _translate_level(std::string level);
    };

    // global logger
    extern std::shared_ptr<Logger> log;

}  //  namespace wshttp3

namespace fmt
{
    template <wshttp3::ToStringFormattable T>
    struct formatter<T, char> : formatter<std::string_view>
    {
        template <typename FormatContext>
        auto format(const T& val, FormatContext& ctx) const
        {
            return formatter<std::string_view>::format(val.to_string(), ctx);
        }
    };
}  // namespace fmt
