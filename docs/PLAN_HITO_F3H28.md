# PLAN F3H28 — Grupos + Map Tools como categorías del Properties Editor

**Estado:** ✅ **CERRADO** — `v2.28.0-fase3-hito28` (2026-05-29).
**Predecesor:** F3H27 (parenting jerárquico — backend para la categoría Grupos).
**Origen:** Item 1 del stub original F3H27 (splitting confirmado por el dev tras F3H26: 4 items → 4 hitos separados).

---

## Sub-fase 3.4 (post-F3H28)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – ✅ Parenting jerárquico de transforms
F3H28 – ✅ Grupos + Map Tools como categorías del Properties Editor ⬅ este hito
F3H29 – 📝 Mundo grande (camera limits / far plane / reverse-Z) (stub)
F3H30 – 📝 HDRI dinámico + ciclo día/noche (stub)
```

---

## Origen

Cita textual del dev al cerrar F3H26:
> *"quiero mejorar esta parte de grupos que estan separados, y los maptools que esten mas insertados mas en menus como los que hicimos en layout, como los de blender, que hicimos hito atras"*

**Hallazgo clave del research previo a implementar** — el stub asumía paneles flotantes "Grupos" + "Map Tools" en el dock derecho, pero la realidad es distinta:
- "Grupos" flotante = `VisGroupsPanel` (F2H33, sistema **visibility-toggle de capas**, NO parenting).
- "Map Tools" flotante = `MapEditorTopBar` (F2H30, toolbar como panel ImGui-dockable).
- Toolbar lateral persistente no existía.

Esto cambió las decisiones cerradas en el stub (B1, C1) y agregó una colisión semántica: "Grupos" en VisGroupsPanel ≠ "Grupos" como Empty Group_<N> de F3H27. Se resolvió renombrando VisGroupsPanel.

---

## Norte

Concentrar 2 vistas en el chasis F3H22 del Inspector (sidebar con icons verticales):
1. **Grupos** = backend Empty-as-parent de F3H27, lista navegable + acciones rápidas.
2. **Map Tools** = configuración global del editor de mapas (sub-modo, herramientas, snap, labels, carve) — reemplaza el viejo MapEditorTopBar.

Más una limpieza de naming colisionante (`VisGroupsPanel` rename) y un reordenamiento del workspace map_editor por pedido del dev.

---

## Decisiones cerradas pre-implementación (vía AskUserQuestion)

**A1 — Toolbar lateral: NO existe.** El stub la asumía pero el research reveló que solo está la top bar de F2H30 (MapEditorTopBar). Decisión efectiva: el MapEditorTopBar se elimina y su contenido vive como categoría del Inspector. Decisión confirmada en C1.

**A2 — Top bar Object/V/E/F (F2H30): NO existe como entidad separada.** El research reveló que el MapEditorTopBar incluía esos sub-modos. Al eliminar MapEditorTopBar, los sub-modos van a la categoría Map Tools (sección "Sub-modo"). Atajos teclado 1/2/3/Esc preservados.

**A3 — Boolean ops (Subtract / Intersect) quedan SOLO en context menu del Outliner.** El dev ya validó esa ubicación en F3H26. Migrar a categoría agrega redundancia sin valor. No tocado.

**B1 — Backend de la categoría "Grupos" = Empty-as-parent de F3H27.** Reusar lo que ya implementamos: lista de Empty Group_<N> del mapa, acciones via comandos existentes (GroupSelectionCommand / UngroupSelectionCommand). Cero código nuevo de backend; UI puro. Memoria `no-reinventar-rueda`.

**B2 (implícita de B1)** — La categoría lista TODOS los Empty del mapa que tienen descendants (mismo filtro que el marker XYZ del overlay de F3H27 R7). Empties huérfanos no aportan click target útil, se ocultan.

**B3 (implícita de B1)** — NO multi-membership. Empty es single-parent por definición. Multi-membership requeriría sistema paralelo (VisGroups u otro) — fuera de scope.

**C1 — Panel viejo "Map Tools" (MapEditorTopBar): ELIMINAR del registry** (no ocultar). El dev fue explícito post-validación: borrar el código del panel y su entry de m_panels. Para no perder funcionalidad: el popover de snap settings se extrae a `SnapPopoverContent.{h,cpp}` como helper compartido; la categoría Map Tools incluye Clip tool + botón Ajustes con el popover migrado.

**C2 — Panel viejo "Grupos" (VisGroupsPanel = VisGroups F2H33): RENOMBRAR a "Visibilidad"** + mantener visible. Distingue claramente del nuevo concepto Empty-as-parent que ocupa el nombre "Grupos" en el Inspector. Sin pérdida funcional (el backend de VisGroups queda intacto). Migrar VisGroups también a categoría del Inspector queda como backlog si emerge demanda.

**D1 — Workspace map_editor: Viewport 3D en top-left** (pedido reactivo del dev mid-implementación: *"en el editor de mapas, me gustaría que el 3D este primero, de todos"*). Pre-F3H28 estaba en top-right (top-left era Top XZ orto). Reordenamiento mínimo: Viewport ↔ Top XZ. Inspector dockeable en columna lateral derecha (22% ancho); "Visibilidad" como tab al lado. Bump ini layout v8 → v9.

---

## Implementación

**Phase 1 — Backend de requests (EditorUI).**
- `EditorUI.h`: 2 declaraciones nuevas `requestGroupSelection() / consumeGroupSelectionRequest()` + `requestUngroupSelection() / consumeUngroupSelectionRequest()` + 2 flags privados.
- `EditorUI_Tools.inl`: 4 impls inline siguiendo el patrón de `requestToggleSnapToVertex`.
- `EditorApplication_Run.cpp`: handlers que consumen los requests y delegan a `groupSelectedEntities()` / `ungroupSelectedEntities()` existentes.

**Phase 2 — Chasis del Inspector (F3H22).**
- `InspectorPanel.h`: declarar `renderGroupsSection()` y `renderMapToolsSection()` (scene-wide, sin Entity param).
- `InspectorPanel.cpp`:
  - Nuevo `isGlobalCat = (activeCat == "groups" || activeCat == "maptools")` que evita el flow per-entity.
  - Early-return dispatch para categorías globales (dibuja categoría + EndChild + End + return).
  - 2 `categoryButton` nuevos en `renderCategoryBar` con `sceneWide=true` (siempre visibles).
  - Fallback de auto-switch a "object" actualizado para no triggear sobre groups/maptools.

**Phase 3 — Categoría Grupos.**
- `InspectorPanel_Groups.cpp` NEW (~130 LOC).
- Header descriptivo + 2 botones acción (Agrupar disabled si selección<2, Desagrupar disabled si ninguno tiene parent).
- Lista de Empties con descendants — iterar `forEach<TransformComponent>` excluyendo entities con Brush/Mesh/Light/Audio/Trigger/Camera/Particle, filtrar `descendantsOf(e).empty()`.
- Cada item Selectable con tag + count de hijos; click invoca `setSelectedEntity(e)`.

**Phase 4 — Categoría Map Tools + extracción del popover.**
- `SnapPopoverContent.{h,cpp}` NEW — extraído del difunto MapEditorTopBar como helper compartido. Sin cambios de lógica.
- `InspectorPanel_MapTools.cpp` NEW (~145 LOC):
  - 5 secciones con SeparatorText: Sub-modo / Herramienta (incluye Clip) / Snap (toggle V + botón Ajustes con popover) / Visualización (labels) / Acciones (carve).
  - Helper local `mapToolsToggleButton` para botones full-width con highlight estado activo.

**Phase 5 — Eliminación MapEditorTopBar.**
- `MapEditorTopBar.h/cpp` DELETED.
- `EditorUI.h`: quitar include + getter `mapEditorTopBar()` + miembro `m_mapEditorTopBar`.
- `EditorUI.cpp`: quitar del `m_panels` registry + `visible = false` + `setEditorUi`.
- `EditorUI::applyDefaultVisibilityForWorkspace` — quitar `setVisible("Map Tools", ...)` del hideOrthoPanels lambda y del workspace map_editor. Inspector pasa a `true` en map_editor para que las categorías sean accesibles.
- `Dockspace.cpp::buildMapEditorWorkspace`: actualizado a nuevo layout (ver D1).
- `CMakeLists.txt`: quitar entry de MapEditorTopBar.cpp; agregar SnapPopoverContent.cpp.

**Phase 6 — VisGroupsPanel rename "Grupos" → "Visibilidad".**
- `VisGroupsPanel.h`: cambiar `name() const override { return "Visibilidad"; }` + comentario explicando rename.
- `applyDefaultVisibilityForWorkspace`: las llamadas `setVisible("Grupos", ...)` siguen apuntando al label viejo (no encontrado tras rename) — quedaron como no-op silencioso. Workspace map_editor ahora usa `setVisible("Visibilidad", ...)` indirectamente via el nuevo Dockspace; Workspace switches preservan el visible flag previo.

**Phase 7 — Workspace map_editor reorganizado (D1).**
- `Dockspace.cpp::buildMapEditorWorkspace`:
  - `dockRightBar` ancho 0.10 → 0.22 (espacio para Inspector docked).
  - `Viewport` → `dockMain` (top-left, antes era `dockTopRight`).
  - `Top (XZ)` → `dockTopRight` (top-right, antes era `dockMain`).
  - `Inspector` + `Visibilidad` → `dockRightBar` (tabs).
- `EditorApplication_Init.cpp`: bump `imgui_layout_v8.ini` → `v9.ini` para forzar fresh layout (layouts v8 mostrarían "Map Tools" como ventana fantasma + Inspector flotante).

**Phase 8 — Iconos + i18n.**
- `IconsFontAwesome6.h`: 2 macros nuevas `ICON_FA_LAYER_GROUP` (0xF5FD) y `ICON_FA_SCREWDRIVER_WRENCH` (0xF7D9).
- `es.json` + `en.json`: ~28 keys nuevas bajo `editor.inspector.category.groups*` / `editor.inspector.category.maptools*` / `editor.inspector.groups.*` / `editor.inspector.maptools.*`.

---

## Tests

Sin tests nuevos. La implementación es UI puro sobre comandos backend ya testeados (F3H27: GroupSelectionCommand / UngroupSelectionCommand / SetParentCommand tienen sus tests; el chasis F3H22 del Inspector no tiene tests UI directos — convención del proyecto). Suite **1283/11841 verde** post-F3H28 (sin regresión).

---

## Backlog post-F3H28

- **VisGroups como tercera categoría del Inspector** — si el dev pide después, agregar "Visibilidad" como categoría siguiendo el mismo patrón. Hoy queda como panel flotante.
- **Snap config popover en lugar más prominente** — el botón Ajustes dentro de la categoría Snap está OK pero puede confundir; alternativa: SeparatorText collapsable inline con los sliders.
- **Migrar Asset Browser / Console / Profiler a categorías del Inspector** — mismo patrón, pero no pidió el dev. Hito propio si emerge demanda.
- **Custom user categories** — dev define agrupaciones propias de componentes. Fuera de scope.
- **Drag para reordenar categorías de la sidebar** — Blender lo permite, fuera de scope.

---

## Lo que NO toca F3H28

- F3H29 (mundo grande / camera limits / far plane / reverse-Z) — hito propio.
- F3H30 (HDRI dinámico + ciclo día/noche) — hito propio.
- Sistema de tags Unity-like (etiquetas string libres) — fuera de scope.
- Backend de VisGroups (F2H33) — el sistema sigue funcionando idéntico, solo el panel se renombró.
- Top bar F2H30 — ESPECÍFICAMENTE: el MapEditorTopBar fue eliminado completo (D1). Lo que queda intacto es el comportamiento (atajos 1/2/3 + W/E/R), no la UI vieja.
- Migrar physics/picking a `worldMatrixOf` (backlog F3H27).

---

## Diff summary

- 4 archivos NEW (`InspectorPanel_Groups.cpp`, `InspectorPanel_MapTools.cpp`, `SnapPopoverContent.h`, `SnapPopoverContent.cpp`).
- 2 archivos DELETED (`MapEditorTopBar.h`, `MapEditorTopBar.cpp`).
- ~12 archivos modificados (CMakeLists + i18n ES/EN + InspectorPanel.h/cpp + VisGroupsPanel.h + IconsFontAwesome6.h + EditorUI.h/cpp + EditorUI_Tools.inl + EditorApplication_Run.cpp + EditorApplication_Init.cpp + Dockspace.cpp).
- 8 decisiones cerradas (4 vía AskUserQuestion pre-implementación + 4 reactivas post-research/validación).
