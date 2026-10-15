# LFM2.5-24B-A2B UD-Q4_K_XL on the 8 GB laptop: PolyStrata and llama.cpp b11540 (2026-10-15)

RTX 4070 Laptop (8 GB), i7-14700HX, 32 GB of RAM, Windows 10. `bench/bench_vs_llama.py run --machine <file>
--rounds 3`: each server in turn, a warm-up, then prompts of about 4,000 and 32,000 tokens (`prompt-4k.txt`,
`prompt-32k.txt`) with a 36,864 context, at temperature 0 and 0.7, and a summary of the text above asked for.
`summary.txt` is `bench_vs_llama.py summary` over the folder; `raw/` has every request.

| Config | Engine | Settings |
| --- | --- | --- |
| `polystrata-setup` | strata-poly of 165bb4da | the run config setup writes for this laptop (`strata-cfg-polystrata-setup.json`) |
| `polystrata-split` | 4f5947f5: attention in blocks of rows | the same config |
| `polystrata-flash` | d74c5e25: a prompt batch's attention in registers | the same config |
| `llama-tuned` | llama-server b11540 | `-ngl 99 --n-cpu-moe 25 -fa on -ctk q8_0 -ctv q8_0 -t 20 -ub 512`, the fastest of the settings tried |
| `llama-fit` | llama-server b11540 | `--fit on -fa on -ctk q8_0 -ctv q8_0` |

The three PolyStrata configs ran the same file and config with the engine of each commit
(`machine-laptop.json`, `machine-laptop-split.json`, `machine-laptop-flash.json`). With the last engine:

| | PolyStrata (d74c5e25) | llama.cpp, `llama-tuned` | llama.cpp, `llama-fit` |
| --- | ---: | ---: | ---: |
| Reads about 4,000 tokens | 1,971 to 1,988 tokens/s | 498 to 505 | 483 to 485 |
| Writes after them | 91 to 95 tokens/s | 57.7 to 58.0 | 55.1 to 55.3 |
| Reads about 32,000 tokens | 3,103 to 3,123 tokens/s | 528 to 537 | 505 to 516 |
| Writes after them | 69 to 71 tokens/s | 52.8 to 53.0 | 50.9 to 51.0 |
| First token after 32,000 | 10.0 s | 57.8 to 58.8 s | 60.2 to 61.5 s |
