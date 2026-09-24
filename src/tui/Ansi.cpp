#include "tui/Ansi.hpp"

#include <array>
#include <charconv>

namespace tetromino::tui::ansi {

namespace {

void appendInt(std::string& out, int value) {
    std::array<char, 16> buf{};
    const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    if (ec == std::errc{}) {
        out.append(buf.data(), end);
    }
}

// SGR colour parameters. `base` is 30 for foreground, 40 for background.
void appendColorParams(std::string& out, Color color, int base, ColorMode mode) {
    if (color.isDefault()) {
        out += ';';
        appendInt(out, base + 9);  // 39 / 49 = default colour
        return;
    }
    const Rgb c = color.value();
    switch (mode) {
    case ColorMode::TrueColor:
        out += ';';
        appendInt(out, base + 8);  // 38 / 48
        out += ";2;";
        appendInt(out, c.r);
        out += ';';
        appendInt(out, c.g);
        out += ';';
        appendInt(out, c.b);
        return;
    case ColorMode::Ansi256:
        out += ';';
        appendInt(out, base + 8);
        out += ";5;";
        appendInt(out, toAnsi256(c));
        return;
    case ColorMode::Ansi16: {
        const int idx = toAnsi16(c);
        out += ';';
        // 30-37 normal, 90-97 bright (40-47 / 100-107 for background).
        appendInt(out, idx < 8 ? base + idx : base + 60 + (idx - 8));
        return;
    }
    case ColorMode::Monochrome:
        return;
    }
}

}  // namespace

void appendMoveTo(std::string& out, int row, int column) {
    out += "\x1b[";
    appendInt(out, row + 1);
    out += ';';
    appendInt(out, column + 1);
    out += 'H';
}

void appendStyle(std::string& out, const Style& style, ColorMode mode) {
    out += "\x1b[0";
    if (style.bold) {
        out += ";1";
    }
    if (style.dim) {
        out += ";2";
    }
    if (mode != ColorMode::Monochrome) {
        appendColorParams(out, style.fg, 30, mode);
        appendColorParams(out, style.bg, 40, mode);
    }
    out += 'm';
}

void appendTitle(std::string& out, std::string_view title) {
    out += "\x1b]0;";
    out += title;
    out += '\x07';
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += '?';
    }
}

}  // namespace tetromino::tui::ansi
