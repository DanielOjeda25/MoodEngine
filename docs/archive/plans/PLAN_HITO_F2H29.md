# Plan — F2H29: Block tool + drag-edit + vertex edit en ortos

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entry F2H28 (split en 2 hitos), `archive/plans/PLAN_HITO_F2H28.md`
> (decisiones de la base sobre la que F2H29 monta).

---

## Objetivo

Habilitar las 3 features de **edición** estilo Valve Hammer Editor que F2H28
dejó explícitamente afuera por scope:

1. **Block tool** — click-drag en una orto dibuja un rectángulo de
   selección → al soltar crea un Box brush con esas dimensiones + altura
   default sobre el eje perpendicular.
2. **Drag-edit** — clickear y arrastrar un brush ya existente desde
   cualquier orto lo mueve sobre el plano de esa vista con snap al delta
   (`pos = round(pos / snap) * snap`). Update en vivo en las otras 3
   vistas.
3. **Vertex/edge edit** — teclas `1` / `2` (reservadas en F2H17) activan
   sub-modos. Click sobre un vertex/edge del brush lo selecciona + drag
   lo mueve con snap. La mesh se regenera al soltar.

**Beneficios**: el dev por fin puede modelar geometría desde los
ortográficos sin recurrir al Inspector. Cierra el flow Hammer-style del
feedback explícito *"lo quiero igual que el hammer"* (F2H17 / F2H22).

---

## Filosofía de diseño

- **Reuso máximo del state expuesto en F2H28**: `m_hammerSnapStep`
  (ya cycleable con Ctrl++/Ctrl+-) se aplica al delta del drag.
  `OrthoCamera::worldFromNdc(...)` da la coord world para arrancar
  cualquier rectángulo o drag. `pickEntityFromRay` hace el hit-test
  inicial.
- **Undo coherente**: cada operación pushea un command:
  - **Block tool** → `AddBoxBrushCommand` (probable extensión del
    existente `spawnBrushEntity` con captura de la entidad para undo).
  - **Drag-edit** → `MultiEditTransformCommand` (mismo patrón que F2H23
    iter 5; ya existe).
  - **Vertex edit** → `EditBrushGeometryCommand` nuevo (captura
    `vector<Plane>` pre/post de las caras afectadas).
- **Preview en vivo**: el block tool dibuja un AABB cyan en las 3 ortos
  + perspectiva durante el drag (vía `debugRenderer().drawAabb`); al
  soltar lo materializa como brush real. Drag-edit aplica el delta al
  Transform en vivo (igual que F2H23 iter 5 hizo para el gizmo
  perspectivo).
- **Snap al delta, no a la posición absoluta**: `delta = round(delta /
  snap) * snap` para que el brush conserve su offset original respecto
  al grid si arrancó desalineado. (Hammer hace esto.)
- **Splittear en bloques chicos**: drag-edit primero (mínimo riesgo,
  máximo valor — el dev puede mover sin Inspector ya), block tool
  después, vertex edit al final (mayor superficie de bugs).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Drag-edit consume click del orto YA | El `OrthoViewportPanel` actual emite `ClickSelect` solo al up del LMB con umbral 4px — agregar `DragMove { active, ndcStartX, ndcStartY, ndcCurX, ndcCurY }` que se mantiene durante el drag. El click-select existente sigue funcionando para "click puro" (drag < 4 px). |
| 2 | Snap aplica al delta | `delta = round((cur - start) / snap) * snap` — preserva offset inicial. F2H29 NO snappea posiciones absolutas. |
| 3 | Drag-edit reusa MultiEditTransformCommand | Push al soltar el LMB. Aplica a TODAS las del SelectionSet (consistente con drag del gizmo perspectivo F2H23 iter 5). |
| 4 | Eje perpendicular intacto | Top (XZ) → drag mueve X y Z, Y intacto. Front (XY) → X+Y, Z intacto. Side (ZY) → Z+Y, X intacto. |
| 5 | Block tool: preview con `drawAabb` | Durante el drag dibuja un AABB cyan en world space en TODAS las vistas (perspectiva + 3 ortos) vía debugRenderer. Altura default = `m_hammerSnapStep * 4` (4 unidades del grid mayor). Al soltar materializa el brush. |
| 6 | Block tool: command nuevo `AddBoxBrushCommand` | Opcional — si extender `spawnBrushEntity` con flags es más simple, evitar el comando dedicado. El undo del spawn ya está cubierto por `CreateEntityCommand` desde Hito 27. Decisión final en Bloque C según costo. |
| 7 | Vertex/edge edit cambia sub-mode | Teclas `1` / `2` togglean `EditorSubMode::Vertex` / `EditorSubMode::Edge`. `Esc` vuelve a Object. F2H17 ya tiene Face mode con tecla `3`; los enum values están reservados desde entonces. |
| 8 | Vertex picking | Hit-test contra los vertices únicos del brush proyectados a NDC; toleancia ~6 px en screen-space. Edge picking similar contra segmentos. Iterate todos los brushes seleccionados (no solo `active` — multi-edit consistente con F2H23). |
| 9 | Vertex move muta planos | Cada vertex es la intersección de 3 planos. Mover un vertex se traduce a desplazar los 3 planos sobre su normal manteniendo el otro vertex de cada cara fijo. Approach simplificado: capturar `delta` y aplicarlo a `plane.distance` de los planos donde el vertex es vértice (3 caras). Validar `isBrushValid` antes de aceptar — si rompe el brush (degenerado), no commitear. |
| 10 | Edge move muta 2 planos | Edge = intersección de 2 caras. Mover edge = trasladar 2 planos en su componente perpendicular al edge. Mismo flujo que vertex pero más simple. |
| 11 | Sin redo de regeneration | La mesh del brush es runtime-regenerable (F2H11 ya lo hace via `dirty=true`). Vertex/edge edit setea `dirty=true` al soltar y deja al `brushPass` regenerar al siguiente frame. |

---

## Bloques

### A — Plan F2H29 (este archivo)

### B — Drag-edit de brushes en ortos

- `OrthoViewportPanel.{h,cpp}`:
  - Nuevo struct `DragMove { active, ndcStartX, ndcStartY, ndcCurX, ndcCurY }`.
  - Detectar drag: si LMB se mueve > umbral 4px sin haberse soltado, transición de click-pending a drag-active.
  - Al soltar (LMB up): consumir como `consumeDragMove()` y emitir el `start/cur` ndc final. Si fue solo click (< 4px), seguir el path `ClickSelect` actual.
- `EditorApplication_Run.cpp`:
  - Bloque nuevo 2.4c (post-2.4b orto click-select): consumir `DragMove` de cada orto + computar delta world (`worldFromNdc(cur) - worldFromNdc(start)`) + snap al delta + aplicar a TODAS las del SelectionSet.
  - **Pre-condición**: el frame del DOWN inicial detecta si hay un brush bajo el cursor (vía `pickEntityFromRay`). Si NO hay brush → fallback al block tool (Bloque C). Si HAY brush → drag-edit.
  - Push de `MultiEditTransformCommand` al soltar (mismo patrón F2H23 iter 5).
- Validación: drag de un brush en Top → se mueve en X/Z, Y intacto, snap aplica al delta, otras 3 vistas refrescan en vivo.

### C — Block tool (crear brush dibujando rectángulo)

- `OrthoViewportPanel`: si el LMB DOWN inicial NO pegó a ningún brush
  (verificado por EditorApplication via `pickEntityFromRay`), pasar a
  modo "block tool drag". Mismo `DragMove` struct; el caller decide qué
  hacer con el delta.
- `EditorApplication_Run.cpp`:
  - Durante el drag dibujar un AABB cyan vía `m_sceneRenderer->debugRenderer().drawAabb(...)`. Las dims son `(min, max)` del rectángulo en world (eje perpendicular: `+/- snapStep * 2` desde `worldHeight/2` para que se vea volumen).
  - Al soltar, validar tamaño mínimo (`> snapStep` en ambos ejes para evitar brushes degenerados de drags accidentales) y llamar al helper existente `spawnBrushEntity(scene, makeBoxBrush(matrix), "Brush_Box", "Box")`.
- Validación: en Top, click-drag desde (0,0) a (4,4) crea un Box brush 4x altura_default x 4 al soltar. Visible en perspectiva + las 3 ortos. Undo lo borra.

### D — Vertex/edge mode

- `EditorMode.h`: extender `EditorSubMode` con `Vertex` y `Edge`. Ya están "reservados" conceptualmente desde F2H17.
- `EditorApplication.cpp::processEvents`: teclas `1` (Vertex), `2` (Edge), `3` (Face — ya existente). `Esc` vuelve a Object.
- `OrthoViewportPanel`: si `subMode == Vertex` o `Edge`, el click hit-tea contra vertices/edges en lugar de brushes. Reusa la lógica de drag pero el target es un vertex/edge, no la entidad.
- `Csg::pickVertexInBrush(brush, ndc, camera, threshold) -> optional<int>` y `pickEdgeInBrush(brush, ndc, camera, threshold) -> optional<{i, j}>` nuevos en `engine/world/csg/Brush.h/.cpp` o helper aparte (`engine/world/csg/BrushPick.h`).
- Drag de vertex muta `face.plane.distance` de las 3 caras adyacentes; drag de edge muta 2 planos.
- Validar `isBrushValid` post-mutación; si falla, revertir y NO push del command.
- `EditBrushGeometryCommand` nuevo: captura `vector<Plane>` (todas las caras) pre + post del brush afectado. Execute/undo restauran las caras + setean `dirty=true`.
- Validación: en Vertex mode, click sobre un vertex del Box → highlight + drag lo mueve con snap → suelta → mesh regenera + Inspector refleja AABB nuevo. Ctrl+Z restaura.

### E — Cierre

- Test manual: dev arranca editor, switch a "Editor de mapas", crea brush con block tool, lo mueve con drag-edit, edita un vertex con tecla 1, deshace todo con Ctrl+Z secuencial.
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`,
  `PENDIENTES.md`), archive plan, commit + tag `v1.19.0-fase2-hito29` + push.

---

## Lo que NO entra en F2H29 (queda para hitos futuros)

- **Multi-selección con rectángulo de selección** en orto (clásico
  Hammer marquee select sobre múltiples brushes a la vez). Diferido
  como sub-bloque si emerge naturalmente.
- **Frustum de cámara perspectiva** dibujado en ortos (líneas que
  muestran el view frustum del 3D en cada vista 2D).
- **Coordenadas del cursor** en world units mostradas en cada orto
  (label "X: 32, Y: 64").
- **Rotación de brushes desde orto** (Hammer permite Shift+drag para
  rotar). Diferido — el gizmo perspectivo F2H13 sigue siendo el path
  preferido.
- **Snap-to-vertex / snap-to-grid configurables independientemente**.
  F2H29 usa el snap único `m_hammerSnapStep` para todo. Si emerge
  necesidad de "snap solo al mover, no al crear" (o viceversa), abrir
  hito propio.

---

## Tradeoffs conocidos

- **Vertex edit con planos como representación**: el algoritmo
  "mover un vertex = mover sus 3 planos" es aproximado. Para casos
  patológicos (brushes muy oblicuos, vertices con > 3 planos
  incidentes) el resultado puede ser inesperado. Mitigación: validar
  `isBrushValid` post-mutación + revertir si falla. Si emerge
  necesidad real, refactor a representación mesh-based para vertex
  edit (rompe el feature lock-to-world UV de F2H15 — costo alto).
- **Block tool sin preview de altura**: la altura del brush es
  `m_hammerSnapStep * 4` fija. Hammer original permite Shift+drag para
  ajustar altura DURANTE el drag mostrando preview en perspectiva.
  Diferido como mejora si emerge.
- **Drag-edit sin "axis lock"**: en Hammer, Shift+drag bloquea el
  movimiento al eje dominante. F2H29 no lo implementa — el dev arrastra
  libre y el snap discretiza. Si el feedback pide axis lock, agregar
  como tweak en hito posterior.
