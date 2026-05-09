# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H30 cerrado: Polish del editor de mapas (vertex/edge edit + pincel poligonal + gizmo proporcional + W/E/R double-tap modal).**
Tag: `v1.20.0-fase2-hito30`.
Verificado por dev: vertex/edge edit funciona con snap absoluto al grid + rebasing al cierre, pincel poligonal crea prismas (4 vertices distintos → brush spawneado, dedupe corrigió "figura desaparece"), gizmo rotate sigue al AABB del brush, W/E/R con double-tap dispara modal Rotate/Scale con anillo amarillo visual, click confirma + Esc cancela.

**Cambio importante**: el esquema de atajos del modal pasó por **3 iteraciones** según feedback del dev:
- **Iter 1 (plan)**: G/R/S puros estilo Blender + W/E/R Maya conviven.
- **Iter 2**: dev pide "solo Blender, no Maya" → removidos W/E/R, R = modal Rotate, S = modal Scale.
- **Iter 3 (final, validado)**: dev pide hibrido double-tap → W = Translate gizmo, E single = Scale gizmo / E doble = modal Scale uniforme, R single = Rotate gizmo / R doble = modal Rotate libre. G y S removidos. Cuadrado central de uniform-scale gizmo eliminado (la S desapareció reemplazada por E doble).

**Decisiones clave de F2H30:**
- **Vertex/edge edit con snap WORLD-space (no local)**: el grid del workspace orto vive en world; snap en local quedaba desfasado para brushes con `tf.position != 0`. Solo se snappean los ejes que el dev MUEVE (`|deltaWorld[i]| > eps`), preservando coords no-grid-aligned en ejes intactos.
- **Rebasing al cierre del drag**: `newCentroidLocal = brush.localAabb.center()`, todos los planos se trasladan por `-newCentroid`, `tf.position += R * newCentroid`. Sin esto el gizmo quedaba en la posición vieja del brush. Mismo patrón que F2H12 boolean ops resolvió con `snapshotResultWorld`.
- **Dedupe en pincel**: dos clicks que snapean a la misma celda generaban polígono degenerado y `closePolygonDraw` cancelaba sin pista — el dev percibía "la figura desaparece". Fix: skipear el segundo click si está a < 1mm del último.
- **Gates anti-conflicto pincel/vertex-edit/block-tool**: activar pincel resetea sub-modo a Object + `activeFaceIndex = -1`; bloque 2.4e (vertex/edge edit) gateado con `!m_polyDraw.active`. Sin estos guards, un click del pincel en sub-modo Vertex disparaba edit accidental del brush selecto.
- **Toolbar lateral "Map Tools"** (columna derecha del workspace, ~10% ancho): pedido del dev *"hagamos uno superior"* mutado a *"prefiero columna lado derecho"*. Botones Objeto/Vertex/Edge/Cara/Pincel apilados verticalmente con highlight del activo + tooltips con shortcut.
- **Gizmo rotate radio = `0.6 * max(localAabb.size())` clamp >= 0.5m**: cubre BrushComponent (via `bc.brush.localAabb`) y MeshRendererComponent (via `MeshAsset::aabbMin/Max`). Antes era fijo 1m world — invisible dentro de brushes grandes.
- **W/E/R + double-tap (<0.4s) hibrido Maya/Blender**: scheme final que el dev validó. State `GizmoKeyTapState { lastKey, lastPressTime }` detecta double-tap. Single-tap toggea `GizmoMode`; double-tap arranca modal con misma tecla. Sin shortcuts cruzados — cada tecla tiene un único significado.
- **Modal con axis-lock X/Y/Z**: durante el modal, presionar X/Y/Z lockea/destrabe el constraint (alineado con Blender). Click izq confirma (push `MultiEditTransformCommand`); Esc cancela (revert al startValue snapshot).
- **Visual feedback del modal**: anillo amarillo (radio = distancia cursor-centro) + línea cursor→centro durante Rotate; línea sola durante Scale. Pedido del dev *"que aparezca ese circulo para rotar"*.

**Implementación (F2H30 Bloques A-E en 4 commits):**

- **Bloque A (commit `5674812` previo + plan en `docs/PLAN_HITO_F2H30.md`)**: plan archivado en [`archive/plans/PLAN_HITO_F2H30.md`](archive/plans/PLAN_HITO_F2H30.md).
- **Bloque B (commit `03ed19a`)**: vertex/edge edit con `Csg::pickVertex/pickEdge` + `EditBrushGeometryCommand` snapshot pre/post de planos + `tf.position` + sub-modos `Vertex` (tecla 1) / `Edge` (tecla 2) en `EditorSubMode`.
- **Bloque C (commit `4db0bd0`)**: pincel poligonal con `Csg::makePrismBrushFromPolygon` + validación CCW con auto-revert + dedupe + toolbar lateral `MapEditorTopBar` + gates anti-conflicto.
- **Bloque D (commit `67611ec`)**: gizmo proporcional + W/E/R double-tap modal con `ModalShortcutState` + `GizmoKeyTapState` + visual feedback. Implementación en `EditorOverlay_Modal.cpp` separado para no inflar `EditorApplication_Run.cpp`.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H30):
- **MVP del editor estilo Hammer está completo**: 4-viewport + click-select + grid + block tool + drag-edit + vertex/edge edit + pincel poligonal + W/E/R modal. Próximo hito **TBD** — definir junto al dev qué prioriza.
- **Validación full del Player** con compiledMesh: deuda menor heredada de F2H26.
- **Multi-selección con marquee select** en orto (clásico Hammer rectángulo de selección sobre múltiples brushes): diferido — no bloquea el modeling flow.
- **Snap-to-vertex** (snap a vertices existentes del scene, no solo al grid): nice-to-have.
- **Brush poligonal cóncavo**: requiere refactor mayor del CSG core (asume convexos).
- Los demás items del backlog "Activos sin orden definido" siguen vivos: split fino de `SceneRenderer_Render.cpp`, iconos Toolbar, polish UX panels, node-graph Material Editor.

**Próximo paso**: **TBD — definir junto al dev**. El editor de mapas tiene su MVP funcional cubierto; queda alinear sobre qué priorizar (gameplay/scripting, polish UI/UX, optimización runtime, features nuevas del editor).

### F2H28 (anterior, ya cerrado)

**🚀 F2H28: Editor de mapas 4-viewport (MVP visual + select + grid).**
Tag: `v1.18.0-fase2-hito28`.

**Decisiones clave de F2H28:** workspace "Editor de mapas" (label castellano, no "Hammer") como 4to default + dockspace 2x2 (Top XZ / Viewport / Front XY / Side ZY) + 3 FBOs LDR + render orto wireframe (color flat celeste GMod / naranja selected) sobre fondo NEGRO (cambio dev sobre plan original gris) + grid 2D shader fullscreen con AA via `fwidth` (menor `(40,40,40)`, mayor naranja Valve `#F58220`) + refactor `pickEntity` → helper `pickEntityFromRay` para rayos paralelos del orto + snap step `m_hammerSnapStep` cycleable Ctrl++/Ctrl+- con label "Grid: Nu" arriba-derecha.

**F2H27 (F6 panel) fue descartado** durante implementación — redundante con Inspector; F6-real es scope grande propio.

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

### Tarea inmediata: definir y abrir el Hito F2H31 (TBD)

F2H30 está cerrado (tag `v1.20.0-fase2-hito30`). El editor estilo Hammer está completo en su MVP funcional. **F2H31 está TBD** — candidatos por discutir con el dev:
- Continuar polish UI/UX del editor de mapas (snap-to-vertex, marquee select, frustum perspectivo en orto, coordenadas world bajo cursor).
- Optimización runtime / build pipeline (validación full del Player con compiledMesh, profiling).
- Gameplay / scripting (raycasts + triggers en Lua, save/load gameplay).
- Features nuevas del editor (node-graph del Material Editor, iconos image-based del Toolbar).

### Flujo recomendado en esta sesión

1. Preguntar al dev qué prioriza para F2H31.
2. Una vez definido, crear `docs/PLAN_HITO_F2H31.md` con bloques A-E concretos.
3. Trabajar bloque por bloque marcando en el plan al cerrar cada uno.
4. Actualizar `docs/DECISIONS.md` cuando aparezca una decisión no trivial.
5. Al final: commits atómicos en español, merge a main, tag `v1.21.0-fase2-hito31`, actualizar este documento + `docs/HITOS.md`, archive del plan.

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
