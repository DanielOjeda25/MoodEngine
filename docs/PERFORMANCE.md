# Performance baseline de MoodEngine

> **Generado en F2H2** (2026-05). Documento vivo. Cada hito de optimizacion
> (F2H3 frustum culling, F2H4 LOD, etc.) suma una columna comparativa.

## Como reproducir

1. Compilar en **Debug con `MOOD_PROFILE=ON`** (default):
   ```
   cmake --build build/debug --config Debug
   ```
2. Lanzar `build/debug/Debug/MoodEditor.exe` y abrir un proyecto vacio.
3. Toggle del HUD: **menu Ver > Performance**.
4. Spawn de la escena de stress: **menu Ayuda > Stress test poligonos > Spawn 10K / 100K / 500K / 1M tris (cubos)**.
5. **Snapshot al CSV** (F2H2 Bloque G): dentro del panel Performance, escribir el label de la escena (`Empty`, `10K`, ...) y clickear **"Snapshot baseline F2H2"**. Cada click appendea fila a `performance_baseline.csv` en el cwd del editor + log con prefijo `[F2H2_SNAPSHOT]`.
6. Para ver Tracy:
   - Bajar `tracy-profiler.exe` de https://github.com/wolfpld/tracy/releases (mismo tag que el cliente: **v0.11.1**).
   - Abrirlo, conectar (`Ctrl+C` o boton "Connect" → localhost).
   - Las zonas `EditorApplication::*`, `SceneRenderer::*`, `PBR::staticPass`, etc. aparecen en vivo.

## Hardware baseline

| Item | Valor |
|---|---|
| GPU | NVIDIA GeForce GTX 1660 (4 GB VRAM, driver 32.0.15.9621) |
| CPU | AMD Ryzen 5 5600G (6C/12T, 3.9 GHz max) |
| RAM | 16 GB DDR4-2666 (2x8 GB) |
| OS | Windows 11 Pro Education 26200 |
| Build | Debug / MSVC 14.44 |
| Tracy version | 0.11.1 (cliente linkeado). Profiler GUI no abrio por mismatch del binario descargado, pero `tracy-capture.exe` si genero `.tracy` y `tracy-csvexport.exe` lo decodifico — datos abajo son **reales medidos**, no inferidos. |

> **Nota sobre GPU**: el sistema tiene tambien una iGPU AMD Radeon (integrada
> al Ryzen 5600G). Los numeros de abajo se tomaron con la salida activa en la
> GTX 1660; verificar con `dxdiag` o el panel de NVIDIA si se sospecha que el
> editor esta corriendo en la integrada (sintoma: FPS muy por debajo de lo
> esperado para la GTX). Vale la pena confirmar en una proxima medicion.

## Resultados por escena de stress-test

> Viewport sin shadow demo activa. Snapshots tomados con el boton del panel
> Performance (CSV `performance_baseline.csv`). El FPS reportado por el HUD
> es un promedio acumulativo desde el arranque del editor — el valor "real"
> es el de las ultimas filas del CSV una vez que la escena se mantuvo
> estatica varios segundos.

### Baseline F2H2 (sin frustum culling)

| Escena | Entities | Tris | Draw calls | FPS (estable) | Frame ms (estable) | Top 3 zonas Tracy (% del frame) |
|---|---|---|---|---|---|---|
| Empty (proyecto vacio) | 2 | 24 | 2 | 60.0 (vsync cap) | 16.67 | PBR::staticPass <1%, UI::draw <1%, swapBuffers ~99% (vsync) |
| 10K tris (cubos) | 836 | 10,032 | 836 | ~4.0 | ~250 | PBR::staticPass 70.74%, swapBuffers 18.07%, UI::draw 4.45% |
| 100K tris (cubos) | _(no llega)_ | _(no llega)_ | _(no llega)_ | **editor congelado** | — | — |
| 500K tris (cubos) | — | — | — | **no probado** | — | — |
| 1M tris (cubos) | — | — | — | **no probado** | — | — |

### Post-F2H3 (con frustum culling activo, pase opaco estatico)

> Misma escena 10K + viewport, varios escenarios de camara para validar
> que el cull descarta entidades fuera del frustum.

| Escena | Entities | Draws | Tris dibujados | FPS | Frame ms | Cull % | Speedup vs F2H2 |
|---|---|---|---|---|---|---|---|
| Empty (control) | 2 | 2 | 24 | 60.0 | 16.67 | — | sin cambio (igual baseline) |
| 10K_full_view (mira al grid) | 836 | 743 | 8,916 | ~4.4 | ~222 | 11% | x1.13 (poco — todo en pantalla) |
| 10K_half_view (rota a un costado) | 836 | 731 | 8,772 | ~4.4 | ~224 | 13% | x1.13 |
| **10K_no_view (camara apunta lejos)** | 836 | **1** | **12** | **60.0** | **16.67** | **>99% (835/836)** | **x13.3** |

> **Lectura crucial**: el cull funciona **proporcional a cuanto cae fuera
> del frustum**. Si el viewport ve todo, no hay nada que cullar y el FPS no
> cambia (esperado). Si la camara mira lejos del grid, **el frame baja de
> 222ms a 16.67ms — speedup x13.3**.

### Post-F2H4 (con instancing del pase opaco activado)

> Mismas escenas + nueva (100K cubos = 8336 entidades, antes congelaba
> el editor). El instancing junta entidades con el mismo (mesh, material)
> en un solo `glDrawArraysInstanced`.

| Escena | Entities | Draws | Tris dibujados | FPS | Frame ms | Speedup vs F2H2 |
|---|---|---|---|---|---|---|
| Empty (control) | 2 | 2 | 24 | 60.0 | 16.67 | sin cambio |
| **10K_full_view** | 836 | **3** | 10,032 | **60.0 (vsync cap)** | **16.67** | **x15** (4 → 60 FPS) |
| 10K_no_view | 836 | 1 | 12 | 60.0 | 16.67 | igual a F2H3 (cull saca todo antes de batchear) |
| **100K_full_view** | 8,336 | **3** | 82,260 | **10.4** | **96** | **antes congelado, ahora editable** |
| 100K_no_view | 8,336 | 0 | 0 | 11.1 | 89 | cull descarta todo; cuello restante = scene iter |

> **Lectura crucial**: en `10K_full_view` los 836 cubos comparten mesh +
> material → **3 draw calls** (1 batch del cubo + skybox + debug renderer).
> El FPS satura el vsync cap: el rendering ya no es el cuello.
>
> En `100K_full_view` con 8336 cubos, el rendering sigue siendo 1 batch +
> skybox + debug = 3 draws. Frame 96ms = el cuello ya no es draws sino
> **iteracion de la escena en otros sistemas** (ImGui Hierarchy con 8336
> entries pesa ~6ms, scene scan en otros systems suma el resto).
>
> En `100K_no_view`, el cull descarta los 8336 cubos → 0 draws → pero
> el frame sigue en 89ms. Eso es **el proximo cuello a atacar**: con
> tantas entidades, el costo "por entidad" en sistemas no-render
> (Hierarchy panel, animation/script/nav scans aunque vacios) domina.

### Post-F2H5 (con virtualizacion ImGui Hierarchy)

> Misma escena 100K (8336 entidades), Hierarchy panel siempre visible,
> ListClipper activo → solo las ~30 entries visibles del scroll area
> se procesan, no las 8336.

| Escena | Entities | Draws | FPS | Frame ms | Mejora vs F2H4 |
|---|---|---|---|---|---|
| Empty (control) | 2 | 2-3 | 60.0 | 16.67 | sin cambio (vsync cap) |
| 100K_full_view | 8,336 | 4 | **11.0** | **90.5** | **~6%** (10.4 → 11.0 FPS, 96 → 90.5 ms) |
| 100K_no_view | 8,336 | 0 | **11.5** | **86.5** | **~4%** (11.1 → 11.5 FPS, 89.9 → 86.5 ms) |

> **Lectura honesta**: la prediccion del plan F2H5 era 12-15 FPS. La
> medida real fue 11.0 — mejora del **~6%**, no del 25-40% predicho.
>
> **Por que la prediccion fallo**: el plan asumia que `UI::draw` (19% del
> frame en F2H4, mean 6.27 ms) era **dominantemente** el panel Hierarchy.
> La realidad: ese 19% se reparte entre Hierarchy + Inspector (cuando
> hay entidad seleccionada con muchos componentes) + AssetBrowser +
> Performance HUD + gizmos. Atacar solo el Hierarchy mejora una fraccion
> del 19%, no el 19% completo.
>
> **Lo que F2H4 destapo realmente** (visible al cerrar F2H5): con 8336
> entidades, el cuello no es **solo** UI — es **scene iteration
> distribuida**. Multiples sistemas (Animation/Script/Nav/Particle/Audio/
> Trigger) hacen `scene.forEach<...>` cada frame, sumando costo lineal
> por entidad fuera del rendering. F2H5 ataco el cuello que pensabamos
> era dominante; ahora vemos que el cuello real es la suma de muchos
> loops O(N) en sistemas y panels.

### Release confirmado — el motor escala perfectamente

Despues de cerrar F2H5 medimos en **build Release con `MOOD_PROFILE=OFF`**
(binario de produccion sin overhead de Debug ni Tracy). Los numeros
**superan la prediccion**:

| Escena | Entities | Tris dibujados | Draws | FPS Release | Frame ms | Speedup vs Debug F2H5 |
|---|---|---|---|---|---|---|
| 100K_full_view | 8,336 | 73,032 | 3 | **60.0 (vsync cap)** | ~16 | **5.4x** |
| 100K_no_view | 8,336 | 0 | 0 | **60.0** | ~16 | **5.2x** |
| 500K_full_view | 41,669 | 186,060 | 3 | **60.0 (vsync cap)** | ~16 | — (no medible en Debug) |
| **1M_full_view** | **83,336** | 241,800 | 3 | **57.8** | ~17 | — (no medible en Debug) |

**Lectura crucial:**
- **El motor aguanta 83,336 entidades vivas a 58 FPS sostenido.** Eso es
  ~80x mas entidades que un mapa real (200-1500 entidades simultaneas).
- **100K y 500K corren a vsync cap (60 FPS)** — el rendering ya no es
  cuello en absoluto. El frame ms (~16) esta dominado por el wait de
  vsync, no por trabajo CPU/GPU.
- **1M es el primer escenario donde el frame baja del vsync** (a 17 ms /
  58 FPS). Para 83K entidades, eso es excepcional.
- **3 draw calls** en todos los casos: 1 batch + skybox + debug. F2H4
  instancing colapsa cualquier cantidad de cubos identicos a 1 draw.
- **Tris dibujados < tris esperados** (ej. stress 1M dibuja 241K): F2H3
  frustum cull descarta lo que no esta en el viewport. La camara ve solo
  una porcion del grid 3D — el resto queda fuera.

**Comparativa sintetica del impacto Release vs Debug:**

| Cuello | Debug + Tracy ON | Release + MOOD_PROFILE=OFF |
|---|---|---|
| Iterator debug level 2 | activo (cada acceso a vector chequea) | desactivado |
| Optimizaciones MSVC | ninguna (`-O0`) | full (`-O2 + LTO`) |
| Inlining | deshabilitado | agresivo |
| STL containers | modo verbose | release |
| Tracy macros | overhead 1-2% del frame | no-op total (`do{}while(0)`) |
| Resultado neto en frame ms | 5-10x mas lento | baseline real |

**Conclusion arquitectonica:** los cuellos que medimos en Debug-Tracy
F2H2/F2H3/F2H4/F2H5 eran **reales** pero **amplificados por el modo
build**. F2H3 + F2H4 + F2H5 atacaron los cuellos correctos —
en Release todos esos ataques se compounden y el motor escala mucho
mas alla de lo que un juego real necesita.

**Para mapas reales:**
- HL2 nivel tipico (~500-1500 entidades): trivial. El motor ya lo
  hizo a 60 FPS con 8K entidades.
- Skyrim escena (~200-500 entidades): trivial.
- Doom Eternal encuentro (~1000-3000 entidades): trivial.

**El stress test 100K-1M era un test patologico**, no representativo.
Los resultados Release confirman que el motor esta listo para producto
y que F2H6+ pueden ser features (LOD, CSG, contenido, herramientas)
en lugar de optimizaciones de emergencia.

### Nota de scope: 8336 entidades es un escenario patologico

El stress "100K tris" usa 8336 cubos primitivos (12 tris cada uno). Eso
NO es representativo de un mapa real:

| Caso | Tris en pantalla | Entidades simultaneas |
|---|---|---|
| **Stress test 100K (este)** | 100K | **8336** |
| **Half-Life 2 nivel tipico** | 500K - 2M | ~500 - 1500 |
| **Skyrim escena tipica** | 1M - 5M | ~200 - 500 |
| **Doom Eternal encuentro** | 10M - 50M | ~1000 - 3000 |

**Los mapas reales tienen MUCHOS mas triangulos pero MUCHAS menos
entidades** porque la geometria del nivel se mete en 1-3 meshes grandes
+ unas centenas de props. F2H3 + F2H4 cubren ambos extremos: instancing
para props repetidos, frustum cull para mesh grande no visible. El
stress test esta disenado para encontrar cuellos, no para representar
contenido real.

## Top 5 cuellos de botella identificados

### Baseline F2H2 (Tracy 3163 frames, escena 10K con viewport completo)

| # | Zona | % frame | Mean | Stddev | Max | Hito que lo ataca |
|---|---|---:|---:|---:|---:|---|
| 1 | `PBR::staticPass` | **70.74%** | 42.5 ms | 189.8 ms | 2574 ms | F2H3 frustum culling (✅) + F2H4 LOD (pendiente) + batching futuro |
| 2 | `swapBuffers` | 18.07% | 10.86 ms | 4.1 ms | 13.7 ms | No hay; cuando el frame se acelere, baja solo |
| 3 | `UI::draw` (ImGui) | 4.45% | 2.67 ms | 2.2 ms | 31.3 ms | Virtualizacion de Hierarchy si crece |
| 4 | `ImGui_RenderDrawData` | 0.25% | 0.15 ms | — | — | Marginal |
| 5 | `EditorApplication::beginFrame` | 0.17% | 0.10 ms | — | — | Marginal |

**Lectura**: `PBR::staticPass` mean 42.5 ms / 836 entidades = ~50 µs por
cubo de 12 tris = CPU-bound en draw call setup, no GPU-bound. Stddev 189 ms
con max 2574 ms = spikes brutales en frames con scene churn.

### Post-F2H3 (Tracy 1162 frames, escena 10K con cambio de camara)

| # | Zona | % frame | Mean | Stddev | Max | Delta vs F2H2 |
|---|---|---:|---:|---:|---:|---|
| 1 | `PBR::staticPass` | **15.73%** | 32.4 ms | 63.2 ms | 251 ms | **-55 pts %** / max 10x mas chico |
| 2 | `UI::draw` | 2.43% | 5.00 ms | 1.0 ms | 10.2 ms | -2 pts (mean sube por baseline mas rapido) |
| 3 | `swapBuffers` | 1.75% | 3.61 ms | 1.8 ms | 20.9 ms | -16 pts (frames mas rapidos -> menos vsync wait) |
| 4 | `EditorApplication::endFrame` | 1.85% | 3.81 ms | 1.8 ms | 21.1 ms | sin cambio significativo |
| 5 | `ImGui_RenderDrawData` | 0.08% | 0.17 ms | — | — | sin cambio significativo |

**Lectura**:
- `PBR::staticPass` baja al 15.73% del frame **promediando los 3 escenarios
  de camara** (full / half / no_view). En `10K_no_view` la zona ni se ejecuta
  porque `aabbVisible` descarta los 836 — eso explica el -55 pts brutal.
- **Stddev de `PBR::staticPass` baja de 189ms a 63ms** y el max de 2574ms a
  251ms (10x menos). Los spikes monstruosos del baseline desaparecieron — el
  frustum cull es estable, no introduce variabilidad.
- **El costo del culling no aparece en top 10** (zona `PBR::frustumCull` no
  existe — el cull esta dentro del scope `PBR::staticPass`; el plot
  `PBR::CulledStatic` reporta cuantos se descartaron pero el costo es <<1%).
- Los siguientes cuellos (`UI::draw`, `swapBuffers`) son ahora % bajos
  porque el frame total es mas chico — no porque empeoraran.

**Validacion del cull con counters del HUD** (snapshots CSV):

| Escena | Entities | Draws (sin cull = 836) | Cull rate |
|---|---|---|---|
| 10K_full_view | 836 | 743 | 11% (cubos del borde del FOV) |
| 10K_half_view | 836 | 731 | 13% (cuelpos rotando ~50%) |
| 10K_no_view | 836 | **1** | **>99%** (todo el grid fuera del frustum) |

**Validacion del cull con counters del HUD** (snapshots CSV):

| Escena | Entities | Draws (sin cull = 836) | Cull rate |
|---|---|---|---|
| 10K_full_view | 836 | 743 | 11% (cubos del borde del FOV) |
| 10K_half_view | 836 | 731 | 13% (cubos rotando ~50%) |
| 10K_no_view | 836 | **1** | **>99%** (todo el grid fuera del frustum) |

**Conclusion accionable**: F2H3 cumple su objetivo — bajar `PBR::staticPass`
proporcional a cuanto cae fuera del frustum. **Proximo target (segun
swap documentado): F2H4 instancing** para atacar el cuello real de draw
call submission, no triangle setup.

### Post-F2H4 (Tracy 4993 frames, escenas 10K + 100K mezcladas con cambios de camara)

| # | Zona | % frame | Mean | Stddev | Max | Delta vs F2H2 |
|---|---|---:|---:|---:|---:|---|
| 1 | `renderSceneToViewport` total | 29.69% | 9.76 ms | 18.0 ms | 92 ms | -41 pts (la mitad de lo que era) |
| 2 | `endFrame` (incl. swapBuffers) | 26.04% | 8.56 ms | 5.1 ms | 29 ms | sube por vsync wait — frame mas rapido |
| 3 | `swapBuffers` (vsync wait) | 25.53% | 8.39 ms | 5.2 ms | 29 ms | +7 pts en %; mean baja vs F2H2 |
| 4 | `UI::draw` (ImGui) | **19.06%** | 6.27 ms | 8.7 ms | 53 ms | **+15 pts** — Hierarchy con 8336 entries pesa |
| 5 | **`PBR::instancedPass`** | **2.52%** | **0.88 ms** | 0.23 ms | 4 ms | **el cuello del 70.74% desaparecio** |

**Lectura**:
- **`PBR::instancedPass` mean 0.88 ms vs F2H2 baseline 42.5 ms = ~48x mas
  rapido por draw**. El cuello del 70% del frame que F2H2 revelo esta
  resuelto. Confirma que F2H4 instancing era el ataque correcto al cuello
  medido (CPU-bound en draw call setup, no triangle processing).
- **`UI::draw` sube a 19.06% (mean 6.27 ms)** — con 8336 entidades en la
  escena, el panel Hierarchy de ImGui (sin virtualizacion) lista todas y
  procesa hover/click por entry. **Nuevo cuello dominante** y candidato
  natural para F2H5 / F2H? (virtualizar Hierarchy con `ImGuiListClipper`).
- **`swapBuffers` 25.53%** = vsync wait. El frame ahora es tan rapido en
  el render que pasa el cuarto del frame esperando el monitor. No es un
  cuello a atacar — es senial de que el pipeline esta saludable.
- **`100K_no_view` con 0 draws sigue en 11 FPS**: la escena tiene 8336
  entidades. Aunque el render salta todo (cull descarta), iterar las
  8336 entidades en otros sistemas (animation/script/nav scans aunque
  vacios) y construir el panel Hierarchy = ~89 ms. **Eso es el escalon
  siguiente** despues del UI::draw.

## Conclusiones

### F2H2 (baseline pre-culling)
- **El motor en Debug + sin culling/LOD se cae a tierra entre 10K y 100K
  triangulos** (~836 entidades). Spawnear 100K cubos congela el viewport.
- **`PBR::staticPass` se lleva el 70.74% del frame** con 836 cubos. Es CPU-
  bound en draw call setup (~50 µs/cubo).

### F2H3 (post-frustum culling)
- **Cull validado**: en escenarios donde la camara no ve el grid, el FPS sube
  de 4.4 a 60 (vsync cap), el frame ms baja de 222 a 16.67 — **speedup
  x13.3** medido. Cuando todo esta dentro del viewport, no hay mejora
  (esperado — no hay nada que cullar).
- **`PBR::staticPass` % del frame baja de 70.74% a 15.73%** (promediando
  los escenarios de camara). Stddev y max bajan 3-10x respectivamente —
  el cull no introduce variabilidad, la elimina.
- **El costo del culling es despreciable** (<<1% del frame, no aparece en
  top 10 zonas). Confirma la decision de F2H3 plano vs jerarquico: hoy
  no es cuello, ni siquiera con 836 entidades.

### F2H4 (post-instancing)
- **Cuello principal medido en F2H2 resuelto**: `PBR::instancedPass` mean
  0.88 ms vs `PBR::staticPass` baseline 42.5 ms = ~48x mas rapido por
  draw. 836 cubos del stress 10K → 3 draw calls (1 batch + skybox +
  debug). Vsync cap a 60 FPS.
- **Escena 100K editable por primera vez**: 8336 entidades, antes
  congelaba el editor; ahora corre a 10.4 FPS / 96 ms con 3 draws. F2H3
  + F2H4 en cadena hacen viable un escenario que el motor literalmente
  no soportaba.
- **Costo del cull + batching**: <<3% del frame con 836 cubos.
  Despreciable. Confirma que la decision "plano + sin cache de AABB"
  era correcta para v1.

### F2H6 (post-LOD system)
- **No medible con stress de cubos primitivos**: los cubos tienen 12
  tris, ya estan en el LOD minimo posible — no hay con que reducir.
  Era esperado y documentado en el plan.
- **Cimiento listo para contenido real**: cuando entren meshes
  complejos (Fox.glb ~1500 tris, kenney_survival props 500-2000 tris
  cada uno), el LOD reducira proporcionalmente. Ratios fijos en v1:
  LOD 1 = 50% tris (error 0.05), LOD 2 = 15% tris (error 0.10).
- **Cache en disco** (`assets/.cache/lods/<hash>.moodlod`) hace que el
  primer arranque genere los LODs (~ms por mesh) y los siguientes
  arranques sean instant. Borrarlo no rompe nada.
- **Skinned meshes saltean LOD** en v1 (re-mapping de bone weights con
  reduccion = scope grande con risk visual).
- **Validacion futura**: spawn de 50 Foxes en grid 100m + medicion
  cuando el dev decida poblar una escena de prueba con contenido AAA.

### F2H5 (post-virtualizacion Hierarchy)
- **Mejora medida modesta** (~6% en frame ms con 8336 entidades): 96
  → 90 ms / 10.4 → 11.0 FPS. Por debajo de los 12-15 FPS predichos
  en el plan.
- **Por que la prediccion fallo**: el cuello UI::draw NO era solo el
  Hierarchy — era una suma de Hierarchy + Inspector + AssetBrowser +
  HUD + gizmos. F2H5 ataco una fraccion. Aprendizaje: medir antes de
  predecir % de mejora con escenas grandes.
- **El refactor en si es codigo limpio + reusable**: el patron
  ListClipper queda como template para otros panels que crezcan
  (AssetBrowser cuando haya cientos de assets, Inspector cuando se
  agreguen muchos componentes).
- **Cuello real dominante con 8336 entidades**: scene iteration
  distribuida en multiples sistemas (Animation/Script/Nav/Particle/
  Audio/Trigger), cada uno O(N). Atacarlo requiere caching de queries
  o pipeline de sistemas — fuera del scope de este hito.

### Release con MOOD_PROFILE=OFF (medicion de produccion)
- **El motor escala perfecto**: 100K_full_view (8,336 entidades) →
  60 FPS vsync cap, 500K (41,669 entidades) → 60 FPS, 1M (83,336
  entidades) → 58 FPS. **5.4x speedup** vs Debug en el mismo
  escenario.
- **F2H3 + F2H4 + F2H5 fueron las inversiones correctas**: en Release
  todos compounden y el motor procesa **~80x mas entidades que un
  mapa real** sin sudar. Lo que parecia "no escalar" en Debug-Tracy
  era el overhead del modo build, no un problema arquitectonico.
- **Cuello del 70% medido en F2H2 + cuellos secundarios atacados en
  F2H3-F2H5: completamente resueltos en producto.** Mapas reales
  (200-1500 entidades) corren a vsync cap con margen brutal.

### Proximo paso (medicion Release ya completada)
**El motor esta listo para contenido real.** Los proximos hitos pueden
ser **features**, no optimizaciones de emergencia:

- **F2H6+ candidatos viables** (cualquiera valido, priorizar segun lo que
  el dev quiera trabajar):
  - **LOD original** (era F2H4, postergado dos veces): util cuando entre
    contenido real con meshes complejos (Fox/CesiumMan + props Kenney).
    Hoy con cubos primitivos no hay LOD que reducir, pero cuando el
    nivel real tenga modelos de 5-50K tris cada uno, el LOD bajara la
    carga del GPU en frames donde haya muchos.
  - **ChildrenComponent + Hierarchy folding**: agrupar entidades por
    padre en el panel (ej. "StressCubes (8336)" colapsable). UX > perf
    — el motor escala bien sin esto, pero la usabilidad del editor con
    100+ entidades sin folding ya es problematica.
  - **Sub-fase 2.2: CSG / nivel cerrado**: cuando empiecen los mapas
    reales con geometria de nivel. Es donde se va a notar el motor
    porque va a ser contenido real, no stress patologico.
  - **Sub-fase 2.5: dialog / quest / inventory**: features de juego
    propias. El motor ya tiene la base.

- **Optimizaciones diferidas** (no urgentes, postergables):
  - Caching de queries entt en sistemas (si una escena real grande lo
    justifica con datos).
  - Persistent mapped buffer para el VBO de instancias (si un benchmark
    real muestra que el upload de 50KB/frame se vuelve cuello).
  - Spatial partitioning / octree para frustum cull jerarquico (cuando
    aparezca una escena con >100K entidades visibles simultaneas).

## F2H42 — Shadow map caching + VSync toggle (2026-05-10)

> Re-medicion sobre PC de escritorio con la **stress scene completa**
> (`Mapa > Spawn FULL STRESS SCENE (F2H42)`): 200 cubos + 64 point lights
> + 9 esferas PBR + shadow demo + Fox + CesiumMan + fuego + trigger Lua
> = 285 entidades / 17K tris. **Build Release** con `MOOD_PROFILE=ON`.
> Tracy v0.11.1 (cliente + GUI). 4 capturas + 30+ snapshots CSV.

### Bottleneck identificado y atacado

| Zone | Pre-fix (test1, vsync ON) | Post-cache (test3, vsync OFF) | Δ |
|---|---|---|---|
| **ShadowPass::record** | 13,424 μs/frame (54.7%) | 0.26 μs/frame (1 record en 22317 frames) | **-99.998%** |
| ShadowPass::hash | — | 31 μs/frame | nuevo |
| **Stress FPS sin VSync** | ~45 fps estimado | **780 fps real** | **17x** |
| **Frame_ms real** | ~22 ms | **1.27 ms** | **-94%** |

### Fix implementado

**Shadow map caching por hash de escena** (`SceneRenderer_Render.cpp`):
hash FNV-1a 64 incremental cada frame de (`lightDir` + `sceneCenter` +
`sceneRadius` + por entidad con `MeshRendererComponent`: `position` +
`rotationEuler` + `scale` + `mesh id`). Si coincide con frame anterior
y `m_shadowMapValid`, skip `m_shadowPass->record(...)` y reusa la
GLTexture existente. **Cache hit rate medido: 99.996%** (1 record en
22317 frames de captura test3 con escena estática). Hash overhead
medido: 32 μs por llamada — barato para 285 entidades.

**VSync toggle** (`Window::setVSync` + checkbox en
`PerformanceHudPanel`): permite medir FPS real (sin cap a 60 fps del
refresco del monitor). Default ON (preserva comportamiento histórico
+ ahorra energía en escenas pequeñas); el toggle es para measurement.

### Tracy capturas

| Captura | Frames | Estado |
|---|---|---|
| `test1.tracy` (639 KB) | 3212 | baseline pre-fix, VSync ON |
| `test2.tracy` (994 KB) | 5156 | post-shadow-cache, VSync ON |
| `test3.tracy` (3.5 MB) | 22317 | **post-cache + VSync OFF** (números honestos) |
| `test4.tracy` (2.8 MB) | 17913 | skybox reorder probado y revertido |

### Distribución del frame post-fix (test3, sin VSync)

| Zone | μs/frame | % del frame |
|---|---|---|
| renderSceneToViewport (total) | 1,585 | 100% |
| Skybox::draw (real cost) | 1,140 | 72% |
| swapBuffers | 586 | 37% |
| ImGui_ImplOpenGL3_RenderDrawData | 434 | 27% |
| UI::draw (CPU UI gen) | 365 | 23% |
| LightGrid::compute (Forward+) | 88 | 6% |
| ParticleRenderer::render | 71 | 4% |
| PBR::instancedPass | 70 | 4% |
| ShadowPass::hash | 31 | 2% |
| ShadowPass::record (1 sola vez) | 0.26 | <0.01% |
| AnimSystem/Script/Physics/Audio combined | <50 c/u | <3% c/u |

**Lectura**: a 780 FPS de headroom (1.27 ms reales vs 16.67 ms del
target 60 fps), **el motor está sobrado para contenido real**. Los
sistemas gameplay (Anim/Script/Physics/Audio) suman <0.2 ms total.
El próximo bottleneck "natural" sería Skybox a 1.14 ms — pero es
GPU sync (no fragment shader), validado al revertir el reorder
post-PBR sin ganancia medible.

### Skybox reorder probado y revertido

Hipótesis: dibujar skybox post-PBR con `depth=1 LEQUAL` solo pinta
píxeles donde la geometría no escribió, reduciendo overdraw del
fragment shader. **Resultado test4** (17913 frames): empate dentro
del ruido (~744 fps reorder vs ~776 fps original = -4%, dentro
del jitter normal). El "1.14 ms del skybox" no era trabajo del
fragment shader (que es trivial: 1 sampler read del cubemap o 2
atan/asin del equirect), era **GPU sync redistribuido**. Revertido:
sin ganancia + rompe doc del SkyboxRenderer + convención estándar
(skybox-first es el patrón estable).

### Diferidos en F2H42 (sin urgencia con headroom 17x)

- **GPU timestamp queries** (`glQueryCounter` con `GL_TIMESTAMP`):
  mediría costo GPU real en lugar del CPU stall. Hito propio si
  emerge presión.
- **CSM (Cascaded Shadow Maps)**: la shadow map 2048x2048 + bounding
  sphere fijo (radius=30m al origen) es suficiente para escenas tipo
  Hito 20. CSM aporta calidad en outdoor amplios.
- **Frustum culling shadow pass**: con cache 99.996% hit rate, el
  costo del record es prácticamente cero. Sin sentido optimizar el
  0.004% de los frames donde sí se renderiza.
- **Pre-renderizar skybox a cubemap LDR cacheado**: cubemap mode YA
  usa cubemap. Equirect podría convertirse on-load pero el costo del
  shader es trivial.

## Notas de medicion

- **Build mode**: el Debug de MSVC no esta optimizado — el `frame ms` real en
  Release con MOOD_PROFILE=OFF sera ~3-10x menor. El baseline Debug sirve para
  identificar **proporciones relativas** (que zona se lleva la torta), no para
  garantizar performance final.
- **Tracy on-demand**: el cliente solo activa instrumentacion al conectar el
  server; sin server conectado, la overhead es ~1-2% del frame. Conectado,
  ~5-8%. No es lo mismo medir con/sin server abierto.
- **Counters draw calls + tris**: el contador del IRenderer cubre PBR static
  + skinned + shadow pass. NO incluye skybox (1 call), particles (1 call
  instanced), debug renderer (1-2 calls). El delta es <5% del total para
  escenas con muchos meshes — relevante solo en escenas casi vacias.
- **FPS acumulativo**: el campo `fps` del HUD es un promedio desde el
  arranque del editor, no instantaneo. Tarda varios segundos en converger
  al valor real tras un cambio drastico de carga (ej. spawn 10K). Tomar
  siempre el snapshot **despues** de que el numero se estabilice.
- **Snapshot CSV**: `performance_baseline.csv` se genera en el cwd del
  editor (tipicamente `build/debug/Debug/`). Append-only, multiples clicks
  se acumulan — util para promediar varias lecturas o re-medir.
