# Plan — Hito 13: Gizmos y selección

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 13) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Que el editor sea **usable con el mouse** sobre la escena 3D: click en una entidad la selecciona; seleccionar muestra un **gizmo** (flechas de translate, anillos de rotate, cajas de scale) sobre el pivote de la entidad; arrastrar el gizmo modifica el `TransformComponent`.

Features core:
- **Click-to-select**: ray-cast desde el cursor al viewport, tocar un body/mesh/light hace `EditorUI::setSelectedEntity`.
- **Translate gizmo**: 3 flechas (X rojo, Y verde, Z azul) + plano para mover en 2 ejes. Click en una flecha + drag mueve la entidad.
- **Rotate gizmo**: 3 anillos (eje-alineados) que rotan la entidad alrededor de ese eje.
- **Scale gizmo**: 3 cajas por eje + central para uniform scale.
- **Hotkeys**: `W` translate, `E` rotate, `R` scale (convención Unity).

Bloque 0 también arrastra: **CharacterController** (diferido del Hito 12) si aparece el bandwidth; si no, otra vez se difiere.

No-goals del hito: multi-selección + rectangle select (Hito 14+), snap to grid, parent-space vs world-space toggle, undo/redo (Hito 22).

---

## Criterios de aceptación

### Automáticos

- [x] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [x] Tests: `tests/test_picking.cpp` cubre ray-AABB, ray-sphere, dos targets (más cercano gana), rayo al cielo, entidad detrás de la cámara, guard sin Transform. **7/7 verdes** (106/106 suite completa).
- [x] Cierre limpio.

### Visuales

- [x] Click en una entidad en el viewport la resalta en Hierarchy (selection sync).
- [ ] Shift+Click agrega a la selección — **diferido** (Hito 14+). No entró en este hito; no es bloqueante.
- [x] Con entidad seleccionada, gizmo aparece sobre el pivote.
- [x] Arrastrar cada eje del gizmo modifica la propiedad correcta y se ve en vivo.
- [x] Inspector refleja los cambios en tiempo real.

---

## Bloque 0 — Pendientes arrastrados del Hito 12

- [ ] **CharacterController de Jolt** (Bloque 4 del Hito 12): integración del jugador como `JPH::CharacterVirtual`. **Diferido otra vez** — se pospone a un hito dedicado cuando el gameplay lo requiera (probablemente post-prefabs).
- [ ] **Rotación del body → Transform.rotationEuler**: hoy solo sincronizamos position. Diferido — todavía no se apila nada que ruede en la demo.
- [ ] **Shape `MeshFromAsset`**: diferido — el picking por AABB cubre el caso común.

## Bloque 1 — Raycast pickable ✅

- [x] `src/engine/scene/ScenePick.{h,cpp}`: `pickEntity(scene, view, projection, ndc) -> ScenePickResult`. Usa AABB unitario de la mesh (escalado por Transform) o esfera de radio 0.6m para Light/Audio sin mesh.
- [ ] ~~Integrar con Jolt `rayCast`~~: se optó por ECS-only (más simple, sin depender del PhysicsWorld). Cuando haya muchas entidades se evalúa.
- [x] Fallback a AABB ECS-side si el body no existe.

## Bloque 2 — Click-to-select ✅

- [x] `ViewportPanel` captura click izquierdo sobre la imagen, con threshold de 4px para distinguir click puro de drag.
- [x] Al click, construye NDC y `EditorApplication` genera el ray + llama a `pickEntity`.
- [x] Resultado → `EditorUI::setSelectedEntity(e)`. Hierarchy e Inspector reaccionan automáticamente.
- [x] Click en vacío deselecciona.
- [x] Guard anti-doble-disparo contra el gizmo: si el drag-start consumió el click, el select no se dispara.

### Bloque 2.5 — Iconos para entidades sin mesh ✅

- [x] Render 2D en screen-space via `ImDrawList` callback `ViewportPanel::setOverlayDraw` (sin assets externos ni shader nuevo). Círculo amarillo para `LightComponent`, cian para `AudioSourceComponent`.
- [x] Halo azul alrededor del icono si la entidad está seleccionada.
- [x] Iconos pickable por esfera de 0.6m (parte del mismo `pickEntity`).
- [x] Solo visibles en Editor Mode (early return en el overlay si `m_mode == Play`).

## Bloque 3 — Gizmo translate ✅

- [x] Overlay en `EditorApplication`: 3 flechas axis-aligned en screen-space (proyección world → screen), color R/G/B por eje.
- [x] Picking por cercanía 2D al segmento (threshold 8px). Hover no resalta — se evalúa solo en mouse-down.
- [x] Drag: `closestParam` entre dos líneas 3D (rayo del mouse vs eje del gizmo). Delta aplicado a `Transform.position`.

## Bloque 4 — Gizmo rotate + scale ✅

- [x] **Rotate**: 3 anillos (48 segmentos polyline) por eje. Sign del delta invertido según `dot(axis, cam_to_origin)` para que rotar a la derecha sienta natural desde cualquier ángulo.
- [x] **Scale**: 3 líneas cortas + cuadrado al final por eje; al arrastrar, factor aplicado al componente correspondiente (`scale.x`, `.y` o `.z`).
- [x] **Uniform scale** (feedback dev): cuadrado blanco al centro del gizmo de scale; drag multiplica los 3 componentes por `1 + deltaPx / k_armLen` (60px). Clamp >= 0.01.

## Bloque 5 — Hotkeys + UX ✅

- [x] `W`/`E`/`R` cambian modo. Ignorados si `ImGui::WantTextInput` (no pisan escribir en un input).
- [ ] ~~Esc cancela drag sin commit~~: diferido. El drag se aplica live, no hay staging; si se quiere cancelar hoy, el usuario deshace con el Inspector. Se contempla con undo/redo (Hito 22).
- [ ] ~~Indicator visual del modo~~: diferido. Los 3 modos se distinguen por la forma del gizmo (flechas vs anillos vs cajas). Si aparece demanda, se agrega un label.

## Bloque 6 — Tests ✅

- [x] `tests/test_picking.cpp`: 7 cases cubriendo scene vacía, ray central hitea mesh en origen, light sin mesh pickable por esfera, dos targets en línea (el más cercano gana), rayo al cielo no hitea, entidad detrás de cámara no hitea, entidad sin Transform no hitea (safety).
- [ ] ~~`tests/test_gizmo.cpp`~~: diferido. La math del gizmo (closestParam + project world→screen) es glm puro; el test de cobertura real sería end-to-end contra ImGui, lo cual queda para un futuro hito de tests de UI.

## Bloque 7 — Cierre ✅

- [x] Recompilar: `cmake --build build/debug --config Debug` limpio. Tests verdes: 106/106 (7 nuevos para picking).
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.13.0-hito13` + push.
- [x] Crear `docs/PLAN_HITO14.md` (Prefabs).

---

## Qué NO hacer en el Hito 13

- **No** multi-selección avanzada (rectangle, CTRL+click across hierarchy).
- **No** snap to grid (entra con Hito 14 prefabs o un toggle simple después).
- **No** undo/redo (Hito 22).
- **No** parent-space vs world-space toggle — siempre world por ahora.
- **No** handles en 2D (mover el plane gizmo). Solo 3 ejes puros + central uniform.
- **No** implementar ImGuizmo (lib externa) — hacerlo propio para entender la matemática y no depender.

---

## Decisiones durante implementación

- **Gizmo en screen-space con `ImDrawList`, no geometría GL**: flechas/anillos/cajas se dibujan como polylines 2D post-frame desde un callback del `ViewportPanel`. Ventaja: cero shaders nuevos, cero VBOs, cero depth-write para overlays. El precio: 1 frame de lag porque la cámara usada es la del frame anterior (`m_lastView`/`m_lastProjection`); imperceptible a la práctica.
- **Picking por AABB unitario escalado (no por mesh real)**: se asume que toda `MeshRendererComponent` ocupa `[-0.5, 0.5]^3` local, y `Transform.scale` define el tamaño world. Para el primitivo cubo es exacto; para modelos importados es una conservative hitbox. Suficiente para el editor; se considera `MeshFromAsset` cuando el picking contra malla curva se vuelva necesario.
- **Entidades sin mesh pickables por esfera de 0.6m**: `LightComponent` y `AudioSourceComponent` se pican contra una sphere en world-space. 0.6m es ~2× el tamaño visual del icono; minimiza "no le pego a la luz".
- **Click vs drag = threshold 4px**: en `ViewportPanel`, si el mouse se movió < 4px entre mouseDown y mouseUp, es click puro → dispara `ClickSelect`. Si se movió más, fue un drag (de cámara o de gizmo) y el select se descarta.
- **Uniform-scale handle = cuadrado blanco central**: en modo Scale, además de los 3 ejes hay un cuarto "eje" (index = 3) en el origen del gizmo que escala los 3 componentes a la vez. Factor = `1 + deltaPx / k_armLen` (60px), clamp >= 0.01.
- **Rotate sign por dot(axis, cam_to_origin)**: sin esto, rotar el anillo X sentía al revés si mirabas la escena desde -X. Se elige el signo del delta según la orientación de la cámara respecto al eje.
- **Inspector esconde scale/rotation sin MeshRenderer**: feedback del dev — no tiene sentido "escalar" una luz puntual, el control solo introducía confusión. Se muestran únicamente si la entidad tiene `MeshRendererComponent`.
- **Overlays ocultos en Play Mode**: iconos + gizmos son andamios de edición. Mostrarlos en Play rompía la inmersión; early return en el callback si `m_mode == Play`.
- **Hotkeys respetan `ImGui::WantTextInput`**: W/E/R no se disparan si el usuario está escribiendo en un input de texto (ej: renombrar entidad en Hierarchy).

---

## Pendientes que quedan para Hito 14 o posterior

- **Shift+click multi-selección** y rectangle select: diferido a Hito 14+.
- **Snap to grid** para el drag de gizmo: pedir a Hito 15 o un toggle simple.
- **Undo/redo**: Hito 22 dedicado.
- **Parent-space vs world-space toggle**: todavía siempre world.
- **Gizmo con handles 2D** (plano para mover en 2 ejes a la vez): diferido; con los 3 axis + uniform alcanza por ahora.
- **Label visual del modo activo** (W/E/R): las formas se distinguen solas; si hace ruido se agrega después.
- **Esc cancela drag sin commit**: requiere staging del valor previo; alineado con undo/redo del Hito 22.
- **Tests de math del gizmo (closestParam, world→screen)**: diferidos a un hito de tests de UI.
- **CharacterController, body-rotation sync y `MeshFromAsset`**: siguen arrastrándose desde el Hito 12.
