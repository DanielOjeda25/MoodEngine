# Plan — F2H2: Tracy + benchmark sistemático de polígonos

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H1 cerrado), `PLAN_FASE2.md` sección 4 (F2H2),
> `DECISIONS.md` (entrada de F2H1).

---

## Objetivo

Instrumentar el motor con un profiler real (Tracy) y medir su comportamiento bajo carga
controlada. **No** es optimización — es **medición**. El resultado es un baseline
cuantitativo y una lista priorizada de cuellos de botella que F2H3 (frustum culling),
F2H4 (LOD) y siguientes pueden atacar con datos.

## Bloques

### A — Plan F2H2 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Integración de Tracy

- Agregar `wolfpld/tracy` via CPM en `CMakeLists.txt`.
- Definir `TRACY_ENABLE` solo en builds Debug + Release con `MOOD_PROFILE` ON
  (default ON en Debug, OFF en Release final). En Release sin profiling las
  macros son no-op total — coste **cero**.
- Crear `src/core/Profiler.h`:
  - `MOOD_PROFILE_FRAME()` — marca fin de frame (`FrameMark`).
  - `MOOD_PROFILE_SCOPE(name)` — zona con nombre fijo.
  - `MOOD_PROFILE_FUNCTION()` — zona con nombre = función.
  - `MOOD_PROFILE_PLOT(name, value)` — graficar valor numérico (FPS, entity count).
  - Cuando `TRACY_ENABLE` está OFF todas son `do{}while(0)` o castos vacíos.
- Tests: ninguno propio (Tracy no rompe build). Suite ya pasa actual = no regresión.

### C — Instrumentar puntos críticos del frame

Insertar `MOOD_PROFILE_SCOPE` en los hot paths que ya identificamos en `EditorApplication::run()`:

1. `processEvents` (SDL pump)
2. `m_ui.draw` (ImGui widget tree)
3. `updateRigidBodies` (Jolt step + sync)
4. `m_scriptSystem->update` (Lua per-entity)
5. `m_animationSystem->update` (skinning matrices)
6. `m_navSystem->update` (A* pathfinding)
7. `m_particleSystem->update` (SoA particle sim)
8. `m_audioSystem->update` (miniaudio sources)
9. `renderSceneToViewport`:
   - Shadow pass
   - Light grid build (Forward+)
   - Opaque draw
   - Particle draw
   - Skybox
   - Post-process
10. ImGui render (final present)

Plots: `MOOD_PROFILE_PLOT("FPS", fps)` + `MOOD_PROFILE_PLOT("EntityCount", n)`.

### D — Generador de escenas stress-test

Spawners en menú **Ayuda > Stress Test**:
- `Spawn 10K tris (cubos)` — 833 cubos × 12 tris = ~10K
- `Spawn 100K tris (cubos)` — 8333 cubos × 12 tris = ~100K
- `Spawn 500K tris (cubos)` — 41666 cubos × 12 tris = ~500K
- `Spawn 1M tris (cubos)` — 83333 cubos × 12 tris = ~1M

Cada spawner:
- Crea un grid 3D centrado en el origen (cube root de N → spacing 2.0).
- Cada cubo es un `Entity` con `TransformComponent` + `MeshRendererComponent`
  (usa `PrimitiveMeshes::cube()`).
- Material default (PBR base color blanco).
- Tag = `"StressCube_<i>"` para identificar y borrar en bulk si hace falta.
- Función helper `processSpawnStressTrisRequest(int targetTris)` en
  `editor/application/DemoSpawners.cpp`.

Justificación: cubos (12 tris/instance) presionan **draw call submission** (no batching)
+ **scene iteration** + **frustum culling cuando exista** (F2H3). Spheres con más
tris/instance presionarían vertex/fragment cost — válido pero secundario para baseline.

### E — Overlay de métricas detalladas

Nuevo panel ImGui flotante **Performance HUD**, toggleable desde menú **Ver > Performance**.
Contenido:
- FPS suavizado (ya existe en `FpsCounter`).
- Frame time (ms).
- Entity count.
- Draw calls del frame anterior (necesita contador en `OpenGLRenderer`).
- Triangle count del frame anterior.
- Memory: Jolt body count + active particles.

Counters: agregar a `OpenGLRenderer` `u32 frameDrawCalls()`, `u32 frameTriangles()`
+ resetear en `beginFrame`. Incrementar en cada `drawIndexed`.

### F — `docs/PERFORMANCE.md`

Plantilla con secciones:
- **Hardware baseline** (GPU, CPU, RAM, OS — relleno del usuario).
- **Build config** (Debug + Release, Tracy ON/OFF).
- **Resultados** por escena de stress-test (cubos, viewport 1280x720):
  - Empty scene → FPS, frame ms.
  - 10K / 100K / 500K / 1M tris → FPS, frame ms, top 3 zonas Tracy.
- **Top 5 cuellos de botella identificados** (con porcentaje de frame y zona Tracy).
- **Conclusiones** (qué hito siguiente ataca cada cuello).

### G — **PARADA: testing en PC**

Al terminar A-F el código está listo pero **no medido**. Aviso al usuario para
moverse a la PC con GTX 1660:
1. `cmake --build build/debug --config Debug` (compila Tracy y todo lo nuevo).
2. Lanzar `MoodEditor.exe` + Tracy server (`tracy-profiler.exe`) en paralelo.
3. Conectar Tracy → toggle Performance HUD → spawn 10K → 100K → 500K → 1M.
4. Capturar valores: FPS, frame ms, top zonas Tracy, draw calls.
5. Reportar números al chat.

### H — Documentación + cierre

Con los datos del usuario:
- Rellenar `PERFORMANCE.md` (resultados + top 5).
- Entrada en `DECISIONS.md` (2026-05-03 actualizado: "Tracy adoptado como profiler").
- `HITOS.md`: sección Fase 2 marca F2H2 cerrado.
- `ESTADO_ACTUAL.md`: actualizar.
- Tag: `v1.1.0-fase2-hito2`.

---

## Suite

Todo el código nuevo es **aditivo** (Tracy = include + macro, spawners = nuevos
métodos, overlay = nuevo panel). Las macros con `TRACY_ENABLE` OFF son no-op.
Suite debería seguir en **319/6613** sin regresiones.

## Riesgos

- **Tracy en CI**: el wrapper requiere Windows o Linux; en Windows MSVC compila
  sin issues conocidos. Si rompe, fallback es `MOOD_PROFILE=OFF` y commit Tracy
  detrás del flag.
- **Stress-test 1M tris en GTX 1660 a 60 FPS**: probablemente NO llegará — esto
  es esperado y deseado, justamente busca exponer el techo. Si crashea por OOM
  (poco probable; 83K cubos × ~200 bytes = ~17 MB de scene data), bajar a
  500K como max útil.
- **Draw call counter race**: `OpenGLRenderer` es single-threaded (todo render
  va en el thread principal), no hay race. Counter `u32` plano.

---

## Criterios de cierre

- [ ] Tracy compila + linkea en Debug.
- [ ] Tracy server (`tracy-profiler.exe`) puede conectarse y ver zonas instrumentadas.
- [ ] 4 spawners de stress-test funcionan (10K/100K/500K/1M tris).
- [ ] Performance HUD muestra FPS / frame ms / draw calls / triangle count / entity count.
- [ ] `docs/PERFORMANCE.md` con baseline real medido en GTX 1660.
- [ ] Top 5 cuellos identificados con datos.
- [ ] Suite **319/6613** sin regresiones.
- [ ] Tag `v1.1.0-fase2-hito2` pusheado.
