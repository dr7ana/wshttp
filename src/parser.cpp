#include "parser.hpp"

#include "address.hpp"
#include "internal.hpp"

namespace wshttp
{
    std::shared_ptr<url_parser> parser = url_parser::make();

    std::shared_ptr<url_parser> url_parser::make()
    {
        static std::shared_ptr<url_parser> p;
        if (not p)
            p = std::shared_ptr<url_parser>{new url_parser{}};
        return p;
    }

    url_result_ptr url_parser::parse(std::string_view input, const url_result_ptr& base)
    {
        url_result_ptr ret = nullptr;
        if (base)
            ret = std::make_unique<url_result>(ada::parse<ada::url_aggregator>(input, &base->value()));
        else
            ret = std::make_unique<url_result>(ada::parse<ada::url_aggregator>(input));

        if (not ret or not *ret)
            log->warn("Parser failed to parse url input: {}", input);

        return ret;
    }

}  //  namespace wshttp
