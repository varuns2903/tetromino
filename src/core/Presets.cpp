#include "core/Presets.hpp"


namespace tetromino::core {

namespace {

// Case-insensitive, and hyphens are ignored: "2minute" matches "2-Minute".
bool sameName(std::string_view a, std::string_view b) {
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    std::size_t i = 0;
    std::size_t j = 0;
    for (;;) {
        while (i < a.size() && a[i] == '-') ++i;
        while (j < b.size() && b[j] == '-') ++j;
        if (i == a.size() || j == b.size()) {
            return i == a.size() && j == b.size();
        }
        if (lower(a[i++]) != lower(b[j++])) {
            return false;
        }
    }
}

template <typename Array>
std::optional<std::size_t> findByName(const Array& items, std::string_view name) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (sameName(items[i].name, name)) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::size_t> findGameType(std::string_view name) { return findByName(kGameTypes, name); }

std::optional<std::size_t> findBoardSize(std::string_view name) { return findByName(kBoardSizes, name); }

std::optional<std::size_t> findDifficulty(std::string_view name) { return findByName(kDifficulties, name); }

}  // namespace tetromino::core
