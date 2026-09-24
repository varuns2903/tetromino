#include "app/Application.hpp"

// ---------------------------------------------------------------------------
// The game loop
//
//     ┌──────────────────────────────────────────────────────────┐
//     │ render: only if the game state changed                   │
//     │ wait: poll() stdin until input arrives or the next frame │
//     │ input: decode keys -> Actions -> game.apply()            │
//     │ events: resize / suspend / focus                         │
//     │ update: game.update(elapsed wall-clock time)             │
//     └──────────────────────────────────────────────────────────┘
//
// Input, update and render are separate steps so each can run at its own
// pace: keys are applied the moment they arrive (poll() wakes us up
// immediately), the simulation advances by however much real time passed,
// and rendering happens at most once per loop and only when something
// changed. Nothing ever sleep()s; the only wait is poll() with a timeout, so
// an idle game (paused, menu) uses ~0% CPU.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <format>
#include <limits>
#include <vector>

#include "core/Game.hpp"
#include "tui/Input.hpp"
#include "tui/Renderer.hpp"
#include "tui/Terminal.hpp"
#include "util/Logger.hpp"
#include "util/Random.hpp"

namespace tetromino::app {

namespace {

using Clock = std::chrono::steady_clock;

// While nothing is animating we only need to wake up for input or signals.
constexpr std::chrono::milliseconds kIdleWait{1000};

// Upper bound on one simulation step. If the process was stalled (debugger,
// heavily loaded machine) we'd rather the game briefly slow down than have a
// piece teleport and lock in one frame.
constexpr std::chrono::milliseconds kMaxStep{100};

const char* modeName(core::GameMode mode) {
    switch (mode) {
    case core::GameMode::StartScreen: return "start";
    case core::GameMode::Playing: return "playing";
    case core::GameMode::Paused: return "paused";
    case core::GameMode::Countdown: return "countdown";
    case core::GameMode::GameOver: return "game-over";
    case core::GameMode::Quit: return "quit";
    }
    return "?";
}

}  // namespace

Application::Application(const Options& options, util::Logger& logger)
    : options_(options), logger_(logger) {}

RunSummary Application::run() {
    const std::uint64_t seed = options_.seed.value_or(util::Random::entropySeed());
    const tui::ColorMode colorMode = options_.colorMode.value_or(tui::detectColorMode());
    logger_.info(std::format("starting: seed={} level={} colorMode={} fps={}", seed, options_.startLevel,
                             static_cast<int>(colorMode), options_.fps));

    core::GameConfig config;
    config.startLevel = options_.startLevel;
    core::Game game{seed, config};
    game.selectSetup(options_.boardSize, options_.difficulty, options_.gameType);

    tui::Terminal terminal;
    tui::Input input{terminal};
    tui::Renderer renderer{
        colorMode == tui::ColorMode::Monochrome ? tui::Theme::monochrome() : tui::Theme::standard(), colorMode};

    const auto resize = [&] {
        const tui::TerminalSize size = terminal.size();
        renderer.resize(size);
        logger_.debug(std::format("resize {}x{} fits={}", size.columns, size.rows, renderer.layout().fits));
        // The game can't be seen on the "too small" screen; don't let it run.
        if ((game.mode() == core::GameMode::Playing || game.mode() == core::GameMode::Countdown) &&
            !renderer.layoutFor(game.state()).fits) {
            game.apply(core::Action::Pause);
        }
    };
    terminal.setTitle("Tetromino");
    resize();

    RunSummary summary;
    const auto framePeriod = std::chrono::duration_cast<Clock::duration>(std::chrono::seconds{1}) / options_.fps;
    std::vector<tui::KeyEvent> events;
    events.reserve(64);

    auto previous = Clock::now();
    auto nextFrame = previous;
    std::uint64_t drawnRevision = std::numeric_limits<std::uint64_t>::max();
    bool forceRender = true;
    core::GameMode lastMode = game.mode();

    while (game.mode() != core::GameMode::Quit && !terminal.quitRequested()) {
        // 1. Render, only if something visible changed. This comes first so
        //    the result of the previous iteration is on screen before we
        //    (possibly) sleep.
        if (forceRender || game.revision() != drawnRevision) {
            renderer.render(game.state());
            if (!renderer.present(terminal)) {
                logger_.error("terminal write failed; exiting");
                break;
            }
            drawnRevision = game.revision();
            forceRender = false;
        }

        // 2. Wait for input, a signal, or the next frame - whichever is first.
        //    When the game isn't running (menus, pause) there's no deadline;
        //    we sleep until a key or signal arrives.
        const bool simulating = game.needsUpdates();
        const auto now = Clock::now();
        const auto wait = simulating
                              ? std::chrono::ceil<std::chrono::milliseconds>(std::max(nextFrame - now, Clock::duration::zero()))
                              : kIdleWait;
        input.poll(wait, events);

        // 3. Input -> actions.
        for (const tui::KeyEvent& event : events) {
            if (event.key == tui::Key::Suspend || event.key == tui::Key::FocusLost) {
                if (game.mode() == core::GameMode::Playing || game.mode() == core::GameMode::Countdown) {
                    game.apply(core::Action::Pause);
                }
                if (event.key == tui::Key::Suspend) {
                    logger_.info("suspend");
                    renderer.render(game.state());
                    renderer.present(terminal);
                    terminal.suspend();  // returns after `fg`
                    logger_.info("resume");
                }
                continue;
            }
            if (const auto action = tui::actionFor(event, game.mode())) {
                if (*action == core::Action::Start || *action == core::Action::Restart) {
                    summary.played = true;
                }
                game.apply(*action);
            }
        }

        // 4. Asynchronous terminal events.
        if (terminal.consumeResize()) {
            resize();
            forceRender = true;
        }

        // 5. Advance the simulation by real elapsed time. Time spent idle
        //    (paused, in a menu) doesn't count.
        const auto current = Clock::now();
        const auto elapsed =
            simulating ? std::min<Clock::duration>(current - previous, kMaxStep) : Clock::duration::zero();
        previous = current;
        game.update(std::chrono::duration_cast<core::Duration>(elapsed));

        if (game.mode() != lastMode) {
            logger_.info(std::format("mode {} -> {}", modeName(lastMode), modeName(game.mode())));
            if (game.mode() == core::GameMode::GameOver) {
                const core::Stats& s = game.state().stats;
                logger_.info(std::format("game over: score={} lines={} level={}", s.score, s.lines, s.level));
            }
            lastMode = game.mode();
        }

        // Frame pacing: advance the deadline; if we fell far behind (e.g.
        // after a suspend), resynchronise instead of trying to catch up.
        if (current >= nextFrame) {
            nextFrame += framePeriod;
            if (nextFrame < current) {
                nextFrame = current + framePeriod;
            }
        }
    }

    if (terminal.quitRequested()) {
        logger_.info("quit requested by signal");
    }
    summary.lastGame = game.state().stats;
    return summary;
}

}  // namespace tetromino::app
