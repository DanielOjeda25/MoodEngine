# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**Hito 11 cerrado, mergeado a `main` y publicado en origin.**
Tag: `v0.11.0-hito11`.
Verificado automático: suite doctest 98/428 pasando (+8 lighting, +1 light round-trip vs Hito 10), editor compila sin warnings nuevos, `lit.{vert,frag}` se compila en el arranque, ningún uniform missing reportado por GL debug. Verificado por el dev a ojo: con luz puntual blanca via `Ayuda > Agregar luz puntual demo`, las paredes muestran caras frente/atrás distinguibles; el slider de intensity en el Inspector se refleja en vivo; tras Ctrl+S y reload del proyecto, la luz se recrea idéntica.

**Próximo paso:** Hito 12 — Física con Jolt Physics. Reemplazar el `PhysicsSystem` AABB-vs-grid del Hito 4 por rigid bodies con Jolt: gravedad real, colliders convexos, capsule controller para el jugador. Plan en `docs/PLAN_HITO12.md`.

### Hito 10 (anterior, ya cerrado)
Tag: `v0.10.0-hito10`.
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

### Tarea inmediata: implementar el Hito 11

El Hito 10 está cerrado (tag `v0.10.0-hito10` en origin). El foco ahora es el **Hito 11 — Iluminación Phong/Blinn-Phong**. Activar `LightComponent` (era placeholder desde Hito 7), extender el shader con normales + cálculo Phong, soportar directional + point lights, demo de escena con iluminación dinámica.

El plan desglosado por tareas está en `docs/PLAN_HITO11.md`.

**Punto de arranque concreto:** extender el vertex attribute layout para incluir la normal (assimp ya la calcula desde Hito 10 vía `aiProcess_GenNormals`). Nuevo shader `lit.{vert,frag}` con cálculo Phong. `LightSystem` que recolecta componentes `LightComponent` de la scene y los sube como uniforms al shader. Demo: agregar `Ayuda > Agregar luz` con posición y color editables.

### Flujo recomendado en esta sesión

1. Leer `docs/PLAN_HITO11.md`.
2. Trabajar bloque por bloque, marcando en el plan al cerrar cada uno.
3. Actualizar `docs/DECISIONS.md` cuando aparezca una decisión no trivial (p. ej. shader hot-reload, cómo pasar el array de luces al shader — uniform array vs UBO, intensidad en W/m² vs 0..1 arbitrario).
4. Al final: commits atómicos en español, merge a main, tag `v0.11.0-hito11`, actualizar este documento y `docs/HITOS.md`, crear `docs/PLAN_HITO12.md`.

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
