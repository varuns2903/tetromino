#include <cstdio>
#include <exception>

#include "TestFramework.hpp"

int testing::runAll() {
    int failed = 0;
    for (const TestCase& test : registry()) {
        try {
            test.body();
            std::printf("  ok    %s\n", test.name);
        } catch (const Failure& f) {
            ++failed;
            std::printf("  FAIL  %s\n        %s\n", test.name, f.message.c_str());
        } catch (const std::exception& e) {
            ++failed;
            std::printf("  FAIL  %s\n        unexpected exception: %s\n", test.name, e.what());
        }
    }
    std::printf("%zu tests, %d failed\n", registry().size(), failed);
    return failed == 0 ? 0 : 1;
}

int main() { return testing::runAll(); }
