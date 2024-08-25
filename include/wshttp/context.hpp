#pragma once

#include "utils.hpp"

namespace wshttp
{
    using ssl_ptr = std::unique_ptr<::SSL_CTX, decltype(ssl_deleter)>;

    class app_context
    {
        app_context();

      public:
        static std::unique_ptr<app_context> make_context();

        ~app_context() = default;

      private:
        ssl_ptr _ctx;

        void _init_internals();
    };
}  //  namespace wshttp
