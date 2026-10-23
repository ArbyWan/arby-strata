<h1 align="center">PolyStrata</h1>

[English](README.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md) · [Deutsch](README.de.md) · **Français** · [Español](README.es.md) · [Português](README.pt-BR.md)

<p align="center"><b>Faites tourner de grands modèles d'IA sur votre propre PC</b><br>
Qwen3.8-Flash-Next, GLM 5.3 Flash, DeepSeek V4 Flash, LFM2.5, Qwen3.6, Ornith 1.5, Gemma 4, Tiel Coder et MiniCPM-V · gratuit et open source</p>

<p align="center"><img src="docs/media/speed.svg" width="820" alt="Tokens par seconde pendant l'écriture d'une réponse : 94 à 100 avec Qwen3.6-35B-A3B sur un portable à carte 8 Go, 394 à 395 sur une RTX 5090"></p>

<p align="center"><img src="docs/media/claude-code-demo.gif" width="820" alt="Claude Code dans un terminal à côté de l'API Monitor de PolyStrata"><br>
<sub>Claude Code avec Qwen3.6-35B-A3B UD-Q3_K_M sur le portable à carte de 8 Go, en temps réel : il ajoute une option <code>--json</code> à un petit programme, écrit un test et lance les tests. Le serveur avait lu le prompt système de Claude Code à son démarrage (<code>"warm_start": true</code>).</sub></p>

<p align="center"><a href="https://github.com/Niko1221/Strata/releases/download/v0.1.10/Pagoda.mp4"><img src="docs/media/pagoda-preview.webp" width="720" alt="Un jardin de pagode en voxels écrit par le modèle de PolyStrata, qui tourne dans le navigateur"></a><br>
<sub>Un jardin de pagode en voxels, généré en une seule requête sur une RTX 5070 avec <a href="https://github.com/Niko1221/Strata">Strata</a> (Qwen3.8-Flash-Next IQ3_S, contexte de 128K) ·
<a href="https://github.com/Niko1221/Strata/releases/download/v0.1.10/Pagoda.mp4">vidéo complète (49 s)</a></sub></p>

PolyStrata fait tourner de grands modèles d'IA sur un PC normal, des modèles qui demandent d'habitude un serveur.
Ils discutent, écrivent du code et fonctionnent avec vos applications et vos agents de code. Rien ne quitte votre PC.

Il fait tourner neuf modèles, avec deux moteurs :

- **[Qwen3.8-Flash-Next](https://huggingface.co/Qwen/Qwen3.8-Flash-Next)** sur une carte graphique NVIDIA ou AMD
  de 12 Go ou plus, sous Windows ou Linux. Il lit aussi les images. Son moteur est celui de
  [Strata](https://github.com/Niko1221/Strata), inchangé.

Les huit autres tournent sur un moteur écrit pour PolyStrata, avec une carte NVIDIA ou [deux](docs/MULTI_GPU.md#the-strata-poly-models) : tous les huit sous Linux, et
les six plus petits aussi sous Windows. Ils sont expérimentaux : ils n'ont tourné que sur un seul PC avec une carte
de 32 Go, et les six plus petits en plus sur un portable avec une carte de 8 Go.

- **[GLM 5.3 Flash](docs/GLM.md)**, le plus grand : un fichier de 79 à 114 Go, dont la plus grande part reste en RAM.
- **[DeepSeek V4 Flash](docs/DEEPSEEK.md)**, un fichier de 91 Go, dont la plus grande part reste aussi en RAM. Sa
  mémoire d'une discussion est petite : un contexte de 128K ne prend que 0,7 Go de VRAM de plus que 32K.
- **[LFM2.5-24B-A2B](docs/LFM.md)** de Liquid AI, un fichier de 11 à 25 Go. La plupart de ses couches n'ont pas
  d'attention, si bien qu'une longue discussion prend peu de VRAM : un contexte de 128K ne prend que 2 Go de plus
  que 32K. Sa licence n'autorise l'usage commercial qu'aux entreprises de moins de 10 millions de dollars de chiffre
  d'affaires annuel.
- **[Qwen3.6-35B-A3B](docs/BACKENDS.md)**, un fichier de 13 à 23 Go. Il lit aussi les images.
- **[Ornith 1.5](docs/BACKENDS.md)**, qui a l'architecture de Qwen3.6-35B-A3B et d'autres poids : un fichier de 14 à 24 Go.
  Il lit aussi les images.
- **[Gemma 4 26B-A4B](docs/BACKENDS.md)**, un fichier de 11 à 17 Go. Il lit aussi les images.
- **[Tiel Coder 35B-A3B](docs/BACKENDS.md)**, Ornith 1.5 quantifié par peculiar-ragdoll pour les agents de code :
  un fichier de 14 à 23 Go. Il lit aussi les images.
- **[MiniCPM-V 4.7 35B-A3B](docs/BACKENDS.md)** d'OpenBMB, avec l'architecture de Qwen3.6-35B-A3B et ses propres
  poids, fait pour lire les images : un fichier de 16 à 29 Go.

Le serveur peut passer d'un modèle installé à un autre pendant qu'il tourne
([comment](docs/BACKENDS.md#changing-the-model), en anglais).

PolyStrata est construit sur [Strata](https://github.com/Niko1221/Strata) de Niko1221.
Pourquoi c'est un dépôt à part, les cartes AMD et plus petites, les fine-tunes et d'autres questions : [FAQ](docs/FAQ.md) (en anglais).

## Quelle est sa vitesse ?

Un token correspond à environ ¾ d'un mot.

- **Écrit les réponses :** la vitesse à laquelle la réponse s'affiche dans une courte discussion. 60 tokens par seconde, c'est plus rapide que votre lecture.
- **Lit votre prompt :** la vitesse à laquelle il absorbe ce que vous envoyez (ici un document, du code ou un historique de discussion de 32K tokens).

### Qwen3.8-Flash-Next

Mesuré par [Strata](https://github.com/Niko1221/Strata) sur deux PC de jeu ordinaires, avec un prompt de 32K tokens.

<table>
<tr><th>NVIDIA : RTX 5070 (12 Go), Ryzen 5 7600, 64 Go de RAM</th><th>AMD : RX 9070 XT (16 Go), Ryzen 9 3900X, 47 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **Q2_0** | 94 tokens/s | 2,650 tokens/s |
| **IQ2_XS** | 79 tokens/s | 2,090 tokens/s |
| **IQ3_XXS** | 62 tokens/s | 1,750 tokens/s |
| **IQ3_S** | 53 tokens/s | 1,620 tokens/s |
| **Coder** | 55 tokens/s | 2,180 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **Q2_0** | 60 tokens/s | 1,160 tokens/s |
| **IQ2_XS** | 52 tokens/s | 1,110 tokens/s |
| **Coder** | 44 tokens/s | 1,420 tokens/s |

</td></tr>
</table>

NVIDIA : Q2_0 avec le moteur 0.1.36, les autres lignes avec 0.1.26 (réponses de 4K, prompts de 32K). Les tableaux
complets sont dans [DETAILS.md](docs/DETAILS.md#speed-measured). Une carte avec plus de VRAM va plus vite : une
RTX 3090 (24 Go) devrait écrire environ 100-140 tokens par seconde. Longues discussions et autres cartes :
[vitesse de chaque modèle](docs/MODELS.md#how-fast-is-each-size), [résultats de la communauté](docs/COMMUNITY_BENCHMARKS.md).

### GLM 5.3 Flash

Mesuré pour PolyStrata sur une station de travail, avec une courte discussion et un prompt de 29K tokens.

<table>
<tr><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **3.0BIT** | 59-66 tokens/s | 820 tokens/s |
| **UD-IQ2_XXS** | 50-52 tokens/s | 1,060-1,080 tokens/s |
| **REAP50-Q3_K_M** | 59-66 tokens/s | 1,300-1,310 tokens/s |

</td></tr>
</table>

GLM 5.3 Flash est un modèle bien plus grand que les autres, et seul un cinquième environ de ses experts tient dans les 32 Go de cette carte :
il écrit donc plus lentement. Ce qui a été mesuré et comment : [docs/GLM.md](docs/GLM.md#what-it-was-measured-on) (en anglais).

### DeepSeek V4 Flash

Mesuré pour PolyStrata sur la même station de travail, avec une courte discussion et un prompt de 30K tokens.

<table>
<tr><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-IQ2_XXS** | 56 tokens/s | 1,040 tokens/s |

</td></tr>
</table>

Sur le même PC, llama.cpp écrivait 25 à 28 tokens par seconde et lisait un prompt de 3,384 tokens à 96. Un prompt de
116,473 tokens, dans un contexte de 128K, a été lu et a reçu sa réponse en 174 secondes, un de 487,529 tokens, dans
un contexte de 512K, en 32 minutes, et les deux fois les trois faits cachés dedans ont été retrouvés. Ce qui a été mesuré et comment : [docs/DEEPSEEK.md](docs/DEEPSEEK.md#what-it-was-measured-on) (en anglais).

### LFM2.5-24B-A2B

Mesuré comme Qwen3.6-35B-A3B ci-dessous, sur les deux mêmes PC, avec un prompt d'environ 30,000 tokens.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q4_K_XL** | 70-73 tokens/s | 3,590-3,620 tokens/s |
| **UD-Q3_K_XL** | 88 tokens/s | 3,760-3,810 tokens/s |
| **QAD-Q4_0** | 77-78 tokens/s | 3,720-3,730 tokens/s |
| **UD-Q6_K_XL** | 42-43 tokens/s | 3,030-3,120 tokens/s |
| **Q8_0** | 23-34 tokens/s | 2,950-3,040 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q4_K_XL** | 480-485 tokens/s | 23,390-23,460 tokens/s |
| **UD-Q3_K_XL** | 490-497 tokens/s | 22,920-23,400 tokens/s |
| **QAD-Q4_0** | 498 tokens/s | 23,880-23,900 tokens/s |
| **UD-Q6_K_XL** | 423-426 tokens/s | 21,250-21,270 tokens/s |
| **Q8_0** | 376 tokens/s | 23,270-23,350 tokens/s |

</td></tr>
</table>

Sur le portable, Q8_0 remplit les 32 Go de RAM, d'où sa lenteur ; la station de travail contient chaque taille en
entier.

Sur le portable, llama.cpp b11540 avec `--n-cpu-moe 25`, le meilleur des réglages essayés là, a lu un prompt de 32,000
tokens à 530 tokens/s et a écrit ensuite à 53 ; PolyStrata l'a lu à 3,540 et a écrit à 71-73. Sur la station de
travail, llama-bench a lu 29,816 tokens à 13,300 tokens/s avec le lot par défaut de llama.cpp, 512, et à 23,600 avec
2,048, aussi vite que PolyStrata, et a écrit token par token à 455 tokens/s, et à 369 après ce prompt ; PolyStrata
écrit à 452 et 387. Un prompt d'environ 124,000 tokens dans un contexte de 128K a été lu et a reçu sa réponse en 8,7
secondes sur la station de travail ; le portable l'a lu en 45 secondes.

### Qwen3.6-35B-A3B

Mesuré par PolyStrata sur deux PC, avec une courte discussion et un prompt de 30,000 à 32,000 tokens : à gauche un
portable dont la carte a 8 Go, où le processeur calcule les experts que la carte ne peut pas contenir, à droite une
station de travail dont la carte contient le modèle entier.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 94-100 tokens/s | 1,690 tokens/s |
| **UD-Q2_K_XL** | 123-124 tokens/s | 1,590 tokens/s |
| **UD-Q4_K_M** | 82-86 tokens/s | 1,560-1,570 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 394-395 tokens/s | 7,800-7,810 tokens/s |
| **UD-Q2_K_XL** | 450 tokens/s | 7,720-7,740 tokens/s |
| **UD-Q4_K_M** | 395-396 tokens/s | 8,070-8,080 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens est lu à 1,660 tokens/s sur le portable et à 6,300 sur la station de travail.
Les chiffres de la carte de la
station limitée à 8 Go, un substitut d'avant la mesure sur le portable, sont dans
[docs/BACKENDS.md](docs/BACKENDS.md#with-8-gb-of-vram) (en anglais).

### Ornith 1.5

Mesuré de la même façon, sur les deux mêmes PC.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **APEX-I-COMPACT** | 76-82 tokens/s | 1,620 tokens/s |
| **APEX-I-MINI** | 80-86 tokens/s | 1,550-1,560 tokens/s |
| **APEX-I-QUALITY** | 59-63 tokens/s | 1,570-1,580 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **APEX-I-COMPACT** | 316-319 tokens/s | 8,040-8,110 tokens/s |
| **APEX-I-MINI** | 289-300 tokens/s | 7,710-7,740 tokens/s |
| **APEX-I-QUALITY** | 297-302 tokens/s | 7,860-7,880 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens est lu à 1,590 tokens/s sur le portable et à 6,400 sur la station de travail.

### Gemma 4 26B-A4B

Mesuré de la même façon, sur les deux mêmes PC.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 92-93 tokens/s | 2,240 tokens/s |
| **UD-Q2_K_XL** | 101-104 tokens/s | 2,150-2,160 tokens/s |
| **UD-Q4_K_M** | 85 tokens/s | 2,150 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_M** | 353-356 tokens/s | 8,420-8,480 tokens/s |
| **UD-Q2_K_XL** | 332-338 tokens/s | 7,940-7,970 tokens/s |
| **UD-Q4_K_M** | 335-346 tokens/s | 8,320-8,390 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens est lu à 2,130 tokens/s sur le portable et à 6,800 sur la station de travail.

### Tiel Coder 35B-A3B

Mesuré de la même façon, sur les deux mêmes PC.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_XL** | 83 tokens/s | 1,700 tokens/s |
| **UD-IQ3_XXS** | 96-97 tokens/s | 1,610 tokens/s |
| **UD-Q4_K_XL** | 74-79 tokens/s | 1,660-1,670 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **UD-Q3_K_XL** | 337-345 tokens/s | 8,080-8,110 tokens/s |
| **UD-IQ3_XXS** | 352 tokens/s | 7,520 tokens/s |
| **UD-Q4_K_XL** | 316-322 tokens/s | 7,750-7,970 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens est lu à 1,670 tokens/s sur le portable et à 6,300 sur la station de travail.

### MiniCPM-V 4.7 35B-A3B

Mesuré de la même façon, sur les deux mêmes PC. Son fichier n'a pas de bloc brouillon : il écrit un token à la fois.

<table>
<tr><th>NVIDIA : RTX 4070 Laptop (8 Go), Core i7-14700HX, 32 Go de RAM</th><th>NVIDIA : RTX 5090 (32 Go), Xeon Platinum 8470Q (25 vCPU), 120 Go de RAM</th></tr>
<tr><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **Q4_K_M** | 64 tokens/s | 1,150 tokens/s |
| **mix_q3gud_im** | 59 tokens/s | 1,130-1,150 tokens/s |
| **Q6_K** | 40 tokens/s | 960-980 tokens/s |

</td><td>

| Taille | Écrit les réponses | Lit votre prompt |
| --- | ---: | ---: |
| **Q4_K_M** | 308-311 tokens/s | 7,910-7,950 tokens/s |
| **mix_q3gud_im** | 291-293 tokens/s | 7,730-7,740 tokens/s |
| **Q6_K** | 277-278 tokens/s | 7,290-7,330 tokens/s |

</td></tr>
</table>

Un prompt de 10,000 tokens est lu à 1,260 tokens/s sur le portable et à 8,790 sur la station de travail. Sur le
portable, Q6_K remplit les 32 Go de RAM, c'est ce qui le ralentit là.

Les huit modèles du moteur propre à PolyStrata ont été mesurés avec son benchmark, que vous pouvez lancer sur votre
PC : `python polystrata.py benchmark --model <name> --prompt-tokens 24000`
([ce qu'il envoie et affiche](docs/BACKENDS.md#benchmark), en anglais).
Ce que d'autres ont mesuré sur leurs cartes, et comment comparer avec llama.cpp sur la vôtre :
[résultats de la communauté](docs/COMMUNITY_BENCHMARKS.md#the-models-polystrata-adds) (en anglais).
Sur le portable, PolyStrata a lu les prompts 2.7 à 3.5 fois plus vite que llama.cpp b11540 et écrit 1.5 à 1.8
fois plus vite, avec Qwen3.6 et avec Tiel Coder ([les deux mesures](docs/COMMUNITY_BENCHMARKS.md#comparing-with-llamacpp), en anglais).

## Ce qu'il vous faut

### Qwen3.8-Flash-Next

| | |
| --- | --- |
| **Carte graphique** | **NVIDIA** GeForce RTX série 20, 30, 40 ou 50, ou **AMD** Radeon RX 7900 XT / XTX, RX 7800 XT / 7700 XT, RX 9060 XT, RX 9070 / 9070 XT, Radeon AI PRO R9700 ou série RX 6800 / 6900. Il faut **12 Go de VRAM ou plus**. |
| **RAM** | 32 Go ou plus. Votre RAM décide [quel modèle](#quel-modèle-choisir-) tient. Avec 64 Go, toutes les tailles tournent. |
| **Disque** | Environ 80 Go libres. Prenez un SSD si possible : le premier démarrage est bien plus rapide. |
| **Système** | Windows 10 / 11 ou Linux, avec un pilote graphique NVIDIA ou AMD à jour. |

L'installateur s'occupe de tout le reste. Deux ou trois cartes peuvent se partager le modèle ([multi-GPU](docs/MULTI_GPU.md)).

Expérimental, écrit et testé par des membres de la communauté sur leurs propres machines :

- **Cartes graphiques plus anciennes** (Tesla P40 / V100, GTX 10, Radeon VII / MI50, RX 6700 XT, RX 5500 XT) : [Older GPUs](docs/OLDER_GPUS.md).
- **Intel Arc**, compilé depuis les sources sous Linux : [Intel Arc](docs/INTEL_ARC.md).
- **AMD Ryzen AI Max (Strix Halo)**, compilé depuis les sources sous Linux : [Strix Halo](docs/STRIX_HALO.md).
- **Processeurs plus anciens sans AVX2** : ça marche, mais lentement. [Older CPUs](docs/INSTALL.md#older-cpus-experimental).

La liste complète : [docs/INSTALL.md](docs/INSTALL.md#what-you-need).

### GLM 5.3 Flash

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente. Il n'a tourné qu'avec **32 Go de VRAM**. Environ 12 Go sont pris avant le premier expert : une carte de 12 Go ne peut donc pas le démarrer. |
| **RAM** | À côté d'une carte de 32 Go, **96 Go ou plus** pour le modèle entier, 82 Go pour sa taille à 2 bits, 60 Go pour la taille qui garde la moitié des experts. Une carte plus petite demande plus de RAM ([combien](#quel-modèle-choisir-)). Il a tourné avec 120 Go. |
| **Disque** | 114 Go, 102 Go ou 79 Go pour le fichier du modèle, sur un SSD. |
| **Système** | Linux, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste. Il compile le moteur sur votre PC et propose d'installer ce qu'il faut pour cela
(g++ et le CUDA Toolkit). Pas encore : les cartes AMD, deux cartes sous Windows, trois cartes ou plus, les images.
Un utilisateur le fait tourner sous Windows avec une carte de 24 Go et 128 Go de RAM ([rapport](https://github.com/VecSzn/PolyStrata/issues/10)).

La liste complète : [docs/GLM.md](docs/GLM.md#what-you-need) (en anglais).

### DeepSeek V4 Flash

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente. Il n'a tourné qu'avec **32 Go de VRAM**. Environ 11,5 Go sont pris avant le premier expert : une carte de 12 Go ne peut donc pas le démarrer. |
| **RAM** | À côté d'une carte de 32 Go, **72 Go ou plus**, selon l'estimation de l'installeur. Une carte plus petite demande plus de RAM ([combien](#quel-modèle-choisir-)). Il a tourné avec 120 Go. |
| **Disque** | 91 Go pour les trois fichiers du modèle, sur un SSD. |
| **Système** | Linux, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Pas encore essayé : les cartes AMD, deux
cartes. Il ne lit que du texte.
Un utilisateur le fait tourner sous Windows avec une carte de 24 Go et 128 Go de RAM ([rapport](https://github.com/VecSzn/PolyStrata/issues/21)).

La liste complète : [docs/DEEPSEEK.md](docs/DEEPSEEK.md#what-you-need) (en anglais).

### LFM2.5-24B-A2B

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 4 Go sont pris avant le premier expert. Il a tourné sur la carte de 8 Go d'un portable et sur une carte de 32 Go. |
| **RAM** | Environ **16, 18, 20, 27 ou 31 Go** à côté d'une carte de 8 Go pour UD-Q3_K_XL, QAD-Q4_0, UD-Q4_K_XL, UD-Q6_K_XL et Q8_0, d'après l'estimation de l'installeur. Une carte de 24 Go contient en entier les trois plus petites. Les cinq tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 11 à 25 Go pour le fichier du modèle. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour Qwen3.6-35B-A3B ci-dessous. Il n'a pas encore été essayé sur une
carte AMD. Il ne lit que du texte. La licence de Liquid AI n'autorise l'usage commercial qu'aux entreprises de moins
de 10 millions de dollars de chiffre d'affaires annuel ([LFM Open License v1.0](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B/blob/main/LICENSE)).

La liste complète : [docs/LFM.md](docs/LFM.md#what-you-need) (en anglais).

### Qwen3.6-35B-A3B

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 5 Go sont pris avant le premier expert. Il a tourné sur une carte de 32 Go, sur cette carte limitée à 8 Go, et sur la carte de 8 Go d'un portable. Sur une carte AMD, il tourne aussi, en bêta, sous Linux et sous Windows : l'installateur compile le moteur sur votre PC ; testé pour l'instant avec Qwen3.6 et Ornith sur une RX 7600. |
| **RAM** | Environ **17, 21 ou 27 Go** à côté d'une carte de 8 Go pour les trois tailles, d'après l'estimation de l'installeur. Une carte de 24 Go contient en entier les deux plus petites. Les trois tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 13, 17 ou 23 Go pour le fichier du modèle, 1 Go de plus pour lire les images. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Sous Windows, il télécharge le moteur déjà compilé
de la dernière version publiée. Sur un processeur sans AVX2, ou dans une copie dont le moteur a changé depuis cette
version, il le compile à la place avec le compilateur 2022 de Visual Studio et le CUDA Toolkit, et propose de les
installer. Pas encore : deux cartes sous Windows, trois cartes ou plus. Sur une carte AMD sous Windows, l'installeur
compile `strata-poly` pour la carte comme sous Linux (les wheels TheRock d'AMD dans `.rocm-win`) et prévient que l'y
faire tourner est nouveau ([AMD_HIP.md](docs/AMD_HIP.md#strata-poly-on-amd-beta)).

Depuis la 0.1.44, le moteur de [Strata](https://github.com/Niko1221/Strata) lui-même fait aussi tourner Qwen3.6, à partir de
UD-Q4_K_XL (`--family qwen36-strata`, 32 Go de RAM ou plus). Sur une RTX 5090 avec UD-Q4_K_XL, il a lu un prompt de 4K
1,6 fois plus vite que strata-poly ; sur un prompt de 31K et en écriture, les deux sont à peu près à égalité
([mesures](docs/BACKENDS.md#qwen36-on-two-engines), en anglais). Les tailles plus petites, pour les cartes de 8 Go, restent sur strata-poly.

### Ornith 1.5

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 4 Go sont pris avant le premier expert. Il a tourné sur une carte de 32 Go, sur cette carte limitée à 8 Go, et sur la carte de 8 Go d'un portable. Sur une carte AMD, il tourne aussi, en bêta, sous Linux et sous Windows : l'installateur compile le moteur sur votre PC ; testé pour l'instant avec Qwen3.6 et Ornith sur une RX 7600. |
| **RAM** | Environ **18, 22 ou 28 Go** à côté d'une carte de 8 Go pour les trois tailles, d'après l'estimation de l'installeur. Une carte de 24 Go contient en entier les deux plus petites. Les trois tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 14, 17 ou 24 Go pour le fichier du modèle, 1 Go de plus pour lire les images. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Sous Windows, il télécharge le moteur déjà compilé
de la dernière version publiée. Sur un processeur sans AVX2, ou dans une copie dont le moteur a changé depuis cette
version, il le compile à la place avec le compilateur 2022 de Visual Studio et le CUDA Toolkit, et propose de les
installer. Pas encore : deux cartes sous Windows, trois cartes ou plus. Sur une carte AMD sous Windows, l'installeur
compile `strata-poly` pour la carte comme sous Linux (les wheels TheRock d'AMD dans `.rocm-win`) et prévient que l'y
faire tourner est nouveau ([AMD_HIP.md](docs/AMD_HIP.md#strata-poly-on-amd-beta)).

### Gemma 4 26B-A4B

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 6 Go sont pris avant le premier expert. Il a tourné sur une carte de 32 Go, sur cette carte limitée à 8 Go, et sur la carte de 8 Go d'un portable. Sur une carte AMD, il tourne aussi, en bêta, sous Linux et sous Windows : l'installateur compile le moteur sur votre PC ; testé pour l'instant avec Qwen3.6 et Ornith sur une RX 7600. |
| **RAM** | Environ **15, 18 ou 21 Go** à côté d'une carte de 8 Go pour les trois tailles, d'après l'estimation de l'installeur. D'après la même estimation, une carte de 16 Go contient en entier les deux plus petites. Les trois tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 11, 13 ou 17 Go pour le fichier du modèle et son modèle brouillon, 1 Go de plus pour lire les images. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Sous Windows, il télécharge le moteur déjà compilé
de la dernière version publiée. Sur un processeur sans AVX2, ou dans une copie dont le moteur a changé depuis cette
version, il le compile à la place avec le compilateur 2022 de Visual Studio et le CUDA Toolkit, et propose de les
installer. Pas encore : deux cartes sous Windows, trois cartes ou plus. Sur une carte AMD sous Windows, l'installeur
compile `strata-poly` pour la carte comme sous Linux (les wheels TheRock d'AMD dans `.rocm-win`) et prévient que l'y
faire tourner est nouveau ([AMD_HIP.md](docs/AMD_HIP.md#strata-poly-on-amd-beta)).

### Tiel Coder 35B-A3B

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 5 Go sont pris avant le premier expert. Il a tourné sur une carte de 32 Go, sur cette carte limitée à 8 Go, et sur la carte de 8 Go d'un portable. Sur une carte AMD, il tourne aussi, en bêta, sous Linux et sous Windows : l'installateur compile le moteur sur votre PC ; testé pour l'instant avec Qwen3.6 et Ornith sur une RX 7600. |
| **RAM** | Environ **18, 21 ou 27 Go** à côté d'une carte de 8 Go pour les trois tailles, d'après l'estimation de l'installeur. Une carte de 24 Go contient en entier les trois tailles. Les trois tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 14, 17 ou 23 Go pour le fichier du modèle, 1 Go de plus pour lire les images. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Sous Windows, il télécharge le moteur déjà compilé
de la dernière version publiée. Sur un processeur sans AVX2, ou dans une copie dont le moteur a changé depuis cette
version, il le compile à la place avec le compilateur 2022 de Visual Studio et le CUDA Toolkit, et propose de les
installer. Pas encore : deux cartes sous Windows, trois cartes ou plus. Sur une carte AMD sous Windows, l'installeur
compile `strata-poly` pour la carte comme sous Linux (les wheels TheRock d'AMD dans `.rocm-win`) et prévient que l'y
faire tourner est nouveau ([AMD_HIP.md](docs/AMD_HIP.md#strata-poly-on-amd-beta)).

### MiniCPM-V 4.7 35B-A3B

| | |
| --- | --- |
| **Carte graphique** | Une carte **NVIDIA** GeForce RTX série 20 ou plus récente avec **8 Go de VRAM ou plus**. Environ 3.5 Go sont pris avant le premier expert. Il a tourné sur une carte de 32 Go et sur la carte de 8 Go d'un portable. Sur une carte AMD, il tourne aussi, en bêta, sous Linux et sous Windows : l'installateur compile le moteur sur votre PC ; il a tourné sur une RX 7600. |
| **RAM** | Environ **19, 25 ou 31 Go** à côté d'une carte de 8 Go pour les trois tailles, d'après l'estimation de l'installeur. Une carte de 32 Go contient en entier les trois tailles. Les trois tailles ont tourné sur un portable de 32 Go. |
| **Disque** | 16, 21 ou 29 Go pour le fichier du modèle, 1 Go de plus pour lire les images. |
| **Système** | Linux ou Windows 10 / 11, et un pilote NVIDIA récent. |

L'installeur s'occupe de tout le reste, comme pour GLM 5.3 Flash. Sous Windows, il télécharge le moteur déjà compilé
de la dernière version publiée. Sur un processeur sans AVX2, ou dans une copie dont le moteur a changé depuis cette
version, il le compile à la place avec le compilateur 2022 de Visual Studio et le CUDA Toolkit, et propose de les
installer. Pas encore : deux cartes sous Windows, trois cartes ou plus. Sur une carte AMD sous Windows, l'installeur
compile `strata-poly` pour la carte comme sous Linux (les wheels TheRock d'AMD dans `.rocm-win`) et prévient que l'y
faire tourner est nouveau ([AMD_HIP.md](docs/AMD_HIP.md#strata-poly-on-amd-beta)).

Ce qu'il faut à chacun des cinq et comment il a été vérifié : [docs/BACKENDS.md](docs/BACKENDS.md#install) (en anglais).

## Installation

### Laissez votre IA l'installer

Vous utilisez un assistant de code IA (Claude Code, Cursor, Codex, GitHub Copilot, ...) ? Collez-lui ceci :

```text
Set up PolyStrata on this PC for me: https://github.com/VecSzn/PolyStrata - follow docs/AI_SETUP.md in that repository.
```

Il vérifie votre carte graphique, votre RAM et votre disque, et choisit le modèle qui convient. Ensuite il l'installe,
le démarre et vous explique comment connecter vos applications. Les outils d'IA peuvent aussi installer, démarrer et
arrêter PolyStrata grâce à son [serveur MCP](docs/MCP_SERVER.md).

### Ou faites-le vous-même

[Téléchargez PolyStrata](https://github.com/VecSzn/PolyStrata/archive/refs/heads/main.zip) et décompressez-le (ou faites un `git clone`).
**Windows :** double-cliquez sur **`START-HERE.bat`**. **Linux :** lancez **`./setup.sh`** dans le dossier PolyStrata.

**Mac (Apple Silicon, M1 à M5, macOS 15 ou plus récent) :** installez une fois les Command Line Tools d'Apple (`xcode-select --install` dans le Terminal ; Xcode lui-même n'est pas nécessaire). Ouvrez ensuite le Terminal dans le dossier PolyStrata et lancez **`./setup.sh`**. Il compile le moteur Mac sur votre Mac (environ 7 minutes), pose les mêmes questions, télécharge le modèle et le démarre. La fois suivante, `./setup.sh` démarre tout de suite. Quelle taille convient à votre Mac et sa vitesse : [docs/MAC.md](docs/MAC.md). Le moteur Mac ne fait tourner que Qwen3.8-Flash-Next ; les huit autres modèles ont besoin d'une carte NVIDIA ou AMD.

Les étapes sont les mêmes pour NVIDIA et AMD. L'installateur détecte votre carte et installe le bon moteur pour elle.
Il vous pose quelques questions :

- quel modèle et quelle taille,
- combien de contexte (la quantité de texte que le modèle garde en tête),
- s'il doit lire les images (GLM 5.3 Flash, DeepSeek V4 Flash et LFM2.5 n'en lisent pas).

Appuyez sur Entrée à chaque fois pour garder la réponse recommandée. Ensuite il télécharge le modèle (environ 70 Go pour
Qwen3.8-Flash-Next, 79 à 114 Go pour GLM 5.3 Flash, 91 Go pour DeepSeek V4 Flash, 11 à 29 Go pour les six autres)
et le démarre. Si le téléchargement s'arrête, relancez-le : il reprend là où il s'était arrêté. Votre navigateur
ouvre l'application PolyStrata à l'adresse `http://127.0.0.1:8080`.

Les neuf figurent dans la liste des modèles de l'installeur. `./setup.sh --family glm`, `deepseek`, `lfm25`, `qwen36`,
`ornith`, `gemma4`, `tiel` ou `minicpmv` mène directement à l'un d'eux ([ce qu'il demande et écrit](docs/GLM.md#install), en anglais).

> **Pendant le démarrage du modèle, votre PC peut ralentir ou ne plus répondre pendant 1-3 minutes** (plus longtemps la première fois).
> PolyStrata charge 35-55 Go dans votre RAM et en réserve une partie pour la carte graphique. C'est normal. Attendez, et
> ne fermez pas la fenêtre. La fenêtre montre ce que fait PolyStrata.

**La fois suivante**, relancez `START-HERE.bat` (ou `./setup.sh`). Il démarre tout de suite et ne télécharge rien
deux fois. Fermez sa fenêtre pour arrêter le modèle. `UPDATE.bat` (`./update.sh`) met PolyStrata à jour sans le démarrer.
Mises à jour, Docker, plusieurs cartes, emplacement des fichiers et toutes les options : [docs/INSTALL.md](docs/INSTALL.md).

**C'est rapide sur votre PC ?** Lancez `BENCH.bat` (`./bench.sh`) : 5 à 10 minutes environ ; il mesure le chat, des prompts de 4K/16K et une session de type agent avec vos propres réglages, et écrit un rapport à partager ([comment](docs/COMMUNITY_BENCHMARKS.md#run-the-benchmark)).

## Quel modèle choisir ?

### Qwen3.8-Flash-Next

L'installateur en recommande un selon votre RAM. Le même modèle existe en plusieurs tailles, plus ou moins
compressées. Les petites tailles sont plus rapides. Les grandes sont un peu plus intelligentes.

| Votre RAM | Prenez | Pourquoi |
| --- | --- | --- |
| **32 Go** | **Coder** | il tient dans 32 Go et il est fait pour le code (avec une carte de 24 Go, Q2_0 et IQ2_XS tournent aussi) |
| **48 Go** | **IQ2_XS** (ou Q2_0, le plus rapide) | les plus grandes tailles ne tiennent pas |
| **64 Go** | **IQ2_XS** (recommandé), ou IQ3_XXS / IQ3_S | toutes les tailles tiennent ; IQ3_S est le meilleur et le plus lent |
| **96 Go ou plus** | **IQ3_S**, ou l'UD-IQ4_XS d'Unsloth (~4 bits) | de la place pour les plus grandes tailles, avec tout le reste ouvert |

- **[Coder](docs/MODELS.md#coder) :** une version pour le code, avec la moitié des experts en moins. Elle atteint 91 %
  du score SWE-bench Verified du modèle complet (mesuré par ses auteurs) et tient dans 32 Go de RAM. Elle est moins
  bonne en dehors du code, y compris pour le chinois et les autres textes CJK (#438). Pour ces cas, prenez Q2_0,
  IQ2_XS ou IQ3_S, qui gardent tous les experts.
- **[Swift 1.5](docs/MODELS.md#swift-15) :** un fine-tune qui réfléchit bien moins longtemps avant de répondre. Vous
  avez la réponse plus tôt, avec à peu près la même qualité.
- **[Unsloth UD-IQ4_XS](docs/MODELS.md#unsloth-ud-iq4_xs)** : la version ~4 bits d'Unsloth, entre IQ3_S et
  UD-Q4_K_XL en qualité. Un téléchargement de 94 Go. Avec moins de ~80 Go de RAM, PolyStrata en lit une partie depuis
  le SSD pendant qu'il répond, il y est donc plus lent (un SSD NVMe aide).
- **[Unsloth UD-Q4_K_XL, UD-Q5_K_XL, UD-Q6_K_XL et Q8_0](docs/MODELS.md#unsloths-files-ud-iq4_xs-to-q8_0) :** de 4 à 8 bits, plus proches du modèle
  complet (93-95 % le même token suivant que Q8_0). Rapides quand tous les experts tiennent dans la RAM (~88 Go pour
  UD-Q4_K_XL jusqu'à ~136 Go pour Q8_0) ; avec moins de RAM, PolyStrata en lit une partie depuis le SSD pendant qu'il
  répond, ce qui est bien plus lent (7-8.5 tokens/s pour UD-Q4_K_XL sur un PC avec 64 Go).
- **[OrcaRouter's Uncensored IQ3_XXS](docs/MODELS.md#orcarouter-uncensored-iq3_xxs) :** vous l'installez à la main.
  Il n'est pas dans le menu de l'installateur.

Tailles, téléchargements et ce qui tient où : [docs/MODELS.md](docs/MODELS.md). Pour ajouter un autre modèle plus
tard, lancez `SETUP.bat` (Linux : `./setup.sh --setup`).

### GLM 5.3 Flash

Trois tailles pour l'instant. L'installeur prend la première pour laquelle votre RAM suffit.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 32 Go |
| --- | --- | ---: | ---: |
| **3.0BIT** | le modèle entier, à environ 3 bits ([neuralll](https://huggingface.co/neuralll/GLM-5.3-Flash-GSQ-RCO-3.0bit-Q4Kattn-GGUF)) | 114 Go | 96 Go |
| **UD-IQ2_XXS** | le modèle entier, à environ 2 bits ([Unsloth](https://huggingface.co/unsloth/GLM-5.3-Flash-GGUF)) : il lit un prompt plus vite et écrit plus lentement | 102 Go | 82 Go |
| **REAP50-Q3_K_M** | la moitié des experts routés retirée ([REAP50](https://huggingface.co/patrickbdevaney/GLM-5.3-Flash-REAP50-GGUF) de patrickbdevaney) | 79 Go | 60 Go |

C'est votre carte graphique qui décide de la RAM nécessaire : les experts que la carte ne contient pas doivent rester en RAM.

| Votre carte graphique | 3.0BIT | UD-IQ2_XXS | REAP50-Q3_K_M | |
| --- | ---: | ---: | ---: | --- |
| **32 Go** | **96 Go** | **82 Go** | **60 Go** | le PC sur lequel il a tourné (120 Go de RAM) |
| **24 Go** | environ 104 Go | environ 90 Go | environ 68 Go | pas encore essayé |
| **16 Go** | environ 113 Go | environ 99 Go | environ 77 Go | pas encore essayé |
| **12 Go ou moins** | | | | ne démarre pas |

Les chiffres de RAM sont l'estimation de l'installeur, d'après le seul PC où il a tourné. Avec moins de RAM il démarre quand
même, mais il lit des experts sur le SSD pendant qu'il répond et devient bien plus lent. L'installeur le dit et demande
d'abord.

Les autres quantifications de GLM 5.3 Flash ne sont pas encore prises en charge : [ce qui manque](docs/GLM.md#what-is-missing) (en anglais).

### DeepSeek V4 Flash

Une seule taille pour l'instant.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 32 Go |
| --- | --- | ---: | ---: |
| **UD-IQ2_XXS** | le modèle entier, à environ 2 bits ([Unsloth](https://huggingface.co/unsloth/DeepSeek-V4-Flash-0731-GGUF)) | 91 Go | 72 Go |

Comme pour GLM 5.3 Flash, les experts que la carte ne garde pas restent en RAM : selon l'estimation de l'installeur,
environ 80 Go à côté d'une carte de 24 Go et 89 Go à côté d'une carte de 16 Go ; une carte de 12 Go ne peut pas le
démarrer. Seule la carte de 32 Go a été essayée.

### LFM2.5-24B-A2B

Cinq tailles. L'installeur prend UD-Q4_K_XL, ou UD-Q3_K_XL sur un PC qui a moins de RAM. Les autres se choisissent par leur nom : `--model QAD-Q4_0`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **UD-Q4_K_XL** | le modèle entier avec des experts à 4 bits ([Unsloth](https://huggingface.co/unsloth/LFM2.5-24B-A2B-GGUF)) | 15 Go | environ 20 Go |
| **UD-Q3_K_XL** | le même avec des experts à 3 bits : celui qui demande le moins de RAM | 11 Go | environ 16 Go |
| **QAD-Q4_0** | 4 bits, de [Liquid AI](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B-GGUF) même, entraîné pour perdre moins à cette quantification | 14 Go | environ 18 Go |
| **UD-Q6_K_XL** | des experts à 6 bits | 22 Go | environ 27 Go |
| **Q8_0** | 8 bits partout, le plus proche du modèle tel qu'entraîné | 25 Go | environ 31 Go |

Comparées à Q8_0, sur cinq prompts de 256 tokens chacun, les autres tailles choisissent le token le plus probable de Q8_0
à 96,8 % des pas (UD-Q6_K_XL), 95,5 % (UD-Q4_K_XL), 92,7 % (QAD-Q4_0) et 91,6 % (UD-Q3_K_XL).

### Qwen3.6-35B-A3B

Trois tailles pour l'instant. L'installeur prend UD-Q3_K_M, ou UD-Q2_K_XL sur un PC qui a moins de RAM. UD-Q4_K_M se choisit par son nom : `--model UD-Q4_K_M`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **UD-Q3_K_M** | le modèle entier avec des experts à 3 bits ([Unsloth](https://huggingface.co/unsloth/Qwen3.6-35B-A3B-MTP-GGUF)) | 17 Go | environ 21 Go |
| **UD-Q2_K_XL** | le même avec des experts à 2 et 3 bits : celui qui demande le moins de RAM | 13 Go | environ 17 Go |
| **UD-Q4_K_M** | le même avec des experts à 4 bits | 23 Go | environ 27 Go |

Une carte plus grande contient une plus grande part du modèle et demande moins de RAM : avec UD-Q3_K_M, environ 17 Go à côté d'une carte de 12 Go, 13 Go à côté d'une carte de 16 Go, et 10 Go dès que la carte contient tout. Ce sont les estimations
de l'installeur. Elles n'ont pas été vérifiées sur un tel PC.

### Ornith 1.5

Trois tailles pour l'instant. L'installeur prend APEX-I-COMPACT, ou APEX-I-MINI sur un PC qui a moins de RAM. APEX-I-QUALITY se choisit par son nom : `--model APEX-I-QUALITY`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **APEX-I-COMPACT** | le modèle entier avec des experts à 3 et 4 bits ([APEX](https://huggingface.co/mudler/Ornith-1.5-35B-A3B-APEX-MTP-GGUF) de mudler) | 17 Go | environ 22 Go |
| **APEX-I-MINI** | le même avec des experts à 2 et 3 bits : celui qui demande le moins de RAM | 14 Go | environ 18 Go |
| **APEX-I-QUALITY** | le même avec des experts de 4 à 6 bits | 24 Go | environ 28 Go |

Une carte plus grande contient une plus grande part du modèle et demande moins de RAM : avec APEX-I-COMPACT, environ 17 Go à côté d'une carte de 12 Go, 13 Go à côté d'une carte de 16 Go, et 10 Go dès que la carte contient tout. Ce sont les estimations
de l'installeur. Elles n'ont pas été vérifiées sur un tel PC.

### Gemma 4 26B-A4B

Trois tailles pour l'instant. L'installeur prend UD-Q3_K_M, ou UD-Q2_K_XL sur un PC qui a moins de RAM. UD-Q4_K_M se choisit par son nom : `--model UD-Q4_K_M`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **UD-Q3_K_M** | le modèle entier avec des experts à 3 bits ([Unsloth](https://huggingface.co/unsloth/gemma-4-26B-A4B-it-GGUF)), et le modèle brouillon de Google pour lui | 13 Go | environ 18 Go |
| **UD-Q2_K_XL** | le même avec des experts à 2 bits : celui qui demande le moins de RAM | 11 Go | environ 15 Go |
| **UD-Q4_K_M** | le même avec des experts à 4 et 5 bits | 17 Go | environ 21 Go |

Une carte plus grande contient une plus grande part du modèle et demande moins de RAM : avec UD-Q3_K_M, environ 14 Go à côté d'une carte de 12 Go, et 10 Go dès que la carte contient tout. Ce sont les estimations
de l'installeur. Elles n'ont pas été vérifiées sur un tel PC.

### Tiel Coder 35B-A3B

Trois tailles pour l'instant. L'installeur prend UD-Q3_K_XL, ou UD-IQ3_XXS sur un PC qui a moins de RAM. UD-Q4_K_XL se choisit par son nom : `--model UD-Q4_K_XL`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **UD-Q3_K_XL** | le modèle entier avec des experts à 3 et 4 bits ([peculiar-ragdoll](https://huggingface.co/peculiar-ragdoll/Tiel-Coder-35B-A3B-GGUF-MTP)) | 17 Go | environ 21 Go |
| **UD-IQ3_XXS** | le même avec des experts à 2 et 3 bits : celui qui demande le moins de RAM | 14 Go | environ 18 Go |
| **UD-Q4_K_XL** | le même avec des experts de 4 à 6 bits | 23 Go | environ 27 Go |

Une carte plus grande contient une plus grande part du modèle et demande moins de RAM : avec UD-Q3_K_XL, environ 17 Go à côté d'une carte de 12 Go, 13 Go à côté d'une carte de 16 Go, et 10 Go dès que la carte contient tout. Ce sont les estimations
de l'installeur. Elles n'ont pas été vérifiées sur un tel PC.

### MiniCPM-V 4.7 35B-A3B

Trois tailles pour l'instant, d'OpenBMB. L'installeur prend Q4_K_M, ou mix_q3gud_im sur un PC qui a moins de RAM. Q6_K se choisit par son nom : `--model Q6_K`.

| Taille | Ce que c'est | Téléchargement | RAM à côté d'une carte de 8 Go |
| --- | --- | ---: | ---: |
| **Q4_K_M** | le modèle entier avec des experts à 4 bits ([OpenBMB](https://huggingface.co/openbmb/MiniCPM-V-4.7-35B-A3B-gguf)) | 21 Go | environ 25 Go |
| **mix_q3gud_im** | le même avec des experts à 3 et 4 bits : celui qui demande le moins de RAM | 16 Go | environ 19 Go |
| **Q6_K** | le même avec des experts à 6 bits | 29 Go | environ 31 Go |

## Utilisation

Les neuf modèles s'utilisent de la même façon : la même application, la même API.

<p align="center"><img src="docs/media/runpagoda.png" width="900" alt="L'onglet Monitor de l'application PolyStrata à côté d'un agent de code"><br>
<sub>Le <b>Monitor</b> de l'application PolyStrata (à gauche) pendant qu'un agent de code écrit le jardin de pagode de la vidéo (à droite). Une capture d'écran de <a href="https://github.com/Niko1221/Strata">Strata</a>.</sub></p>

- **Dans le navigateur :** ouvrez `http://127.0.0.1:8080`. Vous y trouvez **Chat**, un **Monitor** en direct du
  modèle et de votre GPU/CPU/RAM, et **About** avec les réglages et les adresses.
- **Vos applications et agents de code :** ajoutez un fournisseur « OpenAI-compatible » avec l'URL de base
  **`http://127.0.0.1:8080/v1`**. N'importe quelle clé API et n'importe quel nom de modèle fonctionnent.
  - Applications qui utilisent l'API d'Anthropic : `http://127.0.0.1:8080/v1/messages` (Claude Code :
    `ANTHROPIC_BASE_URL=http://127.0.0.1:8080`, et `"fit_max_tokens": true` dans `strata-<model>.json`
    ([pourquoi](docs/FAQ.md#can-i-use-it-with-claude-code-codex-or-other-apps))).
  - Codex CLI et les autres applications qui utilisent l'API OpenAI Responses : `/v1/responses`
    ([configuration](docs/DETAILS.md#the-responses-api-and-codex-cli)).
- **Réflexion :** choisissez **off, low, medium ou high** dans le menu du chat ou dans le « reasoning effort » de
  votre application. Off est le plus rapide. High est le meilleur pour les questions difficiles.
- **Depuis votre téléphone ou un autre PC :** `START-HERE.bat --setup --host 0.0.0.0 --api-key <secret>`. Définissez
  toujours une clé.

Là où ils diffèrent :

| | Qwen3.8-Flash-Next | GLM 5.3 Flash | DeepSeek V4 Flash | LFM2.5-24B-A2B | Qwen3.6-35B-A3B, Ornith 1.5, Gemma 4 26B-A4B, Tiel Coder 35B-A3B, MiniCPM-V 4.7 35B-A3B |
| --- | --- | --- | --- | --- | --- |
| **Images** | Répondez oui à « Images? » pendant l'installation. Ensuite cliquez sur **Picture** dans le chat, ou joignez des images dans votre application. Les cartes AMD lisent les images sous Linux via le processeur ; sous Windows, pas encore. | Pas encore. | Il ne lit que du texte. | Il ne lit que du texte. | Répondez oui à « Images? » pendant l'installation, puis comme avec Qwen3.8-Flash-Next. Sur une carte de moins de 16 Go, l'encodeur d'images tourne sur le processeur, et une image prend quelques secondes. |
| **Une requête à la fois** | Par défaut, PolyStrata répond à une requête et les autres attendent. Pour répondre à plusieurs en même temps, mettez `"parallel": 2` ([BATCHING.md](docs/BATCHING.md)). Sur une carte de 12 Go, chaque réponse devient alors plus lente. | Une à la fois, les autres attendent. | Une à la fois, les autres attendent. | Une à la fois, les autres attendent. | Une à la fois, les autres attendent. |
| **Plusieurs discussions** | La dernière est conservée, donc les messages suivants démarrent en quelques secondes. Deux arguments du moteur en gardent plusieurs en RAM ([détails](docs/DETAILS.md#using-it)). | La dernière est conservée. L'installation demande s'il faut en garder plusieurs en RAM, pour qu'un agent et ses sous-agents se relaient sans que leurs prompts soient relus ([comment](docs/GLM.md#several-conversations-at-once)). | Comme GLM 5.3 Flash. | Comme GLM 5.3 Flash. | Comme GLM 5.3 Flash. |
| **Longs prompts** | PolyStrata lit en entier le premier message d'une discussion, environ 1 minute pour 30,000 tokens. Les messages suivants démarrent en quelques secondes. | Le premier message est lu en entier, environ 40 secondes pour 30,000 tokens. | Le premier message est lu en entier, environ 29 secondes pour 30,000 tokens. | Le premier message est lu en entier : 30,000 tokens prennent 1,3 à 1,5 seconde sur la station de travail et 8 à 10 secondes sur le portable de 8 Go. | Le premier message est lu en entier : 30,000 tokens prennent 4 à 5 secondes sur la station de travail et 14 à 27 secondes sur le portable de 8 Go. |

Plus d'infos : [où sont stockées vos discussions](docs/INSTALL.md#where-things-are-stored), [l'API](docs/DETAILS.md#using-it).

### Optionnel : plus de vitesse sur une seule carte quand tout le modèle tient en RAM

Deux réglages optionnels (de RubenTorney, #1904 et #1905). Ils aident si vous utilisez une seule carte graphique et que votre RAM contient tous les experts (le mode arena de PolyStrata, par exemple Q2_0 ou IQ3_XXS sur un PC de 64 Go). Ouvrez votre configuration `strata-<model>.json` dans le dossier de données de PolyStrata ([où](docs/INSTALL.md#where-things-are-stored)) et ajoutez :

- **Génération plus rapide :** `"--adapt-async", "1"` dans la liste `"args"`. Le cache d'experts est alors mis à jour dans un thread auxiliaire au lieu de mettre la réponse en pause. Mesuré sur une RTX 5070 (Windows, Q2_0) : +4-5 % de génération, +7-10 % sur le code et les longs documents. Les réponses peuvent légèrement différer de celles par défaut (elles restent identiques d'une exécution à l'autre).
- **Lecture du prompt plus rapide sous Windows :** `"STRATA_PIN_BY_NEED": "1"` dans le bloc `"env"`. Windows ne peut verrouiller qu'environ la moitié de la RAM pour la carte ; ce réglage verrouille d'abord les experts dont la carte a vraiment besoin. Mesuré sur la RTX 5070 : +4,4 % en lecture du prompt.

```json
"args": [ ..., "--adapt-async", "1" ],
"env": { "STRATA_PIN_BY_NEED": "1" }
```

Redémarrez PolyStrata (`START-HERE.bat` ou `./setup.sh`) après la modification. Pour annuler, retirez les deux entrées.

## Un problème ?

- **Mon PC s'est figé au premier démarrage de PolyStrata.** C'est normal pendant le chargement du modèle. Attendez, et ne
  fermez pas la fenêtre. Toujours figé après 10 minutes ? Redémarrez le PC, fermez les autres programmes et
  réessayez, ou choisissez une taille plus petite.
- **Il s'est arrêté pendant le téléchargement ou l'installation.** Relancez `START-HERE.bat` (ou `./setup.sh`). Il
  reprend là où il s'était arrêté.
- **C'est très lent et le voyant du disque clignote sans arrêt, ou il affiche « the engine stopped unexpectedly ».**
  Votre PC n'a pas assez de RAM libre. Fermez les autres programmes (les navigateurs en utilisent beaucoup), ou
  choisissez une taille plus petite (Q2_0 ou IQ2_XS).
- **Il dit que le port 8080 est déjà utilisé.** PolyStrata tourne déjà. Cherchez sa fenêtre.

Autres problèmes et leurs solutions : [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md). Toujours bloqué ? Ouvrez une
[issue](https://github.com/VecSzn/PolyStrata/issues) et joignez `strata-<model>.log` depuis le dossier PolyStrata. Vous
avez trouvé un problème de sécurité ? Signalez-le en privé : [SECURITY.md](SECURITY.md).

## Comment ça marche ?

Les modèles comme celui-ci tournent d'habitude sur des serveurs avec des centaines de gigaoctets de mémoire
graphique. Votre carte graphique a 8-32 Go. PolyStrata fait tenir le modèle en **répartissant le travail sur tout votre
PC**. Pensez à une cuisine : ce que vous utilisez tout le temps reste sur le plan de travail, et le reste attend dans
le placard.

### Qwen3.8-Flash-Next

<p align="center"><img src="docs/media/how-it-works.svg" width="860" alt="Les 24,576 experts du modèle : les plus sollicités sur la carte graphique, tous en RAM, une table de correspondance sur le SSD"></p>

- **Le modèle est une équipe de 24,576 petits spécialistes (les « experts »).** Chaque mot n'en demande que 10.
- **Votre carte graphique** garde les quelques milliers d'experts les plus utilisés. **Votre RAM** les contient tous,
  et **votre processeur** travaille sur les autres en même temps. **Votre SSD** contient une grande table de
  correspondance.

<p align="center"><img src="docs/media/guess-and-check.svg" width="860" alt="Un petit assistant devine les mots suivants ; le grand modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** un petit assistant devine les quelques mots suivants. Le grand modèle les vérifie
  tous d'un coup. Vous obtenez la même réponse, 1.6-1.8x plus tôt.
- **Les longs textes sont lus par gros morceaux** (jusqu'à 8,192 tokens à la fois), à plus de 1,000 tokens par seconde.

L'explication plus longue : [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md). Chaque partie et ses chiffres :
[les détails](docs/DETAILS.md#how-it-works) et l'[article](docs/paper/Strata-Paper.pdf).

### GLM 5.3 Flash

<p align="center"><img src="docs/media/how-it-works-glm.svg" width="860" alt="Les 12,096 experts du modèle : les plus utilisés sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **Le modèle est une équipe de 12,096 petits experts**, 288 dans chacune de 42 couches. Chaque mot n'en demande que 8
  par couche.
- **Votre carte graphique** garde les plus utilisés, environ 2,470 sur une carte de 32 Go : le moteur compte ceux que
  votre travail utilise et refait son choix au démarrage suivant. **Votre RAM** contient les autres, et **votre
  processeur** les calcule en même temps. **Votre SSD** contient le fichier du modèle, utilisé là où il se trouve,
  sans seconde copie ; un démarrage prend environ 6 secondes une fois le fichier dans votre RAM.

<p align="center"><img src="docs/media/guess-and-check-glm.svg" width="860" alt="Un bloc du modèle devine les mots suivants ; le modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** un bloc du modèle fait pour deviner propose les quelques mots suivants. Le modèle les
  vérifie tous d'un coup. Vous obtenez la même réponse, 1.1-1.2x plus tôt.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois, les experts que la carte ne contient pas
  passant par elle), à 780 à 1,250 tokens par seconde.

Chaque partie et ses chiffres : [docs/GLM.md](docs/GLM.md#how-it-uses-the-card) (en anglais).

### DeepSeek V4 Flash

<p align="center"><img src="docs/media/how-it-works-dsv4.svg" width="860" alt="Les 11,008 experts du modèle : les plus utilisés sur la carte graphique, le reste en RAM, les fichiers du modèle sur le SSD"></p>

- **Le modèle est une équipe de 11,008 petits experts**, 256 dans chacune de ses 43 couches. Chaque mot n'en demande
  que 6 par couche, plus un que tous les mots utilisent.
- **Votre carte graphique** garde les plus utilisés, environ 2,840 sur une carte de 32 Go : le moteur compte ceux
  dont votre travail se sert. **Votre RAM** garde le reste, et **votre processeur** les calcule en même temps. **Votre
  SSD** garde les fichiers du modèle, utilisés là où ils sont, sans deuxième copie.
- **Une petite mémoire de la discussion :** un mot lit les 128 derniers mots en entier et le reste sous forme de
  lignes compressées, une pour 4 ou pour 128 mots. Un token de contexte prend 6,880 octets, si bien qu'un long contexte
  laisse à la carte presque toute sa place pour les experts.
- **Les longs textes sont lus en gros morceaux** (1,024 tokens à la fois), 30,000 tokens à environ 1,000 tokens par
  seconde. Il n'a pas encore de « deviner puis vérifier » : le modèle brouillon fait pour lui ne tourne pas encore.

Chaque partie et ses chiffres : [docs/DEEPSEEK.md](docs/DEEPSEEK.md#how-it-differs-from-the-other-models) (en anglais).

### LFM2.5-24B-A2B

<p align="center"><img src="docs/media/how-it-works-lfm25.svg" width="860" alt="Les 2,432 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **Le modèle est une équipe de 2,432 petits experts**, 64 dans chacune de 38 couches. Chaque mot n'en demande que 4
  par couche.
- **La plupart de ses 40 couches n'ont pas d'attention.** 30 d'entre elles mélangent les mots par une courte
  convolution sur les trois derniers et ne gardent que deux vecteurs par couche. Seules les 10 autres gardent toute la
  discussion, si bien que sa mémoire d'une discussion est petite : un contexte de 128K ne prend que 2 Go de VRAM de
  plus que 32K.
- **Votre carte graphique** garde autant d'experts qu'elle peut : environ 900 à 1,300 sur une carte de 8 Go, tous à
  partir de 24 Go pour les tailles à 4 bits. Sur la carte de 8 Go, ceux qu'elle contient servent les deux tiers aux
  quatre cinquièmes de ce qu'une réponse utilise. **Votre RAM** contient les autres, et **votre processeur** les
  calcule en même temps. **Votre SSD** contient le fichier du modèle, utilisé là où il se trouve, sans seconde copie.
- **Pas de bloc de brouillon.** Le modèle n'a rien qui devine la suite, donc le moteur ne devine qu'à partir du texte
  que la discussion répète.
- **Quand la carte contient tous les experts**, toute une étape reste sur la carte : elle choisit elle-même les
  experts des mots et fait tourner les 40 couches d'un token comme un seul lancement enregistré.

Chaque partie et ses chiffres : [docs/LFM.md](docs/LFM.md) (en anglais).

### Qwen3.6-35B-A3B

<p align="center"><img src="docs/media/how-it-works-qwen36.svg" width="860" alt="Les 10,240 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **Le modèle est une équipe de 10,240 petits experts**, 256 dans chacune de 40 couches. Chaque mot n'en demande que 8
  par couche.
- **Votre carte graphique** en garde autant qu'elle peut : environ 2,600 sur une carte de 8 Go, tous à partir de 24
  Go. Pendant qu'il écrit, le moteur fait venir sur la carte les experts que la réponse choisit sans cesse, et sur la
  carte de 8 Go ceux qu'elle contient servent environ 60 % de ce qu'une réponse utilise. **Votre RAM** contient les
  autres, et **votre processeur** les calcule en même temps. **Votre SSD** contient le fichier du modèle, utilisé là
  où il se trouve, sans seconde copie.

<p align="center"><img src="docs/media/guess-and-check-qwen36.svg" width="860" alt="Le bloc brouillon du modèle devine les mots suivants ; le modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** le bloc brouillon du modèle lui-même, qui est dans son fichier, propose les quelques
  mots suivants. Le modèle les vérifie tous d'un coup. Vous obtenez la même réponse, 1.2-1.5x plus tôt.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois). Quand la carte ne contient pas tous les
  experts, les autres passent par elle une seule fois pour tout le prompt.

Chaque partie et ses chiffres : [docs/BACKENDS.md](docs/BACKENDS.md#how-a-backend-works) (en anglais).

### Ornith 1.5

<p align="center"><img src="docs/media/how-it-works-ornith.svg" width="860" alt="Les 10,240 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **L'architecture de Qwen3.6-35B-A3B avec d'autres poids :** une équipe de 10,240 petits experts, 256 dans chacune de
  40 couches, et seulement 8 par couche pour chaque mot.
- **Votre carte graphique** en garde autant qu'elle peut : environ 3,100 sur une carte de 8 Go, tous à partir de 24
  Go. Pendant qu'il écrit, le moteur fait venir sur la carte les experts que la réponse choisit sans cesse, et sur la
  carte de 8 Go ceux qu'elle contient servent environ 60 % de ce qu'une réponse utilise. **Votre RAM** contient les
  autres, et **votre processeur** les calcule en même temps. **Votre SSD** contient le fichier du modèle, utilisé là
  où il se trouve, sans seconde copie.

<p align="center"><img src="docs/media/guess-and-check-ornith.svg" width="860" alt="Le bloc brouillon du modèle devine les mots suivants ; le modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** son bloc brouillon est aussi dans son fichier et propose les quelques mots suivants. Il
  devine juste moins souvent que celui de Qwen3.6-35B-A3B : vous obtenez la même réponse 1.1-1.2x plus tôt.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois). Quand la carte ne contient pas tous les
  experts, les autres passent par elle une seule fois pour tout le prompt.

Chaque partie et ses chiffres : [docs/BACKENDS.md](docs/BACKENDS.md#how-a-backend-works) (en anglais).

### Gemma 4 26B-A4B

<p align="center"><img src="docs/media/how-it-works-gemma4.svg" width="860" alt="Les 3,840 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **Le modèle est une équipe de 3,840 petits experts**, 128 dans chacune de 30 couches. Chaque mot n'en demande que 8
  par couche, à côté d'une partie de la couche que tous les mots utilisent.
- **Votre carte graphique** en garde autant qu'elle peut : environ 1,000 sur une carte de 8 Go, tous à partir de 16 Go
  d'après l'estimation de l'installeur. Pendant qu'il écrit, le moteur fait venir sur la carte les experts que la
  réponse choisit sans cesse, et sur la carte de 8 Go ceux qu'elle contient servent environ 55 % de ce qu'une réponse
  utilise. **Votre RAM** contient les autres, et **votre processeur** les calcule en même temps. **Votre SSD**
  contient le fichier du modèle et son modèle brouillon, utilisés là où ils se trouvent, sans seconde copie.

<p align="center"><img src="docs/media/guess-and-check-gemma4.svg" width="860" alt="Le petit modèle brouillon de Google devine les mots suivants ; le modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** le petit modèle brouillon de Google pour lui, un fichier de 0.5 Go que l'installeur
  télécharge avec le modèle, propose les quelques mots suivants. Le modèle les vérifie tous d'un coup. Vous obtenez la
  même réponse, 1.3-1.6x plus tôt.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois). Quand la carte ne contient pas tous les
  experts, les autres passent par elle une seule fois pour tout le prompt.

Chaque partie et ses chiffres : [docs/BACKENDS.md](docs/BACKENDS.md#how-a-backend-works) (en anglais).

### Tiel Coder 35B-A3B

<p align="center"><img src="docs/media/how-it-works-tiel.svg" width="860" alt="Les 10,240 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **Ornith 1.5 dans une autre quantification :** une équipe de 10,240 petits experts, 256 dans chacune de
  40 couches, et seulement 8 par couche pour chaque mot.
- **Votre carte graphique** en garde autant qu'elle peut : environ 2,700 sur une carte de 8 Go, tous à partir de 24
  Go. Pendant qu'il écrit, le moteur fait venir sur la carte les experts que la réponse choisit sans cesse, et sur la
  carte de 8 Go ceux qu'elle contient servent environ 50 % de ce qu'une réponse utilise. **Votre RAM** contient les
  autres, et **votre processeur** les calcule en même temps. **Votre SSD** contient le fichier du modèle, utilisé là
  où il se trouve, sans seconde copie.

<p align="center"><img src="docs/media/guess-and-check-tiel.svg" width="860" alt="Le bloc brouillon du modèle devine les mots suivants ; le modèle les vérifie tous d'un coup et garde les bons"></p>

- **Deviner, puis vérifier :** son bloc brouillon est aussi dans son fichier et propose les quelques mots suivants. Il
  devine juste moins souvent que celui de Qwen3.6-35B-A3B : vous obtenez la même réponse 1.2-1.3x plus tôt.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois). Quand la carte ne contient pas tous les
  experts, les autres passent par elle une seule fois pour tout le prompt.

Chaque partie et ses chiffres : [docs/BACKENDS.md](docs/BACKENDS.md#how-a-backend-works) (en anglais).

### MiniCPM-V 4.7 35B-A3B

<p align="center"><img src="docs/media/how-it-works-minicpmv.svg" width="860" alt="Les 10,240 experts du modèle : ceux qu'une réponse choisit sans cesse sur la carte graphique, les autres en RAM, le fichier du modèle sur le SSD"></p>

- **L'architecture de Qwen3.6-35B-A3B avec les poids d'OpenBMB :** une équipe de 10,240 petits experts, 256 dans
  chacune de 40 couches, et seulement 8 par couche pour chaque mot.
- **Votre carte graphique** en garde autant qu'elle peut : avec Q4_K_M environ 2,000 sur une carte de 8 Go, tous sur
  une carte de 32 Go. Pendant qu'il écrit, le moteur fait venir sur la carte les experts que la réponse choisit sans
  cesse, et sur la carte de 8 Go ceux qu'elle contient servent environ 60 à 70 % de ce qu'une réponse utilise.
  **Votre RAM** contient les autres, et **votre processeur** les calcule en même temps. **Votre SSD** contient le
  fichier du modèle, utilisé là où il se trouve, sans seconde copie.
- **Un mot à la fois :** son fichier n'a pas de bloc brouillon, donc rien ne devine à l'avance.
- **Images :** son propre encodeur d'images lit une grande image comme une vue d'ensemble et quelques tuiles. Une
  capture de 1,200 sur 398 pixels devient environ 300 tokens, un graphique de 2,520 sur 1,620 environ 600.
- **Les longs textes sont lus par gros morceaux** (1,024 tokens à la fois). Quand la carte ne contient pas tous les
  experts, les autres passent par elle une seule fois pour tout le prompt.

Chaque partie et ses chiffres : [docs/BACKENDS.md](docs/BACKENDS.md#how-a-backend-works) (en anglais).

## Crédits et licence

Qwen3.8-Flash-Next est de l'[équipe Qwen](https://huggingface.co/Qwen/Qwen3.8-Flash-Next). Il a été compressé par
[ISTA-DASLab](https://huggingface.co/ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF), UkisAI (Swift 1.5) et Unsloth.
GLM 5.3 Flash tourne à partir des fichiers de [neuralll](https://huggingface.co/neuralll/GLM-5.3-Flash-GSQ-RCO-3.0bit-Q4Kattn-GGUF),
d'Unsloth et de patrickbdevaney. Qwen3.6-35B-A3B est de l'équipe Qwen, Gemma 4 de Google et DeepSeek V4 Flash de DeepSeek, tous trois dans la
quantification d'[Unsloth](https://huggingface.co/unsloth). LFM2.5-24B-A2B est de
[Liquid AI](https://huggingface.co/LiquidAI/LFM2.5-24B-A2B), dans la quantification d'Unsloth et dans le QAD-Q4_0
de Liquid AI. Ornith 1.5 est d'ornith-ai, dans la quantification APEX de
[mudler](https://huggingface.co/mudler/Ornith-1.5-35B-A3B-APEX-MTP-GGUF). Tiel Coder 35B-A3B est Ornith 1.5 dans la quantification de
[peculiar-ragdoll](https://huggingface.co/peculiar-ragdoll/Tiel-Coder-35B-A3B-GGUF-MTP) ;
[u/Sensitive_Song4219](https://www.reddit.com/r/LocalLLM/comments/1x22inc/comment/pf3r111/) a découvert qu'il tourne sur PolyStrata. MiniCPM-V 4.7 35B-A3B est d'[OpenBMB](https://huggingface.co/openbmb/MiniCPM-V-4.7-35B-A3B-gguf),
dans leur propre quantification ; [iMilnb](https://github.com/iMilnb) l'a amené à PolyStrata. PolyStrata est construit sur [Strata](https://github.com/Niko1221/Strata) de Niko1221 et de ses
contributeurs, et utilise des parties de [llama.cpp / ggml](https://github.com/ggml-org/llama.cpp). Tous les
crédits : [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md#credits). PolyStrata est open source sous [licence MIT](LICENSE).
Quelques parties et chaque modèle ont leur propre licence ([lesquelles](docs/HOW_IT_WORKS.md#license)).
