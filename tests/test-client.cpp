#include "utils.hpp"

int main(int argc, char* argv[])
{
    std::signal(SIGPIPE, SIG_IGN);
    // std::signal(SIGINT, wshttp::signal_handler);

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

    std::shared_ptr<wshttp::ssl_creds> creds;

    auto loop = wshttp::event_loop::make();
    creds = (!key_path.empty() && !cert_path.empty()) ? wshttp::ssl_creds::make(key_path, cert_path)
                                                      : wshttp::ssl_creds::make();

    std::shared_ptr<wshttp::endpoint> ep;

    try
    {
        ep = wshttp::endpoint::make(loop, creds);

        ep->listen(5544);
        ep->listen(5545);
        ep->listen(5546);
        // ep->test_extract_method("https://www.google.com", "https://www.reddit.com", "https://www.nytimes.com");
        // ep->test_parse_method("https://www.google.com");
        ep->test_get("https://www.google.com");
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
