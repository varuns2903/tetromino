// tetromino entry point: parse options, run the application, report errors.
// Everything interesting lives in app/Application.cpp.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <variant>

#include "app/Application.hpp"
#include "app/Options.hpp"
#include "util/Logger.hpp"

#ifndef TETROMINO_VERSION
#define TETROMINO_VERSION "dev"
#endif

namespace {

bool debugFromEnvironment() {
    const char* value = std::getenv("TETROMINO_DEBUG");
    return value != nullptr && *value != '\0' && std::string{value} != "0";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace tetromino;

    const auto parsed = app::parseOptions(argc, argv);
    if (const auto* error = std::get_if<std::string>(&parsed)) {
        std::fprintf(stderr, "tetromino: %s\nTry '%s --help'.\n", error->c_str(), argv[0]);
        return 2;
    }
    const app::Options& options = std::get<app::Options>(parsed);
    if (options.showHelp) {
        std::fputs(app::usage(argv[0]).c_str(), stdout);
        return 0;
    }
    if (options.showVersion) {
        std::printf("tetromino %s\n", TETROMINO_VERSION);
        return 0;
    }

    util::Logger logger = (options.debug || debugFromEnvironment()) ? util::Logger{util::Logger::defaultPath()}
                                                                    : util::Logger{};
    try {
        app::Application application{options, logger};
        const app::RunSummary summary = application.run();
        // The terminal has been restored at this point, so plain stdout is
        // safe again.
        if (summary.played) {
            std::printf("Thanks for playing! Score %llu · %d lines · level %d\n",
                        static_cast<unsigned long long>(summary.lastGame.score), summary.lastGame.lines,
                        summary.lastGame.level);
        }
    } catch (const std::exception& e) {
        logger.error(e.what());
        std::fprintf(stderr, "tetromino: %s\n", e.what());
        return 1;
    }
    return 0;
}
