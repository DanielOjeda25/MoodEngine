# Plan — F2H30: Polish del gizmo + atajos Blender + brush poligonal + vertex/edge edit

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entry F2H29 (descope tarde + paquete polish unificado),
> `archive/plans/PLAN_HITO_F2H29.md` (vertex/edge edit ya planeado en
> Bloque D — el approach técnico se reusa acá).

---

## Objetivo

Paquete polish UX del editor de mapas que junta 4 features pedidas por
el dev tras validar F2H29:

1. **Vertex/edge edit en ortos** (Bloque D diferido de F2H29). Teclas
   `1` / `2` activan sub-modos; click sobre vertex/edge del brush + drag
   con snap; mesh regenera al soltar.
2. **Brush poligonal "pincel"**. En una orto, clicks sucesivos sobre
   vertices del grid construyen una base poligonal; doble-click (o
   Enter) cierra creando un brush prismático con altura default.
3. **Gizmo rotate proporcional al AABB del brush**. Bug pre-existente
   F2H13: el radio del anillo es fijo, queda en posición rara cuando
   el brush es muy grande/chico.
4. **Atajos Blender-style** `G` / `R` / `S` modal con cursor + línea
   punteada al centro + Esc para cancelar. Modal interaction model
   distinto al gizmo de flechas — duplica la UX, no la reemplaza.

**Beneficios**: cierra el flow Hammer-style del feedback explícito
acumulado (vertex edit + brush creation libre + control gizmo +
atajos rápidos para devs acostumbrados a Blender). Tras F2H30, el
editor estilo Hammer está completo en su MVP funcional.

---

## Filosofía de diseño

- **Reuso máximo de la infra existente**:
  - Vertex/edge picking reusa `pickEntityFromRay` (rayo paralelo orto)
    + nuevo helper `pickVertexInBrush(brush, ndc, camera, threshold)`.
  - Drag de vertex/edge reusa el `DragState` del `OrthoViewportPanel`
    (mismo struct ya implementado en F2H29 Bloque B).
  - Brush poligonal reusa `Csg::Brush`-construction patterns (similar
    a `makePrismBrush` con N lados, pero con N variable definido por
    el dev).
  - Atajos `G/R/S` reusan los handlers de gizmo existentes
    (`m_gizmo.entity`, `m_gizmo.field`, `finalizeGizmoDrag`) — el
    modal arranca un drag virtual sin click sobre el handle.
  - Gizmo rotate proporcional: el radio se computa desde
    `brush.localAabb.maxExtent()` con un factor `~0.6` (radio dentro
    pero visible).
- **Validación incremental**: 4 commits feat (uno por feature), cada
  uno con suite verde antes de pasar al siguiente. Mismo patrón que
  F2H28 / F2H29.
- **Splittear si emerge presión**: si tras Bloque B (vertex edit) o C
  (brush poligonal) el hito ya pinta como > 1 semana, splittear
  F2H30/F2H31 según dominio (geometría vs. atajos UX).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Vertex/edge mutan planos | Vertex es intersección de 3 planos; mover vertex traslada esos 3 planos sobre su normal en el delta proyectado. Edge es intersección de 2 planos. Validar `isBrushValid` post + revert si rompe. |
| 2 | `EditBrushGeometryCommand` nuevo | Captura `vector<Plane>` (todas las caras) pre + post del brush afectado. Execute/undo restauran las caras + setean `dirty=true` para regenerar mesh. |
| 3 | Brush poligonal con vertex picking + close al doble-click | El dev clickea N vertices sobre el grid (snappeados al `m_hammerSnapStep`); cada click agrega al `polygon points` buffer; doble-click (o Enter) cierra. El brush se construye con `makePrismBrushFromPolygon(points, height)` nuevo en `engine/world/csg/Primitives.h`. |
| 4 | Brush poligonal: validación de polígono | El polígono debe ser convexo (CSG core de F2H11 asume brushes convexos). Si el dev clickea un vertex que rompe la convexidad, mostrar warning visual (línea roja) y rechazar el cierre. Mín 3 vertices. |
| 5 | Gizmo rotate radio proporcional | `radius = max(brush.localAabb.maxExtent() * 0.6f, 0.5f)` (clamp inferior para brushes muy chicos). En el path Object Mode default usa el AABB del MeshAsset (mismo computo que `meshAabbWorld`). |
| 6 | Atajos `G` / `R` / `S` modal | Tecla activa modo modal: gizmo se desactiva visualmente, cursor pasa a mover el SelectionSet con delta proporcional. Click izquierdo confirma; Esc cancela (revert al startValue). Sin línea punteada en el primer commit (es polish — agrega complejidad de overlay 2D); si emerge necesidad real, agregar después. |
| 7 | Atajos exclusivos en Editor Mode | `G/R/S` solo activos cuando workspace ≠ Programar (donde G/R/S son letras del Script Editor) Y el viewport tiene focus de mouse. Idem el resto de atajos modales (Esc no debe cerrar Welcome modal por error). |
| 8 | Modal de atajo: `MultiEditTransformCommand` al confirmar | Mismo patrón que `finalizeGizmoDrag`: capturar startValue al iniciar modal, aplicar delta en vivo, revert al before + push del command al confirmar. Sin command si delta == 0. |

---

## Bloques

### A — Plan F2H30 (este archivo)

### B — Vertex/edge edit en ortos

- `EditorMode.h`: extender `EditorSubMode` con `Vertex` y `Edge`
  (ya reservados conceptualmente desde F2H17).
- `EditorApplication.cpp::processEvents`: tecla `1` toggle Vertex,
  `2` toggle Edge, `3` Face (existente), `Esc` vuelve a Object.
- `engine/world/csg/BrushPick.h/.cpp` nuevo:
  - `pickVertexInBrush(brush, ndc, camera, screenThreshold) -> optional<int>`
    — proyecta los vertices únicos del brush a NDC, devuelve el más
    cercano dentro del threshold (default ~6 px en screen space).
  - `pickEdgeInBrush(brush, ndc, camera, screenThreshold) -> optional<{i, j}>`
    — itera segments de cada cara, prueba distancia point-to-segment
    en NDC.
- `OrthoViewportPanel`: si `subMode == Vertex` o `Edge`, el click hit-tea
  contra vertices/edges en lugar del rayo broadphase. Reusa el mismo
  `DragState` del Bloque B de F2H29.
- `EditorApplication_Run.cpp`: bloque 2.4e nuevo — al iniciar drag en
  Vertex/Edge mode, capturar el vertex/edge picked + `vector<Plane>`
  pre del brush. Cada frame aplicar delta a los 2-3 planos relevantes;
  validar `isBrushValid`; si rompe, no actualizar (visual snap-back).
  Al soltar, push `EditBrushGeometryCommand`.
- `editor/commands/EditBrushGeometryCommand.{h,cpp}` nuevo: ICommand
  que guarda `Entity entity, vector<Plane> before, vector<Plane> after`.
  Execute/undo asignan las caras + `dirty=true`.
- Validación: en Vertex mode, click sobre un vertex del Box → highlight
  (preview cyan?) + drag lo mueve con snap → suelta → mesh regenera
  + Inspector refleja AABB nuevo. Ctrl+Z restaura.

### C — Brush poligonal "pincel"

- `engine/world/csg/Primitives.h`: nuevo `makePrismBrushFromPolygon(
  points, height, axis)` — construye N+2 caras (N laterales + 2 base/top)
  desde una secuencia de N puntos en un plano y una altura sobre `axis`.
  Asume polígono convexo en orden CCW (validar con cross product entre
  edges consecutivos).
- `EditorMode.h`: extender `EditorSubMode` con `Polygon` (o usar un
  flag `m_polygonDrawing` en EditorApplication para no inflar el enum).
- `OrthoViewportPanel`: en modo polygon-drawing, click izq agrega
  punto al buffer; el panel dibuja líneas conectando los puntos
  + segmento desde último punto al cursor (preview en vivo).
  Doble-click / Enter cierra el polígono. Esc cancela.
- `EditorApplication_Run.cpp`: bloque 2.4f nuevo — al cerrar polygon,
  validar convexidad + `makePrismBrushFromPolygon` + `spawnBoxBrushAt`
  pattern (rebase a local space + `tf.position = centroid`).
- UI: botón nuevo en Toolbar (icono "P" o label "Pincel") que activa
  el modo. O atajo (tecla `B` de "Block"?). Decisión final en Bloque
  C según costo.
- Validación: dev clickea 4-5 puntos sobre el grid de la orto Top, ve
  el polígono dibujado en vivo, doble-click cierra → brush prismático
  visible en las 4 vistas. Polígono cóncavo → warning visual + rechazo.

### D — Gizmo rotate proporcional + atajos `G`/`R`/`S`

- `EditorOverlay_Gizmo.cpp`: el radio del anillo de rotate se computa
  desde el AABB del entity activo: `radius = max(AABB.maxExtent() *
  0.6f, 0.5f)`. Si el entity tiene `BrushComponent`, usa
  `bc.brush.localAabb`; si tiene `MeshRendererComponent`, usa el AABB
  del MeshAsset (igual que `meshAabbWorld` de `ScenePick.cpp`); else
  fallback al cubo unitario `0.5`.
- `EditorApplication.cpp::processEvents`: handlers para `G` / `R` /
  `S` (sin modifier, `repeat == 0`, `WantTextInput == false`,
  `m_mode == Editor`, viewport hovered). Cada uno arranca un modal:
  - `G` → modo Grab: cursor mueve el SelectionSet en world XY
    (proyectado al plano de la cámara). Snap aplica.
  - `R` → modo Rotate: cursor distance/angle desde el centro del
    objeto rota sobre el eje view-axis del editor (perspectiva).
    En orto: rota sobre el eje perpendicular del view.
  - `S` → modo Scale: cursor distance del centro escala uniforme
    (todos los ejes igual). Si dev presiona `X`/`Y`/`Z` durante el
    modal, restringe a ese eje.
- `EditorApplication.h`: nuevo struct `ModalShortcutState { active,
  field, startValues[], cursorStart }`. Campos similares a
  `m_gizmo` actual pero sin handle.
- `EditorApplication_Run.cpp`: bloque 2.4g nuevo — si `m_modalShortcut.active`,
  computar delta del cursor cada frame, aplicar al SelectionSet en
  vivo. Click izq confirma → push `MultiEditTransformCommand`. Esc
  cancela → revert a startValues.

### E — Cierre

- Test manual: dev arranca editor, switch a "Editor de mapas":
  - Crea brush con block tool (F2H29).
  - Tecla `1` → Vertex mode → drag un vertex.
  - Tecla `2` → Edge mode → drag un edge.
  - Tecla Esc → vuelve a Object.
  - En perspective, gizmo rotate del brush se ve proporcional al
    tamaño del mesh.
  - Tecla `G` → mover con cursor → click confirma.
  - Tecla `R` → rotar con cursor → Esc cancela (vuelve a posición
    original).
  - Tecla `S` → escala uniforme con cursor → tecla `X` durante el
    modal restringe a eje X.
  - Pincel: tecla `B` → click 4 puntos sobre grid → doble-click
    cierra → brush prismático aparece.
  - Ctrl+Z deshace cada operación en orden.
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`,
  `PENDIENTES.md`), archive plan, commit + tag `v1.20.0-fase2-hito30`
  + push.

---

## Lo que NO entra en F2H30 (queda para hitos futuros)

- **Línea punteada del modal `S`/`R`/`G`** (cosmetic polish — el modal
  funciona sin ella). Si dev pide explícitamente, agregar como
  sub-bloque.
- **Multi-selección con marquee select** en orto (clásico Hammer
  rectángulo de selección sobre múltiples brushes). Diferido — no
  bloquea el modeling flow.
- **Brush poligonal cóncavo** (CSG core asume convexos). Sería un
  refactor mayor del CSG.
- **Vertex move con representación mesh-based** (alternativa al
  approach planos). Solo si emerge bug real con el approach actual.
- **Frustum de cámara perspectiva** dibujado en ortos.
- **Coordenadas del cursor** en world units mostradas en cada orto.
- **Snap-to-vertex** (snap a vertices existentes del scene, no solo
  al grid).

---

## Tradeoffs conocidos

- **Vertex edit aproximado en brushes oblicuos**: el algoritmo "mover
  un vertex = mover sus 3 planos" es exacto solo si los planos se
  intersectan en un único vertex después del move. Para vertices con
  > 3 planos incidentes (ej. corners de cilindros / esferas), el
  resultado puede degenerar. Mitigación: validar `isBrushValid` post +
  revert. Si emerge necesidad, refactor a representación mesh-based
  (rompe lock-to-world UV de F2H15).
- **Pincel solo convexo**: el dev no puede crear formas concavas en
  un solo brush. Workaround: crear varios brushes convexos + boolean
  union (F2H12 ya lo hace).
- **Atajos `G`/`R`/`S` colidan con Programar**: en el workspace
  Programar (Script Editor activo), G/R/S son letras del editor de
  texto. Resolver: solo activar atajos cuando viewport tiene focus,
  no cuando text input.
- **Modal `S` sin axis lock por default**: el dev escala uniforme.
  Si pide axis lock, agregar `X`/`Y`/`Z` durante el modal (alineado
  con Blender). Decisión Bloque D.
