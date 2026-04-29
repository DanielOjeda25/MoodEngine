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
- [x] **Hito 9** — Audio básico (completado, tag `v0.9.0-hito9`).
- [x] **Hito 10** — Importación de modelos 3D (completado, tag `v0.10.0-hito10`).
- [x] **Hito 11** — Iluminación Phong/Blinn-Phong (completado, tag `v0.11.0-hito11`).
- [x] **Hito 12** — Física con Jolt (completado, tag `v0.12.0-hito12`).
- [x] **Hito 13** — Gizmos y selección (completado, tag `v0.13.0-hito13`).
- [x] **Hito 14** — Prefabs (completado, tag `v0.14.0-hito14`).
- [x] **Hito 15** — Skybox, fog, post-procesado (completado, tag `v0.15.0-hito15`).
- [x] **Hito 16** — Shadow mapping (completado, tag `v0.16.0-hito16`).
- [x] **Hito 17** — PBR (completado, tag `v0.17.0-hito17`).
- [x] **Hito 18** — Forward+ (completado, tag `v0.18.0-hito18`).
- [x] **Hito 19** — Animación esquelética (completado, tag `v0.19.0-hito19`).
- [x] Hito 20 — UI del juego (HUD + menú de pausa) (completado, tag `v0.20.0-hito20`).
- [x] Hito 21 — Empaquetado standalone (MoodPlayer + PackageBuilder) (completado, tag `v0.21.0-hito21`).
- [x] Hito 22 — Workflow de scripting Lua (completado, tag `v0.22.0-hito22`).
- [x] Hito 23 — AI / pathfinding básico (completado, tag `v0.23.0-hito23`).
- [x] Hito 24 — Exposed properties Lua (completado, tag `v0.24.0-hito24`).
- [x] Hito 25 — Hot-reload de shaders + polish del sistema de materiales (completado, tag `v0.25.0-hito25`).
- [x] Hito 26 — Asset pipeline: extracción de texturas + asset pack Kenney + IBL bakeado desde equirect (completado, tag `v0.26.0-hito26`).
- [ ] Hito 27 — TBD.

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

## Hito 9 — Audio básico

**Objetivo:** dotar al motor de reproducción de audio posicional sin ceremonia. `miniaudio` como backend, `AudioClip` como segundo tipo de asset, `AudioSourceComponent` integrado al ECS, `AudioSystem` que maneja listener + playback, demo visible con atenuación 3D.

**Criterios de aceptación cumplidos:**
- `miniaudio` v0.11.21 vendored single-header, target INTERFACE + `miniaudio_impl.cpp`.
- `AudioDevice` null-safe: si falla init (CI, hardware roto) queda en mute; el motor sigue corriendo. Log `audio: AudioDevice inicializado (sample_rate=48000, channels=2)`.
- `AudioClip`: inspección no-blocking de metadata (duración, sr, canales) con `ma_decoder`.
- `AssetManager` extendido con tabla paralela de audio (id 0 = missing.wav silencio 100 ms). Factoría inyectable para tests sin hardware.
- `AudioSourceComponent { clip, volume, loop, playOnStart, is3D, handle, started }` en `Components.h`.
- `AudioSystem::update` dispara playOnStart (guard `started`), sincroniza posición 3D cada frame. `clear()` antes de `rebuildSceneFromMap`.
- `InspectorPanel`: combo de clips + sliders + toggles + botón Reproducir (preview).
- `AssetBrowserPanel`: sección "Audio" colapsable con path + duración + sr + canales.
- Demo: `Ayuda > Agregar audio source demo` spawnea entidad con `beep.wav` (440 Hz 0.5 s) loop + 3D en `(-10, 1.5, -10)`. En Play Mode, acercarse con WASD sube el volumen por atenuación 3D de miniaudio.
- Canal log `audio` registrado.
- Tests: `test_audio.cpp` (6 casos: AudioDevice null-safe, AssetManager loadAudio cachea + cae a missing, getAudio nunca null, AudioSystem marca started, clear() vacía activeSoundCount, is3D actualiza posición). Suite total: 83 casos / 346 asserciones.

**Siguiente paso tras completarlo:** Hito 10 — Importación de modelos 3D con assimp. Lectura de `.obj`/`.gltf`, `MeshAsset` como tercer tipo de asset, `MeshRendererComponent` mejorado (material + multi-mesh), primer escenario no-grid.

### Pendientes menores detectados en Hito 9

- **Persistencia de `AudioSourceComponent` en `.moodmap`.** **Trigger:** Scene authoritative (igual que `ScriptComponent`, Hito 10+).
- **Drag & drop de audio** desde AssetBrowser a entidades. Hoy solo texturas. **Trigger:** misma ventana de feature polish UX.
- **Lua bindings de audio** (`audio.play("audio/beep.wav")`). **Trigger:** primer script que lo necesite.
- **Streaming de música** (clips largos). Hoy todo en RAM. **Trigger:** música de fondo >1 min.
- **Listener múltiple / configurable** (hoy = cámara activa). **Trigger:** cámaras cinematográficas.
- **Reverb / filters / effects.** **Trigger:** polish post-Hito 15.
- **`AudioSystem` tracking explícito de handles por entidad** (hoy `clear()` llama `stopAll()`). **Trigger:** segundo sistema que emita sonidos (UI clicks, etc.).

## Hito 10 — Importación de modelos 3D

**Objetivo:** romper el límite "todo es un cubo". Importar OBJ/glTF/FBX via assimp, integrarlos al Scene como `MeshAsset`+`MeshRenderer`, migrar el render a scene-driven, persistir entidades no-tile en el `.moodmap`.

**Criterios de aceptación cumplidos:**
- `assimp v5.4.3` como static lib vía CPM, solo con importers OBJ + glTF + FBX habilitados (~30 MB menos de binario).
- `MeshAsset` como tercer tipo de asset (`Texture` + `AudioClip` + `MeshAsset`); `MeshLoader::loadMeshWithAssimp` con flags `Triangulate | GenNormals | FlipUVs | CalcTangentSpace` y expansión de índices a vértices planos (matchea `glDrawArrays` sin EBO).
- `AssetManager` extendido con `MeshAssetId` (u32, slot 0 = cubo primitivo fallback) + `MeshFactory` inyectable (default `NullMesh` stub para tests sin GL).
- `MeshRendererComponent` refactorizado de `IMesh* + TextureAssetId` a `MeshAssetId + vector<TextureAssetId>`. Helper `materialOrMissing(i)`.
- Render unificado a un solo loop scene-driven (`Scene::forEach<Transform, MeshRenderer>` iterando submeshes). `GridRenderer` eliminado (dead code tras la migración).
- Drop de textura ahora usa `updateTileEntity(x, y, tex)` (edit in-place) en vez de `rebuildSceneFromMap`: preserva la selección del Hierarchy.
- AssetBrowser con sección "Meshes" (encima de Audio) que lista `.obj/.gltf/.glb/.fbx` de `assets/meshes/`. Drag payload `"MOOD_MESH_ASSET"`; drop al Viewport spawnea entidad `Mesh_<path>` en el tile bajo el cursor.
- `SceneSerializer` v2: schema nuevo con sección `entities` (filtrada por prefijo de tag `Tile_*`, solo MeshRenderer en este hito); archivos v1 se siguen leyendo con `entities=[]`. `SavedMap` extendido con `SavedEntity` + `SavedMeshRenderer`.
- `EditorApplication::tryOpenProjectPath` aplica entidades persistidas tras `rebuildSceneFromMap`. Round-trip verificado end-to-end: drag pyramid x3 → Ctrl+S → Cerrar → Abrir → 3 entidades vuelven idénticas.
- Tests: +8 casos / +34 asserciones (5 en `test_mesh_asset.cpp`, 2 en `test_scene_serializer.cpp`, test CMakeLists con `WORKING_DIRECTORY`). Suite total 90/380.
- `assets/meshes/pyramid.obj` como mesh sample escrito a mano (5 verts / 6 tris / UVs por cara). `.gitignore` fix: `!assets/meshes/*.obj` para evitar colisión con `*.obj` de objetos MSVC.

**Siguiente paso tras completarlo:** Hito 11 — Iluminación Phong/Blinn-Phong. Activar `LightComponent` (era placeholder del Hito 7), shader con normales, directional + point lights, demos de una escena iluminada. Plan en `docs/PLAN_HITO11.md`.

### Pendientes menores detectados en Hito 10

- **Preview 3D de meshes en el AssetBrowser** (thumbnails renderizados). El usuario lo pidió durante Bloque 5; requiere FB por mesh + mini-pipeline offscreen. **Trigger:** polish post-Hito 11 o cuando los meshes importados se vuelvan numerosos.
- **EBO / `OpenGLIndexedMesh`.** Hoy el loader expande índices a vértices planos (x1.8 RAM típico). **Trigger:** primer mesh importado con >50k vértices.
- **Hot-reload de meshes.** Análogo al de texturas (Hito 5). **Trigger:** iteración artística con archivos editados externamente.
- **Drop de mesh reemplaza entidad existente** (vs siempre spawnear nueva). **Trigger:** cuando aparezca el caso de uso "cambiarle el mesh a X".
- **Serializar `Script`/`Audio` components.** Fuera de scope hasta Scene authoritative. **Trigger:** Hito 14 (prefabs).
- **Materiales reales** (color, normal map, metallic/roughness). Hoy solo texture por submesh. **Trigger:** Hito 17 (PBR).
- **Drag & drop de audio** (arrastrado desde Hito 9). **Trigger:** Scene authoritative.
- **Light components funcionales.** **Trigger:** Hito 11.

## Hito 11 — Iluminación Phong / Blinn-Phong

**Objetivo:** que la escena deje de ser "unlit" (textura directa) y pase a iluminarse con Blinn-Phong forward. `LightComponent` (placeholder desde Hito 7) pasa a ser real, con soporte para 1 directional + N point lights.

**Criterios de aceptación cumplidos:**
- Vertex layout extendido a stride 11 (pos+color+uv+normal). `createCubeMesh` genera normales planas; `MeshLoader` (assimp) preserva las normales que `aiProcess_GenNormals` calcula.
- `shaders/lit.{vert,frag}`: Blinn-Phong (ambient + diffuse + specular). Soporta hasta `MAX_POINT_LIGHTS=8` con atenuación cuadrática suave hasta `radius`. 1 directional via `uDirectional`.
- `LightComponent { type, color, intensity, radius, direction, enabled }` real. Sliders en Inspector se reflejan en vivo.
- `src/systems/LightSystem.{h,cpp}` recolecta lights del Scene cada frame y sube uniforms (cache de `glGetUniformLocation` en `OpenGLShader` evita el costo).
- Demo `Ayuda > Agregar luz puntual demo` spawnea entidad con Light Point en (0,4,0). Sala 8x8 muestra sombreado real con caras frente/atrás distinguibles.
- Persistencia: `SavedLight` en `SceneSerializer` con type/color/intensity/radius/direction/enabled. `k_MoodmapFormatVersion` 2 → 3 (v2 sin campo `light` se carga igual).
- Tests: 7 nuevos en `test_lighting.cpp` + 1 round-trip de Light en `test_scene_serializer.cpp`. Suite 98/428 (+8 vs Hito 10).

**Siguiente paso tras completarlo:** Hito 12 — Física con Jolt. Reemplazar el `PhysicsSystem` propio (AABB vs grid del Hito 4) por Jolt Physics: rigid bodies, colliders convexos, gravedad, capsule controllers, integración con `Transform`. Plan en `docs/PLAN_HITO12.md`.

### Pendientes menores detectados en Hito 11

- **Hot-reload de shaders** (Bloque 6): diferido. **Trigger:** Hito 16 (shadows) o Hito 17 (PBR) — momento natural cuando los shaders crezcan.
- **Preview 3D de meshes en AssetBrowser**: re-arrastrado desde Hito 10. **Trigger:** asset humano lo pide.
- **Normal mapping**: opcional en plan, no entró. **Trigger:** Hito 17 (PBR).
- **`uNormalMatrix` precalculado en CPU**: hoy es por-vertex en GLSL. **Trigger:** frametimes que dolieren con muchos draws (Hito 18).
- **Múltiples directional lights / SpotLight**: fuera de scope del hito. **Trigger:** mapa real que lo pida.
- **Atenuación con falloff exponente configurable**: hoy cuadrática hardcoded. **Trigger:** feedback visual.
- **Persistencia de `AudioSource`**: sigue arrastrándose desde Hito 9. **Trigger:** Scene authoritative completo (Hito 14+).

## Hito 12 — Física con Jolt

**Objetivo:** reemplazar el `PhysicsSystem` casero AABB-vs-grid del Hito 4 por Jolt Physics: rigid bodies con gravedad real, colliders variados, ECS integration.

**Criterios de aceptación cumplidos:**
- `JoltPhysics v5.2.0` via CPM (runtime `/MDd` forzado, targets auxiliares off).
- `src/engine/physics/PhysicsWorld.{h,cpp}`: wrapper RAII con layers Static/Moving, API para createBody/step/setPosition/addForce.
- `RigidBodyComponent { type, shape, halfExtents, mass, bodyId }` en `Components.h`.
- `EditorApplication::updateRigidBodies` materializa bodies + stepea en Play Mode + sync body → Transform.
- Tiles sólidos ahora tienen `RigidBodyComponent(Static)` automático; el mapa es colisionable en Jolt.
- Demo `Ayuda > Agregar caja física demo` spawnea entidad Dynamic que cae por gravedad.
- Persistencia: `SavedRigidBody` en `.moodmap` (pose NO se guarda; Jolt authoritative runtime). `k_MoodmapFormatVersion` 3 → 4.
- Canal de log `physics`. Tests: round-trip RigidBody (99 casos / 442 asserciones total).

**Siguiente paso tras completarlo:** Hito 13 — Gizmos y selección. Manipuladores 3D sobre entidad seleccionada (translate/rotate/scale), ray-casting click-to-select en el viewport. Plan en `docs/PLAN_HITO13.md`.

### Pendientes menores detectados en Hito 12

- **CharacterController de Jolt** (Bloque 4): diferido — el jugador sigue con `moveAndSlide` AABB del Hito 4. Se re-pospuso en Hito 13; **trigger** pasa a un hito de gameplay específico.
- **`F2` debug draw de Jolt**: diferido junto con CharacterController. Requiere implementar `JPH::DebugRenderer` → `OpenGLDebugRenderer`.
- **`test_physics.cpp`**: diferido para evitar arrastrar Jolt (~40 MB static lib) a `mood_tests`. **Trigger:** lógica C++ no trivial que lo necesite.
- **Rotación del body → Transform.rotationEuler**: hoy solo sync de posición. **Trigger:** colisiones que hagan rodar cajas.
- **Shape `MeshFromAsset`** (Box/Sphere/Capsule solo por ahora). **Trigger:** colisiones precisas para modelos importados.
- **Triggers/sensors** para gameplay/scripts. **Trigger:** Hito 14+.
- Pendientes Hito 11 (hot-reload shaders, preview 3D, normal mapping, multi-directional, persistencia Audio) siguen en tracker.

## Hito 13 — Gizmos y selección

**Objetivo:** hacer el editor usable con el mouse sobre la escena 3D. Click-to-select con raycast + gizmos 3D (translate/rotate/scale) operados en screen-space sobre la entidad seleccionada.

**Criterios de aceptación cumplidos:**
- `src/engine/scene/ScenePick.{h,cpp}`: `pickEntity(scene, view, projection, ndc)` retorna la entidad más cercana al rayo. Ray-AABB (slab test) para meshes, ray-sphere (radio 0.6m) para Light/AudioSource sin mesh.
- `ViewportPanel`: captura click izquierdo con threshold 4px (click vs drag) + callback `setOverlayDraw` para dibujar sobre la imagen con `ImDrawList`.
- Overlay en `EditorApplication`: iconos 2D (círculos) para Light (amarillo) y AudioSource (cian), con halo azul si están seleccionados. Gizmo de Translate (3 flechas axis), Rotate (3 anillos 48-seg), Scale (3 líneas + cuadrados + **cuadrado blanco central** uniform). Color RGB por eje.
- Drag gizmo: `closestParam` 3D-line vs 3D-line para translate/scale-axis; delta angular 2D alrededor del centro del gizmo para rotate (con sign por `dot(axis, cam_to_origin)`).
- Hotkeys `W/E/R` cambian modo, respetan `ImGui::WantTextInput`.
- Inspector esconde scale/rotation si la entidad no tiene `MeshRendererComponent` (pedido del dev: no tiene sentido escalar una luz).
- Overlays ocultos en Play Mode (iconos + gizmos son andamios de edición).
- Tests: `tests/test_picking.cpp` con 7 casos (scene vacía, ray central, light por esfera, más cercano gana, rayo al cielo, entidad detrás, sin Transform). 106/106 suite verde.

**Siguiente paso tras completarlo:** Hito 14 — Prefabs. Definir un formato `.moodprefab` reusable (entidad + componentes + hijos), instanciación en escena desde el AssetBrowser, override local de propiedades por instancia. Plan en `docs/PLAN_HITO14.md`.

### Pendientes menores detectados en Hito 13

- **Shift+click multi-selección + rectangle select**: diferido. **Trigger:** Hito 14+.
- **Snap to grid** en drag de gizmo: diferido. **Trigger:** Hito 15 (skybox/polish) o cuando haga ruido.
- **Esc cancela drag sin commit**: requiere staging del valor previo → alineado con undo/redo (Hito 22).
- **Label visual del modo activo (W/E/R)**: las formas se distinguen solas; si aparece demanda, se agrega.
- **Parent-space vs world-space toggle**: siempre world por ahora.
- **Handles 2D** (plano para mover en 2 ejes): diferido; con 3 axis + uniform alcanza.
- **Tests de math de gizmo (closestParam, project)**: diferidos a un hito de tests de UI.
- **Picking contra malla real** (no AABB conservativa): requiere `MeshFromAsset` + BVH, alineado con Hito 11/Jolt.
- Pendientes Hito 12 (CharacterController, body-rotation sync, `MeshFromAsset`) siguen arrastrándose.

## Hito 14 — Prefabs

**Objetivo:** sistema de prefabs reusables. Guardar una entidad con sus componentes como `.moodprefab`, instanciar copias via drag desde el AssetBrowser al Viewport, mantener override local por instancia (modificarla no toca el prefab base), persistir el link `prefabPath` en el `.moodmap` para futuras features de revert/apply.

**Criterios de aceptación cumplidos:**
- Schema `.moodprefab` v1 (`{version, name?, root: SavedEntity, children: [...]}`). Reusa `SavedEntity` del Hito 10/11/12 (Mesh + Light + RigidBody).
- `engine/serialization/EntitySerializer.{h,cpp}` extraído del `SceneSerializer` para que el helper Component<->JSON viva en un solo lugar; ambos formatos lo comparten.
- `engine/serialization/PrefabSerializer.{h,cpp}` con `save(Entity, name, AssetManager&, path)` y `load(path) -> optional<SavedPrefab>`.
- `AssetManager` extendido con `PrefabAssetId` (slot 0 = vacío fallback) + `loadPrefab` + `getPrefab` + `prefabPathOf` + `prefabCount`. Pattern espejo del catálogo de meshes.
- `PrefabLinkComponent` (string-only) marca a una entidad como instancia de un prefab. `EntitySerializer` lo lee/escribe como `prefab_path`.
- `EditorUI::request/consumeSavePrefabRequest` + ítem de menú `Archivo > Guardar como prefab…` (grayado sin selección; **NO** requiere proyecto activo: los prefabs son globales del repo).
- `AssetBrowserPanel` con sección "Prefabs" entre Meshes y Audio. Drag payload `MOOD_PREFAB_ASSET`. `ViewportPanel::PrefabDrop` + `consumePrefabDrop` análogos al flow de mesh.
- `EditorApplication::handlePrefabDrop` instancia copiando Transform/Mesh/Light/RigidBody (resolviendo paths a ids), agrega `PrefabLinkComponent` y selecciona la nueva entidad. Tag con sufijo numérico.
- `tryOpenProjectPath` reaplica `PrefabLinkComponent` cuando una `SavedEntity` viene con `prefabPath` no vacío al cargar el `.moodmap`.
- Schema bump: `k_MoodmapFormatVersion` 4 → 5. Backward-compat: archivos v4 sin `prefab_path` se siguen leyendo (campo opcional).
- Tests: `test_prefab_serializer.cpp` (5 casos round-trip + 2 de `AssetManager::loadPrefab`). Suite total **113 / 493** (antes 106 / 454).
- Sample asset: `assets/prefabs/sample_light.moodprefab` para validar el flow sin pasar por el dialog.

**Fix reactivo del smoke test (incluido en el hito):**
- `pfd::save_file` para prefabs no mostraba el diálogo en Windows si el `default_path` tenía forward slashes (de `generic_string()`) o caracteres inválidos en el filename (tags como `Mesh_meshes/pyramid.obj_1` con `/`). `handleNewProject` no tenía el bug porque pasa solo el directorio; el handler de prefab pasa `dir + filename`. Fix: sanitizar el filename + usar `string()` (separadores nativos) en lugar de `generic_string()`.

**Siguiente paso tras completarlo:** Hito 15 — Skybox, fog, post-procesado. Cubemap textures, fog exponencial/lineal, primer pase de post-procesado (tonemapping placeholder + bloom toggle). Plan en `docs/PLAN_HITO15.md`.

### Pendientes menores detectados en Hito 14

- **Propagación bidireccional prefab ↔ instancia** (revert / apply overrides). El link queda persistido pero la UX de "actualizar prefab desde instancia" o "reset overrides" no existe. **Trigger:** hito dedicado o cuando aparezca un workflow real iterativo sobre prefabs.
- **Nested prefabs** (un prefab que contiene otro prefab como entity hija). Bloqueado por la falta de `ParentComponent`/`ChildrenComponent` en el ECS. **Trigger:** post-Hito 14, junto con la jerarquía padre/hijo en `TransformComponent`.
- **Drop de prefab sobre entidad existente reemplaza componentes**. **Trigger:** workflow real "swap mesh by prefab" sobre N props.
- **Prefab variants** (Unity-style). **Trigger:** si la cantidad de prefabs casi-iguales se vuelve inmanejable.
- **Thumbnails 3D del prefab en el AssetBrowser**. Pendiente arrastrado desde Hito 10 (también pedido para meshes). **Trigger:** polish post-Hito 17 cuando los preview tengan más utilidad estética.
- **Lua bindings** (`scene.instantiate("prefabs/x.moodprefab")`). **Trigger:** primer script gameplay que necesite spawn dinámico.
- **Undo/Redo** del spawn de prefab. **Trigger:** Hito 22.

## Hito 15 — Skybox, fog y post-procesado

**Objetivo:** convertir el viewport en una escena 3D moderna. Skybox de cubemap detrás de todo, fog uniforme distance-based, render target HDR + post-process pass con tonemapping y gamma. Todo configurable en runtime via un `EnvironmentComponent` serializable.

**Criterios de aceptación cumplidos:**
- `tools/gen_sky_cubemap.py` genera 6 PNG de un cielo gradient en `assets/skyboxes/sky_day/`. Reproducible con Pillow.
- `OpenGLCubemapTexture` (Wrapper de `GL_TEXTURE_CUBE_MAP`) + `SkyboxRenderer` (cubo unitario con depth=1 truc). Render LEQUAL para que la escena escriba encima.
- `engine/render/Fog.h` (header-only): 4 modos (Off / Linear / Exp / Exp2), testeable sin GL. `lit.frag` extendido con uniforms + replica de la matemática.
- `OpenGLFramebuffer` con enum `Format {LDR, HDR}`. RGBA16F para el scene FB; RGBA8 para el LDR final que ImGui muestra.
- `PostProcessPass`: fullscreen triangle desde `gl_VertexID` (sin VBO). Aplica `2^exposure` + tonemap (None / Reinhard / ACES Narkowicz) + gamma 1/2.2.
- `EnvironmentComponent` (skybox / fog / exposure / tonemap) editable desde Inspector. `applyEnvironmentFromScene` resetea a defaults + aplica el primer Environment, llamado cada frame y al cargar proyecto.
- Schema bump: `k_MoodmapFormatVersion` 5 → 6. Backward-compat con archivos sin `environment`.
- Tests: `test_fog.cpp` (6 casos numéricos) + extensión de `test_scene_serializer.cpp` con round-trip de Environment. Suite total **120 / 530** (antes 113 / 493).

**Polish reactivo del hito (no en plan original):**
- Memoria nueva `feedback_no_autoopen_project.md`: el editor NUNCA auto-abre el último proyecto. `loadEditorState` ya no llama a `tryOpenProjectPath`. El Welcome modal aparece SIEMPRE al arrancar.
- Welcome modal con UX de gestión de recientes: botón "Limpiar inexistentes" + botón "X" por entrada + indicador rojo `(no existe)`.

**Siguiente paso tras completarlo:** Hito 16 — Shadow mapping. Shadow map de la directional light principal + sample en el lit shader. Plan en `docs/PLAN_HITO16.md`.

### Pendientes menores detectados en Hito 15

- **Catálogo de skyboxes** (per-proyecto). Hoy se carga `sky_day` fijo al iniciar; `EnvironmentComponent.skyboxPath` se persiste pero no se aplica. **Trigger:** ≥2 skyboxes en el repo.
- **Bloom** en post-process. **Trigger:** highlights HDR apagados sin él (PBR).
- **Anti-aliasing** (MSAA o FXAA). **Trigger:** jaggies visibles.
- **Skybox procedural** (atmospheric scattering). **Trigger:** demo de ciclo día/noche.
- **`ICubemapTexture` abstracta**. **Trigger:** segundo consumidor de cubemaps (IBL Hito 17).

## Hito 16 — Shadow mapping

**Objetivo:** sombras dinámicas para la directional light principal con shadow map depth-only + PCF 3×3 hardware. Toggle por luz vía `castShadows`, persistido en `.moodmap` y `.moodprefab`.

**Criterios de aceptación cumplidos:**
- `OpenGLShadowMap` (FBO depth-only 2048×2048, `GL_DEPTH_COMPONENT24`, `GL_COMPARE_REF_TO_TEXTURE` + `GL_LEQUAL`, border depth=1 para que muestras fuera del frustum sean "iluminadas").
- `shaders/shadow_depth.{vert,frag}` (vert con `uLightSpace * uModel * pos`, frag vacío).
- `ShadowPass` (recorre `Scene::forEach<Transform, MeshRenderer>` con front-face culling para reducir peter-panning).
- `lit.{vert,frag}` extendido: uniform `uLightSpace`, `vLightSpacePos` interpolado, `sampleShadow()` con PCF 3×3 sobre `sampler2DShadow` (36 muestras efectivas con hardware PCF de 4 taps).
- `LightComponent.castShadows` (solo Directional). Checkbox en el Inspector + round-trip en `.moodmap` (campo `cast_shadows` opcional, default false → archivos viejos compatibles sin bump de versión).
- `engine/render/ShadowMath.h`: helper puro (sin GL) con `computeShadowMatrices(lightDir, sceneCenter, sceneRadius)`. Refactor de `ShadowPass` para usarlo. Permite testear el cálculo de matrices.
- Tests nuevos: `test_shadow_proj.cpp` (5 casos: centro NDC, bounding sphere dentro del frustum, fallback de zero-dir, normalización idempotente, `chooseLightUp`). Extensión de `test_scene_serializer` con round-trip de `castShadows`. Suite total **125 / 580**.
- Demo "Ayuda > Agregar demo de sombras": piso plano 20×20 + columna 1×4×1 + sol oblicuo con `castShadows=true`. Smoke test visual confirmado.

**Bloque 0.5: refactor de `EditorApplication.cpp`** (preventivo ante crecimiento del archivo, pedido del dev). Pasó de **2011 → 1154 líneas (-43%)**:
- `editor/EditorRenderPass.cpp` — `renderSceneToViewport` + `applyEnvironmentFromScene`.
- `editor/EditorProjectActions.cpp` — handlers de Nuevo/Abrir/Guardar/Cerrar proyecto + `tryOpenProjectPath` + `loadEditorState`/`saveEditorState`.
- `editor/DemoSpawners.cpp` — handlers `processSpawn*Request` + drops del viewport.

**Polish reactivo del hito (no en plan original):**
- `k_defaultAmbient` bajado de 0.18 a 0.08: las sombras se notan recién con un ambient más bajo.
- Cubo cyan del hover de tile **solo aparece durante drag de asset** (textura/mesh/prefab). Antes se mostraba siempre que el cursor estuviera sobre la imagen — UX ruidosa.
- Iconos de luces estilo Blender en el viewport overlay: anillo + dots para Point, core + 8 rayos para Sun, con línea proyectada hacia donde apunta `direction`.
- Tecla `.` "frame selected" estilo Blender: re-centra el target de la `EditorCamera` y ajusta el radius para que la entidad seleccionada entre en cuadro. Mantiene yaw/pitch.
- Outline OBB naranja (12 aristas vía `OpenGLDebugRenderer`) para la entidad seleccionada con mesh — sigue rotación y escala.

**Siguiente paso tras completarlo:** Hito 17 — PBR (metallic-roughness workflow, IBL básico desde el cubemap del skybox). Plan en `docs/PLAN_HITO17.md`.

### Pendientes menores detectados en Hito 16

- **Cascaded Shadow Maps (CSM)** para escenas grandes. Hoy 1 sola cascada con bounding sphere fijo del mapa. **Trigger:** mundos > 50 m de extensión donde la resolución del shadow map se note pixelada.
- **Sombras de point lights** (cubemap depth, 6 caras). **Trigger:** demos con interiores donde la directional no alcanza.
- **Soft shadows / VSM / PCSS** para sombras con penumbra realista. **Trigger:** post-PBR.
- **Bias por geometría** (slope-scale bias) para reducir peter-panning sin front-face culling. **Trigger:** si front-face culling rompe sombras de geometría no-cerrada (planos, sprites).
- **AABB del MeshAsset** para outlines más precisos en meshes no centrados en `[-0.5, 0.5]³`. Hoy el outline asume cubo unitario. **Trigger:** primer mesh importado con bounds atípicos.

## Hito 17 — PBR (Physically Based Rendering)

**Objetivo:** reemplazar el shading Blinn-Phong por PBR metallic-roughness con IBL básico desde el cubemap del skybox. Materiales editables en runtime via Inspector + sliders, persistibles en `.material` JSON. Showcase 3×3 esferas (metallic × roughness) verificable en una sola toma.

**Criterios de aceptación cumplidos:**

*Bloque 1 — datos + serialización:*
- `engine/render/MaterialAsset.h`: 4 slots de textura (albedo, MR packed, normal, AO) + 4 escalares fallback (`albedoTint`, `metallic`, `roughness`, `ao`). Convención glTF para MR packed (R=AO, G=rough, B=metal).
- `AssetManager` extendido con catálogo `MaterialAssetId` (slot 0 = default mate dieléctrico) + `loadMaterial(.material JSON)` + `loadMaterialFromTexture` (wrapper auto para back-compat) + `createMaterial(prototype)` (helper runtime sin tocar disco).
- `MeshRendererComponent.materials`: `vector<TextureAssetId>` → `vector<MaterialAssetId>`.
- Bump `k_MoodmapFormatVersion` 6 → 7. Upgrader detecta extensión: `.material` se carga como material, otra cosa se trata como textura y se envuelve en wrapper.

*Bloque 2 — shaders:*
- `shaders/pbr.{vert,frag}` reemplazan a `lit.{vert,frag}`. Cook-Torrance con GGX (Trowbridge-Reitz) + Smith-Schlick + Fresnel-Schlick. Energy conservation explicita (`kS = F`, `kD = (1-kS)*(1-metallic)`).
- Shadow factor del Hito 16 multiplica solo el término directo del directional (no el IBL).

*Bloque 3 — IBL:*
- `tools/bake_brdf_lut.py`: tabla universal 256×256 (RG = scale/bias del split-sum de Karis), 1024 samples Hammersley/texel.
- `tools/bake_ibl.py`: irradiance 32×32 (cosine-weighted, 2048 samples) + prefilter mip chain 5 niveles (128→8) con GGX importance sampling. Lento offline (~94s), runtime cero.
- Assets pre-bakeados en `assets/ibl/sky_day/` y `assets/ibl/brdf_lut.png`.
- `OpenGLCubemapTexture` extendido con mip chain + flag `sRgb`. Los cubemaps de color (skybox + IBL) se cargan como `GL_SRGB8_ALPHA8` para que GL haga el decode automático y no haya doble-gamma con el post-process.
- IBL split-sum aplicado en `pbr.frag` (irradiance unit 4, prefilter unit 5, BRDF LUT unit 6). Fallback a `uAmbient` escalar si los assets no están.

*Bloque 4 — Inspector + UX:*
- InspectorPanel con sliders en vivo de `albedoTint` (ColorEdit3), `metallic`, `roughness`, `ao` por slot del MeshRenderer. Editar muta el `MaterialAsset` in-place; el render del frame siguiente lo ve.
- AssetBrowser nueva sección "Materiales" lista los `.material` JSON. Drag payload `MOOD_MATERIAL_ASSET`.
- ViewportPanel con `enum AssetDragKind { None, Texture, Mesh, Prefab, Material }` para distinguir highlights: cubo cyan (drag a tile) vs OBB amarillo (drag a entidad).
- Drop de material vía `pickEntity` reasigna el primer slot del MeshRenderer.
- 5 `.material` samples en `assets/materials/` (oro_pulido, cobre_rugoso, plastico_azul, acero_pulido, caucho_negro).

*Bloque 5 — demo + tests:*
- `createSphereMesh()` en PrimitiveMeshes (UV sphere, 32 segs). `AssetManager::primitiveSphereId()` reservado en el ctor.
- "Ayuda > Agregar esferas PBR de prueba" spawnea un grid 3×3: eje X = metallic (0.0/0.5/1.0), eje Y = roughness (0.05/0.5/1.0).
- `engine/render/PbrMath.h`: helpers puros C++ (Fresnel-Schlick, GGX, Smith) compartidos con el shader.
- Tests nuevos: `test_pbr_brdf.cpp` (8 casos: F0 dieléctrico vs metálico, Fresnel a 0/90°/intermedios, GGX max y monotonía, Smith en [0,1] y NdotL=0). `test_material_serializer.cpp` (7 casos: round-trip, defaults faltantes, cache, archivo inexistente, JSON inválido, paths de textura, createMaterial slots distintos). Suite total **144 / 649**.

**Polish reactivo del hito:**
- `fix(editor): "Nuevo Proyecto" crea su propia carpeta` — antes contaminaba el directorio padre con `maps/` y `textures/` sueltas; ahora `<dir>/<name>/` con todo adentro (Unity/Godot convention).
- LightSystem deja de subir `uSpecularStrength`/`uShininess` (eran del Blinn-Phong, ya no aplican).

**Siguiente paso tras completarlo:** Hito 18 — Deferred / Forward+. Plan en `docs/PLAN_HITO18.md`.

### Pendientes menores detectados en Hito 17

- **HDR cubemaps reales** (`.hdr` Radiance RGBE). Hoy los cubemaps son PNG 8-bit LDR — los reflejos especulares no tienen rango dinámico real. **Trigger:** cuando aparezca un skybox con highlights >1.0 que ameriten el rango.
- **Normal mapping**: el shader `pbr.frag` no samplea el slot `normal` aún (los meshes actuales no traen tangentes). **Trigger:** primer mesh importado con normal map, o agregar tangentes a `createCubeMesh`/`createSphereMesh`.
- **AO sample real desde MR packed (canal R)**: hoy el shader respeta solo `uAoMap` separado y `uAoMult` escalar. **Trigger:** materiales glTF importados con AO en el R del MR packed.
- **Renderer de equirectangular → cubemap** para que el dev pueda dropear un `.hdr` panorámico y se convierta en cubemap automáticamente. **Trigger:** integración con HDR sources externos.
- **Editor de grafo de materiales** (substance-style). **Trigger:** Hito 23 (es un hito dedicado).
- **Multi-submesh material assignment desde drag**: hoy el drop de material asigna SOLO al primer slot. **Trigger:** primer mesh con submeshes que necesite materiales distintos.

## Hito 18 — Forward+ (renderer escalable a muchas luces)

**Objetivo:** subir el cap de point lights por escena de 8 (uniform array) a 256+ (SSBO) y cullear por tile en CPU para que el shader procese SOLO las luces que afectan a cada fragment. Migra el path de iluminación del PBR del forward simple del Hito 17 a Forward+ tile-based.

**Decisión Forward+ vs Deferred:** Forward+. Mantiene compatibilidad con el HDR + post-process actual, no requiere G-buffer extra de memoria y funciona bien con MSAA cuando aparezca. Deferred queda como Hito 18.5 si Forward+ no escala (no esperado para los tamaños de escena del proyecto).

**Criterios de aceptación cumplidos:**

*Bloque 1 — LightGrid CPU:*
- `engine/render/LightGrid.h/.cpp`: tile size 16×16, AABB del bounding sphere de cada luz proyectado a screen-space (8 esquinas), prefix-sum de counts → flat array de indices. CPU-only, testeable.
- `projectSphereToTileRange()` helper puro maneja casos: luz delante (caso normal), luz detrás (descarta), luz parcial cruzando la cámara (marca todo el viewport).
- Convención de coords matchea `gl_FragCoord` (origen abajo-izquierda).

*Bloque 2 — GPU upload via SSBO:*
- `engine/render/opengl/OpenGLSSBO.h/.cpp`: wrapper RAII de `GL_SHADER_STORAGE_BUFFER`. `upload()` reusa storage si capacidad alcanza (`glBufferSubData`); crece con `glBufferData GL_DYNAMIC_DRAW`.
- 3 SSBOs por frame: PointLight (binding 2), TileLightList (binding 3), uint indices (binding 4). Layout std430 con padding explícito en C++ (`PointLightStd430` 48 bytes, matchea el `struct PointLight` de GLSL).

*Bloque 3 — Shader + integración:*
- `pbr.frag` reemplaza el `uniform PointLight uPointLights[8]` por `layout(std430, binding=2) buffer PointLightBuffer`. Loop por tile: `gl_FragCoord / 16` → tileIdx → lookup en `uTiles[]` → loop sobre `uLightIndices[offset, offset+count)`.
- `LightSystem::bindUniforms` deja de subir `uPointLights[].position`/etc. via `glUniform`; el bindeo lo hace `EditorRenderPass` con los SSBOs.
- `k_MaxPointLights` 8 → 256 (sigue habiendo cap por seguridad pero es 32× más alto).

*Bloque 4 — Demo + tests:*
- "Ayuda > Agregar stress test 64 luces" spawnea grid 8×8 de point lights con colores procedurales HSV (hue rotando, saturation 0.85, value 1.0). Cobertura ~17.5×17.5 m, intensidad 1.2, radius 3.5 m.
- `tests/test_light_grid.cpp` (9 casos): luz centrada, luz detrás de cámara, luz lateral fuera de frustum, luz que cruza la cámara, dimensiones de tile correctas, tile no-vacio cuando luz visible, offsets prefix-sum válidos, viewport 0×0 no crashea, luces fuera del frustum no aparecen en el flat array. Suite total **162 / 5109**.

**Polish reactivo del hito:**
- **`EnvironmentComponent.iblIntensity`** (slider [0..2], default 1.0) en el Inspector. Persistido en `.moodmap` solo si != 1.0 (no ensucia archivos viejos). Resuelve el caso "el cubemap es muy claro y ahoga las point lights" — útil cuando el dev quiere que las luces directas dominen sobre el ambient IBL.
- Log diagnóstico one-shot del LightGrid (`LightGrid: N point lights -> M tiles, K no-vacios, T asignaciones`) cuando cambia la cantidad de luces. Diagnóstico de regresión sin spamear el log cada frame.

**Siguiente paso tras completarlo:** Hito 19 — Animación esquelética. Plan en `docs/PLAN_HITO19.md`.

### Pendientes menores detectados en Hito 18

- **Compute shader culling** del LightGrid (mover de CPU a GPU). Hoy CPU-side es ≤1ms con 64 luces; ~5ms con 256 luces empieza a doler. **Trigger:** demo con >100 luces o menos cap se vuelve aceptable bajo profile.
- **Cluster por profundidad (Forward++ / Z-bins)**: en escenas con luces apiladas en profundidad, todas caen en los mismos tiles XY pero a distancias muy distintas. **Trigger:** primer artista que arme una escena con muchas luces dispuestas verticalmente.
- **AABB tighter para light culling**: hoy se proyectan 8 esquinas del AABB world-space del bounding sphere. La sphere proyectada es una elipse, no un AABB; eso da false positives en grazing. **Trigger:** profile que muestre 2-3× más asignaciones de las esperadas.
- **MSAA dentro del scene FB HDR**: Forward+ lo soporta trivialmente (deferred no). **Trigger:** jaggies visibles en geometría angular.
- **Sombras de point lights** (cubemap depth, 6 caras por luz). Sigue del Hito 16. **Trigger:** primera escena interior con un solo point light que necesite sombra.

## Hito 19 — Animación esquelética

**Objetivo:** cargar y reproducir animaciones desde `.glb`/`.gltf` (assimp ya integrado desde Hito 10). Skinning lineal (LBS) 4 huesos por vértice, interpolación lineal entre keyframes, un único clip activo por entidad.

**Criterios de aceptación cumplidos:**

*Bloque 1 — datos puros:*
- `engine/animation/Skeleton.h`: `Bone { name, parent, inverseBind, localBindTransform }`. Helper libre `computeSkinningMatrices(skel, localPose, out)` (O(N) sin recursión por garantía de orden topológico). Constante `k_maxBonesPerSkeleton = 128` matchea el shader.
- `engine/animation/AnimationClip.h`: `BoneTrack` con keyframes pos/rot/scale. Sample con clamp al primer/último key + LERP interno. Quaternions con sign-flip si `dot(q0,q1) < 0` (evita el "long way around"). `evaluate(t, skel, out)` cae al `localBindTransform` para huesos sin track.
- `MeshAsset` extendido con `optional<Skeleton>` + `vector<AnimationClip>` + `aabbMin/aabbMax` del bind pose.

*Bloque 0+2 — carga assimp:*
- `MeshLoader.cpp` reescrito: stride 19 (pos+color+uv+normal+boneIds+boneWeights). Construye un único `Skeleton` por `MeshAsset` aglutinando `aiBone` de todos los `aiMesh` con topo sort por jerarquía de `aiNode`. Top-4 influencias por vértice renormalizadas. Parsea `aiAnimation` → `AnimationClip` con conversión ticks → segundos. Calcula AABB acumulando min/max de positions.
- Flag `aiProcess_LimitBoneWeights` agregado al import.

*Bloque 3 — shader:*
- `shaders/pbr_skinned.vert`: LBS 4 huesos con `uniform mat4 uBoneMatrices[128]`. Reusa `pbr.frag` (recibe `vWorldPos`/`vWorldNormal` ya skinneados). Defensivo: si `sum(weights) < 1e-4` cae al pipeline no-skinneado (mesh sin esqueleto compartiendo el shader).
- Switch en `EditorRenderPass`: dos pases. Pase A estático con `m_pbrShader` skipea entidades con `SkeletonComponent`. Pase B skin bindea `m_pbrSkinnedShader` solo si hay alguna entidad skinneada y sube `uBoneMatrices[i]` por entidad. Lambda `applyShaderUniforms` evita duplicar setup.

*Bloque 4 — sistema + componentes:*
- `AnimatorComponent { clipName, time, speed, playing, loop }` y `SkeletonComponent { skinningMatrices }` en `Components.h`.
- `systems/AnimationSystem.{h,cpp}`: avanza time con loop/clamp, evalúa el clip activo, compone matrices skinning. Entrada en `run()` paso 3.55 (entre Script y Audio).
- InspectorPanel: combo de clips poblado desde el MeshAsset, sliders speed, checkboxes playing/loop, botón Reset.

*Bloque 5 — demo + tests:*
- `assets/meshes/Fox.glb` (162 KB, CC0, glTF Sample Assets de Khronos). 3 clips: Survey/Walk/Run.
- "Ayuda > Agregar personaje animado" spawnea Fox con escala 0.01.
- `tests/test_animation.cpp`: 12 casos. Suite total **165 / 5172** (antes Hito 18 cerrado: 153 / 5109).

**Siguiente paso tras completarlo:** Hito 20 — UI del juego con RmlUi. Plan en `docs/PLAN_HITO20.md`.

### Pendientes menores detectados en Hito 19

- **Persistencia del `AnimatorComponent`/`SkeletonComponent` en `.moodmap`/`.moodprefab`**: hoy no se serializan. El esqueleto vive en el MeshAsset (siempre lo recarga el AssetManager) pero el clip activo y el time del Animator se pierden al guardar. **Trigger:** primer dev que se queje de "abrir el proyecto y la animación arranca de cero" o que necesite un setup específico de animación al cargar.
- **SLERP en quaternions**: hoy LERP normalizado. Ok para keyframes densos (Mixamo, glTF Sample); con keyframes muy separados puede dar twisting visible. **Trigger:** animación con artifacts notorios.
- **Blend de animaciones / state machines**: un único clip activo por entidad. Sin blend ni transiciones suaves. **Trigger:** Hito dedicado (post-20).
- **Compute skinning en GPU** (transform feedback / compute shader): hoy las matrices se componen en CPU (`AnimationSystem`) y se suben como uniforms. Para >5 personajes empezaría a notarse. **Trigger:** profile con muchos personajes.
- **Inverse Kinematics**: cero IK. Personajes pisan literalmente lo que dice el clip, sin foot planting ni grasping de objetos. **Trigger:** demo de gameplay que lo necesite.
- **Vertex Animation Textures** (alternativa a skinning para crowds): si aparece la necesidad de cientos de personajes animados. **Trigger:** RPG con NPCs masivos.
- **AnimationEvent** (callbacks por timeline — "footstep en t=0.3"): permitiría disparar audio / partículas en frames específicos. **Trigger:** primer feature de gameplay que lo necesite.
- **Persistencia del path del `.glb` con esqueleto en `.moodmap`**: hoy `MeshRendererComponent` solo guarda el path del mesh; el esqueleto y los clips se reconstruyen al cargar el MeshAsset. Aceptable mientras un mesh tenga UN esqueleto canónico.

---

## Hito 20 — UI del juego (HUD + menú de pausa)

**Objetivo:** una capa de UI in-game distinguible de la UI del editor (Dear ImGui). HUD con HP/Ammo/crosshair visibles solo en Play Mode, menú de pausa con Esc, y bindings desde Lua para que los scripts puedan mutar el HUD.

**Cambio de plan a mitad del hito:** el plan original era integrar **RmlUi** (HTML/CSS-like) como librería separada. Tras Bloques 1-3 funcionando, el layout responsive de RmlUi mostró bugs persistentes (vw/vh, dp, percentage clamps — el panel no escalaba consistentemente con el viewport). Se decidió abandonar RmlUi y reemplazarlo con **drawlist de Dear ImGui** sobre el callback `OverlayDraw` que ya tenía `ViewportPanel` para el editor. Trade-off: perder el lenguaje declarativo HTML/CSS, ganar simplicidad y un solo motor de UI. Documentado en `DECISIONS.md`. Para casos donde un día se quiera HTML/CSS (menús complejos con animaciones de transición), se puede reintroducir como un layer adicional sin romper el actual.

**Criterios de aceptación cumplidos:**

*Bloque 4 — HUD + pausa via ImGui drawlist:*
- HUD en Play Mode: bloques HP / AMMO + crosshair (cruz con outline) — `EditorApplication::drawGameOverlay`. Posición adaptativa al tamaño del viewport (esquinas con padding fijo de 24 px).
- Menú de pausa: overlay oscuro 50% + panel centrado 50% del ancho con clamp [320, 520] px + 3 botones (Continuar / Opciones / Salir al editor). Hit-testing con `ImGui::IsMouseHoveringRect` + `IsMouseClicked`.
- Esc togglea pausa; `updateCameras` early-return cuando `paused=true` para congelar gameplay.
- Borrado completo de `RmlUi::Core` + FreeType + assets `.rml`/`.rcss` + shaders `ui.{vert,frag}` + `src/engine/ui/{RmlRenderer,RmlSystem,UiLayer}.{h,cpp}` (~1000 líneas).
- Bug fix encontrado en el smoke test: `ViewportPanel` resetea `m_imageHovered=false` al inicio del frame y solo lo setea DESPUÉS de invocar el overlay callback. El check de hover de los botones que combinaba `IsMouseHoveringRect && imageHovered()` daba siempre false. Removido el `&& imageHovered()` — `IsMouseHoveringRect` solo es suficiente porque el clip rect del viewport ya limita el área.

*Refactor: partición de `EditorApplication.cpp`:*
- `EditorApplication.cpp` había crecido a 1514 líneas (lifecycle + ctor con lambda gigante + scene methods + play mode methods + overlay del juego). Repartido en 4 archivos parciales (~500 líneas/cada uno) que comparten el mismo header — convención ya usada en este módulo (`DemoSpawners.cpp`, `EditorProjectActions.cpp`, `EditorRenderPass.cpp`):
  - `EditorApplication.cpp` (652 líneas): ctor + dtor + `processEvents` + `run()` + `beginFrame/endFrame` + `mapWorldOrigin` + `markDirty` + `updateWindowTitle`.
  - `EditorOverlay.cpp` (516 líneas, nuevo): `drawEditorOverlay` con iconos (Light/Audio), halo de selección, gizmo translate/rotate/scale, hotkeys W/E/R/Period.
  - `EditorPlayMode.cpp` (212 líneas, nuevo): `enterPlayMode/exitPlayMode/updateCameras/drawGameOverlay`.
  - `EditorScene.cpp` (192 líneas, nuevo): `buildInitialTestMap`, `rebuildSceneFromMap`, `updateRigidBodies`, `updateTileEntity`, `deleteSelectedEntity`.
- Soft target: ~500 líneas/`.cpp`, hard cap ~800.

*Bloque 5 — bindings Lua + GameState:*
- `engine/game/GameState.{h,cpp}` (nuevo): singleton-namespace con `HudState& hud()`, `bool& paused()`, `reset()`. Estado proceso-global accesible desde C++ y desde Lua.
- Tabla `hud` registrada en `LuaBindings.cpp` con setters `setHp/setAmmo/setPaused` + getters `getHp/getAmmo/getPaused`. Cualquier script con `ScriptComponent` puede mutar el HUD del juego.
- `EditorApplication` migrado a leer/escribir `GameState::hud()` y `GameState::paused()` (eliminados los miembros propios `m_hud`/`m_paused`).
- `exitPlayMode` llama `GameState::reset()` para volver a defaults al salir de Play.
- `assets/scripts/hud_demo.lua` (nuevo): script demo que setea HP=75/Ammo=12 al entrar a Play, drena 1 HP/s y al llegar a 0 fuerza pausa via `hud.setPaused(true)`. Spawneable desde "Ayuda > Agregar HUD demo".
- Bug fix detectado en smoke test: cuando un script Lua flippeaba `paused=true`, el menú aparecía pero el cursor seguía oculto (relative mouse mode). Fix centralizado en `updateCameras`: detecta la transición del flag y llama a `SDL_SetRelativeMouseMode` en un único lugar — así Lua, Esc y el botón "Continuar" pasan por el mismo camino. El handler de Esc y el botón "Continuar" pasan a solo flippear el flag.

*Bonus fix — Delete vía SDL:*
- La tecla `Delete/Backspace` para borrar entidad seleccionada se procesaba con `ImGui::IsKeyPressed` dentro del overlay callback. Dependía del foco del panel y a veces no disparaba. Migrado a evento SDL en `processEvents` (igual que Esc / F1 / Ctrl+S). La lógica de borrado (filtro de tiles + cleanup de Jolt body + destroy + log + markDirty) se extrajo al método público `deleteSelectedEntity` en `EditorScene.cpp`.

*Bloque 6 — tests:*
- `tests/test_game_state.cpp`: 6 casos cubriendo defaults, mutación C++, reset, hud.setHp/setAmmo/setPaused desde Lua, hud.getHp/getAmmo/getPaused desde Lua, dos scripts compartiendo el mismo singleton.
- Fix incidental en `tests/test_lighting.cpp`: el mock `RecordingShader` no overrideaba `setVec2` (agregado a `IShader` en Bloque 4 al integrar lo que iba a ser RmlUi y quedó). Sin esto `mood_tests` no compilaba.
- Suite total **171 / 5188** (antes Hito 19 cerrado: 165 / 5172).

**Siguiente paso tras completarlo:** Hito 21 — Empaquetado standalone (build distribuible del juego sin editor). Plan en `docs/PLAN_HITO21.md`.

### Pendientes menores detectados en Hito 20

- **Botón "Opciones" del menú de pausa**: hoy solo loguea `aún no implementado`. **Trigger:** cuando haya un sistema de settings persistido (volumen, resolución, control mapping).
- **Animaciones de transición del menú de pausa** (fade, slide): hoy aparece/desaparece instantáneo. **Trigger:** primera versión "vendible" donde la presentación importe; o cuando se quiera reintroducir RmlUi para ese caso específico.
- **Persistencia de HUD entre sesiones**: hoy `GameState::reset()` al `exitPlayMode` lo lleva a defaults; los scripts setean valores cada vez. **Trigger:** primera vez que un dev quiera "save game" con HP/Ammo persistidos.
- **HUD escalable a más de HP/Ammo**: hoy 2 bloques fijos. Si crece a 5+ stats vale la pena hacer el HUD data-driven (lista de bloques desde Lua). **Trigger:** primer juego que necesite +3 stats simultáneos.
- **Tests del flujo Esc → menú → click "Continuar"**: hoy verificado solo con smoke test manual. ImGui no es trivialmente testable en headless, pero `GameState::paused()` y `deleteSelectedEntity` (extraído justo para esto) sí podrían tener tests headless. **Trigger:** primer regression bug del menú.

---

## Hito 21 — Empaquetado standalone (MoodPlayer + PackageBuilder)

**Objetivo:** producir un build distribuible del juego (binario + assets) ejecutable por un usuario final sin Visual Studio. Hasta este hito todo vivía como `MoodEditor.exe`; ahora el editor puede generar un paquete autocontenido con un `MoodPlayer.exe` separado que carga el proyecto indicado en un `game.json`.

**Criterios de aceptación cumplidos:**

*Bloque 1 — scaffold MoodPlayer:*
- Nuevo target `MoodPlayer.exe` con `src/player/PlayerApplication.{h,cpp}` + `src/player/main.cpp`. Esqueleto mínimo: ventana SDL 1280x720 + GL 4.5 + ImGui inicializado (sin paneles, `io.IniFilename = nullptr`), Esc cierra. Sin escena todavía.
- Listas de fuentes duplicadas en CMake; preparó la base para el refactor del Bloque 3.

*Bloque 2 — `SceneRenderer` extraído:*
- Pipeline de render (FBs HDR/LDR, shaders PBR estático + skinneado, skybox, shadow pass, post-process, IBL, light grid + 3 SSBOs, debug renderer, environment cache, last view/projection) movido de `EditorApplication` a `engine/render/SceneRenderer`. Editor pasó de ~25 miembros de render a uno solo (`unique_ptr<SceneRenderer>`).
- API por frame: caller pasa scene + assets + view + projection + aspect + cameraPos + tamaño del panel a `renderScene(...)`. SceneRenderer pinta sky + escena + lit al scene FB pero deja el FB bindeado para que el caller agregue debug 3D propio via `debugRenderer()`. Cuando llama a `endFrame()`, flushea el debug y aplica post-process al viewport FB.
- `EditorRenderPass.cpp` queda como orquestador thin (camera dispatch + overlay 3D editor-side). Helper `viewportAspect()` unifica los 5 callsites con la fórmula `m_viewportFb->w/h`.
- Cleanup incidental: `m_defaultShader` eliminado (dead code desde Hito 17).

*Bonus fix arrastrado del Hito 19:* drop de mesh al viewport ahora aterriza al ras del piso (y=0) calculando `yFloorOffset = -autoScale * aabbMin.y`. Antes con autoScale del Hito 19 el zorro flotaba 1.5m sobre el piso (la celda center y).

*Bloque 3 — PlayerApplication completo:*
- Fase A: PlayerApplication carga un mapa de prueba placeholder (sala 8x8 con grid+brick) y lo renderiza con SceneRenderer + FpsCamera. WASD + mouse para moverse; las paredes bloquean por moveAndSlide. Esc cierra.
- Fase B: agregados ScriptSystem + AudioDevice + AudioSystem + AnimationSystem + PhysicsWorld al loop. Tiles del placeholder ganan RigidBodyComponent::Static.
- HUD del juego + menú de pausa extraídos a `engine/game/GameOverlay::draw(...)` como free function compartida entre editor y player. Parametrizada por `exitButtonLabel` + `onExitRequested` callback (editor → `exitPlayMode()`, player → `m_running = false`).
- Esc togglea `GameState::paused()`. Sync de cursor con `SDL_SetRelativeMouseMode` centralizado en `updateCamera` detectando la transición del flag.
- Refactor de CMake: extraída `mood_engine_lib` (static lib) con todo `core/`/`platform/`/`engine/`/`systems/`. Editor y player linkean contra ella y agregan solo sus `.cpp` específicos. Dependencias del motor (SDL2, glm, EnTT, Lua, Jolt, etc.) son `PUBLIC` — los consumidores las heredan.

*Bloque 4 — `game.json` + carga del proyecto:*
- `engine/game/GameManifest`: schema v1 (`version`, `name`, `project`, `default_map`).
- `PlayerApplication::tryLoadGameManifest`: lee `<exe_dir>/game.json` (vía `SDL_GetBasePath()`, no working dir), resuelve el `.moodproj` relativo al manifest, llama `ProjectSerializer::load` + `SceneSerializer::load`, aplica al scene via `SceneLoader::applyEntitiesToScene`. Si cualquier paso falla, fallback al mapa de prueba.
- `engine/serialization/SceneLoader::applyEntitiesToScene` (nuevo): extracción de las ~85 líneas que aplican mesh/material upgrader v6/v7 + light + rigidbody + environment + prefab link de un `SavedMap` a una Scene. Editor (`tryOpenProjectPath`) y player la comparten.
- Tras cargar, ctor pide a SceneRenderer `applyEnvironmentFromScene(*scene)` para que la primera frame ya muestre fog/exposure/tonemap correctos.

*Bloque 5 — `PackageBuilder` + acción "Empaquetar proyecto":*
- `engine/packaging/PackageBuilder::build`: dado un Project + destDir + engineExeDir, copia MoodPlayer.exe + SDL2*.dll + `assets/` + `shaders/` + el directorio del proyecto a `<destDir>/<projectName>/project/` y genera `game.json`.
- V1 simple: copia `assets/` y `shaders/` enteras (sin filtrar por refs). Más grande de lo necesario pero predecible.
- Safety check: refuse si `destDir` está adentro del project root (o viceversa) — sin esto, elegir como destino la carpeta del proyecto provoca recursión infinita en `std::filesystem::copy`. Comparación con `weakly_canonical` + prefix match con separador.
- Item nuevo en `Archivo > Empaquetar proyecto...`. Si hay cambios sin guardar ofrece guardar primero. Abre `pfd::select_folder`, llama al packager, muestra MessageBox con resultado. `SDL_GetBasePath()` resuelve dónde vive `MoodPlayer.exe`. `#ifdef NDEBUG` distingue Debug (`SDL2d.dll`) de Release (`SDL2.dll`).
- Smoke test: paquete con 88 archivos + game.json válido. Doble-click en `MoodPlayer.exe` empaquetado abre la sala con Fox + pyramid persistidos, Esc abre el menú de pausa, "Salir del juego" cierra limpio.

*Bloque 6 — tests:*
- `tests/test_package_builder.cpp`: 8 casos headless. Arman engineExeDir + project mock en directorios temporales y verifican el output. Cubren build feliz (layout completo + game.json válido), recursión bloqueada (dest dentro del project, dest = project root), engineExeDir inexistente, shaders/ faltante (error fatal), assets/ faltante (warning, no error), project name vacío, isDebug=false copia SDL2.dll en lugar de SDL2d.dll.
- Suite total **179/5221** (antes Hito 20 cerrado: 171/5188).

**Siguiente paso tras completarlo:** Hito 22 (TBD). Plan en `docs/PLAN_HITO22.md` con candidatos.

### Pendientes menores detectados en Hito 21

- **Filtrado de assets en el packager**: hoy V1 copia `assets/` entero (~5-10 MB de overhead típico). Para juegos con muchas texturas/mallas/audios sin usar, walker que indexe refs reales de scripts + serializadores ahorraría peso. **Trigger:** primer dev que reporte "el paquete pesa X MB y solo uso Y%".
- **Build Release del player**: el packager soporta `isDebug=false` y copia `SDL2.dll`, pero el flujo actual del editor solo compila Debug. Falta agregar un preset `windows-msvc-release` y que `Empaquetar proyecto...` ofrezca elegir Debug/Release. **Trigger:** primer paquete que vaya a manos de usuarios finales.
- **`mood_engine_lib` como lib compartida**: hoy es `STATIC`. Si dos targets (editor + player) crecen mucho, podría tener sentido `SHARED` para dedup en disco. **Trigger:** tamaño de binarios que moleste.
- **Persistencia del Animator (clip activo + time)**: pendiente arrastrado del Hito 19. Empaquetar un proyecto con un Fox.glb funciona — pero el clip activo y el time se resetean al abrir el player. Sin esto, no se puede congelar una pose o un punto específico de animación al cargar. **Trigger:** primer juego donde el setup inicial de animación importa al cargar.
- **Botón "Opciones" del menú de pausa**: arrastra del Hito 20. Sin un sistema de settings persistido el botón loguea "no implementado".
- **Test E2E del flujo completo**: hoy `test_package_builder` cubre la lógica del packager pura, pero no se prueba el ciclo "editor → empaquetar → ejecutar player → cargar proyecto". **Trigger:** primer regression del flujo end-to-end (ej. game.json schema cambia y el player no lo lee).
- **CI build**: el smoke test corre solo localmente. Un GitHub Action que compile editor + player + corra mood_tests sería el sello automático antes de mergear. **Trigger:** primera vez que un PR rompe `main`.

---

## Hito 22 — Workflow de scripting Lua (UX en el editor)

**Objetivo:** cerrar el gap UX que tenía el motor entre "Lua corre" (Hito 8) y "el dev escribe scripts cómodo". La infra estaba: `ScriptComponent`, hot-reload por mtime, bindings, Inspector con InputText. Lo que faltaba: scripts en el Asset Browser, drop a entidades, "Nuevo Script" desde menú, doc panel.

**Criterios de aceptación cumplidos:**

*Bloque 1 — Scripts en Asset Browser:*
- `AssetBrowserPanel` escanea `assets/scripts/*.lua` con metadata de line count. Sección colapsable nueva entre Materiales y Audio. Drag source con payload `MOOD_SCRIPT_ASSET` (string buffer 256 bytes con el path lógico — los scripts no pasan por AssetManager, los carga ScriptSystem on-demand).

*Bloque 2 — Drop al viewport:*
- `ViewportPanel::AssetDragKind::Script` + `consumeScriptDrop()` con NDC del cursor + path del script. `BeginDragDropTarget` acepta el nuevo payload.
- `EditorApplication::processViewportScriptDrop`: pickEntity → si la entidad ya tiene `ScriptComponent` reemplaza el path + fuerza `loaded=false` (recarga vía mtime); si no, `addComponent`. Path persistido como `assets/<path_logico>` para mantener consistencia con los demos manualmente spawneados.
- Highlight visual: extiende el flow de Material drop — `dragKind == Script` también activa el OBB amarillo sobre la entidad bajo el cursor.

*Bloque 3 — "Nuevo Script..." en menú:*
- `Archivo > Nuevo Script...` abre `pfd::save_file` con default name + filtro `*.lua`. Si el usuario elige un path fuera de `assets/scripts/`, redirige el archivo a esa carpeta (sino el AssetManager no lo encuentra).
- Prompt para sobreescribir si existe.
- Template mínimo: comentario con bindings disponibles + `function onUpdate(self, dt) end`.
- `assetBrowser().rescan()` post-create — el script aparece sin reabrir editor.

*Bloque 4 — Panel "Lua API":*
- Nuevo `LuaApiPanel` registrado en `EditorUI::m_panels`. Tabs `self`/`hud`/`log`/`Lifecycle` con signature en color cian + descripción + snippet de uso en gris. Lista hardcoded — sumar manual al agregar bindings nuevos.

*Bloque 5 (stretch) — Mini editor in-place:*
- **Deferido al Hito 23.** Los 4 bloques anteriores ya entregan el grueso del workflow; el dev sigue editando en VS Code y el hot-reload del ScriptSystem levanta los cambios al guardar. `ImGuiColorTextEdit` como nueva dep CPM se evalúa cuando se retome.

*Bonus fix arrastrado del Hito 19:*
- `SceneLoader::applyEntitiesToScene` detecta meshes con esqueleto + clips y auto-agrega `AnimatorComponent` + `SkeletonComponent` con defaults razonables (clipName="" → primer clip, playing=true, loop=true). Sin esto, un Fox.glb guardado en `.moodmap` reaparecía en bind pose porque esos componentes no están en el schema del `SavedEntity`. Editor + MoodPlayer (que reusa `SceneLoader`) heredan el fix.

*Bloque 6 — tests:*
- `tests/test_scene_loader.cpp`: 3 casos headless. Mesh con skeleton gana Animator/Skeleton; mesh sin skeleton no; back-compat de paths v6 (texturas en lugar de `.material`) los envuelve en wrapper sin caer al missing.
- Suite total **182/5234** (antes Hito 21 cerrado: 179/5221).

**Siguiente paso tras completarlo:** Hito 23 (TBD). Plan en `docs/PLAN_HITO23.md` con candidatos. Si se sigue scripting, retomar el mini editor in-place.

### Pendientes menores detectados en Hito 22

- **Mini editor de código in-place**: deferido como Bloque 5 stretch. Punto de arranque natural si el Hito 23 sigue scripting. Necesita evaluar `ImGuiColorTextEdit` (license, tamaño de la dep CPM).
- **Lista del LuaApiPanel hardcoded**: sumar manualmente al agregar bindings nuevos. Si llegan a 50+, vale la pena introspección automática vía sol2 (no trivial — sol2 no expone metadata estructurada). **Trigger:** primer dev que reporta "el panel quedó desactualizado".
- **Drop a Hierarchy items**: hoy solo Viewport. Si el usuario quiere asignar script a una Light/Audio sin mesh visible, hoy tiene que clickearla y editarla en el Inspector. **Trigger:** primer pedido específico.
- **Persistencia opcional del Animator (clipName / time)**: el auto-add usa defaults. Si el dev necesita clipName="Walk" guardado por entidad, hay que extender `SavedEntity` con un campo opcional `animator: { clipName, playing, loop }`.
- **"Nuevo Script" abre el editor externo del usuario**: hoy crea el archivo y deja al dev hacer alt-tab. Una mejora UX sería invocar `start <path>` (Windows) tras crear, para que el editor default lo abra automáticamente. **Trigger:** primer usuario que diga "tuve que ir a buscar el archivo".

---

## Hito 23 — AI / pathfinding básico

**Objetivo:** primer eslabón de "el motor puede sostener un juego". Hasta el Hito 22 había render + scripts + física + packaging — pero nada que el jugador pueda enfrentar. Este hito agrega A\* sobre GridMap, `NavAgentComponent` + `NavSystem`, y un demo de enemigo que persigue al jugador. Sin esto, todo lo anterior era plumbing — con un enemigo que persigue, el motor recién muestra que puede hacer un juego.

**Criterios de aceptación cumplidos:**

*Bloque 1 — A\* sobre GridMap:*
- `engine/world/Pathfinding::findPath(map, start, goal)` — función pura. 4-connected (sin diagonales) + heurística Manhattan. Retorna vector de TileCoord; vacío si no hay path. Sin corner-cutting drama.
- 6 tests headless: sala vacía 8x8, muro al medio (bordea), sin path posible, start==goal, fuera del grid, goal sólido.

*Bloque 2 — NavAgent + NavSystem:*
- `NavAgentComponent { target, speed, active, path, pathIndex, repathAccumulator, lastPathTarget }`. Estado interno (path / accumulators) NO se persiste — runtime-only.
- `systems/NavSystem`: por frame, recompute path con throttle 0.5s o si target se aleja > 1m del último path. Steering 4-conn en plano XZ (Y constante). Colisión vía `moveAndSlide` igual que el player. Avanza waypoint cuando dist < tileSize/2. Cuando llega al final, queda quieto.
- Cableado en EditorApplication: corre solo en Play Mode. Antes del update, se settea `nav.target = m_playCamera.position()` para que enemigos persigan al jugador.

*Bloque 3 — Demo enemigo + fix de orientación general:*
- "Ayuda > Agregar enemigo demo" spawnea un CesiumMan con NavAgent + Animator (clip default playing). Sin RigidBody — NavSystem ya hace moveAndSlide.
- **Fix arrastrado del Hito 19:** `MeshAsset::importRotationEuler` extraído del `mTransformation` del rootNode de assimp. glTF Y-up nativos (Fox) traen `(0,0,0)`; Z-up convertidos por glTF (CesiumMan, BrainStem, etc.) traen `(-90, 0, 0)`. `processViewportMeshDrop` y `processSpawnEnemyDemoRequest` aplican esta rotación al spawn — cualquier `.glb` arrastrado al viewport queda parado, sin hardcodear por modelo.
- Helper `rotatedAabbWorldY(aabbMin, aabbMax, eulerDeg)` calcula el rango world-Y del AABB después de aplicar la rotación de import. Usado para autoscale (altura visual real, no axis crudo) + floor offset (anclar pies a y=0).
- Asset nuevo: `assets/meshes/CesiumMan.glb` (438 KB, CC0 de Khronos glTF Sample Assets, humanoide bipédo con clip de caminata).

*Bloque 4 — F1 debug del path:*
- `drawEditorScene3DOverlay` con `m_debugDraw == true` itera entidades con NavAgentComponent y dibuja el path actual como polyline cyan entre centros de tiles. Waypoint actual (`path[pathIndex]`) marcado con AABB chiquito brillante. Útil para ver si el agente está atascado o si recomputa cuando debería.

*Bloque 5 — MoodPlayer integration:*
- `PlayerApplication` agrega `m_navSystem` y lo invoca con el mismo patrón que el editor: actualiza target a `m_playCamera.position()` y llama `update`. Skipea cuando `GameState::paused()`. Un proyecto empaquetado con un enemigo persistido corre el comportamiento NavAgent al ejecutar el `MoodPlayer.exe` — sin código extra del dev.

*Bloque 6 — tests:*
- `tests/test_pathfinding.cpp` (6 casos del Bloque 1).
- `tests/test_nav_system.cpp` (4 casos): agente avanza al target, llega y se queda quieto, target inalcanzable no se mueve, agente inactive no se mueve.
- Suite total **192/5283** (antes Hito 22 cerrado: 182/5234).

**Siguiente paso tras completarlo:** Hito 24 (TBD). Plan en `docs/PLAN_HITO24.md`.

### Pendientes menores detectados en Hito 23

- **Rotación del NavAgent hacia su movimiento**: hoy el enemigo camina sin girar — siempre mira en su dirección original. El user lo reportó como "se mueve robóticamente, es un detalle". Fix corto: en `NavSystem::update`, después de calcular `dir` en XZ, setear `tf.rotationEuler.y = atan2(...)`. La fórmula depende del axis "forward" del modelo; para CesiumMan post-importRotation (-90 X) seguramente es `atan2(-dir.z, dir.x)`. **Trigger:** rendering aceptable hoy, polish para Hito 24.
- **Persistencia del NavAgent en `.moodmap`**: V1 NO se persiste (runtime tactico). Si el dev necesita que el enemigo demo "viva" entre cierres del editor, extender `SavedEntity` con `nav_agent: { speed }` (los flags + path se recomputan al runtime). **Trigger:** primer dev que pida "guardar el enemigo en el mapa".
- **Re-pathfind cuando el path se vuelve inválido**: hoy el throttle es 0.5s + delta target. Si un muro aparece in-the-middle (futuro: tile editor en runtime), el path queda inválido hasta el próximo recompute. Mitigación: invalidar al cambiar GridMap. **Trigger:** primer feature que mute tiles en runtime.
- **Asset pack realista para tests físicos**: el dev mencionó querer modelos/mapas más realistas (al estilo Source) para probar físicas en escenarios variados. Se descartó Source por legales + formatos turbios. Alternativas viables: Quaternius (CC0), Sketchfab (CC0/CC-BY), Polyhaven (CC0), Mixamo. Plan: en algún hito posterior, descargar 3-5 props (cajas, barriles, mobiliario) + 1-2 personajes humanoides para enriquecer los demos.
- **Diagonales (8-connected)**: V1 4-conn produce paths visiblemente "boxy". 8-conn con check de corner-cutting daría rutas más naturales. **Trigger:** primera escena donde se note el zigzag.
- **Performance del A\***: 64 nodos (mapa 8x8) es trivial. Si el GridMap escala a 64x64 (4096 nodos), priority_queue empieza a notarse. Mitigaciones: pool de nodos, jump point search. **Trigger:** profile con frame drops por NavSystem.

## Hito 24 — Exposed properties Lua

**Objetivo:** hacer scripts Lua **reusables como componentes**. Hasta el Hito 23 un script tenía constantes hardcoded (`local DEG_PER_SEC = 45.0`); para 3 entidades rotando a velocidades distintas se necesitaban 3 archivos `.lua` o un parámetro editable per-entity. Este hito agrega la segunda opción: el script declara `local speed = engine.exposed("speed", 45.0)` y el Inspector renderiza un campo "speed" por entidad, persistido en `.moodmap`.

**Criterios de aceptación cumplidos:**

*Bloque 1 — `engine.exposed` binding + descubrimiento:*
- `engine/scripting/ExposedProperty.{h,cpp}`: `enum ExposedType { Number, Bool, String, Vec3 }` + `using ExposedValue = std::variant<f32, bool, std::string, glm::vec3>` + `struct ExposedProperty { name, type, defaultValue }`.
- `ScriptComponent` extendido con `std::vector<ExposedProperty> exposedProps` (descubierto al cargar) + `std::unordered_map<std::string, ExposedValue> overrides` (editable). `exposedProps` NO se persiste; se redescubre cada carga.
- `LuaBindings::setupLuaBindings(state, entity, &sc)` — la firma gana un `ScriptComponent*` opcional (nullptr en tests headless). `engine.exposed(name, default)` infiere tipo del Lua default, registra metadata + devuelve el override si existe, sino el default.

*Bloque 2 — Inspector dinámico:*
- `InspectorPanel`: sub-sección "Exposed properties" en el ScriptComponent. Por cada prop renderiza el widget según tipo (DragFloat / Checkbox / InputText / DragFloat3 / ColorEdit3 si el nombre contiene "color"). Botón "Reset" por prop borra el override y vuelve al default del script.
- Edits disparan `sc.loaded = false` para forzar reload del chunk top-level con el nuevo override.

*Bloque 3 — Aplicar overrides en runtime:*
- `ScriptSystem` clear de `exposedProps` antes de cada (re)load — un script que removió un `engine.exposed` entre reloads no deja la prop fantasma en el Inspector. Hot-reload por mtime sigue funcionando: los overrides viven en `ScriptComponent`, no en `sol::state`.
- `assets/scripts/rotator.lua` reescrito: `local speed = engine.exposed("speed", 45.0)`.

*Bloque 4 — Persistencia en `.moodmap`:*
- `SavedEntity` gana `SavedScript` opcional con `{path + overrides}`. Schema: `"script": { "path": "...", "overrides": { name: value, ... } }`.
- `EntitySerializer` serializa/deserializa con tipos number/bool/string/vec3 (vec3 como array `[x,y,z]`). Tipos no soportados (objetos, etc.) loguean warning y se skipean.
- `SceneLoader::applyEntitiesToScene` adjunta el ScriptComponent + overrides; el script se carga perezosamente cuando `ScriptSystem` corre.
- `k_MoodmapFormatVersion` 7 → 8. Archivos v7 sin `script` se leen igual.
- **Fix arrastrado**: el filtro de `SceneSerializer::save` solo persistía entidades con MeshRenderer/Light/RigidBody/Environment; ahora también con Script (path no-vacío). Sin esto el round-trip de un rotator quedaba incompleto.

*Bloque 5 — tests + cierre:*
- `tests/test_exposed_properties.cpp` (8 casos): registro de metadata, override gana sobre default, dedupe por nombre, inferencia de tipos por bool/string/vec3, tipo no soportado se skipea, override de vec3 llega como tabla {x,y,z}, round-trip completo en `.moodmap`, ScriptComponent sin path no se serializa.
- Suite total **200/5287** (antes Hito 23 cerrado: 192/5283).

**Polish reactivo cerrado en el mismo tag:**
- **Mapa de prueba 16x16 con suelo plano + columna central**: antes el demo arena tenía cubos perimetrales con backface-culling visualmente molestos. Ahora el editor + MoodPlayer usan un mapa 16x16 con sólo la columna brick central + un quad escalado como suelo.
- **Skybox equirectangular**: `SkyboxRenderer` gana modo equirect (sampler2D con atan2/asin spherical mapping) además del cubemap original. Evita seams en los polos del cubemap. Asset nuevo: `assets/skyboxes/sky_kloofendal.png` (1.1 MB, kloofendal_43d_clear de Polyhaven CC0, tonemapeado a LDR via `tools/hdr_to_equirect_png.py` con `--exposure 0.4` para preservar detalle de nubes). Shader nuevo `shaders/skybox_equirect.frag` (con la fórmula `v = 0.5 + lat/PI` para compensar el flip-Y de stbi en `OpenGLTexture`).

**Siguiente paso tras completarlo:** Hito 25 — Hot-reload de shaders. Plan en `docs/PLAN_HITO25.md`.

### Pendientes menores detectados en Hito 24

- **Reload spam durante edición del Inspector**: cada frame de DragFloat dispara `sc.loaded = false` → re-ejecuta el chunk top-level → log "Cargado '...'" se imprime ~10 veces por drag. Mitigación: throttle del flag (set sólo al soltar el drag con `IsItemDeactivatedAfterEdit`) o suprimir el log de reload cuando es triggered por override. **Trigger:** dev se queja del spam, o el reload tiene side effects que impactan rendimiento.
- **Inferencia de tipo para `nil` default**: `engine.exposed("foo", nil)` no tiene tipo. Hoy se loguea warning y devuelve nil; la prop no se registra. Si el dev quiere "valor opcional", la API necesitaría un tercer arg explícito de tipo: `engine.exposed("foo", nil, "string")`.
- ~~**Sync manual de shaders al iterar**: editar `shaders/*.{vert,frag}` no surte efecto al relanzar el editor — CMake los copia a `build/debug/Debug/shaders/` sólo en build time. Workaround: `cp shaders/<file> build/debug/Debug/shaders/<file>`. **Mitigación apropiada:** symlink al copy step, o un hot-reload de shaders. **Trigger:** próxima sesión con iteraciones intensas de shader.~~ **Resuelto en Hito 25** con `OpenGLShader::tickHotReload` (mtime + recompile en vivo).
- **Sync de IBL a equirect**: `SceneRenderer.cpp` usa `SkyboxRenderer::Equirect` para el sky pero el IBL bake (`tools/bake_ibl.py`) sigue usando `assets/skyboxes/sky_day/` cubemap. La iluminación PBR no refleja el kloofendal nuevo. **Trigger:** dev nota que las esferas PBR reflejan un cielo distinto al visible.

## Hito 25 — Hot-reload de shaders + polish del sistema de materiales

**Objetivo:** cerrar el ciclo de iteración de shaders sin tener que reiniciar el editor (sufrido durante el polish del Hito 24 con la costura del skybox), y completar el polish del sistema de materiales que reveló dos bugs UX al revisar el render: (1) entidades sin material renderean blanco puro en lugar del patrón missing visible; (2) editar el albedoTint de un cubo contagia a todos los demás que comparten material cacheado.

**Criterios de aceptación cumplidos:**

*Bloque 1 — Hot-reload de shaders (G):*
- `OpenGLShader` guarda `m_vertexPath` + `m_fragmentPath` + `mtime` de la última compilación exitosa. Auto-registro en `s_allShaders` (vector estático) en el ctor; auto-baja en el dtor. Single-thread, sin mutex.
- `OpenGLShader::tryReloadIfChanged()`: si algún archivo cambió en disco, llama al builder helper `buildProgram()` (extraído del ctor para reuso); en éxito tira el `glDeleteProgram` viejo, swapea, invalida la cache de uniforms (las locations pueden diferir si el set de uniforms activos cambió). En fallo loguea el GL error y mantiene el program previo — el render no se rompe.
- `OpenGLShader::tickHotReload(dt)`: throttle 500 ms acumulando dt; cada vez que se vacía itera todos los shaders vivos. Llamado una vez por frame desde `EditorApplication::run()`.
- Para `mtime` desaparecido (file_time_type epoch): se trata como "sin cambios" y no spamea errores — cubre el caso de editores de texto que borran y reescriben el archivo en dos pasos.

*Polish del sistema de materiales (cerrado en el mismo tag):*
- **Costura del skybox equirect**: `shaders/skybox_equirect.frag` cambia `texture()` por `textureLod(..., 0.0)`. En la costura (u salta de 1 a 0) las derivadas `dFdx(u)` son enormes y el sampler elegía un mip bajísimo (1×1 px), produciendo una línea vertical borrosa visible en el cielo. Forzar mip 0 elimina el problema; el skybox no se beneficia de mipmaps porque su cobertura angular por píxel es ~constante.
- **Default material visible como warning**: `MaterialAsset` gana flag `useAlbedoMap` (antes el renderer chequeaba `mat->albedo != 0`, lo cual confundía "sin textura" con "este es el material default"). El default lo tiene en `true` con `albedo=0` para que sample explícitamente `missing.png` (patrón magenta/negro). Materiales tint-puro de archivo (oro_pulido, plastico_azul) inferieren `useAlbedoMap = j.contains("albedo")` — siguen funcionando como antes.
- **Material instance único por entidad**: nuevo `AssetManager::createMaterialFromTexture(texId)` (sin cache, idéntico body al `loadMaterialFromTexture` cacheado pero cada call genera un slot nuevo). Migrados a este path: tiles + floor de `EditorScene::rebuildSceneFromMap` y `PlayerApplication::rebuildSceneFromMap`, los 3 spawners de `DemoSpawners` (rotador, caja física, demo-sombras), `updateTileEntity` (drop de textura), y `SceneLoader` (cada entidad persistida con texture-path obtiene su propio instance). El sentinel `__runtime_tex#<N>` se persiste como el path de la textura subyacente (mismo round-trip que `__tex#<N>` del cacheado). Editar el albedoTint de un cubo no contagia jamás.

**Suite total:** 206/5309 (antes Hito 24 cerrado: 200/5287). Tests nuevos en `test_material_serializer`: 6 cases del flag `useAlbedoMap` y del helper `createMaterialFromTexture` (ids distintos por llamada, edición no muta al hermano, sentinel correcto).

**Verificación funcional del hot-reload (smoke test programático):**
- Editor lanzado en background, modificación de comentario en `shaders/skybox_equirect.frag` → `[render] [info] Hot-reload OK: shaders/skybox.vert + shaders/skybox_equirect.frag` aparece en el log a los ~6 s.
- Inyección de syntax error → `[render] [warning] Hot-reload fallo en ... (se mantiene el shader previo)` — el editor sigue corriendo sin romper render.
- Restauración del shader original → `Hot-reload OK` recovery sin tener que reiniciar.

**Siguiente paso tras completarlo:** Hito 26 (TBD). Plan en `docs/PLAN_HITO26.md` con candidatos.

### Pendientes menores detectados en Hito 25

- **Hot-reload no incluye al MoodPlayer**: el wire-up `tickHotReload` está sólo en `EditorApplication::run`. El Player ships con shaders bundleados al paquete; tiene poco sentido hot-reloadear ahí. **Trigger:** dev quiere debuggear shaders dentro del Player corriendo (poco probable).
- **Encoding del log de error de GL**: `glGetShaderInfoLog` en NVIDIA devuelve a veces caracteres no-UTF-8 que spdlog imprime como `???` y similares. Es pre-existente (también en compile inicial); el hot-reload sólo lo expone más seguido. **Trigger:** dev quiere errores de shader legibles → forzar conversión a UTF-8 o filtrar.
- **Tests del recompile real (sin GL)**: `tryReloadIfChanged` no se cubre con tests automáticos porque requiere contexto OpenGL. Smoke test programático del manual quedó documentado arriba. **Trigger:** infra de tests con GL headless (poco probable a corto plazo).
- **Polish del NavAgent (rotación + persistencia)**: deferido por el dev — "es problema para más adelante cuando ya tenga personajes de verdad". Sigue siendo candidato A del próximo hito.

## Hito 26 — Asset pipeline: extracción de texturas + asset pack Kenney + IBL bakeable

**Objetivo:** cerrar el ciclo "drop un .glb al editor → se ve con su textura". Hasta ahora `MeshLoader` solo extraía geometría, así que cualquier modelo importado salía con el material default (chequer magenta tras el polish del Hito 25). Además, sumar un asset pack real (Kenney Survival Kit, ~80 props) para que el dev pueda armar escenas tipo Source sin tener que modelar a mano. Bonus: cerrar el pendiente del Hito 24 sobre IBL desincronizado del nuevo skybox equirect.

**Criterios de aceptación cumplidos:**

*Bloque 1 — IBL bakeable desde equirect (candidato G):*
- `tools/bake_ibl.py` extiende su entrada para aceptar PNG equirectangular además del cubemap directorio. Si arg es archivo `.png/.jpg`, lo carga con el mapping `atan2/asin` del shader `skybox_equirect.frag` (mismo `v=0.5+lat/PI`). Las funciones de bake (irradiance + 5 mips de prefilter) ahora reciben un `sampler` callable en vez del cubemap directo, así el resto del flujo es agnóstico a la fuente.
- `assets/ibl/sky_kloofendal/` regenerado desde `sky_kloofendal.png` (~41s sobre 32×32 irradiance + 5 mips prefilter, 184 KB de salida). `SceneRenderer` apunta al nuevo path: las esferas PBR ahora reflejan el mismo cielo que el `SkyboxRenderer` muestra.

*Bloque 2 — Asset Browser recursivo:*
- `AssetBrowserPanel` cambia de `directory_iterator` a `recursive_directory_iterator` para `assets/meshes/`. Display name usa el path relativo (`"kenney_survival/barrel.glb"`) para distinguir entre packs. Logical paths siguen siendo `meshes/<rel>` resueltos por VFS sin cambios.

*Bloque 3 — Extracción de texturas embedded + external (candidato E):*
- `MeshAsset` gana `materialAlbedoTextures` (size = `scene->mNumMaterials`). El `MeshLoader` extrae el albedo de cada material referenciado: paths `*N` (embedded en glTF) van por `aiScene::GetEmbeddedTexture` + `loadEmbeddedTexture`; paths externos se resuelven contra la carpeta del archivo (`meshes/kenney_survival/Textures/colormap.png` para un .glb en `meshes/kenney_survival/`).
- `OpenGLTexture` gana ctor desde memoria (`stbi_load_from_memory`). Los dos ctors comparten el helper `uploadDecoded`. Mismo flip vertical que el ctor por path para consistencia.
- `AssetManager` gana `TextureMemoryFactory` + `loadEmbeddedTexture(cacheKey, bytes)` — cacheado por key (típicamente `<glb_path>#<embeddedName>`). Wire-up de la factory real (con `OpenGLTexture` from-memory) en `EditorApplication` y `PlayerApplication`.
- Helper `AssetManager::createMaterialsForMesh(meshId)` arma el vector de `MaterialAssetId` para un `MeshRendererComponent`: cada submesh recibe un material instance con la textura del archivo (`createMaterialFromTexture`), o cae al default-clone si no se pudo extraer (warning visible). Migrados spawn paths de Fox, CesiumMan, y drop genérico al viewport.

*Bloque 4 — Fix del UV flip + autoscale bidireccional:*
- **Removido `aiProcess_FlipUVs`**: el `OpenGLTexture` ya hace `stbi_set_flip_vertically_on_load(true)`. El doble flip cancelaba en texturas simétricas (grid, brick) pero rompía palette swatches como el `colormap.png` de Kenney (cada UV terminaba en pixel ~negro). Tras el fix, los Kenney props se ven con sus colores reales (madera, metal, tela).
- **Autoscale bidireccional al drop**: además de bajar meshes >3m a ~1.5m (Fox importado en cm), ahora subimos meshes <1m a ~1.5m. Sin esto los Kenney barriles (~30 cm) quedaban invisibles en un mundo con tiles de 3m.

*Bloque 5 — Asset pack Kenney Survival Kit:*
- `assets/meshes/kenney_survival/` con 80 GLBs CC0 + License.txt + `Textures/colormap.png` (palette 512×512 compartido). 1.5 MB total. Categorías: barrels/boxes/chests, structures (floor/wall/roof/fence), vegetation (trees/grass/rocks), tools (axe/hammer/pickaxe/shovel), crafting (workbench/campfire/signpost/bedroll), resources (planks/stone/wood). Kenney es CC0 — el editor + MoodPlayer pueden redistribuirlos sin restricciones.

*Bloque 6 — Tests + cierre:*
- 6 tests nuevos en `test_asset_manager.cpp`: `loadEmbeddedTexture` sin factory cae a missing, con factory mock cachea por key + devuelve ids distintos, buffer vacío cae a missing, `createMaterialsForMesh` para missing-mesh devuelve clone único del default, dos llamadas devuelven materiales distintos (clave anti-contagio del Hito 25), id inválido cae al missing-mesh seguro.
- Suite total **212/5326** (antes Hito 25 cerrado: 206/5309).
- `.gitignore` extendido con `__pycache__/` + `*.pyc` (cache de Python que se generaba al importar `tools/bake_ibl.py` y se commiteaba accidentalmente).

**Verificación visual:**
- `Ayuda > Agregar personaje animado` (Fox.glb): ahora aparece naranja/blanco con su textura embedded real (antes chequer magenta tras Hito 25).
- `Ayuda > Agregar enemigo demo` (CesiumMan.glb): idem, skin real.
- Drag de `kenney_survival/barrel.glb` al viewport: aparece a ~1.5m de altura (autoscale up), color madera del colormap (no negro). Idem para los 80 props.
- Drop de la misma textura sobre 2 entidades: cada una recibe instance único — editar el albedoTint de una NO contagia (heredado del Hito 25, validado en este hito por los nuevos tests).

**Siguiente paso tras completarlo:** Hito 27 (TBD). Plan en `docs/PLAN_HITO27.md`.

### Pendientes menores detectados en Hito 26

- **PackageBuilder smart-pack**: identificado en este hito al sumar 1.5 MB de Kenney. Hoy `PackageBuilder` copia `assets/` entero al package final del juego — si el `.moodmap` solo usa 3 de los 80 props, los 77 restantes igual viajan. Anotado como candidato H del Hito 27. **Trigger:** primer juego con > 5 MB de assets sin usar; o cuando algún demo packageado pese > 50 MB.
- **Texturas embedded raw (mHeight > 0)**: `extractAlbedo` solo soporta texturas embedded comprimidas (PNG/JPG con `mHeight=0`). Las raw RGBA inline en glTF se loguean como warning y caen al missing. No vimos un caso real todavía. **Trigger:** modelo CC0 que use embedded raw.
- **Encoding del log de error de mesh loader**: similar al de shaders, assimp puede devolver mensajes con caracteres no-UTF-8 que spdlog imprime mal. Pre-existente. **Trigger:** modelo .glb con error de import + dev necesita leer el mensaje.
- **Inspector no permite cambiar la textura de un material**: hoy el albedo se setea solo al spawn (vía `createMaterialsForMesh`). Si el dev quiere cambiar la textura de un cubo dropeado, no hay UI — tiene que reasignar el material. **Trigger:** dev pide "drop texture sobre material slot del Inspector".
- **Modelos .obj con .mtl**: el path resolution funciona para .glb (carpeta del archivo). Para .obj con .mtl, assimp parsea el .mtl que puede referenciar texturas con paths relativos también. No probado. **Trigger:** primer .obj importado que tenga textura referenciada por .mtl.
