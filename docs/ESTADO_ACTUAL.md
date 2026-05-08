# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H28 cerrado: Editor de mapas 4-viewport (MVP visual + select + grid).**
Tag: `v1.18.0-fase2-hito28`.
Verificado automático: suite doctest **622/8413** verde (+10 asserts del 4to workspace). Verificado por dev: workspace "Editor de mapas" muestra 4 cuadrantes (Top/Front/Side wireframe + perspectiva 3D), grid 2D sobre fondo negro con líneas mayores cada 8×snap en naranja Valve, click-select cross-viewport funciona (clickear en cualquier orto refleja la selección en las 4 vistas + Inspector), Ctrl++/Ctrl+- cicla snap [1, 2, 4, 8, 16, 32, 64, 128] con label "Grid: Nu" arriba-derecha de cada panel.

**Cambio importante**: F2H28 entrega los 4 fundamentos del editor estilo Valve Hammer: layout multi-viewport, render orto wireframe con grid 2D, click-select cross-viewport. **NO** entra el drag-edit ni el block tool — esos quedan para F2H29 (decisión documentada en el plan: split en 2 hitos para no romper todo de una). El snap setting expone el valor en UI pero no lo aplica al desplazamiento real todavía.

**F2H27 (F6 panel "Adjust Last Operation") fue intentado y descartado** durante implementación. La versión "F6-light" (overlay flotante con sliders Position/Rotation/Scale del último brush) resultó redundante con el Inspector. El F6 real de Blender requiere parametrizar todos los comandos con metadata de "params editables" — scope de hito propio. Diferido sin tag.

**Decisiones clave de F2H28:**
- **Workspace orientado a tarea con label castellano "Editor de mapas"** (no "Hammer") alineado con la convención F2H22; internamente seguimos hablando del estilo Hammer como inspiración técnica.
- **Layout 2x2 dockspace**: top-left Top (XZ), top-right Viewport (perspectiva 3D existente), bottom-left Front (XY), bottom-right Side (ZY). Convención Y-up del engine; los nombres de eje en mayúsculas y el sufijo `(plano)` en cada label distinguen las 3 ortos.
- **`OrthoCamera` como struct puro** (en `editor/panels/scene/`, no en `engine/`) — pan/zoom en coords locales right/up, view + proj computadas on-demand. Eligió no almacenar matrices cacheadas: trivial costo (6 mat4/frame) vs. complejidad de invalidación.
- **3 FBOs LDR** pre-creados al ctor del SceneRenderer (1280x720) + resize a demanda en `renderOrthoView`. ~30 MB extra. El render orto usa GL puro (bypassa el RHI) porque corre DESPUÉS del `endFrame()` del frame perspectivo — reusa el `meshCache` hidratado por el `brushPass` perspectivo (no regenera mesh).
- **Fondo NEGRO** (cambio sobre el plan original que decía gris claro `#C8C8C8`) — pedido del dev *"resalta mejor el wireframe celeste"*. Grid menor `(40,40,40)`, grid mayor naranja Valve `#F58220`, wireframe celeste GMod `#6CC1E5`, selected naranja saturado `#FF9933`.
- **Grid 2D como shader fullscreen** (no geometría) con AA via `fwidth(worldXY/spacing)` — grosor estable a cualquier zoom.
- **Refactor `pickEntity` → helper `pickEntityFromRay`** en ScenePick para que el orto picking use rayos paralelos (`origin = worldFromNdc(...) - forwardAxis*1024`, `dir = forwardAxis`) sin duplicar el broadphase.
- **Snap step solo de display** en F2H28 — `m_hammerSnapStep` en EditorApplication, `Ctrl + +` / `Ctrl + -` cicla `[1, 2, 4, 8, 16, 32, 64, 128]`, label "Grid: Nu" arriba-derecha. F2H29 lo aplica al drag-edit.
- **Skip total del render orto si workspace ≠ "Editor de mapas"** — guard en `EditorRenderPass.cpp` evita pagar el ~3x costo CPU en otros workspaces.

**Implementación (F2H28 Bloques A-G en 4 commits feat):**

- **Bloque A — Plan**: archivado en [`archive/plans/PLAN_HITO_F2H28.md`](archive/plans/PLAN_HITO_F2H28.md).
- **Bloques B-D (commit `44ea100`)** — scaffolding (heredado del pull): WorkspaceManager + Dockspace + OrthoCamera + OrthoViewportPanel + SceneRenderer_Ortho + shaders wireframe_ortho.
- **Bloque E (commit `8a66233`)** — grid 2D: shaders nuevos `grid2d.{vert,frag}` + cambio fondo gris→negro + `m_grid2dShader` + `m_grid2dVao` + `panOffset`/`worldHeight` en firma `renderOrthoView` + fix lateral tests workspace count 3→4.
- **Bloque F (commit `6036c19`)** — click-select cross-viewport: refactor `ScenePick` con `pickEntityFromRay` + bloque 2.4b en `EditorApplication_Run` con consume de los 3 ortos + Maya-style Shift/Ctrl modifiers + logs prefijados.
- **Bloque G (commit `cf8d4e5`)** — snap setting: `m_hammerSnapStep` + handler Ctrl++/Ctrl+- en processEvents + setter en panel + label "Grid: Nu" arriba-derecha + sincronización via EditorRenderPass.

**Pendientes conocidos** (post-F2H28):
- **F2H29 — Block tool + drag-edit + vertex edit en ortos** (continuación natural; el snap step que F2H28 expone se aplica al delta del drag).
- **Multi-selección de caras** (Shift+click sobre múltiples caras del mismo brush) — sub-bloque de F2H29 si emerge.
- **Validación full del Player** con compiledMesh: deuda menor heredada de F2H26 (path implementado pero no probado end-to-end con `MoodPlayer.exe`).
- Los demás items del backlog "Activos sin orden definido" siguen vivos: split fino de `SceneRenderer_Render.cpp`, iconos Toolbar, polish UX panels, node-graph Material Editor.

**Próximo paso**: **F2H29 — Block tool + drag-edit + vertex edit**. Continuación natural de F2H28: implementar las 3 features de edición que quedaron explícitamente afuera. El snap step expuesto en F2H28 se aplica al delta del drag (`pos = round(pos / snap) * snap`).

### F2H25 + F2H26 (anteriores, ya cerrados)

**🚀 F2H25 + F2H26: Cull overlap parcial en CompileMap + Runtime-load de mesh compilada en MoodPlayer.**
Tags: `v1.16.0-fase2-hito25` (cull overlap + UI layout v2 stamp) + `v1.17.0-fase2-hito26` (schema v13 + Player path).

**Decisiones clave de F2H25:** algoritmo BSP-style polygon clipping con Sutherland-Hodgman extendido + 3 pre-tests críticos (cara entera afuera/adentro/coplanar) + stats `culledOverlapTriangles` + `splitFragments` + UI layout v2 stamp (`imgui_layout_v2.ini`).

**Decisiones clave de F2H26:** schema bump aditivo v12→v13 con `compiledMesh` opcional + layout PBR 11 floats interleaved + componente `CompiledMeshComponent` move-only + render path nuevo `compiledMeshPass` + flag `useCompiledMesh` en SceneLoader (Player=true, Editor=false).

### F2H24 (anterior, ya cerrado)

**🚀 F2H24 cerrado + Bloque C extendido: Reorganización interna del código.**
Tags: `v1.15.0-fase2-hito24` + `v1.15.1-fase2-hito24-bloque-c`.
Verificado automático: suite doctest **610/8359** verde después de cada Bloque B.X. Verificado por LOC: ningún partial supera 500 LOC; los 5 archivos CRÍTICOS originales (5784 LOC totales) reorganizados en 1 núcleo + N partials cada uno.

**Distribución LOC final** (núcleo + partials, 5 CRÍTICOS):

| Original | LOC original | Núcleo | Partials | Max partial |
|---|---:|---:|---:|---:|
| `InspectorPanel.cpp` | 1338 | 77 | 10 (+ Internal.h) | 208 |
| `EditorProjectActions.cpp` | 1272 | 106 | 6 | 329 |
| `DemoSpawners.cpp` | 1188 | 41 | 4 (+ Internal.h) | 399 |
| `PlayerApplication.cpp` | 1160 | 111 | 3 | 484 |
| `EditorApplication.cpp` | 826 | 173 | 2 | 435 |
| `SceneRenderer.cpp` | 776 | 242 | 1 (+_Render 590) | 590 |
| `MeshLoader.cpp` | 767 | 279 | 3 (+ Internal.h) | 213 |
| `EditorOverlay.cpp` | 745 | 310 | 1 (+ Internal.h) | 460 |
| `AssetManager.cpp` | 743 | 256 | 5 | 276 |

**Cambio importante**: F2H24 cumple el pedido explícito del dev al cerrar F2H23 (*"creo que hay archivos demasiado grandes que te cuesta arreglar"*) + extendido tras pedido posterior *"esa deuda chica podes hacerla ahora"* para cubrir los 4 ALTO 700-780 LOC. Refactor puramente estructural — ningún cambio funcional, ningún cambio de API pública. El editor y el player arrancan idénticos al usuario final. Los 4 archivos ALTO (700-780 LOC: `SceneRenderer`, `MeshLoader`, `EditorOverlay`, `AssetManager`) quedan en `PENDIENTES.md` como deuda chica.

**Decisiones clave**:
- **Split por dominio funcional con archivos parciales de la misma clase**: cada partial implementa métodos privados de la clase declarada en el header. Patrón ya usado pre-F2H24 (`EditorApplication.cpp` ya tenía `EditorProjectActions.cpp` + `DemoSpawners.cpp` + `EditorOverlay.cpp` + `EditorPlayMode.cpp` + `EditorRenderPass.cpp` + `EditorScene.cpp` desde Hito 16); F2H24 lo extiende a los 5 CRÍTICOS de Fase 2.
- **API pública intacta**: solo `InspectorPanel.h` recibió 13 métodos privados nuevos (`renderTagSection(Entity)`, `renderTransformSection(Entity)`, etc.) para que el dispatch del `onImGuiRender` quede legible — esto es API privada, no rompe nada externo.
- **Helpers compartidos via header interno** (`Foo_Internal.h`): namespace `Mood::detail` con `inline` functions y templates para que cada TU tenga acceso. Patrón aplicado a InspectorPanel (`pushEditIfDone` template + `helpMarker` + `isDragActiveOfType`) y DemoSpawners (`WorldYBounds` + `rotatedAabbWorldY`).
- **Cap soft 500 / hard 800 reafirmado**: ningún partial supera 500 LOC excepto `_Frame` y `_Run` que rondan 435-484 (loops monolíticos sin sub-secciones obvias para particionar más). Aceptable.
- **Validación incremental por Bloque B.X**: build + suite verde después de cada split antes de pasar al siguiente. Permite detectar regresiones rápido. La suite 610/8359 quedó idéntica antes y después de cada bloque (refactor sin cambios funcionales).
- **Bloque C (split de ALTO 700-780 LOC) skipped por presupuesto**: 4 archivos movidos a `PENDIENTES.md` como deuda chica. F2H24 entregó los 5 CRÍTICOS — los ALTO son menos urgentes (todos < 800 LOC = bajo el hard cap).
- **Errores resueltos durante el trabajo** (no requieren mención del dev pero quedan registrados): `glm/gtx/compatibility.hpp` removido del `InspectorPanel_Brush.cpp` (extensión experimental no necesaria); `APIENTRY` reemplazado por `GLAD_API_PTR` en `EditorApplication_Init.cpp` (no depende de inclusión transitiva de `<windows.h>`); `FrameStats` requiere include de `IRenderer.h` (definición completa) en `EditorApplication_Run.cpp` además de `SceneRenderer.h` (forward-decl).

**Implementación (Bloques A-E):**

- **Bloque A — Plan F2H24 + auditoría**: subagente recorrió `src/` con `wc -l`. Confirmé números reales con conteo directo (los del subagente eran ligeramente bajos por path antiguo). Plan documentado en `docs/archive/plans/PLAN_HITO_F2H24.md`.
- **Bloque B.1 — InspectorPanel.cpp**: 1338 LOC → núcleo 77 + 10 partials + Internal.h. Tag intermedio: `1cce00a`.
- **Bloque B.2 — EditorProjectActions.cpp**: 1272 LOC → núcleo 106 + 6 partials. Tag intermedio: `05e9afe`.
- **Bloque B.3 — DemoSpawners.cpp**: 1188 LOC → núcleo 41 + 4 partials + Internal.h. Tag intermedio: `dba4aa7`.
- **Bloque B.4 — PlayerApplication.cpp**: 1160 LOC → núcleo 111 + 3 partials. Tag intermedio: `01a80ae`.
- **Bloque B.5 — EditorApplication.cpp**: 826 LOC → núcleo 173 + 2 partials. Tag intermedio: `9abc38b`.
- **Bloque C extendido — split de ALTO 700-780 LOC** (atacado tras pedido del dev): C.1 AssetManager 743→256+5 partials, C.2 EditorOverlay 745→310+_Gizmo 460+Internal.h, C.3 MeshLoader 767→279+_Skeleton 184+_Geometry 213+_Animation 68+Internal.h, C.4 SceneRenderer 776→242+_Render 590 (deuda chica aceptada: frame loop monolítico bajo hard cap). Tag `v1.15.1-fase2-hito24-bloque-c` tras el cierre.
- **Bloque D — validación**: suite 610/8359 verde tras cada Bloque B.X; build OK Debug; cap soft 500 respetado en todos los partials nuevos.
- **Bloque E — cierre**: este documento + HITOS + DECISIONS + PENDIENTES + tag `v1.15.0-fase2-hito24`.

**Pendientes conocidos** (memoria + `PENDIENTES.md`):
- **F2H25 — 4-viewport Hammer-style layout** (próximo, heredado de F2H22→F2H23→F2H24 candidato): workspace nuevo "Hammer" con dockspace en 4 cuadrantes.
- **Split de archivos ALTO** — **CERRADO en Bloque C extendido tag `v1.15.1`**. Quedan documentadas las 4 splits + la deuda chica residual en `SceneRenderer_Render.cpp` (590 LOC, sobre soft 500 pero bajo hard 800 — frame loop monolítico).
- **Iconos image-based del Toolbar** (deuda explícita F2H22): FontAwesome / IcoMoon. Hito chico futuro.
- **CompoundCommand para batch undo** (deuda parcial cerrada en F2H23 con MultiEditTransformCommand).
- **Pase de polish UX continuo (siguiente ronda)**.
- **Node-graph del Material Editor** (deuda F2H21).
- **Runtime-load mesh compilada en MoodPlayer** (deuda F2H20).
- **Cull overlap parcial** (deuda F2H20).

**Próximo paso**: **F2H25 — 4-viewport Hammer-style layout**. Workspace "Hammer" con 4 cuadrantes: 1 perspectiva 3D + 3 ortográficas top/front/side en wireframe + drag-edit entre vistas con grid snap. Tradeoff: ~4× costo CPU del frame — mitigar reusando shadow pass / light grid compartido. Reusa la infra de workspaces de F2H7+F2H22 con un `buildHammerWorkspace` nuevo en `Dockspace.cpp`.


### Hitos anteriores

Para los detalles de hitos cerrados previos a F2H24 (F2H23, F2H22, ..., F2H1, Fase 1, Hito 0-42), ver [`archive/ESTADO_HISTORICO.md`](archive/ESTADO_HISTORICO.md). Split aplicado en F2H26 cierre — este documento mantiene solo el current state + último hito cerrado.

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real instalado en la máquina del dev

- **Visual Studio 2022 Community** → tiene MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Este es el que usamos.**
- **Visual Studio 2026 Community** → instalado **sin** workload de C++. No tiene compilador. Ignorar hasta que el dev lo complete o desinstale.

La ruta clave es:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

### Cargar entorno MSVC desde PowerShell (el comando que funciona)

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde un **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente sin esto.

### Versiones verificadas

- `cl` → `Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35226 for x64`
- `cmake --version` → `3.31.6-msvc6`
- `git --version` → `2.49.0.windows.1`

### Generador CMake correcto

`Visual Studio 17 2022` (ya está en `CMakePresets.json`, no tocar).

---

## 3. Comandos que ya corrieron exitosamente

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug
```

Resultado: `build/debug/Debug/MoodEditor.exe` (3.9 MB) + `SDL2d.dll` (4.3 MB) copiada automáticamente al mismo directorio.

Para ejecutar:

```
.\build\debug\Debug\MoodEditor.exe
```

---

## 4. Qué tiene que hacer el próximo agente

### Tarea inmediata: definir y abrir el Hito 33

El Hito 32 está cerrado (tag `v0.32.0-hito32`). Deudas de undo del Inspector cerradas. **Hito 33 está TBD** — candidatos en `docs/PLAN_HITO33.md`. Top: raycasts + triggers en Lua (combo natural post-31/32), save/load gameplay, UI menu MoodPlayer, completar wire-up del Inspector restante (42 widgets), Inspector drop de textura.

NavAgent polish queda descartado permanentemente por decisión del dev — no proponerlo.

### Flujo recomendado en esta sesión

1. Leer `docs/PLAN_HITO33.md` (candidatos) y discutir con el dev qué se prioriza.
2. Una vez definido, trabajar bloque por bloque marcando en el plan al cerrar cada uno.
3. Actualizar `docs/DECISIONS.md` cuando aparezca una decisión no trivial.
4. Al final: commits atómicos en español, merge a main, tag `v0.33.0-hito33`, actualizar este documento y `docs/HITOS.md`, crear `docs/PLAN_HITO34.md`.

---

## 5. Gotchas conocidos — cosas que ya se aprendieron por las malas

1. **VS 2026 Community se instaló sin el workload de C++.** El `Developer Command Prompt for VS 2026` abre pero `cl` y `cmake` no existen. No depender de VS 2026 hasta que el dev agregue el workload o lo desinstale. Usar siempre VS 2022.
2. **El `Developer Command Prompt` normal de Windows** (sin MSVC) no tiene `cl` en PATH. Si un comando falla con "cl no se reconoce", revisar que el prompt diga "Visual Studio 2022" en el banner.
3. **SDL2 en debug se llama `SDL2d.dll`**, no `SDL2.dll`. El `add_custom_command` POST_BUILD del CMakeLists copia la variante correcta según `$<TARGET_FILE:SDL2::SDL2>`.
4. ~~**GLAD no está incluido en Hito 1.**~~ Resuelto en Hito 2: `EditorApplication.cpp` usa `<glad/gl.h>` y llama `gladLoaderLoadGL()` tras crear el contexto. GLAD y el loader interno de ImGui conviven sin conflictos porque glad2 prefija sus símbolos con `glad_`.
8. **GLAD v2 usa naming distinto de v1.** Header: `<glad/gl.h>` (no `<glad/glad.h>`). Source: `gl.c` (no `glad.c`). Loader: `gladLoaderLoadGL()` / `gladLoaderUnloadGL()`.
9. **Dos máquinas de desarrollo.** Notebook con Intel Iris Xe (GL 4.5 OK, menos performance) y desktop con AMD Ryzen 5 5600G + NVIDIA GTX 1660. En el desktop, forzar NVIDIA para `MoodEditor.exe` desde el Panel de Control NVIDIA (sino Windows puede elegir la iGPU AMD). `imgui.ini` y `build/` NO viajan por git: cada máquina genera lo suyo.
10. **Shaders se buscan con path relativo `shaders/default.vert`.** Funciona desde la raíz del repo (VS_DEBUGGER_WORKING_DIRECTORY) y desde el directorio del exe (post-build `copy_directory shaders/`). Si agregás shaders nuevos, la copia es automática.
5. **El primer `cmake --preset` tarda ~5 minutos** porque baja y compila subdeps de SDL2 + ImGui + spdlog + GLM. Ese tiempo solo ocurre la primera vez; después el build incremental es segundos.
6. **spdlog busca pthread en Windows** y sale un warning de que no lo encuentra. Es esperado y benigno en Windows.
7. **GLM tira warnings de `cmake_minimum_required` deprecated** porque su CMakeLists usa sintaxis vieja. Ignorable; no es nuestro código.

---

## 6. Recordatorios de filosofía (sección 9 del doc técnico)

- Ship something: no romper el build entre commits.
- No implementar hitos futuros "por adelantar trabajo".
- No añadir dependencias que no estén en la sección 4 del doc sin consultar.
- Comentarios en español.
- Convención de commits: `tipo(scope): mensaje` en español.
- Preguntar al dev antes de asumir ante ambigüedades.

---

## 7. Archivos clave que tocar cuando

| Para... | Tocar |
|---|---|
| Añadir una dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags de compilador | `cmake/CompilerWarnings.cmake` |
| Añadir un panel al editor | `src/editor/panels/*` + `EditorUI.(h|cpp)` |
| Registrar una decisión arquitectónica | `docs/DECISIONS.md` |
| Marcar progreso de hito | `docs/HITOS.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Al arrancar una sesión nueva

1. Leer este archivo entero.
2. Leer `docs/PLAN_HITO<N>.md` del hito en curso para ver qué tareas quedan.
3. Leer `MOODENGINE_CONTEXTO_TECNICO.md` si es la primera vez.
4. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5` para ver tags y cambios recientes.
5. Preguntar al desarrollador: "¿seguimos con el Hito en curso o pasó algo nuevo?"
6. Actuar según la respuesta.
