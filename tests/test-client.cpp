#include "utils.hpp"

int main(int argc, char* argv[])
{
    std::signal(SIGPIPE, SIG_IGN);
    // std::signal(SIGINT, wshttp::signal_handler);

    CLI::App cli{"WSHTTP test client"};

    std::string log_level{"debug"};
    cli.add_option(
        "-L,--log-level", log_level, "Log verbosity level; one of trace, debug, info, warn, error, or critical");

    std::string key_path, cert_path;
    cli.add_option("-K, --keyfile", key_path, "Path to private key file")->required();
    cli.add_option("-C, --certfile", cert_path, "Path to cert file")->required();

    try
    {
        cli.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return cli.exit(e);
    }

    wshttp::log->set_level(log_level);

    auto loop = wshttp::event_loop::make();
    auto creds = wshttp::ssl_creds::make(key_path, cert_path);
    std::shared_ptr<wshttp::endpoint> ep;

    try
    {
        ep = wshttp::endpoint::make(loop);
        ep->listen(5544, creds);
        // ep->test_parse_method("https://www.google.com");
    }
    catch (const std::exception& e)
    {
        wshttp::log->critical("Failed to start client: {}", e.what());
        return 1;
    }

    // ep.reset();

    for (;;)
        std::this_thread::sleep_for(10min);
}
