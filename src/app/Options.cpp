#include "app/Options.hpp"

#include <charconv>
#include <string_view>

namespace tetromino::app {

namespace {

template <typename T>
std::optional<T> parseNumber(std::string_view text) {
    T value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<tui::ColorMode> parseColorMode(std::string_view text) {
    if (text == "truecolor" || text == "24bit") {
        return tui::ColorMode::TrueColor;
    }
    if (text == "256") {
        return tui::ColorMode::Ansi256;
    }
    if (text == "16") {
        return tui::ColorMode::Ansi16;
    }
    if (text == "mono" || text == "none") {
        return tui::ColorMode::Monochrome;
    }
    return std::nullopt;
}

}  // namespace

std::variant<Options, std::string> parseOptions(int argc, const char* const* argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        // Accept both "--opt value" and "--opt=value".
        std::string_view name = arg;
        std::optional<std::string_view> inlineValue;
        if (const auto eq = arg.find('='); arg.starts_with("--") && eq != std::string_view::npos) {
            name = arg.substr(0, eq);
            inlineValue = arg.substr(eq + 1);
        }
        const auto value = [&]() -> std::optional<std::string_view> {
            if (inlineValue) {
                return inlineValue;
            }
            if (i + 1 < argc) {
                return std::string_view{argv[++i]};
            }
            return std::nullopt;
        };

        if (name == "-h" || name == "--help") {
            options.showHelp = true;
        } else if (name == "-V" || name == "--version") {
            options.showVersion = true;
        } else if (name == "--debug") {
            options.debug = true;
        } else if (name == "--mono") {
            options.colorMode = tui::ColorMode::Monochrome;
        } else if (name == "--seed") {
            const auto v = value();
            const auto seed = v ? parseNumber<std::uint64_t>(*v) : std::nullopt;
            if (!seed) {
                return std::string{"--seed expects a non-negative integer"};
            }
            options.seed = *seed;
        } else if (name == "--level") {
            const auto v = value();
            const auto level = v ? parseNumber<int>(*v) : std::nullopt;
            if (!level || *level < 1 || *level > 30) {
                return std::string{"--level expects a number between 1 and 30"};
            }
            options.startLevel = *level;
        } else if (name == "--fps") {
            const auto v = value();
            const auto fps = v ? parseNumber<int>(*v) : std::nullopt;
            if (!fps || *fps < 10 || *fps > 240) {
                return std::string{"--fps expects a number between 10 and 240"};
            }
            options.fps = *fps;
        } else if (name == "--color") {
            const auto v = value();
            const auto mode = v ? parseColorMode(*v) : std::nullopt;
            if (!mode) {
                return std::string{"--color expects one of: truecolor, 256, 16, mono"};
            }
            options.colorMode = *mode;
        } else {
            return "unknown option: " + std::string{arg};
        }
    }
    return options;
}

std::string usage(const char* programName) {
    std::string text = "Usage: ";
    text += programName;
    text += R"( [options]

Tetromino: a falling-block puzzle game for the Linux terminal.

Options:
  --level N        start at level N (1-30, default 1)
  --seed N         fixed random seed, for a reproducible piece sequence
  --color MODE     truecolor | 256 | 16 | mono   (default: auto-detect)
  --mono           same as --color mono
  --fps N          render rate, 10-240 (default 60)
  --debug          write a debug log to $XDG_STATE_HOME/tetromino/tetromino.log
  -h, --help       show this help
  -V, --version    show version

Controls:
  ← → / A D        move            ↑ / W / X      rotate clockwise
  ↓ / S            soft drop       Z              rotate counter-clockwise
  Space            hard drop       C              hold
  P / Esc          pause           R              restart (paused / game over)
  Q                quit            Ctrl-Z         suspend to shell
)";
    return text;
}

}  // namespace tetromino::app
