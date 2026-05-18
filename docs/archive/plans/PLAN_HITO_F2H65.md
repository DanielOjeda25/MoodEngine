# PLAN HITO F2H65 — Jolt constraints (joints)

> **Estado**: ARRANCANDO (2026-05-18).
> **Tag previsto**: `v1.52.0-fase2-hito65`.
> **Origen**: primer hito de Sub-fase 2.4 (Física avanzada) del plan original `PLAN_FASE2.md`. F2H64 cerrado, follow-ups movidos a BACKLOG. Dev pidió: *"avanza para que este sea un motor fisico, realista, como el source"*.

---

## 🎮 Mecánicas (lo que vive el dev)

### Qué entrega F2H65

- **Puertas con bisagra estilo Half-Life / Source.** Brush pared + brush puerta + Joint tipo Hinge con limits angulares (ej. 0°–90°). La puerta rota empujada por el char controller o disparos. Opcionalmente con friction simulando resistencia.
- **Cuerdas / cadenas colgantes.** Series de cuerpos dinámicos conectados por joints Distance. Anclados a un punto fijo (un Static), oscilan con gravedad. Sirven para colgadas tipo escaleras, lámparas, cables del techo.
- **Brazos articulados / palancas / barreras.** Segmentos conectados por Hinges en cadena. Barrera de parking que rota empujada por un trigger; brazo grúa con 2 segmentos.
- **Pivote libre 3 DoF.** Dos cuerpos conectados por un punto. Rotación libre en X/Y/Z. Sirve para esferas colgadas tipo péndulo Newton, juguetes.

### Cómo se integra

- Componente nuevo `JointComponent`: tipo (Hinge/Distance/Point) + referencia a 2 entities + parámetros locales (pivote, eje, limits, distancia objetivo).
- Inspector con sección "Joint" — dropdown tipo + drop target para body B + sliders/spinners.
- Debug overlay: línea entre los pivots de A y B, color por tipo (cyan Hinge, amarillo Distance, magenta Point).
- Persistencia aditiva en `.moodmap` (el slot `SavedJoint` ya existía precableado en `SceneSerializer.h`).

### Qué NO entra (queda agendado)

- **Ragdolls** — F2H66 si seguimos plan original. Requieren joints + esqueleto + preset humanoide.
- **Vehicle physics** — F2H67. Wheels constraints específicos del paper Jolt vehicles.
- **Slider / Pistón** — sin demanda específica.
- **Cone-twist / 6DOF configurable** — Hinge tendrá `limit min/max` simple; cone-twist espera demanda.
- **Break force** — joints v1 son irrompibles. Si emerge demanda (vidrio que se rompe, cuerdas tensadas), F2H68+.
- **Joint motors** (que un joint empuje activamente, ej. motor de un robot) — pasivo v1.

---

## 🧱 Bloques de ejecución

### Bloque A — PhysicsWorld API para joints (~2h)

- Agregar a `PhysicsWorld.h` 3 métodos:
  ```cpp
  u32 createHingeConstraint(u32 bodyA, u32 bodyB,
                              glm::vec3 pivotWorld, glm::vec3 axisWorld,
                              f32 limitMinDeg, f32 limitMaxDeg);
  u32 createDistanceConstraint(u32 bodyA, u32 bodyB,
                                 glm::vec3 pivotA_world, glm::vec3 pivotB_world,
                                 f32 minDistance, f32 maxDistance);
  u32 createPointConstraint(u32 bodyA, u32 bodyB,
                              glm::vec3 pivotWorld);
  void destroyConstraint(u32 constraintId);
  ```
- En `PhysicsWorld.cpp`: incluir `<Jolt/Physics/Constraints/HingeConstraint.h>`, `DistanceConstraint.h`, `PointConstraint.h`. Crear `JPH::HingeConstraint` etc. via `JPH::PhysicsSystem::AddConstraint`. Mapping `u32 constraintId → JPH::Ref<JPH::Constraint>` en `Impl`.
- `Impl::physicsSystem` queda privado; los métodos públicos hacen el bridge.
- Tests headless: crear 2 bodies + hinge + step + assert que body B rote alrededor del eje.

### Bloque B — JointComponent (~1h)

- En `Components.h` debajo de `RigidBodyComponent`:
  ```cpp
  struct JointComponent {
      enum class Type : u8 { Hinge = 0, Distance = 1, Point = 2 };
      Type type = Type::Hinge;
      u32 targetEntity = 0;        // raw entt::entity handle de body B
      glm::vec3 pivotLocal{0.0f};  // en local space de A
      glm::vec3 axisLocal{0,1,0};  // solo Hinge
      f32 limitMinDeg = -180.0f;   // solo Hinge
      f32 limitMaxDeg = 180.0f;
      f32 minDistance = 0.0f;       // solo Distance
      f32 maxDistance = 1.0f;
      u32 constraintId = 0;        // llenado por PhysicsSystem
      bool dirty = true;           // re-create constraint cuando cambia
  };
  ```
- Inspector category: "Physics" (junto con RigidBody).

### Bloque C — PhysicsSystem crea/destruye joints (~2h)

- En `systems/physics/PhysicsSystem.cpp`:
  - Loop nuevo: `scene.forEach<JointComponent>(...)`. Si `dirty` o `constraintId==0`: destruir el constraint previo (si existe) + crear nuevo via `PhysicsWorld`. Resolver targetEntity → bodyId via `RigidBodyComponent::bodyId`. Skip si A o B aún no tienen body.
  - Lifecycle: cuando una entity con JointComponent se destruye, destroyConstraint.
  - Idempotencia: si nada cambia, no-op (lookup constraintId existente).
- Acceso al world space pivot: cache para no recalcular per frame (joint static after init).

### Bloque D — Inspector UI Joint (~2h)

- `InspectorPanel_Physics.cpp` gana sección `renderJointSection(Entity e)`:
  - Combo Type (Hinge/Distance/Point).
  - **Drop target** para targetEntity: acepta payload `MOOD_ENTITY` desde el Hierarchy panel. Muestra tag de la entity actual + botón Clear.
  - Hinge: DragFloat3 pivotLocal + DragFloat3 axisLocal + SliderFloat limitMinDeg/limitMaxDeg.
  - Distance: DragFloat3 pivotLocal + DragFloat minDistance/maxDistance.
  - Point: DragFloat3 pivotLocal solo.
  - Sliders/inputs marcan `joint.dirty = true` para que PhysicsSystem recree.
  - Undo via `pushEditIfDone` por cada campo.
  - i18n keys nuevas (es + en).

### Bloque E — Persistencia .moodmap (~1.5h)

- `SceneSerializer.h` ya tiene `std::optional<SavedJoint> joint;` precableado. Implementar la estructura:
  ```cpp
  struct SavedJoint {
      std::string type{"hinge"};           // "hinge" | "distance" | "point"
      std::string targetTag;                // tag de la entity B (lookup post-load)
      glm::vec3 pivotLocal{0.0f};
      glm::vec3 axisLocal{0,1,0};
      f32 limitMinDeg = -180.0f;
      f32 limitMaxDeg = 180.0f;
      f32 minDistance = 0.0f;
      f32 maxDistance = 1.0f;
  };
  ```
- `EntitySerializer.cpp`: write + parse. Target por **tag string**, no entity handle (portable entre sesiones).
- `SceneLoader.cpp`: re-resolver target tag → entity handle al cargar. Warning si no encuentra.

### Bloque F — Debug overlay (~30 min)

- En `SceneRenderer_Render.cpp` o en `DebugRenderer` extension:
  - Iterar JointComponent + dibujar línea world A pivot → world B pivot.
  - Hinge: cyan (0, 1, 1). Distance: amarillo (1, 1, 0). Point: magenta (1, 0, 1).
  - Solo cuando F1 debug overlay está activo (no contaminar viewport en producción).

### Bloque G — Sample shipado: puerta Hinge (~1.5h)

- Mapa demo `assets/maps/joints_demo.moodmap` (generado programáticamente o autoredactado):
  - Suelo (Floor brush).
  - Pared con un hueco (3 brushes recortados).
  - Puerta (brush thin) + JointComponent Hinge:
    - target = pared.
    - axis = (0, 1, 0) eje vertical.
    - limits = 0°–90° (abre hacia un solo lado).
  - PlayerSpawn para que el char controller pueda empujarla.
- Entry "Cargar demo de joints" en Welcome modal o menú Help/Demos.

### Bloque H — Tests headless (~1.5h)

- `test_physics_world.cpp` (nuevo): `createHingeConstraint` → tick → body B rota; `createDistanceConstraint` → tick → distancia constante; `createPointConstraint` → tick → body B no se traslada respecto al pivot.
- `test_scene_serializer.cpp` o partial: roundtrip JSON con SavedJoint en cada tipo.
- `test_components.cpp` (si existe) o test nuevo: JointComponent defaults sensatos.

### Bloque I — Validación visual + cierre (~1h)

- Dev abre `joints_demo.moodmap`:
  1. Empuja la puerta con el char → rota hasta el limit.
  2. Mueve la pared (cambiando el target body) → la puerta sigue colgada.
  3. Cambia el tipo a Point en runtime → libera rotación 3 DoF.
- Updates docs: HITOS one-liner + ESTADO_ACTUAL 0.1 + DECISIONS + crear `docs/hitos/F2H65.md` + archivar este plan.
- Commit + tag `v1.52.0-fase2-hito65` + push.

---

## Total estimado

**9 bloques A-I, ~13h reales.** Hito mediano-grande. Lo más riesgoso:

- **Bloque A**: primera integración con Jolt constraints. Cuidar refs (`JPH::Ref<JPH::Constraint>`) para que no se liberen mientras Jolt los tiene en la simulación.
- **Bloque C**: timing de creación. El constraint requiere que ambos bodies existan ya. Necesitamos un retry pass si A y B se crean en frames distintos.
- **Bloque E**: lookup por tag tiene casos de borde (tag duplicado, tag vacío).

---

## Decisiones de scope (confirmadas con dev 2026-05-18)

1. **3 tipos en v1**: Hinge, Distance, Point. Las primitivas de motor físico realista mainstream (Source ships estos 3, Unity también).
2. **Body B obligatorio**. Para anclar al "mundo" se conecta a un body Static. Más simple que tratar el caso "world frame" especial.
3. **Hinge con limits opcionales**. Defaults a (-180, 180) = libre rotación; el dev los ajusta a (0, 90) para puerta de un solo lado.
4. **Lookup target por tag string**. Portable entre sesiones + escena. Menos brittle que entity handle persistido.
5. **Joints v1 irrompibles** (sin break force). Si emerge demanda → hito futuro.
6. **Sin motors** (joints pasivos). Si emerge demanda → hito futuro.

---

## Riesgos identificados

- **Order of creation**: si JointComponent se crea antes que los RigidBody, el constraint no se puede crear. Mitigación: PhysicsSystem hace retry mientras `dirty=true` hasta que A y B tengan bodyId.
- **Tag lookup ambiguo**: dos entities con el mismo tag → el joint conecta al "primero". Mitigación: warning en log + el dev resuelve renombrando.
- **Limits invertidos** (min > max en Hinge): Jolt asume min ≤ max. Mitigación: clamp en setter del Inspector + warning si el archivo `.moodmap` está corrupto.
- **Joints + Save/Load (Hito 41)**: el constraint NO se persiste en `.moodsave` (gameplay state) — se recrea desde el `.moodmap` al cargar. La pose y velocidades de los bodies sí se persisten igual que F2H41 (transform + linVel + angVel).

---

## Referencias

- Jolt Physics docs — Constraints: https://jrouwe.github.io/JoltPhysics/index.html (Constraints section).
- Source SDK: phys_hinge / phys_lengthconstraint / phys_ballsocket — las 3 primitivas mainstream.
- Real-Time Collision Detection 4th ed., cap 14 (Constraints).
