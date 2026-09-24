#pragma once

// The application: wires Terminal -> Input -> Game -> Renderer together and
// runs the main loop. This is the only place that knows about both the engine
// and the terminal front end.

#include "app/Options.hpp"
#include "core/GameState.hpp"

namespace tetromino::util {
class Logger;
}

namespace tetromino::app {

struct RunSummary {
    bool played = false;  // at least one game was started
    core::Stats lastGame{};
};

class Application {
public:
    Application(const Options& options, util::Logger& logger);

    // Runs until the player quits or the terminal goes away. Throws on
    // terminal setup failure (terminal state is restored before the throw
    // propagates, courtesy of RAII).
    RunSummary run();

private:
    const Options& options_;
    util::Logger& logger_;
};

}  // namespace tetromino::app
