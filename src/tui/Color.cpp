#include "tui/Color.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace tetromino::tui {

namespace {

std::uint8_t clampChannel(float v) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}

int squaredDistance(Rgb a, Rgb b) {
    const int dr = a.r - b.r;
    const int dg = a.g - b.g;
    const int db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

// The six intensity steps used by the 6x6x6 colour cube in the xterm 256
// palette (indices 16..231).
constexpr std::array<int, 6> kCubeSteps{0, 95, 135, 175, 215, 255};

int nearestCubeIndex(int channel) {
    int best = 0;
    for (int i = 1; i < 6; ++i) {
        if (std::abs(kCubeSteps[static_cast<std::size_t>(i)] - channel) <
            std::abs(kCubeSteps[static_cast<std::size_t>(best)] - channel)) {
            best = i;
        }
    }
    return best;
}

// Approximate xterm defaults for the 16 base colours.
constexpr std::array<Rgb, 16> kAnsi16Palette{{
    {0, 0, 0},       {205, 0, 0},     {0, 205, 0},     {205, 205, 0},
    {0, 0, 238},     {205, 0, 205},   {0, 205, 205},   {229, 229, 229},
    {127, 127, 127}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
    {92, 92, 255},   {255, 0, 255},   {0, 255, 255},   {255, 255, 255},
}};

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

}  // namespace

Color Color::darkened(float factor) const {
    if (isDefault()) {
        return *this;
    }
    return Color::rgb(clampChannel(static_cast<float>(rgb_.r) * factor),
                      clampChannel(static_cast<float>(rgb_.g) * factor),
                      clampChannel(static_cast<float>(rgb_.b) * factor));
}

Color Color::blended(Color other, float t) const {
    if (isDefault() || other.isDefault()) {
        return *this;
    }
    const auto mix = [t](std::uint8_t a, std::uint8_t b) {
        return clampChannel(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t);
    };
    const Rgb o = other.value();
    return Color::rgb(mix(rgb_.r, o.r), mix(rgb_.g, o.g), mix(rgb_.b, o.b));
}

std::uint8_t toAnsi256(Rgb c) {
    // Candidate 1: the colour cube.
    const int ri = nearestCubeIndex(c.r);
    const int gi = nearestCubeIndex(c.g);
    const int bi = nearestCubeIndex(c.b);
    const Rgb cube{static_cast<std::uint8_t>(kCubeSteps[static_cast<std::size_t>(ri)]),
                   static_cast<std::uint8_t>(kCubeSteps[static_cast<std::size_t>(gi)]),
                   static_cast<std::uint8_t>(kCubeSteps[static_cast<std::size_t>(bi)])};
    const int cubeIndex = 16 + 36 * ri + 6 * gi + bi;

    // Candidate 2: the 24-step greyscale ramp (232..255), values 8, 18, ..., 238.
    const int avg = (c.r + c.g + c.b) / 3;
    const int greyStep = std::clamp((avg - 8 + 5) / 10, 0, 23);
    const auto greyValue = static_cast<std::uint8_t>(8 + greyStep * 10);
    const Rgb grey{greyValue, greyValue, greyValue};

    return static_cast<std::uint8_t>(squaredDistance(c, grey) < squaredDistance(c, cube)
                                         ? 232 + greyStep
                                         : cubeIndex);
}

std::uint8_t toAnsi16(Rgb c) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < kAnsi16Palette.size(); ++i) {
        if (squaredDistance(c, kAnsi16Palette[i]) < squaredDistance(c, kAnsi16Palette[best])) {
            best = i;
        }
    }
    return static_cast<std::uint8_t>(best);
}

ColorMode detectColorMode() {
    // https://no-color.org: any non-empty value disables colour.
    if (const char* noColor = std::getenv("NO_COLOR"); noColor != nullptr && *noColor != '\0') {
        return ColorMode::Monochrome;
    }
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm != nullptr) {
        const std::string_view ct{colorterm};
        if (ct == "truecolor" || ct == "24bit") {
            return ColorMode::TrueColor;
        }
    }
    const char* termEnv = std::getenv("TERM");
    const std::string_view term = termEnv != nullptr ? termEnv : "";
    if (term.empty() || term == "dumb") {
        return ColorMode::Monochrome;
    }
    // Terminals known to support truecolor even when COLORTERM isn't forwarded
    // (e.g. over ssh).
    if (contains(term, "kitty") || contains(term, "foot") || contains(term, "alacritty") ||
        contains(term, "wezterm") || contains(term, "direct")) {
        return ColorMode::TrueColor;
    }
    if (contains(term, "256color")) {
        return ColorMode::Ansi256;
    }
    return ColorMode::Ansi16;
}

bool isLight(Rgb c) {
    // Rec. 709 luma weights; good enough to tell a light theme from a dark one.
    const double luma = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    return luma > 127.5;
}

std::optional<Rgb> parseBackgroundReply(std::string_view bytes) {
    constexpr std::string_view kPrefix = "\x1b]11;rgb:";
    const std::size_t start = bytes.find(kPrefix);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    std::string_view rest = bytes.substr(start + kPrefix.size());

    // Three '/'-separated hex components, terminated by BEL or ESC '\'.
    std::array<int, 3> values{};
    for (std::size_t i = 0; i < 3; ++i) {
        std::size_t len = 0;
        unsigned long value = 0;
        while (len < rest.size() && len < 4) {
            const char ch = rest[len];
            int digit = -1;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
            if (digit < 0) break;
            value = value * 16 + static_cast<unsigned long>(digit);
            ++len;
        }
        if (len == 0) {
            return std::nullopt;
        }
        // Scale n hex digits to 0-255 (e.g. "ffff" -> 255, "f" -> 255).
        const unsigned long max = (1UL << (4 * len)) - 1;
        values[i] = static_cast<int>((value * 255 + max / 2) / max);
        rest.remove_prefix(len);
        if (i < 2) {
            if (rest.empty() || rest.front() != '/') {
                return std::nullopt;
            }
            rest.remove_prefix(1);
        }
    }
    if (!(rest.starts_with("\x07") || rest.starts_with("\x1b\\"))) {
        return std::nullopt;
    }
    return Rgb{static_cast<std::uint8_t>(values[0]), static_cast<std::uint8_t>(values[1]),
               static_cast<std::uint8_t>(values[2])};
}

std::optional<bool> lightBackgroundFromColorFgBg(const char* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    // The background is the last field: "fg;bg" or "fg;default;bg".
    const std::string_view v{value};
    const std::size_t semi = v.rfind(';');
    if (semi == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view bg = v.substr(semi + 1);
    if (bg.empty() || bg.size() > 2 || bg.find_first_not_of("0123456789") != std::string_view::npos) {
        return std::nullopt;
    }
    const int index = std::stoi(std::string{bg});
    // In the 16-colour palette, 7 (white) and 9-15 (bright colours) read as
    // light backgrounds; 0-6 and 8 as dark.
    return index == 7 || index >= 9;
}

}  // namespace tetromino::tui
