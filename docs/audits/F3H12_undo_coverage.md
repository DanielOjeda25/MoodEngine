# F3H12 — Undo coverage audit del Inspector

> Auditoría sistemática de los 15 archivos `InspectorPanel_*.cpp` para detectar
> qué widgets editan estado de la escena/asset sin pasar por el `HistoryStack`
> (Ctrl+Z no los revierte).
>
> **Fecha:** 2026-05-26.
> **Scope:** edits desde el Inspector (no incluye gizmo, ortho dragging,
> AssetBrowser, Hierarchy — fuera de F3H12).

## 1. Infra existente

- `HistoryStack` ([HistoryStack.h](../../src/editor/commands/HistoryStack.h)) — undo/redo central.
- `InspectorEditTracker` ([InspectorEditTracker.h](../../src/editor/panels/scene/InspectorEditTracker.h)) — single-entity, captura `before` al `IsItemActivated`, empuja `EditPropertyCommand<T>` al `IsItemDeactivatedAfterEdit`. Variant tipado: `f32`, `glm::vec3`, `glm::vec4`, `bool`, `std::string`, `u32`, `std::pair<f32,f32>`.
- `MultiEditTracker` ([MultiEditTracker.h](../../src/editor/panels/scene/MultiEditTracker.h)) — N entidades, snapshot por entity al activar. Variant: `f32`, `glm::vec3` (gap: `bool`, `u32`, `glm::vec4`).
- Helpers en [InspectorPanel_Internal.h](../../src/editor/panels/scene/InspectorPanel_Internal.h):
  - `detail::pushEditIfDone<T>(...)` — wraps `trackPropertyEdit<T>` con `ui->historyStack()`.
  - `detail::fieldDragFloat3 / fieldDragFloat / fieldColorEdit3` — triplete label + widget + push.
  - `detail::multiEditColor3 / multiEditDragFloat` — multi-edit con live preview + `MultiEditPropertyCommand<T>`.
- Commands tipados existentes en [src/editor/commands/](../../src/editor/commands/):
  `EditPropertyCommand`, `MultiEditPropertyCommand`, `EditTransformCommand`, `MultiEditTransformCommand`, `EditScriptComponentCommand`, `EditMeshRendererMaterialCommand`, `EditAssetPropertyCommand`, `EditBrushUVCommand`, `EditBrushMaterialCommand`, `EditBrushGeometryCommand`, `EditBrushFaceMaterialCommand`, `PasteComponentCommand`, `AddComponentCommand`, `CreateEntityCommand`, `DeleteEntityCommand`, `BooleanOpCommand`, `SetTileCommand`, `NodeGraphCommand`, `VisGroupCommands`.

## 2. Cobertura por panel

Leyenda:
- ✅ **cubierto** — todos los widgets editables empujan command.
- 🟡 **parcial** — falta cobertura en widgets específicos (gaps listados).
- 🔴 **sin cobertura** — el panel edita state sin commands en absoluto.

| Panel | Estado | Notas |
|---|:---:|---|
| `InspectorPanel.cpp` (Add/Remove component, paste) | ✅ | `makeAddComponentCommand<T>` + `makeRemoveComponentCommand<T>` + `PasteComponentCommand` |
| `Transform` | ✅ | 3 DragFloat3 (pos/rot/scale) — multi-edit por delta a peers + `pushEditIfDone<vec3>` |
| `MeshRenderer` | 🟡 | gap menor — combo `Shader graph` |
| `Camera` | ✅ | 3 DragFloats stub `BeginDisabled` (break-A7) cubiertos via `fieldDragFloat` |
| `Light` | 🟡 | gaps: `enabled` checkbox, `direction` DragFloat3, `castShadows` checkbox |
| `Environment` | 🟡 | gaps grandes — 8 combos, 5 checkboxes, 6 reset buttons, file picker (sky/LUT) |
| `Script` | 🔴 | path InputText + 4 tipos de exposed value + Reset overrides — todo sin commands |
| `RigidBody` | ✅ | type/shape combos via `EditPropertyCommand<u32>`, halfExt/mass/fric via `fieldDragFloat*`, isSensor via `EditPropertyCommand<bool>` |
| `Ragdoll` | ✅ | totalMass/limbRadius/spawnImpulse via `fieldDragFloat*`, useGravity via `EditPropertyCommand<bool>` |
| `Joint` | ✅ | type combo + target drop + pivot/axis/limits — todo cubierto |
| `Vehicle` | 🔴 | declarado "NO undoable v1" (línea 13) — configPath InputText + 11 DragFloats live tuning + Combo preset + 2 buttons. Vive en `VehicleConfig*` (asset compartido). |
| `AudioSource` | 🟡 | gap: combo `clip` |
| `Animator` | 🟡 | gaps: alias InputText, Play/Remove buttons en external clips, drop `MOOD_ANIMCLIP_ASSET` |
| `ParticleEmitter` | 🟡 | gap: combo `emissionShape` |
| `Trigger` | 🟡 | gaps: 3 checkboxes (`triggersOnPlayer`, `oneShot`, `enabled`) |
| `ForceField` | 🟡 | gaps: 2 combos (shape, mode), 3 checkboxes (linearFalloff, ignoreMass, enabled) |
| `Cloth` | 🟡 | gaps: 2 SliderInt (resX, resY), combo anchor, checkbox useGravity |
| `Brush` | ✅ | UV scale/rot/offset + lockToWorld + 6 buttons alignment — todo via `EditBrushUVCommand` |
| `Inventory` | 🔴 | mode combo, max_items/grid_w/grid_h InputInt, equipment slot names/tags, qty/slot_index InputInt, drop ITEM, clear — todo escribe directo |

## 3. Detalle de gaps por panel (widgets afectados)

### 🔴 Vehicle ([InspectorPanel_Vehicle.cpp:13](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L13))

Comentario explícito: `// Inspector NO undoable v1 (mantengo footprint chico; agendable polish).`

- `configPath` InputText ([:89](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L89))
- `Soltar .moodvehicle aqui` drop target ([:103](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L103))
- `Rematerializar (dirty=true)` button ([:133](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L133))
- `Aplicar preset##vt_preset` Combo ([:161](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L161)) — aplica ~10 mutaciones a `VehicleConfig*` en una sola
- 11 DragFloats live tuning ([:176-212](../../src/editor/panels/scene/InspectorPanel_Vehicle.cpp#L176)) — Masa, Torque, RPM, Brake, Steer, Damping, CoM (vec3), Friccion long/lat — editan `VehicleConfig*` (asset compartido por instancias)

**Patrón requerido**: `EditAssetPropertyCommand` o variante propia que mute el config + setee `veh.dirty=true`. La forma del setter difiere de los componentes (asset, no entidad).

### 🔴 Script ([InspectorPanel_Script.cpp](../../src/editor/panels/scene/InspectorPanel_Script.cpp))

- `path` InputText ([:29](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L29)) — escribe `sc.path` + `sc.loaded=false`
- `Recargar` button ([:36](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L36)) — solo resetea `loaded`, acción (no edit)
- Exposed properties — DragFloat (Number, [:70](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L70)), Checkbox (Bool, [:78](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L78)), InputText (String, [:88](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L88)), ColorEdit3/DragFloat3 (Vec3, [:102-104](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L102))
- `Reset` SmallButton ([:115](../../src/editor/panels/scene/InspectorPanel_Script.cpp#L115)) — borra `sc.overrides[name]`

**Patrón requerido**: `EditScriptComponentCommand` (ya existe en commands/) — verificar si soporta overrides map. Si no, extender o agregar `EditPropertyCommand<map>` específico.

### 🔴 Inventory ([InspectorPanel_Inventory.cpp](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp))

- `mode` Combo ([:62](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L62))
- `max_items` InputInt ([:72](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L72))
- `grid_width` / `grid_height` InputInt ([:81-87](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L81))
- Equipment slot `name` + `tag_filter` InputTexts ([:105-118](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L105))
- `Eliminar slot` (x) SmallButton ([:124](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L124))
- `Agregar slot` Button ([:131](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L131))
- Entry `quantity` / `slot_index` InputInt ([:162-174](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L162))
- `Eliminar entry` SmallButton ([:181](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L181))
- Drop `MOOD_ITEM_ASSET` ([:200](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L200))
- `Clear` Button ([:214](../../src/editor/panels/scene/InspectorPanel_Inventory.cpp#L214))

**Patrón requerido**: probablemente un `EditInventoryStateCommand` que snapshotee `Inventory::State` entero antes/después (igual que `EditBrushUVCommand`). Inputs primitivos podrían ir con `EditPropertyCommand<u32/string>` pero la estructura anidada (`config.equipment_slots`, `entries`) sugiere snapshot-based.

### 🟡 Light ([InspectorPanel_Light.cpp](../../src/editor/panels/scene/InspectorPanel_Light.cpp))

- `enabled` Checkbox ([:26](../../src/editor/panels/scene/InspectorPanel_Light.cpp#L26))
- `direction` DragFloat3 ([:105](../../src/editor/panels/scene/InspectorPanel_Light.cpp#L105)) — solo cuando type=Directional
- `castShadows` Checkbox ([:111](../../src/editor/panels/scene/InspectorPanel_Light.cpp#L111)) — solo Directional

**Patrón requerido**: `pushEditIfDone<bool>` para los 2 checkboxes, `pushEditIfDone<glm::vec3>` para direction. Trivial — ya hay precedente en el mismo archivo (color, intensity, radius via multiEdit*).

### 🟡 Environment ([InspectorPanel_Environment.cpp](../../src/editor/panels/scene/InspectorPanel_Environment.cpp))

- `skybox_preset` Combo + file picker custom ([:135-173](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L135))
- `fog mode` Combo ([:182](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L182))
- `tonemap` Combo ([:252](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L252))
- `bloom_enabled` Checkbox ([:288](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L288))
- `ssao_enabled` Checkbox ([:345](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L345))
- `color_grading_enabled` Checkbox ([:437](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L437))
- `color_grading_preset` Combo + file picker ([:488-528](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L488))
- `ssr_enabled` Checkbox ([:565](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L565))
- 6 `drawSectionResetButton` ([:67](../../src/editor/panels/scene/InspectorPanel_Environment.cpp#L67)) — fog/tonemap/bloom/ssao/csm/cgrade/ssr — cada uno reasigna 3-5 campos a `kEnvDefaults` sin command

**Patrón requerido**:
- Combos / Checkboxes simples → `EditPropertyCommand<u32 / bool / string>`.
- Reset buttons → `EditPropertyCommand` con snapshot del before del subset y after = defaults. Considera batch command (1 Ctrl+Z revierte todo el reset, no campo por campo). Alternativa: snapshot del `EnvironmentComponent` entero pre/post (igual que Brush UV).

### 🟡 MeshRenderer ([InspectorPanel_MeshRenderer.cpp:207](../../src/editor/panels/scene/InspectorPanel_MeshRenderer.cpp#L207))

- `Shader graph` Combo — escribe directo `mat->shaderGraphPath`. Botones `Editar` / `+ Nuevo` son acciones (abren panel), no edits.

**Patrón requerido**: `EditAssetPropertyCommand` o `EditPropertyCommand<std::string>` con setter que captura `assetsCap + matIdCap` (igual que albedoTint del mismo archivo).

### 🟡 AudioSource ([InspectorPanel_Audio.cpp:41](../../src/editor/panels/scene/InspectorPanel_Audio.cpp#L41))

- Combo `clip` (BeginCombo + Selectable) — escribe `asrc.clip`. Resto del panel ya cubierto.

**Patrón requerido**: `EditPropertyCommand<u32>` (AudioAssetId == u32). Setter resetea `asrc.started` también (igual que el manual).

### 🟡 Animator ([InspectorPanel_Animation.cpp](../../src/editor/panels/scene/InspectorPanel_Animation.cpp))

- Alias `InputText` ([:141](../../src/editor/panels/scene/InspectorPanel_Animation.cpp#L141)) — escribe `alias.assign(buf)`
- `Play` SmallButton ([:129](../../src/editor/panels/scene/InspectorPanel_Animation.cpp#L129)) — setea clipName + reset time. Discutible (acción, no edit).
- `Remove (-)` SmallButton ([:162](../../src/editor/panels/scene/InspectorPanel_Animation.cpp#L162)) — borra de `externalClips`.
- Drop `MOOD_ANIMCLIP_ASSET` ([:185](../../src/editor/panels/scene/InspectorPanel_Animation.cpp#L185)) — agrega a `externalClips`.

**Patrón requerido**: `EditPropertyCommand<vector<pair<string,id>>>` para `externalClips` (snapshot antes/después). El clipName Play sí debería ser undoable (=Combo clipName ya cubierto via cmd similar).

### 🟡 ParticleEmitter ([InspectorPanel_Particles.cpp:53](../../src/editor/panels/scene/InspectorPanel_Particles.cpp#L53))

- Combo `emissionShape` — escribe `em.emissionShape`. Resto cubierto.

**Patrón requerido**: `EditPropertyCommand<u32>` (mismo precedente que Light type / RigidBody shape).

### 🟡 Trigger ([InspectorPanel_Misc.cpp:107-111](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L107))

- `triggersOnPlayer`, `oneShot`, `enabled` Checkboxes — todos escriben directo.

**Patrón requerido**: 3x `pushEditIfDone<bool>`. Trivial.

### 🟡 ForceField ([InspectorPanel_Misc.cpp:131-197](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L131))

- Combo `shape` ([:131](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L131))
- Combo `mode` ([:159](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L159))
- Checkboxes `linearFalloff` ([:174](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L174)), `ignoreMass` ([:194](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L194)), `enabled` ([:197](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L197))

**Patrón requerido**: 2x `EditPropertyCommand<u32>` para combos + 3x `pushEditIfDone<bool>` para checkboxes.

### 🟡 Cloth ([InspectorPanel_Misc.cpp:234-282](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L234))

- SliderInt `res_x` ([:234](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L234))
- SliderInt `res_y` ([:240](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L240))
- Combo `anchor` ([:249](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L249))
- Checkbox `useGravity` ([:282](../../src/editor/panels/scene/InspectorPanel_Misc.cpp#L282))

**Patrón requerido**: 2x `pushEditIfDone<u32>` + 1x `EditPropertyCommand<u32>` para combo + 1x `pushEditIfDone<bool>`.

## 4. Gaps del MultiEditTracker

[MultiEditTracker.h](../../src/editor/panels/scene/MultiEditTracker.h) hoy solo soporta `std::vector<f32>` y `std::vector<glm::vec3>` en el variant `before`. Para paridad con single-edit faltan:

- `std::vector<bool>` — para multi-edit de checkboxes (Light.enabled, Trigger.*, Cloth.useGravity, etc.).
- `std::vector<u32>` — para multi-edit de combos (Light.type, RigidBody.shape, ParticleEmitter.emissionShape, etc.) y AssetIds (AudioSource.clip).
- `std::vector<glm::vec4>` — para multi-edit de ColorEdit4 (ParticleEmitter.colorStart/End).
- `std::vector<std::string>` — para multi-edit de InputText (Trigger.requiredTag, Tag.name).

Sin esto, los gaps fixedos en single-edit no se beneficiarán de multi-edit cuando hay N entidades seleccionadas.

## 5. Plan de fixes (orden de impacto)

1. **`MultiEditTracker` extension** — agregar `vector<bool>`, `vector<u32>`, `vector<vec4>`, `vector<string>` al variant. Sin esto, los fixes de checkboxes/combos del paso 2 solo trabajarán en single-entity.
2. **Light** (3 gaps triviales) — `enabled`, `direction`, `castShadows`. Mismo patrón que `multiEditColor3/multiEditDragFloat` del mismo archivo.
3. **Trigger + ForceField + Cloth + ParticleEmitter** — 11 gaps de checkbox/combo trivales (`pushEditIfDone<bool>` + `EditPropertyCommand<u32>`).
4. **AudioSource** combo `clip` — trivial.
5. **MeshRenderer** combo `shader graph` — trivial.
6. **Environment** — 14 gaps (combos, checkboxes, 6 reset buttons + sky picker). Reset buttons probablemente con snapshot batch command.
7. **Script** — 6 widgets, requiere extender `EditScriptComponentCommand` para overrides map o usar `EditPropertyCommand<map>`.
8. **Inventory** — diseñar `EditInventoryStateCommand` (snapshot-based, igual que `EditBrushUVCommand`).
9. **Vehicle** — diseñar `EditVehicleConfigCommand` (mutación de `VehicleConfig*` con flag `dirty=true` re-aplicado por undo/redo). El "Aplicar preset" debería ser 1 sólo command, no 10.

## 6. Cobertura de tests

Suite actual `1087/11182` (post-deferreds break). Tests de regresión a agregar — 1 happy-path por panel-componente fixedo:

```
[Inspector_Undo] Light.enabled toggle is undoable
[Inspector_Undo] Light.direction DragFloat3 is undoable
[Inspector_Undo] Trigger 3 checkboxes are undoable
[Inspector_Undo] ForceField 2 combos + 3 checkboxes are undoable
[Inspector_Undo] Cloth resX/resY SliderInt + anchor combo + useGravity checkbox
[Inspector_Undo] ParticleEmitter emissionShape combo
[Inspector_Undo] AudioSource clip combo
[Inspector_Undo] MeshRenderer shaderGraphPath combo
[Inspector_Undo] Environment 8 combos + 5 checkboxes + 6 resets
[Inspector_Undo] Script path + 4 exposed types + reset overrides
[Inspector_Undo] Inventory mode + capacity + entries + clear
[Inspector_Undo] Vehicle configPath + preset + 11 live tuning DragFloats
```

Target: `+12` tests, suite `1099` aprox.

## 7. Riesgos identificados

- **Vehicle live tuning** edita asset compartido (`VehicleConfig*` del AssetManager) — un undo después de cerrar la entidad podría afectar a otras instancias que usaban el mismo config. Mitigación: dejar claro que la edición no se persiste (ya lo dice el TextDisabled línea 151), y el undo opera sobre el config in-memory (no toca disco).
- **Environment skybox/LUT picker** — el file dialog abre un picker fuera de ImGui (pfd::open_file). El estado del Inspector entre apertura y selección es indeterminado para el tracker. Mitigación: capturar `before` *antes* de llamar al picker (sync), aplicar after sólo si user picked algo.
- **Reset buttons multi-field** — 1 click reasigna 3-7 campos. Si cada uno empuja un command, Ctrl+Z requiere N pulsaciones para deshacer un reset. Mejor batch command (1 click = 1 command que captura before/after del subset).
- **Inventory.equipment_slots** es un `std::vector` editable in-place (add/remove/edit). No encaja con `EditPropertyCommand<T>` (que asume valor copiable simple). Snapshot-based (igual que `EditBrushUVCommand`) es el patrón correcto.
