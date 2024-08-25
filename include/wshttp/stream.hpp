#pragma once

#include "utils.hpp"

namespace wshttp
{
    class Stream
    {
        Stream();

        // No copy, no move; always hold in shared_ptr using static ::make()
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        Stream(Stream&&) = delete;
        Stream& operator=(Stream&&) = delete;

      public:
        static std::shared_ptr<Stream> make();

        ~Stream();

      private:
        std::vector<char> _uri;
        int32_t _id;

      public:
        //
    };
}  //  namespace wshttp
