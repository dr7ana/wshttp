#include "parser.hpp"

#include "address.hpp"
#include "internal.hpp"

#include <ada.h>

namespace wshttp
{
    auto parser = url_parser::make();

    std::shared_ptr<url_parser> url_parser::make()
    {
        static std::shared_ptr<url_parser> p;
        if (not p)
            p = std::shared_ptr<url_parser>{new url_parser{}};
        return p;
    }

    uri url_parser::extract()
    {
        return uri{
            {_url()->get_protocol().data(),
             _url()->get_username().data(),
             _url()->get_host().data(),
             _url()->get_port().data(),
             _url()->get_pathname().data(),
             _url()->get_search().data(),
             _url()->get_hash().data()}};
    }

    bool url_parser::_parse()
    {
        log->debug("{} called", __PRETTY_FUNCTION__);
        _res = std::make_unique<url_result>(ada::parse<ada::url_aggregator>(*_data));

        if (not _res or not *_res)
        {
            log->critical("Parser failed to parse url input: {}", *_data);
            return false;
        }

        if (not _url()->set_protocol(https_proto))
        {
            log->critical("Something serious went wrong setting protocol:{} on url input: {}", https_proto, *_data);
            return false;
        }

        log->debug("Successfully parsed input...");
        return true;
    }

    bool url_parser::read(std::string input)
    {
        _data.reset();
        _data = std::make_unique<std::string>(std::move(input));
        if (_parse())
            return true;
        _reset();
        return false;
    }

    url_result& url_parser::url()
    {
        try
        {
            return _url();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error{"Bad access to url parser result: {}"_format(e.what())};
        }
    }

    std::string url_parser::href_str()
    {
        return std::string{_res ? _url()->get_href() : ""};
    }

    std::string_view url_parser::href_sv()
    {
        return std::string_view{_res ? _url()->get_href() : ""};
    }

    url_result& url_parser::_url()
    {
        return *_res;
    }

    void url_parser::_reset()
    {
        _data.reset();
        _res.reset();
    }

    void url_parser::print_aggregates()
    {
        if (not _res)
        {
            log->critical("Parser has no ada::parse result to reference!");
            return;
        }

        auto u = extract();
        log->critical("{}", u.host());

        log->critical("scheme: {}", u.scheme());
        log->critical("userinfo: {}", u.userinfo());
        log->critical("host: {}", u.host());
        log->critical("port: {}", u.port());
        log->critical("special port: {}", _url()->get_special_port());
        log->critical("path: {}", u.path());
        log->critical("query: {}", u.query());
        log->critical("fragment: {}", u.fragment());
    }

}  //  namespace wshttp
