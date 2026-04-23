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

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Log del canal `project`: `Proyecto cargado: <path>`, `Proyecto guardado: <path>`, `Mapa guardado: <path> (<N> tiles solidos)`.
- [ ] Tests: round-trip serialización para `GridMap` (escribir + leer → debe quedar idéntico). Tests de JSON helpers básicos.
- [ ] Cierre limpio, exit 0.

### Visuales

- [ ] `Archivo > Nuevo Proyecto` abre un FileDialog (implementar con `tinyfiledialogs` o similar, plataforma-específico), crea la estructura `<root>/maps/`, `<root>/<nombre>.moodproj`, un `<root>/maps/default.moodmap` con el mapa actual.
- [ ] `Archivo > Abrir Proyecto` carga un `.moodproj` y muestra el mapa default en el Viewport.
- [ ] `Archivo > Guardar` escribe cambios al mapa actual. `Ctrl+S` atajo.
- [ ] Sin proyecto abierto, `Guardar` está deshabilitado; `Nuevo`/`Abrir` disponibles.
- [ ] Título de la ventana cambia a `MoodEngine - <nombre-proyecto>` cuando hay proyecto abierto, con `*` si hay cambios sin guardar.

---

## Bloque 0 — Pendientes arrastrados del Hito 5

Encarar antes del grueso de serialización. Habilitan editar mapas de verdad antes de guardarlos.

- [ ] Extraer `GridRenderer` a `src/systems/GridRenderer.{h,cpp}`. Recibe `const GridMap&`, `const AssetManager&`, `mapWorldOrigin`; dibuja los tiles sólidos. `EditorApplication::renderSceneToViewport` queda limpio. Necesario para cuando el mapa se reemplace al cargar un proyecto nuevo.
- [ ] Tile picking: raycast desde el cursor del Viewport al plano XZ=0 → tile (x, y) bajo el mouse. Feature base de casi todo el resto (editar, drag&drop, selección).
- [ ] Drag & drop de Asset Browser → Viewport: cuando se suelta sobre un tile, el tile recibe la textura arrastrada. Usa el tile picking del punto anterior.
- [ ] Tests `AssetManager` (factory inyectable o `MockTexture` sin GL) para cubrir cacheo, fallback e id-reuse.

## Bloque 1 — Dependencia JSON

- [ ] CPMAddPackage para `nlohmann/json` (último tag estable, ~3.11.x).
- [ ] `src/engine/serialization/JsonHelpers.h` con utilitarios: `to_json` / `from_json` para tipos comunes del motor (`glm::vec3`, `AABB`, `TileType`), versión del formato como constante.
- [ ] Decidir estrategia de versionado: entero monotónico + checks al cargar (fail explícito si es mayor que el soportado). Documentar en `DECISIONS.md`.

## Bloque 2 — Serializador de mapas (`.moodmap`)

- [ ] `src/engine/serialization/SceneSerializer.{h,cpp}` con `save(const GridMap&, path)` y `load(path) -> std::optional<GridMap>`.
- [ ] Schema del `.moodmap`: `{ version, name, width, height, tileSize, tiles: [{type, texture}] }`. `texture` se guarda como path lógico (string), no como `TextureAssetId` (los ids son volátiles entre sesiones).
- [ ] Al cargar, `AssetManager::loadTexture(path)` para cada textura única → reconstruye el mapa con los ids actuales.
- [ ] Tests round-trip: `GridMap` armado en memoria → save → load → comparar.

## Bloque 3 — Serializador de proyectos (`.moodproj`)

- [ ] `src/engine/serialization/ProjectSerializer.{h,cpp}` con `save(Project&, path)` y `load(path) -> std::optional<Project>`.
- [ ] `struct Project { std::string name; std::filesystem::path root; std::vector<std::filesystem::path> maps; std::filesystem::path defaultMap; }`.
- [ ] Schema `.moodproj`: `{ version, name, defaultMap, maps: [...] }`. Paths relativos a la carpeta del proyecto.
- [ ] Creación de estructura al hacer `Nuevo Proyecto`: carpeta raíz, subcarpetas `maps/` y `textures/`, primer `.moodmap` default copiando el estado actual del editor.

## Bloque 4 — Integración al editor

- [ ] `MenuBar` handlers reales para `Archivo > Nuevo / Abrir / Guardar / Guardar como / Cerrar`.
- [ ] File dialogs: elegir `tinyfiledialogs` (header-only C) vs `portable-file-dialogs` (header-only C++). Decidir en Bloque 1.
- [ ] `EditorApplication` sabe si tiene proyecto abierto (`std::optional<Project>`). Sin proyecto: el mapa hardcoded de Hito 4 Bloque 1 sigue sirviendo como placeholder.
- [ ] Título de ventana dinámico (`SDL_SetWindowTitle`).
- [ ] Atajo `Ctrl+S` → guardar.
- [ ] Flag "dirty" (`m_projectDirty`) que se pone en true al modificar tiles/texturas; `Guardar` lo limpia.

## Bloque 5 — Persistencia de preferencias del editor

- [ ] `editor.state.json` en `<cwd>/.mood/` o equivalente multi-plataforma: último proyecto abierto, último mapa abierto, modo, flags como `m_debugDraw`.
- [ ] Cargar al arrancar (si existe), guardar en el destructor de `EditorApplication`.

## Bloque 6 — Tests

- [ ] `test_json_helpers.cpp`: round-trip de `glm::vec3` y `AABB` con el JSON helper.
- [ ] `test_scene_serializer.cpp`: mapa pequeño con texturas distintas → save → load → igual al original.
- [ ] `test_project_serializer.cpp`: proyecto con 2 mapas → save → load → igual.

## Bloque 7 — Cierre

- [ ] Recompilar y verificar criterios.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.6.0-hito6` + push.
- [ ] Crear `docs/PLAN_HITO7.md`.

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

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 7 o posterior

_(llenar al cerrar el hito)_
