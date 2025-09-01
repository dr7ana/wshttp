#pragma once

#include "encoding.hpp"
#include "utils.hpp"

#include <unlog.hpp>

#include <algorithm>
#include <cstdint>
#include <span>

namespace wshttp
{
    using cspan = unlog::cspan;
    using uspan = unlog::uspan;
    using bspan = unlog::bspan;

    using namespace unlog::literals;

}  //  namespace wshttp
