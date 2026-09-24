#include "util/Logger.hpp"

#include <chrono>
#include <cstdlib>
#include <format>
#include <system_error>

namespace tetromino::util {

namespace {

std::string_view levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}

}  // namespace

Logger::Logger(const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    file_.open(path, std::ios::app);
    if (file_.is_open()) {
        log(LogLevel::Info, "---- log opened ----");
    }
}

void Logger::log(LogLevel level, std::string_view message) {
    if (!file_.is_open()) {
        return;
    }
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    file_ << std::format("{:%F %T} [{}] {}\n", now, levelName(level), message);
    file_.flush();
}

std::filesystem::path Logger::defaultPath() {
    if (const char* state = std::getenv("XDG_STATE_HOME"); state != nullptr && *state != '\0') {
        return std::filesystem::path{state} / "tetromino" / "tetromino.log";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path{home} / ".local" / "state" / "tetromino" / "tetromino.log";
    }
    return std::filesystem::path{"tetromino.log"};
}

}  // namespace tetromino::util
