# PLAN F2H67 — Vehicle physics estilo GTA San Andreas

> **Sub-fase**: 2.4 — Física avanzada. F2H66 cerró ragdolls (plan F2H24); F2H67 ataca **plan original F2H25** — Jolt Vehicle Constraint con drivable cars desde el viewport.
>
> **Sabor objetivo**: GTA San Andreas — **arcade-ish, no simulación**. Cita del dev (2026-05-19): *"me gustan los autos de gta san andreas, se manejan facil y tienen fisicas mas que suficiente"*. Tracción alta (no derrapes fáciles), suspensión blanda pero estable, motor con torque generoso, brakes fuertes, dirección responsiva.
>
> **Backend**: `JPH::VehicleConstraint` + `JPH::WheeledVehicleController` nativos de Jolt. Tienen engine + transmission + differentials + per-wheel suspension + tire friction model. Reimplementar a mano sería tirar la inversión que Jolt ya hizo.

---

## Decisiones de UX/arquitectura confirmadas

1. **Ensamblaje del vehículo**: 1 entity "chassis" con `VehicleComponent` + 4 child-entities "wheel". `VehicleSystem` escribe las Transforms de los 4 wheel children cada frame (rotación + suspensión). Permite swappear meshes de ruedas en el editor sin tocar physics. Descartado: skinned mesh único (rígido, requiere import pipeline).
2. **Input vía Lua**: `vehicle.set_input(tag, throttle, brake, steer, handbrake)` por frame. Mismo patrón que el resto (player movement, dialog choice, etc.). El gameplay layer controla cuándo/cómo se conduce. Descartado: hardcoded engine-level input.
3. **Cámara**: switch automático al entrar al auto (F) → "vehicle chase cam" third-person orbitando detrás-arriba. F sale, vuelve al CharacterController. La cam del player se preserva (no se destruye al montar).
4. **Tuning persistente**: asset nuevo `.moodvehicle` (JSON). Referenciado por path desde `VehicleComponent`. Permite reusar el mismo "Banshee_SA" en N mapas sin duplicación.

---

## Splits de archivos (memoria: soft 500 / hard 800 LOC)

Revisión actual (2026-05-19, post-F2H66):

| Archivo | LOC | Estado | Acción F2H67 |
|---|---|---|---|
| `Components.h` | 665 | soft excedido | OK con +50 (VehicleComponent ligero, fields chicos) — atender en AUDIT futuro |
| `EntitySerializer.cpp` | 680 | soft excedido | OK con +30 — atender en AUDIT futuro |
| `PhysicsWorld.cpp` | 627 | soft excedido pero margen | OK — el código nuevo va a `PhysicsWorld_Vehicle.cpp` aparte |
| `EditorScene.cpp` | 470 | OK | Solo 1 invocación a `VehicleSystem::tick` |
| `LuaBindings.cpp` | 387 | OK | Bindings nuevos en archivo dedicado `LuaBindings_Vehicle.cpp` |
| `SceneLoader.cpp` | 574 | soft excedido | OK con +30 — atender en AUDIT futuro |

**No requiere pre-split** — todos los crecimientos van a archivos nuevos o son menores en archivos ya excedidos.

---

## Decisiones técnicas anticipadas

- **Backend `JPH::WheeledVehicleController`** (no `MotorcycleController` ni `TrackedVehicleController` — solo 4 wheels en v1).
- **Defaults SA-style**:
  - Mass del chassis: 1500 kg.
  - Center of mass: bajo (offset -0.2 m en Y desde el centro geométrico) — evita flips.
  - Wheels: radius 0.35 m, width 0.2 m, suspension max length 0.5 m, frecuencia 1.5 Hz, damping 0.5.
  - Engine: max torque 500 Nm, max RPM 6000, 5 gears (1: 2.66, 2: 1.78, 3: 1.30, 4: 1.0, 5: 0.74), final drive 3.42.
  - Brake torque: 1500 Nm. Handbrake torque (ruedas traseras): 4000 Nm.
  - Steering: max angle 35°, lerp speed 4.0 (responsiva).
  - Tire friction: lateral 1.4, longitudinal 1.6 (alta — SA no derrapa).
- **Driveline**: 4WD por default (más estable que RWD para arcade). Diferencial central 50/50, diff frontal/trasero open.
- **Sync mesh ↔ physics**: cada frame post-step, `VehicleSystem` lee `chassis.worldTransform` → entity Transform; para cada wheel lee `wheel.transform_local_to_chassis` + steering angle + spin angle → child-entity Transform (4 wheels).
- **Mount/dismount**: `VehicleSeatComponent` opcional en el chassis (offset local del asiento). Al montar, el player entity se "esconde" (`Visible = false`) + se setea `playerInVehicle = entityHandle del auto`. Input handler hace dispatch: si `playerInVehicle != 0` → escribir input del vehículo; si `0` → input normal del CharacterController.
- **Camera switch**: `EditorScene::Camera` gana un `CameraMode { Free, Player, Vehicle }`. `Vehicle` orbita al chassis con offset (default 5 m atrás, 2 m arriba, FOV 65°). Mouse Y rota pitch (clamp ±60°), mouse X yaw libre. Sin orbit, sigue la dirección del vehículo (auto-follow).

---

## Bloques

### Bloque 0 — Pre-split (preparatorio)

**Goal**: no requiere acción según la revisión arriba. Si en mid-hito un archivo cruza el hard cap 800, splitear ahí.

### Bloque A — `VehicleConfig` (puro, sin Jolt)

**Goal**: struct declarativa con TODO el tuning de un vehículo + helper que construye defaults SA-style. Header puro testeable headless.

- Nuevo `src/engine/physics/vehicle/VehicleConfig.{h,cpp}`.
- `struct WheelConfig { vec3 attachLocal; f32 radius; f32 width; f32 suspMaxLen; f32 suspFreq; f32 suspDamp; f32 forwardFriction; f32 lateralFriction; bool driven; bool steered; bool handbraked; }`.
- `struct EngineConfig { f32 maxTorque; f32 maxRPM; f32 minRPM; std::vector<f32> gearRatios; f32 finalDriveRatio; f32 brakeTorque; f32 handbrakeTorque; }`.
- `struct VehicleConfig { vec3 chassisHalfExtents; f32 chassisMass; vec3 centerOfMassLocal; std::array<WheelConfig, 4> wheels; EngineConfig engine; f32 maxSteerAngleDeg; f32 steerLerpSpeed; }`.
- `VehicleConfig makeDefaultSA()` con los números arriba.
- Tests: `test_vehicle_config.cpp` — defaults OK, mass conservación, wheels distinguibles por nombre.

**LOC est.**: ~250 cpp + ~150 h. Tests ~100.

### Bloque B — `VehicleComponent` + persistencia `.moodmap`

```cpp
struct VehicleComponent {
    /// Path al .moodvehicle (relative a assets/). Asset_id en runtime.
    std::string configPath;

    /// Input state — Lua lo escribe cada frame.
    f32 inputThrottle  = 0.0f;  // [0, 1]
    f32 inputBrake     = 0.0f;  // [0, 1]
    f32 inputSteer     = 0.0f;  // [-1, 1] left/right
    f32 inputHandbrake = 0.0f;  // [0, 1]

    // --- Runtime (NO persiste) ---
    u32 vehicleId = 0;     // handle del PhysicsWorld; 0 = no creado
    std::array<u32, 4> wheelEntityHandles{0, 0, 0, 0};  // child entities
    bool dirty = true;
};

struct VehicleSeatComponent {
    glm::vec3 seatOffsetLocal{0.0f, 0.6f, 0.2f};
};
```

Persistencia aditiva. `VehicleSeatComponent` opcional (si no está, mount usa default).

**LOC est.**: ~30 Components.h + ~40 EntitySerializer + ~30 SceneLoader.

### Bloque C — `PhysicsWorld_Vehicle.cpp` (wrapper Jolt)

```cpp
u32  createVehicle(const VehicleConfig& cfg,
                   const glm::mat4& initialWorldTransform);
void destroyVehicle(u32 vehicleId);
void setVehicleInput(u32 vehicleId, f32 throttle, f32 brake,
                     f32 steer, f32 handbrake);

struct VehicleState {
    glm::mat4 chassisWorld;
    std::array<glm::mat4, 4> wheelWorlds;  // local-to-chassis composed
    f32 forwardSpeed;
    bool grounded;
    int gear;
    f32 engineRPM;
};
bool readVehicleState(u32 vehicleId, VehicleState& out) const;

void applyVehicleImpulse(u32 vehicleId, const glm::vec3& impulseWorld);
u32  vehicleCount() const;
```

Construye `JPH::VehicleConstraintSettings` desde `VehicleConfig`. Crea chassis body (Dynamic, BoxShape de `chassisHalfExtents`, mass del config) + `WheeledVehicleController` con 4 wheels configurados. Storage: `unordered_map<u32, VehicleEntry>` con `{ JPH::Ref<JPH::VehicleConstraint>, JPH::Body* chassisBody }`.

**LOC est.**: ~500 cpp + ~50 h adicional en `PhysicsWorld.h`.

### Bloque D — `VehicleSystem` (módulo dedicado)

`src/systems/physics/VehicleSystem.{h,cpp}`. `EditorScene::tickPhysics` solo invoca `VehicleSystem::tick(...)` después del step.

Loop:

```cpp
m_scene->forEach<VehicleComponent, TransformComponent>(
    [&](Entity e, VehicleComponent& veh, TransformComponent& tf) {
        // 1. Materialize si dirty.
        if (veh.dirty && veh.vehicleId == 0) {
            const VehicleConfig* cfg = m_assets->getVehicleConfig(veh.configPath);
            if (cfg == nullptr) return;
            veh.vehicleId = m_physicsWorld->createVehicle(*cfg, tf.worldMatrix());
            // Spawn 4 wheel child entities con MeshRenderer placeholder
            // (o detectar las que ya existen como children por tag "Wheel_FL/FR/RL/RR").
            // ...
            veh.dirty = false;
        }
        // 2. Push input.
        m_physicsWorld->setVehicleInput(veh.vehicleId, veh.inputThrottle,
                                          veh.inputBrake, veh.inputSteer,
                                          veh.inputHandbrake);
        // 3. Read state -> escribir Transforms.
        VehicleState st;
        if (m_physicsWorld->readVehicleState(veh.vehicleId, st)) {
            tf.position = vec3(st.chassisWorld[3]);
            tf.rotationEuler = eulerFromMat4(st.chassisWorld);
            for (int i = 0; i < 4; ++i) {
                if (veh.wheelEntityHandles[i] == 0) continue;
                Entity wheel = m_scene->entityFromHandle(...);
                auto& wtf = wheel.getComponent<TransformComponent>();
                // Apply world transform de la rueda (composed chassis * wheel local).
            }
        }
    });
```

Cleanup en `DeleteEntityCommand`: callback `VehicleCleanup` análogo a F2H40/65/66.

**LOC est.**: ~350 cpp + ~50 h + ~20 DeleteEntityCommand.

### Bloque E — Lua bindings

```lua
vehicle.set_input(tag, {throttle=0.5, brake=0, steer=-0.3, handbrake=0})
vehicle.get_speed(tag) -> float (forward speed m/s)
vehicle.is_grounded(tag) -> bool
vehicle.respawn(tag, {x=,y=,z=}, rotY)
vehicle.gear(tag) -> int
vehicle.rpm(tag) -> float
```

Archivo nuevo `src/engine/scripting/bindings/LuaBindings_Vehicle.cpp`. Patrón consistente con `ragdoll.*` y `inventory.*` (lookup por tag).

**LOC est.**: ~150 cpp + agregar a tests CMakeLists.

### Bloque F — Player ↔ Vehicle mount/dismount + chase cam

**Game-feel core. Único bloque con validación visual obligatoria del dev.**

- Nuevo flag en `EditorScene` (o `PlayerState` si existe): `EntityHandle playerMountedVehicle = 0`.
- Detección "F cerca del auto": en `EditorApplication_Run.cpp` (Play Mode), si `F` se presiona Y `playerMountedVehicle == 0` Y hay un vehicle dentro de un radio (3m) del player → mount. Si `playerMountedVehicle != 0` → dismount.
- Mount: `player.Visible = false` + `playerMountedVehicle = vehicleHandle`. Camera mode → Vehicle.
- Dismount: `player.position = vehicleWorld * seatOffset + (0, 0.5, 1.5)` (offset al lado) + `Visible = true` + `playerMountedVehicle = 0`. Camera → Player.
- Input dispatch: si `playerMountedVehicle != 0` → WASD escribe `inputThrottle/inputBrake/inputSteer` del vehicle (W=throttle, S=brake, A/D=steer). Space=handbrake. Sin overlap con CharacterController.
- **Camera "chase cam"**: nueva clase / modo en `EditorScene::Camera`. Sigue la posición del chassis + offset (5 m atrás, 2 m arriba en local space del chassis). Yaw del mouse rota la cam alrededor del auto (no del auto). Pitch clamp ±60°. Sin input de mouse N segundos → "auto-follow": yaw lerpea hacia la dirección del chassis.

**LOC est.**: ~400 EditorApplication_Run + ~100 EditorScene Camera + ~30 player setup.

### Bloque G — Asset `.moodvehicle` + editor panel + Banshee_SA sample

- Asset type nuevo. JSON con campos del `VehicleConfig` serializado. Path standard `assets/vehicles/<name>.moodvehicle`.
- `AssetManager::getVehicleConfig(path)` — load + cache (igual patrón que mesh/material).
- Panel nuevo `VehicleEditorPanel` opcional o tab en Asset Browser para tunear el config (DragFloat de los parámetros + sample drive preview — diferido si LOC explota).
- 1 sample shipado: `assets/vehicles/banshee_sa.moodvehicle` con los defaults SA-style (que es lo que devuelve `makeDefaultSA()`).
- Inspector del `VehicleComponent`: combo "Config" con drop target de `MOOD_VEHICLE_ASSET` (nueva convención de payload, análoga a `MOOD_MESH_ASSET`).

**LOC est.**: ~400 (loader + serializer + Inspector + 1 sample JSON).

### Bloque H — Debug overlay 3D bajo F1

En `EditorRenderPass_Overlay.cpp` junto a F2H65/F2H66:

- Forall vehicles activos: dibujar OBB del chassis (azul claro) + 4 wheels como círculos (verde) + vector de velocidad (amarillo) + vector de suspensión por wheel (gris).
- Colores: chassis `(0.40, 0.70, 1.00)`, wheels `(0.20, 0.90, 0.30)`, velocidad `(1.0, 1.0, 0.2)`, suspensión `(0.7, 0.7, 0.7)`.

**LOC est.**: ~150.

### Bloque I — Sample mapa `vehicle_demo.moodmap`

- Terreno plano + rampa de 30° + paredes laterales + 1 auto spawneado (chassis box + 4 wheels children) con `VehicleComponent` apuntando a `banshee_sa.moodvehicle`.
- Trigger box cerca del auto con texto "Press F to enter" (rendering del texto vía Lua + HUD existente de F2H39+).
- Script Lua `vehicle_demo.lua`: lee `Input.is_key_down("W")` etc. y llama `vehicle.set_input(...)` cada frame mientras player montado.

**LOC est.**: ~50 JSON + ~80 Lua.

### Bloque J — Tests headless

- `test_vehicle_config.cpp` (4-5 tests): defaults SA OK, mass > 0, wheels[4] todos seteados, gears monotónicamente decrecientes.
- `test_physics_vehicle.cpp` (~7-8 tests, runtime Jolt):
  - `createVehicle` con config válida → handle no-zero.
  - `setInput throttle=1.0` + 60 ticks → `forwardSpeed > 5 m/s`.
  - `setInput brake=1.0` (after speed) → `forwardSpeed` decrece.
  - `setInput steer=1.0` + ticks → chassis rotation Y cambia.
  - `readVehicleState` 4 wheels grounded en plano.
  - `destroyVehicle` idempotente.
  - `applyVehicleImpulse` → velocidad cambia inmediato.
  - Mass conservación: chassis mass del state == cfg.chassisMass.
- `test_scene_serializer_vehicle.cpp` (~3 tests): round-trip VehicleComponent + path persistido + VehicleSeatComponent opcional.

**LOC est.**: ~500 totales.

### Bloque K — Validación + cierre

1. Probar `vehicle_demo.moodmap`: caminar al auto → F monta → WASD conduce → rampa salta → handbrake derrapa controlado → F dismonta → cam vuelve al player.
2. Verificar que el feel sea SA: arrancada fuerte, brakes potentes, dirección responsiva, no flips fáciles, derrapes solo con handbrake.
3. Probar respawn via Lua (`vehicle.respawn`) si el auto queda volcado.
4. Verificar suite tests verde.
5. Commits agrupados por bloque + tag `v1.54.0-fase2-hito67`.
6. Update HITOS/ESTADO_ACTUAL/DECISIONS/hitos/F2H67.md + archivar plan.

---

## LOC total estimado

~3300 cpp + ~500 h + ~600 tests = **~4400 LOC** efectivos (más overhead de boilerplate ~1000). **Hito grande, ~10-14 horas distribuidas.**

## Limitaciones agendadas para futuros hitos (sub-fase 2.4)

- **Motorcycles** (`JPH::MotorcycleController`). Out of scope F2H67 (4 wheels only).
- **Tracked vehicles** (tanques). Out of scope.
- **Multiple seats** (pasajero). Diferido — un asiento por vehículo en v1.
- **Damage model**: bumps reducen HP del vehículo + efectos visuales. Diferido.
- **NPC drivers** (AI conduciendo): integración con NavAgent. Diferido.
- **Audio del motor** (RPM-mapped engine sound). Diferido — F2H66 dejó audio en buen estado pero engine sound requiere asset pipeline.
- **Tuning UI completo del `.moodvehicle`** (panel dedicado tipo Material Editor). En F2H67 expone solo los campos básicos en el Inspector; tunning fino queda como follow-up si emerge demanda.

## Riesgos identificados

1. **Feel SA tunning iterativo** — los defaults numéricos van a requerir tweak en runtime. Mitigación: exponer los parámetros más sensibles (`tireFriction`, `engineMaxTorque`, `centerOfMassLocal`) en el Inspector del `VehicleComponent` como override per-instance, además del `.moodvehicle` global.
2. **Sync chassis ↔ child wheel entities** — si las wheel entities no existen como children al `createVehicle`, las podemos auto-spawnnear en runtime, pero el dev espera verlas en el editor. Mitigación: auto-spawn al `dirty=true` con tags `Wheel_FL/FR/RL/RR` + log info.
3. **Camera chase pop al montar** — saltar de FPS player cam a third-person chase puede ser jarring. Mitigación: lerp de 0.3s en posición + rotación durante el switch.
4. **Volcado / flip** — si el auto se vuelca, queda inutilizable. Mitigación: `vehicle.respawn(tag, pos, rotY)` desde Lua. Si emerge frustración, futuro hito agrega "press R = auto-right" estilo SA.
5. **Performance con N vehicles** — F2H67 v1 testea con 1-2 vehicles. Si el dev quiere tráfico (10+), agendar perf pass.
