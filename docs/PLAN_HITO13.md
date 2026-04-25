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

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Tests: helpers de ray-AABB intersect, ray-gizmo-axis picking (ray vs capped cylinder para las flechas).
- [ ] Cierre limpio.

### Visuales

- [ ] Click en una entidad en el viewport la resalta en Hierarchy (selection sync).
- [ ] Shift+Click agrega a la selección — **opcional** para este hito; si no entra, queda diferido.
- [ ] Con entidad seleccionada, gizmo aparece sobre el pivote.
- [ ] Arrastrar cada eje del gizmo modifica la propiedad correcta y se ve en vivo.
- [ ] Inspector refleja los cambios en tiempo real.

---

## Bloque 0 — Pendientes arrastrados del Hito 12

- [ ] **CharacterController de Jolt** (Bloque 4 del Hito 12): integración del jugador como `JPH::CharacterVirtual`. Si hay bandwidth al final, entra; si no, se difiere de nuevo.
- [ ] **Rotación del body → Transform.rotationEuler**: hoy solo sincronizamos position. Cuando las cajas empiecen a rodar por colisiones (Hito 13 con cajas apiladas) se va a notar.
- [ ] **Shape `MeshFromAsset`**: colisiones precisas para modelos importados. Útil para el ray-cast de click-to-select (si queremos picking preciso contra la pirámide, no solo su AABB).

## Bloque 1 — Raycast pickable

- [ ] `src/engine/scene/ScenePick.{h,cpp}` (análogo a `ViewportPick` del Hito 6 pero para entidades): `pickEntity(scene, assets, ray_origin, ray_dir) -> std::optional<Entity>`. Usa AABB de la mesh o sphere de la light.
- [ ] Integrar con Jolt si es más rápido: `PhysicsWorld::rayCast(origin, dir) -> BodyID`. Luego mapear BodyID → Entity via tabla inversa.
- [ ] Fallback a AABB ECS-side si el body no existe (entidades sin RigidBody).

## Bloque 2 — Click-to-select

- [ ] `ViewportPanel` captura click izquierdo sobre la imagen (diferenciar drag-de-gizmo vs click-select).
- [ ] Al click, el viewport construye el ray desde la cámara y llama al ScenePick.
- [ ] Resultado → `EditorUI::setSelectedEntity(e)`. Hierarchy y Inspector reaccionan automáticamente.
- [ ] Click en vacío deselecciona.

### Bloque 2.5 — Iconos de sprite para entidades sin mesh visible

Feature que pidió el dev al cerrar el Hito 12. Entidades como `LightComponent` (Point/Directional) o `AudioSourceComponent` hoy son invisibles en el viewport. Como consecuencia:
- No hay forma de click-to-select una luz sin seleccionarla por Hierarchy.
- No sabés dónde está parada la fuente hasta que te fijás en el Transform.

- [ ] `assets/textures/icons/light_point.png`, `light_directional.png`, `audio_source.png` (32×32 PNG con fondo transparente). Generadas con script en `tools/gen_editor_icons.py` (o bajadas CC0 de fontawesome/similares).
- [ ] `src/engine/render/opengl/OpenGLBillboardRenderer.{h,cpp}` (o integrado en `OpenGLDebugRenderer`): dibuja sprites view-aligned en world-space. Tamaño constante en pantalla (scale por distancia cámara).
- [ ] `EditorApplication::renderSceneToViewport` emite un billboard por cada entidad con `LightComponent`/`AudioSourceComponent` sin `MeshRendererComponent` (las que sí lo tienen ya son visibles).
- [ ] El icono es pickable: el ScenePick también testea ray vs quad-del-billboard. Click sobre el icono selecciona la entidad.
- [ ] Solo en Editor Mode — en Play Mode los iconos se ocultan (no contaminan la experiencia final).

> Alternativa al .png: dibujar los íconos como shapes procedurales (esfera wireframe para point, flecha para directional, altavoz para audio) con `OpenGLDebugRenderer`. Menos bonito pero no requiere assets; decidir durante implementación.

## Bloque 3 — Gizmo translate

- [ ] `src/engine/scene/Gizmo.{h,cpp}`: render en viewport de 3 flechas axis-aligned sobre el Transform de la entidad seleccionada. Color por eje (RGB).
- [ ] Raycast ray ↔ flecha (capped cylinder): al hover, tint brighter; al click + drag, entra modo translate.
- [ ] Drag: proyectar el delta del mouse sobre el plano perpendicular al eje + plano de la vista. Aplicar delta a `Transform.position`.
- [ ] Liberar click: commit. `markDirty()`.

## Bloque 4 — Gizmo rotate / scale

- [ ] Rotate: 3 anillos (torus-ish) por eje. Drag aplica rotación al eje Euler correspondiente.
- [ ] Scale: 3 cajas por eje. Drag aplica factor al `Transform.scale`.

## Bloque 5 — Hotkeys + UX

- [ ] `W` / `E` / `R` cambian modo (translate/rotate/scale). Estado en `EditorUI`.
- [ ] Indicator visual del modo actual (label + color en toolbar o menú).
- [ ] Esc cancela drag en curso sin commit.

## Bloque 6 — Tests

- [ ] `tests/test_picking.cpp`: ray-AABB, ray-sphere, ray-capped-cylinder.
- [ ] `tests/test_gizmo.cpp`: proyección de delta sobre eje / plano (math pura).

## Bloque 7 — Cierre

- [ ] Recompilar, tests verdes, demo visible (seleccionar la caja demo, moverla con el gizmo, ver la caja apoyarse en el nuevo lugar cuando entra a Play).
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.13.0-hito13` + push.
- [ ] Crear `docs/PLAN_HITO14.md` (prefabs).

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

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 14 o posterior

_(llenar al cerrar el hito)_
