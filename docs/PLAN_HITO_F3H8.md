# PLAN F3H8 — Multi-edit del Inspector (N entidades, valor común vs mixed)

**Estado:** Planeado (primer hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos", arranca tras `v2.7.0-fase3-hito7`).
**Predecesor:** F3H7 (cierre Sub-fase 3.1 — UserSettings > Editor migrado).
**Origen:** Plan maestro [`PLAN_FASE3.md`](PLAN_FASE3.md) sección Sub-fase 3.2 — *"F3H8 — Multi-select edita N entidades a la vez. Shift+click + Ctrl+click en Hierarchy. Inspector muestra valores comunes (o `—` para mixed). Editar aplica a todas."*

---

## Qué siente el usuario

El dev selecciona 3 cubos en el viewport (Shift+click acumula desde F2H23 iter 5). Abre el Inspector — hoy muestra **solo el active** y un texto "+2 adicionales seleccionadas" sin permitir editarlas en bulk (excepto Transform via gizmo).

Después de F3H8, el Inspector muestra cada field con uno de 2 estados:

- **Valor común**: las 3 entidades tienen el mismo color de luz → slider muestra ese color, editar aplica a las 3.
- **Mixed**: las 3 entidades tienen distintos colores → el control muestra "—" (em-dash) o un placeholder visual. Editar lo nuevo aplica a las 3 (las "rompe" del mixed pero el dev lo pidió).

Cambios son **undoable como un solo Ctrl+Z** (pattern del `MultiEditTransformCommand` de F2H23 iter 5).

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H8:**
- Detector de "valor común" por field (`InspectorEditTracker` actual ya trackea pre/post de una sola entidad; extender a `vector<Entry>` cuando `selection.size() > 1`).
- Indicador visual mixed: prefijo "—" o `TextDisabled("—")` antes del control. Decisión D1 abajo.
- Edición aplica a todas las entidades del SelectionSet — patrón heredado de F2H23 iter 5: snapshot pre del active + N otherStarts, en `IsItemDeactivatedAfterEdit` aplicar delta o asignación a todas + push `MultiEditPropertyCommand<T>`.
- Comandos nuevos: `MultiEditPropertyCommand<T>` (gemelo de `EditPropertyCommand<T>` pero con `vector<Entry>`). Generalizable a `f32`/`vec3`/`Color`/`bool`/`int`.
- Cobertura: 5-6 componentes principales (Transform ya existe; agregar Light, MeshRenderer materials, Audio, Particles, Trigger). Component-by-component en bloques.

**NO en F3H8:**
- Hierarchy: el multi-select ya existe (F2H13 SelectionSet + F2H23 iter 5 Maya-style modifiers). F3H8 no toca Hierarchy, solo Inspector.
- Multi-edit de strings (tag, name): los strings no tienen "valor común" útil — si las 3 entidades se llaman "Cube_01/02/03", mostrar mixed; si editás, asignar "Cube" a todas las renombra a duplicados → mal UX. Diferido a hito futuro.
- Multi-edit de assets (cambiar material o mesh en bulk): scope distinto — drag&drop sobre el Inspector con multi-select; F3H9 podría atacarlo (copy/paste de components).
- Component add/remove en bulk: el "+ Add Component" hoy aplica al active. Multi-add es scope distinto.

---

## Bloques

### A — Comando `MultiEditPropertyCommand<T>`
- Gemelo de `EditPropertyCommand<T>` (Hito 32 D) pero con `std::vector<Entry { Entity, T before, T after }>`.
- `execute`: por entrada, find entity por tag (no handle EnTT — robusto a delete/recreate), aplicar `after` via setter.
- `undo`: por entrada, aplicar `before`.
- `name()`: "Multi-edit {Field} ({N} entities)" en castellano para statusbar.

### B — Helper `multiEditField<T>` en `InspectorPanel_Internal.h`
- Extiende `fieldDragFloat`/`fieldColorEdit3` etc. (de F2H74) con awareness del SelectionSet.
- Si `selection.size() == 1`: comportamiento idéntico al `EditPropertyCommand<T>` actual (back-compat).
- Si `selection.size() > 1`:
  1. **Detector de valor común**: leer el field de las N entidades, comparar con epsilon (para f32) o `==` (para int/bool/string).
  2. **Display**: si todas iguales → mostrar el valor con el control normal. Si difieren → mostrar control con placeholder "—" + tooltip "Valores mixtos en N entidades".
  3. **Tracking**: `IsItemActivated` captura `vector<T> beforeValues` (N items). `IsItemDeactivatedAfterEdit` captura `vector<T> afterValues`. Si difieren → push `MultiEditPropertyCommand`.
- Tipos: `f32`, `glm::vec3` (colors + positions), `bool`, `int`.

### C — Aplicar el helper en N componentes
Inspector ya tiene 10 partials (F2H24 split). Migrar uno por uno:

**Tier 1 (mayor impacto, atacar primero):**
1. `InspectorPanel_Light` — color (vec3), intensity (f32), range (f32).
2. `InspectorPanel_MeshRenderer` — visible (bool), cast shadows (bool).
3. `InspectorPanel_Transform` — confirmar que el patrón actual de F2H23 iter 5 (gizmo + DragFloat3 con `applyDeltaToSelection`) se mantiene consistente con el nuevo helper.

**Tier 2 (si queda tiempo):**
4. `InspectorPanel_Audio` — volume (f32), pitch (f32), loop (bool).
5. `InspectorPanel_Particles` — emission rate (f32), color (vec3).
6. `InspectorPanel_Trigger` — required tag (string — display only, no multi-edit), triggers on player (bool), one-shot (bool), enabled (bool).

**Diferidos a hito siguiente:**
- Script path (string), Shader graph asset (string), Brush UV params (multi-cara per-entity).

### D — Indicador visual mixed
- Decisión D1 abajo: cómo mostrar "valores mixtos" sin agregar visual noise.
- Probable: placeholder en el control + tooltip al hover. Sin cambiar layout.

### E — Tests
- 6-8 tests del `MultiEditPropertyCommand<T>`:
  - 1 entity → equivalente al `EditPropertyCommand<T>`.
  - N entities mismo valor → execute aplica a todas, undo restaura.
  - N entities valores mixtos → execute homogeniza, undo restaura los N distintos.
  - Edge case: entity removida entre execute y undo → skip silencioso.
  - Redo idempotente.

### F — i18n
- 1 key nueva: `editor.inspector.multi_edit.mixed_values_tooltip`.
- 1 key nueva: `editor.inspector.multi_edit.applies_to_n` ("Aplica a {} entidades").

### G — Validación visual
- Seleccionar 3 cubos con luces de distinto color → Inspector muestra "—" en el slider color, hover muestra tooltip "Valores mixtos en 3 entidades".
- Cambiar color → las 3 luces se actualizan en vivo en el viewport.
- Ctrl+Z → las 3 luces vuelven a sus colores originales (un solo undo).
- Seleccionar 3 cubos con misma intensity → slider muestra el valor común, editar aplica a todos.

---

## Decisiones a tomar (pre-implementación)

### D1 — Cómo indicar "mixed values" visualmente

**Opciones:**
- **(a)** Placeholder "—" reemplaza el valor en el control (ej. SliderFloat muestra "—" en vez de "0.50"). Tooltip en hover explica.
- **(b)** Control normal con valor del active + ícono "≠" al lado + tooltip.
- **(c)** Pintar el control de un color tenue (gris cyan) cuando mixed + tooltip.

**Recomendación:** **(a)** — convención Unity/Unreal. El placeholder "—" es universal. Editar reemplaza el placeholder con el nuevo valor (homogeniza).

### D2 — Aplicar a las N o solo al active si el dev cambia mientras hay mixed

**Contexto:** si el dev tiene 3 entidades con color rojo/verde/azul y mueve el slider a amarillo, ¿las 3 se vuelven amarillas o solo el active?

**Decisión:** **las 3** — el dev seleccionó N entidades expresamente; editar en multi-select implica "aplicar a todas". El active es solo el primary de UI (resaltado), no un "modo singular dentro del multi-select".

**Justificación:** mismo patrón que Unity/Unreal/Godot. Cuando el dev quiere editar solo el active, debe deseleccionar los otros (click sin modifier → reemplaza con singular).

### D3 — Snapshot al inicio vs delta-aware

**Opciones:**
- **(a) Snapshot**: capturar `vector<T> beforeValues` al `IsItemActivated`, aplicar `afterValues` directos al `IsItemDeactivatedAfterEdit`. Undo restaura cada uno a su `before`.
- **(b) Delta**: capturar `T deltaValue = newValue - oldValueOfActive`, aplicar `before + delta` a cada entrada. Undo aplica `-delta`.

**Decisión:** **(a) Snapshot** — más simple, predecible, sin acumulación de error f32. El patrón delta es solo útil cuando el "valor común" no existe (ej. multi-edit de posiciones donde no querés mover todas al mismo X sino moverlas la misma cantidad).

**Excepción:** Transform position/rotation/scale ya usa delta (F2H23 iter 5 `applyDeltaToSelection`). F3H8 NO cambia Transform — el patrón existente sigue. Para los componentes nuevos (Light/Audio/etc) usar snapshot. Si emerge una excepción real, sub-hito.

### D4 — Component-by-component vs migración full-bang

**Opciones:**
- **(a) Migrar Tier 1 (Light + MeshRenderer + Transform) en F3H8**, los demás en hitos siguientes (F3H8.1, F3H8.2 o capturarlo en PENDIENTES.md).
- **(b) Migrar TODOS los componentes en F3H8** (Light + Audio + Particles + Trigger + MeshRenderer + más).

**Recomendación:** **(a)** — Tier 1 cubre los componentes que el dev edita con más frecuencia (luces, MeshRenderer). Tier 2 (Audio/Particles/Trigger) son comportamientos más "set-and-forget", multi-edit aporta menos. Migrar Tier 1 rinde 80% del valor con 50% del esfuerzo.

---

## Riesgos / a confirmar temprano

- **`InspectorEditTracker` actual**: revisar cómo está implementado (Hito 32 D) — si trackea pre/post por entity handle, hay que extender a `vector<entity>`. Posiblemente reescribirlo como template-friendly.

- **Tags como identidad para undo**: el `MultiEditPropertyCommand<T>` debe capturar `vector<EntityTag>` no `vector<entt::entity>` (mismo patrón que F2H16 cleanup). Si entre execute y undo el usuario borra una entidad con undo viejo, el handle queda stale.

- **Performance del comparador de valores comunes**: para `selection.size() = 50`, leer 50 colors y comparar f32-by-f32 con epsilon es ~150 floats — negligible. Si emerge fricción, cache por field-id.

- **vec3 con epsilon**: para colors usar epsilon 0.001f (1 LSB en 8-bit). Para positions epsilon 0.0001f (1mm). Distinción por field-key, no hardcode global.

---

## Tamaño estimado

Hito mediano (~4-6h, más grande que F3H4-F3H7 porque toca infra del Inspector + N componentes). Bloques A (comando) + B (helper) son foundation; C (aplicación) es repetitivo per-componente.

## Cierre del hito

- [ ] Suite verde (+6-8 tests nuevos de `MultiEditPropertyCommand<T>`).
- [ ] Tier 1 componentes (Light + MeshRenderer + Transform) con multi-edit funcional.
- [ ] Indicador "—" para mixed values + tooltip.
- [ ] Ctrl+Z agrupa la edición de N entidades como un solo undo.
- [ ] Validación visual end-to-end: 3 luces distintas → mixed → editar color → todas se igualan → Ctrl+Z restaura los 3 distintos.
- [ ] Tag `v2.8.0-fase3-hito8`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H9.md` (Copy/Paste de components).
