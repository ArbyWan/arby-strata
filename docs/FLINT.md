# Flint 40B-A2.5B (`strata-poly`)

`strata-poly`, the engine of GLM 5.3 Flash and the smaller models, also runs Recursive's Flint 40B-A2.5B: llama.cpp's
architecture `flint`, read from Unsloth's GGUF files. The server, the web app and the API are the ones the README
describes.

Recursive trained Flint from scratch for coding agents: 39.8 billion parameters, of which a token uses about 2.5
billion ([its model card](https://huggingface.co/RecursiveSI/flint)). It is new here. This project has run it on two
machines: a Windows laptop whose card has 8 GB, and a Linux workstation with a 32 GB card. Its license is Apache 2.0.

## The sizes

| Size | From | Download | Experts | RAM beside an 8 GB card (setup's estimate) |
| --- | --- | ---: | ---: | ---: |
| **UD-Q4_K_XL** | [Unsloth](https://huggingface.co/unsloth/flint-GGUF): experts in IQ4_XS and Q5_K | 25.1 GB | 23.1 GB | about 29 GB |
| **UD-Q3_K_XL** | Unsloth: experts in IQ3_XXS and IQ3_S, IQ4_XS where they write | 18.5 GB | 16.5 GB | about 22 GB |
| **UD-Q6_K_XL** | Unsloth: experts in Q6_K, Q8_0 where they write | 36.3 GB | 34.2 GB | about 40 GB |
| **Q8_0** | Unsloth: 8-bit throughout | 42.3 GB | 40.2 GB | about 46 GB |

Setup takes UD-Q4_K_XL where the PC has about 15 GB of RAM past that estimate (about 40 GB beside an 8 GB card),
else UD-Q3_K_XL; the others are chosen by name (`--model Q8_0`). Every file is pinned to the repository's commit of
2026-10-21 and checked against its SHA-256.

## How fast

The README's numbers are PolyStrata's own benchmark through the server, the test every model of this engine gets
([BACKENDS.md](BACKENDS.md#a-prompt-of-30000-tokens)): `polystrata.py benchmark --prompt-tokens 24000`, a short chat
and a prompt built from this repository's documents, 256 tokens written after each, three runs a size and the last
two below, with the run configs setup writes for each machine.

### On a laptop with 8 GB of VRAM

An RTX 4070 Laptop with 8 GB, an i7-14700HX, 32 GB of RAM, Windows 10, CUDA 13.0. A context of 32,768 and 13 threads
for the experts the card does not hold, and a prompt read 2,048 tokens at a time. The prompt came to 28,827 tokens;
for Q8_0 and the last run of UD-Q6_K_XL, 28,968, after this repository's docs had changed.

| Size | First token, s | Reads, tokens/s | Writes a short chat | Writes after the prompt | Experts from VRAM | Machine's RAM, GiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| UD-Q4_K_XL | 27.8 to 31.6 | 916 to 1,039 | 20 to 30 | 38 to 39 | 40% | 31.5 |
| UD-Q3_K_XL | 25.0 | 1,155 | 61 to 63 | 45 | 49% to 50% | 29.2 to 29.4 |
| UD-Q6_K_XL | 46.0 to 46.2 | 631 to 632 | 7 to 8 | 6 to 7 | 29% | 31.7 to 31.8 |
| Q8_0 | 52.8 to 54.0 | 544 to 554 | 4 to 5 | 5 to 6 | 27% to 28% | 31.5 to 31.6 |

Only UD-Q3_K_XL stays in the laptop's RAM beside the desktop's programs. The other three fill the 32 GB, and the
experts that drop out of the OS file cache are read from the SSD again while the model answers: UD-Q4_K_XL's short
chat came at 14 to 30 tokens/s over its three runs, and UD-Q6_K_XL and Q8_0 are of little use there. That is why
setup takes UD-Q4_K_XL only from about 40 GB of RAM beside an 8 GB card.

Read 1,024 tokens at a time, as setup did here before Flint got a batch of 2,048, the prompt went at 840 to 901
tokens/s with UD-Q3_K_XL and 715 to 777 with UD-Q4_K_XL. With 2,048 the experts outside VRAM are copied to the card
half as often, and the answer after the prompt came no slower: 45 tokens/s against 39 to 42 with UD-Q3_K_XL.

### On a workstation

An RTX 5090 (32 GB), 25 vCPU of a Xeon Platinum 8470Q, 90 GB of RAM for the container, Linux, CUDA 12.8. The card
holds every expert of UD-Q3_K_XL and UD-Q4_K_XL; of Q8_0 it holds 8,447 of the 12,032 at a 32K context, and the CPU
runs the rest. A prompt is read 2,048 tokens at a time.

| Size | First token, s | Reads, tokens/s | Writes a short chat | Writes after the prompt | Experts from VRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| UD-Q4_K_XL | 3.6 | 8,283 to 8,316 | 249 to 250 | 144 | 100% |
| UD-Q3_K_XL | 3.6 | 8,315 to 8,360 | 260 to 261 | 143 | 100% |
| Q8_0 | 5.4 | 5,455 to 5,463 | 140 to 149 | 103 to 106 | 88% to 89% |

UD-Q6_K_XL did not fit on the workstation's disk beside the other three, and was not run there.

### Against llama.cpp

On the workstation, `llama-bench` of llama.cpp a37cc46104 (the first with Flint), every layer on the card and flash
attention on, UD-Q4_K_XL:

| | PolyStrata | llama.cpp, batch of 512 (its default) | llama.cpp, batch of 2,048 |
| --- | ---: | ---: | ---: |
| Reads 29,816 tokens | 7,842 to 8,154 tokens/s (7,323 to 7,379 with a batch of 1,024) | 7,794 | 12,470 |
| Writes, one token at a time | 250 to 253 tokens/s | 222 | |
| Writes after 29,816 tokens, one at a time | 132 to 135 tokens/s | 188 | |

With a batch of 2,048 llama.cpp reads the prompt 1.5 to 1.6 times as fast as PolyStrata. One token at a time
PolyStrata writes 13% to 14% faster after a short prompt and 28% to 30% slower after 29,816 tokens: Flint's heads have
128 values and a sink, and its attention runs on the engine's general kernels, not on the ones written for heads of
64. Through the server, with its guesses from text the conversation repeats, it wrote 249 to 250 tokens/s in the
short chat and 144 after the long prompt.

## Long contexts

Flint's model card gives it a context of 262,144 tokens; setup offers up to 131,072. The test is the one DEEPSEEK.md
has: a document of this repository's docs with three facts planted at 15%, 50% and 85% of it, then three questions.
On the workstation, UD-Q4_K_XL with a context of 131,072 read and answered the 124,122-token request through the
server in 39.6 s with setup's batch of 2,048 (41.0 s with 1,024) and found all three facts both times. Flint always
thinks, and its thinking quoted the three sentences before it answered; it wrote 171 and 177 tokens in all.

## How it was checked

- The tokenizer gives the same ids as llama.cpp's `llama-tokenize` on 195,861 tokens: three READMEs (English,
  Chinese, Japanese), BACKENDS.md, a Python file and a CUDA file.
- With greedy sampling and the expert cache fixed, the engine and llama.cpp with its experts on the CPU write the
  same tokens on two short prompts, 34 of 34 and 64 of 64, on the laptop and on the workstation. After the first 100,
  600, 1,500 and 3,740 tokens of a longer prompt they agree on the first 3 to 17 of 24 tokens (once on all 24), and
  part each time where llama.cpp's two or three most likely tokens were 0.17 to 0.74 nats apart.
- Three answers written together are the ones written one at a time, all 64 tokens of each, after a prompt of 15
  tokens and one of 3,000, with every expert on the card and with the card held to 8 GB.
- `poly_sink_attend_test` checks the attention of a prompt batch against a token's, with the sinks and a window of
  513 over a ring of rows; `serve/test_flint.py` checks the pre-tokenizer, the tool calls, the stop tokens and the
  thinking.

## How the sizes compare

Q8_0 wrote 256 greedy tokens on each of five prompts (code, a summary of 4,987 tokens of BACKENDS.md, arithmetic,
a story, a question in Chinese); each other size then followed those tokens, and the share of steps where its most
likely token is Q8_0's was counted. Q8_0 stands in for the model as trained here. UD-Q4_K_XL and UD-Q3_K_XL were
compared on the workstation; UD-Q6_K_XL, which did not fit there, on the laptop against the laptop's own run of Q8_0.

| Size | Same most likely token as Q8_0 | Mean probability of Q8_0's token |
| --- | ---: | ---: |
| UD-Q6_K_XL | 97.9% (1,253 of 1,280) | 0.858 |
| UD-Q4_K_XL | 96.4% (1,234 of 1,280) | 0.857 |
| UD-Q3_K_XL | 93.5% (1,197 of 1,280) | 0.839 |

Q8_0 gives its own tokens 0.861 on average on the workstation and 0.857 on the laptop.

## What you need

- An NVIDIA card, RTX 20 or newer, with 8 GB of VRAM or more. About 4 GB go to the weights outside the experts, the
  caches and the buffers at a 32K context; the rest holds experts. A 32 GB card holds UD-Q4_K_XL whole.
- RAM for the experts the card does not hold. They are read from the mapped file while the model answers, so they
  should stay in the OS file cache: setup's estimate is in the table above. On the 32 GB laptop only UD-Q3_K_XL
  stayed there beside the desktop's programs.
- Linux or Windows 10 / 11. AMD cards: the engine compiles for them on Linux and under Windows as well (AMD's TheRock
  wheels); this model has not been tried on one, and setup says so.

## Install

```
./setup.sh --family flint
```

On Windows, `START-HERE.bat` and pick it from the list. Setup asks for the size, the context and whether to keep
several conversations in RAM, downloads the file, checks its SHA-256, writes the tokenizer and the run config
`strata-flint-<size>.json`, and starts the model. `--model UD-Q3_K_XL` (or another size) skips the question, and
`--gguf-dir <folder>` uses a file you already have, under its name from the repository.

## How it differs from the other models

- **Most layers look back 513 tokens.** 35 of its 49 layers attend to the current token and the 512 before it, with
  RoPE; their keys and values are a ring of rows that does not grow with the conversation. The other 14 attend over
  the whole conversation and have no position encoding at all. 4 key heads of 128 values serve the 32 query heads,
  so a context token takes 28,672 bytes of VRAM: a 128K context takes 2.6 GB more than 32K.
- **Attention sinks.** Each head has a learned value that joins its softmax like the score of one more key with no
  value, so a head can give most of its weight to nothing. The engine adds it to the softmax of a token and of a
  prompt batch.
- **The experts.** 47 layers have 256 routed experts each, of which a token uses 6, with sigmoid gates and weights
  that sum to one, and a shared expert that every token uses. The first two layers have a plain FFN. The output matrix
  is its own, not the token table.
- **Large activations.** In layer 46 the input of the down experts reaches 45,000, and a block of 32 of them sums to
  92,660, past the largest half-precision number (65,504). The engine reads a prompt batch with llama.cpp's matrix
  kernels, which keep that sum in half precision for Q4_K and Q5_K, so it became infinite and the answer a row of
  token 0. For Flint the engine scales such a row down by a power of two before it is quantized and back up after the
  product, which changes no bit of the result. The other models' sums stay in range, and they skip the check: it
  cost LFM2.5 4% of a 30K prompt's read.
- **No draft block.** The files have nothing that guesses ahead, so the engine guesses only from text the
  conversation repeats, as for LFM2.5 and DeepSeek V4 Flash.
- **Tool calls** come as GLM's: `<tool_call>NAME<arg_key>K</arg_key><arg_value>V</arg_value></tool_call>`. The server
  turns them into OpenAI's and Anthropic's tool calls, and stops an answer at `<|user|>`, `<|observation|>` and
  `<|developer|>` as well as at the end token, as llama.cpp does.
- **Thinking.** The model opens its `<think>` block itself and thinks whatever a request says: its template has no
  switch for it. The server returns the thinking as `reasoning_content`; Recursive asks clients to send it back in
  the history, and the template puts it there. The model card suggests temperature 1.0, top-p 0.95 and top-k 20;
  setup does not set them. To use them for clients that send no sampling of their own, add
  `"sampling": {"temperature": 1.0, "top_p": 0.95, "top_k": 20}` to the run config.

## What is missing

- Pictures: the model reads text only.
- A draft model. Recursive publishes none.
- Contexts past 128K in setup, although the model card gives 262,144: not tried here.
- The engine's own work for it. The attention runs on the general kernels, without the ones written for 64-value
  heads, and writing one token at a time after a long prompt is 28% to 30% slower than llama.cpp's.
