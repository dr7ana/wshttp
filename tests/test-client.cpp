#include "utils.hpp"

using namespace wshttp;

int main(int argc, char* argv[])
{
    std::signal(SIGPIPE, SIG_IGN);
    // std::signal(SIGINT, signal_handler);

    CLI::App cli{"WSHTTP test endpoint"};

    std::string log_level{"debug"};
    cli.add_option(
            "-L,--log-level", log_level, "Log verbosity lesvel; one of trace, debug, info, warn, error, or critical");

    std::string key_path, cert_path;
    cli.add_option("-K, --keyfile", key_path, "Path to private key file") /* ->required() */;
    cli.add_option("-C, --certfile", cert_path, "Path to cert file") /* ->required() */;

    try
    {
        cli.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return cli.exit(e);
    }

    wshttp::log->set_level(log_level);

    std::shared_ptr<ssl_creds> creds;

    auto loop = event_loop::make();
    creds = (!key_path.empty() && !cert_path.empty()) ? ssl_creds::make(key_path, cert_path) : ssl_creds::make();

    std::shared_ptr<endpoint> ep;

    session_opts sopts{[](std::vector<char> buf) {
        // std::fwrite(buf.data(), buf.size(), 1, stderr);
        wshttp::log->info("User supplied session callback invoked! Received {}B payload", buf.size());
    }};

    request_opts ropts{[](std::vector<char> buf) {
        // std::fwrite(buf.data(), buf.size(), 1, stderr);
        wshttp::log->info("User supplied request callback invoked! Received {}B payload", buf.size());
    }};

    try
    {
        ep = endpoint::make(loop, creds);

        ep->listen(5544);
        ep->listen(5545);
        ep->listen(5546);

        auto session = ep->initiate_session("https://www.google.com", std::move(sopts));

        // ep->test_extract_method("https://www.google.com", "https://www.reddit.com", "https://www.nytimes.com");
        // ep->test_parse_method("https://www.google.com");
        session->request(METHOD::GET);
        session->request(METHOD::GET, std::move(ropts));
        session->request(METHOD::GET, request_opts{hdr_flags::CLOSE});
        // ep->test_get("https://www.google.com");
        // ep->test_get("http://www.google.com");
    }
    catch (const std::exception& e)
    {
        wshttp::log->critical("Test endpoint runtime exception: {}", e.what());
        return 1;
    }

    for (;;)
        std::this_thread::sleep_for(10min);
}
