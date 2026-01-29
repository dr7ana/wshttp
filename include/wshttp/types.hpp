#pragma once

#include "encoding.hpp"
#include "utils.hpp"

#include <unlog.hpp>

#include <algorithm>
#include <cstdint>
#include <span>

namespace wshttp {
    using cspan = unlog::cspan;
    using uspan = unlog::uspan;
    using bspan = unlog::bspan;

    using request_id_t = size_t;

    using namespace unlog::literals;
    using namespace un::log::operators;

}  //  namespace wshttp
