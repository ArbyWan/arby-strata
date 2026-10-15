# LFM2.5-24B-A2B (`strata-poly`)

`strata-poly`, the engine of GLM 5.3 Flash and the smaller models, also runs Liquid AI's LFM2.5-24B-A2B: llama.cpp's
architecture `lfm2moe`, read from the GGUF files of Unsloth and Liquid AI. The server, the web app and the API are the
ones the README describes.

It is new here. This project has run it on two machines: a Windows laptop whose card has 8 GB, and a Linux
workstation with a 32 GB card.

Its license is Liquid AI's LFM Open License v1.0, not an open-source one: commercial use is licensed only to
companies under 10 million US dollars of yearly revenue
([the license](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B/blob/main/LICENSE)).

## The sizes

| Size | From | Download | Experts | RAM beside an 8 GB card (setup's estimate) |
| --- | --- | ---: | ---: | ---: |
| **UD-Q4_K_XL** | [Unsloth](https://huggingface.co/unsloth/LFM2.5-24B-A2B-GGUF): 4-bit experts, 5- and 6-bit where they write | 14.9 GB | 14.0 GB | about 20 GB |
| **UD-Q3_K_XL** | Unsloth: 3-bit i-quant experts, 4-bit where they write | 10.9 GB | 10.2 GB | about 16 GB |
| **QAD-Q4_0** | [Liquid AI](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B-GGUF): Q4_0, trained for it (quantization-aware distillation) | 13.6 GB | 12.9 GB | about 18 GB |
| **UD-Q6_K_XL** | Unsloth: 6-bit experts, 8-bit where they write | 21.9 GB | 20.9 GB | about 27 GB |
| **Q8_0** | Unsloth: 8-bit throughout | 25.5 GB | 24.4 GB | about 31 GB |

Setup takes UD-Q4_K_XL, or UD-Q3_K_XL when the PC has too little RAM for it; the others are chosen by name
(`--model QAD-Q4_0`). Every file is pinned to its repository's commit of 2026-10-15 and checked against its SHA-256.

## How fast

The README's numbers are PolyStrata's own benchmark through the server, the test every model of this engine gets
([BACKENDS.md](BACKENDS.md#a-prompt-of-30000-tokens)): `polystrata.py benchmark --prompt-tokens 24000`, a short chat
and a prompt built from this repository's documents, 256 tokens written after each, three runs a size and the last
two below. The engine is d74c5e25's, with the run configs setup writes for each machine.

### On a laptop with 8 GB of VRAM

An RTX 4070 Laptop with 8 GB, an i7-14700HX, 32 GB of RAM, Windows 10, CUDA 13.0. A context of 32,768 and 13 threads
for the experts the card does not hold. The prompt came to 29,759 tokens.

| Size | First token, s | Reads, tokens/s | Writes a short chat | Writes after the prompt | Experts from VRAM | Machine's RAM, GiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| UD-Q4_K_XL | 9.6 | 3,118 to 3,124 | 68 to 70 | 69 to 74 | 55% to 56% | 22.1 to 22.6 |
| UD-Q3_K_XL | 9.0 to 9.1 | 3,271 to 3,301 | 88 | 72 to 74 | 71% to 73% | 18.4 to 18.9 |
| QAD-Q4_0 | 9.3 to 9.4 | 3,192 to 3,220 | 75 to 78 | 76 to 79 | 63% | 20.8 to 21.3 |
| UD-Q6_K_XL | 11.3 | 2,642 to 2,645 | 42 to 44 | 51 to 53 | 38% | 28.9 to 29.2 |
| Q8_0 | 11.5 to 12.6 | 2,377 to 2,598 | 21 to 25 | 44 to 48 | 32% | 31.2 |

Q8_0 fills the machine's RAM, and part of its file is read from the disk again; that is what makes it the slowest
here. Two passes before this one, with engines from before d74c5e25, read the prompt at 1,290 to 1,728 tokens/s. A
README cell has the highest of the three: one of the earlier two wrote the short chat a little faster with
UD-Q4_K_XL, QAD-Q4_0, UD-Q6_K_XL and Q8_0.

### On a workstation

An RTX 5090 (32 GB), 25 vCPU of a Xeon Platinum 8470Q, 120 GB of RAM, Linux, CUDA 12.8. The card holds every expert
of every size, so setup's config reads a prompt 2,048 tokens at a time there. The prompt came to 29,816 tokens.

| Size | First token, s | Reads, tokens/s | Writes a short chat | Writes after the prompt | Experts from VRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| UD-Q4_K_XL | 1.5 | 21,312 to 21,325 | 473 to 475 | 602 to 603 | 100% |
| UD-Q3_K_XL | 1.5 | 21,127 to 21,166 | 473 to 474 | 528 to 535 | 100% |
| QAD-Q4_0 | 1.5 | 21,653 to 21,666 | 497 | 412 to 413 | 100% |
| UD-Q6_K_XL | 1.6 | 19,200 to 19,342 | 419 to 420 | 360 to 361 | 100% |
| Q8_0 | 1.5 | 21,244 to 21,274 | 370 | 321 | 100% |

The answer after the prompt goes faster than the short chat where it repeats the prompt's text and the engine's
guesses from it are right. One token at a time (`--window 1`), UD-Q4_K_XL writes at 452 tokens/s after a short
prompt and at 350 after 29,816 tokens. Two passes before this one, with 165bb4da's and 4f5947f5's engines and a
prompt batch of 1,024, read the prompt at 10,007 to 10,854 tokens/s and wrote the short chat at 372 to 498 tokens/s;
a README cell has the highest of the three.

### Against llama.cpp

On the laptop, UD-Q4_K_XL through each server with the same prompts
([the runs](../bench/results/2026-10-15-laptop-lfm25-llamacpp-b11540/README.md)):

| | PolyStrata | llama.cpp b11540, `--n-cpu-moe 25` | llama.cpp b11540, `--fit on` |
| --- | ---: | ---: | ---: |
| Reads about 4,000 tokens | 1,971 to 1,988 tokens/s | 498 to 505 | 483 to 485 |
| Writes after them | 91 to 95 tokens/s | 57.7 to 58.0 | 55.1 to 55.3 |
| Reads about 32,000 tokens | 3,103 to 3,123 tokens/s | 528 to 537 | 505 to 516 |
| Writes after them | 69 to 71 tokens/s | 52.8 to 53.0 | 50.9 to 51.0 |

`--n-cpu-moe 25` was the fastest of the settings tried for llama.cpp there. On the workstation, `llama-bench` of the
llama.cpp that [Strata](https://github.com/Niko1221/Strata) v0.1.39 vendors, every layer on the card and flash
attention on:

| | PolyStrata | llama.cpp, batch of 512 (its default) | llama.cpp, batch of 2,048 |
| --- | ---: | ---: | ---: |
| Reads 29,816 tokens | 21,070 tokens/s (16,100 with a batch of 1,024) | 13,311 | 23,614 |
| Writes, one token at a time | 452 tokens/s | 455 | |
| Writes after 29,816 tokens, one at a time | 350 tokens/s | 369 | |

With a batch of 2,048 llama.cpp reads the prompt 12% faster than PolyStrata, and after a long prompt it writes 5%
faster one token at a time. PolyStrata's guesses make up the second where the answer repeats the prompt.

## How it was checked

- The tokenizer gives the same ids as llama.cpp's `llama-tokenize` on 174,442 tokens: three READMEs (English,
  Chinese, Japanese), BACKENDS.md, a Python file and a CUDA file.
- With greedy sampling the engine and llama.cpp on the CPU write the same tokens until two candidates are close: 1,
  39 and 10 of 64 tokens on three short prompts, 20 on a prompt of 17,575 tokens. llama.cpp's CUDA build parts
  from its CPU build at 5, 40 and 23 tokens on the same three. While they agree on the long prompt, the
  probabilities are 0.066 apart at most.
- A window of guesses writes the same tokens as one token at a time, with the experts on the CPU: 300 tokens after
  a prompt of 26 tokens and after one of 4,259.
- `poly_attend_test`, `poly_norm_q8_test`, `poly_shortconv_test` and `poly_route_test` check the kernels written for
  it against the ones they stand for; `serve/test_lfm2.py` the tool calls and the chat template.

## How the sizes compare

Q8_0 wrote 256 greedy tokens on each of five prompts (code, a summary of 4,260 tokens of BACKENDS.md, arithmetic,
a story, a question in Chinese); each other size then followed those tokens, and the share of steps where its most
likely token is Q8_0's was counted. Q8_0 stands in for the model as trained here.

| Size | Same most likely token as Q8_0 | Mean probability of Q8_0's token |
| --- | ---: | ---: |
| UD-Q6_K_XL | 96.8% (1,239 of 1,280) | 0.782 |
| UD-Q4_K_XL | 95.5% (1,222 of 1,280) | 0.781 |
| QAD-Q4_0 | 92.7% (1,186 of 1,280) | 0.762 |
| UD-Q3_K_XL | 91.6% (1,173 of 1,280) | 0.766 |

## What was tuned for it

The decode numbers are UD-Q4_K_XL on the RTX 5090 with the engine's guesses, after a short prompt and after one of
4,300 tokens, tokens/s:

| Commit | Change | Effect |
| --- | --- | --- |
| d4d5049c | the model runs | 386 and 347 |
| 6af34f82 | a layer's residual sum, RMS norm and Q8_1 quantization in one launch | 401 and 359 |
| 962da9b9 | the router's kernel with as few slots a lane as 64 experts need | 405 and 368 |
| 72a738f6, 165bb4da | with every expert on the card, the card picks a few tokens' experts itself, and a token's 40 layers run as one recorded launch | 454 and 408 |
| 4f5947f5 | the attention of a few tokens in blocks of 64 rows, a launch block each | one token at a time after 4,300 tokens 381 to 441, after 30,000 196 to 350; the laptop after 30,000 49 to 56 |
| d74c5e25 | a prompt batch's attention in registers, the way FlashAttention-2 goes | a prompt of 29,816 tokens read in 1,852 ms instead of 2,787 on the 5090, in 8.9 s instead of 16.8 on the laptop |
| 0ec4802d | setup's config reads 2,048 tokens at a time on a card that holds every expert | 1,415 ms instead of 1,852 |

## What you need

- An NVIDIA card, RTX 20 or newer, with 8 GB of VRAM or more. About 4 GB go to the weights outside the experts, the
  caches and the buffers at a 32K context; the rest holds experts. A 24 GB card holds the three smaller sizes
  whole, by setup's estimate.
- RAM for the experts the card does not hold. They are read from the mapped file while the model answers, so they
  should stay in the OS file cache: setup's estimate is in the table above. All five sizes have run on the laptop
  with 32 GB, but Q8_0 fills it and is slow there.
- Linux or Windows 10 / 11. AMD cards: the engine compiles for them on Linux, and this model has not been tried on
  one; setup says so.

## Install

```
./setup.sh --family lfm25
```

On Windows, `START-HERE.bat` and pick it from the list. Setup asks for the size, the context and whether to keep
several conversations in RAM, downloads the file, checks its SHA-256, writes the tokenizer and the run config
`strata-lfm25-<size>.json`, and starts the model. `--model UD-Q3_K_XL` (or another size) skips the question, and
`--gguf-dir <folder>` uses a file you already have, under its name from the repository.

## How it differs from the other models

- **Most layers have no attention.** 30 of its 40 layers mix the tokens with a short convolution over the last
  three: each projects a token to three vectors B, C and x, convolves B times x, and scales the result by C. Its
  state is two vectors a layer. The other 10 layers attend over the whole conversation, with 8 key heads of 64
  values, so a context token takes 20,480 bytes of VRAM: a 128K context takes 2.0 GB more than 32K.
- **The experts.** 38 layers have 64 routed experts each, of which a token uses 4, with sigmoid gates, a bias that
  steers the pick and weights that sum to one. There is no shared expert. The first two layers have a plain FFN.
  The output matrix is the token table.
- **No draft block.** The files have nothing that guesses ahead, so the engine guesses only from text the
  conversation repeats, as for DeepSeek V4 Flash.
- **Tool calls** come as a Python list between two tokens, `<|tool_call_start|>[f(a='x'), g(b=1)]<|tool_call_end|>`,
  several calls in one; the server turns them into OpenAI's and Anthropic's tool calls.
- **Thinking.** The template always opens a `<think>` block; with thinking off the server closes it at once, as for
  GLM. Liquid AI's model card suggests temperature 0.1, top-k 50 and a repetition penalty of 1.05; setup does not
  set them. To use them for clients that send no sampling of their own, add
  `"sampling": {"temperature": 0.1, "top_k": 50, "repetition_penalty": 1.05}` to the run config.

## What is missing

- Pictures: the model reads text only.
- A draft model. Liquid AI publishes none for it.
- Contexts past 128K: it was trained for 131,072 tokens, and setup offers up to that.
