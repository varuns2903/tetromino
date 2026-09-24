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
#include <cstdlib>
#include <format>
#include <limits>
#include <vector>

#include "core/Bot.hpp"
#include "core/Game.hpp"
#include "tui/AutoRepeat.hpp"
#include "tui/Input.hpp"
#include "tui/Renderer.hpp"
#include "tui/Terminal.hpp"
#include "util/HighScores.hpp"
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

// Attract mode: after this long on the menu without a key press, a demo game
// played by the bot starts; it presses a key every kBotStep, and restarts
// kDemoRestart after its game ends.
constexpr std::chrono::seconds kAttractDelay{15};
constexpr std::chrono::milliseconds kBotStep{110};
constexpr std::chrono::seconds kDemoRestart{3};

// The high-score key for a setup: preset names, not indices.
struct SetupNames {
    std::string_view mode;
    std::string_view board;
    std::string_view difficulty;
};

SetupNames namesOf(const core::Setup& setup) {
    return {core::kGameTypes[setup.gameType].name, core::kBoardSizes[setup.boardSize].name,
            core::kDifficulties[setup.difficulty].name};
}

}  // namespace

tui::Theme Application::chooseTheme(const tui::TerminalCapabilities& caps, tui::ColorMode colorMode) {
    if (colorMode == tui::ColorMode::Monochrome) {
        return tui::Theme::monochrome();
    }
    switch (options_.theme) {
    case ThemeChoice::Dark: return tui::Theme::standard();
    case ThemeChoice::Light: return tui::Theme::light();
    case ThemeChoice::Auto: break;
    }
    if (const auto& bg = caps.background) {
        const bool light = tui::isLight(*bg);
        logger_.info(std::format("terminal background rgb({}, {}, {}): {} theme", bg->r, bg->g, bg->b,
                                 light ? "light" : "dark"));
        return light ? tui::Theme::light() : tui::Theme::standard();
    }
    if (const auto light = tui::lightBackgroundFromColorFgBg(std::getenv("COLORFGBG"))) {
        logger_.info(std::format("COLORFGBG: {} theme", *light ? "light" : "dark"));
        return *light ? tui::Theme::light() : tui::Theme::standard();
    }
    logger_.info("terminal background unknown: dark theme");
    return tui::Theme::standard();
}

namespace {

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

    util::HighScores scores{util::HighScores::defaultPath()};
    const auto bestFor = [&scores](const core::Setup& setup) -> std::optional<std::uint64_t> {
        const SetupNames n = namesOf(setup);
        if (const auto r = scores.best(n.mode, n.board, n.difficulty)) {
            return r->score;
        }
        return std::nullopt;
    };

    core::GameConfig config;
    config.startLevel = options_.startLevel;
    core::Game game{seed, config};
    game.selectSetup(options_.boardSize, options_.difficulty, options_.gameType);

    tui::Terminal terminal;
    tui::Input input{terminal};
    const tui::TerminalCapabilities caps = terminal.probe(std::chrono::milliseconds{300});
    tui::Renderer renderer{chooseTheme(caps, colorMode), colorMode};

    // Terminals that report key releases get held-key movement timed by the
    // game; others keep the OS key repeat.
    const bool keyReleases = caps.keyReleaseEvents && !options_.legacyKeys;
    if (keyReleases) {
        terminal.enableKeyReleaseEvents();
    }
    logger_.info(std::format("key release events: {}", keyReleases ? "on" : "off"));
    tui::AutoRepeat autoRepeat{{std::chrono::milliseconds{options_.dasMs}, std::chrono::milliseconds{options_.arrMs},
                                std::chrono::milliseconds{options_.arrMs}}};
    std::vector<core::Action> repeated;

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
    tui::HudInfo hud;
    tui::HudInfo drawnHud;

    std::optional<core::Game> demo;  // attract mode, when running
    core::Bot bot;
    auto lastKey = Clock::now();
    Clock::duration botTimer{};
    Clock::duration demoOverFor{};

    while (game.mode() != core::GameMode::Quit && !terminal.quitRequested()) {
        // What's on screen: the demo while it runs, otherwise the real game.
        const core::Game& shown = demo ? *demo : game;

        // 1. Render, only if something visible changed. This comes first so
        //    the result of the previous iteration is on screen before we
        //    (possibly) sleep.
        hud.best = bestFor(shown.state().setup);
        hud.attract = demo.has_value();
        if (game.mode() != core::GameMode::GameOver) {
            hud.newBest = false;
        }
        if (forceRender || shown.revision() != drawnRevision || hud != drawnHud) {
            renderer.render(shown.state(), hud);
            drawnHud = hud;
            if (!renderer.present(terminal)) {
                logger_.error("terminal write failed; exiting");
                break;
            }
            drawnRevision = shown.revision();
            forceRender = false;
        }

        // 2. Wait for input, a signal, or the next frame - whichever is first.
        //    When the game isn't running (menus, pause) there's no deadline;
        //    we sleep until a key or signal arrives.
        // (A demo keeps ticking even on its game-over screen, to restart.)
        const bool simulating = demo.has_value() || shown.needsUpdates();
        const auto now = Clock::now();
        const auto wait = simulating
                              ? std::chrono::ceil<std::chrono::milliseconds>(std::max(nextFrame - now, Clock::duration::zero()))
                              : kIdleWait;
        input.poll(wait, events);

        // 3. Input -> actions.
        for (const tui::KeyEvent& event : events) {
            const bool keyPress = event.phase == tui::KeyPhase::Press && event.key != tui::Key::FocusGained &&
                                  event.key != tui::Key::FocusLost;
            if (keyPress) {
                lastKey = Clock::now();
            }
            if (demo) {
                // Any key ends the demo (and does nothing else).
                if (keyPress) {
                    demo.reset();
                    forceRender = true;
                    logger_.info("demo ended by key press");
                }
                continue;
            }
            if (event.phase == tui::KeyPhase::Release) {
                // Only used to stop held-key repeats; releases never act.
                if (const auto action = tui::actionFor(event, core::GameMode::Playing)) {
                    autoRepeat.release(*action);
                }
                continue;
            }
            if (event.phase == tui::KeyPhase::Repeat && game.mode() == core::GameMode::Playing) {
                continue;  // the game times held keys itself (AutoRepeat)
            }
            if (event.key == tui::Key::Suspend || event.key == tui::Key::FocusLost) {
                autoRepeat.releaseAll();
                if (game.mode() == core::GameMode::Playing || game.mode() == core::GameMode::Countdown) {
                    game.apply(core::Action::Pause);
                }
                if (event.key == tui::Key::Suspend) {
                    logger_.info("suspend");
                    renderer.render(game.state(), hud);
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
                const bool wasPlaying = game.mode() == core::GameMode::Playing;
                game.apply(*action);
                if (keyReleases && wasPlaying) {
                    autoRepeat.press(*action);  // ignores non-movement actions
                }
            }
        }
        if (game.mode() != core::GameMode::Playing) {
            autoRepeat.releaseAll();  // paused, menu, game over: nothing stays held
        }

        // Attract mode: start a demo once the menu has sat idle long enough,
        // with the setup currently selected in the menu.
        if (!demo && options_.demo && game.mode() == core::GameMode::StartScreen &&
            Clock::now() - lastKey >= kAttractDelay) {
            demo.emplace(util::Random::entropySeed(), config);
            const core::Setup& s = game.state().setup;
            demo->selectSetup(s.boardSize, s.difficulty, s.gameType);
            demo->apply(core::Action::Start);
            bot.reset();
            botTimer = Clock::duration::zero();
            demoOverFor = Clock::duration::zero();
            forceRender = true;
            logger_.info("demo started");
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
        if (keyReleases && game.mode() == core::GameMode::Playing) {
            repeated.clear();
            autoRepeat.update(std::chrono::duration_cast<core::Duration>(elapsed), repeated);
            for (const core::Action action : repeated) {
                game.apply(action);
            }
        }
        game.update(std::chrono::duration_cast<core::Duration>(elapsed));

        if (demo) {
            // The bot presses one key per step, like a (fast) human.
            botTimer += elapsed;
            while (botTimer >= kBotStep) {
                botTimer -= kBotStep;
                if (const auto action = bot.nextAction(*demo)) {
                    demo->apply(*action);
                }
            }
            demo->update(std::chrono::duration_cast<core::Duration>(elapsed));
            if (demo->mode() == core::GameMode::GameOver) {
                demoOverFor += elapsed;
                if (demoOverFor >= kDemoRestart) {
                    demo->apply(core::Action::Restart);
                    bot.reset();
                    demoOverFor = Clock::duration::zero();
                }
            }
        }

        if (game.mode() != lastMode) {
            logger_.info(std::format("mode {} -> {}", modeName(lastMode), modeName(game.mode())));
            if (game.mode() == core::GameMode::GameOver) {
                const core::Stats& s = game.state().stats;
                logger_.info(std::format("game over: score={} lines={} level={}", s.score, s.lines, s.level));
                const SetupNames n = namesOf(game.state().setup);
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(s.playTime).count();
                std::string error;
                hud.newBest = scores.submit({std::string{n.mode}, std::string{n.board}, std::string{n.difficulty},
                                             s.score, s.lines, s.level, ms, util::todayIso()},
                                            &error);
                if (!error.empty()) {
                    logger_.warning("high score not saved: " + error);
                }
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
