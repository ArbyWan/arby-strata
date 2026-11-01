# Relationship to upstream

## Sources

| What | Where | Version |
| --- | --- | --- |
| Engines, tools, docs, bench | https://github.com/VecSzn/PolyStrata | tag `v0.1.45-poly.1`, commit `d0cd84dc6bd009d1d9e2191e4e28cc76b80f1ee3` |
| ggml, llama, common, mtmd, cpp-httplib | https://github.com/robertkarl/llama.cpp, branch `UnbiasedStrata` | commit `385445fad49e42fa718b5e1f71668f35b7a08559` — ggml-org `3cf03257` (the commit upstream pins) plus six parser commits |
| The C++ front end (`frontend/`, its CMake block, its tests) | https://github.com/robertkarl/UnbiasedStrata | as of `22abfdb`, MIT, adapted |

The import commit is byte-identical to PolyStrata `v0.1.45-poly.1`. `git diff <import> HEAD` is the whole story
of this fork.

The fork exists because upstream's server is 30,730 lines of Python between a request and a token. The rules
applied, in order of force:

1. Nothing reaches the network during setup, build or start.
2. Nothing puts Python between a request and a token.
3. Nothing complicates the one-command build.
4. Marketing is not source.

## Build (`CMakeLists.txt`, `Makefile`, `.gitmodules`, `.gitignore`)

- **ggml from the submodule, not a fetch.** `STRATA_GGML_DIR` defaults to `third_party/llama.cpp` and the
  configure stops with the `git submodule update --init` line if `ggml/CMakeLists.txt` is missing. Upstream's
  `FetchContent` of llama.cpp (the build's only network access) is deleted.
- **The llama.cpp pin is a commit, not a patch.** The submodule points at robertkarl/llama.cpp branch
  `UnbiasedStrata`, which is ggml-org `3cf03257` plus the six tool-call parser fixes described in UnbiasedStrata's
  `UPSTREAM.md` (Qwen3-Coder argument order, stuttered close tags, blank lines between arguments, and the
  partial-re-parse guard that turned a mid-stream 500 into a logged warning). Those fixes matter here because
  the engine samples unconstrained, so the grammars see text llama.cpp's own constrained sampler would never
  produce. `git status` is clean after a build.
- **llama.cpp's libraries for the front end.** A second `add_subdirectory` of the submodule with
  `LLAMA_BUILD_COMMON` and `LLAMA_BUILD_MTMD` on, and tests, tools, examples, server, app, UI, prebuilt UI,
  OpenSSL, llguidance and curl off. It reuses the ggml targets configured above. Gated by `STRATA_FRONTEND`
  (default on) **and** `STRATA_NATIVE_EXPERTS`, not by CUDA or HIP, so the front end also builds on a machine
  with no GPU.
- **The front end, `arby-strata`.** Seven of llama.cpp's `tools/server` files compiled unmodified into
  `arby_llama_server`; `frontend/ui_stub/ui.cpp` supplies the empty web-UI asset table that `server-http.cpp`
  references. `frontend/strata_context.cpp` replaces `server-context.cpp`.
- **`.gitignore`** no longer ignores `/third_party/llama.cpp*` (upstream line 31) and ignores the two build
  directories this fork uses.
- **`Makefile`.** `make build` configures once and builds `strata` and `arby-strata` into `build-hip/bin/`, not
  upstream's `all`, which also builds ~70 parity and bench programs. `make frontend` builds the front end alone
  in `build-frontend/`; `make test` builds and runs the front end's tests there against a fake engine. `HIP_ARCHS`
  defaults to `gfx1201`, this box's card.

## Front end behaviour where it differs from upstream's server

Recorded because a client written for upstream may notice.

- `/health` returns llama.cpp's `{"status": "ok", "busy": ...}` plus `"service": "strata"`, `"loaded": true`,
  `"model"`, `"max_context"` — upstream's own `tools/strata_bench.py` refuses a server without the first two.
- Streams always end with a usage chunk (empty `choices`, `usage`, `timings`), whether or not the client sent
  `stream_options.include_usage`. Upstream's bench client reads `prompt_tokens` from it.
- One slot: requests are served one at a time in arrival order.
- **No constrained decoding.** `response_format` and tool schemas are parsed and used for output parsing, not
  enforced at the sampler, because the engine samples. Logged once per such request.
- Images are encoded in the front end (`--mmproj FILE`) with llama.cpp's `mtmd`: the projector runs, and the
  embedding rows reach the engine in upstream's `SVE1` record file through `GENI`. Projector width is checked
  against the model's embedding width at start. CPU by default, `--mmproj-gpu` to move it.
- No Anthropic `/v1/messages`, no Responses dialect, no MCP hub, no web UI, no Prometheus, no model switching,
  no replicas, no `SAVE`/`RESTORE` from the front end (`PARKSAVE`/`PARKLOAD` arrive with the engine-side port,
  see the project tracker).

## Removed from upstream

Each entry names the rule it satisfies.

- **Rule 1 (network):** `setup.py` (hardware detection, pip, CUDA, llama.cpp zips, prebuilt engines from GitHub
  releases, model downloads, pack and MTP orchestration, run-config JSON, start), `setup.sh`, `SETUP.bat`,
  `START-HERE.bat`, `UPDATE.bat`, `update.sh`, `Dockerfile`, `docker-entrypoint.sh`, `.dockerignore`,
  `flake.nix`, `flake.lock`, `requirements.txt` (replaced by `tools/requirements.txt`: numpy, regex),
  `conftest.py`, `tools/strata_mcp.py` (an installer driven over MCP), `tools/image/` (stable-diffusion.cpp,
  Vulkan SDK and model downloads).
- **Rule 2 (Python in the serving path):** `serve/` (30,730 lines: the HTTP server with the OpenAI, Anthropic
  and Responses dialects, jinja2 templating, the Python tokenizer path, per-family reasoning and tool-call
  parsers, MCP, structured output, replicas, Prometheus, the hardware monitor, the web app and its tests) and
  `polystrata.py`, the terminal front end that imports it. Replaced by `frontend/`.
- **Scripts and tests that imported the removed modules:** `tools/calibrate.py`, `tools/strata_bench.py` and
  `bench.sh`, `tools/make_poly_release.py`, `tools/{batch_groups_cache,cancel_park,conversation_cache_*,
  conversation_restore_speed,logit_bias_*,session_pressure}_*.py`, `tools/test_{chat,dll_closure,
  effort_end_args,mtp_fetch,poly_prebuilt,qimage}.py`. Upstream's copies run against upstream.
- **Rule 4 (marketing):** the six translated READMEs, `docs/media/`, `.github/`, `SECURITY.md` (upstream's
  advisory process), `AGENTS.md` (contributor notes for upstream's installers), `bench/results/` (per-machine
  run records are records, not source), `README.md` (rewritten here).

## Not ported (deliberately, for now)

The engine changes carried by the earlier 0.1.40.3 fork — elastic context (`STRATA_CTX_GROW`), the releasable
expert-cache VMM band, `PARKSAVE`/`PARKLOAD` disk parking, L3 prefetch, helper-CPU pinning, `STRATA_PF_INPASS`,
resident lend, the PCIe pick/order knobs, the VRAM bands — are a **measured re-port**, not a merge: 16 commits,
86 conflict hunks against this tree. They land one at a time, each with a benchmark, after the stock tree has
been measured on this box. The exact list, hunk counts and order live in the project tracker, not here.
