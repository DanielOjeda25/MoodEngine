# PLAN_HITO_F2H68 — Auto-ragdoll por impacto vehicle ↔ NPC

> **Estado:** En curso (2026-05-19)
> **Predecesor:** F2H67 (vehicles) + F2H66 (ragdolls) — este hito conecta los dos.
> **Motivación:** En la prueba de F2H67 el dev preguntó *"si un vehiculo, si choca un NPC con trigger ragdoll, este caera o sentira el impacto?"*. Verificado: hoy NO. Se anotó como entrada `1.-4` del BACKLOG (alta presión).

---

## Objetivo

Cuando el chassis de un vehículo (o cualquier body Dynamic con masa/velocidad suficiente) impacta a un body cuyo Entity tiene `RagdollComponent::state == Animated`, automáticamente transicionar al estado `Ragdolling` y aplicar un impulse derivado de la velocidad relativa. Sin requerir Lua/script — gameplay puro estilo GTA SA.

## Estado pre-hito

- `RagdollComponent` (`Components.h:287`): activación 100% manual via Lua `ragdoll.enable(tag, impulse?)` o `state = Ragdolling` en C++.
- `PhysicsWorld` (`PhysicsWorld_Internal.h`): NO tiene `JPH::ContactListener` registrado. Único "evento" físico es overlap de `TriggerVolumeComponent` (sensor, no contact real).
- No existe mapa `BodyID → entt::entity` explícito — cada subsistema (RigidBody, Ragdoll, Vehicle) guarda su lookup interno pero no hay índice unificado para "dado un BodyID arbitrario, qué Entity es".

---

## Diseño

### Mapa BodyID → Entity (infra cross-subsistema)

Vivirá en `PhysicsWorld::Impl` como `std::unordered_map<u32, entt::entity> bodyToEntity`. Lo mantienen los **callers** (PhysicsSystem para RigidBody, VehicleSystem para chassis, RagdollSystem para cada part-body) llamando a una API nueva:

```cpp
void registerBodyEntity(u32 bodyId, entt::entity e);
void unregisterBodyEntity(u32 bodyId);
entt::entity entityOfBody(u32 bodyId) const;  // null si no registrado
```

Para no acoplar `PhysicsWorld.h` a EnTT, el handle será `u32` (raw entt::entity static_cast). Nivel de gameplay sigue el patrón existente de `EditorPlayMode::m_playerMountedVehicleEntity`.

### ContactListener

Clase nueva `physics_internal::ContactListener` en `PhysicsWorld_Internal.h` (o archivo propio si crece). Override de `OnContactAdded(const Body& b1, const Body& b2, const ContactManifold& m, ContactSettings& s)`:

1. Leer `m.GetWorldSpaceContactPointOn1/2`, `b1/b2.GetLinearVelocity()`, masas.
2. Calcular velocidad relativa proyectada sobre la normal: `vrel · n`.
3. Si `|vrel · n| < k_minImpactSpeed` (default 4 m/s) → return.
4. Estimar `impulseScalar = |vrel · n| * (m1*m2)/(m1+m2)` (impulso elástico equivalente, formula estándar).
5. Determinar cuál de los 2 bodies es "víctima animada": lookup `entityOfBody(id1)` y `entityOfBody(id2)`, chequear `RagdollComponent::Animated`. Si ambos lo son, encolar para ambos. Si ninguno → return.
6. Push `RagdollImpactEvent{victimEntity, impulseWorld = -n * impulseScalar * k_arcadeFactor, attackerVelocity}` a una cola thread-safe (`std::vector` + mutex; Jolt sólo llama desde el hilo del step, pero defensivo). `k_arcadeFactor` = 0.3 default.

**Nota Jolt**: el callback NO puede mutar bodies / cambiar shapes / crear entities. Sólo modificar `ContactSettings` (fricción / restitución de ese contacto). Por eso encolamos para drenar fuera.

### Drain en RagdollSystem

`RagdollSystem::tick` drenea la cola **antes** del materialize lazy:

```cpp
auto events = physicsWorld.drainImpactEvents();
for (const auto& ev : events) {
    Entity victim = scene.entityFromHandle(static_cast<entt::entity>(ev.victimEntity));
    if (!victim || !victim.hasComponent<RagdollComponent>()) continue;
    auto& rd = victim.getComponent<RagdollComponent>();
    if (rd.state != RagdollComponent::State::Animated) continue;  // ya ragdoll o disabled
    rd.state         = RagdollComponent::State::Ragdolling;
    rd.spawnImpulse  = ev.impulseWorld;
    rd.dirty         = true;  // disparar materialize en el mismo tick
}
```

Después del drain, el flujo existente de materialize (que ya soporta `spawnImpulse` via `applyRagdollImpulse` post-create) hace el resto.

### Tuning de umbrales

| Constante | Default | Notas |
|---|---|---|
| `k_minImpactSpeed` | 4 m/s relativo | Evita falsos disparos por roce. Tunear si NPCs se caen al rozarlos. |
| `k_arcadeFactor` | 0.3 | El impulse físico exacto los manda volando absurdo; 0.3 da feel "GTA SA". Configurable global via `PhysicsWorld::setRagdollImpactFactor(f)`. |
| `k_minAttackerMass` | 50 kg | Char player no dispara ragdoll a otros chars por chocarlos caminando. |

---

## Bloques

### Bloque A — ContactListener + body↔entity map (infra)

- `PhysicsWorld_Internal.h`:
  - `class ContactListener : public JPH::ContactListener` skeleton (sólo `OnContactAdded`, otras vacías).
  - `Impl::bodyToEntity` map + `Impl::contactListener` instance.
  - `Impl::impactEventQueue` (vector + mutex) + struct `RagdollImpactEvent { u32 victimEntity; glm::vec3 impulseWorld; }`.
- `PhysicsWorld.h`: API pública `registerBodyEntity / unregisterBodyEntity / entityOfBody / drainImpactEvents / setRagdollImpactFactor`.
- `PhysicsWorld.cpp` ctor: `physicsSystem->SetContactListener(&impl->contactListener)`.
- `PhysicsWorld.cpp` `createBody/destroyBody`: register/unregister auto (los caller dueños lo settean luego con `registerBodyEntity` porque createBody no sabe la entity). **Decisión:** dejamos `createBody` neutral; los sistemas (PhysicsSystem / VehicleSystem / RagdollSystem) llaman `registerBodyEntity` después.

### Bloque B — Callback impl + cola drain API

- `ContactListener::OnContactAdded` con la lógica del diseño.
- Acceso al `bodyToEntity` map + scene RagdollComponent → necesita scene injection. **Problema:** `PhysicsWorld` no conoce el `Scene`. Solución: el callback NO consulta `RagdollComponent` (no puede). Sólo encola `{bodyId, impulseWorld}` y deja que el drain en `RagdollSystem` resuelva entity + filtre por state. Ajuste de struct: `RagdollImpactEvent { u32 victimBodyId; glm::vec3 impulseWorld; }`.
- `drainImpactEvents()`: swap out el vector bajo mutex, devolver copia.

### Bloque C — Registro de bodies + drain en RagdollSystem

- `PhysicsSystem` (RigidBody): al crear un body, llamar `physicsWorld.registerBodyEntity(bodyId, entityHandle)`. Al destroyBody, unregister.
- `VehicleSystem`: registrar chassis body al `createVehicle`. Destroy en `destroyVehicle` (que ya tiene el handle via `chassisBodyId`).
- `RagdollSystem`: registrar cada part-body. La API `PhysicsWorld::createRagdoll` devuelve `ragdollId` único; necesitamos enumerar los `BodyID` internos del Ragdoll. Jolt los expone via `JPH::Ragdoll::GetBodyIDs()`. Wrapper nuevo: `PhysicsWorld::ragdollBodyIds(ragdollId) -> std::vector<u32>`. RagdollSystem llama esto post-create y registra cada uno con la mismaentity (todos los parts de un ragdoll mapean al mismo Entity owner).
- `RagdollSystem::tick`: drain pre-materialize. Resolver `bodyToEntity[ev.victimBodyId]` → si entity tiene `RagdollComponent::Animated`, setear `state=Ragdolling + spawnImpulse + dirty=true`.

### Bloque D — Tests + sample

- `tests/test_physics_ragdoll_impact.cpp` (nuevo):
  - Helper: crear PhysicsWorld + scene minimal + 1 entity vehicle + 1 entity NPC animado.
  - Test 1: vehicle a 10 m/s impacta NPC parado → after step + drain, NPC state == Ragdolling.
  - Test 2: vehicle a 1 m/s (< threshold) NO dispara ragdoll.
  - Test 3: 2 NPCs animados chocan caminando (m_attackerMass < 50 kg) → ninguno cae.
  - Test 4: NPC ya Ragdolling, vehicle lo vuelve a impactar → idempotente, no re-dispara (sigue Ragdolling).
- Sample: agregar 1 entity NPC con ragdoll Mixamo en `vehicle_demo.moodmap` parado en el medio del piso, para que el dev valide visualmente "atropellar".

### Bloque E — Cierre

- `docs/hitos/F2H68.md` análogo a F2H67.md (objetivo + diseño + bloques + decisiones).
- `docs/HITOS.md`: entry F2H68 en Sub-fase 2.4.
- `docs/ESTADO_ACTUAL.md`: 0.1 = F2H68, 0.2 = F2H67, "En curso" + "Próximo" actualizados.
- `docs/DECISIONS.md`: decisiones clave (ContactListener vs polling raycast, factor 0.3 arcade, drain en RagdollSystem vs PhysicsSystem genérico).
- Move `docs/PLAN_HITO_F2H68.md` → `docs/archive/plans/`.
- Backlog: remover entrada `1.-4` (ya implementada).
- Commits agrupados al final + tag `v1.55.0-fase2-hito68` + push.

---

## Riesgos / open questions

- **Player vs vehicle**: si el player camina contra un NPC con `RagdollComponent::Animated`, el `k_minAttackerMass = 50 kg` evita falso disparo (char default ~70 kg, **revisar**). Si el char está modelado más liviano, el threshold del speed (4 m/s) podría no alcanzar — el char camina a 5.5 m/s. Si emerge, subir threshold a 7 m/s o agregar flag explícito "puedeRagdollizar" per-body.
- **Ragdoll-to-ragdoll**: si un ragdoll cae sobre otro ragdoll, no debería re-disparar nada (ambos ya Ragdolling). El gate `state == Animated` en el drain lo cubre.
- **Vehicle vs vehicle**: bodies dynamic chocan sin disparar nada (ninguno es Animated). OK.
