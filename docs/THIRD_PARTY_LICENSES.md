# Third-Party Licenses

Este documento lista las dependencias externas de MoodEngine que se distribuyen embebidas o portadas dentro del código fuente del proyecto, con sus respectivas licencias y obligaciones de atribución.

Las dependencias gestionadas por CPM (CMake Package Manager) — `glm`, `imgui`, `imnodes`, `assimp`, `spdlog`, `EnTT`, `Lua`, `SDL2`, `Tracy`, `doctest`, `nlohmann_json`, `meshoptimizer`, `portable-file-dialogs`, `Jolt`, `zlib`, `stb`, `miniaudio` — mantienen sus propios `LICENSE` files en sus repos respectivos. Esta lista cubre únicamente los componentes **portados a código fuente** dentro del repo MoodEngine.

---

## F3H31 — Sky Atmosphere Hosek-Wilkie

### Hosek-Wilkie Sky Model (dataset + helper CPU)

- **Origen**: Lukas Hosek + Alexander Wilkie, Charles University in Prague (Czech Republic).
- **Paper**: *"An Analytic Model for Full Spectral Sky-Dome Radiance"*, SIGGRAPH 2012.
- **Paper companion**: *"Adding a Solar Radiance Function to the Hosek Skylight Model"*, IEEE CG&A 2013.
- **Dataset oficial**: http://cgg.mff.cuni.cz/projects/SkylightModelling/
- **Atribución requerida**: mantener el banner del paper en `src/engine/render/sky/hosek_data_rgb.inl`. El banner del archivo trae copyright literal `Copyright (c) 2012, Lukas Hosek and Alexander Wilkie, Charles University in Prague`.
- **Uso comercial**: OK (uso libre con atribución académica/comercial según los términos del paper original).

Ports usados como referencia:

- **diharaw/sky-models** (MIT, 2019). Dihara Wijetunga.
  - Repo: https://github.com/diharaw/sky-models
  - License: MIT.
  - Archivos portados con modificaciones:
    - `src/engine/render/sky/HosekWilkie.cpp` (helper CPU `compute_coefficients`).
    - `src/engine/render/sky/hosek_data_rgb.inl` (dataset 270 coefs/canal RGB).
    - `shaders/procedural_sky.frag` (núcleo analítico Hosek-Wilkie GLSL).
  - Modificaciones: `#version 450 core` añadido, `varying` → `in/out`, removidas deps de `dw::Program`/`<macros.h>`/`<logger.h>`, namespace `Mood::Sky`, struct `HosekCoefficients` en lugar de clase.

### IBL Bake — Irradiance + GGX Prefilter

- **Referencias conceptuales** (no copia de código, sólo algoritmos públicos):
  - Brian Karis, *"Real Shading in Unreal Engine 4"*, SIGGRAPH 2013 Course Notes. Split-sum approximation, GGX importance sampling.
  - Holger Dammertz, *"Hammersley Points on the Hemisphere"*, low-discrepancy sequence (uso de fórmulas públicas, CC BY 3.0 — la matemática es genérica).
- **Archivos escritos desde cero** (no copia de LearnOpenGL — descartado por licencia CC BY-NC):
  - `shaders/ibl_irradiance.frag` — convolución hemisférica uniforme.
  - `shaders/ibl_prefilter.frag` — GGX importance sampling con Hammersley 2D.
  - `src/engine/render/sky/IBLBaker.cpp` — orquesta los 2 shaders contra FBO + cubemap targets.

---

## F3H30 — Texture pack HL1-style procedural

### Pillow + numpy (scripts Python)

- **Origen**: gestionado por el entorno Python del sistema (no embebido en el repo).
- **Pillow**: HPND (BSD-style, comercial OK).
- **numpy**: BSD-3 (comercial OK).
- **Scripts**: `tools/gen_*.py` + `tools/_texture_lib.py` son **originales** del proyecto MoodEngine, MIT (heredan la licencia general del repo).

---

## F2H86 — IBL Bake offline (legacy)

- **Pipeline**: `tools/bake_ibl.py` (script Python) — **original** del proyecto, MIT.

---

## Notas

- Si agregás dependencias nuevas con código embebido (portado al árbol del repo), documentarlas acá con: origen, licencia, archivos afectados, modificaciones aplicadas.
- Dependencias gestionadas por CPM **no necesitan entry aquí** — su `LICENSE` viaja con el repo descargado y CMake lo trae automáticamente.
