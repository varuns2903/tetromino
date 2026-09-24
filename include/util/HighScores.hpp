#pragma once

// Best scores, one per mode x board x difficulty, kept in a small text file
// ($XDG_STATE_HOME/tetromino/highscores by default).
//
// Records are keyed by the preset *names* ("Endless", "Classic", "Normal"),
// not indices, so reordering or adding presets never mixes records up.
// A missing or unreadable file just means "no scores yet"; failing to save is
// reported but never fatal - a lost high score shouldn't crash a game.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tetromino::util {

struct ScoreRecord {
    std::string mode;
    std::string board;
    std::string difficulty;
    std::uint64_t score = 0;
    int lines = 0;
    int level = 0;
    std::int64_t playTimeMs = 0;
    std::string date;  // YYYY-MM-DD

    friend bool operator==(const ScoreRecord&, const ScoreRecord&) = default;
};

class HighScores {
public:
    // No file: nothing is loaded or saved (tests, --no-scores style use).
    HighScores() = default;
    // Load from `path` (missing file = no records).
    explicit HighScores(std::filesystem::path path);

    [[nodiscard]] std::optional<ScoreRecord> best(std::string_view mode, std::string_view board,
                                                  std::string_view difficulty) const;

    // Record a finished game. Returns true if it beat (or set) the best for
    // its mode/board/difficulty, in which case the file is rewritten.
    // `saveError` receives a message if saving failed.
    bool submit(const ScoreRecord& record, std::string* saveError = nullptr);

    [[nodiscard]] const std::vector<ScoreRecord>& records() const { return records_; }

    [[nodiscard]] static std::filesystem::path defaultPath();

private:
    bool save(std::string* error) const;

    std::filesystem::path path_;
    std::vector<ScoreRecord> records_;
};

// Today's date as YYYY-MM-DD (local time).
[[nodiscard]] std::string todayIso();

}  // namespace tetromino::util
