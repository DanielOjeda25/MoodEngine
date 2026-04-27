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
