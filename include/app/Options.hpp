#pragma once

// Command-line options.

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "core/Presets.hpp"
#include "tui/Color.hpp"

namespace tetromino::app {

enum class ThemeChoice : std::uint8_t { Auto, Dark, Light };

struct Options {
    std::optional<std::uint64_t> seed;       // --seed N: reproducible piece sequence
    int startLevel = 1;                      // --level N
    std::size_t gameType = core::kDefaultGameType;      // --mode NAME (pre-selected in the menu)
    std::size_t boardSize = core::kDefaultBoardSize;    // --size NAME (pre-selected in the menu)
    std::size_t difficulty = core::kDefaultDifficulty;  // --difficulty NAME (pre-selected)
    std::optional<tui::ColorMode> colorMode; // --color MODE (default: auto-detect)
    ThemeChoice theme = ThemeChoice::Auto;   // --theme auto|dark|light
    int fps = 60;                            // --fps N
    int dasMs = 167;                         // --das MS: delay before a held move repeats
    int arrMs = 33;                          // --arr MS: time between repeated moves (0 = instant)
    bool legacyKeys = false;                 // --legacy-keys: ignore key-release support
    bool debug = false;                      // --debug: write a log file
    bool showHelp = false;
    bool showVersion = false;
};

// Parse argv. Returns the options, or an error message.
[[nodiscard]] std::variant<Options, std::string> parseOptions(int argc, const char* const* argv);

[[nodiscard]] std::string usage(const char* programName);

}  // namespace tetromino::app
