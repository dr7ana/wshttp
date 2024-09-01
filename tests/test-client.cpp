#include "utils.hpp"

int main(int argc, char* argv[])
{
    CLI::App cli{"WSHTTP test client"};

    std::string log_level{"debug"};

    cli.add_option(
        "-L,--log-level", log_level, "Log verbosity level; one of trace, debug, info, warn, error, or critical");

    try
    {
        cli.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return cli.exit(e);
    }

    wshttp::log->set_level(log_level);

    auto loop = wshttp::Loop::make();
    std::shared_ptr<wshttp::Endpoint> ep;

    try
    {
        ep = wshttp::Endpoint::make(loop);
        ep->listen(5544);
        ep->listen(5545);
        ep->listen(5546);
        ep->test_parse_method("https://www.google.com");
    }
    catch (const std::exception& e)
    {
        wshttp::log->critical("Failed to start client: {}", e.what());
        return 1;
    }

    ep.reset();

    for (;;)
        std::this_thread::sleep_for(10min);
}
