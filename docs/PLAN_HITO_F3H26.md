# PLAN F3H26 — Polish UX del editor (post-F3H25)

**Estado:** ✅ **CERRADO** (2026-05-28) — tag `v2.26.0-fase3-hito26`. Hito creado tras cerrar F3H25 cuando el dev pidió pulir 5 detalles de interacción + 1 conflict de hotkey + decidir el modelo CSG-convex de Union. Sin AskUserQuestion al arrancar — los items vinieron como feedback directo del dev sobre screenshots del editor.
**Predecesor:** F3H25 (Crash recovery + autosave).
**Origen:** feedback del dev al validar visualmente el cierre de F3H25. F3H25 era originalmente el último hito de Sub-fase 3.4 / Fase 3; el dev pidió insertar este polish + F3H27 (gaps de UX restantes) antes de Fase 4.

---

## Avance de Sub-fase 3.4 (post-F3H25)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor ⬅ este plan
F3H27 –  — Grupos + Map Tools integrados + mundo grande + HDRI dinámico (stub)
```

---

## Norte

El dev validó F3H25 visualmente y reportó 5 puntos de UX que rompían su flujo, más 1 conflict de hotkey que descubrió al usar el editor, más 1 pregunta arquitectónica sobre Union CSG que se resolvió quitando la op del UI. Antes de pasar al último hito de Fase 3 (F3H27 gaps), corregimos estos detalles en bloque.

---

## Decisiones cerradas

### D1 — Modal "Acerca de" + window title: sacar mención "Hito 3 / Hito 4"
El dev: *"el modal de introduccion sacale el hito 3, porque esta re desactivalizado"*.
- `editor.modal.about.version`: "Versión 0.3.0 (Hito 3)" → "Versión 2.25.0 — Fase 3 cerrada".
- `EditorApplication_Init.cpp` spec.title: "MoodEngine Editor - v0.4.0-dev (Hito 4)" → "MoodEngine Editor".
- `updateWindowTitle()` ya armaba el sufijo del proyecto bien, solo el spec inicial estaba desactualizado.

### D2 — MenuBar reorden: Archivo > Editar > Mapa > Ver > Debug > Ayuda
El dev: *"arriba dice archivo y luego mapa, usualmente es archivo luego editar"*.
- Orden estándar VSCode/Unity (Edit segundo).
- Sub-cambio implícito: el item top-level "Brush" desaparece (ver D3).

### D3 — Brush ops (booleanas) → context menu del Outliner (right-click sobre brush)
El dev: *"el de brush solo tiene las operaciones booleans no se como eso no esta como algun modificador como los de blender o algo en lugar de ocupar una seccion arriba"*.

Investigamos 4 patrones de la industria + 3 propuestas (Inspector / Right-click Outliner / Modifier no-destructivo). El dev eligió **right-click contextual** en el Outliner:
- Solo aparece si `e.hasComponent<BrushComponent>()` (gate por tipo de entidad).
- Reusa `EditorUI::drawBooleanOpMenu` que ya tenía la lógica de validación `>= 2 brushes`.
- Iteración intermedia (`Editar > Brushes (booleanas)`) fue rejected por el dev porque seguía contaminando el MenuBar; quitado en favor del context menu puro.
- Keys i18n del intermedio (`editor.menu.edit.brush_ops`) removidas.

### D4 — Union CSG: removida del UI (Hammer-style)
El dev: *"he dado click en el mas chico y luego el mas grande y hago union, pero asi funciona la union den hammer? porque me termino creando 4 piezas separadas, es raro"*.

Investigamos el comportamiento. Causa: `Csg::unionOp(A, B)` en overlap parcial devuelve `(A \ B) ∪ {B}` — matemáticamente la única forma de representar `A ∪ B` cuando `A ∪ B` NO es convexo (caso general con 2 cajas overlappeadas). Decomponer en N convexos es **correcto** matemáticamente, no es un bug del algoritmo.

Otros editores:
| Editor | Decisión |
|---|---|
| Hammer (Source) | NO ofrece Union — exactamente por esta razón. |
| Unreal Modeling Mode | Sí Union, también descompone en piezas. |
| Blender | Modifier no-destructivo con BMesh (no-convex) — otro modelo de geometría. |

3 opciones presentadas al dev: (1) quitar Union, (2) toast explicativo, (3) agrupar piezas en entidad padre. El dev eligió **#1: quitar Union** ("cierra esto, y pasamos a otro hito"). Junto con la explicación del workflow brush-based (no se "unen" para hacer edificios — quedan hermanos visualmente adyacentes; las booleanas son para cortes tipo ventana/arco), se confirmó la decisión.

- `EditorUI::drawBooleanOpMenu`: removido el `MenuItem` de Union, quedó solo Subtract + Intersect.
- `Csg::unionOp` en `engine/world/csg/BrushOps.cpp` **se mantiene intacto** — código sin uso pero correcto, por si emerge un modelo no-convex en el futuro (F3H27/F4: HDRI/skybox dinámico no requiere esto, pero al rediseñar geometría sí).
- Keys i18n `editor.menu.boolean.union` siguen presentes (forward-compat con JSON viejo, sin uso UI).

### D5 — UserPreferencesPanel: sidebar de categorías estilo Blender
El dev: *"algo que no me gusta de mi panel es que lo veo poco categorizado, te pongo el de blender alado para que veas que esta mas organizado"*.

Reemplazo de `TabBar` horizontal (2 tabs: General + Editor) por **layout split sidebar + content**, estilo Blender Preferences:
- Window resize de 540×360 → 720×480.
- `BeginChild` izquierdo (sidebar 150px, border) con 5 `Selectable`: General / Viewport / Assets / Performance / Notificaciones.
- `BeginChild` derecho (content, scroll vertical) con un `switch (m_activeCategory)`.
- `drawEditorTab` viejo se splitea en 4 sub-métodos: `drawViewport`, `drawAssets`, `drawPerformance`, `drawNotifications`. Cada uno recibe `cfg`, `defaults`, `dirty`, `saveNow` por ref (compartidos en el switch).
- `SeparatorText` para sub-secciones internas (ej. Viewport tiene "Cámara ortográfica" / "Gizmos" / "Interacción").

### D6 — Toast parpadeo en el frame de aparición
El dev: *"el toast a veces en lo que aparece, parpadea multiples veces rapidamente"*.

Causa: `ToastsOverlay::draw()` usaba `&t` (dirección del `Toast` en el snapshot temporal del vector) como ID de la ventana ImGui. Cada `Toasts::snapshot()` devuelve un vector NUEVO con direcciones distintas → ImGui veía un ID diferente cada frame → recreaba la ventana sin cache de size del frame anterior → `AlwaysAutoResize` necesitaba 2 frames para estabilizarse → flicker visible en el frame de aparición.

Fix: `Toasts::Toast` gana un `u64 id` monótonamente creciente asignado en `push()`. `ToastsOverlay::draw` usa `"##toast_<id>"` como ID estable. ImGui mantiene el cache de size correctamente y el primer frame ya tiene el tamaño calculado.

### D7 — Ctrl+Z disparaba cycle de render mode + undo simultáneo
El dev: *"si doy ctrl + z, me esta cambiando entre tipos de render, entiendo que el z cambia pero choca una cosa con otra"*.

Causa: `EditorOverlay.cpp:520` usaba `ImGui::IsKeyPressed(ImGuiKey_Z, false)` sin chequear modificadores. Ctrl+Z dispara AMBOS: el handler de undo en `EditorApplication.cpp` (que sí chequea `KMOD_CTRL`) Y el handler de cycle render mode (que ignoraba modificadores).

Fix: gate por `!io.KeyCtrl && !io.KeyShift && !io.KeyAlt` antes del `IsKeyPressed`. "Z desnuda" sigue ciclando; Ctrl+Z solo deshace.

---

## Implementación

### Engine (core)
- `src/core/Toasts.h`: agregado `u64 id` al struct `Toast`.
- `src/core/Toasts.cpp`: contador `s_nextId` monótono + assign en `push` bajo el mutex.

### Editor (UI)
- `src/editor/ui/MenuBar.cpp`:
  - Reorden: Archivo → Editar → Mapa → Ver → Debug → Ayuda.
  - Bloque "Brush" top-level removido.
- `src/editor/ui/EditorUI.cpp`:
  - `drawBooleanOpMenu`: removido `MenuItem` de Union.
- `src/editor/ui/ToastsOverlay.cpp`:
  - ID estable: `##toast_<u64 id>` en lugar de `##toast_<&t puntero>`.

### Editor (panels)
- `src/editor/panels/scene/HierarchyPanel.cpp`:
  - Context menu `##entity_ctx`: si `e.hasComponent<BrushComponent>()`, agrega `drawBooleanOpMenu()` (que ya es un `BeginMenu` con sus items dentro).
  - Include nuevo: `engine/scene/components/BrushComponent.h`.
- `src/editor/panels/project/UserPreferencesPanel.{h,cpp}`:
  - Enum `Category { General, Viewport, Assets, Performance, Notifications }`.
  - `onImGuiRender`: split sidebar + content.
  - Métodos nuevos: `drawSidebar`, `drawGeneral`, `drawViewport(cfg, ...)`, `drawAssets(...)`, `drawPerformance(...)`, `drawNotifications(...)`.
  - Métodos viejos `drawGeneralTab` / `drawEditorTab` eliminados (contenido migrado).
- `src/editor/application/EditorOverlay.cpp`:
  - Gate por modificadores antes del `IsKeyPressed(Z)`.
- `src/editor/application/EditorApplication_Init.cpp`:
  - `spec.title = "MoodEngine Editor";` (sin "(Hito 4)").

### i18n
- Modificadas: 2 keys (`editor.modal.about.version` en es+en).
- Agregadas: 10 keys nuevas:
  - 5 categorías sidebar: `category.general`, `category.viewport`, `category.assets`, `category.performance`, `category.notifications`.
  - 5 separators: `section.ortho_cam`, `section.gizmos`, `section.interaction`, `section.asset_browser`, `section.profiler`.
- Removidas: `editor.menu.edit.brush_ops` (iteración intermedia descartada).

### Tests
- Sin tests nuevos (todos los cambios son UI). Suite **1277/11810 verde** mantiene (no regresiones).

---

## Ajustes reactivos post-validación

### `ImGuiChildFlags_Border` no compila
Versión de ImGui del proyecto (docking branch ~1.92) no exporta `ImGuiChildFlags_Border`. Fix trivial: usar la sobrecarga `BeginChild(id, size, /*border=*/true, flags)` que sí está disponible. Cambio en `UserPreferencesPanel.cpp:89`.

### Iteración "Editar > Brushes (booleanas)"
Primera propuesta tras quitar el top-level: meter las booleanas como submenu dentro de Editar. El dev rejected — *"me gusta como esta el de preference, pero prefiero que el de brush este en otro lado"*. Lo movimos al context menu del Outliner (right-click sobre brush). Keys i18n del submenu intermedio se removieron.

### Workflow brush-based explicado al dev
El dev preguntó *"y si quiero crear un mapa, una rampa, edificios, y quiero unir nose, una base con una torre, como hago? uso blender para modelar?"* y *"si quiero mover un edificio que tiene 40k piezas separadas? que hago? como hicieron en source?"*. La conversación abrió el alcance de F3H27 (parent/child transforms + grupos) — ver stub.

---

## Backlog del hito (no cerrado en F3H26)

- **Parent/child transforms + Groups (Ctrl+G)** — promovido a F3H27 explícitamente.
- **Mundo grande (camera orbital limita 1u=1m)** — promovido a F3H27.
- **HDRI dinámico + ciclo día/noche** — promovido a F3H27.
- **Map Tools panel** + **Grupos panel** integrados como categorías del Properties Editor (estilo F3H22) en vez de paneles flotantes separados — promovido a F3H27.
- **Csg::unionOp código sin usar** — se mantiene por si emerge un modelo no-convex (mesh editing real, BMesh-style). Si Fase 4 lo confirma como muerto, se borra.

---

## Lo que NO toca F3H26

- F3H27 (gaps UX restantes — ver stub) — hito siguiente, separado.
- Reorden del MenuBar Ver / Debug / Ayuda — quedan como estaban (sólo se movió Editar y se quitó Brush).
- Recovery modal del F3H25 — sin cambios; sigue funcionando.
- Tests headless — no aplica para cambios UI puros.
