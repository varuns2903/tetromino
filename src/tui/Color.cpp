#include "tui/Color.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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

}  // namespace tetromino::tui
