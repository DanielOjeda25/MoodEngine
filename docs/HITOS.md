# Roadmap de hitos — MoodEngine

Ver `MOODENGINE_CONTEXTO_TECNICO.md` sección 10 para la lista completa con detalle.

## Estado

- [x] **Hito 0** — Repositorio creado en GitHub.
- [x] **Hito 1** — Shell del editor (completado, tag `v0.1.0-hito1`).
- [x] **Hito 2** — Primer triángulo con OpenGL (completado, tag `v0.2.0-hito2`).
- [x] **Hito 3** — Cubo texturizado con cámara (completado, tag `v0.3.0-hito3`).
- [x] **Hito 4** — Mundo grid + colisiones AABB (completado, tag `v0.4.0-hito4`).
- [x] **Hito 5** — Asset Browser + gestión de texturas (completado, tag `v0.5.0-hito5`).
- [x] **Hito 6** — Serialización de proyectos y mapas (completado, tag `v0.6.0-hito6`).
- [x] **Hito 7** — Entidades, componentes, jerarquía (completado, tag `v0.7.0-hito7`).
- [x] **Hito 8** — Scripting con Lua (completado, tag `v0.8.0-hito8`).
- [ ] Hito 9 — Audio básico.
- [ ] Hito 10 — Importación de modelos 3D.
- [ ] Hito 11 — Iluminación Phong/Blinn-Phong.
- [ ] Hito 12 — Física con Jolt.
- [ ] Hito 13 — Gizmos y selección.
- [ ] Hito 14 — Prefabs.
- [ ] Hito 15 — Skybox, fog, post-procesado.
- [ ] Hito 16 — Shadow mapping.
- [ ] Hito 17 — PBR.
- [ ] Hito 18 — Deferred / Forward+.
- [ ] Hito 19 — Animación esquelética.
- [ ] Hito 20 — UI del juego con RmlUi.
- [ ] Hito 21 — Empaquetado standalone.
- [ ] Hito 22 — Undo/Redo.
- [ ] Hito 23 — Editor de materiales.
- [ ] Hito 24 — Particle system.
- [ ] Hito 25 — Linux.

## Hito 1 — Shell del editor

**Objetivo:** ventana SDL2 con contexto OpenGL 4.5 + Dear ImGui docking + logging + estructura completa de carpetas.

**Criterios de aceptación:** ver `MOODENGINE_CONTEXTO_TECNICO.md` sección 11.2.

**Siguiente paso tras completarlo:** Hito 2 — inicializar GLAD, escribir un shader básico, dibujar un triángulo al viewport (render a textura que ImGui muestra en el panel Viewport).

### Pendientes menores detectados en Hito 1

- ~~Solapamiento visual entre paneles al arrancar (layout inicial del dockspace).~~ Resuelto en Hito 2 con `DockBuilder` (`Dockspace.cpp`), respeta `imgui.ini` si existe.

## Hito 2 — Primer triángulo con OpenGL

**Objetivo:** dejar de ser shell vacío y dibujar geometría propia (triángulo RGB) en el panel Viewport, con pipeline de render real: GLAD + RHI abstracto + backend OpenGL + shaders propios + framebuffer offscreen.

**Criterios de aceptación:** ver `PLAN_HITO2.md` (todos cumplidos: automáticos + visuales + resize del panel + default layout sin superposiciones).

**Siguiente paso tras completarlo:** Hito 3 — cubo texturizado con cámara. Agregar stb_image (textura), matrices de modelo/vista/proyección con GLM, cámara FPS controlable con WASD + ratón en Play Mode, cámara orbital en Editor Mode. Empezar a usar doctest para testear matemática.

### Pendientes menores detectados en Hito 2

- ~~Debug context de OpenGL (`GL_ARB_debug_output`).~~ Resuelto post-cierre Hito 6: `Window` pide `SDL_GL_CONTEXT_DEBUG_FLAG` en builds Debug, `EditorApplication` engancha `glDebugMessageCallback` y rutea a `Log::render()`.
- **Dual máquina (laptop Iris Xe + desktop NVIDIA GTX 1660).** Ya documentado como gotcha en `ESTADO_ACTUAL.md` sección 5; no es un pendiente de código.
- **DPI awareness**: fuentes/widgets no escalan entre monitores con distinta densidad. **Trigger:** Hito 15+ (UI polish).
- **Shader hot-reload**: útil cuando los shaders crezcan. **Trigger:** Hito 11 (iluminación Phong/Blinn-Phong) o Hito 17 (PBR).

## Hito 3 — Cubo texturizado con cámara

**Objetivo:** pipeline 3D completo: matrices MVP, texturas con stb_image, dos cámaras (orbital / FPS) con entradas reales, modos Editor/Play, y primer target de tests con doctest.

**Criterios de aceptación cumplidos:** cubo rotando con textura en Viewport, resize sin distorsión, toggle Play/Stop con Esc para salir, botón verde/rojo en menu bar, tests pasando (10 casos, 37 asserciones).

**Siguiente paso tras completarlo:** Hito 4 — Mundo grid + colisiones AABB. Mapa definido como grid 2D renderizado como cubos 3D (modo "Wolfenstein simulado"), colisiones AABB propias para que el jugador no atraviese paredes, debug rendering básico (líneas, cajas).

### Pendientes menores detectados en Hito 3

- ~~FPS counter en la status bar no se visualiza claramente en todos los layouts.~~ Resuelto 2026-04-23: `StatusBar` usa `ImGui::BeginViewportSideBar(ImGuiDir_Down)` y se dibuja antes del dockspace para que el viewport recorte su work area antes de que los paneles docked lo consuman.

## Hito 4 — Mundo grid + colisiones AABB

**Objetivo:** primer "mundo" jugable. Mapa definido como grid 2D de tiles renderizado como cubos 3D, colisiones AABB propias que impiden atravesar paredes (con slide en esquinas), y debug rendering para visualizar las AABBs.

**Criterios de aceptación cumplidos:**
- Viewport muestra sala 8×8 con bordes sólidos y columna central (29 paredes).
- En Play Mode, WASD mueve y no atraviesa paredes; slide diagonal funciona.
- Tecla `F1` togglea debug draw (AABBs de tiles en amarillo + AABB del jugador en verde en Play).
- Log del canal `world`: `Mapa cargado: prueba_8x8 (29 tiles solidos)`.
- Cierre limpio, exit 0. Suite de tests: 30 casos, 159 asserciones.

**Siguiente paso tras completarlo:** Hito 5 — Asset Browser + gestión de texturas. AssetManager, VFS inicial, texturas por tile desde el editor, textura fallback rosa-negro, consola in-game. Antes de empezar, aplicar la convención de escala realista postergada del Hito 4 (ver `docs/PLAN_HITO5.md`).

### Pendientes menores detectados en Hito 4

- ~~Convención de escala 1 unidad = 1 m SI.~~ Resuelto en Hito 5 Bloque 0.
- ~~`GridRenderer` inline en EditorApplication.~~ Resuelto en Hito 6 Bloque 0.
- ~~Face culling (`GL_CULL_FACE`).~~ Resuelto post-cierre Hito 6: activado en `OpenGLRenderer::init()` con winding CCW.
- **Desacoplar `wallHeight` de `tileSize` en `GridMap`**: hoy el cubo es `scale(tileSize)` en los 3 ejes. **Trigger:** cuando aparezcan salas con altura distinta del ancho de tile (Hito 13+ con prefabs más ricos).
- **Extraer `IDebugRenderer` abstracto**: `OpenGLDebugRenderer` es concreto. **Trigger:** cuando se agregue un segundo backend de render (Vulkan/DirectX) — no está en el roadmap cercano.
- **`mood_core` como static lib compartida con `mood_tests`**: `tests/CMakeLists.txt` recompila 10 .cpp en la actualidad. **Trigger medible:** cuando ese número pase de 15, o el build del target `mood_tests` exceda los 15 segundos sostenidos.
- **Tests de `FpsCamera::computeMoveDelta` directos**: hoy lo testeo via `move`. **Trigger:** cuando cambie la lógica interna de strafe/yaw.

## Hito 5 — Asset Browser + gestión de texturas

**Objetivo:** pasar de "motor que carga una textura hardcoded" a motor con catálogo de assets consultable desde el editor. AssetManager con cache, VFS sandbox, textura fallback, texturas por tile, Asset Browser con miniaturas reales, Console in-game.

**Criterios de aceptación cumplidos:**
- Convención de escala 1 unit = 1 m SI adoptada (Bloque 0, arrastrado del Hito 4).
- Textura `missing.png` (chequered rosa/negro) cargada en slot 0 del `AssetManager`; cualquier fallo de carga cae ahí.
- `VFS` rechaza `..`, `.`, paths absolutos y leading `/`/`\`; `AssetManager` lo usa para resolver paths lógicos.
- `AssetBrowserPanel` muestra miniaturas reales de los PNG de `assets/textures/` con click-to-select.
- Paredes del mapa renderizadas con 2 texturas distintas (grid.png en borde, brick.png en columna central).
- `ConsolePanel` acoplado al inferior-derecho, muestra todos los logs del `LogRingSink` coloreados por nivel con filtro por canal.
- Tests: suite 35/179 (+5 para VFS).

**Siguiente paso tras completarlo:** Hito 6 — Serialización de proyectos y mapas (`.moodproj` + `.moodmap`). Antes, el drag & drop desde Asset Browser a tile del viewport y el refactor `GridRenderer`, ambos arrastrados del Hito 5.

### Pendientes del Hito 5 (todos resueltos o con condición de disparo clara)

- ~~Tests de `AssetManager`.~~ Resueltos en Hito 6 Bloque 0 (factory inyectable + `MockTexture`, 7 casos).
- ~~Drag & drop Asset Browser → tile del viewport.~~ Resuelto en Hito 6 Bloque 0 (`ViewportPick` + `BeginDragDropTarget`).
- ~~Extraer `GridRenderer` de `EditorApplication`.~~ Resuelto en Hito 6 Bloque 0 (`src/systems/GridRenderer.{h,cpp}`).
- ~~Menú "Restablecer layout".~~ Resuelto en Hito 6 Bloque 0 (`Ver > Restablecer layout`).
- ~~Color configurable del AABB del jugador en debug draw.~~ El objetivo real (distinguir visualmente player de tiles) ya está cumplido: tiles amarillos, player verde. "Configurable vía UI" no tiene caso de uso sin panel de Settings; si se quiere cambiar, se edita la constante en `EditorApplication::renderSceneToViewport`. Cerrado.
- ~~Hot-reload del `AssetBrowserPanel`.~~ Resuelto post-cierre de Hito 6: `AssetManager::reloadChanged()` compara mtime y re-invoca la factory. El botón "Recargar" dispara el reupload al GPU entre frames (safe: no destruye handles mid-draw).
- **Tracy profiler**: no es un pendiente olvidado, es una **integración planificada con condición de disparo explícita**. El doc técnico sección 9.3 y 9.5 establece "no pre-optimizar, introducir complejidad sólo cuando aparece el problema concreto". Tracy entra cuando los frames se vuelvan medibles como pesados — objetivos concretos: escenas complejas del Hito 11 (iluminación) o Hito 17 (PBR). Hasta entonces no aporta valor.

## Hito 6 — Serialización de proyectos y mapas

**Objetivo:** persistir el estado del editor en disco (`.moodproj` + `.moodmap`) con JSON versionado. Ciclo completo desde el menú Archivo: nuevo, abrir, guardar, cerrar.

**Criterios de aceptación cumplidos:**
- `Archivo > Nuevo Proyecto` abre diálogo nativo (portable-file-dialogs), crea estructura `<root>/maps/`, `<root>/textures/`, `<root>/<name>.moodproj` y guarda el estado actual del editor como `maps/default.moodmap`.
- `Archivo > Abrir Proyecto` filtra `*.moodproj` y carga el mapa default.
- `Archivo > Guardar` (Ctrl+S) escribe ambos archivos; grayado si no hay proyecto.
- `Archivo > Cerrar proyecto` vuelve al mapa de prueba hardcodeado.
- Título dinámico `MoodEngine Editor - <name>` con `*` si hay cambios sin guardar.
- Versionado: `k_MoodmapFormatVersion = 1`, `k_MoodprojFormatVersion = 1`. Cargar versión futura falla con mensaje claro.
- Tests: 61 casos / 281 asserciones (+6 JSON helpers, +6 SceneSerializer, +7 ProjectSerializer, +7 AssetManager con factory mock).
- Bonus (Bloque 0): `GridRenderer` extraído, `ViewportPick` + hover cyan, drag & drop Asset Browser→tile, pan estilo Blender (middle-drag), `Ver > Restablecer layout`, debug lines 2 px, tests AssetManager.

**Siguiente paso tras completarlo:** Hito 7 — Entidades, componentes, jerarquía. Integrar EnTT detrás de una fachada (`Entity`, `Scene`), panel Hierarchy (árbol de entidades), Inspector funcional editando componentes. Componentes básicos: `Transform`, `MeshRenderer`, `Camera`, `Light`.

### Pendientes menores detectados en Hito 6

- ~~Bloque 5 (editor state persistence).~~ Resuelto post-cierre: `.mood/editor_state.json` con último proyecto + `debugDraw`; reabre al arrancar.
- ~~Diálogo de confirmación al descartar cambios sin guardar.~~ Resuelto: `confirmDiscardChanges()` con `pfd::message` yes/no/cancel; disparado desde Cerrar / Abrir / Nuevo.
- **"Guardar como" completo** (hoy duplica vía Nuevo Proyecto). **Trigger:** cuando haya proyectos con ≥2 mapas en la práctica (polish).
- **UI multi-mapa por proyecto**: el schema `.moodproj` ya soporta `maps[]` pero el editor abre siempre `defaultMap`. **Trigger:** mismo que arriba.
- **Hot-reload de `.moodmap` editado externamente**: por ahora ignoramos cambios en disco del archivo abierto. **Trigger:** cuando se integre un editor externo tipo Tiled o similar.
- **Tests E2E editor↔serialization**: los serializers están testeados con mocks a bajo nivel. **Trigger:** Hito 10+ (assimp + meshes reales).

## Hito 7 — Entidades, componentes, jerarquía

**Objetivo:** infraestructura ECS (Scene + Entity facade sobre EnTT) + paneles Hierarchy e Inspector funcionales. La sala de prueba existe como escena de 29 entidades (una por tile sólido).

**Criterios de aceptación cumplidos:**
- EnTT 3.13.2 via CPM, integrada en MoodEditor + mood_tests.
- `Scene` + `Entity` + `Components.h` (Tag / Transform / MeshRenderer / Camera / Light) — EnTT escondido detrás de la fachada.
- `rebuildSceneFromMap` mantiene la scene sincronizada con el `GridMap` (llamada en buildInitialTestMap / tryOpenProjectPath / drop / close).
- `HierarchyPanel` (panel izquierdo 18% del dockspace default) lista las entidades por Tag. Click selecciona.
- `InspectorPanel` reescrito: secciones por componente con widgets ImGui (InputText, DragFloat3, ColorEdit3, Combo). Edición ephemeral por el alcance del hito.
- Tests: +8 casos / +24 asserciones para Scene + Entity. Suite total: 71 casos / 322 asserciones.

**Siguiente paso tras completarlo:** Hito 8 — Scripting con Lua. Integrar Lua 5.4 + sol2 como binding C++. `ScriptComponent` con path al `.lua`. API mínima expuesta: transform, input, log, spawn/destroy entity. Hot-reload de scripts al detectar mtime.

### Pendientes menores detectados en Hito 7

- **RenderSystem sobre Scene** (iterar entidades en vez del grid). **Trigger:** Hito 10 (assimp) o Hito 11 (iluminación por entidad). **Update Hito 8:** se agregó un pase scene-driven mínimo para dibujar el rotador demo; la migración completa sigue diferida.
- **Scene authoritative + persistencia de ediciones del Inspector.** **Trigger:** mismo.
- **Jerarquía padre-hijo** (ChildrenComponent). **Trigger:** Hito 14 (prefabs).
- **Drag & reparent en Hierarchy.** **Trigger:** mismo.
- **Drop ECS-based** (editar MeshRendererComponent en vez de GridMap.setTile). **Trigger:** Scene authoritative.
- **Undo/Redo del Inspector.** **Trigger:** Hito 22.

## Hito 8 — Scripting con Lua

**Objetivo:** permitir que entidades tengan comportamiento en Lua 5.4 sin reiniciar el editor. `ScriptComponent` con path al `.lua`, API mínima a C++ (Transform + Log), hot-reload por mtime, demo visible.

**Criterios de aceptación cumplidos:**
- Lua 5.4.5 vía `walterschell/Lua` (target `lua_static`) + sol2 v3.3.0, linkeados a MoodEditor y mood_tests.
- `ScriptComponent { path, loaded, lastError }` en `Components.h`; serialización diferida (el `.moodmap` no guarda entidades Scene-authoritative todavía).
- `ScriptSystem` con mapa `entt::entity → unique_ptr<sol::state>` (islas aisladas). `clear()` lo vacía al rebuildear la scene.
- `LuaBindings`: `Entity.tag`/`.transform`, `TransformComponent.position`/`.rotationEuler`/`.scale`, `log.info`/`log.warn`. Libs habilitadas: base, math, string (sin io/os/package).
- Hot-reload con throttle 0.5 s: detecta cambio de mtime y re-ejecuta `safe_script_file` sobre el mismo `sol::state` (preserva globals). Log canal `script`: `Recargado '…'`.
- Inspector: sección ScriptComponent con `InputText` del path + botón Recargar + `lastError` en rojo. Editar path o clickear Recargar → reset duro del state.
- Demo: `Ayuda > Agregar rotador demo` spawnea una entidad con `rotator.lua` (+45°/s en Y) visible y rotando en el viewport. Pase scene-driven agregado en `renderSceneToViewport` (filtra tiles por prefijo `Tile_` para evitar overdraw).
- Canal de log `script` registrado en `Log.{h,cpp}`.
- Tests: `test_lua.cpp` (smoke sol2) + `test_lua_bindings.cpp` (script muta Transform, hot-reload re-ejecuta, script roto deja `lastError`). Suite total: 74/330.

**Siguiente paso tras completarlo:** Hito 9 — Audio básico con miniaudio. AudioClip asset, AudioSourceComponent, reproducción desde Lua (sumar `audio.play(...)` a la API expuesta). Base para posicional 3D.

### Pendientes menores detectados en Hito 8

- **`Input` bindings en Lua.** **Trigger:** primer script de gameplay que necesite teclado/mouse (proyecto Wolfenstein-like).
- **`scene:createEntity` / `scene:destroyEntity` desde Lua.** **Trigger:** primer script que necesite spawn/despawn runtime (proyectiles, enemigos).
- **Acceso a más componentes desde Lua** (`MeshRenderer`, `Camera`). **Trigger:** cuando un script real lo pida.
- **Debugger de Lua** (mobdebug o similar). **Trigger:** cuando los scripts crezcan más allá de `print`.
- **Persistencia de `ScriptComponent` en `.moodmap`.** **Trigger:** Scene authoritative (Hito 10+).
- **Leak de mtimes viejos al cambiar path en caliente.** Negligible hoy (pocos paths). **Trigger:** >100 paths distintos.
