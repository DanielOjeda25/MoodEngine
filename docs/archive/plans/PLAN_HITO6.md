# Plan — Hito 6: Serialización de proyectos y mapas

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md`. Sección 4.15 y 5.6 de `MOODENGINE_CONTEXTO_TECNICO.md` definen el alcance planeado.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Persistir el estado del editor en disco: un proyecto es una carpeta con uno o más mapas, y cada mapa es el estado completo de un `GridMap` (tiles + texturas asignadas). Sin esto no se puede "terminar" un nivel, ni retomar trabajo entre sesiones, ni distribuir contenido.

1. **`.moodproj`**: archivo de proyecto con metadata (nombre, versión del formato, lista de mapas referenciados, path del mapa por defecto).
2. **`.moodmap`**: archivo de mapa con todas las celdas del `GridMap` (tipo + textura) y metadata (nombre, dimensiones, escala).
3. **Menú Archivo > Nuevo / Abrir / Guardar / Guardar como** funcionales. El editor puede arrancar sin proyecto (estado inicial vacío) o abrir uno existente.
4. **Persistencia de estado del editor** entre sesiones: último proyecto abierto, layout de paneles, modo (Editor/Play) se mantiene. `imgui.ini` ya lo cubre para layout; lo demás va en un `editor.state.json` en la config del usuario.

Entregable: desde el editor puedo hacer `Archivo > Nuevo Proyecto` → me pide carpeta → crea estructura → genero un mapa con tiles → `Guardar` → reinicio → `Abrir Proyecto` → veo el mapa como lo dejé.

---

## Criterios de aceptación

### Automáticos

- [x] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [x] Log del canal `project`: `Proyecto cargado: <path>`, `Proyecto guardado: <path>`, `Mapa guardado: <path> (<N> tiles solidos)`.
- [x] Tests: round-trip serialización para `GridMap` (escribir + leer → debe quedar idéntico). Tests de JSON helpers básicos.
- [x] Cierre limpio, exit 0.

### Visuales

- [x] `Archivo > Nuevo Proyecto` abre un FileDialog (implementar con `tinyfiledialogs` o similar, plataforma-específico), crea la estructura `<root>/maps/`, `<root>/<nombre>.moodproj`, un `<root>/maps/default.moodmap` con el mapa actual.
- [x] `Archivo > Abrir Proyecto` carga un `.moodproj` y muestra el mapa default en el Viewport.
- [x] `Archivo > Guardar` escribe cambios al mapa actual. `Ctrl+S` atajo.
- [x] Sin proyecto abierto, `Guardar` está deshabilitado; `Nuevo`/`Abrir` disponibles.
- [x] Título de la ventana cambia a `MoodEngine - <nombre-proyecto>` cuando hay proyecto abierto, con `*` si hay cambios sin guardar.

---

## Bloque 0 — Pendientes arrastrados del Hito 5

Encarar antes del grueso de serialización. Habilitan editar mapas de verdad antes de guardarlos.

- [x] Extraer `GridRenderer` a `src/systems/GridRenderer.{h,cpp}`. Recibe `const GridMap&`, `const AssetManager&`, `mapWorldOrigin`; dibuja los tiles sólidos. `EditorApplication::renderSceneToViewport` queda limpio. Necesario para cuando el mapa se reemplace al cargar un proyecto nuevo.
- [x] Tile picking: raycast desde el cursor del Viewport al plano XZ=0 → tile (x, y) bajo el mouse. Feature base de casi todo el resto (editar, drag&drop, selección).
- [x] Drag & drop de Asset Browser → Viewport: cuando se suelta sobre un tile, el tile recibe la textura arrastrada. Usa el tile picking del punto anterior.
- [x] Tests `AssetManager` (factory inyectable o `MockTexture` sin GL) para cubrir cacheo, fallback e id-reuse.

## Bloque 1 — Dependencia JSON

- [x] CPMAddPackage para `nlohmann/json` (último tag estable, ~3.11.x).
- [x] `src/engine/serialization/JsonHelpers.h` con utilitarios: `to_json` / `from_json` para tipos comunes del motor (`glm::vec3`, `AABB`, `TileType`), versión del formato como constante.
- [x] Decidir estrategia de versionado: entero monotónico + checks al cargar (fail explícito si es mayor que el soportado). Documentar en `DECISIONS.md`.

## Bloque 2 — Serializador de mapas (`.moodmap`)

- [x] `src/engine/serialization/SceneSerializer.{h,cpp}` con `save(const GridMap&, path)` y `load(path) -> std::optional<GridMap>`.
- [x] Schema del `.moodmap`: `{ version, name, width, height, tileSize, tiles: [{type, texture}] }`. `texture` se guarda como path lógico (string), no como `TextureAssetId` (los ids son volátiles entre sesiones).
- [x] Al cargar, `AssetManager::loadTexture(path)` para cada textura única → reconstruye el mapa con los ids actuales.
- [x] Tests round-trip: `GridMap` armado en memoria → save → load → comparar.

## Bloque 3 — Serializador de proyectos (`.moodproj`)

- [x] `src/engine/serialization/ProjectSerializer.{h,cpp}` con `save(Project&, path)` y `load(path) -> std::optional<Project>`.
- [x] `struct Project { std::string name; std::filesystem::path root; std::vector<std::filesystem::path> maps; std::filesystem::path defaultMap; }`.
- [x] Schema `.moodproj`: `{ version, name, defaultMap, maps: [...] }`. Paths relativos a la carpeta del proyecto.
- [x] Creación de estructura al hacer `Nuevo Proyecto`: carpeta raíz, subcarpetas `maps/` y `textures/`, primer `.moodmap` default copiando el estado actual del editor.

## Bloque 4 — Integración al editor

- [x] `MenuBar` handlers reales para `Archivo > Nuevo / Abrir / Guardar / Guardar como / Cerrar`.
- [x] File dialogs: elegir `tinyfiledialogs` (header-only C) vs `portable-file-dialogs` (header-only C++). Decidir en Bloque 1.
- [x] `EditorApplication` sabe si tiene proyecto abierto (`std::optional<Project>`). Sin proyecto: el mapa hardcoded de Hito 4 Bloque 1 sigue sirviendo como placeholder.
- [x] Título de ventana dinámico (`SDL_SetWindowTitle`).
- [x] Atajo `Ctrl+S` → guardar.
- [x] Flag "dirty" (`m_projectDirty`) que se pone en true al modificar tiles/texturas; `Guardar` lo limpia.

## Bloque 5 — Persistencia de preferencias del editor

- [ ] `editor.state.json` en `<cwd>/.mood/` o equivalente multi-plataforma: último proyecto abierto, último mapa abierto, modo, flags como `m_debugDraw`.
- [ ] Cargar al arrancar (si existe), guardar en el destructor de `EditorApplication`.

> **Diferido al Hito 7+.** La funcionalidad core del hito (nuevo/abrir/guardar/cerrar) quedó cerrada sin esto. Se suma al polish UX; entra cuando empiece a molestar tener que re-abrir el proyecto cada vez. Tracker: sección pendientes al final.

## Bloque 6 — Tests

- [x] `test_json_helpers.cpp`: round-trip de `glm::vec3` y `AABB` con el JSON helper.
- [x] `test_scene_serializer.cpp`: mapa pequeño con texturas distintas → save → load → igual al original.
- [x] `test_project_serializer.cpp`: proyecto con 2 mapas → save → load → igual.

## Bloque 7 — Cierre

- [x] Recompilar y verificar criterios.
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.6.0-hito6` + push.
- [x] Crear `docs/PLAN_HITO7.md`.

---

## Qué NO hacer en el Hito 6

- **No** empaquetar en `.pak`/`.zip` (Hito 21).
- **No** serializar entidades ni componentes (Hito 7 con ECS).
- **No** serializar cámaras ni player (todavía no hay gameplay persistente).
- **No** hot-reload de archivos mientras el editor está abierto (agregable después).
- **No** migrar a binario (JSON está bien para todo el hito; binario entra si los mapas crecen mucho).
- **No** integrar Lua / scripting (Hito 8 según `HITOS.md`).

---

## Decisiones durante implementación

Entradas detalladas en `docs/DECISIONS.md` (fecha 2026-04-23 bajo Hito 6).

- **Bloque 0 arrastrado del Hito 5** ejecutado entero: `GridRenderer` extraído, `ViewportPick` + hover cyan, drag & drop Asset Browser→tile, tests de `AssetManager` con factory inyectable. Bonus: `Ver > Restablecer layout`, líneas debug a 2 px.
- **Pan estilo Blender (middle-drag)** en `EditorCamera`: sensibilidad escalada por el radio (0.0015 × radius) para que se sienta constante a distintos zooms.
- **TextureFactory inyectable** en `AssetManager`: destrabó los tests sin necesidad de contexto GL. `EditorApplication` pasa una lambda que crea `OpenGLTexture`; tests pasan factorías que devuelven `MockTexture`.
- **nlohmann/json 3.11.3** como dep; adaptadores ADL en `JsonHelpers.h` para `glm::vec2/3/4` (como array `[x,y,z]`) y `AABB` (como `{min, max}`). `TileType` con `NLOHMANN_JSON_SERIALIZE_ENUM` → strings estables `"empty"/"solid_wall"`.
- **Versionado del formato** como entero monotónico (`k_MoodmapFormatVersion=1`, `k_MoodprojFormatVersion=1`). `checkFormatVersion` acepta igual o menor; versión futura falla con mensaje explícito.
- **Paths lógicos (string)** para serializar texturas en `.moodmap`, NO `TextureAssetId` (los ids son volátiles entre sesiones). Requirió agregar `AssetManager::pathOf(id)` + vector paralelo `m_texturePaths` + cachear la `missing.png` al id 0 para que round-trip preserve los ids al reabrir.
- **portable-file-dialogs 0.1.0** para los diálogos nativos. DOWNLOAD_ONLY + target INTERFACE propio (el repo no trae `CMakeLists.txt` utilizable).
- **Request/consume** en `EditorUI` para las acciones del menú Archivo (`ProjectAction` enum). Mismo patrón que `togglePlayRequest` del Hito 3.
- **Project.root no se serializa**: se infiere del `parent_path` del `.moodproj` al cargar. Paths dentro del `.moodproj` (maps, defaultMap) son relativos a root.
- **Fix render (drag&drop)**: capturar el rect de la `ImGui::Image` ANTES de `BeginDragDropTarget` — adentro el "último item" ya no es la imagen. Además `IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)` para que el cyan no desaparezca durante drags.

---

## Pendientes que quedan para Hito 7 o posterior

- **Bloque 5 (editor state persistence)**: `editor.state.json` con último proyecto abierto + `m_debugDraw` + modo. Polish UX, no bloqueante.
- **Guardar como completo**: hoy llama a `handleNewProject` (duplica estructura). Un verdadero "save as" debería renombrar el proyecto actual y mover sus mapas al nuevo folder. Entra cuando haya más de un mapa por proyecto (Hito 7 con entidades).
- **Manejo de dirty no confirmado**: si hay cambios sin guardar, los handlers actuales (Cerrar, Abrir, Nuevo) descartan silenciosamente. Agregar un diálogo `pfd::message` con "¿Guardar antes de cerrar?". Polish UX.
- **Múltiples mapas por proyecto**: el schema `.moodproj` ya los soporta (`maps[]`), pero el editor siempre abre el `defaultMap`. Falta UI para listar/cambiar/crear mapas dentro del panel Project (no existe todavía).
- **Hot-reload de archivos**: si el `.moodmap` cambia en disco (editor externo, otra herramienta), el editor no lo ve. Agregable después.
- **Tests de la integración Editor↔Serialization**: actualmente los serializers están testeados a bajo nivel con mocks. Un test E2E que arranque el editor headless y haga new→drop→save sería útil en Hito 10+.
