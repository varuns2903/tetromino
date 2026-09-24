#pragma once

// Command-line options.

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "tui/Color.hpp"

namespace tetromino::app {

struct Options {
    std::optional<std::uint64_t> seed;       // --seed N: reproducible piece sequence
    int startLevel = 1;                      // --level N
    std::optional<tui::ColorMode> colorMode; // --color MODE (default: auto-detect)
    int fps = 60;                            // --fps N
    bool debug = false;                      // --debug: write a log file
    bool showHelp = false;
    bool showVersion = false;
};

// Parse argv. Returns the options, or an error message.
[[nodiscard]] std::variant<Options, std::string> parseOptions(int argc, const char* const* argv);

[[nodiscard]] std::string usage(const char* programName);

}  // namespace tetromino::app
