#include "utils.hpp"

int main(int argc, char* argv[])
{
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    std::signal(SIGPIPE, SIG_IGN);
    // std::signal(SIGINT, wshttp::signal_handler);

    CLI::App cli{"WSHTTP test client"};

    std::string log_level{"debug"};
    cli.add_option(
        "-L,--log-level", log_level, "Log verbosity level; one of trace, debug, info, warn, error, or critical");

    std::string key_path, cert_path;
    cli.add_option("-K, --keyfile", key_path, "Path to private key file");
    cli.add_option("-C, --certfile", cert_path, "Path to cert file");

    try
    {
        cli.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return cli.exit(e);
    }

    wshttp::log->set_level(log_level);

    std::shared_ptr<wshttp::endpoint> ep;
    std::shared_ptr<wshttp::ssl_creds> creds;

    auto loop = wshttp::event_loop::make();
    if (not key_path.empty() and not cert_path.empty())
        creds = wshttp::ssl_creds::make(key_path, cert_path);

    try
    {
        ep = wshttp::endpoint::make(loop, creds);
        if (creds)
            ep->listen(5544);
        else
            ep->listen(5544);
        // ep->test_parse_method("https://www.google.com");
        ep->connect("https://www.google.com");
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
