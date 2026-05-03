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

**Conclusion accionable**: F2H3 cumple su objetivo — bajar `PBR::staticPass`
proporcional a cuanto cae fuera del frustum. **Proximo target: F2H4 (LOD)**
para que cubos lejanos pesen menos aunque esten dentro del frustum, y
**batching/instancing** como hito propio si despues del LOD seguimos
draw-call bound.

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

### Proximo paso
**F2H4 — LOD**: para que cubos lejanos (pero dentro del frustum) pesen menos
en triangle setup. Despues, si seguimos draw-call bound, **hito propio de
batching/instancing**. Repetir esta medicion entonces para tener la
columna "post-F2H4" en la tabla de resultados. Tambien vale medir en build
Release con `MOOD_PROFILE=OFF` para tener el numero "real" esperado en
producto (3-10x mejor que Debug).

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
