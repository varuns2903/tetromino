#pragma once

// File logger.
//
// stdout belongs to the TUI: a single stray printf would corrupt the screen.
// Diagnostics therefore go to a file, by default
//   $XDG_STATE_HOME/tetromino/tetromino.log  (usually ~/.local/state/...)
// and only when debug logging is enabled (--debug or TETROMINO_DEBUG=1).
//
// A disabled Logger costs one branch per call. There's no global instance:
// whoever needs to log is handed a reference.

#include <filesystem>
#include <fstream>
#include <string_view>

namespace tetromino::util {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    // A logger that discards everything.
    Logger() = default;
    // A logger writing to `path` (parent directories are created). If the
    // file can't be opened the logger silently becomes disabled - failing to
    // log must never stop the game.
    explicit Logger(const std::filesystem::path& path);

    void log(LogLevel level, std::string_view message);
    void debug(std::string_view message) { log(LogLevel::Debug, message); }
    void info(std::string_view message) { log(LogLevel::Info, message); }
    void warning(std::string_view message) { log(LogLevel::Warning, message); }
    void error(std::string_view message) { log(LogLevel::Error, message); }

    [[nodiscard]] bool enabled() const { return file_.is_open(); }

    // The XDG-conforming default log location.
    [[nodiscard]] static std::filesystem::path defaultPath();

private:
    std::ofstream file_;
};

}  // namespace tetromino::util
