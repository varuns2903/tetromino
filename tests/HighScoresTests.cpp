#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "TestFramework.hpp"
#include "util/HighScores.hpp"

using tetromino::util::HighScores;
using tetromino::util::ScoreRecord;

namespace {

// A fresh temporary directory per test, removed afterwards.
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        std::string pattern = (std::filesystem::temp_directory_path() / "tetromino-test-XXXXXX").string();
        path = ::mkdtemp(pattern.data());
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

ScoreRecord record(std::uint64_t score, std::string mode = "Endless", std::string board = "Classic",
                   std::string difficulty = "Normal") {
    return ScoreRecord{std::move(mode), std::move(board), std::move(difficulty), score, 12, 2, 61000, "2026-09-24"};
}

}  // namespace

TEST(missing_file_means_no_scores) {
    TempDir dir;
    const HighScores scores{dir.path / "nope" / "highscores"};
    CHECK(scores.records().empty());
    CHECK(!scores.best("Endless", "Classic", "Normal").has_value());
}

TEST(first_score_is_a_new_best_and_is_saved) {
    TempDir dir;
    const auto file = dir.path / "sub" / "highscores";  // parent is created
    {
        HighScores scores{file};
        CHECK(scores.submit(record(1200)));
    }
    const HighScores reloaded{file};
    const auto best = reloaded.best("Endless", "Classic", "Normal");
    CHECK(best.has_value());
    CHECK(*best == record(1200));
}

TEST(only_higher_scores_replace_the_best) {
    TempDir dir;
    HighScores scores{dir.path / "highscores"};
    CHECK(scores.submit(record(1200)));
    CHECK(!scores.submit(record(900)));
    CHECK(!scores.submit(record(1200)));  // a tie isn't a new best
    CHECK(scores.submit(record(5000)));
    CHECK_EQ(scores.best("Endless", "Classic", "Normal")->score, 5000u);
    CHECK_EQ(scores.records().size(), 1u);
}

TEST(each_mode_board_and_difficulty_has_its_own_best) {
    TempDir dir;
    HighScores scores{dir.path / "highscores"};
    CHECK(scores.submit(record(100, "Endless", "Classic", "Normal")));
    CHECK(scores.submit(record(50, "2-Minute", "Classic", "Normal")));
    CHECK(scores.submit(record(50, "Endless", "Small", "Normal")));
    CHECK(scores.submit(record(50, "Endless", "Classic", "Expert")));
    CHECK_EQ(scores.records().size(), 4u);
    CHECK_EQ(scores.best("2-Minute", "Classic", "Normal")->score, 50u);
}

TEST(zero_scores_are_not_recorded) {
    TempDir dir;
    HighScores scores{dir.path / "highscores"};
    CHECK(!scores.submit(record(0)));
    CHECK(scores.records().empty());
    CHECK(!std::filesystem::exists(dir.path / "highscores"));
}

TEST(corrupt_lines_are_skipped) {
    TempDir dir;
    const auto file = dir.path / "highscores";
    {
        std::ofstream out{file};
        out << "# tetromino high scores v1\n";
        out << "garbage\n";
        out << "Endless\tClassic\tNormal\tnot-a-number\t1\t1\t1\t2026-01-01\n";
        out << "Endless\tClassic\tNormal\t777\t3\t1\t9000\t2026-01-01\n";
        out << "Endless\tClassic\tNormal\t500\t3\t1\t9000\t2026-01-01\n";  // duplicate, lower
    }
    const HighScores scores{file};
    CHECK_EQ(scores.records().size(), 1u);
    CHECK_EQ(scores.best("Endless", "Classic", "Normal")->score, 777u);
}

TEST(in_memory_scores_never_touch_disk) {
    HighScores scores;
    CHECK(scores.submit(record(10)));
    CHECK(scores.best("Endless", "Classic", "Normal").has_value());
}

TEST(today_is_an_iso_date) {
    const std::string d = tetromino::util::todayIso();
    CHECK_EQ(d.size(), 10u);
    CHECK(d[4] == '-' && d[7] == '-');
}
