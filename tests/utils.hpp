#pragma once

#include <CLI/CLI.hpp>
#include <wshttp.hpp>

#include <csignal>

static volatile std::sig_atomic_t signal_status;

namespace wshttp
{
    static constexpr auto KEY_FILE_PLACEHOLDER = ""sv;
    static constexpr auto CERT_FILE_PLACEHOLDER = ""sv;

    inline void signal_handler(int s)
    {
        signal_status = s;
    }

}  //  namespace wshttp
