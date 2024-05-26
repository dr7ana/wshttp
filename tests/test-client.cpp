#include "utils.hpp"

int main(int argc, char* argv[])
{
    CLI::App cli{"ZHTTP test client"};

    std::string log_level = "debug";

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

    zhttp::log->set_level(log_level);

    try
    {
        auto c = zhttp::Client::make();

        c->get("127.0.0.2:6900", "connecttothatmotherfucker");
    }
    catch (const std::exception& e)
    {
        zhttp::log->critical("Failed to start client: {}!", e.what());
        return 1;
    }

    for (;;)
        std::this_thread::sleep_for(10min);
}
