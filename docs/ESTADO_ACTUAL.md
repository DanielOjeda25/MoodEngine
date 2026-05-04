# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H8 cerrado: Multi-mapa intra-proyecto + fix de save/load de tiles modificados.**
Tag: `v1.1.6-fase2-hito8`.
Verificado automático: suite doctest **396/6948** sin regresiones (+13 cases en `test_maps_manager.cpp` + casos del Floor en `test_save_load_full_roundtrip.cpp`). Verificado por el dev a ojo: crear proyecto, modificar tiles + Floor, guardar, crear segundo mapa, modificar, guardar, switchear entre mapas → todo persiste sin duplicación, sin freeze, los cambios se mantienen al cerrar y reabrir.

**Cambio importante**: cierra dos deudas técnicas a la vez.

**1) Multi-mapa intra-proyecto** (Save As completo, opción C que el dev eligió):
- Backend ya estaba: el `.moodproj` schema soporta `maps[]` y `defaultMap` desde Hito 6. F2H8 implementa el UX faltante.
- `src/editor/project/MapsManager.{h,cpp}`: helper PURO con invariantes (≥1 mapa, default + current dentro de la lista, dedup por `generic_string`).
- 5 handlers nuevos en `EditorApplication`: `handleNewMap`, `handleSaveMapAs`, `handleOpenMap`, `handleSetCurrentMapAsDefault`, `handleDeleteCurrentMap`. Cada uno con confirm popups donde aplica + sync del snapshot al `EditorUI`.
- UI `Archivo > Mapa`: submenu con 5 items + sub-submenu "Abrir mapa" listando los `project.maps[]` (current marcado con `*`, default con `[default]`).
- `handleSaveAs` (era stub desde Hito 6) ahora redirige a `handleSaveMapAs`.
- "Save Project As" (copiar carpeta entera del proyecto a otra ubicación) sigue **out-of-scope** — diferido para hito futuro si emerge necesidad.

**2) Fix del bug de save/load de Floor** (descubierto durante testing visual de F2H8):
- El dev mueve el `Floor` del piso, guarda, cambia de mapa, vuelve. El editor parecía freezar.
- Diagnóstico con datos del `.moodmap`: el Floor se persistía como entidad regular del JSON, pero al cargar el mapa, `rebuildSceneFromMap` creaba un Floor default + `applyEntitiesToScene` aplicaba el Floor del JSON → **2 planos 48×48 superpuestos** → fillrate brutal → freeze percibido.
- Fix en `SceneLoader::applyOneEntity`: el reemplazo de entidades existentes con mismo tag se extiende de `Tile_X_Y` (fix anterior) a también `Floor`. Lista hardcoded de "tags auto-generados por rebuild"; otros tags siguen permitiendo duplicados (necesario para Multi/CajaFisica en redo de comandos batch).
- Test cubre el caso exacto.

**Próximo paso:** F2H9 (era F2H7 plan original = "Documentación pública") o sub-fase 2.2 = CSG (era F2H9 plan). Decidible según necesidad.

### F2H7 (anterior, ya cerrado)
Tag: `v1.1.5-fase2-hito7`.
Verificado automático: suite doctest **367/6847** sin regresiones (+9 cases en `test_workspace_manager.cpp`). Verificado por el dev a ojo: 4 tabs (Layout/Scripting/Profile/Materials) en la misma fila del menú principal (estilo Blender), click en cada tab cambia layout completamente, panels editados (drag/resize) persisten al switchear y volver. Persistencia al `.moodproj` confirmada en proyecto nuevo.

**Cambio cualitativo de UX**, no optimización. Mejora cómo trabaja el dev día a día. Modela Blender:
- Workspaces son parte del proyecto (`.moodproj` schema con array `workspaces`).
- Switching = swap del layout completo de panels. Mismo proyecto, distinta vista.
- 4 defaults hardcoded; UI para crear/eliminar custom = hito futuro (F2H8+).
- Modificaciones manuales se auto-guardan al switchear (captura ini ImGui actual al workspace previo).
- Sin back-compat (el dev borró proyectos viejos): proyecto sin `workspaces` = poblar con los 4 defaults.

**Implementación**:
- `src/editor/workspace/Workspace.{h}`: struct `{name, iniLayout}`.
- `src/editor/workspace/WorkspaceManager.{h,cpp}`: PURO (sin ImGui). Lista + activo + helpers.
- `src/editor/ui/Dockspace.cpp`: layouts predefinidos via `ImGui::DockBuilder` (4 builders distintos según nombre del workspace).
- `src/editor/ui/MenuBar.cpp`: tabs como Buttons con highlight del activo (sin `BeginTabBar` para evitar conflictos con state interno de ImGui que producía flicker).
- `EditorUI::applyPendingWorkspaceSwitch` en `EditorApplication::beginFrame` ANTES de `ImGui::NewFrame` (la doc de ImGui prohíbe `LoadIniSettingsFromMemory` dentro de un frame activo).
- `IPanel::category()` virtual con default `"Scene"`. Override en panels de Assets/Debug.

**Pendiente (no bloqueante para cerrar el hito)**: bug pre-existente en save/load del scene reportado por el dev — modificaciones de Transform.scale + materiales asignados no se restauran al reabrir el proyecto. Investigación profesional con tests = próximo paso inmediato.

**Próximo paso:** **investigar y arreglar el bug de save/load** con tests reales. Después F2H8 (docs públicas + iconos en tabs si querés) o F2H10+ (CSG).

### F2H6 (anterior, ya cerrado)
Tag: `v1.1.4-fase2-hito6`.
Verificado automático: suite doctest **358/6814** sin regresiones (+12 cases en `test_lod.cpp` + 1 case en `test_render_batching.cpp`). Verificado por el dev a ojo: editor compila + arranca, panel Inspector muestra info de LODs read-only para meshes con LODs generados. Validación con escenario de meshes complejos (Fox.glb / kenney_survival) postergada — los stress de cubos primitivos no se benefician (12 tris ya, no hay reducción posible). Build Release confirmó (post-F2H5) que el motor procesa 1M tris a 58 FPS sin LODs — el LOD entra como cimiento para cuando llegue contenido AAA real, no como optimización urgente.

**Cambio importante:** retoma el LOD original (era F2H4 plan, postergado dos veces — primero por instancing F2H4, después por virtualización F2H5). Implementación con **cimiento sólido** (filosofía industria estándar Unity/Unreal/Godot, documentada en DECISIONS.md):

- **`meshoptimizer v0.21` via CPM**: nueva dep para `meshopt_simplify`. Linkeada a `mood_engine_lib` y `mood_tests`.
- **`MeshAsset` extendido**: `lod1Submeshes` / `lod2Submeshes` (vacíos = no LOD generado, fallback a LOD 0). Campo `lodDistances{30, 80}` configurable per-mesh con default global. Helper inline `selectLod(distance, thresholds)` + método `submeshesForLod(lod)` con fallback automático.
- **`LodCache` (`assets/.cache/lods/<hash>.moodlod`)**: cache binario lateral al `.moodmap` formato. FNV-1a 64-bit del logical path como nombre. Header con magic/version/mtime/size para invalidación. Borrarlo NO rompe el proyecto — solo fuerza re-generación. Documentado en `.gitignore` (`assets/.cache/`).
- **`MeshLoader::loadMeshWithAssimp` auto-gen**: tras cargar el mesh original, si NO es skinned y >100 tris, intenta cargar LODs del cache; si miss, llama `meshoptimizer::simplify` con ratios 50%/15% (errores 0.05/0.10) y persiste. Skinned salta (re-mapping de bone weights con LOD = scope grande con risk visual, postergado a hito futuro).
- **`groupByBatch` extendido**: `BatchKey` gana campo `lod`, recibe `cameraPos`. Calcula distancia camera→centro AABB world-space y llama `selectLod` con thresholds del mesh. Entidades del mismo mesh+material pero distinto LOD van a batches separados.
- **`SceneRenderer::renderScene`**: pasa `cameraPos` a `groupByBatch`, usa `submeshesForLod(key.lod).front().mesh` con fallback automático. Plots Tracy nuevos: `PBR::InstancesLod{0,1,2}` con la distribución por nivel.
- **Inspector**: muestra read-only info de LODs cuando hay MeshRendererComponent seleccionado (tris LOD 0/1/2 + thresholds en m). Editar manualmente o regenerar = hito futuro.

**Mejora medida**: validación con escenarios de meshes complejos pendiente. Para los stress patológicos de cubos primitivos (12 tris/instance), el LOD no reduce nada porque ya están en el mínimo. Cuando entre contenido real (Fox.glb ~1500 tris, kenney_survival props ~500-2000 tris cada uno), spawnar 50+ instancias a distintas distancias mostrará el speedup esperado del LOD 1 (50%) y LOD 2 (15%).

**Próximo paso:** **F2H7 — Reorg UI editor por categorías** (era F2H6 según orden plan original) o **F2H8 — Documentación pública** (era F2H7). Ambos son sub-fase 2.1 cierre. Después: sub-fase 2.2 = CSG arquitectura (F2H9 plan).

### F2H5 (anterior, ya cerrado)
Tag: `v1.1.3-fase2-hito5`.
Verificado automático: suite doctest **345/6736** sin regresiones (+5 cases en `test_hierarchy_collect.cpp`). Verificado por el dev a ojo: panel Hierarchy se ve idéntico al pre-F2H5; con 8336 entidades (stress 100K) el panel scrollea fluido. Selección persiste correcta, orden estable. CSV confirma mejora del frame ms.

**Swap vs PLAN_FASE2 (segundo swap):** F2H5 era LOD según el plan. El cuello que F2H4 destapó (`UI::draw` 19% del frame con 8336 entries en Hierarchy) era atacable con un patrón estándar — decidimos hacer eso primero. LOD pasa a F2H6+. Documentado en DECISIONS.md.

**Cambio importante:** primera virtualización ImGui en el motor. Refactor de `src/editor/panels/scene/HierarchyPanel.cpp`: cache `vector<HierarchyEntry{handle,tag*}>` cacheado por frame (storage reusable entre frames vía `clear()+reserve`), iteración con `ImGuiListClipper` — solo las ~30 entries visibles del scroll area se procesan, no las 8336. Helper `collectHierarchyEntries` extraído como función pura en `HierarchyCollect.cpp` para testing aislado de ImGui.

**Mejora medida**: ~6% del frame ms con 8336 entidades (96→90.5 ms, 10.4→11.0 FPS). **Por debajo del 25-40% predicho** en el plan F2H5 — el cuello `UI::draw` no era dominantemente el Hierarchy sino una **suma** con Inspector + AssetBrowser + Performance HUD + gizmos. **Aprendizaje aceptado**: las predicciones de % de mejora con escenas grandes son frágiles si no hay attribución por panel separada en Tracy. El refactor es correcto y el patrón ListClipper queda reusable para otros panels que crezcan.

**Cuello real dominante con escenas patológicas (8336+ entidades en Debug):** scene iteration distribuida — múltiples sistemas (`Animation/Script/Nav/Particle/Audio/Trigger`) hacen `scene.forEach<...>` cada frame, sumando costo lineal por entidad fuera del rendering. Atacable con caching de queries entt o pipeline de sistemas — fuera del scope F2H5.

**Importante (contexto que cambia la urgencia)**: todas las mediciones F2H2-F2H5 son en **build Debug** + Tracy ON. MSVC Debug agrega 5-10x overhead vs Release. Antes de seguir optimizando, **medir Release** es el siguiente paso natural. Predicción: 100K_full_view pasaría de 11 FPS / 90 ms (Debug) a **45-60 FPS / 16-20 ms (Release)** sin tocar código. Si Release confirma rendimiento sano, el motor está listo para contenido real y los próximos hitos pueden ser features (LOD, CSG, etc) en lugar de optimizaciones desesperadas.

**Aclaración de scope sobre "100K tris"**: el stress de 100K tris usa **8336 cubos primitivos individuales** (12 tris cada uno). Mapas reales (HL2/Skyrim/Doom) tienen MUCHOS más triángulos pero MUCHAS menos entidades (200-1500 simultáneas) porque la geometría del nivel está en 1-3 meshes grandes + props. F2H3 (cull) + F2H4 (instancing) cubren ambos extremos. **El stress 100K es patológico, no representativo de un mapa real.**

**Próximo paso:** **medir Release** (build incremental, mismo flujo de snapshots, comparar). Después decidir F2H6 con datos: probable LOD original (postergado dos veces) o ChildrenComponent + folding del Hierarchy (UX + perf).

### F2H4 (anterior, ya cerrado)
Tag: `v1.1.2-fase2-hito4`.
Verificado automático: suite doctest **340/6678** sin regresiones (+9 cases en `test_render_batching.cpp`, -1 caso obsoleto). Verificado por el dev a ojo + Tracy: 836 cubos del stress 10K → **3 draws / 60 FPS (vsync cap)**; 8336 cubos del stress 100K (antes congelaba el editor) → editable a 10.4 FPS; visual comparado con/sin instancing — idéntico. Captura `test3.tracy` confirma `PBR::instancedPass` mean 0.88 ms vs F2H2 baseline 42.5 ms.

**Swap vs PLAN_FASE2.md:** F2H4 era LOD según el plan original. Con datos del baseline F2H2 vimos que el cuello real era CPU-bound en **draw call submission** (50 µs por cubo de 12 tris), no triangle setup. Entonces F2H4 = instancing y F2H5 = LOD. Documentado en DECISIONS.md con razones. El LOD no se descarta — sigue siendo necesario cuando aparezca contenido con meshes complejos (Fox/CesiumMan, props Kenney).

**Cambio importante:** segunda optimización medible de Fase 2. Helpers nuevos:
- `src/engine/render/scene_renderer/RenderBatching.{h,cpp}`: helper PURO `groupByBatch(scene, assets, frustum)` → `BatchMap<(meshId,materialId), vector<mat4 model>>` + `vector<Entity> nonBatchable` + `culledCount`. Reglas v1: 1 submesh + materials.size() ≤ 1.
- `src/engine/render/backend/opengl/OpenGLInstanceBuffer.{h,cpp}`: VBO recyclable con orphan upload (`glBufferData GL_DYNAMIC_DRAW`) y `bindAsAttributeMat4` que setea las 4 columnas en locations consecutivas con divisor=1.
- `IRenderer::drawMeshInstanced` agregado al RHI; `OpenGLRenderer` lo implementa con `glDrawArraysInstanced`. Counter `frameStats().drawCalls` se incrementa en 1 (un solo draw call), `triangles` en `instanceCount * (vc/3)`.
- `shaders/pbr_instanced.vert`: clon de `pbr.vert` con `mat4 aModel` como atributo de instancia. Reusa `pbr.frag`.
- `SceneRenderer::renderScene`: agrupa por (mesh, material) y emite 1 `drawMeshInstanced` por batch. Non-batchables caen al path no-instanced del F2H3 (fallback). Plots Tracy nuevos: `PBR::BatchedDrawCalls`, `PBR::Instances`.

**Mejora medida (vs F2H2 baseline):** `PBR::instancedPass` mean **0.88 ms** vs `PBR::staticPass` baseline mean **42.5 ms** = **~48x más rápido por draw**. Escena 10K: 836→3 draws, 4→60 FPS. Escena 100K: antes congelado, ahora 10.4 FPS / 3 draws / 82K tris. F2H3 + F2H4 en cadena hacen viable un escenario que el motor literalmente no soportaba.

### F2H3 (anterior, ya cerrado)
Tag: `v1.1.1-fase2-hito3`.
Verificado automático: suite doctest **331/6645** sin regresiones (+12 cases en `test_frustum.cpp`). Verificado por el dev a ojo: editor + spawn 10K cubos + cámara apuntando al grid → 4.4 FPS (igual baseline F2H2 — todo dentro del frustum); cámara apuntando lejos → 60 FPS vsync-cap (835/836 cubos descartados); CSV `performance_baseline.csv` y captura Tracy `testtrace2.tracy` confirman los números.

**Cambio importante:** primera optimización medible de Fase 2. Nuevo header `src/engine/render/pipeline/Frustum.h` con `Frustum`, `Plane`, `frustumFromViewProj` (extracción Gribb-Hartmann), `aabbVisible` (test conservador con truco p-vertex — 1 dot product por plano), `worldAabb` (transforma AABB local→world rotando 8 vértices). Gate de visibilidad en `SceneRenderer::renderScene` antes del pase opaco estático: descarta entidades cuyo AABB world-space cae fuera del frustum. Plot Tracy `PBR::CulledStatic` reporta el conteo descartado por frame.

**Mejora medida (vs F2H2 baseline):** `PBR::staticPass` baja de **70.74% → 15.73%** del frame promediando los 3 escenarios de cámara (full / half / no_view). Stddev baja 3x, max 10x. Caso extremo (cámara apuntando lejos del grid 10K): frame ms **222 → 16.67** = **speedup x13.3**, draws **836 → 1**. Cuando todo cae dentro del viewport, el cull no inventa mejora (esperado). Costo del culling: <<1% del frame, no aparece en top 10 zonas.

**Scope intencional v1:** solo PBR static. Shadow + skinned passes excluidos (ROI bajo según baseline F2H2). Implementación plana (loop linear) — jerárquico (BVH/octree) postergado a hito propio si una medición posterior lo justifica. API extensible: el upgrade interno no rompe callers.

### F2H2 (anterior, ya cerrado)
Tag: `v1.1.0-fase2-hito2`.
Verificado automático: suite doctest **319/6613** sin regresiones (todo el código nuevo es aditivo + macros no-op cuando `MOOD_PROFILE=OFF`). Verificado por el dev a ojo: editor con HUD `Ver > Performance` muestra FPS / frame ms / draws / tris / entities en vivo; spawner `Ayuda > Stress test` produce los grids de cubos esperados; botón "Snapshot baseline F2H2" appendea fila a `performance_baseline.csv` correctamente.

**Cambio importante:** primer profiler real integrado. Cliente Tracy v0.11.1 vía CPM (`MOOD_PROFILE` ON por default — macros no-op cuando OFF, coste cero). 10 zonas instrumentadas en frame loop + sub-zonas dentro de SceneRenderer (shadow / skybox / light grid / PBR static / PBR skinned / particle / debug / post-process). Counter `FrameStats` en `IRenderer` (draws + tris) consumido por el HUD. 4 spawners stress-test (10K/100K/500K/1M tris en cubos). Nuevo panel `PerformanceHudPanel` con snapshot CSV.

**Baseline medido (GTX 1660 / Ryzen 5 5600G / 16GB DDR4-2666):** documentado en `docs/PERFORMANCE.md`. Empty: 60 FPS vsync-cap. 10K tris (836 cubos): ~4 FPS, ~250 ms/frame. 100K+ tris: viewport congelado en Debug, postergado a post-F2H3/F2H4. Top cuello medido: `PBR::staticPass` 70.74% del frame (mean 42.5 ms; ~50 µs por cubo de 12 tris = CPU-bound en draw call setup, no GPU).

### F2H1 (anterior, ya cerrado)
Tag: `v1.1.0-fase2-hito1`.
Verificado automático: suite doctest **319/6613** sin regresiones después de cada uno de los 18 sub-commits. Editor + MoodPlayer compilan limpios y se ven idénticos a v1.0.0 (zero behavior change).

**Cambio importante:** `src/engine/` flat → jerárquico por dominio. La estructura nueva está documentada en `ARCHITECTURE.md` y la sección 2 de `PLAN_FASE2.md`. Resumen:
- `engine/render/{rhi,backend/opengl,pipeline,resources,scene_renderer,debug,passes}/`
- `engine/scene/{core,components,serialization,queries}/`
- `engine/physics/{world,components,character,queries}/`
- `engine/animation/{skeleton,clips,animator}/`
- `engine/audio/{device,clips,sources}/`
- `engine/scripting/{runtime,bindings,exposed}/`
- `engine/assets/{manager,loaders,primitives}/`
- `engine/world/{grid,csg,streaming}/`
- `engine/game/{manifest,overlay,state,dialog,quest,inventory}/` + `engine/i18n/`
- `systems/{render,physics,animation,ai,particles,audio,light,scripting}/`
- `editor/{application,ui,panels/{scene,assets,debug,world},commands,tools,overlay}/`

Carpetas `.gitkeep` documentan dónde van features de hitos posteriores (CSG en F2H9-F2H16, dialog/quest/inventory en F2H29-F2H31, i18n en F2H5).

### Fase 1 cerrada — Recapitulación
Tags: `v1.0.0` (Hito 39, fin de Fase 1) + `v0.40.0-hito40` + `v0.41.0-hito41` + `v0.41.1-hito41-final` (fix Load Game) + `v0.42.0-hito42` (Material Editor lite).

El motor Fase 1 está completo para producir demos FPS reales con progresión, save/load, triggers, particles, animation, scripts, raycasts, undo/redo, paquetes standalone. F2H1 reorganiza el código antes de empezar a sumar features grandes (CSG, material node-graph, dialog/quest/inventory, Mixamo importer).

### Hito 41 (anterior, ya cerrado)
Tag: `v0.41.0-hito41`.
Verificado automático: suite doctest **315/6591** (+3 tests `test_save_load`: round-trip bodies + round-trip script globals + back-compat v1). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: F5 con cajas físicas empujadas → load restaura pose + velocity exactas; script con globals captura/restaura correcto; archivos `.moodsave` v1 cargan sin errores ni warnings (back-compat).

**Cambio importante:** `.moodsave` schema bumpea a v2 con campos opcionales `bodies` (Dynamic body snapshots por tag) + `script_globals` (filtradas por ExposedValue variant). Nuevos getters/setters en PhysicsWorld para velocity. ScriptSystem gana `captureGlobals` + `restoreGlobals` con `m_pendingGlobals` para casos donde el script aún no cargó.

**Próximo paso:** Hito 42 (Editor de materiales visual) — último item grande del backlog. Después: recapitulación del dev + Fase 2 (TBD).

### Hito 40 (anterior, ya cerrado)
Tag: `v0.40.0-hito40`.
Verificado automático: suite doctest **312/6564** (+3 de `test_raycast` API contract de setters runtime + 1 de `test_particle_system` Cone). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: F1 toggle muestra OBB rotado en triggers; editar `coyoteWindowSec` en el `.moodproj` cambia el feel del salto; dos emisores de humo overlapping NO flickerea entre frames; emisor con `localSpace=true` rotado en Y → partículas siguen rotación.

**Cambio importante:** 11 de 14 bloques cerrados (3 documentados como pendientes Fase 2 por requerir refactor a pure functions o version Jolt actualizada). Inspector con undo en 50+ widgets ahora (sumando Hitos 32+34+35+36+40). Particle Cone shape con axis custom. Settings de char controller per-proyecto. Sort partículas estable. localSpace con worldMatrix completa.

**Próximo paso:** Hitos 41 (Save/Load extendido con snapshots Jolt + Lua globals) + 42 (Editor de materiales visual). Después: recapitulación del dev + Fase 2 (TBD).

### Hito 39 (anterior, ya cerrado — `v1.0.0` = fin Fase 1)
Tags: `v0.39.0-hito39` + **`v1.0.0`**.
Verificado automático: suite doctest **308/6554** (+1 de `test_particle_system` Cone + 1 de `test_trigger_system` OBB + 1 de `test_raycast` layerMask). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: particle Cone con size 2 confina partículas en un cono recto; trigger rotado 45° respeta orientación (punto en world (0.7, 0, 0) detectado dentro del OBB pero no del AABB); raycast con mask=1 solo pega paredes Static, mask=2 solo cajas Dynamic; editar friction del Inspector durante Play cambia el deslizamiento de la caja física al instante.

**Próximo paso:** recapitulación del dev + planning de Fase 2 (TBD). Tras 39 hitos el motor está completo para producir demos FPS reales con progresión.

### Hito 38 (anterior, ya cerrado)
Tag: `v0.38.0-hito38`.
Verificado automático: suite doctest **305/5947** (+5 de `test_save_load`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: doble-click a `MoodPlayer.exe` → menu modal "MoodEngine" con New Game / Load Game / Quit. New Game → entra al juego con char en spawn default + HUD reset. Load Game → file dialog filtra `.moodsave`. F5 durante el juego → quicksave silencioso a `<exeDir>/quicksave.moodsave`. "Salir al menu" del pause overlay vuelve al menu (no cierra la app — Quit definitivo solo desde el main menu).

**Cambio importante:** primer feature visible post-cleanup. Nuevo módulo `engine/saving/SaveLoad.{h,cpp}` con schema `.moodsave` v1 (JSON aparte del `.moodmap`: state runtime separado del level design). Persiste hud + player position + camera yaw/pitch. `MoodPlayer` ahora arranca con `m_inMainMenu = true` y un guard `gameUpdating` que detiene TODO el update del juego durante el menu (camera/physics/scripts/animation/nav/particles). MoodPlayer ahora linkea `pfd` para el file dialog.

**Próximo paso:** Hito 39 (polish reactivo + tag final `v1.0.0` que cierra la Fase 1 del proyecto). Plan en `docs/PLAN_HITO39.md`.

### Hito 37 (anterior, ya cerrado)
Tag: `v0.37.0-hito37`.
Verificado automático: suite doctest **300/5927** (+2 de `test_package_builder` smart-pack walk + 3 de `test_trigger_system` body detection + 4 de `test_particle_system` emission shapes). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: "Empaquetar proyecto" produce paquetes con solo los assets referenciados — el zip pesa fracciones de lo que pesaba con el copy entero. Caja física empujada al AABB de un trigger imprime `on_trigger_body_enter` con el body id; al sacarla `on_trigger_body_exit`. Particle emitter con shape `Sphere` distribuye partículas en un volumen real (no todas saliendo de un punto). Save/cerrar/reabrir preserva shape + size.

**Cambio importante:** sexto hito seguido cerrando deudas (32 → 33 → 34 → 35 → 36 → 37). Las 3 medianas pendientes barridas (Hito 26 + 33 + 29). 3 bloques: smart-pack escanea `.moodmap` + `.material` y copia solo assets referenciados (whitelist defensiva: `textures/missing.png`, `audio/missing.wav`, `skyboxes/` recursivo); `TriggerSystem` con segundo loop sobre `RigidBodyComponent` Dynamic+Kinematic dispatcha `on_trigger_body_enter/exit/stay(bodyId)` (Static se ignora); `ParticleEmitterComponent::EmissionShape { Point, Box, Sphere, Disc }` con sampler funcs por shape (rejection sampling) y persistencia opcional. Backlog de scope chico-medio = limpio.

**Próximo paso:** Hito 38 (TBD). Plan en `docs/PLAN_HITO38.md`.

### Hito 36 (anterior, ya cerrado)
Tag: `v0.36.0-hito36`.
Verificado automático: suite doctest **291/5574** (+2 de `test_edit_property_command` para `EditPropertyCommand<u32>` + 2 de `test_trigger_system` para `on_trigger_stay`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: drop de textura sobre material slot → Ctrl+Z restaura el material previo (anti-contagio del Hito 25 preservado). Editar `maxParticles` con DragInt → Ctrl+Z restaura el valor + resetea la pool runtime. Trigger con script que define `on_trigger_stay` loguea cada frame que el player sigue dentro.

**Cambio importante:** quinto hito seguido cerrando deudas (32 → 33 → 34 → 35 → 36). El backlog "scope chico arrastrando" está limpio. 3 bloques: undo del drop de textura via reuso de `EditPropertyCommand<u32>` (sin clase nueva), `u32` en el variant del `InspectorEditTracker` + cableo del slider DragInt de `maxParticles` con cleanup de pool runtime al revertir, dispatch de `on_trigger_stay` per-frame en `TriggerSystem` (frame del enter solo enter, frames siguientes mientras adentro stay, frame del exit solo exit).

### Hito 35 (anterior, ya cerrado)
Tag: `v0.35.0-hito35`.
Verificado automático: suite doctest **287/5561** (+1 de `test_mesh_asset` `.obj`+`.mtl`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: drag de textura del AssetBrowser sobre el slot del Inspector reemplaza el material por uno nuevo (otras entidades que compartían el material original no se ven afectadas). Editar fov/near/far de Camera, fog/exposure/IBL, audio volume/loop/etc, animator speed/playing/loop, toggles + colores + velocities de partículas → Ctrl+Z revierte cada uno. Drop de un `.obj` con `.mtl` que referencia textura externa muestra la textura correcta (antes caía a missing por bug del path).

**Cambio importante:** scope chico, mismo patrón que Hito 34 — cerrar deudas viejas no atacadas. 4 bloques: drop textura sobre material slot del Inspector, wire-up undo de 23 widgets más (total Inspector con undo: **40 widgets**, era 17), validación `.obj`+`.mtl` que destapó un bug latente del Hito 26 con paths que escapan su carpeta. Fix: `MeshLoader::extractAlbedo` ahora normaliza paths con `lexically_normal()` antes de pasarlos al VFS — antes texturas en `../paths` caían a missing silenciosamente.

### Hito 34 (anterior, ya cerrado)
Tag: `v0.34.0-hito34`.
Verificado automático: suite doctest **286/5555** (+2 de `test_scene_serializer` friction round-trip + 3 de `test_raycast` con `ignoredBodyId`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: caja física en piso de friction 0.05 resbala lejos vs 0.5 default frena en pocos metros. Saltar 50ms después de correr off platform aún gatilla el salto (coyote). Apretar SPACE 100ms antes de aterrizar igual hace saltar al touchdown (buffer). Headbob al correr full-speed se siente igual; crouched la amplitud baja; quieto = sin bob. Editar light color/intensity/radius o rigid body mass/halfExtents → Ctrl+Z revierte.

**Cambio importante:** scope chico-medio, mismo patrón que Hito 32 — barrer deudas chicas acumuladas en hitos 30-33 antes de meternos en features pesados (save/load, main menu). 5 bloques: `friction` per-body en `RigidBodyComponent` con persistencia opcional, `physics.raycast` con `ignoredBodyId` opcional, coyote (100ms) + jump buffer (150ms) con detección de flanco de SPACE, headbob amp escalado linealmente con velocity horizontal, wire-up de 7 widgets más del Inspector con undo (Light, RigidBody, ParticleEmitter selectos).

### Hito 33 (anterior, ya cerrado)
Tag: `v0.33.0-hito33`.
Verificado automático: suite doctest **281/5535** (+5 de `test_raycast` + 5 de `test_trigger_system`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: "Ayuda > Agregar trigger demo" spawnea entidad en (0, 1, 0) con `halfExtents=(1,1,1)`. En Play Mode, caminar al centro del trigger imprime `[script] [info] [trigger] player entro` en la consola; al salir, `[trigger] player salio`. Editar `halfExtents` desde el Inspector → Ctrl+Z revierte. Save/cerrar/reabrir preserva el componente.

**Cambio importante:** primera API de queries físicas. `PhysicsWorld::raycast(origin, dir, maxDist)` via `JPH::NarrowPhaseQuery::CastRay` + `BodyLockRead` para la normal. Expuesta a Lua como `physics.raycast(...)` devolviendo tabla. Nuevo `TriggerComponent` (AABB sensor) + `TriggerSystem` stateless con flank-detection que dispatcha `on_trigger_enter`/`on_trigger_exit` al script via `ScriptSystem::dispatchEvent` (`sol::protected_function`, miss silencioso). Persistencia `.moodmap` con campo opcional (sin bump de schema mayor).

### Hito 32 (anterior, ya cerrado)
Tag: `v0.32.0-hito32`.
Verificado automático: suite doctest **271/5512** (+6 de `test_edit_property_command`). Editor compila limpio. Verificado por el dev a ojo: editar position/rotation/scale del Transform desde el Inspector → Ctrl+Z revierte el drag completo. Editar `albedoTint`/`metallic`/`roughness`/`ao` del material → Ctrl+Z restaura. Renombrar entidad por el Tag input → Ctrl+Z restaura el nombre. Flujo `edit → delete → undo → undo` ahora completa correctamente (antes el segundo undo era silencioso).

**Cambio importante:** infra de undo del Inspector. Nuevo `EditPropertyCommand<T>` templado + helper `trackPropertyEdit<T>` con detección drag-end vía `IsItemActivated/IsItemDeactivatedAfterEdit`. Handle remap en HistoryStack vía `ICommand::onEntityRemap` virtual. 9 de 51 widgets del Inspector cableados (los más editados); los 42 restantes siguen el mismo patrón y quedan como pendiente menor.

### Hito 31 (anterior, ya cerrado)
Tag: `v0.31.0-hito31`.
Verificado automático: suite doctest **265/5499** (+1 de `test_particle_system` round-trip de `localSpace`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: caja física empujada por el char no se desliza interminablemente (friction 0.2 → 0.5), crouch suave en 200ms (no snap), caminar con headbob sutil de 4cm a 5Hz. Emisor de partículas con `localSpace=true` movido por gizmo arrastra todas las partículas con el emisor; humo se ve correctamente layered post-sort.

**Cambio importante:** scope chico — pulir el feel de Hito 29 (particles) + Hito 30 (char controller) ahora que están en uso real. Cinco fixes baratos pero visibles. NavAgent polish quedó descartado permanentemente por decisión del dev.

### Hito 30 (anterior, ya cerrado)
Tag: `v0.30.0-hito30`.
Verificado automático: suite doctest **264/5494** (+7 de `test_character_controller`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: WASD camina sin atravesar paredes, caja física se empuja al chocarla (slide alto por fricción default baja — pendiente menor), Space salta ~1.5m, LCtrl baja la cámara sin caer abruptamente y restaura standing al soltar.

**Cambio importante:** primer hito post-Hito 4 que cambia cómo se siente el player. Capsule física via `JPH::CharacterVirtual` (no más AABB del Hito 4). El char respeta slopes < 50°, empuja `RigidBody::Dynamic`, no es "fantasma" para Jolt como antes.

### Hito 29 (anterior, ya cerrado)
Tag: `v0.29.0-hito29`.
Verificado automático: suite doctest **257/5476** (+9 de `test_particle_system` lógica + 2 round-trip en `.moodmap`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: "Ayuda > Agregar particulas de fuego demo" spawnea un emisor en (0, 0.5, 0). Las chispas suben (gravityFactor=-0.05), pasan de naranja a rojo transparente, blend aditivo. Editar parámetros en el Inspector cambia la simulación en vivo. Save/cerrar/reabrir el proyecto preserva la configuración del emisor (schema v9).

**Cambio importante en render:** nuevo pase de partículas en `SceneRenderer` entre el PBR pass (estático + skinneado) y el debug renderer. Depth test ON pero depth write OFF — las partículas se ocluyen por geometría opaca pero se mezclan correctamente entre sí. Blend per-emisor (additive vs alpha). Sin nueva dep — todo sobre `glDrawArraysInstanced` + `glm`.

**Próximo paso:** Hito 30 (TBD). Plan en `docs/PLAN_HITO30.md`.

### Hito 28 (anterior, ya cerrado)
Tag: `v0.28.0-hito28`.
Verificado automático: suite doctest **246/5431** (+8 de `test_create_entity_command`). Spawnear cualquier entidad desde "Ayuda > Agregar X" o drop de mesh/prefab al viewport → Ctrl+Z destruye lo recién creado, Ctrl+Y lo recrea con componentes intactos. Drop de barril Kenney 0.27m a escala nativa (no inflado a 1.5m). Editor in-place de scripts via `ScriptEditorPanel` (sin nueva dep — `ImGui::InputTextMultiline` + autosave on Lose Focus).

**Decisión clave:** después de 4 hitos consecutivos de polish editor, cerramos Hito 28 con A parcial (CreateEntityCommand) + F (script editor) + G (autoscale fix). InspectorEditCommand y commands para drops modificadores quedan como pendientes menores con trigger explícito.

### Hito 27 (anterior, ya cerrado)
Tag: `v0.27.0-hito27`.
Verificado automático: suite doctest **238/5409**. Drag de gizmo (W/E/R) sobre cualquier entidad → soltar → Ctrl+Z revierte el drag completo en una sola operación (no 60 micro-edits por frame). Delete sobre Fox.glb → Ctrl+Z lo recrea con animación intacta. Delete sobre CajaFisica → Ctrl+Z la recrea con `bodyId=0` que se rematerializa.

**Cambio importante:** `EditorApplication::deleteSelectedEntity` ahora pushea un `DeleteEntityCommand` al `m_history`. El comando captura un `SavedEntity` snapshot via `EntitySerializer` para que el undo recree la entidad completa. **Decoupling de Jolt:** `DeleteEntityCommand` recibe un callback `BodyCleanup = std::function<void(u32)>` en lugar de un `PhysicsWorld*` directo, permitiendo tests headless con `{}` no-op.

### Hito 26 (anterior, ya cerrado)
Tag: `v0.26.0-hito26`.
Verificado automático: suite doctest **212/5326** (+6 de `test_asset_manager` para `loadEmbeddedTexture` + `createMaterialsForMesh`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo: drag de `kenney_survival/barrel.glb` al viewport spawnea un barril de madera (~1.5 m, autoscale up) con la textura colormap real (NO chequer magenta). Fox.glb y CesiumMan.glb spawnean con sus texturas embedded reales. Las esferas PBR reflejan el cielo kloofendal del SkyboxRenderer (antes mismatch con el cubemap sky_day).

**Cambio importante en assets:** `MeshLoader` extrae el albedo de cada material referenciado por el archivo (embedded `*N` via `aiScene::GetEmbeddedTexture` + external relativo a la carpeta del .glb). `MeshAsset.materialAlbedoTextures` se popula con los `TextureAssetId` resueltos. Spawn paths usan el nuevo helper `AssetManager::createMaterialsForMesh(meshId)` para armar el vector de materiales con esas texturas (instance único por entidad, hereda invariante anti-contagio del Hito 25).

**Asset pack agregado:** `assets/meshes/kenney_survival/` con 80 GLBs CC0 de Kenney Survival Kit (barrels, boxes, structures, vegetation, tools, crafting) compartiendo `Textures/colormap.png` 512×512. 1.5 MB total.

**Fix bonus:** removido `aiProcess_FlipUVs`. `OpenGLTexture` ya hace `stbi_set_flip_vertically_on_load(true)`; el doble flip cancelaba en texturas simétricas (grid, brick) pero rompía palette swatches como el `colormap.png` de Kenney. Documentado en `DECISIONS.md`.

### Hito 25 (anterior, ya cerrado)
Tag: `v0.25.0-hito25`.
Verificado automático: suite doctest **206/5309** (+6 de `test_material_serializer`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo y por smoke test programático: editar `shaders/skybox_equirect.frag` con el editor abierto produce `[render] [info] Hot-reload OK: shaders/skybox.vert + shaders/skybox_equirect.frag` en el log a los ~6 s; inyectar un syntax error produce `[render] [warning] Hot-reload fallo en ... (se mantiene el shader previo)` y el editor sigue corriendo sin romper render; restaurar el shader recovery sin reinicio.

**Polish reactivo del Hito 24 cerrado en el mismo tag:**
- **Costura del skybox equirect**: `texture()` → `textureLod(..., 0.0)`. En la costura las derivadas saltan y el sampler elegía un mip 1×1 produciendo línea vertical borrosa.
- **Default material visible como warning**: nuevo flag `MaterialAsset::useAlbedoMap`. El default lo tiene en `true` con `albedo=0` para que sample `missing.png`. Antes, una entidad sin material quedaba blanco puro disfrazado de feature.
- **Material instance único por entidad**: nuevo `AssetManager::createMaterialFromTexture()` (sin cache). Migrados tiles + floor del Editor y Player, los 3 spawners de `DemoSpawners`, `updateTileEntity`, y `SceneLoader`. Editar el albedoTint de un cubo no contagia jamás. Sentinel `__runtime_tex#<N>` se persiste como el path de la textura subyacente.

### Hito 24 (anterior, ya cerrado)
Tag: `v0.24.0-hito24`.
Verificado automático: suite doctest **200/5287** (+8 de `test_exposed_properties`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo (smoke test): scripts Lua declaran parámetros con `local x = engine.exposed("name", default)`; el Inspector muestra cada prop como widget tipado (DragFloat / Checkbox / InputText / DragFloat3 / ColorEdit3). Editar el valor cambia el comportamiento del script en vivo (un rotator a 30°/s vs otro a 90°/s usando el mismo `rotator.lua`). Guardar / cerrar / reabrir el proyecto preserva los overrides via el bloque `script: {path, overrides}` del `.moodmap` (schema v8).

**Cambio importante en serialización:** `SceneSerializer::save` filtraba entidades a persistir por componente (MeshRenderer/Light/RigidBody/Environment). Ahora también incluye Script con path no-vacío. Sin esto, una entidad cuya razón de ser es llevar un script (rotator demo, controlador puro Lua) se descartaba al guardar.

**Polish reactivo cerrado en el mismo tag:**
- **Mapa de prueba 16x16 con suelo plano + columna central** (antes había cubos perimetrales con backface-culling visualmente molestos).
- **Skybox equirectangular** (sampler2D con mapping spherical) además del cubemap original — evita seams en los polos. Asset: `assets/skyboxes/sky_kloofendal.png` (kloofendal_43d_clear de Polyhaven CC0, tonemapeado a LDR via `tools/hdr_to_equirect_png.py`).

### Hito 23 (anterior, ya cerrado)
Tag: `v0.23.0-hito23`.
Verificado automático: suite doctest **192/5283** (+10 de `test_pathfinding` + `test_nav_system`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo (smoke test): "Ayuda > Agregar enemigo demo" spawnea un CesiumMan (CC0 humanoide bipédo) parado al ras del piso. En Play Mode el enemigo camina hacia el jugador siguiendo paths A* sobre el GridMap, respetando muros via moveAndSlide. F1 dibuja el path activo como polyline cyan + waypoint actual brillante.

**Cambio importante en assets:** `MeshAsset::importRotationEuler` extraído del `mTransformation` del rootNode de assimp. Glb autoreados Z-up (CesiumMan, BrainStem, etc.) reciben automáticamente -90° X de rotación al spawnearse; Y-up nativos (Fox, pyramid) reciben 0. Sin esto, cualquier `.glb` Z-up dropeado al viewport quedaba acostado.

### Hito 22 (anterior, ya cerrado)
Tag: `v0.22.0-hito22`.
Verificado automático: suite doctest **182/5234** (+3 de `test_scene_loader` para el fix arrastrado del Fox). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo (smoke test): scripts dropeables desde el Asset Browser sobre entidades del viewport (`Drop script 'scripts/rotator.lua' -> entidad 'X'` en log), nuevo script via `Archivo > Nuevo Script...`, panel "Lua API" en el dockspace, Fox persistido en `.moodmap` se anima al cargar.

**Bloques cerrados:**
1. Sección "Scripts" en Asset Browser (scan `assets/scripts/*.lua` + drag source con payload `MOOD_SCRIPT_ASSET`).
2. Drop al viewport (highlight OBB amarillo + `processViewportScriptDrop` que addOrReplace `ScriptComponent`).
3. `Archivo > Nuevo Script...` (file dialog + template + force a `assets/scripts/` + rescan del browser).
4. Panel "Lua API" con tabs `self`/`hud`/`log`/`Lifecycle` y snippets de uso.
5. Mini editor in-place con `ImGuiColorTextEdit` — **deferido al Hito 23** (stretch del plan; los 4 bloques anteriores ya entregan el workflow base, el dev sigue editando en VS Code y el hot-reload del ScriptSystem levanta los cambios automáticamente).
6. Tests headless + cierre.

**Bonus fix arrastrado del Hito 19:** meshes con esqueleto + clips persistidos en `.moodmap` reaparecían en bind pose porque `AnimatorComponent` y `SkeletonComponent` no están en el schema del `SavedEntity`. `SceneLoader::applyEntitiesToScene` ahora detecta el caso (mismo check que `processViewportMeshDrop`) y los auto-agrega con defaults razonables (clipName="" → primer clip, playing=true, loop=true). Si el dev necesita un setup específico, el Inspector lo deja editar tras la carga.

**Próximo paso:** Hito 23 (TBD). Plan en `docs/PLAN_HITO23.md` con candidatos. Nota arrastrada: el mini editor de scripts in-place quedó pendiente como punto de arranque natural si se elige seguir el workflow de scripting.

### Hito 21 (anterior, ya cerrado)
Tag: `v0.21.0-hito21`.
Verificado automático: suite doctest **179/5221** (+8 de `test_package_builder`). Editor + MoodPlayer compilan limpios. Verificado por el dev a ojo (smoke test): `Archivo > Empaquetar proyecto...` produce un paquete autocontenido (88 archivos en el smoke test) en la carpeta destino. Doble-click en el `MoodPlayer.exe` empaquetado abre la sala del proyecto con sus entidades persistidas (Fox + pyramid en el smoke test), corre Play Mode, Esc abre el menú de pausa, "Salir del juego" cierra limpio.

**Layout del paquete:**
```
<destDir>/<projectName>/
├── MoodPlayer.exe
├── SDL2d.dll  (o SDL2.dll si NDEBUG)
├── game.json     ← {version, name, project, default_map}
├── assets/       ← copia del engine (skybox, IBL, primitivas, scripts default)
├── shaders/      ← copia del engine
└── project/<name>.moodproj + maps/
```

**Refactor estructural del hito:** se extrajo `mood_engine_lib` (static lib) con todo `core/`/`platform/`/`engine/`/`systems/`. MoodEditor y MoodPlayer linkean contra ella y solo agregan sus propios `.cpp` (`editor/` y `player/` respectivamente). El render pipeline vivía en `EditorApplication`; ahora lo encapsula `engine/render/SceneRenderer` y los dos binarios lo comparten. El HUD del juego (HP/Ammo/crosshair) y el menú de pausa también se compartieron via `engine/game/GameOverlay::draw(...)` parametrizado por el label del botón "Salir" + un callback `onExitRequested`.

**Próximo paso:** Hito 22 (TBD por confirmar). Candidatos en `docs/PLAN_HITO22.md`: workflow de scripting (Asset Browser tab + mini editor de código), exposed properties Lua, AI/pathfinding, networking, save/load de gameplay.

### Hito 20 (anterior, ya cerrado)
Tag: `v0.20.0-hito20`.
Verificado automático: suite doctest **171/5188** (+6 de `test_game_state` para GameState singleton + hud Lua bindings). Editor arranca limpio. Verificado por el dev a ojo (smoke test): Play Mode muestra HUD con HP=100/AMMO=30 + crosshair central; Esc togglea menú de pausa con 3 botones (Continuar/Opciones/Salir al editor). "Ayuda > Agregar HUD demo" spawnea entidad invisible con `hud_demo.lua` que en Play setea HP=75/Ammo=12, drena 1 HP/s, y al llegar a 0 fuerza pausa via `hud.setPaused(true)` — el cursor aparece automáticamente para clickear botones.

Cambio de plan a mitad del hito: el plan original era **RmlUi**; tras 3 bloques funcionando, los bugs persistentes de layout responsive llevaron a abandonarlo y reescribir HUD + pausa con **drawlist de Dear ImGui** sobre el callback `OverlayDraw` que ya tenía `ViewportPanel`. Ver `DECISIONS.md`.

`EditorApplication.cpp` se partió de 1514 → 652 líneas + 3 archivos parciales nuevos (`EditorOverlay.cpp`, `EditorPlayMode.cpp`, `EditorScene.cpp`) que comparten el header. Soft target ~500 líneas/`.cpp`, hard cap ~800.

### Hito 19 (anterior, ya cerrado)
Tag: `v0.19.0-hito19`.
Verificado automático: suite doctest **165/5172** (+12 de `test_animation` para Skeleton, BoneTrack, AnimationClip::evaluate). Editor arranca limpio. Verificado por el dev a ojo (smoke test): `Ayuda > Agregar personaje animado` spawnea Fox.glb (CC0 de glTF Sample Assets, 24 huesos, 3 clips Survey/Walk/Run). El zorro se anima en Editor y Play Mode; el combo de clips en el Inspector cambia entre los 3 sin recompilar. Recibe sombra del directional (Hito 16) y luces PBR + IBL (Hito 17 + 18) como cualquier otro mesh.

**Stack del frame de render (post Hito 19):**
1. Shadow pass al depth FB (si hay directional con `castShadows`).
2. Skybox al scene FB (HDR RGBA16F, sRGB cubemap).
3. AnimationSystem avanza el time de cada `AnimatorComponent` y rellena `SkeletonComponent.skinningMatrices` (CPU).
4. Build LightGrid CPU desde `LightFrameData` (proyección de bounding spheres a tiles).
5. Upload de los 3 SSBOs (point lights, tiles, indices). Bindings 2/3/4.
6. Pase A: `m_pbrShader` (Cook-Torrance + Smith + Schlick + IBL split-sum + Forward+) sobre entidades estáticas (`forEach<Transform, MeshRenderer>` skipeando `SkeletonComponent`).
7. Pase B (solo si hay alguna entidad skinneada): `m_pbrSkinnedShader` (mismo `pbr.frag`, `pbr_skinned.vert` con LBS 4 huesos). Sube `uBoneMatrices[i]` por entidad.
8. Debug renderer (AABBs, OBBs, líneas).
9. Post-process pass: exposure (2^EV) + tonemap + gamma → `m_viewportFb` LDR RGBA8.
10. ImGui muestra `m_viewportFb` en el panel Viewport.

**Vertex layout de los meshes importados:** stride 19 floats (pos+color+uv+normal+boneIds+boneWeights). Los meshes sin esqueleto guardan boneIds/boneWeights en 0 — el shader skinneable detecta `sum(weights) < 1e-4` y cae al pipeline no-skinneado. Las primitivas (cubo, esfera) siguen con stride 11 (no se animan).

### Hito 18 (anterior, ya cerrado)
Tag: `v0.18.0-hito18`.
Verificado automático: suite doctest 153/5109 (+9 de `test_light_grid` para light → tile assignment). Editor arranca con `LightGrid: 0 point lights -> N tiles (XxY), 0 no-vacios, 0 asignaciones (avg 0.00/tile)` en el log al cargar mapa vacío. Verificado por el dev a ojo: spawn de 64 stress lights vía `Ayuda > Agregar stress test 64 luces` muestra spots de colores HSV procedurales sobre el suelo; con `IBL intensity` bajado a ~0.4 las luces destacan claramente sobre el ambient.

**Polish reactivo del Hito 18 cerrado en el mismo tag:**
- `EnvironmentComponent.iblIntensity` (slider [0..2]) en Inspector. Persistido en `.moodmap` solo si != 1.0.
- Log diagnóstico one-shot del LightGrid al cambiar la cantidad de luces.

### Hito 17 (anterior, ya cerrado)
Tag: `v0.17.0-hito17`.
Verificado automático: suite doctest **144/649** (+8 de `test_pbr_brdf` + 7 de `test_material_serializer` + 4 de `test_asset_manager` para material catalog). Editor arranca con `IBL cargado: irradiance + prefilter (5 mips) + BRDF LUT.` y `AssetManager: esfera primitiva generada en slot N` en el log. Verificado por el dev a ojo: grid 3×3 de esferas (`Ayuda > Agregar esferas PBR de prueba`) muestra el rango completo metallic 0/0.5/1 × roughness 0.05/0.5/1.0 — esquina inferior-derecha refleja el cielo como espejo, esquina superior-izquierda es chalk mate. Inspector con sliders en vivo de albedoTint/metallic/roughness/ao actualiza el shader sin relanzar. Drag de `.material` desde AssetBrowser sobre cualquier mesh asigna el material al primer slot.

**Stack del frame de render (post Hito 17):**
1. Shadow pass al depth FB (si hay directional con `castShadows`).
2. Skybox al scene FB (HDR RGBA16F, sRGB cubemap).
3. Loop scene-driven con shader `pbr` (Cook-Torrance + Smith + Schlick + IBL split-sum) — termina en `m_sceneFb`.
4. Debug renderer (AABBs, OBBs, líneas).
5. Post-process pass: exposure (2^EV) + tonemap (None/Reinhard/ACES) + gamma → `m_viewportFb` LDR RGBA8.
6. ImGui renderiza el `m_viewportFb` en el panel Viewport.

**Polish reactivo del Hito 17 cerrado en el mismo tag:**
- `fix(editor): "Nuevo Proyecto" crea su propia carpeta` — convención Unity/Godot, evita contaminar el directorio padre con `maps/`/`textures/` sueltas.
- LightSystem limpia uniforms `uSpecularStrength`/`uShininess` que eran del Blinn-Phong.
- Fix doble-gamma: cubemaps de color cargan como `GL_SRGB8_ALPHA8` (skybox + IBL) — antes se veía todo muy claro porque el post-process aplicaba gamma encode sobre datos ya gamma-encoded.

**Próximo paso:** Hito 18 — Deferred / Forward+. Plan en `docs/PLAN_HITO18.md`.

### Hito 16 (anterior, ya cerrado)
Tag: `v0.16.0-hito16`.
Suite 125/580. Shadow mapping con sampler2DShadow + PCF 3×3 hardware. `castShadows` checkbox + round-trip en `.moodmap`. Polish UX (iconos Blender, frame selected, outline OBB, cubo cyan condicional, refactor 2011→1154 líneas).

### Hito 15 (anterior, ya cerrado)
Tag: `v0.15.0-hito15`.
Suite 120/530 (+6 de fog numérico + 1 de round-trip Environment). Editor arranca con `SkyboxRenderer inicializado` y `PostProcessPass inicializado`; el Welcome modal aparece SIEMPRE (no auto-open). Verificado por el dev a ojo: skybox visible detrás de la sala, fog se aplica en vivo desde Inspector, exposure / tonemap reactivos sin relanzar, `EnvironmentComponent` se persiste con el `.moodmap`.

### Hito 14 (anterior, ya cerrado)
Tag: `v0.14.0-hito14`.
Suite 113/493. PrefabSerializer + EntitySerializer compartido; AssetBrowser sección Prefabs; drag al Viewport instancia con copia de Mesh/Light/RigidBody. `PrefabLinkComponent` persistido en `.moodmap` v5.

### Hito 13 (anterior, ya cerrado)
Tag: `v0.13.0-hito13`.
Suite 106/454. Click en el viewport selecciona la entidad; gizmos de translate/rotate/scale aparecen sobre el pivote; hotkeys `W/E/R` cambian modo; iconos 2D para luces/audios pickables; gizmos + iconos solo en Editor Mode.

### Hito 12 (anterior, ya cerrado)
Tag: `v0.12.0-hito12`.
Verificado automático: suite doctest 99/442 pasando (+1 round-trip RigidBody), editor arranca con `[physics] Jolt inicializado (max_bodies=1024, max_pairs=1024, gravity=-9.81)`, sin warnings nuevos. El mapa 8×8 tiene 29 static bodies (uno por tile sólido) y la caja demo (Dynamic 5kg) cae por gravedad al entrar a Play Mode.

### Hito 11 (anterior, ya cerrado)
Tag: `v0.11.0-hito11`.
Verificado automático (suite doctest 90/380 pasando con +8 tests nuevos — 5 de MeshLoader/AssetManager, 2 de round-trip de entidades, test setup con WORKING_DIRECTORY). Verificado por el dev a ojo: drag de `pyramid.obj` desde AssetBrowser al Viewport spawnea la entidad con metadata `[1 submeshes, 18 v]`; Ctrl+S persiste 3 entidades; cerrar + reabrir trae las 3 entidades con posición/rotación/scale/material intactos (`Mapa cargado: … (N tiles sólidos, 3 entidades)` en el log).

**Próximo paso:** Hito 11 — Iluminación Phong/Blinn-Phong. Activar `LightComponent` (era placeholder del Hito 7), shader con normales + multiple lights, demo visual de escena iluminada. Plan en `docs/PLAN_HITO11.md`.

### Lo que ya está hecho

**Hito 1** — Shell del editor completo (tag `v0.1.0-hito1`):
- Estructura completa de carpetas del repo (sección 6 del doc técnico).
- Build: CMake 3.24+ con CPM.cmake, CMakePresets, MSVC.
- `src/core/`: logging (spdlog), Types, Assert, Time (Timer + FpsCounter).
- `src/platform/Window`: wrapper RAII sobre SDL2 con contexto OpenGL 4.5 Core.
- `src/editor/`: EditorApplication, EditorUI, MenuBar, StatusBar, Dockspace + 3 paneles.

**Hito 2** — Primer triángulo con OpenGL (tag `v0.2.0-hito2`):
- `external/glad/`: GLAD v2 para OpenGL 4.5 Core generado con `glad2` Python, files committed, target estático `glad`.
- `src/engine/render/`: RHI abstracta (`IRenderer`, `IShader`, `IMesh`, `IFramebuffer`, `RendererTypes.h`).
- `src/engine/render/opengl/`: backend OpenGL (OpenGLRenderer, OpenGLShader con cache de uniforms, OpenGLMesh, OpenGLFramebuffer con color RGBA8 + depth RB).
- `shaders/default.{vert,frag}`: GLSL 4.5 Core. Copiado post-build junto al exe.
- `EditorApplication` monta renderer + framebuffer + shader + mesh; renderiza offscreen cada frame antes de la UI.
- `ViewportPanel` muestra el color attachment con `ImGui::Image` y UV invertido vertical.
- `Dockspace.cpp` arma layout por defecto con `DockBuilder` cuando `imgui.ini` no tiene nodos split.

**Hito 3** — Cubo texturizado con cámara (tag `v0.3.0-hito3`):
- `external/stb/`: `stb_image.h` (2.30) + `stb_image_write.h` (1.16) commiteados; target INTERFACE `stb`.
- `src/engine/assets/stb_impl.cpp`: única TU con `STB_IMAGE_IMPLEMENTATION`.
- `src/engine/assets/PrimitiveMeshes.{h,cpp}`: helper `createCubeMesh()` (36 vértices, pos+color+uv).
- `src/engine/render/ITexture.h` + `OpenGLTexture.{h,cpp}`: carga con stb_image, genera mipmaps, flip vertical al cargar.
- `src/engine/scene/EditorCamera.{h,cpp}`: orbital (yaw/pitch/radio + target). Input: right-drag + wheel.
- `src/engine/scene/FpsCamera.{h,cpp}`: libre (position + yaw/pitch). Input: WASD + mouse relativo.
- `src/editor/EditorMode.h`: enum Editor/Play + toggle request via `EditorUI`.
- `MenuBar` tiene ahora un botón Play/Stop verde/rojo empujado a la derecha.
- `StatusBar::draw(EditorMode)` muestra "Editor Mode" o "Play Mode" dinámicamente.
- Shaders extendidos: `uModel/uView/uProjection` + `sampler2D uTexture`, atributos pos/color/uv.
- Depth test activo en `OpenGLRenderer::init()`.
- `assets/textures/grid.png` generada con `tools/gen_grid_texture.py` (Python + Pillow, 256x256 RGBA).
- `tests/` con doctest: 10 casos, 37 asserciones (matemática GLM + cámaras).

**Hito 5** — Asset Browser + gestión de texturas (tag local `v0.5.0-hito5`):
- Convención de escala 1 unidad = 1 m SI (Bloque 0, arrastrado del Hito 4). `tileSize=3 m`, player 0.6×1.8×0.6 m a 1.6 m de eye height, speed 3 m/s (ajustado tras feedback del dev: 4→3), orbit radius 30, mapa 24×24 m.
- `assets/textures/missing.png` generada con `tools/gen_missing_texture.py` (256×256, chequered rosa/negro estilo Source). Canal de log nuevo `assets`.
- `src/engine/assets/AssetManager.{h,cpp}`: catálogo de texturas con cache por path, `TextureAssetId` propio (u32, 0 = missing), fallback automático al fallar carga (no throw en el callsite).
- `src/platform/VFS.{h,cpp}`: resuelve paths lógicos contra `assets/`, rechaza `..`, `.`, paths absolutos y leading `/`/`\`. 5 casos de test.
- `AssetBrowserPanel` ahora escanea `assets/textures/` y muestra miniaturas 64×64 con `ImGui::ImageButton` (UV flip vertical); click selecciona, borde azul en la seleccionada.
- `assets/textures/brick.png` (aparejo inglés 64×32 con offset, dos tonos) como segunda textura del mapa. `GridMap` extendido con array paralelo `std::vector<TextureAssetId>`; render bindea por tile con tracking `lastBound`.
- `src/core/LogRingSink.{h,cpp}`: custom spdlog sink con ring buffer de 512 entradas, thread-safe con mutex propio. `src/editor/panels/ConsolePanel.{h,cpp}`: lee el sink y renderiza los logs con color por nivel, filtro por substring de canal, auto-scroll, botón Limpiar.
- `Dockspace` actualizado: layout default ahora incluye Console en la parte derecha del bottom dock (Asset Browser queda a la izquierda).
- Suite total 35/179 (+5 VFS).

**Hito 4** — Mundo grid + colisiones AABB (tag `v0.4.0-hito4`):
- `src/engine/world/GridMap.{h,cpp}`: grilla 2D de tiles con `TileType { Empty, SolidWall }`. Helpers `tileAt`, `isSolid` (fuera = pared), `setTile`, `aabbOfTile`, `solidCount`. Coords map-local; el world offset lo maneja el callsite.
- `src/core/math/AABB.h`: header-only con `intersects/contains/expanded/merge` + helpers (`center/size/extents/isValid`).
- `src/systems/PhysicsSystem.{h,cpp}`: `moveAndSlide(map, mapWorldOrigin, box, desired) -> glm::vec3`. Resuelve X luego Z (permite slide en esquinas); Y pasa directo (muros infinitos por ahora). Tiles fuera del mapa se tratan como pared.
- `src/engine/render/opengl/OpenGLDebugRenderer.{h,cpp}` + `shaders/debug_line.{vert,frag}`: debug draw de líneas/AABBs. VBO dinámico con growth 2×, flush una vez por frame.
- `EditorApplication`: render del grid inline (loop por tiles sólidos con `translate*scale(tileSize)`), Play Mode con colisiones AABB vs grid (AABB jugador 0.4×0.9×0.4, antes 0.3×0.9×0.3, ver DECISIONS), toggle F1 para debug draw. Cámara orbital `radius=12`, FPS starts at tile interior `(-1.5, 0.6, 2.5)`.
- `FpsCamera` dividido en `computeMoveDelta` + `translate` para que el callsite pueda pasar por `moveAndSlide` antes de aplicar.
- Nuevo canal de log `world`. Logger inicial: `Mapa cargado: prueba_8x8 (29 tiles solidos)`.
- `fix(render)`: `drawMesh` ya no desbindea shader/mesh al terminar (permitía todos los cubos apilados por silent-fail de `glUniform*`).
- `fix(editor)`: status bar migrada a `ImGui::BeginViewportSideBar` + dibujada antes del dockspace; cierra el pendiente menor del Hito 3.
- Tests: +13 casos nuevos (7 AABB, 5 GridMap, 8 PhysicsSystem). Suite total 30/159.

**Hito 10** — Importación de modelos 3D con assimp (tag `v0.10.0-hito10`):
- `assimp v5.4.3` como static lib vía CPM. Solo importers OBJ + glTF + FBX habilitados (todos los demás + exporters + tests + CLI off). `ASSIMP_WARNINGS_AS_ERRORS OFF` forzado en cache antes del CPMAddPackage (el código de assimp tiene warnings en /W4).
- `src/engine/render/MeshAsset.h` + `src/engine/assets/MeshLoader.{h,cpp}`: `MeshAsset` = `{logicalPath, vector<SubMesh>}`, SubMesh = `{unique_ptr<IMesh>, materialIndex, vertexCount}`. `loadMeshWithAssimp` con flags `Triangulate | GenNormals | FlipUVs | CalcTangentSpace`. Expande índices a vértices planos (matchea `glDrawArrays` sin EBO).
- `AssetManager` extendido como tercer tipo de asset. `MeshAssetId` (u32, 0 = cubo primitivo fallback). Slot 0 generado programáticamente con `createCubeMesh()` + MeshFactory (path sentinela `"__missing_cube"`). `MeshFactory` inyectable con default `NullMesh` stub (tests sin GL); producción pasa lambda que crea `OpenGLMesh`.
- `MeshRendererComponent` refactorizado: de `IMesh* mesh + TextureAssetId texture` a `MeshAssetId mesh + vector<TextureAssetId> materials`. Helper `materialOrMissing(i)` cae al slot 0 si el array es más corto que los submeshes.
- Render unificado a un solo loop scene-driven. `GridRenderer` eliminado (archivos borrados, dead code tras la migración). `renderSceneToViewport` itera `Scene::forEach<Transform, MeshRenderer>` y dibuja submesh por submesh cambiando textura según `materialIndex`.
- `updateTileEntity(x, y, tex)`: helper nuevo para edits localizados de tiles — encuentra la entidad `Tile_X_Y` por tag y edita in-place, o la crea si la celda era Empty. Reemplaza el `rebuildSceneFromMap` completo en el drop de textura (preserva selección del Hierarchy).
- `AssetBrowserPanel` seccion "Meshes" (entre texturas y Audio). Lista `.obj/.gltf/.glb/.fbx` de `assets/meshes/` con metadata (submesh + vertex count). Drag payload `"MOOD_MESH_ASSET"` — fix detectado durante smoke test: `BeginDragDropSource` tiene que ir inmediatamente tras `Selectable`, no después de SameLine + TextDisabled.
- `ViewportPanel::MeshDrop` + `consumeMeshDrop()` análogos al flow de textura. `BeginDragDropTarget` acepta ambos payloads.
- `EditorApplication` consume el mesh drop: spawnea `Mesh_<path>` en el centro del tile bajo el cursor con `Transform + MeshRendererComponent(meshId, slot 0)`.
- Schema v2 del `.moodmap` (`k_MoodmapFormatVersion = 2`): sección `entities` nueva con `{tag, transform, mesh_renderer:{mesh_path, materials[]}}`. Filtro por prefijo `Tile_*` (esas se reconstruyen del grid). Archivos v1 se siguen leyendo con `entities=[]` (campo opcional al parsear).
- `SavedMap` extendido con `SavedEntity` + `SavedMeshRenderer`. `SceneSerializer::save` acepta `Scene*` opcional. `EditorApplication::tryOpenProjectPath` aplica las entidades persistidas tras `rebuildSceneFromMap`.
- Tests: +8 casos / +34 asserciones en `test_mesh_asset.cpp` + `test_scene_serializer.cpp`. `add_test` con `WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}` para que tests que abren archivos reales (pyramid.obj via assimp) resuelvan desde la raíz del repo. Suite total 90/380.
- `assets/meshes/pyramid.obj`: pirámide base cuadrada escrita a mano (5 v / 6 tris / con UVs). `.gitignore` fix: `!assets/meshes/*.obj` para evitar colisión con `*.obj` de objetos MSVC.

**Hito 13** — Gizmos y selección (tag `v0.13.0-hito13`):
- `src/engine/scene/ScenePick.{h,cpp}`: API pura `pickEntity(scene, view, projection, ndc) -> ScenePickResult{entity, worldPoint, distance}`. Ray-AABB (slab test) para entidades con `MeshRendererComponent` (asume cubo unitario `[-0.5, 0.5]^3` escalado por Transform); ray-sphere de radio 0.6m para `LightComponent`/`AudioSourceComponent` sin mesh.
- `ViewportPanel`: click izquierdo con threshold de 4px distingue click puro de drag. Dispara `ClickSelect{ndcX, ndcY}` via `consumeClickSelect()`. Nuevo callback `setOverlayDraw(fn)` invocado después de `ImGui::Image` con `ImDrawList` clippeado al área de la imagen — es el único "pincel" para iconos + gizmos.
- `EditorApplication`: overlay callback completo con helpers `project(world→screen)`, `cameraRay(ndc)`, `closestParam(lineA, lineB)`. Guarda `m_lastView`/`m_lastProjection` para que el overlay use las mismas matrices que el render del frame anterior.
- **Iconos 2D**: círculo amarillo para `LightComponent`, cian para `AudioSourceComponent`. Halo azul si la entidad está seleccionada. Sin assets externos: todo con `ImDrawList::AddCircle` / `AddCircleFilled`.
- **Translate gizmo**: 3 flechas (X rojo / Y verde / Z azul) axis-aligned desde la entidad seleccionada. Picking por distancia 2D al segmento (8px). Drag proyecta el delta del mouse al eje con `closestParam` 3D-line vs 3D-line.
- **Rotate gizmo**: 3 anillos (48-seg polyline) por eje. Sign del delta angular invertido según `dot(axis, cam_to_origin)` para que rotar a la derecha siempre se sienta natural.
- **Scale gizmo**: 3 ejes (línea + cuadrado al final) + **cuadrado blanco central** para uniform scale (handle `axis=3`). Factor `1 + deltaPx / 60`, clamp >= 0.01.
- **Hotkeys W/E/R** cambian modo. Respetan `ImGui::WantTextInput` (no pisan edits del Inspector).
- `InspectorPanel`: esconde rotation/scale si la entidad no tiene `MeshRendererComponent` (feedback del dev — no tiene sentido escalar una luz puntual). Sigue mostrando position siempre.
- **Play Mode limpio**: early return del overlay si `m_mode == Play`. Los iconos + gizmos son andamios de edición, no parte del juego.
- Tests: `tests/test_picking.cpp` con 7 casos (scene vacía, ray central hitea mesh, light pickable por esfera, más cercano gana, ray al cielo no hitea, entidad detrás no hitea, sin Transform safe). Suite total 106/454.

**Hito 12** — Física con Jolt (tag `v0.12.0-hito12`):
- `JoltPhysics v5.2.0` via CPM, runtime dinámica `/MDd` forzada (default `/MTd` colisionaba con nuestro proyecto). Solo la lib principal; samples/viewer/tests off.
- `src/engine/physics/PhysicsWorld.{h,cpp}`: wrapper RAII sobre `JPH::PhysicsSystem` + Factory + TempAllocator + JobSystem + filters. Layers `Static` (geometría del mapa) + `Moving` (dinámicos). API compacta: `createBody`, `destroyBody`, `bodyPosition`, `setBodyPosition`, `addForce`, `addImpulse`, `step`. Forward decls de `JPH::` en el header para no contaminar el motor.
- `RigidBodyComponent { type, shape, halfExtents, mass, bodyId }` en `Components.h`. Types: Static / Kinematic / Dynamic. Shapes: Box / Sphere / Capsule.
- `EditorApplication::updateRigidBodies(dt)`: materializa bodies (bodyId==0 → create), stepea la sim en Play Mode y sincroniza body→Transform.position.
- `rebuildSceneFromMap`: destruye bodies de Jolt antes del `registry.clear()`; cada tile sólido agrega `RigidBodyComponent(Static, Box, halfExtents=tileSize/2)`.
- Demo `Ayuda > Agregar caja física demo`: spawnea entidad "CajaFisica" (Dynamic, 1m Box, 5kg) a 6m de altura que cae por gravedad.
- InspectorPanel: sección RigidBody con combos type/shape, halfExtents, mass (solo Dynamic), display de bodyId.
- Persistencia: `SavedRigidBody {type, shape, halfExtents, mass}` en `SceneSerializer`; pose NO se guarda (Jolt authoritative runtime). `k_MoodmapFormatVersion` 3 → 4.
- Canal de log `physics`. Tests: round-trip RigidBody (99/442 suite).
- **Diferidos:** CharacterController (Bloque 4), debug draw F2 (Bloque 6), `test_physics.cpp`. Ver `PLAN_HITO12.md` sección pendientes.

**Hito 11** — Iluminación Blinn-Phong (tag `v0.11.0-hito11`):
- Vertex layout extendido a stride 11 (pos+color+uv+normal). `createCubeMesh` emite normales planas por cara; `MeshLoader` (assimp) preserva las normales que `aiProcess_GenNormals` calcula. Stride viejo de 8 dejó de existir.
- `shaders/lit.{vert,frag}`: Blinn-Phong forward (ambient + diffuse + specular). Soporta 1 `DirectionalLight` global + hasta `MAX_POINT_LIGHTS=8`. Atenuación cuadrática smooth con cutoff por `radius`. Normal en mundo via `mat3(transpose(inverse(uModel)))` por vertex.
- `LightComponent { type, color, intensity, radius, direction, enabled }` real (antes placeholder Hito 7). Inspector tiene sección con sliders en vivo.
- `src/systems/LightSystem.{h,cpp}`: `buildFrameData(scene)` arma snapshot por frame; `bindUniforms(shader, data, cameraPos)` sube nombres tipo `uPointLights[i].radius` (la cache de `glGetUniformLocation` lo hace barato).
- `EditorApplication`: agrega `m_litShader` + `m_lightSystem`; el render scene-driven usa lit en lugar de default. `cameraPos` viene de la cámara activa (Editor o Play).
- Demo: `Ayuda > Agregar luz puntual demo` spawnea entidad "Luz demo" en (0,4,0). La sala se ilumina realista.
- Persistencia: `SavedLight` en `SceneSerializer` con type/color/intensity/radius/direction/enabled. `k_MoodmapFormatVersion` 2 → 3 (v2 sin campo `light` se carga igual).
- Tests: `test_lighting.cpp` (7 casos LightSystem) + round-trip de Light en `test_scene_serializer.cpp`. Suite total 98/428.

**Hito 9** — Audio básico con miniaudio (tag `v0.9.0-hito9`):
- `miniaudio` v0.11.21 vendored single-header (`external/miniaudio/`), INTERFACE target + `miniaudio_impl.cpp` con `MA_NO_ENCODING` + `MA_NO_GENERATION`.
- `src/engine/audio/AudioDevice.{h,cpp}`: wrapper RAII sobre `ma_engine`. Null-safe (mute si falla init). API: `play(clip, volume, loop, is3D, pos) -> SoundHandle`, `stop`, `stopAll`, `setSoundPosition`, `setListener`, `activeSoundCount`.
- `src/engine/audio/AudioClip.{h,cpp}`: metadata-only (path, duración, sr, canales). El PCM lo cachea el resource manager interno de miniaudio.
- `AssetManager` extendido con tabla paralela de audio: `AudioAssetId` (u32, 0 = missing), `loadAudio`, `getAudio`, `audioPathOf`. Factory inyectable para tests sin hardware.
- `AudioSourceComponent { clip, volume, loop, playOnStart, is3D, handle, started }` en `Components.h`.
- `src/systems/AudioSystem.{h,cpp}`: dispara `playOnStart`, sincroniza posición 3D desde `TransformComponent`, `clear()` invalida handles antes del `registry.clear()`.
- Inspector: sección AudioSource con combo de clips + sliders volumen + toggles + botón Reproducir.
- AssetBrowser: sección "Audio" colapsable (paths + metadata).
- Demo: `Ayuda > Agregar audio source demo` spawnea entidad con `beep.wav` loop 3D en `(-10, 1.5, -10)`. En Play Mode WASD cerca/lejos modula volumen.
- `assets/audio/missing.wav` (silencio 100 ms) + `assets/audio/beep.wav` (sinusoidal 440 Hz 0.5 s) generados con `tools/gen_missing_audio.py` y `tools/gen_beep_audio.py`.
- Canal de log `audio` registrado.
- Tests: `tests/test_audio.cpp` (6 casos). Suite 83/346.

**Hito 8** — Scripting con Lua (tag `v0.8.0-hito8`):
- `walterschell/Lua` (tag `v5.4.5`) como wrapper CMake de Lua 5.4 upstream. Target `lua_static`. Opciones: `LUA_BUILD_BINARY OFF`, `LUA_BUILD_COMPILER OFF`, `LUA_ENABLE_TESTING OFF` (si no, registra `lua-testsuite` en CTest que requiere `lua.exe`).
- `ThePhD/sol2` v3.3.0 como binding C++17↔Lua. Wrapped detrás de `src/engine/scripting/LuaBindings.{h,cpp}` para no filtrar `sol::*`.
- `ScriptComponent { std::string path; bool loaded; std::string lastError; }` en `Components.h`. El `sol::state` NO vive en el componente (no copiable); vive en un `std::unordered_map<entt::entity, std::unique_ptr<sol::state>>` dentro del `ScriptSystem`.
- `src/systems/ScriptSystem.{h,cpp}`: `update(Scene&, dt)` carga lazy al primer frame por entidad, llama `onUpdate(self, dt)`. Errores se loguean al canal nuevo `script` y desactivan el script hasta recarga. `clear()` se llama desde `rebuildSceneFromMap` (el `registry.clear()` invalida los handles).
- `LuaBindings::setupLuaBindings(state, entity)`: expone `Entity.tag` (R) / `.transform` (REF a `TransformComponent&`), `TransformComponent.position/.rotationEuler/.scale` (glm::vec3 con `.x/.y/.z`), tabla global `log` con `info/warn`. `self` global. Libs habilitadas: `base` + `math` + `string` (sin io/os/package: sandbox razonable).
- Hot-reload (Bloque 4): throttle global 500 ms via accumulator; al cambiar mtime re-ejecuta `safe_script_file` sobre el mismo `sol::state` (preserva globals). Log: `Recargado '…'`.
- Inspector (Bloque 5): sección Script con `InputText` del path + botón **Recargar** + `lastError` en rojo. Recargar/editar path → `loaded=false` (reset duro del state).
- Demo (Bloque 6): `assets/scripts/rotator.lua` (+45°/s en Y). Item de menú `Ayuda > Agregar rotador demo`. `EditorUI::requestSpawnRotator/consumeSpawnRotatorRequest`. `EditorApplication` crea entidad "Rotador" con Transform(0,4,0) + MeshRenderer(cubo+grid.png) + ScriptComponent.
- **Fix reactivo**: sin render scene-driven, la entidad tenía MeshRenderer pero nadie la dibujaba. Se agregó un pase inline en `renderSceneToViewport` que itera `Scene::forEach<Transform, MeshRenderer>` saltando entidades con tag prefijo `Tile_`. Transicional — Hito 10 migrará el render completo a scene-driven.
- Tests: +3 casos nuevos en `test_lua_bindings.cpp` (script muta Transform, hot-reload re-ejecuta, error guarda `lastError`). Suite total 74/330.

**Hito 7** — Entidades, componentes, jerarquía (tag `v0.7.0-hito7`):
- EnTT 3.13.2 via CPM, linkeada a MoodEditor + mood_tests.
- `Scene` (envuelve `entt::registry`) + `Entity` (16 bytes: `entt::entity` + `Scene*`) + `Components.h` (Tag, Transform, MeshRenderer, Camera, Light). Todo `entt::` escondido detrás de la fachada.
- `rebuildSceneFromMap` mantiene la Scene sincronizada con el GridMap (llamada en buildInitial / tryOpenProjectPath / drop / closeProject). Se hace `registry.clear()` en-place para no invalidar los `Scene*` de los paneles.
- `HierarchyPanel` (panel izquierdo 18%) lista entidades por tag, click selecciona.
- `InspectorPanel` reescrito con secciones por componente (InputText, DragFloat3, ColorEdit3, Combo). Ediciones son ephemeral en este hito (el `.moodmap` sigue siendo grid).
- Tests: +8 casos / +24 asserciones. Suite 71/322.

**Hito 6** — Serialización de proyectos y mapas (tag `v0.6.0-hito6`):
- Bloque 0 (arrastrado Hito 5): `GridRenderer` extraído a `src/systems/GridRenderer.{h,cpp}`; `ViewportPick` + hover cyan del tile bajo el cursor; drag & drop Asset Browser→tile con payload `"MOOD_TEXTURE_ASSET"`; `AssetManager` con `TextureFactory` inyectable (desbloquea tests sin GL); menú `Ver > Restablecer layout`; debug lines a 2 px; pan middle-drag estilo Blender en `EditorCamera`.
- `src/engine/serialization/JsonHelpers.h` (header-only): adaptadores ADL para `glm::vec2/3/4` (array `[x,y,z]`), `AABB` (`{min,max}`), `TileType` (strings `"empty"/"solid_wall"` via `NLOHMANN_JSON_SERIALIZE_ENUM`). Constantes `k_MoodmapFormatVersion=1`, `k_MoodprojFormatVersion=1` + `checkFormatVersion` que rechaza versiones futuras.
- `src/engine/serialization/SceneSerializer.{h,cpp}`: `save(map, name, assets, path)` y `load(path, assets) -> optional<SavedMap>`. Schema `.moodmap` con tiles en row-major + texturas por path lógico (string estable).
- `src/engine/serialization/ProjectSerializer.{h,cpp}`: `save(Project)`, `load(moodprojPath) -> optional<Project>`, `createNewProject(root, name) -> optional<Project>`. Schema `.moodproj` con version/name/defaultMap/maps[]. `Project::root` se infiere del `parent_path` al cargar.
- `AssetManager` extendido con `pathOf(id) -> string` (vector paralelo `m_texturePaths`), `TextureFactory` inyectable, y caché de `"textures/missing.png"` → id 0.
- `portable-file-dialogs 0.1.0` como dep (CPM DOWNLOAD_ONLY + target INTERFACE) para diálogos nativos.
- `EditorApplication` gana `std::optional<Project>`, `m_currentMapPath`, `m_projectDirty`, `updateWindowTitle`, `markDirty`, y 5 handlers (`handleNewProject/OpenProject/Save/SaveAs/CloseProject`). Título SDL dinámico con `*` si dirty. Atajo `Ctrl+S` (en Editor Mode). Menú Archivo con Nuevo/Abrir/Guardar/Guardar como/Cerrar; últimos 3 se grayan sin proyecto.
- Helper `buildInitialTestMap()` separado: se ejecuta al arrancar y al cerrar proyecto (baseline conocido).
- Tests: +26 casos, +102 asserciones (7 AssetManager con mocks, 6 JsonHelpers, 6 SceneSerializer, 7 ProjectSerializer). Suite total: 61/281.
- Fixes incorporados: drop al tile correcto (rect capturado antes de `BeginDragDropTarget`); cyan visible durante drag (flag `AllowWhenBlockedByActiveItem`).
- Pendientes menores (ver `PLAN_HITO6.md`): Bloque 5 (editor.state.json) diferido; "guardar como" completo; diálogo de dirty-check al cerrar/abrir; UI multi-mapa.

### Dependencias que baja CPM al configurar

- SDL2 `release-2.30.2`
- GLM `1.0.1`
- spdlog `1.14.1`
- ImGui rama `docking` (compilado como target interno `imgui` con backends SDL2 + OpenGL3)
- doctest `2.4.11` (sólo para el target `mood_tests`)
- nlohmann_json `3.11.3` (serialización de `.moodmap` / `.moodproj`)
- portable-file-dialogs `0.1.0` (DOWNLOAD_ONLY, target INTERFACE propio `pfd`)
- EnTT `3.13.2` (ECS, Hito 7; oculto detrás de fachada `Scene`/`Entity`)
- Lua `5.4.5` via `walterschell/Lua` wrapper (Hito 8; target `lua_static`)
- sol2 `v3.3.0` (Hito 8; binding C++↔Lua header-only, aislado detrás de `LuaBindings`)
- miniaudio `v0.11.21` vendored single-header (Hito 9; `external/miniaudio/`, target INTERFACE `miniaudio`)
- JoltPhysics `v5.2.0` (Hito 12; target `Jolt`, runtime `/MDd` forzado)
- assimp `v5.4.3` via CPM (Hito 10; static, solo importers OBJ/glTF/FBX, sin exporters/tests/CLI)

### Herramientas externas necesarias (solo para regenerar, no para build)

- `glad2` (Python) — para regenerar GLAD si cambia la versión de GL. Ver `external/glad/README.md`.
- `Pillow` (Python) — para regenerar `assets/textures/grid.png` desde `tools/gen_grid_texture.py`. No hace falta si el PNG ya está commiteado.

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
