// [LOCAL-FIX friday / exp-live-ui] arby-strata live console UI — state machine tests.
// Build: build_and_test.bat (no GPU, no model, no llama.cpp, no terminal).
#include "live_task.hpp"

#include <cstdio>
#include <string>

using namespace live_ui;

static int g_fail = 0;

static void check(bool ok, const std::string & what) {
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

static bool has(const std::string & hay, const std::string & needle) {
    return hay.find(needle) != std::string::npos;
}

// the status line: the only line the console carries
static std::string mid(const Render & r) {
    return r.lines.empty() ? std::string() : r.lines.front();
}

static void dump(const Render & r) {
    for (const std::string & l : r.lines) std::printf("      | %s\n", l.c_str());
}

static LiveTask make_task(bool thinking) {
    LiveTask t;
    t.set_start_time_us(0);
    t.set_model_name("Qwen3.8-Flash-Next-IQ3_XXS");
    t.set_server_info(":8080  ctx 131768");
    t.set_thinking(thinking, " thinking", "</think>");
    return t;
}

int main() {
    std::printf("\n=== 0. the load frame ===\n");
    {
        LiveTask t = make_task(true);
        t.set_start_time_us(1000000);
        Render r = t.render(100, false, 72000000);
        dump(r);
        check(r.lines.size() == 1, "exactly one line");
        check(has(mid(r), "Loading Model: Qwen3.8-Flash-Next-IQ3_XXS"), "names the model");
        check(has(mid(r), "1m 11s"), "elapsed load time");
        check(has(mid(r), "####"), "the marching bar");
    }

    std::printf("\n=== A. full turn with reasoning on ===\n");
    {
        LiveTask t = make_task(true);
        t.on_engine_ready(71.4);
        Render r = t.render(100, false, 72000000);
        check(has(mid(r), "Idle"), "idle after the engine handshake");

        t.on_request_start(73000000);
        t.set_max_tokens(1208);
        t.on_pp(5908, 5908, 812.0);
        r = t.render(100, false, 73000000);
        dump(r);
        check(has(mid(r), "Reading Prompt"), "activity is Reading Prompt");
        check(has(mid(r), "prefill 812 tok/s"), "prefill rate");
        check(has(mid(r), "decode -"), "no decode rate yet");
        check(has(mid(r), "####"), "the bar tracks the task");

        int64_t now = 73000000 + 7300000;
        now += 21739;
        t.on_token(now, " thinking");
        for (int i = 0; i < 412; ++i) {
            now += 21739;
            t.on_token(now, "token ");
        }
        r = t.render(100, false, now);
        dump(r);
        check(has(mid(r), "Thinking"), "activity is Thinking");
        check(has(mid(r), "decode 46.0 tok/s"), "live decode rate");

        now += 21739;
        t.on_token(now, "</think>");
        r = t.render(100, false, now);
        dump(r);
        check(has(mid(r), "Writing Reply"), "end tag moves to Writing Reply");

        t.on_done(24108, 18200, 1208, 7270.0, 26100.0, 312, 400, "stop", "15:41:22");
        r = t.render(100, false, now + 1000);
        dump(r);
        check(has(mid(r), "Idle"), "DONE returns to Idle");
        check(has(mid(r), "last 15:41:22 stop"), "the Idle line keeps the last request");
        check(t.record().gen_rate > 40 && t.record().gen_rate < 50, "gen rate ~46 tok/s from the DONE ms");
    }

    std::printf("\n=== B. reasoning off ===\n");
    {
        LiveTask t = make_task(false);
        t.on_engine_ready(70);
        t.on_request_start(0);
        t.on_token(1000, "Right, Boss");
        check(t.phase() == Phase::Writing, "no Thinking when the request does not reason");

        LiveTask t2 = make_task(true);      // capable, but the reply never opens a think block
        t2.on_engine_ready(70);
        t2.on_request_start(0);
        t2.on_token(1000, "Right, Boss");
        check(t2.phase() == Phase::Writing, "no open tag on a capable model still means Writing");
    }

    std::printf("\n=== C. a late PP cannot pull the label back to Reading ===\n");
    {
        LiveTask t = make_task(true);
        t.on_engine_ready(70);
        t.on_request_start(0);
        t.on_token(1000, " thinking");
        t.on_pp(12000, 24108, 700.0);
        Render r = t.render(100, false, 2000);
        check(has(mid(r), "Thinking"), "still Thinking");
        check(!has(mid(r), "Reading"), "did not regress to Reading");
    }

    std::printf("\n=== D. redraw caps ===\n");
    {
        LiveTask t = make_task(true);
        int64_t now = 0;
        for (int i = 0; i < 60; ++i) {           // 60 s of loading
            now += 1000000;
            if (t.should_render(now)) { t.mark_rendered(now); t.count_render(); }
        }
        std::printf("      loading 60 s -> %d renders\n", t.renders());
        check(t.renders() <= 62 && t.renders() >= 55, "load line redraws at ~1 Hz");

        t.on_engine_ready(60);
        t.on_request_start(now);
        now = now + 1000;
        int renders = 0;
        for (int i = 0; i < 1000; ++i) {         // 1000 tokens at 46 tok/s
            now += 21739;
            t.on_token(now, "x");
            if (t.should_render(now)) { t.mark_rendered(now); t.count_render(); ++renders; }
        }
        std::printf("      1000 tokens over 21.7 s -> %d renders (%.1f Hz)\n", renders, renders / 21.7);
        check(renders <= 115, "counters stay near the 5 Hz cap, not one per token");
        check(renders >= 90, "and still look live");
    }

    std::printf("\n=== E. cancel ===\n");
    {
        LiveTask t = make_task(true);
        t.on_engine_ready(70);
        t.on_request_start(0);
        t.on_pp(5908, 24108, 812.0);
        for (int i = 0; i < 50; ++i) t.on_token(i * 21739, "t");
        t.on_cancel("15:43:10", 12.5);
        Render r = t.render(100, false, 2000000);
        dump(r);
        check(has(mid(r), "Idle") && has(mid(r), "cancel"), "cancel shows on the Idle line");
    }

    std::printf("\n=== F. a second request while the slot is busy ===\n");
    {
        LiveTask t = make_task(true);
        t.on_engine_ready(70);
        t.on_request_start(0);
        t.on_pp(100, 24000, 800.0);
        t.on_token(21739, " thinking");
        t.on_task_accepted(2);
        Render r = t.render(100, false, 40000);
        dump(r);
        check(has(mid(r), "Thinking"), "the active request is still named");
        check(has(mid(r), "2 queued"), "and the queue is shown beside it");
    }

    std::printf("\n=== G. formatting ===\n");
    check(fmt_tokens(412) == "412", "fmt_tokens small");
    check(fmt_tokens(24108) == "24.1K", "fmt_tokens thousands");
    check(fmt_rate(812.4) == "812 tok/s", "fmt_rate integer above 100");
    check(fmt_rate(45.93) == "45.9 tok/s", "fmt_rate one decimal below 100");
    check(fmt_secs(7.34) == "7.3s", "fmt_secs sub-minute");
    check(fmt_secs(72.5) == "1m 13s", "fmt_secs over a minute");
    check(make_bar(0.5, 10, false) == "#####-----", "ASCII bar");

    std::printf("\n=== H. narrow terminal ===\n");
    {
        LiveTask t = make_task(true);
        t.on_engine_ready(70);
        t.on_request_start(0);
        t.on_pp(5908, 24108, 812.0);
        Render wide = t.render(100, false, 0);
        Render narrow = t.render(40, false, 0);
        std::printf("      wide  : %s\n      narrow: %s\n", mid(wide).c_str(), mid(narrow).c_str());
        check(!has(mid(narrow), "#"), "no bar under 60 columns");
        check(has(mid(narrow), "812 tok/s"), "rate survives the narrow case");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
