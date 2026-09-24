#include "util/HighScores.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

namespace tetromino::util {

namespace {

// File format: a header line, then one record per line, fields separated by
// tabs (preset names contain spaces and hyphens, never tabs):
//
//   # tetromino high scores v1
//   Endless<TAB>Classic<TAB>Normal<TAB>12450<TAB>37<TAB>4<TAB>183200<TAB>2026-09-24
constexpr std::string_view kHeader = "# tetromino high scores v1";

std::optional<ScoreRecord> parseLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in{line};
    while (std::getline(in, field, '\t')) {
        fields.push_back(field);
    }
    if (fields.size() != 8 || fields[0].empty() || fields[1].empty() || fields[2].empty()) {
        return std::nullopt;
    }
    ScoreRecord r;
    r.mode = fields[0];
    r.board = fields[1];
    r.difficulty = fields[2];
    try {
        std::size_t used = 0;
        r.score = std::stoull(fields[3], &used);
        if (used != fields[3].size()) return std::nullopt;
        r.lines = std::stoi(fields[4]);
        r.level = std::stoi(fields[5]);
        r.playTimeMs = std::stoll(fields[6]);
    } catch (const std::exception&) {
        return std::nullopt;  // corrupt line: skip it, keep the rest
    }
    r.date = fields[7];
    return r;
}

bool sameKey(const ScoreRecord& r, std::string_view mode, std::string_view board, std::string_view difficulty) {
    return r.mode == mode && r.board == board && r.difficulty == difficulty;
}

}  // namespace

HighScores::HighScores(std::filesystem::path path) : path_(std::move(path)) {
    std::ifstream in{path_};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (auto r = parseLine(line)) {
            // Keep only the best per key, even if the file has duplicates.
            auto existing = std::find_if(records_.begin(), records_.end(),
                                         [&](const ScoreRecord& e) { return sameKey(e, r->mode, r->board, r->difficulty); });
            if (existing == records_.end()) {
                records_.push_back(std::move(*r));
            } else if (r->score > existing->score) {
                *existing = std::move(*r);
            }
        }
    }
}

std::optional<ScoreRecord> HighScores::best(std::string_view mode, std::string_view board,
                                            std::string_view difficulty) const {
    for (const ScoreRecord& r : records_) {
        if (sameKey(r, mode, board, difficulty)) {
            return r;
        }
    }
    return std::nullopt;
}

bool HighScores::submit(const ScoreRecord& record, std::string* saveError) {
    auto existing = std::find_if(records_.begin(), records_.end(), [&](const ScoreRecord& e) {
        return sameKey(e, record.mode, record.board, record.difficulty);
    });
    if (existing != records_.end()) {
        if (record.score <= existing->score) {
            return false;
        }
        *existing = record;
    } else {
        if (record.score == 0) {
            return false;  // nothing worth remembering
        }
        records_.push_back(record);
    }
    save(saveError);
    return true;
}

bool HighScores::save(std::string* error) const {
    if (path_.empty()) {
        return true;
    }
    std::error_code ec;
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
    }
    // Write a temporary file and rename it over the old one, so a crash or
    // full disk mid-write can't leave a truncated score file behind.
    std::filesystem::path tmp = path_;
    tmp += ".tmp";
    {
        std::ofstream out{tmp, std::ios::trunc};
        out << kHeader << '\n';
        for (const ScoreRecord& r : records_) {
            out << std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", r.mode, r.board, r.difficulty, r.score, r.lines,
                               r.level, r.playTimeMs, r.date);
        }
        out.flush();
        if (!out) {
            if (error != nullptr) *error = "could not write " + tmp.string();
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        if (error != nullptr) *error = "could not replace " + path_.string() + ": " + ec.message();
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

std::filesystem::path HighScores::defaultPath() {
    if (const char* state = std::getenv("XDG_STATE_HOME"); state != nullptr && *state != '\0') {
        return std::filesystem::path{state} / "tetromino" / "highscores";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path{home} / ".local" / "state" / "tetromino" / "highscores";
    }
    return {};  // nowhere sensible: don't persist
}

std::string todayIso() {
    const auto now = std::chrono::system_clock::now();
    try {
        return std::format("{:%F}", std::chrono::zoned_time{std::chrono::current_zone(), now});
    } catch (const std::exception&) {
        // No timezone database (minimal containers): UTC is close enough.
        return std::format("{:%F}", std::chrono::floor<std::chrono::days>(now));
    }
}

}  // namespace tetromino::util
