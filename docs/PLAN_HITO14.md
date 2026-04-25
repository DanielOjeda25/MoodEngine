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

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Tests: round-trip `PrefabSerializer::save/load` (igual pattern que `SceneSerializer` del Hito 6). Instanciación de un prefab genera una entidad con todos los componentes del original.
- [ ] Cierre limpio.

### Visuales

- [ ] En Hierarchy, seleccionar una entidad + `Archivo > Guardar como prefab…` produce `assets/prefabs/<nombre>.moodprefab`.
- [ ] AssetBrowser tiene sección "Prefabs" que lista esos archivos.
- [ ] Drag de un prefab al Viewport spawnea una entidad en el world-point donde se soltó (usando `pickEntity` o un plano XZ si nada bloquea).
- [ ] Dos instancias distintas se pueden mover independientemente sin afectarse mutuamente.

---

## Bloque 1 — Formato `.moodprefab`

- [ ] `src/engine/serialization/PrefabSerializer.{h,cpp}`: `save(Entity root, path)` y `load(path) -> optional<SavedPrefab>`.
- [ ] `SavedPrefab` reusa `SavedEntity` del Hito 10 (+ light/rigidbody/audio/script del 11/12/9/8) + `vector<SavedEntity> children`.
- [ ] Schema JSON con `k_MoodprefabFormatVersion = 1`. Campos: `version`, `name` (opcional), `root: SavedEntity`, `children: [SavedEntity...]` (depth-first preorder; cada child tiene `parentIndex` relativo al array para reconstruir el árbol).
- [ ] `tests/test_prefab_serializer.cpp`: round-trip de entidad con MeshRenderer + Light + Transform; verifica que los campos se preservan byte-a-byte tras save+load.

## Bloque 2 — AssetManager + catálogo de prefabs

- [ ] `AssetManager` extendido con tabla de prefabs: `PrefabAssetId` (u32, 0 = missing/null), `loadPrefab(path) -> id`, `getPrefab(id) -> const SavedPrefab*`, `prefabPathOf(id) -> string`.
- [ ] `PrefabFactory` opcional (inyectable) para tests sin disco.
- [ ] Escaneo de `assets/prefabs/*.moodprefab` al iniciar proyecto + refresh on demand (igual que meshes/textures/audio).

## Bloque 3 — Guardar desde Hierarchy

- [ ] `EditorUI::requestSavePrefabDialog()` + `consumeSavePrefabRequest()`.
- [ ] `MenuBar`: `Archivo > Guardar como prefab…` (gris si no hay entidad seleccionada). Abre `portable-file-dialogs::save_file` filtrado a `*.moodprefab` con default `assets/prefabs/`.
- [ ] `EditorApplication::handleSavePrefab(path)`: serializa la entidad seleccionada (+ sus hijos vía `TransformComponent.parent` si el hito 7 lo soportaba — si no, Hito 14 solo guarda la entidad sin hijos y lo declara). Registra en AssetManager.
- [ ] Log `[assets] Prefab guardado: <ruta>` en canal `assets`.

## Bloque 4 — AssetBrowser sección Prefabs

- [ ] `AssetBrowserPanel` agrega sección "Prefabs" (colapsable, entre Meshes y Audio) que lista los prefabs del AssetManager.
- [ ] `BeginDragDropSource` con payload `"MOOD_PREFAB_ASSET"` (u32 id) — mismo pattern que `MOOD_MESH_ASSET`.
- [ ] Icono/thumbnail: por ahora un cuadrado con el primer material del root si tiene MeshRenderer, o un placeholder.

## Bloque 5 — Drop al Viewport + instanciación

- [ ] `ViewportPanel::PrefabDrop{pending, ndcX, ndcY, prefabId}` + `consumePrefabDrop()`. `BeginDragDropTarget` acepta `MOOD_PREFAB_ASSET`.
- [ ] `EditorApplication::handlePrefabDrop(ndc, prefabId)`:
  - Construir ray con la cámara actual.
  - Usar `pickEntity` para encontrar un hit; si hay, la instancia se ubica en `worldPoint`. Si no, fallback a un plano XZ a `y=0` intersectado con el ray.
  - Crear entidades: root primero (`Scene::createEntity("<prefab>_N")`), luego children en preorder respetando el `parentIndex`.
  - Por ahora sin relación parent/child real en `TransformComponent` (si el Hito 7 no llegó a exponerla) — las children se crean con `Transform.position = root.position + child_local_offset`.
  - `EditorUI::setSelectedEntity(root)` al terminar, `markDirty()`.

## Bloque 6 — Persistencia del link en `.moodmap`

- [ ] `SavedEntity` gana un campo opcional `prefabPath: string` (vacío = no vino de prefab).
- [ ] `handlePrefabDrop` llena ese campo en el root.
- [ ] `k_MoodmapFormatVersion` 4 → 5. Archivos v4 se siguen leyendo (campo ausente = `""`).
- [ ] Este hito **no** implementa "revert to prefab" ni "apply to prefab" — solo el campo queda registrado. La UX viene en Hito 14.5 o dedicado.

## Bloque 7 — Tests + cierre

- [ ] Round-trip `PrefabSerializer` (save + load).
- [ ] Instanciación: fake scene, load prefab, verificar que las entidades creadas tienen los componentes esperados.
- [ ] `tests/test_asset_manager.cpp`: agregar caso de loadPrefab con factory mock.
- [ ] Recompilar, todos los tests verdes.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.14.0-hito14` + push.
- [ ] Crear `docs/PLAN_HITO15.md` (Skybox/fog/post-process).

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

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 15 o posterior

_(llenar al cerrar el hito)_
