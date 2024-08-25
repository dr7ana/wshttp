#include "format.hpp"

#include <stdexcept>

namespace wshttp
{
    // global logger from format.hpp
    std::shared_ptr<Logger> log = Logger::make_logger();
    std::shared_ptr<spdlog::sinks::dist_sink_mt> sink = Logger::make_sink();

    const auto started_at = std::chrono::steady_clock::now();

    std::shared_ptr<Logger> Logger::make_logger()
    {
        static std::shared_ptr<Logger> l;
        if (not l)
            l = std::make_shared<Logger>();
        return l;
    }

    std::shared_ptr<spdlog::sinks::dist_sink_mt> Logger::make_sink()
    {
        static std::shared_ptr<spdlog::sinks::dist_sink_mt> s;
        if (not s)
            s = std::make_shared<spdlog::sinks::dist_sink_mt>();
        return s;
    }

    // Custom log formatting flag that prints the elapsed time since startup
    class startup_elapsed_flag : public spdlog::custom_flag_formatter
    {
      private:
        static constexpr fmt::format_string<
            std::chrono::hours::rep,
            std::chrono::minutes::rep,
            std::chrono::seconds::rep,
            std::chrono::milliseconds::rep>
            format_hours{"+{0:d}h{1:02d}m{2:02d}.{3:03d}s"},  // >= 1h
            format_minutes{"+{1:d}m{2:02d}.{3:03d}s"},        // >= 1min
            format_seconds{"+{2:d}.{3:03d}s"};                // < 1min

      public:
        void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
        {
            using namespace std::literals;
            auto elapsed = std::chrono::steady_clock::now() - started_at;

            dest.append(fmt::format(
                elapsed >= 1h         ? format_hours
                    : elapsed >= 1min ? format_minutes
                                      : format_seconds,
                std::chrono::duration_cast<std::chrono::hours>(elapsed).count(),
                (std::chrono::duration_cast<std::chrono::minutes>(elapsed) % 1h).count(),
                (std::chrono::duration_cast<std::chrono::seconds>(elapsed) % 1min).count(),
                (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed) % 1s).count()));
        }

        std::unique_ptr<custom_flag_formatter> clone() const override
        {
            return std::make_unique<startup_elapsed_flag>();
        }
    };

    Logger::Logger(std::string sink, std::string level)
    {
        _logger_init(std::move(sink), level);
    }

    void Logger::set_level(std::string level)
    {
        _logger->set_level(_translate_level(level));
    }

    spdlog::level::level_enum Logger::_translate_level(std::string_view level)
    {
        if (level == "trace")
            return spdlog::level::trace;
        else if (level == "debug")
            return spdlog::level::debug;
        else if (level == "info")
            return spdlog::level::info;
        else if (level == "warn")
            return spdlog::level::warn;
        else if (level == "err")
            return spdlog::level::err;
        else if (level == "critical")
            return spdlog::level::critical;
        else if (level == "off")
            return spdlog::level::off;
        else
            throw std::invalid_argument{
                "Log level must be one of 'trace', 'debug', 'info', 'warn', 'err', 'critical', or 'off'"};
    }

    void Logger::_logger_init(std::string s, std::string level)
    {
        if (s == "stderr")
            _logger = spdlog::stderr_color_mt("wshttp", spdlog::color_mode::always);
        else if (s == "stdout")
            _logger = spdlog::stdout_color_mt("wshttp", spdlog::color_mode::always);

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<startup_elapsed_flag>('*');
        formatter->set_pattern(PATTERN_COLOR);
        _logger->set_formatter(std::move(formatter));
        _logger->set_level(_translate_level(level));
    }

    Logger::~Logger()
    {
        spdlog::shutdown();
    }

}  // namespace wshttp
