# Plan — F2H5: Virtualizacion del panel Hierarchy con ImGuiListClipper

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H4 cerrado), `docs/PERFORMANCE.md`
> (baseline post-F2H4 — `UI::draw` 19% del frame con 8336 entries),
> `DECISIONS.md` (entrada del swap F2H4↔F2H5 — F2H5 LOD original quedo
> postergado).

---

## Objetivo

Atacar el cuello dominante medido al cierre de F2H4: con 8336 entidades
(stress 100K), `UI::draw` se lleva el **19% del frame, mean 6.27 ms**. La
causa es el panel Hierarchy iterando con `scene.forEach<TagComponent>` y
emitiendo un `ImGui::Selectable` por cada una — sin virtualizacion, ImGui
calcula layout y dispatcha hover por las 8336 entries aunque solo ~30
sean visibles en el scroll area.

`ImGuiListClipper` resuelve esto en docena de lineas: el clipper sabe
cuantas entries caben en el viewport del panel y dispatcha el loop solo
sobre el rango visible.

**Speedup esperado** (basado en F2H4 baseline):
- `UI::draw` mean 6.27 ms → ~0.3 ms (solo render de las ~30 entries
  visibles), quedando bajo 2% del frame.
- Escena 100K total: 10.4 FPS → 12-15 FPS (recupera ~6 ms). El frame
  queda dominado por scene iteration en otros sistemas (proximo target
  post-F2H5).

**Scope explicito v1:**
- Solo el panel **Hierarchy**. AssetBrowser, Inspector, etc. quedan
  como estan — no son cuello hoy. Si emergen con escenas reales,
  se virtualiza con el mismo patron.
- Cache de la lista de entries (entity + tag pointer) construido al
  inicio del frame (`onImGuiRender`). Reutilizamos el vector con
  `clear()` + `reserve()` para no reallocar.
- Comportamiento visual identico: las entries se ven y se seleccionan
  igual que antes.

---

## Bloques

### A — Plan F2H5 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Refactor `HierarchyPanel` con `ImGuiListClipper`

`src/editor/panels/scene/HierarchyPanel.{h,cpp}`:

- Agregar miembro privado `std::vector<HierarchyEntry> m_entries;` donde
  `HierarchyEntry { entt::entity handle; const TagComponent* tag; }`.
  Cachear cada frame; reservar una vez con la capacidad observada.
- En `onImGuiRender`:
  1. Limpiar `m_entries` y rellenarlo con un solo `scene.forEach<TagComponent>`.
  2. Si `m_entries.empty()` → mostrar "Escena vacia" como antes.
  3. `ImGuiListClipper clipper; clipper.Begin(static_cast<int>(m_entries.size()));`.
  4. `while (clipper.Step()) { for (i in clipper.DisplayStart..End) {... }}`.
  5. Dentro del loop: el `PushID + Selectable + PopID` actual, pero
     accediendo via `m_entries[i]`.
- Helper extraido a funcion libre `collectHierarchyEntries(scene, out)`
  (puro, sin ImGui) para que un test pueda validar el orden / contenido.

### C — Tests del helper

Nuevo archivo `tests/test_hierarchy_collect.cpp`:

- Scene vacia → out vacio.
- Scene con 3 entidades con tag → 3 entries con los 3 tags.
- Entidad sin TagComponent → no aparece (consistente con el comportamiento
  actual del panel).
- Orden estable: dos llamadas seguidas dan el mismo orden (necesario para
  selection persistence).
- Reuso del vector: pasar un vector con datos previos, helper hace
  `clear()` antes de rellenar — no acumula entries entre llamadas.

Suite esperada: 340 → 345+ test cases.

### D — Re-medir baseline post-F2H5

1. Build Debug `MOOD_PROFILE=ON`.
2. Borrar CSV viejo, lanzar editor.
3. Snapshot CSV con labels:
   - `Empty` (control).
   - `100K_full_view` — esperado: FPS sube 10.4 → 12-15, `UI::draw` baja
     a <2% del frame.
   - `100K_no_view` — esperado: similar; el cuello del Hierarchy se
     materializaba en cualquiera de los dos casos (panel siempre visible).
4. Capturar `.tracy` durante un escenario, exportar con csvexport.
5. Comparar `UI::draw` antes/despues.
6. Actualizar `docs/PERFORMANCE.md` con columna post-F2H5.

### E — Documentacion + cierre

- Actualizar `docs/PERFORMANCE.md`: columna comparativa post-F2H5.
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "Virtualizacion ImGui
  panels con ListClipper — patron a aplicar a otros panels que crezcan".
- `docs/HITOS.md`: marcar F2H5 [x].
- `docs/ESTADO_ACTUAL.md`: seccion 1 reescrita con F2H5 cerrado, mover
  F2H4 a "anterior, ya cerrado". Proximo paso candidato = F2H6 LOD
  (cuando entre contenido de alto poly) o atacar el cuello restante de
  scene iteration en otros sistemas si emerge antes.
- Tag: `v1.1.3-fase2-hito5`.

---

## Suite

Cambio aditivo + refactor local. El test del helper agrega ~5 cases.
Visualmente identico (selection y orden no cambian). Suite esperada al
cierre: **345+/6720+** sin regresiones.

## Riesgos

- **Tag duplicado en entidades**: `Selectable` con texto duplicado
  funcionaba antes por el `PushID(handle)`. Mantenemos el `PushID(handle)`
  con virtualizacion — no hay regresion.
- **Selection persistence**: el `selectedEntity` del UI se compara por
  `Entity::operator==`. El cache no toca eso, asi que la seleccion
  sobrevive el clipper.
- **Scroll position**: `ImGuiListClipper` calcula el scroll basado en
  height de las entries. `Selectable` tiene altura uniforme, asi que
  el clipper funciona out-of-the-box.
- **Cache reuso**: usar `clear()` no reduce capacity; `reserve` solo
  crece si hace falta. Para escenarios con churn (entities crean/
  destruyen), el vector mantiene capacidad maxima — ~64KB para 8K
  entries. Aceptable.

---

## Criterios de cierre

- [ ] `HierarchyPanel` refactoreado con `ImGuiListClipper` + helper
      `collectHierarchyEntries` extraido y testeable.
- [ ] 5+ tests nuevos en `test_hierarchy_collect.cpp` pasando.
- [ ] Suite global sin regresiones (345+/6720+).
- [ ] `docs/PERFORMANCE.md` con columna post-F2H5 + speedup medido.
- [ ] `docs/DECISIONS.md` con entrada del patron de virtualizacion.
- [ ] `docs/HITOS.md` y `ESTADO_ACTUAL.md` actualizados.
- [ ] Tag `v1.1.3-fase2-hito5` pusheado.
