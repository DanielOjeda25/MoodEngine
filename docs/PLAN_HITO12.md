# Plan — Hito 12: Física con Jolt

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md`. Sección 4.13 y 10 (Hito 12) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Reemplazar el `PhysicsSystem` casero AABB-vs-grid del Hito 4 por **Jolt Physics** real (Horizon Zero Dawn 2 / Doom Eternal usan Jolt). Esto desbloquea:

- Rigid bodies con **gravedad** y dinámica real, no solo "no-atravesar".
- **Colliders** distintos de AABB: capsule (jugador), sphere (proyectiles), convex hull (props), mesh (mapas detallados).
- **Capsule controller** para el jugador (slope handling, step-up).
- Eventos de colisión (callbacks que el gameplay puede consumir; útil cuando Lua de Hito 8 pida triggers).
- Trayectorias predecibles para rigid bodies (cajas que se voltean al ser empujadas, etc.).

No-goals del hito: ragdoll, soft body, cloth, vehicles, IK, networking de física.

---

## Criterios de aceptación

### Automáticos

- [x] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos. Jolt es vocal con warnings; agregar pragma para que sus headers no contaminen `/W4` en nuestros TUs.
- [x] Log del canal `physics` (nuevo): `Jolt inicializado (X bodies, Y constraints, Z solver iters)`.
- [x] Tests: helpers de conversión entre `glm::vec3` y `JPH::Vec3`, smoke test de inicializar Jolt + crear un rigid body + step + leer la posición sin crashear.
- [x] Cierre limpio (sin leaks reportados por Jolt).

### Visuales

- [ ] ~~En Play Mode el jugador usa un **CharacterController** de Jolt: gravedad real, no atraviesa paredes (igual que Hito 4) pero ahora también responde a slope, step-up de tiles bajos, salto con SPACE.~~ **Diferido.** Ver pendientes.
- [x] `Ayuda > Agregar caja física demo` spawnea una entidad con `RigidBodyComponent { dynamic, box collider }` que cae al piso por gravedad y se queda apoyada.
- [ ] ~~Empujar la caja con el cuerpo del jugador la voltea/desliza.~~ **Diferido** junto con CharacterController.
- [ ] ~~`F2` (nuevo toggle) dibuja debug de Jolt.~~ **Diferido** junto con CharacterController.

---

## Bloque 0 — Pendientes arrastrados del Hito 11

- [ ] **Hot-reload de shaders** (Bloque 6 diferido del Hito 11): sigue diferido.
- [ ] **Preview 3D de meshes en AssetBrowser**: sigue diferido.
- [ ] **Persistencia de `AudioSourceComponent`**: sigue diferido; entra en Hito 14 con Scene authoritative.

## Bloque 1 — Dependencia Jolt

- [ ] CPMAddPackage `jrouwe/JoltPhysics v5.x.x`. Opciones: `TARGET_UNIT_TESTS OFF`, `TARGET_HELLO_WORLD OFF`, `TARGET_PERFORMANCE_TEST OFF`, `TARGET_VIEWER OFF`, `TARGET_SAMPLES OFF`, `OVERRIDE_CXX_FLAGS OFF` (no queremos que pise `/W4`), `INTERPROCEDURAL_OPTIMIZATION OFF` (Debug build no aprovecha LTO; ahorra tiempo).
- [ ] `target_link_libraries(MoodEditor PRIVATE Jolt)`. Mismo para `mood_tests`.
- [ ] Si Jolt fuerza warnings sobre `/W4`, agregar `target_compile_options(Jolt PRIVATE /W0)` o equivalente para silenciarlos sin pisar `/WX` (no lo tenemos activado todavía).

## Bloque 2 — PhysicsWorld wrapper

- [ ] `src/engine/physics/PhysicsWorld.{h,cpp}`: RAII sobre `JPH::PhysicsSystem`. Constructor inicializa allocator, tempAllocator, jobSystem, broadphase + objectLayerFilter. Destructor cierra limpio. Singleton no — `EditorApplication` posee la instancia.
- [ ] Layer scheme mínimo: `Static` (geometría del mapa) + `Dynamic` (props y player). Filter table que dice qué colisiona con qué.
- [ ] `step(dt, collisionSteps=1)`: avanza la simulación. Llamado por `EditorApplication` solo en Play Mode.

## Bloque 3 — RigidBodyComponent + collider data

- [ ] `Components.h`: `RigidBodyComponent { JPH::BodyID id, float mass, BodyType (Static|Dynamic|Kinematic), Shape (Box|Sphere|Capsule|MeshFromAsset), glm::vec3 halfExtents (Box) | float radius (Sphere) | ... }`. La `BodyID` la asigna el `PhysicsSystem` cuando crea el body.
- [ ] `src/systems/PhysicsSystem.{h,cpp}` (refactor del Hito 4): el viejo `moveAndSlide` se conserva temporalmente para fallback; el nuevo flujo crea bodies en Jolt al ver entidades con `RigidBodyComponent`. `update(scene, dt)` hace step + sincroniza positions de body → `TransformComponent`.

## Bloque 4 — Character controller

- [ ] ~~`src/systems/CharacterController.{h,cpp}`: wrapper de `JPH::CharacterVirtual`.~~ **Diferido** post-Hito 12.
- [ ] ~~`EditorApplication::updateCameras` Play Mode: usar el controller en lugar de moveAndSlide.~~ **Diferido.**
- [ ] ~~Salto con SPACE: impulso vertical cuando grounded.~~ **Diferido.**

> **Diferido al Hito 13+.** El jugador sigue usando el `moveAndSlide` AABB-vs-grid del Hito 4. Los rigid bodies dinámicos (cajas demo) funcionan perfecto; lo único que queda por migrar es la presencia del jugador en Jolt. Se difiere por tiempo — implementar `CharacterVirtual` correctamente es trabajo no trivial (handling de slope, step-up, ground detection, jump state machine) y el demo actual ya muestra que Jolt está vivo (la caja cae, los tiles son colliders).
> Tracker concreto: Hito 13 o 14, junto con gizmos (para poder seleccionar y mover la caja con mouse) o prefabs.

## Bloque 5 — Mapa → bodies estáticos

- [ ] `rebuildSceneFromMap` ahora crea, además de las entidades visuales `Tile_X_Y`, un body estático en Jolt por cada tile sólido (`Box(tileSize/2)` posicionado en world). Cuando se cierra el proyecto / se abre otro, los bodies se descartan.
- [ ] Drop de textura sigue siendo edit visual; el body estático no cambia (cambiar el tipo del tile sí reconstruye).

## Bloque 6 — Inspector y demo

- [ ] InspectorPanel: sección RigidBody (tipo, mass, shape, half extents). En modo Play algunos campos quedan readonly (Jolt es authoritative).
- [ ] `Ayuda > Agregar caja física demo`: spawnea una entidad con `Transform(0, 6, 0)` + `MeshRenderer(cubo + grid.png)` + `RigidBodyComponent(Dynamic, Box, 1m)`. Mass = 5 kg. La caja cae por gravedad.
- [ ] Toggle `F2`: debug draw de Jolt (colliders en wireframe + velocity vectors).

## Bloque 7 — Serialización

- [ ] `SavedRigidBody` opcional en `SavedEntity`: `{ type, shape, half_extents | radius, mass }`. **No** se serializa la pose actual (Jolt es authoritative en runtime; al cargar el body se crea con el Transform de la entidad).
- [ ] `k_MoodmapFormatVersion` 3 → 4 (v3 sin `rigid_body` se carga igual).

## Bloque 8 — Tests

- [ ] ~~`tests/test_physics.cpp`: helpers de conversión vec3 + step básico.~~ **Diferido.** Jolt init es pesado (~15 ms factory + JobSystem + threads) y agregar Jolt al target `mood_tests` arrastra ~40 MB de lib estática extra a cada rebuild. Se difiere hasta Hito 13 o cuando aparezca un caso de test que lo exija.
- [x] Round-trip de RigidBody en `test_scene_serializer`: type, shape, halfExtents, mass preservados para Dynamic + Static. Suite total: 99/442.

## Bloque 9 — Cierre

- [ ] Recompilar, tests verdes, demo funcional.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.12.0-hito12` + push.
- [ ] Crear `docs/PLAN_HITO13.md` (gizmos + selección).

---

## Qué NO hacer en el Hito 12

- **No** ragdoll (Hito 19 con animación esquelética).
- **No** soft bodies, cloth.
- **No** vehicles.
- **No** networking. Física determinista local.
- **No** quitar todavía el `PhysicsSystem` AABB del Hito 4 — déjalo como fallback hasta que Jolt esté validado.
- **No** trigger volumes complejos (un AABB sensor sí, callbacks Lua entran cuando un script lo pida).

---

## Decisiones durante implementación

Detalle en `DECISIONS.md` (fecha 2026-04-24 bajo Hito 12).

- **Jolt v5.2.0 vía CPM con `USE_STATIC_MSVC_RUNTIME_LIBRARY OFF`**: Jolt default usa `/MTd`; nuestro proyecto `/MDd`. Mezclar produce LNK2038. Forzamos a Jolt a dynamic runtime para alinear.
- **PhysicsWorld como facade sin exponer tipos Jolt**: el header usa forward decls (`JPH::PhysicsSystem`) y `u32` como `BodyID` (internamente `GetIndexAndSequenceNumber()`). Evita arrastrar `<Jolt/...>` al resto del motor.
- **Map tiles → static bodies vía RigidBodyComponent**: en vez de que `PhysicsWorld` tenga una API aparte para "bodies del mapa", `rebuildSceneFromMap` agrega `RigidBodyComponent(Static, Box)` a cada tile y `updateRigidBodies` lo materializa como cualquier otra entidad. Uniforme.
- **Pose NO persistida en `SavedRigidBody`**: Jolt es authoritative en runtime. Al cargar un proyecto, el body se crea en la posición del `TransformComponent`. Persistir también `body.position` duplicaría la fuente de verdad.
- **CharacterController diferido**: `JPH::CharacterVirtual` requiere handling correcto de slopes, step-up, ground detection, contact callbacks — trabajo no trivial. El `moveAndSlide` del Hito 4 sigue moviendo al jugador; la física "real" es de los rigid bodies dinámicos del demo. Trade-off: demo audible de gravedad listo sin bloquear el hito.
- **Bodies de Scene tests no linkean Jolt**: `test_scene_serializer` del round-trip de `RigidBodyComponent` solo testea la capa de serialización (JSON ↔ struct). No toca `PhysicsWorld`. Eso evita sumar Jolt a `mood_tests`.

---

## Pendientes que quedan para Hito 13 o posterior

- **CharacterController (Bloque 4)**: diferido. Trigger natural: Hito 13 (gizmos de selección) o Hito 14 (prefabs + gameplay real).
- **Debug draw de Jolt (F2)**: diferido junto con CharacterController. Requiere implementar `JPH::DebugRenderer` que emita líneas al `OpenGLDebugRenderer`.
- **`test_physics.cpp`**: diferido. Linkear Jolt a `mood_tests` arrastra ~40 MB por target. Trigger: cuando aparezca lógica no trivial en el lado C++ (conversions, handles, mgmt de bodies por scene).
- **Rotación del body → Transform.rotationEuler**: hoy solo sync de posición. Cuando una caja ruede (necesita colisiones contra otras cajas o terreno inclinado), queremos ver la rotación.
- **Shape `MeshFromAsset`**: hoy solo Box/Sphere/Capsule. Para colisiones precisas de modelos importados (Hito 10), convertir el `MeshAsset` a `JPH::MeshShape` (estático) o descomposición convexa (dinámico). Trigger: cuando algún asset real lo pida.
- **Triggers / sensors**: bodies que detectan sin colisionar. Útiles para Lua scripting (`onEnter`, `onExit`). Trigger: Hito 14+ con gameplay.
- **Pendientes arrastrados del Hito 11**: hot-reload shaders, preview 3D meshes, normal mapping, multi-directional, persistencia AudioSource. Todos siguen en el tracker.
