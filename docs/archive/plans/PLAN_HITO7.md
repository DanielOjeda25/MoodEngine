# Plan — Hito 7: Entidades, componentes, jerarquía

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 4.14 y 5.4 de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Reemplazar el modelo mapa-como-grid por un modelo escena-de-entidades (ECS):
- Una **`Scene`** es un contenedor de entidades (internamente `entt::registry`).
- Una **`Entity`** es una fachada liviana sobre un `entt::entity` + referencia al registro.
- Cada entidad tiene cero o más **componentes** de datos planos.
- Los **sistemas** (`RenderSystem`, `PhysicsSystem`) iteran entidades con ciertas combinaciones de componentes.

Componentes básicos del hito:
- `TransformComponent { position, rotation, scale }` — todas las entidades con presencia en el mundo.
- `MeshRendererComponent { meshHandle, textureId }` — entidades visibles.
- `CameraComponent { fov, near, far }` — futuro: cámara como entidad.
- `LightComponent { type, color, intensity }` — placeholder para el Hito 11.

Paneles del editor:
- **Hierarchy**: árbol de entidades. Click selecciona. Drag reordena / reparenta (si queda tiempo).
- **Inspector**: muestra los componentes de la entidad seleccionada con widgets de edición.

Entregable: al abrir el editor sin proyecto, la sala de prueba existe como escena de 29 entidades (una por tile sólido). El panel Hierarchy las lista. Clickear una muestra su Transform/MeshRenderer en el Inspector. Editar `position` o `scale` se refleja en el viewport.

---

## Bloque 0 — Pendientes arrastrados

- [x] Bloque 5 del Hito 6 (`editor.state.json`): si empieza a molestar tener que reabrir el proyecto cada vez, arrancar por acá. Si no, seguir diferido.
- [x] Diálogo de dirty-check (`pfd::message`) al Cerrar/Abrir/Nuevo cuando hay cambios sin guardar. Rápido y evita perder trabajo.

## Bloque 1 — EnTT como dependencia

- [x] CPMAddPackage para `skypjack/entt` (última versión estable, v3.13.x).
- [x] Linkearla a `MoodEditor` y a `mood_tests`.
- [x] Compilar un smoke test (crear registry, entity, asignar un componente trivial, leerlo) para confirmar que engancha.

## Bloque 2 — Fachada Entity + Scene

- [x] `src/engine/scene/Scene.{h,cpp}`: envuelve `entt::registry`. API pública:
  - `Entity createEntity(const std::string& name = {})` — crea entidad con componentes `TagComponent` (name) + `TransformComponent` por defecto.
  - `void destroyEntity(Entity)`.
  - `template<typename T, typename... Ts> void forEach(F&& f)` — itera todas las entidades que tienen los componentes `T, Ts...`.
- [x] `src/engine/scene/Entity.{h,cpp}`: contiene `entt::entity m_handle` + `Scene* m_scene` (o `entt::registry*`). API:
  - `template<typename T, typename... Args> T& addComponent(Args&&...)`.
  - `template<typename T> T& getComponent()`.
  - `template<typename T> bool hasComponent() const`.
  - `template<typename T> void removeComponent()`.
  - `operator bool()` / `operator==` / `entt::entity handle()`.
- [x] Esconder todo `entt::` fuera del impl: los headers del editor sólo incluyen `Scene.h` + `Entity.h`.

## Bloque 3 — Componentes básicos

- [x] `src/engine/scene/Components.h` (todos en un header liviano):
  - `TagComponent { std::string name; }`.
  - `TransformComponent { glm::vec3 position{0}; glm::quat rotation{1,0,0,0}; glm::vec3 scale{1}; mat4 worldMatrix() const; }`.
  - `MeshRendererComponent { MeshHandle mesh; TextureAssetId texture; }` — `MeshHandle` inicialmente alias de `IMesh*` (Hito 10 lo convierte en handle real).
  - `CameraComponent { float fov = 60.0f; float near = 0.1f; float far = 100.0f; }` (stub, sin uso todavía).
  - `LightComponent { enum Type { Directional, Point }; glm::vec3 color = {1,1,1}; float intensity = 1.0f; }` (stub para Hito 11).

## Bloque 4 — Migrar la sala de prueba a escena de entidades

- [x] `EditorApplication` gana un `std::unique_ptr<Scene>` al lado del `GridMap` actual. El `GridMap` se mantiene por compatibilidad con el serializer y las colisiones del Hito 4; el paso real a "sólo entidades" es más adelante (el serializer debería convertir el grid en entidades al cargar, y las colisiones empezar a iterar AABBs por componente).
- [x] `buildInitialTestMap()` además de llenar el `GridMap`, crea 29 entidades en la escena — una por tile sólido — con `TransformComponent` (posición = centro del tile, scale = tileSize) + `MeshRendererComponent { cubeMesh, texturaDelTile }`.
- [x] `GridRenderer::draw` se reemplaza/complementa con un `RenderSystem::draw(Scene&, ...)` que itera entidades con `Transform+MeshRenderer`.

## Bloque 5 — Panel Hierarchy

- [x] `src/editor/panels/HierarchyPanel.{h,cpp}`.
- [x] Recibe `Scene*` vía setter. Itera entidades y las muestra como `ImGui::Selectable` con el nombre del `TagComponent`.
- [x] Click selecciona la entidad. Guarda `Entity m_selected` compartido con el Inspector (via `EditorUI` o un `SelectionService` dedicado).
- [x] Integrar al dockspace default: a la izquierda del Viewport, por encima del Asset Browser.

## Bloque 6 — Panel Inspector funcional

- [x] `InspectorPanel` deja de ser placeholder. Recibe `Entity` seleccionada.
- [x] Por cada componente conocido, dibuja sus campos con widgets ImGui:
  - `TagComponent`: `InputText` del nombre.
  - `TransformComponent`: `DragFloat3` para position, rotation (como euler deg), scale.
  - `MeshRendererComponent`: texto del handle + botón para cambiar textura (abre selección rápida del AssetBrowser).
- [x] Marcar dirty al modificar cualquier valor (`markDirty()` del Hito 6).

## Bloque 7 — Tests

- [x] `tests/test_scene.cpp`: crear scene, agregar entity, check que `createEntity` setea `TagComponent` y `TransformComponent` por default.
- [x] `tests/test_entity.cpp`: addComponent/getComponent/hasComponent/removeComponent para un componente trivial (p.ej. `struct Counter { int v; }`).

## Bloque 8 — Cierre

- [x] Recompilar y verificar (build limpio + tests + editor arranca con Hierarchy poblado).
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.7.0-hito7` + push.
- [x] Crear `docs/PLAN_HITO8.md` (Lua scripting).

---

## Qué NO hacer en el Hito 7

- **No** migrar serialización a entidades todavía. `.moodmap` sigue siendo un grid; la conversión grid → entidades se hace al cargar.
- **No** integrar Jolt ni reemplazar `PhysicsSystem` actual. La colisión sigue contra el `GridMap`.
- **No** empezar con scripting (Hito 8).
- **No** agregar `RigidBodyComponent`, `ScriptComponent`, `AudioSourceComponent` — entran en sus hitos.
- **No** implementar undo/redo de cambios del Inspector (Hito 22).
- **No** exponer `entt::registry` fuera de la implementación de `Scene`.

---

## Decisiones durante implementación

Entradas detalladas en `DECISIONS.md` bajo fecha 2026-04-23 (Hito 7).

- **Fachada liviana**: `Entity` es 16 bytes (entt::entity + Scene*), `Scene` envuelve `entt::registry`. EnTT se incluye por `Entity.h`/`Scene.h` pero el resto del motor no depende directamente de `entt::`.
- **`TransformComponent` usa Euler deg, no quat**: simplifica la UI del Inspector. Si aparece gimbal lock en uso real, migrar a quat interno + conversión en accessors.
- **`Scene` derivada de `GridMap`, no authoritative**: el render sigue siendo `GridRenderer` sobre el grid. `RenderSystem` sobre entidades se mencionó en el plan pero se difiere: invertiría el modelo con poca ganancia visible hasta que haya geometría no-grid (Hito 10 con assimp). Scene existe para que Hierarchy/Inspector tengan con qué trabajar hoy.
- **Rebuild en-place del registry** (no reallocate de `Scene`): evita invalidar los `Scene*` de los paneles cada vez que cambia el mapa.
- **`emplace_or_replace` en `addComponent`** (en vez de `emplace`): más tolerante a doble-adds y cubre el uso "quiero que este componente exista con estos valores".
- **Edición en Inspector es ephemeral** en este hito: las ediciones no se persisten porque `.moodmap` solo conoce el grid. Cuando Scene sea authoritative, persistirán. Documentado como limitación explícita.

---

## Pendientes que quedan para Hito 8 o posterior

- **RenderSystem sobre Scene**. El plan Bloque 4 proponía migrar el render de `GridRenderer` a un `RenderSystem` que itere entidades. Se difirió: no aporta valor visible hasta que haya geometría no-grid. **Trigger**: Hito 10 (assimp) o Hito 11 (iluminación por entidad).
- **Scene authoritative + persistencia de ediciones del Inspector**. Hoy el `.moodmap` solo conoce el grid. Cuando el formato evolucione para guardar entidades con sus componentes, las ediciones del Inspector persistirán. **Trigger**: mismo que arriba.
- **Jerarquía padre-hijo** (ChildrenComponent / ParentComponent). Hierarchy hoy es plana. **Trigger**: cuando aparezca una entidad compuesta (prefab con subpartes, Hito 14).
- **Drag & reparent en Hierarchy** (arrastrar una entidad sobre otra). **Trigger**: mismo que arriba.
- **ECS-based drag & drop**: el drop de textura actual toca `m_map.setTile` + rebuildScene. Cuando Scene sea authoritative, el drop debería editar directamente el `MeshRendererComponent` de la entidad hovered. **Trigger**: Scene authoritative.
- **Undo/Redo del Inspector**. **Trigger**: Hito 22 (Undo/Redo general).
- **Combo de tipo Light con selector de color HDR** para futuro PBR. **Trigger**: Hito 17.
