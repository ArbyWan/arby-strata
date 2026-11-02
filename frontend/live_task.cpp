// [LOCAL-FIX friday / exp-live-ui] arby-strata live console UI — state machine implementation.
#include "live_task.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace live_ui {

namespace {
constexpr int64_t kWindowUs       = 2000000;   // 2 s rolling window for the decode rate
constexpr int64_t kRedrawUs       = 200000;    // 5 Hz for the counters
constexpr int64_t kRedrawLoadUs   = 1000000;   // 1 Hz while loading: elapsed seconds only
constexpr size_t  kTailKeep       = 128;       // end-tag scan buffer
constexpr int     kBarCells       = 20;

std::string fmt_double(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return std::string(buf);
}

std::string fmt_num(double v) {   // a rate without its unit, for the second mention on a line
    if (!(v > 0)) return "-";
    return v >= 100 ? fmt_double(v, 0) : fmt_double(v, 1);
}

// Display cells, not bytes: the ASCII UI keeps this trivially true, but a stray UTF-8 byte from a
// log line must not make a line look wider than it renders.
int utf8_width(const std::string & s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string truncate_to_width(const std::string & s, int width) {
    if (width <= 0) return std::string();
    int n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (((unsigned char) s[i] & 0xC0) != 0x80) {
            if (n == width) return s.substr(0, i);
            ++n;
        }
    }
    return s;
}

std::string fit(const std::string & s, int width) {
    return utf8_width(s) > width ? truncate_to_width(s, width) : s;
}

std::string rule(int width, char c) {
    return std::string((size_t) std::max(20, std::min(width, 200)), c);
}
}   // namespace

std::string fmt_tokens(long long n) {
    if (n >= 1000000) return fmt_double((double) n / 1e6, 1) + "M";
    if (n >= 1000)    return fmt_double((double) n / 1e3, 1) + "K";
    return std::to_string(n);
}

std::string fmt_rate(double tok_per_s) {
    if (!(tok_per_s > 0)) return "-";
    return fmt_num(tok_per_s) + " tok/s";
}

std::string fmt_secs(double seconds) {
    if (!(seconds > 0)) return "0s";
    if (seconds < 60)   return fmt_double(seconds, 1) + "s";
    const long long total = (long long) std::llround(seconds);
    return std::to_string(total / 60) + "m " + std::to_string(total % 60) + "s";
}

std::string make_bar(double frac, int cells, bool ansi) {
    if (cells <= 0) return std::string();
    frac = std::min(1.0, std::max(0.0, frac));
    const int filled = (int) std::lround(frac * cells);
    std::string out;
    out.reserve((size_t) cells);
    for (int i = 0; i < cells; ++i) out += (i < filled) ? (ansi ? "\u2588" : "#") : (ansi ? "\u2591" : "-");
    return out;
}

void LiveTask::push_sample(int64_t now_us) {
    samples_.emplace_back(now_us, n_gen_);
    while (samples_.size() > 2 && now_us - samples_.front().first > kWindowUs) samples_.pop_front();
}

double LiveTask::window_rate(int64_t now_us) const {
    if (samples_.size() < 2 || now_us <= samples_.front().first) return 0.0;
    const double dt = (double) (samples_.back().first - samples_.front().first) / 1e6;
    if (!(dt > 0)) return 0.0;
    return (double) (samples_.back().second - samples_.front().second) / dt;
}

void LiveTask::on_boot() {
    phase_  = Phase::Loading;
    record_ = RequestRecord{};
}

void LiveTask::on_engine_ready(double load_s) {
    load_s_ = load_s;
    phase_  = Phase::Idle;
}

void LiveTask::on_task_accepted(int waiting) {
    waiting_ = waiting;
    if (phase_ == Phase::Idle || phase_ == Phase::Loading) phase_ = Phase::Queued;
}

void LiveTask::on_request_start(int64_t now_us) {
    phase_          = Phase::Reading;
    request_us_     = now_us;
    first_token_us_ = 0;
    end_seen_       = false;
    head_.clear();
    tail_.clear();
    pp_done_ = pp_total_ = 0;
    pp_rate_        = 0;
    prompt_cached_  = 0;
    prompt_tokens_  = 0;
    n_gen_          = 0;
    thought_gen_    = 0;
    draft_pct_      = -1;
    max_tokens_     = -1;
    samples_.clear();
    if (waiting_ > 0) --waiting_;
}

void LiveTask::on_pp(long long done, long long total, double rate) {
    // No regression: a late PP can never pull the line back from Thinking/Writing to Reading.
    if (phase_ != Phase::Reading) return;
    pp_done_  = done;
    pp_total_ = total;
    if (rate > 0) pp_rate_ = rate;
}

void LiveTask::on_token(int64_t now_us, const std::string & piece) {
    if (first_token_us_ == 0) first_token_us_ = now_us;
    ++n_gen_;
    if (phase_ == Phase::Reading) {
        if (think_supported_ && !think_open_.empty()) {
            head_ += piece;
            if (head_.find(think_open_) != std::string::npos) {
                phase_ = Phase::Thinking;
            } else if (head_.size() >= think_open_.size() + 1) {
                phase_ = Phase::Writing;
            }
        } else {
            phase_ = Phase::Writing;
        }
    }
    if (!end_seen_ && !end_tag_.empty()) {
        tail_ += piece;
        if (tail_.size() > kTailKeep) tail_.erase(0, tail_.size() - kTailKeep);
        if (tail_.find(end_tag_) != std::string::npos) {
            end_seen_ = true;
            if (phase_ == Phase::Thinking) {
                phase_       = Phase::Writing;
                thought_gen_ = n_gen_;
            }
        }
    }
    push_sample(now_us);
}

void LiveTask::on_done(long long prompt_n, long long prompt_cached, long long gen_n,
                       double prompt_ms, double decode_ms, long long draft_ok, long long draft_all,
                       const std::string & finish, const std::string & clock) {
    record_               = RequestRecord{};
    record_.valid         = true;
    record_.clock         = clock;
    record_.prompt_n      = prompt_n;
    record_.prompt_cached = prompt_cached;
    record_.gen_n         = gen_n;
    record_.prompt_rate   = prompt_ms > 0 ? (double) (prompt_n - prompt_cached) / (prompt_ms / 1e3) : pp_rate_;
    record_.gen_rate      = decode_ms > 0 ? (double) gen_n / (decode_ms / 1e3) : 0.0;
    record_.ttft_s        = first_token_us_ > request_us_ && request_us_ > 0
                                ? (double) (first_token_us_ - request_us_) / 1e6 : 0.0;
    record_.total_s       = (prompt_ms + decode_ms) / 1e3;
    record_.draft_pct     = draft_all > 0 ? (int) std::lround(100.0 * (double) draft_ok / (double) draft_all) : -1;
    record_.finish        = finish;
    prompt_cached_        = prompt_cached;
    phase_                = Phase::Idle;
    waiting_              = 0;
}

void LiveTask::on_cancel(const std::string & clock, double total_s) {
    record_               = RequestRecord{};
    record_.valid         = true;
    record_.clock         = clock;
    record_.prompt_n      = pp_total_;
    record_.prompt_cached = prompt_cached_;
    record_.gen_n         = n_gen_;
    record_.prompt_rate   = pp_rate_;
    record_.gen_rate      = 0;
    record_.total_s       = total_s;
    record_.finish        = "cancel";
    phase_                = Phase::Idle;
    waiting_              = 0;
}

void LiveTask::on_error(const std::string & clock, double total_s) {
    on_cancel(clock, total_s);
    record_.finish = "error";
}

// "Loading Model: <name>  [    |    ]  71s"
std::string LiveTask::load_line(int width, int64_t now_us) const {
    const double age = start_us_ > 0 && now_us > start_us_ ? (double) (now_us - start_us_) / 1e6 : 0.0;
    // The engine reports nothing until READY, so this bar is a marching highlight, not a percentage:
    // it never claims progress it cannot know.
    std::string bar((size_t) kBarCells, '-');
    if (width >= 60) {
        const int pos = (int) std::fmod(age * 1.5, (double) (kBarCells + 6)) - 3;
        for (int i = 0; i < 5; ++i) {
            const int k = pos + i;
            if (k >= 0 && k < kBarCells) bar[(size_t) k] = '#';
        }
    } else {
        bar.clear();
    }
    std::string name = model_name_.empty() ? std::string("model") : model_name_;
    std::string line = "Loading Model: " + name;
    if (!bar.empty()) line += "  [" + bar + "]";
    line += "  " + fmt_secs(age);
    return fit(line, width);
}

// "Reading Prompt | prefill 812 tok/s /// decode 46.3 tok/s | [####----] 24.1K/32.8K"
std::string LiveTask::status_line(int width, int64_t now_us) const {
    std::string activity;
    double      frac = 0.0;
    std::string count;

    switch (phase_) {
        case Phase::Reading:
            activity = "Reading Prompt";
            if (pp_total_ > 0) {
                frac  = (double) pp_done_ / (double) pp_total_;
                count = fmt_tokens(pp_done_) + "/" + fmt_tokens(pp_total_);
            }
            break;
        case Phase::Thinking:
            activity = "Thinking";
            if (max_tokens_ > 0) {
                frac  = (double) n_gen_ / (double) max_tokens_;
                count = fmt_tokens(n_gen_) + "/" + fmt_tokens(max_tokens_);
            } else {
                count = fmt_tokens(n_gen_) + " tok";
            }
            break;
        case Phase::Writing:
            activity = "Writing Reply";
            if (max_tokens_ > 0) {
                frac  = (double) n_gen_ / (double) max_tokens_;
                count = fmt_tokens(n_gen_) + "/" + fmt_tokens(max_tokens_);
            } else {
                count = fmt_tokens(n_gen_) + " tok";
            }
            break;
        case Phase::Queued:
            activity = waiting_ > 0 ? "Queued (" + std::to_string(waiting_) + " waiting)" : "Queued";
            break;
        case Phase::Idle:
        default:
            activity = "Idle";
            if (record_.valid) {
                count = "last " + record_.clock;
                if (!record_.finish.empty()) count += " " + record_.finish;
            }
            break;
    }

    const double prefill = (phase_ == Phase::Reading && pp_rate_ > 0) ? pp_rate_
                                                                     : (record_.valid ? record_.prompt_rate : pp_rate_);
    const double decode  = (phase_ == Phase::Thinking || phase_ == Phase::Writing)
                               ? window_rate(now_us)
                               : (record_.valid ? record_.gen_rate : 0.0);

    // Assembled by priority: the activity and the two rates always, then the bar with its counts,
    // then the extras (ETA, cache) as far as the width allows.
    std::string line = activity;
    line += " | prefill " + fmt_rate(prefill);
    line += " /// decode " + fmt_rate(decode);

    std::vector<std::string> extras;
    if (phase_ == Phase::Reading && pp_rate_ > 0 && pp_total_ > pp_done_)
        extras.push_back(fmt_secs((double) (pp_total_ - pp_done_) / pp_rate_) + " left");
    if ((phase_ == Phase::Thinking || phase_ == Phase::Writing) && decode > 0 && max_tokens_ > n_gen_)
        extras.push_back(fmt_secs((double) (max_tokens_ - n_gen_) / decode) + " left");
    if (prompt_cached_ > 0) extras.push_back("cache " + fmt_tokens(prompt_cached_));

    const auto try_add = [&](const std::string & piece) {
        if (utf8_width(line + " " + piece) <= width) { line += " " + piece; return true; }
        return false;
    };

    if (width >= 60) {
        const std::string mid_bar = "| [" + make_bar(frac, kBarCells, false) + "]";
        if (width - utf8_width(line) > 24 && try_add(mid_bar)) {
            if (!count.empty()) try_add(count);
        }
    } else if (!count.empty()) {
        try_add(count);
    }
    for (const std::string & piece : extras)
        if (!try_add(piece)) break;

    if (waiting_ > 0 && phase_ != Phase::Queued && phase_ != Phase::Idle && phase_ != Phase::Loading)
        line += " | " + std::to_string(waiting_) + " queued";
    return fit(line, width);
}

Render LiveTask::render(int width, bool ansi, int64_t now_us) const {
    // ONE line, redrawn in place. The frame rules are gone: the console has nothing else in it,
    // because every log line goes to --log-file.
    Render r;
    r.lines.push_back(phase_ == Phase::Loading ? load_line(width, now_us) : status_line(width, now_us));
    (void) ansi;   // ASCII by design: a legacy codepage must not turn the line into mojibake
    return r;
}

bool LiveTask::should_render(int64_t now_us) const {
    if (phase_ != last_rendered_phase_) return true;              // a state change shows at once
    if (last_render_us_ == 0) return true;
    const int64_t gap = phase_ == Phase::Loading ? kRedrawLoadUs : kRedrawUs;
    return now_us - last_render_us_ >= gap;
}

}   // namespace live_ui
