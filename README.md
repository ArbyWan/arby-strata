# arby-strata

A fork of [PolyStrata](https://github.com/VecSzn/PolyStrata) `v0.1.45-poly.1` that keeps the inference engines
and replaces the Python around them with a C++ front end built from llama.cpp's own server code. One AMD GPU
(RDNA4, gfx1201), Linux/WSL2, HIP. No installer, no downloads at build or start, no Python between a request
and a token.

```
git submodule update --init      # shallow; third_party/llama.cpp @ robertkarl/llama.cpp, branch UnbiasedStrata
make build                       # build-hip/bin/{strata, arby-strata}
make test                        # the front end against a fake engine; no GPU, no model
```

## What this is

The engines are upstream's, unmodified: `strata` (Qwen3.8-Flash-Next from packs, with MTP drafts) and the
`strata-poly`/`strata-dsv41` trees that ship beside it. The engine never tokenizes, never sees text and never
sees a tool call: it samples on the GPU and returns token ids over a line protocol.

What changed is everything around it:

| | upstream | here |
| --- | --- | --- |
| server | `serve/` — 30,730 lines of Python (OpenAI + Anthropic + Responses dialects, MCP, web UI, telemetry) | `frontend/` — 2,642 lines of C++ over llama.cpp's `tools/server` |
| tokenizer | hand-written BPE in Python | `llama_vocab` from the GGUF |
| chat template | jinja2 | `common/chat.cpp` |
| tool-call parsing | five hand-written per-family parsers | `common/chat_parse` + its PEG autoparser |
| SSE, pings, disconnect, `max_tokens` clamp | written by hand | llama.cpp's `server-http`/`server-stream` |
| setup | `setup.py`, 8,000+ lines of downloads at install, build and start | `git submodule update --init` then `make build` |

Seven files of `tools/server` — `server-http`, `server-queue`, `server-task`, `server-common`, `server-chat`,
`server-schema`, `server-stream` — are compiled **unmodified** from the pinned submodule. The eighth,
`server-context.cpp`, is bound to `llama_context` and is replaced by `frontend/strata_context.cpp`, which runs
the engine as a child process over its line protocol and uses a vocab-only `llama_model` for tokenizing, the
chat template and the output parsers.

## Build

Requirements: ROCm with HIP, hipBLAS and hipBLASLt development files, CMake 3.24+, a C++20 host compiler,
Git, and ~4 GB of disk. The front end alone needs only CMake and a C++20 compiler — not a GPU.

```sh
make build HIP_ARCHS=gfx1201          # engine + front end
make frontend                        # the front end alone, no HIP, no CUDA
make test                            # fake engine, no GPU, no model
```

Two configures, deliberately: the engine is built for HIP (`build-hip/`), the front end without any GPU
backend (`build-frontend/`). They share one source pin and one ggml; the front end has no link-time
relationship with the engine, because it spawns it.

## Run

The front end starts the engine as a child (`--engine`), adds `--serve` and speaks its line protocol.

```sh
build-hip/bin/arby-strata --engine build-hip/bin/strata \
  -m /models/swift-abliterated-iq3_xxs/...00001-of-00002.gguf \
  --host 0.0.0.0 --port 8080 --alias swift-abliterated-iq3_xxs \
  -- --pack /models/packs/swift-abliterated-iq3_xxs \
     --native /models/swift-abliterated-iq3_xxs/...00001-of-00002.gguf \
     --ple-gguf /models/swift-abliterated-iq3_xxs/...00001-of-00002.gguf \
     --expert-profile /models/packs/swift-abliterated-iq3_xxs/expert-profile.bin \
     --expert-cache auto --prefill auto --spec 4 --spec-min-p 0.5 --mtp /models/mtp/rt \
     --max-context 131768 --kv int8 --kv-resident 32768 --vram-reserve-mib 1400
```

Everything after `--` goes to the engine untouched. Images, when enabled, go with `--mmproj FILE` before the
`--` (llama.cpp's `mtmd` encodes them in the front end, on the CPU by default).

## Known limits

- **No constrained decoding.** The engine samples, so `response_format` and `tool_choice=required` are parsed
  for output parsing, not enforced at the sampler. Upstream has the same limit and logs it the same way.
- **One slot.** Requests are served one at a time in arrival order.
- **No Anthropic or Responses dialect, no MCP hub, no web UI, no Prometheus** — cut with the Python server.
  `/v1/chat/completions`, `/v1/models`, `/health` are what this speaks.
- The `tools/server` internal API compiled here is not llama.cpp's public surface. The pin is fixed; moving it
  is a deliberate act, not a `git pull`.

## Credits and licence

The engines are PolyStrata's, by VecSzn and contributors, built on [Strata](https://github.com/Niko1221/Strata)
by Niko1221. The front end follows [UnbiasedStrata](https://github.com/robertkarl/UnbiasedStrata) by robertkarl —
the C++ server built from llama.cpp's `tools/server`, its engine adapter and its fake-engine tests — adapted
here to a HIP build and this fork's engine. Tokenizer, chat templates, tool-call parsing and the HTTP layer are
[llama.cpp](https://github.com/ggml-org/llama.cpp)'s (MIT). See `UPSTREAM.md` for every change and why.
