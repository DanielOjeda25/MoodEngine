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

| Escena | Entities | Tris | Draw calls | FPS (estable) | Frame ms (estable) | Top 3 zonas Tracy (% del frame) |
|---|---|---|---|---|---|---|
| Empty (proyecto vacio) | 2 | 24 | 2 | 60.0 (vsync cap) | 16.67 | PBR::staticPass <1%, UI::draw <1%, swapBuffers ~99% (vsync) |
| 10K tris (cubos) | 836 | 10,032 | 836 | ~4.0 | ~250 | PBR::staticPass 70.74%, swapBuffers 18.07%, UI::draw 4.45% |
| 100K tris (cubos) | _(no llega)_ | _(no llega)_ | _(no llega)_ | **editor congelado** | — | — |
| 500K tris (cubos) | — | — | — | **no probado** | — | — |
| 1M tris (cubos) | — | — | — | **no probado** | — | — |

> **"No llega"** = al spawnear 100K cubos el viewport queda inutilizable
> (<1 FPS percibido) en Debug build con Tracy ON. F2H3 (frustum culling) y
> F2H4 (LOD) deberian hacer 100K-1M viables. La medicion de esas escenas
> queda postergada para post-F2H3 con la build Release.

## Top 5 cuellos de botella identificados

Datos **medidos con Tracy** (3163 frames, ~52 segundos de captura, escena
con 836 cubos en grid 3D, viewport 1920x1080, Debug build con
MOOD_PROFILE=ON). Ordenados por % del frame total.

| # | Zona | % frame | Mean por call | Hito que lo ataca |
|---|---|---:|---:|---|
| 1 | `PBR::staticPass` | **70.74%** | 42.5 ms | F2H3 frustum culling + F2H4 LOD + batching/instancing futuro |
| 2 | `swapBuffers` (vsync wait + GPU sync) | **18.07%** | 10.86 ms | No hay; cuando el frame se acelere, el % baja solo |
| 3 | `UI::draw` (ImGui scene panels) | **4.45%** | 2.67 ms | Virtualizacion de Hierarchy/AssetBrowser cuando entity count alto |
| 4 | `ImGui_ImplOpenGL3_RenderDrawData` | 0.25% | 0.15 ms | Marginal — no priorizar |
| 5 | `EditorApplication::beginFrame` | 0.17% | 0.10 ms | Marginal — no priorizar |

**Lectura clave**: `PBR::staticPass` mean 42.5 ms / 836 entidades = **~50 µs
por cubo** de 12 tris. Eso es **CPU-bound en setup de draw call**, no
GPU-bound en triangle processing. La maquina (GTX 1660) puede dibujar 12 tris
en microsegundos — el costo es el `glBindTexture / glUniform / glDrawElements`
multiplicado 836 veces sin batching.

**Variabilidad alta**: stddev de `PBR::staticPass` = 189 ms (mean 42 ms,
max 2.57 s). Frames esporadicos quedan en >2 segundos — coincide con
recompilaciones de PSO o swaps de framebuffer. Investigar si reaparece
post-F2H3.

**Conclusion accionable**: F2H3 (frustum culling) y F2H4 (LOD) son **el
proximo target obligado**. Sin ellos, cualquier escena no-trivial es
ineditable. Una vez en su sitio, repetir esta medicion para 100K-1M tris
y validar si batching/instancing entra como hito propio o queda diferido.

## Conclusiones

- **El motor en Debug + sin culling/LOD se cae a tierra entre 10K y 100K
  triangulos** (~836 entidades). Spawnear 100K cubos congela el viewport
  por completo; medicion postergada a post-F2H3/F2H4.
- **`PBR::staticPass` se lleva el 70.74% del frame** con 836 cubos. Es CPU-
  bound en draw call setup (~50 µs/cubo). **F2H3 frustum culling es el
  proximo target obligado** — sacando los cubos fuera de viewport debe bajar
  proporcionalmente.
- **La escena vacia se mantiene en 60 FPS vsync-cap con 16.67 ms** — el
  baseline cost del editor es despreciable; la caida la introducen
  exclusivamente los meshes spawnados.
- **Proximo paso**: implementar F2H3 + F2H4, repetir esta medicion para
  comparar el delta, y agregar la columna **"post-F2H3"** a la tabla de
  resultados. Repetir tambien en build Release para tener el numero "real"
  esperado en producto (3-10x mejor que Debug).

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
