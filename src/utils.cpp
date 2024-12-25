#include "utils.hpp"

namespace wshttp
{
    namespace detail
    {
        std::chrono::steady_clock::time_point get_time()
        {
            return std::chrono::steady_clock::now();
        }
    }  //  namespace detail
}  //  namespace wshttp
