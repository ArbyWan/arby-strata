// [LOCAL-FIX friday / exp-live-ui] arby-strata live console UI — the terminal side.
//
// Owns stdout while the front end runs: every log line goes through log_line(), so nothing can
// scribble through the bottom block. In Live mode the console carries the block and nothing else
// except warnings and errors; everything else lands in --log-file.
#pragma once

#include "live_task.hpp"

#include <mutex>
#include <string>

namespace live_ui {

class ConsoleUI {
public:
    enum class Mode { Live, Plain, Off };

    ~ConsoleUI() { shutdown(); }

    // Live needs a TTY that can do ANSI; init() demotes itself to Plain (this is never an error).
    bool init(Mode mode, const std::string & log_file, std::string & err);
    void shutdown();

    // A log line: written to the log file always, and above the block when to_console is set.
    void log_line(const std::string & line, bool to_console = true);

    // Replace the block. Identical content writes nothing at all.
    void publish(const Render & r);

    void set_quiet_logs(bool quiet) { quiet_ = quiet; }
    bool live() const { return mode_ == Mode::Live; }
    Mode mode() const { return mode_; }
    int  width() const { return term_width(); }

    static void log_callback(int level, const char * text, void * user);

private:
    void draw_locked(const Render & r);
    void clear_block_locked();
    void write_raw(const std::string & s);
    int  term_width() const;

    mutable std::mutex mu_;
    Mode        mode_          = Mode::Off;
    bool        quiet_         = true;    // Live: INFO/DEBUG to the file only
    int         block_lines_   = 0;
    bool        cursor_hidden_ = false;
    std::string log_path_;
    FILE *      log_fp_        = nullptr;
    std::string last_block_;
#ifdef _WIN32
    void *        hout_          = nullptr;
    unsigned long original_mode_ = 0;
#endif
};

}   // namespace live_ui
