#include "core/Presets.hpp"

#include <algorithm>

namespace tetromino::core {

namespace {

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [&](char x, char y) { return lower(x) == lower(y); });
}

template <typename Array>
std::optional<std::size_t> findByName(const Array& items, std::string_view name) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (equalsIgnoreCase(items[i].name, name)) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::size_t> findBoardSize(std::string_view name) { return findByName(kBoardSizes, name); }

std::optional<std::size_t> findDifficulty(std::string_view name) { return findByName(kDifficulties, name); }

}  // namespace tetromino::core
