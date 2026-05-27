# PLAN F3H18 — Validador de assets rotos

**Estado:** ✅ **CERRADO** (`v2.18.0-fase3-hito18`, 2026-05-26).
**Predecesor:** F3H17 (drag & drop con feedback visual).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Validador de assets rotos".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 ✅ — Drag & drop con feedback visual
F3H18 ✅ — Validador de assets rotos
F3H19 –  — ⬅ próximo: Rename con cascada
```

---

## Lo que entregó

### `AssetValidator` engine

Subsistema nuevo en `src/engine/assets/validation/`:
- **`AssetValidator.h`**: API + structs (`IssueKind {BrokenRef, LoadFailed}`, `AssetIssue {kind, assetPath, detail, usedBy, entity}`, `validateProject(Scene&, const AssetManager&)`).
- **`AssetValidator.cpp`**: implementación del escaneo lateral (~210 LOC).

**Cobertura Tier 1** (decisión D1 + recomendación al dev en plan stub):
- **String paths** chequeados contra disco vía `assets.resolvePath(path) + std::filesystem::exists`:
  - `ScriptComponent.path`
  - `DialogComponent.dialogPath`
  - `ItemPickupComponent.itemPath`
  - `VehicleComponent.configPath`
  - `EnvironmentComponent.skyboxPath` (con heurística especial: equirect `<base>.png` O cubemap dir `<base>/px.png`, replicando `SceneRenderer::loadSkyboxAndIblFromBase`).
- **AssetIds indirectos** vía `pathOf(id)` cuando el resultado no es sentinel `__missing_X`:
  - `AudioSourceComponent.clip`
  - `MeshRendererComponent.mesh + .materials`
  - `AnimatorComponent.externalClips`
  - `ParticleEmitterComponent.texture`
  - `BrushComponent.materials`
- **Materials cargados**: recorre `materialCount() > 0` slots, chequea `albedo`/`metallicRoughness`/`normal`/`ao` texture refs.

**Sentinels skipped**: paths que arrancan con `__` (ej. `__default_material`, `__tex#<id>`, `__runtime#<id>`, `__missing_cube`) son referencias internas legítimas — no reportar.

**Detalles como i18n keys**: el `detail` del `AssetIssue` NO es texto traducido — es la i18n key (ej. `"editor.asset_validator.detail.script"`). El engine layer NO depende de I18n; el panel UI resuelve a runtime via `I18n::T(detail)`. Decision D2.

### `AssetIssuesPanel` UI

Nuevo panel en `src/editor/panels/project/`:
- **`AssetIssuesPanel.h`**: clase `IPanel` (category `"Project"`, default `visible=false`). Cache local de `m_issues`. State `m_pendingSelect` (request go-to entity, single-frame consume) + `m_refreshRequested` (request scan).
- **`AssetIssuesPanel.cpp`** (~150 LOC):
  - Tabla 4 columnas: Tipo (icon coloreado) / Asset (path + detail debajo) / Usado por (entity tag o "Material: <path>") / Acción (botón "Ir a").
  - Icons: `ICON_FA_LINK_SLASH` rojo para BrokenRef, `ICON_FA_TRIANGLE_EXCLAMATION` ambar para LoadFailed (LoadFailed reservado para Tier 2 futuro).
  - Empty state: `ICON_FA_CIRCLE_CHECK` verde + texto "Sin problemas detectados".
  - 2 iconos nuevos agregados al subset curado FontAwesome (`IconsFontAwesome6.h`): `ICON_FA_LINK_SLASH` (0xF127) + `ICON_FA_CIRCLE_CHECK` (0xF058).

### Badge en MenuBar

Chip rojo `! N` insertado entre el botón Play/Stop y el selector de workspace (`MenuBar.cpp:283-300`). Visible solo cuando `assetIssues().issueCount() > 0`. Click abre el panel. Tooltip al hover. Color rojo saturado (`vec4(0.62, 0.22, 0.22)`).

### Inspector inline broken-ref highlight

Helper `detail::inspectorBrokenRefBorder(EditorUI* ui, Entity e, const std::string& path)` en `InspectorPanel_Internal.h:127-141`. Llamado tras un widget drop target / InputText: si el `AssetIssuesPanel` tiene un issue con esa `(entity, path)`, pinta borde rojo 2 px sobre el último item + tooltip explicando el problema.

**Aplicado en F3H18** (2 sites):
- `InspectorPanel_Script.cpp:34` — InputText del script path.
- `InspectorPanel_Vehicle.cpp:175` — InputText del configPath del vehicle.

**Diferidos** (backlog para extender en hitos siguientes): MeshRenderer material slots, Animation clip slot, Inventory item slot, Audio clip combo, Dialog/Item drop target en el wrapper genérico del Inspector.

### Wire en EditorApplication

- `tryOpenProjectPath` (`EditorProjectActions_FileIO.cpp:204`): post-load refresh del panel para que el badge aparezca al abrir.
- `pumpUiRequests` (`EditorApplication_Run.cpp:223`): consume `refreshRequested()` (cuando dev clickea Refresh) + `consumePendingSelect()` (cuando dev clickea Ir a) → llama `setSelectedEntity`.

### Tests headless

`tests/test_asset_validator.cpp` (7 casos, ~150 LOC):
1. Proyecto sin refs → sin issues.
2. ScriptComponent.path inexistente → 1 BrokenRef con path correcto + entity correcta + usedBy = tag.
3. ScriptComponent.path que existe → sin issues.
4. ScriptComponent.path vacío → sin issues (no es ref muerta, es no-ref).
5. DialogComponent + ItemPickupComponent rotos → 2 issues con orden estable (dialog antes que item).
6. `AssetIssue.detail` es la i18n key estable (contrato con el panel).
7. VehicleComponent.configPath roto → BrokenRef con detail correcto.

Suite **1187/11499 verde** (+7 cases / +17 asserts vs F3H17).

### i18n

26 keys nuevas en `es.json` + `en.json`:
- 11 detalles (`editor.asset_validator.detail.*`): uno por tipo de ref roto.
- 11 keys del panel (`editor.panel.asset_issues.*`): título, empty state, columnas, botones.
- 1 tooltip del badge en menubar.
- 1 tooltip del inline broken-ref del Inspector.
- 2 keys ya tenían pero el panel las reusa.

---

## Sites tocados

```
src/engine/assets/validation/AssetValidator.h                NEW
src/engine/assets/validation/AssetValidator.cpp              NEW
src/editor/panels/project/AssetIssuesPanel.h                 NEW
src/editor/panels/project/AssetIssuesPanel.cpp               NEW
src/editor/ui/IconsFontAwesome6.h                            +2 icons
src/editor/ui/EditorUI.h                                     panel registration + accessor
src/editor/ui/EditorUI.cpp                                   panel pointer en m_panels
src/editor/ui/MenuBar.cpp                                    badge ! N + tooltip
src/editor/panels/scene/InspectorPanel_Internal.h            inspectorBrokenRefBorder helper
src/editor/panels/scene/InspectorPanel_Script.cpp            broken ref border en sc.path
src/editor/panels/scene/InspectorPanel_Vehicle.cpp           broken ref border en veh.configPath
src/editor/application/EditorApplication_Run.cpp             pumpUiRequests refresh+select
src/editor/application/EditorProjectActions_FileIO.cpp       refresh post-open project
tests/test_asset_validator.cpp                               NEW
tests/CMakeLists.txt                                         +test + +AssetValidator.cpp
CMakeLists.txt                                               +AssetValidator.cpp + +AssetIssuesPanel.cpp
assets/i18n/es.json                                          +26 keys
assets/i18n/en.json                                          +26 keys
```

---

## Decisiones

**D1 — Cobertura inicial Tier 1 (broken refs only) vs ampliada (schema mismatch + oversized files).**
Decisión vía `AskUserQuestion`: Tier 1. Cubre el 90% del caso (renombrar / mover / borrar externo). Schema mismatch requeriría schema versioning + upgrader index — scope hito propio. Oversized files es policy decisional (¿qué cap?) sin demanda clara — backlog. Lo que vale para F3H18 es el flujo end-to-end: scanner → panel → badge → highlight inline. Cobertura ampliada hereda esa infra cuando emerja.

**D2 — Engine `AssetValidator` agnóstico a i18n.**
El `AssetIssue.detail` es una i18n key (string como `"editor.asset_validator.detail.script"`), no el texto traducido. Razones: el engine layer (`src/engine/`) no debe depender de I18n (capa core/i18n + editor). El panel UI resuelve la key con `I18n::T(detail.c_str())` a runtime. Beneficio: el validator es reusable por MoodPlayer u otros frontends (CLI tooling, headless validators de CI) sin trabar el binario al diccionario i18n del editor. Costo: el panel debe llamar `I18n::T` por cada issue — N pequeño (típicamente <20), no es bottleneck.

**D3 — Scan on-demand (refresh manual + post-open) vs scan continuo.**
El validador se ejecuta SOLO al abrir proyecto + cuando el dev clickea "Refrescar" en el panel. NO se ejecuta cada frame ni en background polling. Razones: (a) `O(entities + materials)` típico < 5 ms — aceptable de pagar 1 vez, no 60/s; (b) si el dev rompe una ref en vivo (edita InputText), el badge se actualizará en el próximo refresh — costo: el badge puede quedar stale unos segundos hasta que el dev presione Refresh o re-abra el proyecto. UX trade-off explícito: prefiero badge stale a runtime overhead permanente. Hito futuro puede agregar invalidate selectivo (al editar un InputText path, marcar dirty + refresh next frame).

**D4 — Helper inline solo en 2 sites (Script + Vehicle) en F3H18, diferir el resto.**
El helper `inspectorBrokenRefBorder` es reusable en cualquier widget drop-target / InputText (MeshRenderer slots, Animation clip, Inventory item, Audio combo, etc). F3H18 lo instrumenta solo en Script + Vehicle (los más comunes en cuanto a refs rotas reportadas). Los otros sites quedan como follow-up — agregarlos es mecánico (1 línea por site) pero infla el diff del hito sin agregar capacidad nueva. La memoria `[[asset_validator_inline_coverage]]` agenda los diferidos.

**D5 — Reporte por entity, no por asset.**
El `AssetIssue` lleva la entity como source (cuando aplica) — el "Ir a" del panel selecciona la entity en Hierarchy + Inspector. Para issues de Material cacheado sin entity asociada (Material.albedo apunta a textura faltante), `entity` queda falsy y el botón "Ir a" se deshabilita con tooltip ("Esta referencia no pertenece a una entidad"). Razones: el dev típicamente repara desde la entity (cambia el path en el Inspector, o reemplaza el componente). Si el issue es del Material, el dev sabrá qué material editar por `usedBy = "Material: <path>"` y abrirá el Material Editor manualmente. Hito futuro puede agregar "Ir al asset" para casos sin entity.

---

## Validación

Dev confirmó "todo ok" tras testear:
1. Editor abre con welcome modal (sin issues badge si proyecto vacío).
2. Tras abrir un proyecto: si hay refs muertas, aparece chip rojo `! N` en menubar.
3. Click en chip → abre panel con tabla detallada.
4. Click "Ir a" → entity seleccionada en Hierarchy + Inspector.
5. Inspector Script con path inválido → InputText con borde rojo + tooltip.
6. Inspector Vehicle con configPath inválido → idem.
7. Botón Refrescar → re-escanea on-demand.

---

## Lo que NO toca F3H18

- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
- Schema mismatch / oversized files / shadergraph refs: cobertura ampliada — backlog si emerge.
- Inline broken-ref highlight en MeshRenderer/Animation/Inventory/Audio/Dialog/Item: mecánico, diferido.
- Auto-fix / "reemplazar todas las refs a X por Y": scope de F3H19.
- Refresh continuo / invalidate selectivo (badge sin stale): scope futuro si emerge UX.
- Reporte de issues en Prefabs cacheados (sin spawnear): los Prefabs viven en `AssetManager.prefabCount()`, su content es `SavedPrefab`. Walk de SavedPrefab.entities → componentes con refs es similar al walk de Scene — diferido a hito propio si emerge demanda.
