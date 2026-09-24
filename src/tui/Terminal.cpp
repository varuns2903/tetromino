#include "tui/Terminal.hpp"

// ---------------------------------------------------------------------------
// Why raw mode?
//
// By default a tty is in *canonical* ("cooked") mode: the kernel's line
// discipline collects keystrokes into a line buffer, handles backspace itself,
// echoes every key back to the screen, and only hands the line to the program
// when Enter is pressed. It also turns Ctrl-C into SIGINT and Ctrl-Z into
// SIGTSTP. That's perfect for a shell and useless for a game: we need every
// key the instant it's pressed, and we don't want it printed.
//
// termios lets us switch those behaviours off individually:
//
//   ICANON  off -> no line buffering; read() returns bytes as they arrive
//   ECHO    off -> keys are not printed back
//   ISIG    off -> Ctrl-C / Ctrl-Z arrive as bytes 0x03 / 0x1a instead of
//                  signals, so *we* decide what they do (quit / suspend)
//   IEXTEN  off -> no Ctrl-V literal-next processing
//   IXON    off -> Ctrl-S / Ctrl-Q don't freeze the output
//   ICRNL   off -> Enter arrives as '\r' instead of being translated to '\n'
//   OPOST   off -> the kernel doesn't rewrite '\n' to "\r\n" on output
//
//   VMIN=0, VTIME=0 -> read() never blocks: it returns whatever is buffered,
//                      possibly nothing.
//
// Note: we deliberately do *not* set O_NONBLOCK on stdin. On a terminal,
// stdin and stdout usually share one "open file description", and O_NONBLOCK
// lives on that description - setting it on stdin silently makes writes to
// stdout non-blocking too, so large frames fail with EAGAIN. VMIN/VTIME gives
// us non-blocking reads without that side effect, and poll() gives us a way
// to sleep until input arrives.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "tui/Ansi.hpp"

namespace tetromino::tui {

namespace {

// ---------------------------------------------------------------------------
// State shared with signal handlers.
//
// Signal handlers can't reach a Terminal object, so the little they need is
// kept here. This is the one place in the project with file-level mutable
// state, and it's unavoidable: POSIX signal handlers are plain functions.
// Everything touched from a handler is either a lock-free atomic or written
// once before the handlers are installed and only read afterwards.
// ---------------------------------------------------------------------------
static_assert(std::atomic<bool>::is_always_lock_free, "signal handlers require lock-free atomics");

std::atomic<bool> gInstanceAlive{false};
std::atomic<bool> gResizePending{false};
std::atomic<bool> gQuitRequested{false};
// True while the tty is in our modified state and needs restoring.
std::atomic<bool> gModified{false};

termios gOriginalTermios{};
termios gRawTermios{};

// The sequence that puts the screen into game mode. Kept as a constant so the
// SIGCONT path can re-apply it with a single async-signal-safe write().
constexpr std::string_view kEnterAll =
    "\x1b[?1049h"  // alternate screen
    "\x1b[?25l"    // hide cursor
    "\x1b[?7l"     // no auto-wrap
    "\x1b[?1004h"; // focus events

void writeAllRaw(std::string_view bytes) {
    // Async-signal-safe: only write(2), no allocation, errors ignored.
    while (!bytes.empty()) {
        const ssize_t n = ::write(STDOUT_FILENO, bytes.data(), bytes.size());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        bytes.remove_prefix(static_cast<std::size_t>(n));
    }
}

void emergencyRestore() {
    if (gModified.load()) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOriginalTermios);
        writeAllRaw(ansi::kRestoreAll);
    }
}

void reapplyGameMode() {
    if (gModified.load()) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &gRawTermios);
        writeAllRaw(kEnterAll);
        gResizePending.store(true);
    }
}

void setHandler(int sig, void (*handler)(int)) {
    struct sigaction sa {};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    // No SA_RESTART: we *want* poll() to return EINTR on SIGWINCH so the
    // resize is handled immediately rather than at the next timeout.
    sa.sa_flags = 0;
    ::sigaction(sig, &sa, nullptr);
}

void resetAndReraise(int sig) {
    setHandler(sig, SIG_DFL);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    // The signal is blocked while its own handler runs; unblock it so the
    // re-raised signal takes effect now, not after we return.
    ::sigprocmask(SIG_UNBLOCK, &set, nullptr);
    ::raise(sig);
}

void onResize(int /*sig*/) { gResizePending.store(true); }

void onQuit(int /*sig*/) { gQuitRequested.store(true); }

void onStop(int sig);

void onContinue(int /*sig*/) { reapplyGameMode(); }

void onStop(int sig) {
    // Put the terminal back for the shell, then actually stop.
    emergencyRestore();
    resetAndReraise(sig);
    // Execution resumes here after SIGCONT (`fg`).
    setHandler(sig, onStop);
    reapplyGameMode();
}

void onFatal(int sig) {
    // Crashing is bad; crashing *and* leaving the user's terminal in raw mode
    // on the alternate screen is worse. Restore, then die with the original
    // signal so the exit status / core dump are unchanged.
    emergencyRestore();
    resetAndReraise(sig);
}

constexpr int kQuitSignals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
constexpr int kFatalSignals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT};

void installHandlers() {
    setHandler(SIGWINCH, onResize);
    setHandler(SIGTSTP, onStop);
    setHandler(SIGCONT, onContinue);
    for (const int sig : kQuitSignals) {
        setHandler(sig, onQuit);
    }
    for (const int sig : kFatalSignals) {
        setHandler(sig, onFatal);
    }
}

void uninstallHandlers() {
    setHandler(SIGWINCH, SIG_DFL);
    setHandler(SIGTSTP, SIG_DFL);
    setHandler(SIGCONT, SIG_DFL);
    for (const int sig : kQuitSignals) {
        setHandler(sig, SIG_DFL);
    }
    for (const int sig : kFatalSignals) {
        setHandler(sig, SIG_DFL);
    }
}

int envInt(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return 0;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || parsed <= 0 || parsed > 10000) {
        return 0;
    }
    return static_cast<int>(parsed);
}

}  // namespace

Terminal::Terminal() {
    if (::isatty(STDIN_FILENO) == 0 || ::isatty(STDOUT_FILENO) == 0) {
        throw std::runtime_error("stdin and stdout must be a terminal");
    }
    if (gInstanceAlive.exchange(true)) {
        throw std::logic_error("only one Terminal instance may exist");
    }
    if (::tcgetattr(STDIN_FILENO, &gOriginalTermios) != 0) {
        gInstanceAlive.store(false);
        throw std::runtime_error("tcgetattr failed: cannot read terminal settings");
    }
    gResizePending.store(false);
    gQuitRequested.store(false);
    installHandlers();

    try {
        enterRawMode();
        enterAlternateScreen();
        hideCursor();
        write(ansi::kDisableAutoWrap);
        write(ansi::kEnableFocusEvents);
    } catch (...) {
        restore();
        uninstallHandlers();
        gInstanceAlive.store(false);
        throw;
    }
}

Terminal::~Terminal() {
    restore();
    uninstallHandlers();
    gInstanceAlive.store(false);
}

void Terminal::enterRawMode() {
    if (rawMode_) {
        return;
    }
    termios raw = gOriginalTermios;
    raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    raw.c_cflag |= static_cast<tcflag_t>(CS8);
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    // TCSAFLUSH: apply after pending output is written, and discard any
    // typed-ahead input so stray keys from before startup aren't replayed.
    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        throw std::runtime_error("tcsetattr failed: cannot enter raw mode");
    }
    gRawTermios = raw;
    rawMode_ = true;
    gModified.store(true);
}

void Terminal::enterAlternateScreen() {
    if (!altScreen_) {
        write(ansi::kEnterAltScreen);
        write(ansi::kClearScreen);
        altScreen_ = true;
    }
}

void Terminal::leaveAlternateScreen() {
    if (altScreen_) {
        write(ansi::kLeaveAltScreen);
        altScreen_ = false;
    }
}

void Terminal::hideCursor() {
    if (!cursorHidden_) {
        write(ansi::kHideCursor);
        cursorHidden_ = true;
    }
}

void Terminal::showCursor() {
    if (cursorHidden_) {
        write(ansi::kShowCursor);
        cursorHidden_ = false;
    }
}

void Terminal::setTitle(std::string_view title) {
    std::string out;
    if (!titlePushed_) {
        out += ansi::kPushTitle;
        titlePushed_ = true;
    }
    ansi::appendTitle(out, title);
    write(out);
}

void Terminal::restore() {
    if (titlePushed_) {
        write(ansi::kPopTitle);
        titlePushed_ = false;
    }
    if (!rawMode_ && !altScreen_ && !cursorHidden_) {
        return;
    }
    // Output first (while we still know what we changed), then termios.
    write(ansi::kRestoreAll);
    altScreen_ = false;
    cursorHidden_ = false;
    if (rawMode_) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOriginalTermios);
        rawMode_ = false;
    }
    gModified.store(false);
}

void Terminal::suspend() {
    // The SIGTSTP handler does the restore / stop / re-enter dance, so a
    // keyboard Ctrl-Z and an external `kill -TSTP` behave identically.
    ::raise(SIGTSTP);
}

TerminalSize Terminal::size() const {
    winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {ws.ws_col, ws.ws_row};
    }
    const int cols = envInt("COLUMNS");
    const int rows = envInt("LINES");
    if (cols > 0 && rows > 0) {
        return {cols, rows};
    }
    return {80, 24};
}

bool Terminal::write(std::string_view bytes) {
    while (!bytes.empty()) {
        const ssize_t n = ::write(STDOUT_FILENO, bytes.data(), bytes.size());
        if (n >= 0) {
            bytes.remove_prefix(static_cast<std::size_t>(n));
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            pollfd pfd{STDOUT_FILENO, POLLOUT, 0};
            ::poll(&pfd, 1, 100);
            continue;
        }
        // EIO / EPIPE: the terminal is gone. Nothing sensible left to do.
        gQuitRequested.store(true);
        return false;
    }
    return true;
}

bool Terminal::waitForInput(std::chrono::milliseconds timeout) {
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    const int ms = static_cast<int>(std::max<std::chrono::milliseconds::rep>(0, timeout.count()));
    const int rc = ::poll(&pfd, 1, ms);
    if (rc < 0) {
        // EINTR: a signal (usually SIGWINCH) woke us. The caller will check
        // consumeResize() / quitRequested().
        return false;
    }
    if (rc > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0 && (pfd.revents & POLLIN) == 0) {
        // Terminal hung up. Without this we'd spin: poll() keeps returning
        // immediately with POLLHUP.
        gQuitRequested.store(true);
        return false;
    }
    return rc > 0;
}

std::size_t Terminal::readAvailable(std::span<char> buffer) {
    const ssize_t n = ::read(STDIN_FILENO, buffer.data(), buffer.size());
    return n > 0 ? static_cast<std::size_t>(n) : 0;
}

bool Terminal::consumeResize() { return gResizePending.exchange(false); }

bool Terminal::quitRequested() const { return gQuitRequested.load(); }

}  // namespace tetromino::tui
