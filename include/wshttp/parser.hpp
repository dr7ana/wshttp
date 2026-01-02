#pragma once

#include "types.hpp"

#include <ada.h>

namespace wshttp {
    using url_result = ada::result<ada::url_aggregator>;
    // using url_result_ptr = std::shared_ptr<url_result>;
    using url_result_ptr = std::unique_ptr<url_result>;

    class url_parser {
        url_parser() = default;

      public:
        static std::shared_ptr<url_parser> make();

        url_result_ptr parse(std::string_view input, const url_result_ptr& base = nullptr);
    };

}  // namespace wshttp
