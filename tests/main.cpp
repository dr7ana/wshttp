#include "utils.hpp"

#include <catch2/catch_session.hpp>

using namespace Catch::Clara;

int main(int argc, char* argv[])
{
    Catch::Session session;

    std::string log_level = "debug";

    auto cli = session.cli()
        | Opt(log_level, "level")["--log-level"](
                   "log level to apply to the test run (one of trace, debug, info, warn, error, or critical)");

    session.cli(cli);

    if (int rc = session.applyCommandLine(argc, argv); rc != 0)
        return rc;

    wshttp3::log->set_level(log_level);

    return session.run(argc, argv);
}
