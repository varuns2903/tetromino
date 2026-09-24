#pragma once

// A deliberately tiny unit-test harness (~60 lines) so the project has no
// external dependencies. Usage:
//
//   TEST(board_starts_empty) {
//       Board b;
//       CHECK(b.isEmpty());
//       CHECK_EQ(b.fullRows().size(), 0u);
//   }
//
// Each test executable links TestMain.cpp, which runs every registered test
// and returns non-zero if any failed (so CTest sees the failure).

#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

struct Failure {
    std::string message;
};

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected, const char* actualExpr, const char* expectedExpr,
                const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream os;
        os << file << ':' << line << ": CHECK_EQ(" << actualExpr << ", " << expectedExpr << ") failed";
        if constexpr (requires(std::ostream& o) { o << actual << expected; }) {
            os << "\n      actual:   " << actual << "\n      expected: " << expected;
        }
        throw Failure{os.str()};
    }
}

int runAll();

}  // namespace testing

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static const ::testing::Registrar registrar_##name{#name, &test_##name}; \
    static void test_##name()

#define CHECK(expr)                                                                                  \
    do {                                                                                             \
        if (!(expr)) {                                                                               \
            throw ::testing::Failure{std::string{__FILE__} + ":" + std::to_string(__LINE__) +        \
                                     ": CHECK(" #expr ") failed"};                                   \
        }                                                                                            \
    } while (false)

#define CHECK_EQ(actual, expected) \
    ::testing::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)
