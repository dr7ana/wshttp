#pragma once

#include "utils.hpp"

namespace wshttp
{
    class stream
    {
        stream();

        // No copy, no move; always hold in shared_ptr using static ::make()
        stream(const stream&) = delete;
        stream& operator=(const stream&) = delete;
        stream(stream&&) = delete;
        stream& operator=(stream&&) = delete;

      public:
        static std::shared_ptr<stream> make();

        ~stream() = default;

      private:
        std::vector<char> _uri;
        int32_t _id;

      public:
        //
    };
}  //  namespace wshttp
