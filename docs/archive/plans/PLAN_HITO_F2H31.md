# Plan — F2H31: Productivity selection + visual polish del editor de mapas

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entry F2H30 cierre (esquema final de atajos + gates anti-
> conflicto que F2H31 reusa para no romper el flow del pincel/vertex edit).

---

## Objetivo

Cerrar las brechas de **productividad** y **feedback visual** del editor
de mapas estilo Hammer. F2H30 cerró el MVP funcional (4-viewport + click-
select + grid + block tool + drag-edit + vertex/edge edit + pincel + W/E/R
modal); F2H31 levanta la calidad de uso al nivel de Hammer real:

1. **Marquee select en orto**: rectángulo de selección sobre múltiples
   brushes con Shift / Ctrl modifiers (clásico Hammer).
2. **Group transform via drag-edit**: el drag-edit actual mueve solo el
   brush bajo el cursor; extender a "todos los brushes seleccionados se
   mueven juntos con el delta" — natural cuando hay multi-select.
3. **Snap-to-vertex**: toggle global; cuando activo, vertex/edge edit y
   block tool snapean al vertex más cercano del scene si está dentro de
   un threshold (default 8 px screen-space), sino al grid del workspace.
4. **Frustum de cámara perspectiva dibujado en los 3 ortos**: rectángulo
   amarillo proyectado al plano de cada orto, indica "qué está mirando
   la 3D cam".
5. **Coordenadas world del cursor**: label en cada orto bajo el cursor
   `(x.x, y.y, z.z)` — referencia rápida sin abrir Inspector.

**Beneficios**: cierra el "Hammer feel" en productividad — sin marquee
select, mover 5 brushes es 5 drags; con marquee + group transform es 1.
Snap-to-vertex destrabe el flow de "construir contra geometría existente"
sin grid-aligned. Frustum + coords son polish de descubribilidad
visual.

---

## Filosofía de diseño

- **Reuso máximo de la infra existente**:
  - Marquee usa `OrthoViewportPanel::DragState` ya existente — distinguir
    "drag sobre vacío" (marquee select) de "drag sobre brush" (drag-edit
    o block tool). Ya tenemos las 3 sesiones (`OrthoDragSession`,
    `OrthoBlockToolSession`, `OrthoVertexEditSession`); marquee es la 4ta.
  - Group transform reusa `MultiEditTransformCommand` de F2H23 iter 5 +
    `OrthoDragSession::startPositions` (ya cachea por entidad).
  - Snap-to-vertex reusa `Csg::enumerateBrushVertices` (F2H30 Bloque B)
    + spatial broadphase por AABB del brush.
  - Frustum: `EditorCamera::projectionMatrix(aspect)` + `viewMatrix()`
    ya existen; proyectar las 8 esquinas del frustum a cada orto via su
    `vp` ya disponible.
  - Coords: `OrthoCamera::worldFromNdc(...)` ya devuelve el world point;
    formatearlo en el `OrthoViewportPanel` overlay.
- **Validación incremental**: 4 commits feat (1 por bloque B-D), cada
  uno con build OK + verificación visual del dev antes de pasar al
  siguiente. Mismo patrón que F2H28 / F2H29 / F2H30.
- **Sin schema bump**: nada se persiste a `.moodmap` en F2H31. Todo es
  state in-memory del editor + UI overlays.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Marquee se activa solo en empty space | Si el LMB-down arranca sobre un brush (pickEntityFromRay devuelve hit), no es marquee — lo consume drag-edit (`OrthoDragSession`) o block tool. Si arranca en vacío, marquee. Sin esto, marquee y block tool comparten "drag en empty space" y se pisan. |
| 2 | Marquee modifiers Hammer-style | Plain marquee = replace selección. Shift+marquee = add. Ctrl+marquee = toggle. Sin modifier sobre selección vacía = clear (consistente con click-select). |
| 3 | Marquee hit-test con AABB world | Para cada entidad con `BrushComponent` o `MeshRendererComponent`: AABB world (mismo `meshAabbWorld` / `brushAabbWorld` de `ScenePick`) → proyectar los 8 corners al plano del orto → si el rectángulo del marquee contiene CUALQUIER corner, hit. Más liberal que "todos los corners adentro" — alineado con Hammer (cualquier overlap selecciona). |
| 4 | Group transform: rebasing por entidad | Cada entry en `OrthoDragSession::startPositions` ya tiene su `pos` start; aplicar `delta_world` snappeado (en world, no per-entity) a cada uno. `MultiEditTransformCommand` cubre el push undoable. Mismo patrón que el gizmo perspectivo F2H23 iter 5 que ya hace multi-edit. |
| 5 | Snap-to-vertex: spatial broadphase + threshold screen | Un toggle `m_snapToVertexEnabled` en EditorApplication (default false). Cuando activo + el cursor está sobre un orto: enumerar vertices únicos de todos los brushes con AABB intersect del cursor + threshold ~32 world-units; proyectar cada vertex al ndc del orto; el más cercano dentro de `kThresholdPx` (~8 px) gana. Si ninguno, fallback al snap al grid. |
| 6 | Snap-to-vertex visual feedback | Marker celeste (mismo que vertex/edge edit) sobre el vertex que ganó el snap, mientras el dev sostiene el drag. Sin esto, el dev no ve cuál vertex se está usando. |
| 7 | Frustum: dibujar 4 corners + 4 edges del near-plane proyectados | Solo el rectángulo del near-plane (8 corners + 12 edges sería ruidoso). El near-plane proyectado al plano del orto da un cuadrilátero amarillo claro. Sin culling: si el frustum está completamente fuera del orto, aún queda invisible (no degrada). |
| 8 | Coords: top-left del orto, font monospace | Texto `Cursor: (x.x, y.y, z.z)` en la esquina top-left del overlay del panel, mismo color/font que el label "Top (XZ)" / "Grid: 16u" ya existentes. Solo se muestra si el mouse está dentro del panel (`liveCursor().hovered`). |
| 9 | Toggle de snap-to-vertex via tecla `V` | Acepta tecla `V` (sin modifier) para toggle, solo en workspace "Editor de mapas" + !WantTextInput + !modal. Botón visible en el toolbar lateral "Map Tools" abajo del Pincel. |

---

## Bloques

### A — Plan F2H31 (este archivo)

### B — Marquee select + group transform en orto

- `OrthoViewportPanel`: si el `DragState.active` arranca en empty space
  (sin hit con `pickEntityFromRay` al inicio del drag), marcar la sesión
  como "marquee mode"; sino sigue como antes (drag-edit si hit, block
  tool si vacío). El panel solo expone el state — la decisión ocurre en
  `EditorApplication_Run.cpp`.
- `EditorApplication_Run.cpp`: bloque 2.4f nuevo — al iniciar un drag
  en empty space del orto, **antes del block tool**, hacer un pick de
  prueba al `ds.ndcStartX/Y`; si hit → no es marquee, dejar que 2.4d
  (block tool) o 2.4c (drag-edit) lo manejen. Si miss → marquee mode.
  Cada frame del drag, dibujar rectángulo amarillo. Al cerrar:
  - Calcular AABB world de cada entidad seleccionable.
  - Proyectar 8 corners al ndc del orto.
  - Para cada brush: si CUALQUIER corner cae dentro del rect del
    marquee (en ndc), hit.
  - Aplicar modifiers (replace / add / toggle / clear) según
    Shift/Ctrl al momento del release.
- `EditorApplication_Run.cpp`: extender bloque 2.4c (drag-edit) — si el
  drag arrancó sobre UN brush pero el `SelectionSet` tiene N seleccionados
  (incluyendo el del cursor), aplicar `delta_world` a TODOS al iniciar
  la sesión. `OrthoDragSession::startPositions` ya guarda por entidad,
  solo extender la inicialización para popular con todo el SelectionSet
  en lugar de solo el clicked.
- Validación: dev abre map, drag en empty space marca rectángulo
  amarillo → suelta → 3 brushes seleccionados a la vez → drag uno → los
  3 se mueven juntos con snap → Ctrl+Z deshace todo agrupado.

### C — Snap-to-vertex toggle

- `EditorApplication.h`: nuevo `bool m_snapToVertexEnabled = false`.
- `EditorOverlay.cpp`: tecla `V` toggle; solo activa en workspace
  "Editor de mapas" + !WantTextInput + !modal.
- `MapEditorTopBar.cpp`: botón nuevo "Snap V" debajo de "Pincel" con
  highlight si activo. Tooltip: "Snap a vertex (V) — snapea al vertex
  mas cercano del scene en lugar del grid".
- `EditorApplication_Run.cpp`: en bloques 2.4a-poly (pincel), 2.4d
  (block tool LMB), 2.4e (vertex/edge edit) — cuando se va a snappear
  un punto al grid, primero intentar snap-to-vertex si está activo:
  - Enumerar vertices únicos de todos los brushes (reusa
    `Csg::enumerateBrushVertices` por entidad — broadphase con AABB
    world intersect contra una caja de threshold ~32u alrededor del
    cursor world).
  - Proyectar cada candidato al ndc del orto.
  - El más cercano al cursor dentro de `kSnapVertexPxThreshold = 8.0`
    px gana.
  - Si ninguno cae adentro, fallback al snap al grid del workspace
    (mismo comportamiento que ahora).
- `EditorRenderPass.cpp`: si snap-to-vertex está activo Y hay un vertex
  candidato dentro del threshold, dibujar marker celeste (mismo color
  que vertex edit markers) sobre el vertex en los 4 viewports — el dev
  ve cuál se usaría si soltara ahí.
- Validación: toggle V → el cursor en el orto muestra un dot celeste
  sobre el vertex hovered del brush vecino → click del pincel agrega
  un vertex en EXACTAMENTE esa coordenada (no snappeada al grid).

### D — Frustum perspectivo + coords cursor

- `EditorRenderPass.cpp`: para cada orto, antes del flush del
  debugRenderer, encolar 4 líneas formando el rectángulo del near-plane
  de la `EditorCamera` perspectiva, proyectado al plano del orto. Color
  amarillo claro `(255, 240, 100, 180)`. Esquinas del near-plane:
  ```
  np = editorCamera.position() + forward * near
  half_h = near * tan(fovY/2)
  half_w = half_h * aspectPerspective
  // 4 corners en world: np + right * ±half_w + up * ±half_h
  ```
- `OrthoViewportPanel.cpp`: cuando `m_liveCursor.hovered`, calcular el
  world point del cursor via `worldFromNdc` (ya disponible) y dibujar
  un texto monospace `Cursor: (x.x, y.y, z.z)` en la esquina top-left
  del panel, debajo del label "Top (XZ)" existente (offset ~16 px). El
  text usa `ImGui::GetWindowDrawList()->AddText()` con la font default.
- Validación: dev mueve el mouse en el orto Top → texto en top-left
  cambia en vivo con coords X / Z (Y siempre = panOffset.z = altura del
  plano de la vista). En Front: X/Y cambia, Z constante. En Side: Y/Z
  cambia, X constante. Frustum amarillo visible si la 3D cam apunta a
  donde el orto enfoca.

### E — Cierre

- Test manual completo (dev valida visualmente):
  - Drag en empty space del orto Top → rectángulo amarillo del marquee
    → 3 brushes seleccionados a la vez (highlight naranja en las 4
    vistas).
  - Drag sobre uno de los 3 → todos se mueven juntos con snap → Ctrl+Z
    deshace agrupado (los 3 vuelven a la posición original).
  - Tecla V → log `[snap] vertex snap = on` + botón "Snap V" del
    toolbar resaltado.
  - Hover el cursor cerca de un vertex de brush vecino → marker celeste
    aparece encima de ese vertex.
  - Click del pincel allí → vertex agregado en la coord exacta (no
    redondeado al grid step).
  - Frustum amarillo visible en los 3 ortos siguiendo a la 3D cam
    cuando se rota.
  - Coords `Cursor: (...)` en cada orto top-left siguiendo el mouse.
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`,
  `PENDIENTES.md`), archive plan, commit + tag `v1.21.0-fase2-hito31`
  + push.

---

## Lo que NO entra en F2H31 (queda para F2H32 / F2H33)

- **Clip tool** (3-click define plano que splittea brushes). Va a
  **F2H32 — Geometry tools**.
- **Carve UI** (boolean subtract con flow Hammer-style). Va a **F2H32**.
- **VisGroups** (agrupar brushes con nombre + hide/show + persistencia).
  Va a **F2H33 — Organización + face polish**.
- **Texture alignment del Face Edit Sheet** (Align/Fit/Justify). Va a
  **F2H33**.
- **Marquee inverso (Alt+marquee = remove)**: scope incremental, agregar
  si el dev lo pide después del F2H31.
- **Drag de los handles del frustum** para mover la 3D cam desde una
  orto: scope mayor, no típico de Hammer.

---

## Tradeoffs conocidos

- **Marquee no excluye props (Light/Audio sin BrushComponent)**: el
  marquee actual selecciona solo entidades con AABB world derivable
  (Mesh/Brush). Si el dev pide incluir Light/Audio en marquee, agregar
  fallback al icon sphere de F2H24. Diferido.
- **Snap-to-vertex N² por frame**: enumerar vertices de todos los
  brushes cada frame es O(N) brushes × O(V) vertices/brush. Mitigado
  por broadphase AABB + threshold de 32u — solo brushes cerca del
  cursor entran al inner loop. Si emerge slow en escenas con > 500
  brushes, refactor a spatial hash.
- **Frustum solo near-plane, no far**: dibujar near + far + 4 edges
  laterales = 16 líneas por orto = ruidoso. Decisión: solo near (4
  líneas) — clearly indica "qué mira la cam". Far no aporta info que
  el dev necesite.
- **Group transform bypassa drag-edit single**: si el dev tiene 1 brush
  seleccionado y hace drag-edit, el comportamiento es el mismo (move
  ese brush). Si tiene N seleccionados y dragea uno, los N se mueven —
  eso es nuevo. Sin "modifier para forzar single move" — alineado con
  Hammer (no tiene un modifier dedicado para eso tampoco).
- **Coords en orto Top muestran Y constante**: ese eje es el
  perpendicular al view (height del plano). Mostrarlo igual aunque sea
  redundante — facilita ver la altura efectiva del plano de vista.
