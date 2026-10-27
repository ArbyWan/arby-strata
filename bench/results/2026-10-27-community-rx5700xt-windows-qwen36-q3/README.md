# RX 5700 XT on Windows: Qwen3.6 UD-Q3_K_M, 32K suite and 64K/128K retrieval

One RX 5700 XT successfully ran the official default benchmark suite and two
separate long-prompt retrieval probes using the real `gfx1010` HIP target.
This is a community report of an unvalidated device, not a support-list change.
The top-level setup GPU allowlist rejected this card; a small adapter invoked
unchanged upstream `poly_setup` with the actual measured GPU metadata.

## Hardware and build

| Component | Tested configuration |
|---|---|
| GPU | Radeon RX 5700 XT, 8176 MiB reported VRAM, actual `gfx1010:xnack-` |
| CPU | Xeon W-2145, 8 cores / 16 threads, nominal 3.70 GHz |
| RAM | 256 GiB installed (8 x 32 GiB DDR4-2400); OS reports 255.6 GiB usable |
| Storage | SSD; disk/model cache was not flushed between tests |
| OS / driver | Windows 11 Pro, build 10.0.26200 / AMD 32.0.21045.16010 |
| PolyStrata | `cc508adc50c32e0a98bd9bc1c5688ab5195b2538`, poly.5-equivalent engine/build/setup source |
| Release comparison | Engine/build/setup identical to `v0.1.45-poly.5` (`bfd8911efbb8c27214d5aabc511907c6149171e7`); executable reports `0.1.45` |
| Toolchain | Python 3.12.10; VS 2022 Build Tools 17.14.42; CMake 4.4.3; Ninja 1.13.2 |
| HIP runtime | Official AMD TheRock Windows `gfx1010` package, `10.2.0a20260930` |
| Model | `unsloth/Qwen3.6-35B-A3B-MTP-GGUF`, `Qwen3.6-35B-A3B-UD-Q3_K_M.gguf`, 17,104,402,720 bytes |

The model revision, SHA256 hashes, dependency commit and runtime inventory are
in [provenance.json](provenance.json). No driver replacement, reboot, GPU
architecture spoofing or engine/kernel patch was used. PCIe link state, power,
temperature, background load and clock behavior were not controlled or measured.

## Settings and what ran on the GPU

The upstream recommended Qwen3.6 Q3 configuration used HIP, FP16 KV,
`--kv-whole`, `--tail-skip 6`, automatic conversation cache (8192 MiB cap),
embedded MTP and 16 CPU workers. Requests used temperature 0 and thinking off.
`STRATA_SLOTS_WARP` is the
HIP default; `STRATA_HIP_VALUE_SPLIT` was not enabled.
The sanitized [configs](configs/) retain the actual arguments; replace
`<workspace>` with an installation directory before use.

This is hybrid execution: dense layers and attention use the GPU, with CPU
expert work and GPU expert caching/staging. At 128K, the engine admitted zero
persistent GPU expert slots; decode expert math used the CPU while prompt
batches staged experts through GPU MMQ. Tail skip 6 approximates inference by
omitting uncached experts ranked 7/8 and can change answers. These results do
not establish quality equivalence to full-expert execution.

Only `--max-context` changed for the 64K/128K configurations. No quantized-KV
fallback or WDDM budget override was used. Observed WDDM budgets were about
7383 MiB for the baseline and approximately 6378 MiB for the longer contexts. Thus the
cross-context speed differences also include a budget/cache-layout confound.

## Official benchmark, configured context 32768

The unmodified `tools/strata_bench.py` ran against an already started server:

```powershell
python tools/strata_bench.py --url http://127.0.0.1:18085 --runs 3
```

Default suite, no `--long`, no parallel clients, thinking off. The suite took
322.287 seconds: 33 measured requests, zero failed, zero skipped. The original
tool-sanitized [results.json](results.json) is included byte-for-byte.
Calibration/setup requests are outside that measured request count. Chat consists
of three different questions repeated three times, each capped at 100 tokens.
The three basic probes below ran on this same server before the suite's warm-up.

| Workload | Samples | Actual input tokens | Median engine tok/s (range) | Median wall TTFT, seconds (range) |
|---|---:|---:|---:|---:|
| Short chat | 9 requests | See individual requests | Decode 31.4 (27.9–37.2) | 0.711 (0.651–0.767) |
| 4K prompt | 3 requests | 4019; zero reused | Prompt 516.7 (515.7–518.3) | 7.797 (7.761–7.800) |
| 16K prompt | 3 requests | 16058; zero reused | Prompt 510.6 (510.5–511.6) | 31.468 (31.428–31.470) |
| Agent follow-ups | 15 requests, across 3 six-turn sessions | Last-turn median 15343 | Decode 30.3 (23.3–37.0) | 8.387 (4.362–10.661) |

Agent sessions contain 18 requests including the three initial turns. Follow-up
cache-hit median was 65.956%. Engine prompt/decode rates use engine-reported
phase timings; TTFT is measured by the client and includes waiting/prompt work.
Decode tok/s alone does not represent end-to-end long-prompt speed.

## Separate correctness and longer-context probes

Three basic synthetic checks passed: arithmetic returned `391` (29 input
tokens); sorting returned `{"sorted": [1, 2, 7, 9]}` (40); a repeated-notes
retrieval returned the planted synthetic code `MAPLE-7392` (2789).
Exact fixtures, responses and timings are in [basic-checks.json](basic-checks.json).

Each longer-context row is **one request**, retrieving three planted codes at
the beginning, middle and end of seeded random-word filler. Output cap was
128 tokens; each generated 48 tokens with zero reused input tokens.
All three codes matched exactly in each row. The official suite was not rerun
at these context settings.

| Configured context | Actual input | Engine prompt tok/s | Engine decode tok/s | Wall TTFT | Wall total | Result |
|---:|---:|---:|---:|---:|---:|---|
| 65536 | 58117 | 345.4 | 28.1 | 168.324 s | 170.028 s | 3/3 codes |
| 131072 | 116115 | 232.8 | 22.8 | 498.798 s | 500.899 s | 3/3 codes |

Per-request data: [context64.json](context64.json), [context128.json](context128.json).
These probes establish successful admission and bounded retrieval at the actual
input lengths; they do not establish full-window capacity, broad quality,
multi-user stability or useful speed for every task. No larger context was tested.

## 128K system memory

[memory-summary.json](memory-summary.json) retains aggregate byte counts from
260 RAM samples at a target cadence of 2 seconds over 523.445 seconds, plus 96
GPU counter samples. The GPU monitor initially failed to launch under the shell
script policy; an encoded invocation worked without changing system policy,
leaving a startup gap. These are sampled peaks, not guaranteed instantaneous maxima.

| Counter | Observed value |
|---|---:|
| Whole-system RAM before launch | 17.731 GiB |
| Whole-system RAM sampled peak | 36.014 GiB |
| Increase above baseline | 18.283 GiB |
| Whole-system RAM after cleanup | 17.800 GiB |
| Engine sampled working set / private commit | 17.944 / 2.470 GiB |
| Whole-system sampled commit peak | 28.782 GiB |
| GPU sampled dedicated / shared memory | 6.133 / 1.723 GiB |

Whole-system changes include other applications and monitors. Mapped working
set, private commit, GPU shared memory and cache can overlap; do not add them.
The 8192 MiB conversation-cache cap is not a measurement of RAM usage.
No reliable system-RAM peak was captured for 32K or 64K. All owned inference
workers were stopped after the tests. No endurance or thermal test was performed.

## Reproduction and retained failures

Use a clean checkout of the pinned commit, Python 3.12, VS C++ Build Tools and
the official AMD TheRock Windows `gfx1010` SDK/runtime matching the version
above. The original setup installed that runtime into the checkout's
`.rocm-win` environment. Confirm a HIP device probe reports the actual
`gfx1010:xnack-` target before building; do not substitute another architecture.
The dependency pin is in provenance.json. This report does not claim that an
arbitrary newer runtime or driver reproduces these measurements.

[setup-gfx1010.py](setup-gfx1010.py) is the small allowlist-bypass adapter used
for this machine, generalized to accept local source/workspace paths. For the
included configs, use `--source <workspace>/source --workspace <workspace>`.
Place the pinned, SHA256-verified GGUF in `<workspace>/models` first; this adapter
uses `--gguf-dir` and does not download the GGUF. It calls
upstream setup/build routines with the actual device and recommended answers;
it is not a proposed installer change. Setup may download supporting packs and
build dependencies before compiling. Read it before running, and use it only
for the reported hardware.

The first source-archive build attempt failed because the archive omitted the
upstream `tests/` directory. Restoring that unchanged directory from the same
commit resolved it; the subsequent build succeeded. This packaging
failure and the earlier top-level allowlist rejection are not GPU kernel failures.
No failed measured inference request occurred. Private full logs are retained
locally and deliberately excluded from this submission.

After starting a server with the appropriate context configuration:

```powershell
# Verify fixed synthetic fixtures offline, without starting inference:
python reproduce.py --verify-only
# Use tools/strata_bench.py from the pinned source tree for the same HTTP client:
python reproduce.py --source <checkout> --context 65536 --url http://127.0.0.1:18085
python reproduce.py --source <checkout> --context 131072 --url http://127.0.0.1:18085
```

The fixed word counts reproduce the original HTTP prompt strings, with hashes
checked before sending. Do not recalibrate them when reproducing these exact
fixtures. Actual tokenizer counts must be read from the returned API usage.
No comparison against another quantization, runtime or release is claimed.

The tests ran sequentially and reused the same expert-profile path; the engine
updates that profile during use. Exact profile contents were not frozen or hashed
at each phase boundary. Consequently the fixtures reproduce the prompt strings,
but cannot reproduce the exact initial expert/cache state of every measured run.
Zero reused prompt tokens does not mean a cold model, disk cache or expert profile.

Privacy: only synthetic prompts are published. Usernames, hostnames, user paths,
LAN addresses, process IDs, adapter identifiers and raw monitoring/log archives
are excluded; numerical measurements and artifact hashes are retained.
