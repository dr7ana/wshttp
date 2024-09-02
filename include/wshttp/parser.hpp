#pragma once

#include "utils.hpp"

#include <ada.h>

/**
    TODO:
        - make parser entirely internal
        - access through static methods defined in .cpp files
            ex: uri::parse(...)
 */
namespace wshttp
{
    using url_result = ada::result<ada::url_aggregator>;

    static constexpr auto https_proto = "https"sv;

    struct uri;

    class url_parser
    {
        std::unique_ptr<std::string> _data{};

        std::unique_ptr<url_result> _res;

        url_parser() = default;

      public:
        static std::shared_ptr<url_parser> make();

        bool read(std::string input);

        void print_aggregates();

        uri extract();

        url_result& url();

        std::string href_str();
        std::string_view href_sv();

      private:
        url_result& _url();  // does no safety checking
        bool _parse();
        void _reset();
    };

    // global parser
    extern std::shared_ptr<url_parser> parser;
}  //  namespace wshttp
