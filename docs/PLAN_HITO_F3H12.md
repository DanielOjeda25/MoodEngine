# PLAN F3H12 — Undo coverage audit del Inspector + fixes

**Estado:** **CERRADO** — `v2.12.0-fase3-hito12` (quinto hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos").
**Predecesor:** F3H11 (persistencia Audio/Camera + Brush refactor + ComponentClipboard Tier 3).
**Origen:** memoria de `ESTADO_ACTUAL.md`: *"F3H12 — Pendiente decidir el bloque. PLAN_FASE3.md Sub-fase 3.2 menciona 'Undo coverage audit del Inspector' — auditoría sistemática de qué edits del Inspector están envueltos en commands undoables y cuáles caen al void."*

---

## Qué siente el usuario

**Hoy (post-F3H11):**
- Cobertura de undo del Inspector es alta vía helpers `detail::pushEditIfDone` y `detail::field*` para DragFloat/SliderFloat/ColorEdit3/InputText — pero invisible para el dev cuáles widgets son undoables y cuáles no. Inconsistente sin querer.
- Ctrl+Z silencioso en widgets puntuales: toggle `enabled` de una luz, cambiar `shape` de un ForceField, modificar el live tuning de un Vehicle, etc. El dev se pierde un edit fino y no puede volver.
- Vehicle Inspector tiene comentario explícito `// Inspector NO undoable v1` (deuda técnica de F2H82).

**Post-F3H12:**
- Todo widget editable del Inspector está cubierto por undo, con tests que validan el round-trip por tipo.
- Edits a assets compartidos (VehicleConfig) son undoables via setter que captura `assets+id`.
- Reset buttons del Environment generan **1 sola entrada** del HistoryStack — 1 Ctrl+Z revierte los 5 fields del fog reset.
- Cambio de preset del Vehicle genera **1 sola entrada** del HistoryStack — 1 Ctrl+Z revierte los ~10 fields del preset.
- Helpers nuevos (`multiEditCheckbox`/`multiEditCombo`/`multiEditColor4`/`pushAtomicEdit<T>`) disponibles para futuros panels que necesiten el patrón atómico (sin drag).

---

## Plan ejecutado (5 pasos)

### Paso 1 — Auditoría doc

[`docs/audits/F3H12_undo_coverage.md`](audits/F3H12_undo_coverage.md): recorrido widget-by-widget de los 15 `InspectorPanel_*.cpp`. Tabla por panel: widgets → ¿cubierto? → gap específico → patrón de fix sugerido.

### Paso 2 — Fixes en 8 paneles

**2a — Infra**: `MultiEditTracker` variant extendido con `bool`/`u32`/`vec4` (F3H8 solo tenía `f32`/`vec3`). 3 helpers nuevos en `InspectorPanel_Internal.h`:
- `multiEditCheckbox(mTracker, sTracker, ui, e, ..., setter)` — atómico, mismo patrón que `multiEditColor3` pero sin tracker drag.
- `multiEditCombo(mTracker, sTracker, ui, e, ..., items, count, setter)` — atómico.
- `multiEditColor4(mTracker, sTracker, ui, e, ..., setter)` — drag pattern como `multiEditColor3` con 4 componentes.
- `pushAtomicEdit<T>(ui, e, before, after, setter, label)` — single-entity atómico (cuando multi-edit no aplica, ej. Environment, Audio, Vehicle).

**2b — Light**: enabled (multiEditCheckbox) + direction (fieldDragFloat3) + castShadows (multiEditCheckbox). Removido `ImGui::Checkbox` directo en 2 sites.

**2c — Trigger/ForceField/Cloth/ParticleEmitter**:
- Trigger: 3 checkboxes (triggersOnPlayer/oneShot/enabled) → multiEditCheckbox.
- ForceField: 2 combos (shape/mode) → multiEditCombo + 3 checkboxes (linearFalloff/ignoreMass/enabled) → multiEditCheckbox.
- Cloth: 2 SliderInt (resX/resY con dirty=true en setter) → pushEditIfDone<u32>; combo anchor → multiEditCombo; useGravity → multiEditCheckbox.
- ParticleEmitter: combo emissionShape → multiEditCombo.

**2d — AudioSource + MeshRenderer**:
- AudioSource: `clip` combo es `BeginCombo + Selectable` (no `ImGui::Combo`) → no encaja con `multiEditCombo`. Fix inline con `EditPropertyCommand<u32>` directo en el Selectable click (setter resetea `started=false` igual que la asignación manual pre-F3H12).
- MeshRenderer: combo `shaderGraphPath` → `EditPropertyCommand<std::string>` con setter capturando `assets+matId` (mismo patrón que albedoTint/metallic/etc del archivo). Signatura de `drawMaterialShaderGraph` extendida con `MaterialAssetId matId`.

**2e — Environment**: 8 combos + 5 checkboxes inline + 6 reset buttons batch.
- Combos simples (fog mode, tonemap): `pushAtomicEdit<u32>`.
- Combos `BeginCombo+Selectable+file picker` (skybox preset, color grading preset): `pushAtomicEdit<std::string>` en cada Selectable + en el resultado del `pfd::open_file`.
- Checkboxes (bloomEnabled/ssaoEnabled/colorGradingEnabled/ssrEnabled): `pushAtomicEdit<bool>` con `before = !env.X` (Checkbox YA toggled).
- **Reset buttons** (6 secciones: fog/tonemap/bloom/ssao/csm/cgrade/ssr): command custom **file-local** `EditEnvironmentSubsetCommand` con `applyBefore` (snapshot del subset) + `applyAfter` (defaults). 1 click = 1 entrada del HistoryStack que revierte los 3-7 fields de la sección.

**2f — Script**:
- `path` InputText → `pushEditIfDone<std::string>` con setter que también resetea `loaded`/`lastError`.
- Exposed properties (Number/Bool/String/Vec3): cada tipo con su `pushEditIfDone<T>` / `pushAtomicEdit<bool>`. Setter captura `prop.name` por valor + resetea `loaded`/`lastError`.
- `Reset` SmallButton del override: **sin undo en F3H12** (re-editar el slider restaura, los exposed default vienen del script Lua — no hace falta snapshot). Comentario explícito en código como follow-up si el dev lo reclama.

**2g — Inventory**:
- Tipados: mode combo (`pushAtomicEdit<u32>`), max_items/grid_w/grid_h InputInt (`pushEditIfDone<u32>`), equipment slot name/tag InputText (`pushEditIfDone<std::string>`), entry qty/slot_index (`pushEditIfDone<u32>`). Setters capturan el `i` (índice) por valor con guard.
- Estructurales (add/remove slot, add/remove entry, drop, clear): **sin undo en F3H12** + comentario explícito como follow-up. Snapshot-based command (`EditInventoryStateCommand`) sería el approach correcto pero out-of-scope.

**2h — Vehicle**: el más grande. Command custom file-local `EditVehicleConfigCommand` (snapshot del config completo, captura `assets+configId`, marca `dirty=true` en undo/redo).
- `configPath` InputText (EnterReturnsTrue) + drop target → `pushAtomicEdit<std::string>`.
- Combo preset → `EditVehicleConfigCommand` con `before = *cfg`, `after = preset aplicado`. 1 Ctrl+Z revierte los ~10 fields del preset.
- 11 DragFloats live tuning → `pushEditIfDone<f32>` cada uno con setter que captura `assets+configId` + `dirty=true`. Helper local file-local `makeSetter(fieldSet)` para reducir boilerplate.
- 2 friccion (longitudinal/lateral, aplican a 4 wheels) → `pushEditIfDone<f32>` con setter que itera `for (auto& w : c.wheels) w.X = v`.
- CoM local (vec3) → `pushEditIfDone<glm::vec3>` con setter dedicado.
- Comentario "Inspector NO undoable v1" del header removido + nota explicando el cambio.

### Paso 3 — Tests de regresión unit

[`tests/test_f3h12_inspector_undo.cpp`](../tests/test_f3h12_inspector_undo.cpp): 7 casos cubriendo round-trip de los tipos nuevos. `EditPropertyCommand<bool/u32/std::string>` + `MultiEditPropertyCommand<bool/u32/glm::vec4>`. Validan que la mecánica execute/undo es correcta — no testean UI (toggle + Ctrl+Z visual va en validación visual).

### Paso 4 — Validación visual (2 rondas)

- **Ronda 1 (post 2b-2d)**: dev confirma "todo OK" en Light toggle/multi-edit, ForceField combo, Audio clip combo.
- **Ronda 2 (post 2e-2h)**: dev confirma "todo OK" en Environment reset batch, Vehicle preset combo + live tuning, Script exposed.
- **Discovery del dev**: gaps de UX no relacionados a F3H12 — ForceField/Cloth no spawnables desde UI; meshes sin workflow "agregar sonido al activar". Anotados en memoria `backlog-ux-gaps-editor` para Sub-fase 3.3/3.4.
- **Discovery operativo**: el editor crashea si se lanza con cwd=sandbox (busca fonts en `<cwd>/assets/ui/`). Fix: cwd=repo, el proyecto se carga via File→Open Project. Memoria `project_sandbox_demo_f2h67` actualizada con la regla.

### Paso 5 — Cierre

- HITOS.md entry + DECISIONS.md (4 decisiones) + ESTADO_ACTUAL.md actualizado.
- PLAN_HITO_F3H13.md creado.
- Suite **1180/11530 verde** (+7 cases / +24 asserts vs F3H11).
- Tag `v2.12.0-fase3-hito12`.

---

## Decisiones (ver DECISIONS.md 2026-05-26 para detalles)

- **D1**: `pushAtomicEdit<T>` helper separado de `pushEditIfDone<T>` — semánticas atómico vs drag son distintas, mezclarlas confunde.
- **D2**: Commands custom file-local (`EditEnvironmentSubsetCommand` + `EditVehicleConfigCommand`) vs agregar a `commands/` — uso file-local exclusivo, no promover a global namespace sin reuso.
- **D3**: Scope acotado en Inventory — operaciones estructurales sin undo, diferidas a hito propio si el dev lo reclama. Re-aplicar add/remove de slot/entry es trivial vs diseñar un command nuevo.
- **D4**: Multi-edit innecesario en Environment — single-entity típico (1 EnvironmentComponent por escena, convención Unity/Unreal).

---

## Lo que NO se tocó (out-of-scope)

- Operaciones estructurales del Inventory (add/remove slot/entry, drop, clear) — sin undo.
- Reset SmallButton del override en Script — sin undo (re-editar restaura).
- Animator: alias InputText, Play/Remove buttons, drop AnimClip — el panel queda como estaba pre-F3H12 (ya tenía cobertura básica en speed/playing/loop/clipName combo).
- Multi-edit en Trigger/ForceField/Cloth/ParticleEmitter/AudioSource/MeshRenderer combo — los helpers están disponibles pero los call-sites de F3H12 usan single-entity por brevedad. Promover si el dev quiere multi-edit en esos componentes.
- Backlog de UX externo a F3H12: spawn de ForceField/Cloth desde UI + workflow "agregar sonido al mesh" — anotados en memoria, diferidos a Sub-fase 3.3/3.4.

---

## Métricas

- 8 paneles del Inspector con undo coverage extendida.
- 2 commands custom file-local (`EditEnvironmentSubsetCommand`, `EditVehicleConfigCommand`).
- 4 helpers nuevos en Internal.h (`multiEditCheckbox`, `multiEditCombo`, `multiEditColor4`, `pushAtomicEdit`).
- 3 tipos nuevos al variant del `MultiEditTracker` (`bool`, `u32`, `glm::vec4`).
- 7 tests nuevos (+24 asserts). Suite 1180/11530 verde.
- 1 memoria nueva (`backlog-ux-gaps-editor`), 1 actualizada (`project_sandbox_demo_f2h67`).
