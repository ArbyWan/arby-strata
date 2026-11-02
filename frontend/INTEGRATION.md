# Integration notes — live console UI

**Status: implemented.** These notes record the hook points that were wired and why; the shipped
code is `frontend/live_task.*` and `frontend/ui_live.*`, and `C:\Hermes\Default Project\strata-cli-ui-draft.md`
carries the full design record.

## What has actually been verified

`build_and_test.bat` compiles `live_task.cpp` + `live_task_test.cpp` with the same `clang++` the
project build uses (ROCm 10.2 venv, MSVC CRT) and runs them:

- full turn with reasoning: `Reading → Thinking → Writing → Idle`, labels and numbers
- reasoning off: `Reading → Writing`, no `Thinking`
- a late `PP` cannot pull the block back to `Reading`
- 1,000 tokens at 46 tok/s ⇒ **100 renders (4.6 Hz)** — the 5 Hz cap holds, it is not one per token
- cancel mid-thinking ⇒ `Idle · … · cancel`
- a second request while the slot is busy ⇒ `· 2 queued` appended to the live phase
- formatting (tokens/rates/durations/bar), and the narrow-terminal case dropping bar and ETA

`ui_live.cpp` compiles (`-c`, warnings only from MSVC's `fopen` deprecation, which the frontend
already carries).

**Not verified:** anything that needs a terminal — the ANSI block redraw, the cursor save/restore,
the conhost VT path. Those need a real console, i.e. a frontend build and a run (your task, not the
assistant's).

## Where the files go

```
frontend/live_task.hpp        <- as-is
frontend/live_task.cpp        <- as-is
frontend/ui_live.hpp          <- as-is
frontend/ui_live.cpp          <- as-is
frontend/tests/live_task_test.cpp  <- as-is
```

`CMakeLists.txt`, next to the existing `engine_test` target (`:2195`):

```cmake
add_executable(live_task_test EXCLUDE_FROM_ALL frontend/tests/live_task_test.cpp frontend/live_task.cpp)
target_include_directories(live_task_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/frontend)
target_compile_features(live_task_test PRIVATE cxx_std_20)
```

and both `.cpp` files added to the front end target's source list.

## Hook 1 — `frontend/engine.hpp`: keep the two fields the engine already sends

```cpp
// struct EngineLine
long long pp_done = 0, pp_total = 0;
double    pp_ms   = 0, pp_rate = 0;    // [LIVE-UI] 3rd and 4th fields of the PP line
```

```cpp
} else if (line.rfind("PP ", 0) == 0) {
    out.kind = EngineLine::PP;
    // [LIVE-UI] four fields, not two: src/poly/main.cpp:853 prints "PP <done> <total> <ms> <tok/s>"
    std::sscanf(line.c_str(), "PP %lld %lld %lf %lf",
                &out.pp_done, &out.pp_total, &out.pp_ms, &out.pp_rate);
}
```

## Hook 2 — `frontend/strata_context.cpp`

Add `#include "live_task.hpp"` / `#include "ui_live.hpp"`, a `live_ui::LiveTask live;` member, and a
`live_ui::ConsoleUI * ui = nullptr;` set from `main.cpp`.

**Thinking tags** — in `load_model`, right after `I.chat_params` is built (`:1358`):

```cpp
// [LIVE-UI] the autoparser already derived these from this model's template
const std::string end_tag = !I.chat_params.thinking_end_tags.empty()
                                ? I.chat_params.thinking_end_tags.front()
                                : literals.think_close;   // vocab fallback
live.set_thinking(I.chat_params.supports_thinking && !end_tag.empty(), end_tag);
```

**Request start** — where `slot.task` is assigned (`:303`):

```cpp
live.set_prompt_tokens(t.n_tokens());
live.set_max_tokens(slot.n_predict);
live.on_request_start(now_us());          // std::chrono::steady_clock in microseconds
```

**Queue** — when a task is deferred (`queue_tasks` push, same area): `live.on_task_accepted((int) queue_tasks.size());`

**Prefill** — `case EngineLine::PP` (`:704`), **outside** the `params.return_progress` guard:

```cpp
live.on_pp(line.pp_done, line.pp_total, line.pp_rate);
if (!slot.cancelled && slot.task->params.stream && slot.task->params.return_progress) { ... unchanged ... }
```

**Tokens** — end of `on_token()` (`:790`): `live.on_token(now_us(), token_str);`

**Done** — after the stats block in `on_done()` (`:838`):

```cpp
live.on_done(d.prompt_n, (long long) slot.stats.n_prompt_cached, d.produced,
             d.prompt_ms, d.decode_ms, d.draft_accepted, d.draft_offered, d.finish, clock_now());
```

**Cancel / error** — in the cancel path and `case EngineLine::ERR`:
`live.on_cancel(clock_now(), elapsed_s());` / `live.on_error(clock_now(), elapsed_s());`

**Log lines** — every `SRV_INF/LOG_INF` that should reach the console goes through
`ui->log_line(...)` instead of straight to stdout; `case EngineLine::INFO` (`:732`) becomes
`ui->log_line("engine: " + line.raw)`. That is what stops anything scribbling through the block.

## Hook 3 — `frontend/main.cpp`

- Parse `--ui=live|plain|off` and `--log-file <path>` **before** `common_params_parse` (they are the
  front end's own flags, not llama.cpp's).
- `ui.init(mode, log_file, err)`; on failure print `err` and exit 2.
- `llama_log_set(live_ui::ConsoleUI::log_callback, &ui);` after `llama_backend_init()`.
- `ctx_server.set_ui(&ui);` before `ctx_server.init(...)`.
- `ui.shutdown()` in `clean_up()` and on the SIGINT path, so the cursor comes back.

## Hook 4 — the ticker

The engine read loop blocks on the pipe, so the counters need their own beat:

```cpp
// [LIVE-UI] 20 Hz check, 5 Hz redraw at most (LiveTask::should_render holds the policy)
std::thread([&] {
    while (!stopping) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int64_t now = now_us();
        if (!live.should_render(now)) continue;
        ui.publish(live.render(ui.width(), ui.live(), now));
        live.mark_rendered(now);
        live.count_render();
    }
}).detach();
```

## Two things learned from the engine source while writing this

- `PP <done> <total>` — the **total is the fresh token count**, not the whole prompt
  (`src/qwen35/q36_engine.cu:3545` prints `done, fresh`). With a cache hit it is therefore smaller
  than the prompt. The label takes the request's own `n_tokens()` (`set_prompt_tokens`), the bar
  takes `PP`. Do not "fix" this by labelling from `PP` — with an 18 K cache hit it would announce a
  24 K prompt as 5.9 K.
- The old `task N: DONE produced=… prompt_ms=…` line stays available (it is the greppable raw form);
  the human record is a second line, not a replacement, until you decide otherwise.

## Rollback

`--ui=off` prints exactly what the front end prints today (no block, no ANSI, logs as before). The
whole change is additive: four new files, one struct field pair, five call sites.
