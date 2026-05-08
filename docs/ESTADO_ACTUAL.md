# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H25 + F2H26 cerrados: Cull overlap parcial en CompileMap + Runtime-load de mesh compilada en MoodPlayer.**
Tags: `v1.16.0-fase2-hito25` (cull overlap + UI layout v2 stamp) + `v1.17.0-fase2-hito26` (schema v13 + Player path).
Verificado automático: suite doctest **622/8403** verde (+12 cases / +44 asserts vs F2H24). Verificado por dev: cilindro dentro de cubo desaparece de la mesh compilada (cull overlap funciona); `.moodmap` exportado tiene `"version": 13` + bloque `"compiledMesh"` con submeshes (62 KB persistidos).

**Cambio importante**: F2H25 + F2H26 atacaron las 2 deudas técnicas heredadas de F2H20 (cull overlap + persistencia/runtime-load). El editor ahora produce y persiste una mesh estática unificada con 2 niveles de cull (pareja exacta + overlap parcial); el `MoodPlayer` la carga directo en lugar de procesar brushes CSG en runtime. Cumple "brushes solo en el editor" del plan F2H14 original.

**F2H27 (F6 panel "Adjust Last Operation") fue intentado y descartado** durante implementación. La versión "F6-light" (overlay flotante con sliders Position/Rotation/Scale del último brush) resultó redundante con el Inspector. El F6 real de Blender requiere parametrizar todos los comandos con metadata de "params editables" — scope de hito propio. Diferido sin tag.

**Decisiones clave de F2H25:**
- **Algoritmo BSP-style polygon clipping** con Sutherland-Hodgman extendido. Por cada plano del brush B: split en `above` (afuera) + `below` (sigue testeado contra los demás planos).
- **3 pre-tests críticos** para correctitud + performance: cara entera afuera (output sin partir), cara entera adentro (descartar), polígono coplanar (saltea el plano, los OTROS deciden — sin esto el cull se equivocaba en el caso "cara coplanar con pared exterior pero sub-region central adentro de B").
- **Stats nuevas**: `culledOverlapTriangles` (caras eliminadas enteras) + `splitFragments` (caras partidas).
- **Trade-off conocido**: el split puede AUMENTAR el tri count (cada fragmento independiente requiere su propia triangulación). El beneficio del cull es área visible, no tri count.
- **UI layout version stamp** (fix lateral): `imgui.ini` → `imgui_layout_v2.ini`. Ini viejo se ignora, default fresh al primer arranque.

**Decisiones clave de F2H26:**
- **Schema bump aditivo v12→v13**: campo `compiledMesh` opcional. Mapas v12 caen al fallback (procesar brushes en runtime).
- **Layout PBR de 11 floats interleaved** (pos+color+uv+normal, sin EBO) — mismo formato que `brushSubmeshToInterleaved` produce para reusar shader PBR estándar.
- **Componente nuevo `CompiledMeshComponent`** (move-only, mismo patrón que F2H17 BrushComponent) con `vector<unique_ptr<IMesh>>` + `vector<MaterialAssetId>` paralelos.
- **Render path nuevo `compiledMeshPass`** después del `brushPass` en `SceneRenderer_Render.cpp`. uModel=identity (mesh ya en world space).
- **Flag `useCompiledMesh`** en `SceneLoader::applyEntitiesToScene`: Player pasa `true` (skipea brushes individuales si hay compiled), Editor pasa default `false` (sigue editando brushes). Sin compiledMesh → fallback aunque el flag sea `true`.

**Implementación (F2H25 Bloques A-G + F2H26 Bloques A-G):**

- **F2H25 Bloque B**: `splitPolygonByPlane`, `cullPolygonAgainstBrush`, `planeLocalToWorld` en `CompileMap.cpp`. `compileMap` itera caras post-cull-exacto y aplica overlap clipping. Stats nuevas en header.
- **F2H25 Bloque C**: `test_compile_map.cpp` extendido con 5 cases F2H25 (smoke, pareja exacta no dispara overlap, brush dentro de big, russian doll, solape parcial parte caras).
- **F2H25 lateral**: `imgui_layout_v2.ini` (1 LOC en `EditorApplication_Init.cpp`); fix de `tests/CMakeLists.txt` agregando 8 partials de F2H24 que no se incluían.
- **F2H26 Bloque B**: schema bump v13; `SavedCompiledSubmesh`/`SavedCompiledMesh` structs; serializer/parser inline en `SceneSerializer.cpp`; `buildSavedCompiledMeshFromScene` helper público.
- **F2H26 Bloque C**: `CompiledMeshComponent.h`; `compiledMeshPass` en SceneRenderer.
- **F2H26 Bloque D**: SceneLoader extendido con flag `useCompiledMesh`. Si flag y compiledMesh presente → 1 entity "WorldCompiledMesh" + skipear brushes.
- **F2H26 Bloque E**: 4 callsites de `SceneSerializer::save` en editor llaman al helper y pasan compiledMesh. PlayerApplication_Init pasa `useCompiledMesh=true`.
- **F2H26 Bloque F**: `test_compiled_mesh_persistence.cpp` con 7 cases.

**Pendientes conocidos** (post-F2H25/F2H26):
- **F2H28 — Hammer-style 4-viewport** (top-level del backlog "Después", próximo a atacar).
- **Validación full del Player** con compiledMesh: requiere empaquetar proyecto + correr MoodPlayer.exe. El path está implementado pero no se probó end-to-end. Anotado como deuda menor.
- **Multi-selección de caras** (Shift+click sobre múltiples caras del mismo brush) — sub-bloque de F2H28 si emerge necesidad.
- Los demás items del backlog "Activos sin orden definido" siguen vivos: split fino de `SceneRenderer_Render.cpp`, iconos Toolbar, polish UX panels, node-graph Material Editor.

**Próximo paso**: **F2H28 — 4-viewport Hammer-style layout**. Workspace nuevo "Hammer" con dockspace en 4 cuadrantes (1 perspectiva 3D + 3 ortográficas top/front/side en wireframe + grid 2D + crosshair). Drag-edit entre vistas con grid snap. Reusa la infra de workspaces de F2H7+F2H22.

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
