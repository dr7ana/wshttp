#include "format.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace wshttp
{
    // global logger from format.hpp
    std::shared_ptr<Logger> log = std::make_shared<Logger>();

    Logger::Logger(std::string sink, std::string level)
    {
        _logger_init(sink, level);
    }

    void Logger::set_level(std::string level)
    {
        _logger->set_level(_translate_level(level));
    }

    spdlog::level::level_enum Logger::_translate_level(std::string level)
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

    void Logger::_logger_init(std::string sink, std::string level)
    {
        if (sink == "stderr")
            _logger = spdlog::stderr_color_mt("wshttp", spdlog::color_mode::always);
        else if (sink == "stdout")
            _logger = spdlog::stdout_color_mt("wshttp", spdlog::color_mode::always);

        auto formatter = std::make_shared<spdlog::pattern_formatter>();

        _logger->set_level(_translate_level(level));
        _logger->set_pattern(PATTERN_COLOR);
    }

    Logger::~Logger()
    {
        spdlog::shutdown();
    }

}  // namespace wshttp
