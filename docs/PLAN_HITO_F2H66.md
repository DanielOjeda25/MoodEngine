# PLAN F2H66 — Ragdolls (auto-build desde skeleton Mixamo)

> **Sub-fase**: 2.4 — Física avanzada. F2H65 entregó joints; F2H66 los compone automáticamente sobre el skeleton de un NPC para flopping físico realista al morir.
>
> **Origen**: plan original F2H24. Decisiones del dev (2026-05-18) tras presentación en mecánicas:
> 1. Disparo del ragdoll: **manual via Lua + impulse al activar** (estilo HL2, el cadáver vuela en la dirección del disparo). La auto-detección HP→0 queda en el script Lua del NPC, no es scope F2H66.
> 2. **Sin vuelta a animación** — una vez ragdoll, queda ragdoll para siempre.
> 3. **Pocos simultáneos** (1-5) — apoyamos sleeping de Jolt para que los muertos en reposo no consuman CPU.
> 4. **Auto-build desde Mixamo** — convención de nombres `mixamorig:*` da mapping bone→body sin config manual.
> 5. **Solo Play Mode** — no preview en Editor.

---

## Splits de archivos (memoria: soft 500 / hard 800 LOC por .cpp/.h)

Antes de empezar code, revisión de tamaños actuales:

- `PhysicsWorld.cpp` **850 LOC** — YA sobre hard cap. **Split obligatorio antes de F2H66**.
- `EntitySerializer.cpp` 655 LOC — soft excedido, no crítico para F2H66 (+40 LOC).
- `Components.h` 628 LOC — soft excedido, no crítico (+30 LOC).
- `EditorRenderPass_Overlay.cpp` 570 LOC — soft excedido, no crítico (+80 LOC).
- `SceneLoader.cpp` 574 LOC — soft excedido, no crítico (+40 LOC).
- `EditorScene.cpp` 458 LOC — OK margen para 1 invocación.

**Acción 1 (pre-bloque-A)**: split de `PhysicsWorld.cpp`:
- `PhysicsWorld.cpp` queda con Hito 12 / F2H40 (createBody/destroyBody/step/bodyPosition/raycast/...).
- `PhysicsWorld_Constraints.cpp` (mueve F2H65: createHinge/Distance/PointConstraint, destroyConstraint, constraintCount).
- `PhysicsWorld_Ragdoll.cpp` (NUEVO en bloque C, toda la API ragdoll).

Mismo patrón que `EditorUI_*.inl`, `InspectorPanel_*.cpp`, `EditorRenderPass*.cpp` ya tienen.

**Acción 2 (bloque D)**: la lógica del RagdollSystem va a un módulo dedicado `src/systems/physics/RagdollSystem.{h,cpp}` y NO inline en EditorScene.cpp. EditorScene.cpp solo invoca `RagdollSystem::tick(...)`. Mantiene EditorScene magro + el sistema testeable headless.

---

## Decisiones técnicas anticipadas

- **Backend: `JPH::Ragdoll`** (nativo de Jolt). Tiene API completa: `RagdollSettings` declarativo (Parts + constraints + collision groups), `Ragdoll::AddToPhysicsSystem`, `GetBodyState`, `Stabilize`. Reinventarlo desde joints sueltos sería trabajo perdido.
- **Bodies son capsules** por defecto (mejor llenado del cuerpo humano que boxes; performance OK con N=15). Override per-bone si emerge necesidad.
- **Constraints SwingTwist** para shoulders/hips/spine/neck (3 DOF con cono + twist); **Hinge** para elbow/knee (1 DOF). Wrist/ankle/finger NO se ragdollean en v1 (parte del body parent rígido).
- **Mass distribution**: dado un `totalMass` (default 70 kg), cada body se asigna por **volumen del capsule** (proporcional a `π·r²·h + 4/3·π·r³`).
- **Collision filter**: bodies del mismo ragdoll NO colisionan entre sí (sino el ragdoll se "explota" por contactos internos). Usamos `GroupFilterTable` de Jolt.
- **Sync ragdoll → mesh**: cada frame post-step, leemos pose mundial de cada body, computamos `localPose = inverse(global_parent) · global_self`, y feedeamos a `computeSkinningMatrices`. Bones sin body (manos/pies/dedos) heredan del parent ragdolleado.

---

## Bloques

### Bloque 0 — Split de `PhysicsWorld.cpp` (preparatorio)

**Goal**: bajar `PhysicsWorld.cpp` de 850 → ~520 LOC antes de agregar nada nuevo. No introduce funcionalidad nueva; es refactor mecánico.

- Crear `src/engine/physics/world/PhysicsWorld_Constraints.cpp`.
- Mover ahí las 4 funciones de F2H65: `createHingeConstraint`, `createDistanceConstraint`, `createPointConstraint`, `destroyConstraint`, `constraintCount`.
- `PhysicsWorld.cpp` queda con bodies + step + raycast + sync.
- Update `CMakeLists.txt` con el archivo nuevo.
- Compilar + tests passing (sin cambio funcional, deben quedar verdes igual).

**LOC delta**: PhysicsWorld.cpp 850 → 520, PhysicsWorld_Constraints.cpp nuevo 330.

### Bloque A — Layout helper (puro, sin física)

**Goal**: dado un `Skeleton` con convención Mixamo, identificar qué bones forman el ragdoll y calcular las shape/size de cada body.

- Nuevo archivo `src/engine/physics/ragdoll/RagdollLayout.{h,cpp}`.
- `struct RagdollLayout { vector<RagdollBone> bones; }` con cada `RagdollBone { int boneIndex; CollisionShape shape; vec3 size; ParentBoneIdx; ConstraintType; SwingLimits/TwistLimits/HingeLimits; mass; }`.
- `RagdollLayout buildMixamoLayout(const Skeleton& skel, f32 totalMass = 70.0f, f32 limbRadius = 0.05f)`. Mapping hardcoded:
  - Hips → capsule vertical (root)
  - Spine/Spine1/Spine2 → capsule vertical
  - Neck → capsule chica
  - Head → capsule pequeña o sphere
  - LeftShoulder/RightShoulder → SKIP (parte del torso)
  - LeftArm/RightArm → capsule (upper arm)
  - LeftForeArm/RightForeArm → capsule
  - LeftHand/RightHand → SKIP (parte del forearm)
  - LeftUpLeg/RightUpLeg → capsule
  - LeftLeg/RightLeg → capsule
  - LeftFoot/RightFoot → SKIP
- Constraint config hardcoded por par parent→child:
  - Hips→Spine: SwingTwist (twist ±30°, swing 45°)
  - Spine→Spine1: SwingTwist (limites más estrechos)
  - Neck→Head: SwingTwist (90° cono)
  - Hips→UpLeg: SwingTwist (90° cono, twist ±30°)
  - UpLeg→Leg: Hinge X axis (0 a 150° = rodilla)
  - Spine2→Arm: SwingTwist (160° cono, twist ±45°)
  - Arm→ForeArm: Hinge (0 a 140° = codo)
- **Cap defensivo**: si el skeleton no es Mixamo (no encuentra ningún bone con prefix `mixamorig:`), retorna layout vacío + log warn. El RagdollSystem hace skip si layout vacío.
- Tests unitarios: fixture Skeleton con bones Mixamo standard → layout con 11 bodies + 10 constraints. Edge case: skeleton sin Mixamo → layout vacío.

**LOC est.**: ~250 cpp + ~80 h. Tests ~150. **Soft cap respetado.**

### Bloque B — RagdollComponent

```cpp
struct RagdollComponent {
    enum class State : u8 { Animated = 0, Ragdolling = 1 };
    State state = State::Animated;

    /// Config tweak por entity (persiste).
    f32  totalMass = 70.0f;
    f32  limbRadius = 0.05f;
    bool useGravity = true;

    /// Impulse aplicado al body torso al transicionar a Ragdolling.
    /// Usa world coords. Magnitud típica: 5-50 N·s.
    glm::vec3 spawnImpulse{0.0f};

    // --- Runtime (NO persiste) ---
    u32 ragdollId = 0;     // handle del PhysicsWorld; 0 = no creado
    bool dirty = true;      // true al spawn / al editar tweaks
};
```

Persistencia aditiva en `.moodmap` (sin schema bump). `state` no se persiste — siempre arranca Animated al cargar (los NPCs muertos persistidos NO es scope; F2H66 v1 asume save antes de muerte).

**LOC est.**: ~40 h adicional + ~30 EntitySerializer + ~20 SceneLoader.

### Bloque C — PhysicsWorld Ragdoll API (en archivo nuevo `PhysicsWorld_Ragdoll.cpp`)

- `u32 createRagdoll(const RagdollLayout& layout, const vector<glm::mat4>& boneGlobals, const Transform& entityTransform, f32 totalMass, bool useGravity)`.
  - Construye `JPH::RagdollSettings` desde el layout: 1 Part por bone con shape capsule + transform world inicial = `entityTransform.worldMatrix() * boneGlobals[i]` (igual la pose del Animator).
  - Crea constraints SwingTwist/Hinge entre Parts según layout.
  - Setea `GroupFilterTable` para deshabilitar self-collision.
  - Llama `Ragdoll::AddToPhysicsSystem` con `EActivation::Activate`.
  - Devuelve handle u32 al map interno `unordered_map<u32, Ref<Ragdoll>>`.
- `void destroyRagdoll(u32 id)` — `Ragdoll::RemoveFromPhysicsSystem` + erase.
- `bool readRagdollPose(u32 id, vector<glm::mat4>& outBoneGlobals)` — lee `Body.GetWorldTransform()` por cada Part, devuelve global poses en mesh-space (para volver a feedear computeSkinningMatrices con localPose derivada).
- `void applyRagdollImpulse(u32 id, int partIndex, const glm::vec3& impulseWorld)` — para el spawn impulse al torso.

**LOC est.**: ~200 cpp + ~30 h.

### Bloque D — RagdollSystem (módulo dedicado `src/systems/physics/RagdollSystem.{h,cpp}`)

EditorScene::tickPhysics solo invoca `RagdollSystem::tick(*m_scene, *m_physicsWorld, *m_assets)`. La implementación del loop vive en `RagdollSystem.cpp`.

Loop nuevo después de los joints F2H65:

```cpp
m_scene->forEach<RagdollComponent, AnimatorComponent, SkeletonComponent, TransformComponent>(
    [&](Entity, RagdollComponent& rag, AnimatorComponent& anim,
        SkeletonComponent& skel, TransformComponent& tf) {
        // Transición Animated -> Ragdolling.
        if (rag.state == RagdollComponent::State::Ragdolling && rag.ragdollId == 0) {
            // 1. Build layout desde skeleton (acceso via mesh.skeleton).
            const auto layout = buildMixamoLayout(meshAsset->skeleton, rag.totalMass, rag.limbRadius);
            if (layout.bones.empty()) { rag.state = State::Animated; return; }
            // 2. Get bone global poses del Animator (último frame).
            vector<glm::mat4> globals = computeBoneGlobals(meshAsset->skeleton, anim.currentLocalPose);
            // 3. Create ragdoll.
            rag.ragdollId = m_physicsWorld->createRagdoll(layout, globals, tf, rag.totalMass, rag.useGravity);
            // 4. Disable Animator (sigue allocado pero no avanza).
            anim.playing = false;
            // 5. Apply spawn impulse al torso (Part del Spine).
            if (glm::length(rag.spawnImpulse) > 0.001f) {
                m_physicsWorld->applyRagdollImpulse(rag.ragdollId, layout.torsoPartIndex, rag.spawnImpulse);
            }
            rag.dirty = false;
        }

        // Sync runtime: lee poses del ragdoll, escribe en skel.
        if (rag.state == State::Ragdolling && rag.ragdollId != 0) {
            vector<glm::mat4> ragdollGlobals;
            if (m_physicsWorld->readRagdollPose(rag.ragdollId, ragdollGlobals)) {
                applyRagdollPoseToSkeleton(meshAsset->skeleton, layout, ragdollGlobals, skel.skinningMatrices);
            }
        }
    });
```

Función nueva `applyRagdollPoseToSkeleton` en `RagdollLayout.cpp`: itera bones del skel; si está en el layout, usa la pose del ragdoll; si no, hereda del parent ragdolleado (rebuildea localPose via `inverse(parentGlobal) * selfGlobal`).

Cleanup en `DeleteEntityCommand`: callback `RagdollCleanup` análogo a `BodyCleanup`/`ConstraintCleanup` de F2H40/F2H65.

**LOC est.**: ~150 EditorScene + ~80 RagdollLayout (apply func) + ~30 DeleteEntityCommand.

### Bloque E — Lua bindings

Tabla nueva `entity:` en LuaBindings:
- `entity:enableRagdoll(impulseX, impulseY, impulseZ)` — setea `state=Ragdolling` + `spawnImpulse=(x,y,z)` + `dirty=true`. No-op si ya está ragdolling.
- `entity:isRagdolling() -> bool`.

Wrapper sobre EditorUI (que conoce el SelectionSet) sería raro; mejor binding global por tag o por entity handle. Patrón existente para inventory ya hace `inventory.give(entity_tag, item, qty)` — copiamos: `ragdoll.enable(entity_tag, ix, iy, iz)`.

Decisión final ya: **`ragdoll.enable("Tag")`** + opcional impulse via tabla `{x=0, y=10, z=-5}`. Más Lua-idiomático que múltiples args.

**LOC est.**: ~80 cpp + 10 docs.

### Bloque F — Debug overlay 3D

En `EditorRenderPass_Overlay.cpp` bajo F1:
- Forall ragdolls activos: dibujar wireframe capsule por body (color por estado: amarillo=Animated lab pose, naranja=Ragdolling).
- Líneas finas entre body parent→child para visualizar constraint pivots (igual estilo F2H65 Bloque F).

Costo: tiene que iterar bodies via `readRagdollPose` o expandir la API a `forEachRagdollPart`. Voy con `forEachRagdollPart(handle, callback)` para no copiar arrays grandes por frame.

**LOC est.**: ~80 overlay + ~30 PhysicsWorld API.

### Bloque G — Sample en `narrative_demo.moodmap`

- Agregar `RagdollComponent` al `NPC` existente (totalMass=60, limbRadius=0.04, useGravity=true).
- Agregar nuevo TriggerComponent al NPC con halfExtents (1.5, 1, 1.5) (ya existe del F2H48.1 — reusamos).
- Script Lua nuevo `scripts/ragdoll_demo.lua`: al `on_trigger_enter` con tag del player, llama `ragdoll.enable("NPC", {x=0, y=200, z=-300})` — el NPC se desploma cuando el player se acerca.
- Si el dev quiere algo más teatral: spawn de varios cubos físicos arriba que caen al activar (out of scope, anecdotal).

**LOC est.**: ~5 JSON map + ~15 Lua + actualización demo_intro dialog opcional.

### Bloque H — Tests headless

- `test_ragdoll_layout.cpp` (nuevo):
  - Fixture Skeleton Mixamo standard → `buildMixamoLayout` retorna 11 bodies + 10 constraints.
  - Skeleton vacío → layout vacío.
  - Skeleton sin Mixamo (bones con nombres distintos) → layout vacío + warn.
  - Mass distribution: suma de masses por bone == totalMass (±0.5%).
  - `applyRagdollPoseToSkeleton`: poses input X → skinning matrices output que matchean.
- `test_physics_ragdoll.cpp` (nuevo, runtime Jolt):
  - `createRagdoll` con layout válido → handle no-zero + bodies activos + sleep deshabilitado en activación.
  - `applyRagdollImpulse` al torso → torso se mueve en la dirección del impulse tras 1 step.
  - `readRagdollPose` retorna size correcto.
  - `destroyRagdoll` idempotente.
  - Ragdoll bajo gravedad: tras 60 ticks cae < 2 m verticalmente (no atravieza el infinito).

**LOC est.**: ~250 layout + ~200 runtime.

### Bloque I — Validación + cierre

1. Probar `narrative_demo.moodmap`: caminar hacia el NPC → trigger se dispara → NPC se desploma con impulse + flopping físico realista.
2. Verificar que el ragdoll **respeta los limits** (codos no se doblan al revés, cuello no rota 360°).
3. Verificar **sleep**: NPC quieto tras 5 seg → tracy shows 0 ticks de simulación de su body (Jolt sleep).
4. Confirmar **save/load**: ragdoll en mid-flop NO se persiste como ragdoll (el dev guarda PRE-evento si quiere preservar el momento).
5. Commit + tag `v1.53.0-fase2-hito66`.
6. Update HITOS/ESTADO_ACTUAL/DECISIONS/hitos/F2H66.md + archivar plan.

---

## LOC total estimado

~1900 cpp + ~300 h + ~600 tests = **~2800 LOC**. Hito mediano-grande. Estimación: **6-9 horas** distribuidas.

## Limitaciones agendadas para futuros hitos (sub-fase 2.4)

- **Vuelta animación ↔ ragdoll** (`disableRagdoll` + IK transition). Mencionada en docs/BACKLOG si emerge necesidad.
- **Partial ragdoll** (golpe al brazo lo dobla, NPC sigue vivo). Plan original F.E.A.R.-style. Diferido.
- **Ragdoll persistente en `.moodmap`** (guardar el estado mid-flop). Diferido — pocos casos reales.
- **Hit reactions** (impulso a un body específico sin matar al NPC). Out of scope F2H66; encajaría en partial ragdoll.
- **Per-mesh ragdoll config** (skeleton no-Mixamo). Out of scope F2H66.

## Riesgos identificados

1. **Mass distribution / scale del mesh** — si el NPC fue importado con scale != 1 (typical Mixamo con scale 0.01 para cm→m), los capsule sizes pueden quedar mal. Mitigación: leer `Transform.scale` del entity y aplicar al sizing.
2. **Bone names no-Mixamo** — Fox.glb tiene su propio naming. Si el dev mete un Fox al narrative_demo, el ragdoll va a estar vacío. Por eso la regla "layout vacío + warn" es defensiva.
3. **Jolt Ragdoll API**: cambios entre versiones. Lock al commit actual de Jolt; bump consciente si hay update.
4. **Animator vs Ragdoll race**: el AnimationSystem corre antes que tickPhysics. Cuando ragdoll está activo, Animator debe estar pausado o el sync ragdoll→skinning queda pisado. El plan: setear `anim.playing = false` ANTES de la primera transición resuelve esto.
