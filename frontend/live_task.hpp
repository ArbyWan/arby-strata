// arby-strata live console UI — the state machine behind the status frame.
// [LOCAL-FIX friday / exp-live-ui]
//
// Pure C++20, no I/O, no llama.cpp, no Windows headers: testable on its own
// (frontend/tests/live_task_test.cpp). The renderer that owns the terminal lives in ui_live.*.
//
// Data comes from the engine protocol the frontend already parses (frontend/engine.hpp):
//   PP <done> <total> <ms> <tok/s>   progress, with the rate the engine measured
//   T <id>                           one token
//   DONE <gen> <prompt> <prompt_ms> <decode_ms> <finish> <drafts_ok> <drafts_all> <reused>
//
// Layout:
//
//   arby-strata  :8080  Qwen3.8-Flash-Next-IQ3_XXS  ctx 131768    <- printed once, at startup
//   ------------------------------------------------------------------
//   Loading Model: Qwen3.8-Flash-Next-IQ3_XXS  [    |    ]  71s
//   ------------------------------------------------------------------
//
//   ==================================================================
//   Reading Prompt | prefill 812 tok/s /// decode - | [####----] 24.1K
//   ==================================================================
//
// The bar belongs to the CURRENT task and resets with it: prefill progress while reading, then
// generated tokens against the request's n_predict while thinking and writing.
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace live_ui {

enum class Phase { Loading, Idle, Queued, Reading, Thinking, Writing };

// The numbers kept after a request finishes.
struct RequestRecord {
    bool        valid         = false;
    std::string clock;
    long long   prompt_n      = 0;
    long long   prompt_cached = 0;
    long long   gen_n         = 0;
    double      prompt_rate   = 0;
    double      gen_rate      = 0;
    double      ttft_s        = 0;
    double      total_s       = 0;
    int         draft_pct     = -1;
    std::string finish;
};

// The whole block, drawn in place: separator, status, separator.
struct Render {
    std::vector<std::string> lines;
};

std::string fmt_tokens(long long n);           // 412 / 5.9K / 24.1K / 1.2M
std::string fmt_rate(double tok_per_s);        // "812 tok/s" or "45.9 tok/s"
std::string fmt_secs(double seconds);          // "7.3s" / "45.1s" / "1m 12s"
std::string make_bar(double frac, int cells, bool ansi);

class LiveTask {
public:
    void set_start_time_us(int64_t us) { start_us_ = us; }
    void set_model_name(std::string name) { model_name_ = std::move(name); }
    void set_server_info(std::string info) { server_info_ = std::move(info); }

    // thinking markers: literals.hpp resolves them from the vocabulary; supported=false ⇒ no
    // Thinking phase, ever. With the open tag the phase follows what the reply actually opens with,
    // so a thinking-capable model that answers directly is never labelled "Thinking".
    void set_thinking(bool supported, std::string open_tag, std::string end_tag) {
        think_supported_ = supported && !open_tag.empty();
        think_open_      = std::move(open_tag);
        end_tag_         = std::move(end_tag);
    }
    void set_max_tokens(long long n) { max_tokens_ = n; }        // n_predict: the bar's denominator
    void set_prompt_tokens(long long n) { prompt_tokens_ = n; }  // t.n_tokens(): the label's number
    void on_cache(long long n) { prompt_cached_ = n; }           // RESUME: the cache hit, live

    void on_boot();
    void on_engine_ready(double load_s);
    void on_task_accepted(int waiting);
    void on_request_start(int64_t now_us);
    void on_pp(long long done, long long total, double rate);
    void on_token(int64_t now_us, const std::string & piece);
    void on_done(long long prompt_n, long long prompt_cached, long long gen_n,
                 double prompt_ms, double decode_ms, long long draft_ok, long long draft_all,
                 const std::string & finish, const std::string & clock);
    void on_cancel(const std::string & clock, double total_s);
    void on_error(const std::string & clock, double total_s);

    Phase phase()   const { return phase_; }
    int   waiting() const { return waiting_; }
    const RequestRecord & record() const { return record_; }
    const std::string & model_name() const { return model_name_; }
    const std::string & server_info() const { return server_info_; }

    // [LOCAL-FIX friday / status-endpoint] read-only view of the live counters, for GET /status.
    static const char * phase_name(Phase p);
    const char * phase_label() const { return phase_name(phase_); }
    long long prompt_tokens()   const { return prompt_tokens_; }
    long long max_tokens()      const { return max_tokens_; }
    long long pp_done()         const { return pp_done_; }
    long long pp_total()        const { return pp_total_; }
    double    pp_rate()         const { return pp_rate_; }
    long long gen_tokens()      const { return n_gen_; }
    long long prompt_cached()   const { return prompt_cached_; }
    int       draft_pct()       const { return draft_pct_; }
    int64_t   start_us()        const { return start_us_; }
    int64_t   request_us()      const { return request_us_; }
    int64_t   first_token_us()  const { return first_token_us_; }
    double    decode_rate(int64_t now_us) const { return window_rate(now_us); }

    Render render(int width, bool ansi, int64_t now_us) const;

    // State changes redraw at once, counters at most 5 Hz, the load line at 1 Hz. A Windows console
    // in selection mode blocks the next write, so writing less is a feature, not a compromise.
    bool should_render(int64_t now_us) const;
    void mark_rendered(int64_t now_us) { last_render_us_ = now_us; last_rendered_phase_ = phase_; }
    int  renders() const { return renders_; }
    void count_render() { ++renders_; }

private:
    double      window_rate(int64_t now_us) const;
    void        push_sample(int64_t now_us);
    std::string load_line(int width, int64_t now_us) const;
    std::string status_line(int width, int64_t now_us) const;

    Phase       phase_           = Phase::Loading;
    int         waiting_         = 0;
    bool        think_supported_ = false;
    std::string think_open_;
    std::string end_tag_;
    bool        end_seen_        = false;
    std::string head_;
    std::string tail_;
    std::string model_name_;
    std::string server_info_;

    int64_t start_us_       = 0;
    int64_t request_us_     = 0;
    int64_t first_token_us_ = 0;
    double  load_s_         = 0;

    long long pp_done_       = 0;
    long long pp_total_      = 0;
    double    pp_rate_       = 0;
    long long prompt_cached_ = 0;
    long long n_gen_         = 0;
    long long thought_gen_   = 0;
    long long prompt_tokens_ = 0;
    long long max_tokens_    = -1;
    int       draft_pct_     = -1;

    std::deque<std::pair<int64_t, long long>> samples_;   // (us, n_gen) over the last 2 s
    RequestRecord record_;

    int64_t last_render_us_      = 0;
    Phase   last_rendered_phase_ = Phase::Loading;
    int     renders_             = 0;
};

}   // namespace live_ui
