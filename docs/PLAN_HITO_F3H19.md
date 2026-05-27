# PLAN F3H19 — Rename con cascada

**Estado:** ✅ **CERRADO** (`v2.19.0-fase3-hito19`, 2026-05-27).
**Predecesor:** F3H18 (validador de assets rotos).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Rename con cascada".

---

## Cierre de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 ✅ — Drag & drop con feedback visual
F3H18 ✅ — Validador de assets rotos
F3H19 ✅ — Rename con cascada            ⬅ ESTE
─────────────────────────────────────────
Sub-fase 3.3 CERRADA 🏁
```

Próxima sub-fase: 3.4 — Viewport pro + Performance + Feedback (5 hitos consolidados desde 8 originales).

---

## Lo que entregó

### Stage A — `AssetRefIndex` engine

Subsistema nuevo en `src/engine/assets/refs/`:
- **`AssetRefIndex.h`**: API + structs.
  - `enum RefKind` (17 variantes cubriendo string-path + id-based + Material textures).
  - `struct RefSite{kind, entity, materialPath, slotIndex, animAlias}`.
  - `std::vector<RefSite> findRefs(Scene&, AssetManager&, const std::string& assetLogicalPath)`.
  - Helper `std::string normalizePath(const std::string&)` (forward slashes, quita prefijo `assets/`).
- **`AssetRefIndex.cpp`** (~200 LOC): walk de la Scene + Materials cacheados. Reusa el patrón del `AssetValidator` (F3H18) pero acumula RefSites tipados en vez de issues.

**Cobertura backend completa** (decisión D1 del dev):
- 7 string paths en componentes: `ScriptComponent.path`, `DialogComponent.dialogPath`, `ItemPickupComponent.itemPath`, `VehicleComponent.configPath`, `EnvironmentComponent.{skyboxPath, colorGradingLutPath}`, `PrefabLinkComponent.path`.
- 6 refs id-based: `AudioSourceComponent.clip`, `MeshRendererComponent.{mesh, materials[]}`, `AnimatorComponent.externalClips[alias]`, `ParticleEmitterComponent.texture`, `BrushComponent.materials[]`.
- 4 texturas en Materials cargados: `albedo`, `metallicRoughness`, `normal`, `ao`.

### Stage B — `AssetManager::renameLogicalPath` API

- **`AssetRegistry<T>::rename(Id, std::string newPath)`** (~25 LOC): actualiza el path lógico asociado a un id en el registry interno. Reescribe el mapeo bidirección id↔path en `m_cache` + `m_paths`. No-op para slot 0 (sentinela fallback).
- **`AssetManager::renameLogicalPath(oldPath, newPath)`** (~85 LOC, archivo nuevo `AssetManager_Rename.cpp`): detecta familia por extension del path, delega a `m_xxx.rename()`. Cubre 10 familias:
  - `.png/.jpg/.jpeg/.tga/.hdr/.bmp` → textures.
  - `.wav/.ogg/.mp3/.flac` → audio.
  - `.fbx` con stem `anim_*` → animation clips; otro → meshes.
  - `.obj/.gltf/.glb` → meshes.
  - `.material` → materials.
  - `.moodprefab` → prefabs.
  - `.mooddialog` → dialogs.
  - `.mooditem` → items.
  - `.moodquest` → quests.
  - `.moodvehicle` → vehicle configs.
  - `.lua` → no-op (scripts no se cachean, viven solo como string en componentes).

### Stage C — `RenameAssetCommand` undoable

- **`src/editor/commands/RenameAssetCommand.h/.cpp`** (~180 LOC).
- 3 mutaciones atómicas en `execute()`:
  1. `std::filesystem::rename(oldDisk, newDisk)`.
  2. `assets.renameLogicalPath(oldLogical, newLogical)` → cache id↔path sincronizado.
  3. Por cada RefSite con kind string-path (los 7 tipos), reescribir el campo del componente.
- Refs id-based NO se tocan individualmente — siguen apuntando al mismo id, y el AssetManager devuelve el nuevo path en `pathOf(id)` automáticamente. **Esta es la clave del diseño**: el rename es eficiente porque la mayoría de las refs son id-based y no requieren tocar la Scene.
- Side-effects extra al reescribir:
  - `ScriptComponent.loaded = false` → fuerza reload con el nuevo path en el siguiente tick del ScriptSystem.
  - `VehicleComponent.dirty = true` → fuerza rematerialización del physics body.
- `undo()` aplica las 3 mutaciones en reversa (newDisk → oldDisk, newLogical → oldLogical, refs con `to=oldLogical`).
- Si `fs::rename` falla (ec set), abort silencioso: loguea + retorna sin mutar scene ni AssetManager. **El caller (modal del Asset Browser) debe validar pre-construct que `newDisk` no existe** (decisión D2 — abort vs sufijo automático).

### Stage D — UI Asset Browser (rename modal + context menu)

- **`AssetBrowserPanel_Rename.cpp`** (~165 LOC, archivo nuevo): `openRenameModal`, `drawRenameModal`, `addRenameContextMenu` helpers + state miembros nuevos en `AssetBrowserPanel.h` (m_renameModalOpen, m_renameOldLogical, m_renameOldDisk, m_renameNewName, m_renameError, m_renameRefsCache).
- **Modal de rename**:
  - InputText con el filename actual (preserva carpeta).
  - Lista compacta de refs (max 8 visibles, scroll) — muestra `Entity N` o `Material: <path>` según source.
  - Validación inline: empty / mismo nombre / nombre ya existe en disco (mensaje rojo).
  - Botón Renombrar disabled si el nuevo nombre no es válido.
  - Cancel descarta el snapshot y cierra.
- **Context menu**: helper `addRenameContextMenu(logicalPath)` llamado después de cada widget en los 8 tabs (Textures / Meshes / Vehicles / Animations / Prefabs / Materials / Scripts / Audio). Vehicle tab tiene su propio popup multi-acción ("Renombrar..." + "Eliminar..."), así que el item de rename se agrega inline al popup existente para evitar conflict de IDs.
- **Wire en EditorApplication**:
  - `setScene(m_scene.get())` inyectado en `EditorApplication_Init.cpp` post-init de la scene.
  - `consumePendingRename()` consumido en `pumpUiRequests` (`EditorApplication_Run.cpp`): construye `RenameAssetCommand`, push al `m_history` (undoable), rescan del browser + refresh del AssetIssues panel + `markDirty()`.
- **`PendingRename` struct** público en el panel (`AssetBrowserPanel.h`): oldDiskPath / newDiskPath / oldLogical / newLogical / refs. Single-frame consume vía `consumePendingRename()` (patrón gemelo de `m_pendingDeleteVehicle`).

### Tests headless

`tests/test_asset_ref_index.cpp` + `tests/test_rename_asset_command.cpp`:
- **AssetRefIndex (9 casos)**: normalizePath, scene vacía, ScriptComponent matching, path distinto, campo vacío, multiples refs al mismo path, 7 tipos string-path simultáneos, Skybox vs ColorGradingLut, PrefabLink path.
- **renameLogicalPath (6 casos)**: asset no cargado, oldPath == newPath, extension `.lua`, textura cargada actualiza pathOf, tras rename el oldPath queda huérfano del cache, `.fbx` con stem anim distingue mesh vs anim.
- **RenameAssetCommand (5 casos)**: rename de Script + componente actualizado + undo restaura, cascada a 3 entities, textura cargada con pathOf actualizado, Skybox + ColorGradingLut del mismo Environment, name() incluye old + new.

Total: **20 cases / 62 asserts F3H19**, suite verde 1207/11519+ (incremental sobre F3H18).

### i18n

9 keys nuevas en `es.json` + `en.json`:
- `editor.asset_browser.rename` — "Renombrar..." / "Rename..." (context menu item).
- `editor.asset_browser.rename_modal.{title, new_name, refs_count, no_refs, confirm, cancel, error_same, error_exists}` — labels + errores inline del modal.

---

## Sites tocados

```
src/engine/assets/refs/AssetRefIndex.h                       NEW
src/engine/assets/refs/AssetRefIndex.cpp                     NEW
src/engine/assets/manager/AssetRegistry.h                    +rename() method
src/engine/assets/manager/AssetManager.h                     +renameLogicalPath decl
src/engine/assets/manager/AssetManager_Rename.cpp            NEW
src/editor/commands/RenameAssetCommand.h                     NEW
src/editor/commands/RenameAssetCommand.cpp                   NEW
src/editor/panels/assets/AssetBrowserPanel.h                 +setScene + PendingRename + state del modal
src/editor/panels/assets/AssetBrowserPanel.cpp               +drawRenameModal call
src/editor/panels/assets/AssetBrowserPanel_Tabs.cpp          +addRenameContextMenu en 7 tabs + inline en Vehicles popup
src/editor/panels/assets/AssetBrowserPanel_Rename.cpp        NEW
src/editor/application/EditorApplication_Init.cpp            +assetBrowser().setScene(m_scene)
src/editor/application/EditorApplication_Run.cpp             +consumePendingRename + push command + rescan + refresh issues
tests/test_asset_ref_index.cpp                               NEW
tests/test_rename_asset_command.cpp                          NEW
tests/CMakeLists.txt                                         +2 tests + +2 .cpp engine
CMakeLists.txt                                               +3 .cpp (AssetRefIndex + AssetManager_Rename + AssetBrowserPanel_Rename) + RenameAssetCommand
assets/i18n/es.json                                          +9 keys
assets/i18n/en.json                                          +9 keys
```

---

## Decisiones

**D1 — Cobertura backend completa, UI inicial en AssetBrowser.**
Validada via `AskUserQuestion`. El backend (AssetRefIndex + renameLogicalPath + RenameAssetCommand) cubre los 17 tipos de refs identificados. La UI de iniciar el rename se ofrece desde el AssetBrowser principal (8 tabs). Browsers especializados (ItemBrowserPanel, DialogBrowserPanel, QuestPropertyEditorPanel) NO tienen botón "Renombrar" todavía — al ser browsers para edición de un asset, el rename desde ahí es secundario; cuando emerja demanda concreta, agregar el menú es mecánico (1 llamada a `openRenameModal` por panel). Memoria backlog [[asset_rename_browser_coverage]] agenda los 3 paneles diferidos.

**D2 — Abort si el nombre destino ya existe.**
Validada via `AskUserQuestion`. Cuando el dev intenta renombrar a un nombre que ya existe en disco, el modal muestra error rojo "Ya existe un archivo con ese nombre" y el botón Renombrar queda en estado de fallo (al click vuelve a chequear pero no destruye el archivo destino). Alternativas descartadas: sufijo automático `_2` (puede sorprender al dev — no quiere magia en operación destructiva), confirmar overwrite (más clicks, mismo riesgo que sufijo). El abort es el patrón más seguro para evitar pérdida de datos.

**D3 — RenameAssetCommand confía en pre-conditions del caller.**
El comando NO valida que `newDiskPath` no exista. La validación vive en el modal (Stage D), antes de construir el comando. Si por bug el caller no valida, `fs::rename` en Windows sobreescribe el destino — el comando loguea pero no rollback. Razón: poner validación en el comando duplicaría check con el modal y el comando no tiene UI para reportar errores. Trade-off explícito: el modal es la fuente de verdad para validación.

**D4 — Refs id-based NO se reescriben en componentes — solo se actualiza el cache del AssetManager.**
Las refs por AssetId (Mesh/Material/Audio/Animation/etc) NO requieren tocar individualmente cada componente porque el id no cambia, solo el path interno del AssetManager. `pathOf(id)` devuelve el nuevo path automáticamente al render/save siguiente. Esto vale 80%+ del rendimiento del rename: si hay 500 entities con MeshRendererComponent apuntando al mismo mesh renombrado, no iteramos 500 componentes — basta un solo `m_meshes.rename(id, newPath)`. Las únicas refs reescritas son string-path en 7 componentes específicos (Script/Dialog/Item/Vehicle/Skybox/ColorGradingLut/PrefabLink) donde el path es directo, no via id.

**D5 — Side-effects al reescribir paths string en componentes.**
Algunos componentes con string path tienen estado runtime derivado que necesita invalidarse al cambiar el path:
- `ScriptComponent.loaded = false` → fuerza ScriptSystem a recargar el .lua con el path nuevo.
- `VehicleComponent.dirty = true` → fuerza VehicleSystem a rematerializar el physics body con el config nuevo.
Otros (`DialogComponent`, `ItemPickupComponent`, `EnvironmentComponent`, `PrefabLinkComponent`) no tienen estado derivado de invalidar — se leen lazy por el sistema correspondiente y el path nuevo entra en el primer uso.

---

## Validación

Dev confirmó "todo ok" tras testear:
1. Right-click en cualquier tab del Asset Browser → context menu con "Renombrar...".
2. Modal aparece con path actual + InputText + preview de refs.
3. Cambiar nombre + Renombrar → archivo se mueve en disco, refs en componentes se actualizan (validado en Inspector), browser muestra el nombre nuevo tras rescan.
4. Validation inline:
   - Nombre vacío → botón disabled.
   - Mismo nombre → botón disabled.
   - Nombre ya existe → mensaje rojo + abort.
5. Ctrl+Z → archivo restaurado + refs revertidas.

---

## Lo que NO toca F3H19

- **Rename desde ItemBrowserPanel / DialogBrowserPanel / QuestPropertyEditorPanel**: backlog mecánico — 1 llamada por panel cuando emerja demanda. Memoria [[asset_rename_browser_coverage]].
- **Bulk rename** (renombrar N assets a la vez): backlog si emerge.
- **Move** (cambiar carpeta de un asset): scope distinto, no entra a F3H19.
- **Rename con conflict resolution intelligent** (sufijo auto / overwrite confirm): scope hito propio si emerge.
- **Rename de refs dentro de prefabs cacheados sin spawn** (los prefabs son `SavedPrefab` con su propio sub-Scene): backlog si emerge — walk del prefab content es similar al walk de Scene.
- **Rename de refs dentro de shader graphs** (`.shadergraph`): los shadergraphs tienen sus propias refs internas a texturas/samplers, no cubiertas por AssetRefIndex. Backlog si emerge.

---

## Siguiente hito

**F3H20 — Snapping configurable** (Sub-fase 3.4 arranca). Plan stub en [`PLAN_HITO_F3H20.md`](PLAN_HITO_F3H20.md).
