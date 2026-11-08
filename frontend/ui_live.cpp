// [LOCAL-FIX friday / exp-live-ui] arby-strata live console UI — terminal implementation.
#include "ui_live.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

namespace live_ui {

namespace {
constexpr const char * kUpOne   = "\x1b[1A";
constexpr const char * kClear   = "\x1b[2K";
constexpr const char * kHideCur = "\x1b[?25l";
constexpr const char * kShowCur = "\x1b[?25h";

bool stdout_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}
}   // namespace

bool ConsoleUI::init(Mode mode, const std::string & log_file, std::string & err) {
    std::lock_guard<std::mutex> lock(mu_);
    mode_ = mode;

    if (mode_ == Mode::Live) {
        // ARBY_UI_FORCE=live lets a test drive the live path without a console attached.
        const char * force = std::getenv("ARBY_UI_FORCE");
        const bool   forced  = force && std::string(force).rfind("live", 0) == 0;
        const bool   no_con  = force && std::string(force) == "live-nocon";   // test hook
#ifdef _WIN32
        // Our own handle on the console, taken BEFORE stdout/stderr are redirected below: every
        // channel in this process then lands in the log file and only the block reaches the screen.
        HANDLE con = no_con ? INVALID_HANDLE_VALUE
                            : CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (con == INVALID_HANDLE_VALUE && !forced) {
            mode_ = Mode::Plain;
        } else if (con != INVALID_HANDLE_VALUE) {
            DWORD m = 0;
            if (GetConsoleMode(con, &m)) {
                original_mode_ = m;
                SetConsoleMode(con, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
            hout_ = (void *) con;
        } else {
            hout_ = nullptr;   // forced without a console: the block goes to stdout, escapes and all
        }
#else
        if (!stdout_is_tty() && !forced) mode_ = Mode::Plain;
#endif

        // The redirect that makes the promise true: llama's common logger writes INFO to stdout and
        // WARN/ERR to stderr by design (common/log.h), and common_log_set_file only ADDS a copy. With
        // both streams on the log file, nothing anywhere in the process can reach the console.
        if (mode_ == Mode::Live && !log_file.empty()) {
            log_path_ = log_file;
            if (!std::freopen(log_file.c_str(), "a", stdout) || !std::freopen(log_file.c_str(), "a", stderr)) {
                err = "cannot redirect stdout/stderr to the log file: " + log_file;
                return false;
            }
            quiet_ = true;
        }
    }

    if (!log_file.empty() && !log_fp_) {
        log_path_ = log_file;
        log_fp_   = std::fopen(log_file.c_str(), "ab");
        if (!log_fp_) {
            err = "cannot open the log file: " + log_file;
            return false;
        }
    }

    quiet_ = quiet_ || !log_file.empty();   // a log file means the console is for the UI
    if (mode_ == Mode::Live) {
        write_raw(std::string(kHideCur));
        cursor_hidden_ = true;
    }
    return true;
}

void ConsoleUI::shutdown() {
    std::lock_guard<std::mutex> lock(mu_);
    if (mode_ == Mode::Off && block_lines_ == 0) return;
    clear_block_locked();
    if (cursor_hidden_) {
        write_raw(std::string(kShowCur));
        cursor_hidden_ = false;
    }
#ifdef _WIN32
    if (hout_) {
        SetConsoleMode((HANDLE) hout_, (DWORD) original_mode_);
        hout_ = nullptr;
    }
#endif
    if (log_fp_) { std::fclose(log_fp_); log_fp_ = nullptr; }
    mode_ = Mode::Off;
}

int ConsoleUI::term_width() const {
#ifdef _WIN32
    HANDLE h = hout_ ? (HANDLE) hout_ : GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &info))
        return (int) (info.srWindow.Right - info.srWindow.Left + 1);
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return (int) ws.ws_col;
#endif
    return 100;
}

void ConsoleUI::write_raw(const std::string & s) {
#ifdef _WIN32
    if (hout_) {
        DWORD written = 0;
        WriteConsoleA((HANDLE) hout_, s.data(), (DWORD) s.size(), &written, nullptr);
        return;
    }
#endif
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fflush(stdout);
}

void ConsoleUI::clear_block_locked() {
    if (block_lines_ <= 0) return;
    // The cursor sits at the start of the line BELOW the block after a draw, so the clear has to
    // move UP first. Getting this backwards makes every redraw append a new line instead of
    // replacing the old one - which is exactly what "Loading Model: ..." repeated down the screen was.
    std::string out;
    for (int i = 0; i < block_lines_; ++i) {
        out += kUpOne;
        out += kClear;
    }
    out += "\r";
    write_raw(out);
    block_lines_ = 0;
}

void ConsoleUI::draw_locked(const Render & r) {
    clear_block_locked();
    std::string out;
    for (const std::string & line : r.lines) out += line + "\r\n";
    write_raw(out);
    block_lines_ = (int) r.lines.size();
}

void ConsoleUI::publish(const Render & r) {
    std::lock_guard<std::mutex> lock(mu_);
    if (mode_ == Mode::Off || r.lines.empty()) return;

    std::string joined;
    for (const std::string & line : r.lines) joined += line + "\n";
    // Nothing changed ⇒ write nothing. A console in selection mode blocks the next write, so a
    // block that redraws on a timer is a console that hangs the moment the user clicks in it.
    if (joined == last_block_) return;
    last_block_ = joined;

    if (mode_ == Mode::Live) {
        draw_locked(r);
        cursor_hidden_ = true;
        return;
    }
    // Plain: the line only, no rules, no escape codes — greppable.
    write_raw(r.lines.front() + "\n");
}

void ConsoleUI::log_line(const std::string & line, bool to_console) {
    std::lock_guard<std::mutex> lock(mu_);
    if (log_fp_) {
        std::fwrite(line.data(), 1, line.size(), log_fp_);
        std::fputc('\n', log_fp_);
        std::fflush(log_fp_);
    }
    if (!to_console && mode_ != Mode::Off) return;   // Off: everything reaches the console, as before
    if (mode_ == Mode::Live) {
        clear_block_locked();        // step above the block, print, and let the next publish redraw
        write_raw(line + "\n");
        last_block_.clear();
        return;
    }
    write_raw(line + "\n");
}

void ConsoleUI::log_callback(int level, const char * text, void * user) {
    ConsoleUI * self = (ConsoleUI *) user;
    if (!self || !text) return;
    std::string line(text);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    if (line.empty()) return;
    // Warnings and errors always reach the console; INFO/DEBUG only when not quiet (Live = quiet).
    const bool important = level >= 3;   // GGML_LOG_LEVEL_WARN
    self->log_line(line, important || !self->quiet_);
}

}   // namespace live_ui
