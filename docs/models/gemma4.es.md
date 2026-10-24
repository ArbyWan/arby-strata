# Gemma 4 26B-A4B

Volver a [README.es.md](../../README.es.md)

**Gemma 4 26B-A4B**, un archivo de 11 a 17 GB. También entiende imágenes.

## ¿Qué velocidad tiene?

Medido igual que [Qwen3.6-35B-A3B](qwen36.es.md), en los mismos dos PC.

<table>
<tr><th>NVIDIA: RTX 4070 Laptop (8 GB), Core i7-14700HX, 32 GB de RAM</th><th>NVIDIA: RTX 5090 (32 GB), Xeon Platinum 8470Q (25 vCPU), 120 GB de RAM</th></tr>
<tr><td>

| Tamaño | Escribe respuestas | Lee tu prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 92-93 tokens/s | 2,240 tokens/s |
| **UD-Q2_K_XL** | 101-104 tokens/s | 2,150-2,160 tokens/s |
| **UD-Q4_K_M** | 85 tokens/s | 2,150 tokens/s |

</td><td>

| Tamaño | Escribe respuestas | Lee tu prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 353-356 tokens/s | 8,420-8,480 tokens/s |
| **UD-Q2_K_XL** | 332-338 tokens/s | 7,940-7,970 tokens/s |
| **UD-Q4_K_M** | 335-346 tokens/s | 8,320-8,390 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens se lee a 2,130 tokens/s en el portátil y a 6,800 en la estación de trabajo.

## Qué necesitas

| | |
| --- | --- |
| **Tarjeta gráfica** | Una tarjeta **NVIDIA** GeForce RTX serie 20 o más nueva con **8 GB de VRAM o más**. Unos 6 GB se ocupan antes del primer experto. Ha funcionado en una tarjeta de 32 GB, en esa tarjeta limitada a 8 GB y en la tarjeta de 8 GB de un portátil. En una tarjeta AMD también lo ejecuta, en beta, en Linux y en Windows: el instalador compila el motor en tu PC; probado hasta ahora con Qwen3.6 y Ornith en una RX 7600. |
| **RAM** | Unos **15, 18 o 21 GB** junto a una tarjeta de 8 GB para los tres tamaños, según la estimación del instalador. Según la misma estimación, una tarjeta de 16 GB guarda enteros los dos tamaños más pequeños. Los tres tamaños han funcionado en un portátil con 32 GB. |
| **Disco** | 11, 13 o 17 GB para el archivo del modelo y su modelo borrador, 1 GB más para leer imágenes. |
| **Sistema** | Linux o Windows 10 / 11, y un controlador de NVIDIA actual. |

El instalador se encarga de todo lo demás, como con GLM 5.3 Flash. En Windows descarga el motor ya compilado de la
última versión publicada. En un procesador sin AVX2, o en una copia cuyo motor cambió después de esa versión, lo
compila en su lugar con el compilador de 2022 de Visual Studio y el CUDA Toolkit, y ofrece instalarlos. Todavía no:
dos tarjetas en Windows, tres tarjetas o más. En una tarjeta AMD bajo Windows el instalador compila `strata-poly` para
la tarjeta, como en Linux (las wheels TheRock de AMD en `.rocm-win`), y avisa de que ejecutarlo allí es nuevo
([AMD_HIP.md](../AMD_HIP.md#strata-poly-on-amd-beta)).

## ¿Qué modelo elijo?

Tres tamaños por ahora. El instalador toma UD-Q3_K_M, o UD-Q2_K_XL en un PC con menos RAM. UD-Q4_K_M se elige por su nombre: `--model UD-Q4_K_M`.

| Tamaño | Qué es | Descarga | RAM junto a una tarjeta de 8 GB |
| --- | --- | ---: | ---: |
| **UD-Q3_K_M** | el modelo entero con expertos de 3 bits ([Unsloth](https://huggingface.co/unsloth/gemma-4-26B-A4B-it-GGUF)), y el modelo borrador de Google para él | 13 GB | unos 18 GB |
| **UD-Q2_K_XL** | el mismo con expertos de 2 bits: el que menos RAM pide | 11 GB | unos 15 GB |
| **UD-Q4_K_M** | el mismo con expertos de 4 y 5 bits | 17 GB | unos 21 GB |

Una tarjeta más grande guarda más parte del modelo y necesita menos RAM: con UD-Q3_K_M, unos 14 GB junto a una tarjeta de 12 GB y 10 GB cuando la tarjeta lo guarda todo. Son estimaciones del instalador. No se
han comprobado en un PC así.

## ¿Cómo funciona?

El diagrama y cómo funciona: [HOW_IT_WORKS_MODELS.es.md](../HOW_IT_WORKS_MODELS.es.md#gemma-4-26b-a4b).
