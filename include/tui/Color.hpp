#pragma once

// Colour model for the TUI.
//
// Everything above the ANSI layer talks in 24-bit RGB. How that RGB value is
// actually sent to the terminal depends on what the terminal supports
// (ColorMode), and that decision is made in exactly one place: ansi::appendStyle.
// This lets the renderer and themes stay oblivious to terminal capabilities.

#include <cstdint>
#include <optional>
#include <string_view>

namespace tetromino::tui {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend constexpr bool operator==(Rgb, Rgb) = default;
};

// What the terminal can display. Detected from the environment at startup.
enum class ColorMode : std::uint8_t {
    Monochrome,  // no colour at all (NO_COLOR, dumb terminals)
    Ansi16,      // SGR 30-37 / 90-97
    Ansi256,     // SGR 38;5;n  (xterm-256color)
    TrueColor,   // SGR 38;2;r;g;b (COLORTERM=truecolor)
};

class Color {
public:
    enum class Kind : std::uint8_t { Default, Rgb };

    // "Default" means "whatever the terminal's default fg/bg is" (SGR 39/49).
    constexpr Color() = default;
    static constexpr Color terminalDefault() { return Color{}; }
    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        return Color{Rgb{r, g, b}};
    }

    [[nodiscard]] constexpr Kind kind() const { return kind_; }
    [[nodiscard]] constexpr Rgb value() const { return rgb_; }
    [[nodiscard]] constexpr bool isDefault() const { return kind_ == Kind::Default; }

    // Returns a colour scaled towards black: factor 1.0 = unchanged, 0.0 = black.
    [[nodiscard]] Color darkened(float factor) const;
    // Returns a colour blended towards `other` by `t` (0 = this, 1 = other).
    [[nodiscard]] Color blended(Color other, float t) const;

    friend constexpr bool operator==(Color, Color) = default;

private:
    constexpr explicit Color(Rgb value) : kind_(Kind::Rgb), rgb_(value) {}

    Kind kind_ = Kind::Default;
    Rgb rgb_{};
};

// Visual attributes of one terminal cell (besides the glyph itself).
struct Style {
    Color fg{};
    Color bg{};
    bool bold = false;
    bool dim = false;

    friend constexpr bool operator==(const Style&, const Style&) = default;
};

// Palette downsampling, used when the terminal isn't truecolor-capable.
[[nodiscard]] std::uint8_t toAnsi256(Rgb c);
// Returns 0..15: 0-7 are the normal colours, 8-15 the bright ones.
[[nodiscard]] std::uint8_t toAnsi16(Rgb c);

// Guess terminal colour support from $NO_COLOR, $COLORTERM and $TERM.
[[nodiscard]] ColorMode detectColorMode();

// Perceived brightness above one half: dark text belongs on this background.
[[nodiscard]] bool isLight(Rgb c);

// Find a terminal's reply to the background-colour query (OSC 11) in raw
// input bytes, e.g. "ESC ] 11 ; rgb:1e1e/1e1e/1e1e ESC \". Components may
// have 1-4 hex digits. Returns nullopt if there's no well-formed reply.
[[nodiscard]] std::optional<Rgb> parseBackgroundReply(std::string_view bytes);

// Background brightness from $COLORFGBG ("15;0" = light text on colour 0),
// set by some terminals (rxvt, Konsole). nullopt if unset or unclear.
[[nodiscard]] std::optional<bool> lightBackgroundFromColorFgBg(const char* value);

}  // namespace tetromino::tui
