#pragma once

extern "C" {
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent_struct.h>
#include <event2/http.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
}

#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace wshttp
{
    using namespace std::literals;
    namespace fs = std::filesystem;

    inline constexpr auto localhost = "127.0.0.1"sv;

    inline constexpr size_t inverse_golden_ratio = sizeof(size_t) >= 8 ? 0x9e37'79b9'7f4a'7c15 : 0x9e37'79b9;

    namespace detail
    {
        std::chrono::steady_clock::time_point get_time();

        std::string localhost_ip(uint16_t port);

        template <std::integral T>
        constexpr bool increment_will_overflow(T val)
        {
            return std::numeric_limits<T>::max() == val;
        }
    }  // namespace detail

}  //  namespace wshttp
