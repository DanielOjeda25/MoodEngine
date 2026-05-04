# Plan — Hito 14: Prefabs

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 14) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Sistema de **prefabs**: tomar una entidad con sus componentes (y eventualmente hijos) y guardarla como un asset `.moodprefab` reusable. Instanciar copias en la escena desde el AssetBrowser con drag-drop al Viewport (reutilizando el pick 3D del Hito 13 para ubicar el spawn). Permitir overrides locales por instancia sin desvincularla del prefab base.

Features core:
- **Guardar**: selección en Hierarchy + `Archivo > Guardar como prefab…` (pfd) → `.moodprefab` en `assets/prefabs/`.
- **Cargar**: `AssetBrowser` tiene sección "Prefabs" con los archivos de `assets/prefabs/`. Drag al Viewport spawnea una instancia.
- **Instanciar**: al soltar, generar una copia de los componentes + hijos. Naming del root = `<prefab>_<counter>`.
- **Override local**: editar `Transform.position` de una instancia no modifica el prefab — cada instancia tiene sus valores propios, el prefab es solo un template al momento del spawn.
- **Persistencia en `.moodmap`**: cada entidad instanciada recuerda su `prefabPath` de origen (para futuro "revertir a prefab" o "aplicar cambios al prefab"). Hito 14 solo **guarda** el link; la propagación bidireccional queda para hitos siguientes.

No-goals del hito: propagación bidireccional prefab ↔ instancia (cambiar el prefab no actualiza instancias existentes), nested prefabs (prefab dentro de prefab), prefab variants, prefab-aware undo/redo (pospuesto a Hito 22).

---

## Criterios de aceptación

### Automáticos

- [x] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [x] Tests: round-trip `PrefabSerializer::save/load` (igual pattern que `SceneSerializer` del Hito 6). Instanciación de un prefab genera una entidad con todos los componentes del original.
- [x] Cierre limpio.

### Visuales

- [x] En Hierarchy, seleccionar una entidad + `Archivo > Guardar como prefab…` produce `assets/prefabs/<nombre>.moodprefab`.
- [x] AssetBrowser tiene sección "Prefabs" que lista esos archivos.
- [x] Drag de un prefab al Viewport spawnea una entidad en el world-point donde se soltó (usando `pickEntity` o un plano XZ si nada bloquea).
- [x] Dos instancias distintas se pueden mover independientemente sin afectarse mutuamente.

---

## Bloque 1 — Formato `.moodprefab`

- [x] `src/engine/serialization/PrefabSerializer.{h,cpp}`: `save(Entity root, name, AssetManager&, path)` y `load(path) -> optional<SavedPrefab>`.
- [x] `SavedPrefab` reusa `SavedEntity` del Hito 10/11/12 (Mesh/Light/RigidBody/PrefabLink) + `vector<SavedEntity> children` (vacio en Hito 14; reservado para nested).
- [x] Schema JSON con `k_MoodprefabFormatVersion = 1`. Campos: `version`, `name?`, `root: SavedEntity`, `children: [SavedEntity...]`.
- [x] **Refactor previo**: extraer `serializeEntityToJson` y `parseEntityFromJson` del namespace anonimo de `SceneSerializer.cpp` a un nuevo `engine/serialization/EntitySerializer.{h,cpp}` para que el `.moodmap` y el `.moodprefab` compartan el mapeo Component <-> JSON sin duplicar.
- [x] `tests/test_prefab_serializer.cpp` (5 casos): round-trip MeshRenderer + Light, persistencia de PrefabLink, archivo inexistente, version futura, JSON corrupto.

## Bloque 2 — AssetManager + catálogo de prefabs

- [x] `AssetManager` extendido con tabla de prefabs: `PrefabAssetId` (u32, 0 = vacio), `loadPrefab(path) -> id`, `getPrefab(id) -> const SavedPrefab*`, `prefabPathOf(id) -> string`, `prefabCount()`.
- [x] Slot 0 = `SavedPrefab` vacio generado en el ctor (sentinela `__empty_prefab`). Mismo pattern que mesh slot 0.
- [x] Tests: `loadPrefab` cachea + cae a missing ante fallo (path invalido, archivo inexistente, fuera de rango). `loadPrefab` carga un prefab real escrito a disco y lo cachea (cache hit en segunda llamada).

## Bloque 3 — Guardar desde Hierarchy

- [x] `EditorUI::request/consumeSavePrefabRequest`.
- [x] `MenuBar`: `Archivo > Guardar como prefab…`. Grayado si no hay entidad seleccionada (proyecto activo NO requerido — los prefabs son globales del repo).
- [x] `EditorApplication::handleSavePrefabRequest`: abre `pfd::save_file` filtrado a `*.moodprefab`, default a `<cwd>/assets/prefabs/<tag>.moodprefab`. Sanitiza el filename (caracteres invalidos `<>:"/\|?*` → `_`) y usa path nativo (`string()` no `generic_string()`) para evitar que el shell de Windows rechace el path con forward slashes.
- [x] Tras guardar: agrega `PrefabLinkComponent(rel_path)` a la entidad fuente y refresca el AssetBrowser (`rescan()`).
- [x] Log `[assets] Prefab guardado: <ruta>`.

## Bloque 4 — AssetBrowser sección Prefabs

- [x] `AssetBrowserPanel::rescan` movido a public.
- [x] Escaneo de `assets/prefabs/*.moodprefab` (lazy parse via `loadPrefab`).
- [x] Sección "Prefabs" entre Meshes y Audio (collapsable, default open). Cada item es drag source con payload `MOOD_PREFAB_ASSET` (u32 id).

## Bloque 5 — Drop al Viewport + instanciación

- [x] `ViewportPanel::PrefabDrop{pending, ndcX, ndcY, prefabId}` + `consumePrefabDrop()`. `BeginDragDropTarget` acepta `MOOD_PREFAB_ASSET`.
- [x] `EditorApplication::handlePrefabDrop`: usa el tile picking del Hito 6 — si el cursor cae sobre un tile valido, spawnea la instancia en el centro del tile.
- [x] Crea entidad `<root.tag>_<counter>` con copia de Transform/MeshRenderer/Light/RigidBody resolviendo paths a ids via `loadMesh`/`loadTexture`.
- [x] Agrega `PrefabLinkComponent(prefabPath)` al root + selecciona la nueva entidad + `markDirty()`.
- [ ] Drop sobre una entidad existente (via Hierarchy) reemplaza su mesh — **diferido**: ambiguedad semantica (reemplazar submeshes? renombrar tag? sobre que pivot?). Trigger: cuando aparezca el caso de uso "cambiar mesh de X" en flujo real.

## Bloque 6 — Persistencia del link en `.moodmap`

- [x] `SavedEntity` gana campo opcional `prefabPath: string` (vacío = no vino de prefab).
- [x] `EntitySerializer` lee/escribe `prefab_path` cuando la entidad tiene `PrefabLinkComponent`.
- [x] `tryOpenProjectPath` reaplica `PrefabLinkComponent` cuando una `SavedEntity` viene con `prefabPath` no vacio.
- [x] `k_MoodmapFormatVersion` 4 → 5. Archivos v4 sin `prefab_path` se siguen leyendo (campo opcional).
- [x] Hito 14 NO implementa "revert to prefab" ni "apply to prefab" — solo el campo queda registrado. La UX viene en hito dedicado.

## Bloque 7 — Tests + cierre

- [x] Round-trip `PrefabSerializer` (5 casos) + `AssetManager::loadPrefab` (2 casos).
- [x] Recompilar, todos los tests verdes (113 / 493 tras Hito 14, antes 106 / 454).
- [x] Smoke test end-to-end: select tile → Guardar como prefab → archivo aparece en AssetBrowser sin reabrir → drag al viewport instancia con todos los componentes → Ctrl+S persiste el link → reabrir reaplica el `PrefabLinkComponent`.
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.14.0-hito14` + push.
- [x] Crear `docs/PLAN_HITO15.md` (Skybox/fog/post-process).

---

## Qué NO hacer en el Hito 14

- **No** propagación bidireccional (cambiar un prefab no actualiza sus instancias). Se difiere a un hito dedicado.
- **No** nested prefabs. Si un prefab contiene una entidad que fue instancia de otro prefab, se guarda como entidad plana sin el link.
- **No** prefab variants (Unity). Si aparece la necesidad, sería post-Hito 14.
- **No** "revert to prefab" / "apply overrides". El link se guarda, pero la UX es futura.
- **No** undo/redo prefab-aware — Hito 22.
- **No** instanciar desde Lua en este hito. Si surge, se evalúa como bloque extra.

---

## Decisiones durante implementación

Detalle en `DECISIONS.md` bajo fecha 2026-04-26 (Hito 14).

- **`EntitySerializer` compartido entre `.moodmap` y `.moodprefab`**: la lógica de mapeo Component <-> JSON estaba duplicada en el namespace anonimo de `SceneSerializer.cpp`. Se extrajo a `engine/serialization/EntitySerializer.{h,cpp}` antes de implementar el `PrefabSerializer` para evitar el clásico bug de divergencia (un componente nuevo se serializa en uno de los dos formatos pero no en el otro).
- **Prefabs como assets globales del repo**, no per-proyecto. Convencion consistente con texturas / audio / meshes (todos en `<cwd>/assets/<tipo>/`). Permite reusar el mismo prefab en distintos proyectos sin duplicar archivos. La carpeta del proyecto solo persiste mapas (`.moodmap`) + el manifiesto (`.moodproj`).
- **`PrefabLinkComponent` como string-only marker** (no struct con metadata): solo path logico. Sin propagacion bidireccional el link no necesita versioning ni dirty tracking; es un breadcrumb.
- **Sin nested prefabs en este hito**: `TransformComponent` del Hito 7 no expone `parent`, asi que la instanciacion no puede reconstruir jerarquia. Children del schema queda reservado pero todos los prefabs guardados desde el editor producen `children: []`. Cuando ECS gane parent/child (Hito 14.5 o dedicado), los children se popularan al guardar.
- **Drop instancia, no reemplaza**: el plan original incluia "drop sobre una entidad existente reemplaza su mesh". Se difirio: ambiguedad semantica (reemplazar mesh manteniendo Transform local? reemplazar todos los componentes? renombrar tag?). Hoy siempre se crea una entidad nueva.
- **Sanitizacion de filename + path nativo en `pfd::save_file`**: bug detectado en smoke test. (a) Tags pueden tener caracteres invalidos para filenames de Windows (`/` en "Mesh_meshes/pyramid.obj_1") que rechazan IFileSaveDialog; (b) `generic_string()` de un path FILE+DIR produce forward slashes que el shell de Windows interpreta como path invalido, devolviendo "" sin mostrar el dialogo. Fix: `sanitize()` reemplaza invalidos por `_`, `string()` mantiene separadores nativos. `handleNewProject` no tenia el bug porque pasa solo el directorio.

---

## Pendientes que quedan para Hito 15 o posterior

- **Propagación bidireccional prefab ↔ instancia**. Requiere "revert to prefab" + "apply overrides to prefab" como acciones en el menu de la instancia. **Trigger**: hito dedicado o cuando aparezca un workflow de iteracion sobre prefabs.
- **Nested prefabs**: prefabs que referencian a otros. Requiere ECS con jerarquia padre/hijo + serialization recursiva. **Trigger**: post-Hito 14, junto con `ChildrenComponent` / `ParentComponent`.
- **Drop de prefab sobre entidad existente reemplaza componentes**. **Trigger**: workflow real que necesite "swap mesh by prefab".
- **Prefab variants** (Unity-style). **Trigger**: si la cantidad de prefabs ligeramente distintos se vuelve inmanejable con la duplicacion.
- **Prefab thumbnails 3D** (mini-render del root al spawnear). **Trigger**: polish post-Hito 17 (PBR) cuando los preview tengan más utilidad.
- **Lua bindings de prefabs** (`scene.instantiate("prefabs/x.moodprefab")`). **Trigger**: primer script gameplay que necesite spawn dinamico.
- **Undo/Redo** de spawn de prefab. **Trigger**: Hito 22.
