# PLAN_HITO_F2H69 — Debug del trigger NPC + pipeline glTF multi-node + DeLorean

> **Estado:** En curso (2026-05-19) — **handoff a otra maquina del dev mid-hito**.
> **Predecesor:** F2H68 (auto-ragdoll por impacto — infra completa, sample con bug conocido).
> **Motivación:** Tres entregables acordados con el dev al cerrar F2H68:
> 1. Resolver el bug conocido (NPC sensor no transiciona al ser embestido).
> 2. Pipeline glTF multi-node estilo Unity/Unreal: 1 GLB → N entities (chassis + wheels + doors) sin sub-mesh selector.
> 3. Reemplazar el sedan Kenney por el DeLorean GLB ya commiteado en `assets/dmc_delorean/`.

---

## HANDOFF (2026-05-19 20:45) — leer ANTES de continuar

El dev cambio de maquina mid-hito. Estado real al momento del handoff:

**✅ Bloque A — Auto-ragdoll bug F2H68 RESUELTO**. Validado runtime con log:
```
[F2H69-DEBUG] ENCOLA: b1=16777218 (Sensor=false) b2=33554432 (Sensor=true) closingSpeed=2.25
[F2H69-DEBUG] RagdollSystem drain: 2 events
[F2H69-DEBUG]   TRANSITION entHandle=2 -> Ragdolling (closingSpeed=2.25 m/s, impulse=(0.00,-0.60,-0.31))
```
Fix: `mAllowSleeping=false` para sensor bodies + `impactSpeedThreshold` bajado de 4.0 a 1.0 m/s
(arcade feel SA, sin falsos positivos porque wheels Jolt son raycasts no bodies).

**⏳ Bloques B+C — Polish visual del DeLorean parcial**. El MeshLoader ahora aplica node transforms
para `.gltf/.glb` (sin esto el DeLorean salia gigante o overlapped en origen). PERO el DeLorean
sigue en unidades del 3DS Max original (mm), AABB Y=0..7331 unidades. **Workaround temporal:**
`Transform.scale = 0.00015` en `vehicle_demo.moodmap` para que se vea como auto de 4m.

**El dev esta re-escalando el DeLorean en Blender** a medidas correctas:
- Largo: 4.0 m (eje Y en Blender → Z en glTF). El "frente" del DeLorean debe apuntar a Y+ Blender.
- Ancho: 1.8 m (X en Blender).
- Alto: 1.0 m (Z en Blender → Y en glTF).

Al re-importar el GLB re-escalado, **cambiar `scale: [0.00015, ...]` por `scale: [1.0, 1.0, 1.0]`**
en `assets/maps/vehicle_demo.moodmap` y `c:/tmp/MoodDemo_F2H69/maps/vehicle_demo.moodmap`.

**Pendientes despues del re-escale:**
- Confirmar visual del DeLorean OK (no tumbado, no diagonal).
- Quitar logs `[F2H69-DEBUG]` de `PhysicsWorld_Impact.cpp` + `RagdollSystem.cpp` + `MeshLoader.cpp`.
- Decidir si el `impactSpeedThreshold = 1.0 m/s` queda como default o se justifica.
- Tunear el feel "pesado" reportado por el dev (subir torque o bajar mass en `VehicleConfig::makeDefaultSA`).
- Tests con assets reales (DeLorean GLB) si emerge bug del fix MeshLoader.
- Cierre: docs/hitos/F2H69.md + HITOS.md + ESTADO_ACTUAL.md + DECISIONS.md + tag `v1.56.0-fase2-hito69`.

**Cosas que NO entran en F2H69** (queda pipeline glTF multi-node como follow-up): el split-by-node real
que separe wheels en MeshAssets independientes. Lo que hicimos es consolidacion (todos los nodes en
1 MeshAsset con vertices preTransformed), suficiente para visual unitario tipo "1 auto solid", no
para wheels que roten independientes. Si emerge demanda real (animacion wheels), abrir hito propio.

---

---

## Objetivo

Cerrar la cadena trigger → drain → state transition para que el sample end-to-end vehicle-vs-NPC funcione, y entregar el pipeline de assets de vehículos al estándar industry (cada parte del modelo es un asset / entity independiente).

## Estado pre-hito

- `PhysicsWorld_Impact.cpp`: ContactListener registrado, cola deferred, `bodyToEntity` map. Tests unit verde (6 cases).
- `vehicle_demo.moodmap`: NPC `kinematic + is_sensor: true + RagdollComponent`. Banshee con `vehicle` component.
- 1029/10227 tests verde post-F2H68.
- Bug reproducido por el dev: chassis atraviesa al NPC sensor (sensor body funciona) pero el ragdoll NUNCA dispara.

---

## Diseño

### Hipótesis principal (Bloque A)

**El sensor Kinematic se duerme tras ~5s sin movimiento → Jolt deja de invocar `OnContactAdded` para ese body.**

Regla de Jolt: sensors solo detectan contactos cuando ellos están awake. Un Kinematic con velocidad zero entra a sleep tras `cBodyMotionTimeToSleep` (default 5s) y deja de generar callbacks. El chassis Dynamic activo SÍ está awake, pero la regla de Jolt se aplica al sensor, no al "attacker".

**Fix mínimo:** `BodyCreationSettings::mAllowSleeping = false` cuando `isSensor=true`. Patrón estándar de Jolt para triggers/sensors permanentes.

Hipótesis subordinadas que los logs van a discriminar si el fix sleep no resuelve:

- `closingSpeed` < 4 m/s en el momento del impacto (proyección sobre normal incorrecta o velocidad insuficiente).
- `bodyToEntity[sensorBodyId]` no registrado al momento del callback (timing del registerBodyEntity tras createBody).
- Callback dispara pero el drain del RagdollSystem corre fuera de orden.

### Pipeline glTF multi-node (Bloque B)

`MeshLoader.cpp` extendido para detectar aiNodes con mesh children y opcionalmente generar `MeshAsset`s separados por node (cada uno centrado en su origen local). API nueva: opt-in via flag `MeshLoadOptions::splitByNode = true`. Cada sub-asset queda registrado en `AssetManager` con un path lógico derivado del nombre del node (`<gltf_path>#<node_name>`).

Resultado: el dev importa un GLB con `body / wheel_FL / wheel_FR / ...` como nodes separados → obtiene N `MeshAsset`s independientes → cada uno se asigna a su entity correspondiente (chassis + 4 wheels) sin sub-mesh selector ni tracking de pivots.

Pattern Unity (importer split-by-mesh), Unreal (FBX/glTF importer split options), Godot (glTF Document Importer flag). Sin re-inventar.

### DeLorean GLB (Bloque C)

`assets/dmc_delorean/scene.gltf` ya commiteado (CC-BY, ya bajado en F2H68). Tareas:

1. Verificar el GLB: estructura de nodes (body + wheels esperados), scale, axis convention.
2. Eliminar `assets/vehicles/banshee_sa/sedan.fbx` (Kenney) — incompatible con sub-mesh selector y reemplazado.
3. Actualizar `vehicle_demo.moodmap`: chassis entity → mesh = DeLorean body node; spawnear 4 wheel entities con tag `Wheel_FL/FR/RL/RR` apuntando a los nodes de wheel del GLB.
4. El `.moodvehicle` (`banshee_sa.moodvehicle`) y `VehicleConfig::makeDefaultSA()` se quedan — son la "configuración" lógica del vehicle, no el asset visual. Renombrable a `.moodvehicle` genérico si el dev quiere.

---

## Bloques

### Bloque A — Debug del trigger NPC (PRIMER COMMIT)

- Fix `mAllowSleeping = false` para sensor bodies en `PhysicsWorld::createBody`.
- Logs `[F2H69-DEBUG]` temporales en:
  - `ContactListener::OnContactAdded`: bodyIds, IsDynamic, IsSensor, closingSpeed, threshold, decision (skip/enqueue).
  - `RagdollSystem::tick` drain: número de events, entHandle resolution, component check, transition.
- Validación: el dev corre el demo, atropella al NPC, comparte logs si falla.

### Bloque B — Confirmación + cleanup (post-validación)

- Si el fix sleep resuelve: remover logs `[F2H69-DEBUG]`, dejar el comportamiento confirmado.
- Si no: leer logs y aplicar fix dirigido a la hipótesis confirmada (closingSpeed, timing, orden).

### Bloque C — Pipeline glTF multi-node

- `MeshLoader.cpp`: detect aiNodes con mesh children + opt-in `splitByNode` flag.
- `AssetManager::loadMesh` extendido con la opción.
- Tests: roundtrip headless con un GLB sample (DeLorean serviría).
- Sin sub-mesh selector — el filter `MeshRendererComponent::subMeshName` queda para casos legacy pero no es el path principal.

### Bloque D — DeLorean swap en el demo

- Verificar `assets/dmc_delorean/scene.gltf` con el pipeline B.
- Eliminar `assets/vehicles/banshee_sa/sedan.fbx`.
- Actualizar `vehicle_demo.moodmap`: 1 chassis entity + 4 wheels con sus meshes split.
- Tag del vehicle puede ser `DeLorean` (en vez de `Banshee`); el `.moodvehicle` se queda con tuning SA-style genérico.

### Bloque E — Cierre

- Tests verdes (suite completa).
- `docs/hitos/F2H69.md` con detalle.
- `docs/HITOS.md` entry one-liner.
- `docs/ESTADO_ACTUAL.md`: 0.1 → F2H69, 0.2 → F2H68.
- `docs/DECISIONS.md`: decisiones de B y C (split-by-node opt-in, DeLorean como asset visual default).
- Move `docs/PLAN_HITO_F2H69.md` → `docs/archive/plans/`.
- Backlog: confirmar que el follow-up del F2H68 cerrado.
- Commits agrupados + tag `v1.56.0-fase2-hito69` + push.

---

## Riesgos / open questions

- **Si `mAllowSleeping=false` no resuelve**: hipótesis subordinadas (closingSpeed bajo, timing del registro, orden) son las siguientes a chequear. Los logs del Bloque A las discriminan en 1 sola corrida.
- **Pipeline glTF multi-node + AssetManager**: cada sub-asset necesita un path lógico estable. Convención `<gltf_path>#<node_name>` es la más natural; verificar que no rompe el serializer del `.moodmap` (probablemente OK porque es string opaco).
- **DeLorean axis convention**: si el GLB tiene Z-up vs Y-up (Blender vs Unity convention), el orientation va a quedar volcada. Verificar con un import en blanco antes de actualizar el mapa.
