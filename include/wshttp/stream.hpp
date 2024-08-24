#pragma once

#include "utils.hpp"

namespace wshttp
{
    class Stream
    {
      public:
        static std::shared_ptr<Stream> make();

        // No copy, no move; always hold in shared_ptr using static ::make()
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        Stream(Stream&&) = delete;
        Stream& operator=(Stream&&) = delete;

      private:
        Stream();

        const std::string _uri;
        const int32_t _id;

      public:
        //
    };
}  //  namespace wshttp
