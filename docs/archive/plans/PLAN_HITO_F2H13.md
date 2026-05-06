# Plan — F2H13: Multi-selección estilo Blender / Hammer

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H12 cerrado),
> `PLAN_FASE2.md` sección 4. Este hito **adelanta** lo que el plan
> original tenía como F2H15 ("Brush selection y multi-edit") al
> tercer puesto de sub-fase 2.2 — el feedback del dev al validar
> F2H12 fue que el flow del menú boolean con combobox era awkward
> y lo correcto es **selección visual primero**, ops después. Las
> primitivas extendidas (cilindro/esfera/etc) bajan a F2H14, el
> resto se renumera +1.

---

## Objetivo

Migrar el editor de **selección singular** (`Entity m_selectedEntity`)
a **multi-selección** estilo Blender/Hammer: un conjunto de N
entidades seleccionadas + una "active" (la última clickeada,
visualmente destacada). Click resetea, **Shift+click** acumula,
**Ctrl+click** togglea. La active es el target de Inspector y
Gizmo. Las ops booleanas se reescriben para operar sobre la
selección completa.

**Lo que entra en F2H13**:
- Modelo `SelectionSet` puro testeable.
- Multi-click en Hierarchy panel y Viewport picking.
- Outline visual diferenciado active vs selected (debug renderer).
- Inspector + Gizmo + Boolean ops adaptados.
- Tests unitarios del modelo + tests de regresión de los flujos
  que dependen de selección singular (DeleteEntityCommand,
  EditPropertyCommand, etc.).

**Lo que NO entra en F2H13**:
- Selección de cara individual (sub-modo Face Mode) — F2H16 del
  plan renumerado.
- Box-select / lasso-select en el viewport — diferido a hito
  futuro si emerge necesidad.
- Multi-edit "type" (editar propiedad de N entidades a la vez en
  el Inspector) — diferido. F2H13 muestra solo la active en el
  Inspector.
- Visgroups / Layers — F2H17 renumerado (era F2H16 original).

---

## Filosofía de diseño

- **Back-compat first**: la mayoría del código del editor usa
  `ui.selectedEntity()` para acceder a la entidad activa
  (Inspector, gizmo, viewport overlays, comandos). Mantener ese
  método con la semántica "devuelve la `active`" — cero refactor
  cascada.
- **Modelo puro testeable**: `SelectionSet` es un struct + helpers
  libres en `editor/selection/` (carpeta nueva), sin acoplamiento
  a EnTT más allá del `Entity` opaque handle. Tests unitarios
  cubren todas las transiciones (click, shift+click, ctrl+click,
  remove de entidad destruida, etc.).
- **Visual distinto pero discreto**: el active no debe taparse a
  sí mismo con un outline ruidoso. Outline tenue (gris claro)
  para selected, outline brillante (naranja) para active.
  Implementación con debug renderer (líneas) — sin tocar shaders.
- **Ops que escalan a N**: cualquier comando que hoy opere sobre
  "la entidad seleccionada" se adapta a "operar sobre el set
  seleccionado". Boolean ops, delete, duplicate, etc.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Carpeta del modelo | `src/editor/selection/SelectionSet.h` (header-only por ahora; `.cpp` si emerge complejidad) |
| 2 | Estructura | `struct SelectionSet { std::vector<Entity> selected; Entity active; }` con helpers libres `add(set, e)`, `remove(set, e)`, `toggle(set, e)`, `clear(set)`, `setActive(set, e)`, `contains(set, e)` |
| 3 | Invariante | Si `set.active` no es default, debe estar en `set.selected`. Si `set.selected.empty()` entonces `set.active == Entity{}`. Helpers garantizan el invariante o son no-op si la operación lo violaría. |
| 4 | Back-compat | `EditorUI::selectedEntity()` devuelve `m_selectionSet.active`. `setSelectedEntity(e)` resetea el set y pone `e` como active. Todo el código existente sigue funcionando. |
| 5 | Click semantics | Click → `clear` + `setActive(e)`. Shift+click → `toggle(e)` (si está → quitar; si no → agregar + setActive). Ctrl+click → `add(e)` + `setActive(e)`. |
| 6 | Detection del modifier | ImGui (`ImGui::GetIO().KeyShift` / `KeyCtrl`) en Hierarchy. SDL via flags expuestos por el viewport overlay (ya existe el patrón en `ViewportPanel`). |
| 7 | Render outline | Debug renderer (líneas, GL_LINES). 12 líneas por AABB world-space. Color tenue gris (selected) y naranja brillante (active). Render después del PBR pass, antes del particles. |
| 8 | Outline AABB world | Reusa `meshAabbWorld` / `brushAabbWorld` existentes (F2H11 los introdujo). Para entidades sin AABB (Light, Audio sin mesh), seguir el patrón actual del editor (icono sphere). |
| 9 | Inspector | Muestra solo la `active`. TextDisabled "X seleccionadas adicionales" si N>1 — no permite editarlas en F2H13 (multi-edit es diferido). |
| 10 | Gizmo | Opera sobre la `active` only en F2H13. Mover/rotar/escalar la active no afecta a las otras seleccionadas. Multi-edit del transform = hito futuro si emerge. |
| 11 | Boolean ops reescritas | Con N≥2 seleccionados: "Mapa > Boolean > Subtract" → el `active` es el "tool brush" B; los demás seleccionados ≠ active son las A's; aplica subtract en cascada. Si N=1 → disabled. Si N=0 → disabled. Sin combobox. |
| 12 | Comando Delete | El `DeleteEntityCommand` ya soporta múltiples entities desde Hito 27 (su API toma `std::vector<Entity>`). F2H13 solo cambia el callsite del menú "Editar > Eliminar" para pasarle el set entero. |
| 13 | Persistencia | El SelectionSet **NO se persiste** entre sesiones. Al cargar un mapa, set vacío. Razón: la selección es estado UI volátil, igual que el scroll del Hierarchy. |

---

## Tipos públicos

```cpp
// editor/selection/SelectionSet.h
namespace Mood {

struct SelectionSet {
    std::vector<Entity> selected;
    Entity active{};  // default-constructed = sin active
};

// Helpers libres. Garantizan invariantes (active dentro de selected
// o Entity{} si selected vacio; sin duplicados en selected).

void clear(SelectionSet& set);

/// @brief True si `e` esta en `set.selected`.
bool contains(const SelectionSet& set, Entity e);

/// @brief Agrega `e` (si no estaba) y lo setea como active.
///        No-op si `e` es default-constructed.
void add(SelectionSet& set, Entity e);

/// @brief Quita `e` (si estaba). Si era el active, el nuevo active
///        es el ultimo elemento del set tras la remocion (o
///        Entity{} si quedo vacio).
void remove(SelectionSet& set, Entity e);

/// @brief Si `e` esta -> remove. Si no -> add + setActive.
void toggle(SelectionSet& set, Entity e);

/// @brief Patron click "limpio": clear + add(e).
void replaceWithSingle(SelectionSet& set, Entity e);

} // namespace Mood
```

---

## Bloques

### A — Plan F2H13 (este archivo)

Documento del hito. **Nace en `docs/`** y al cierre se mueve a
`docs/archive/plans/` junto con el commit final (patrón "commits
agrupados" — memoria del proyecto).

### B — `SelectionSet` puro + tests

Crear:
- `src/editor/selection/SelectionSet.h` (header-only).
- `tests/test_selection_set.cpp` (~15-20 cases):
  - `clear`: vacía + active=Entity{}.
  - `contains`: entity ausente / presente / Entity{}.
  - `add`: agrega y setea active; segunda llamada con misma entity
    no duplica pero sí re-setea active.
  - `add` con `Entity{}` es no-op.
  - `remove`: quita; si era el active, active = último o Entity{}.
  - `remove` de entity ausente es no-op.
  - `toggle`: 2 llamadas vuelve al estado inicial.
  - `replaceWithSingle`: clear + add atomic.
  - Invariante: tras cualquier op, si `set.selected.empty()` →
    `set.active == Entity{}`.
  - Invariante: si `set.active != Entity{}` → `contains(set, set.active)`.
  - Determinismo: misma secuencia de ops produce mismo estado.

Validar: tests verdes.

### C — Hierarchy panel multi-click

Modificar `src/editor/panels/scene/HierarchyPanel.cpp`:
- Reemplazar `ui.setSelectedEntity(e)` con lógica modifier-aware:
  - Plain click → `replaceWithSingle(set, e)`.
  - Shift+click → `toggle(set, e)`.
  - Ctrl+click → `add(set, e)` (ya garantiza setActive).
- Render: cada entry destaca con color de fondo distinto si es
  `active` (más fuerte) vs solo `selected` (mas tenue) vs ninguno.
- Update del scroll-into-view: solo la `active` (no todas).

`EditorUI`:
- Agregar `m_selectionSet`, `selectionSet()` getter.
- `setSelectedEntity(e)` ahora hace `replaceWithSingle(m_selectionSet, e)`.
- `selectedEntity()` devuelve `m_selectionSet.active`.

Validar: lanzar editor, multi-seleccionar entidades en Hierarchy
con Shift y Ctrl. Active visualmente distinto.

### D — Viewport picking multi-click + render outline

Modificar `EditorApplication::onViewportClick` (donde se llama
`pickEntity` + `setSelectedEntity`):
- Detectar Shift / Ctrl del frame actual via SDL key state (ya
  hay patrón en el editor para esto).
- Aplicar `replaceWithSingle` / `toggle` / `add` según.
- Si el click no pega en nada (`pickEntity` devuelve null) Y
  no hay modifier → `clear(set)`.

Outline visual:
- Nuevo helper `editor/overlay/SelectionOverlay.cpp` (o agregar
  al `EditorSceneOverlay.cpp` existente).
- Por cada entidad en el set: computar AABB world-space
  (reusa `meshAabbWorld` / `brushAabbWorld`).
- Dibujar 12 líneas (8 vertices del AABB → 12 aristas) con
  `m_debugRenderer->drawLine`. Color: `active` = `(1, 0.6, 0)`
  (naranja); resto = `(0.6, 0.6, 0.6)` (gris).
- Render entre PBR static pass y particles, dentro de `renderScene`.

Validar: lanzar editor, click en viewport con Shift / Ctrl,
ver outlines.

### E — Inspector + Gizmo + Boolean menu

Inspector (`InspectorPanel.cpp`):
- Sigue mostrando solo la `active` (no cambia nada del código
  existente porque `selectedEntity()` ya devuelve la active).
- Agregar al header: `TextDisabled("+%d seleccionadas adicionales", N)`
  si `selectionSet().selected.size() > 1`.
- Las ediciones del Inspector solo afectan la active. Multi-edit
  diferido.

Gizmo (`ViewportPanel`):
- Opera sobre la active. Sin cambios — ya usa `selectedEntity()`.

Boolean menu (`EditorUI::drawBooleanOpMenu`):
- **Reescribir** completamente. Eliminar el submenu cascading con
  combobox de B.
- Nueva semántica:
  - Habilitado solo si `set.selected.size() >= 2` y todos tienen
    `BrushComponent`.
  - Click "Subtract" / "Union" / "Intersect" → aplica al instante.
  - El `active` es el "tool brush" B. Los demás seleccionados son
    A's. Aplicar la op a cada A vs el mismo B (cascade).
  - Para Subtract: cada A ≠ active queda reemplazado por sus
    pedazos `subtract(A, active)`. El active se conserva tal
    cual (es la "herramienta", no la víctima).
  - Para Union: agarrar todos como un solo conjunto e iterar
    `unionOp` izquierda-asociativamente. Resultado: N brushes
    convexos cubriendo el espacio total.
  - Para Intersect: iterar `intersectOp` izquierda-asociativamente.
    Resultado: 0 o 1 brush.
  - Pushear `BooleanOpCommand` adaptado al nuevo modelo (puede
    requerir un `BooleanOpBatchCommand` que envuelva N ops).

`handleBooleanOp` se reescribe para tomar `kind` solamente (no `Entity B` —
sale del set).

Tests del nuevo flow: agregar a `test_csg_brush_ops.cpp` los casos
de cascade subtract / union sequencial.

Validar visual: spawn 3 brushes con overlap, Shift+click los 3
en hierarchy, Subtract → 2 de ellos se restan al active.

### F — Tests + cleanup

- Verificar que comandos existentes que dependen de selección
  singular siguen funcionando: `DeleteEntityCommand` (multi-target
  ya soportado), `EditPropertyCommand` (single-target — sigue
  operando sobre la active, OK).
- Run suite completa, fix regressions si las hay.
- `MEMORY.md`: actualizar `project_pending_menu_org.md` si hay
  cambios.

### G — Cierre + tag

- `docs/HITOS.md`: F2H13 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H13 cerrado, F2H12 a "anterior".
- `docs/DECISIONS.md` (2026-05-XX):
  - "SelectionSet header-only con helpers libres — modelo puro
    testeable, separa state de logic".
  - "Multi-edit del Inspector diferido — solo la active edita".
  - "Boolean ops cascade: active = tool brush; los demás = víctimas".
- Mover `docs/PLAN_HITO_F2H13.md` a `docs/archive/plans/`.
- Tag: `v1.4.0-fase2-hito13`.

---

## Suite

Cases nuevos esperados:
- `test_selection_set.cpp`: ~18-22 cases, ~60-80 asserts.
- `test_csg_brush_ops.cpp`: +5-7 cases del cascade.

**Suite estimada: 474 → ~500 cases / 7668 → ~7800 asserts.**

---

## Riesgos

- **Comandos del HistoryStack que capturan `Entity` por handle**:
  el `EditTransformCommand`, `EditPropertyCommand`, etc. capturan
  un `Entity` específico. Multi-edit cambiaría esto. F2H13 no lo
  toca — los comandos siguen siendo single-target operando sobre
  la active.
- **Active se vuelve inválido**: si la active se destruye (ej.
  por DeleteEntityCommand de la propia active), hay que limpiar
  el set. Mitigación: chequeo de validez en cada draw del
  Inspector / overlay (ya existe pattern en el código actual).
- **Performance del outline**: con 100K entidades seleccionadas
  son 1.2M líneas de outline. Aceptable porque el debug renderer
  ya hace batching y porque 100K seleccionadas no es un caso de
  uso real. Si emerge, usar fade-by-distance o limitar a primeras
  N.
- **Conflicto con interacciones existentes**: Shift es modifier
  de muchos atajos del editor. Verificar que Shift+click en
  Hierarchy / Viewport no interfiere con otros atajos.
- **Ctrl en macOS = Cmd**: el editor solo corre en Windows
  oficialmente, pero documentar para futuro.

---

## Criterios de cierre

- [ ] `src/editor/selection/SelectionSet.h` con helpers + invariantes.
- [ ] `tests/test_selection_set.cpp` ~18+ cases verde.
- [ ] HierarchyPanel + Viewport picking implementan multi-click
      con Shift/Ctrl semantics.
- [ ] Outline visual: gris (selected), naranja (active).
- [ ] Inspector muestra "+N adicionales" cuando hay multi-select.
- [ ] Gizmo opera sobre la active.
- [ ] Boolean ops menu reescrito sin combobox; opera sobre el set.
- [ ] Validación visual: 3 brushes con overlap, multi-select,
      subtract → cada víctima reemplazada por sus pedazos.
- [ ] Suite ~500 cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado.
- [ ] Tag `v1.4.0-fase2-hito13` pusheado.

---

## Refs canónicas

- Blender selection model: https://docs.blender.org/manual/en/latest/scene_layout/object/selecting.html — modelo de active vs selected con highlight diferenciado.
- TrenchBroom selection: https://trenchbroom.github.io/manual/latest/#selection — Shift+click acumula, Ctrl+click toggle, multi-select básico para CSG.
- Hammer 4 / Hammer 5: el "Selection Tool" con multi-click es el flujo de mapping clásico.
