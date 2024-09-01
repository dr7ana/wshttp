#include "internal.hpp"

#include <ada.h>

namespace wshttp
{
    using url_result = ada::result<ada::url_aggregator>;

    static constexpr auto proto = "http"sv;

    class url_parser : public _parser
    {
        std::optional<url_result> _url{};
        std::unique_ptr<url_result> _url_res{};

        bool _parse() override;

        url_parser() = default;

      public:
        static std::shared_ptr<url_parser> make() { return std::shared_ptr<url_parser>{new url_parser{}}; }

        void print_aggregates() override;
    };

    std::shared_ptr<_parser> _parser::make()
    {
        return url_parser::make();
    }

    bool url_parser::_parse()
    {
        _url = ada::parse<ada::url_aggregator>(*_data);

        if (not _url)
        {
            log->critical("Parser failed to emplace ada::parse result!");
            return false;
        }

        auto& url = *_url;

        if (not url)
        {
            log->critical("Failed to parse url input: {}", *_data);
            return false;
        }

        if (not url->set_protocol(proto))
        {
            log->critical("Something serious went wrong setting protocol:{} on url input: {}", proto, *_data);
            return false;
        }

        return true;
    }

    void url_parser::print_aggregates()
    {
        if (not _url)
        {
            log->critical("Parser has no ada::parse result to reference!");
            return;
        }

        log->critical("href: {}", (*_url)->get_href());
    }

    auto parser = detail::make_parser<_parser>();

}  //  namespace wshttp
