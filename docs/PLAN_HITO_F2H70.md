# PLAN_HITO_F2H70 — Sistema data-driven de vehículos + engine fixes

> **Estado:** En curso (2026-05-19) — handoff a notebook del dev. Bloques A+B parcialmente implementados; el resto pendiente.
> **Predecesor:** F2H69 (DeLorean swap parcial — 3 workarounds asset-specific identificados).
> **Motivación:** Cuando intentamos atacar F2H70 originalmente con specs hardcoded del DeLorean en `VehicleConfig.cpp::makeDeLoreanDMC12()`, el dev objetó (verbatim): *"qué pasa si mañana yo agrego 10 autos más? entiendo que tendremos que tener algo más complejo como un sistema para trabajar otros autos... tener los valores hardcodeados, no lo veo realmente viable."* **Pivot del plan**: F2H70 pasa de "fix 3 bugs" a "sistema data-driven escalable". Engineering profesional, no parches.

---

## HANDOFF — leer antes de retomar

**Lo que YA está implementado en `main` (commits hoy, pushed):**

- ✅ **Bloque A — Auto-spawn-height vía `TransformComponent.pivotYOffset`**:
  - `Components.h`: campo nuevo `f32 pivotYOffset = 0.0f` (runtime-only, no persistido); `worldMatrix()` aplica `position.y + pivotYOffset` antes de translate.
  - `VehicleSystem::tick`: materialize lazy setea `tf.pivotYOffset = chassisRenderYOffset(e, assets)` (que devuelve `-aabbMin.y` del MeshAsset). Spawn matrix usa `tf.worldMatrix()` (ya incluye offset).
  - `VehicleSystem::tick` post-tick: `writeWorldMatrixToTransform(world, tf, tf.pivotYOffset)` resta el offset antes de escribir al TC → `tf.position` queda en "raw" (lo que el dev escribió en el moodmap).
  - `SceneLoader.cpp`: al cargar el moodmap, si una entity tiene `VehicleComponent + MeshRendererComponent`, se setea `tf.pivotYOffset` ahí mismo → funciona en Editor mode sin Play.
  - **Helper público** `Mood::VehicleSystem::chassisRenderYOffset(Entity, AssetManager&)` exportado en `VehicleSystem.h`.

- ✅ **Bloque B — Quat sync sin gimbal**:
  - `Components.h`: campos nuevos `glm::quat rotation` + `bool useQuaternion = false`. `worldMatrix()` con `useQuaternion=true` aplica `mat4_cast(rotation)` en lugar de euler XYZ.
  - `VehicleSystem`: `writeWorldMatrixToTransform` extrae quat con `glm::quat_cast(mat3(world))` y setea `tf.rotation + tf.useQuaternion=true`. Sin pasar por euler intermedio.
  - `InspectorPanel_Transform.cpp`: si `useQuaternion=true`, refresca `rotationEuler` desde el quat para display consistente. Al editar manualmente, resetea `useQuaternion=false` (intención del dev gana).

- ✅ **Cleanup vehicle_demo.moodmap**: `position: [0,0,0]` y `rotationEuler: [0,0,0]`. Los valores asset-specific (0.568, 180°) desaparecieron — el engine los absorbe vía pivotYOffset + quat sync.

- ✅ **Suite 1029/10227 verde**, sin regresiones.

**Lo que está PENDIENTE (replan data-driven):**

Ver los bloques C–H abajo. El cambio de dirección del plan emergió durante la sesión cuando se intentó hardcodear specs reales del DeLorean DMC-12 en `makeDeLoreanDMC12()` — el dev paró diciendo que eso no escala a 10+ autos. La pivotada es: **specs de cada vehículo viven en archivos de datos `.moodvehicle` (JSON), no en código C++**.

**Bugs visuales que quedaron abiertos al cerrar la sesión:**

1. **Sincronización modelo ↔ chassis Jolt al dar Play**: el dev reportó que el modelo y el wireframe del chassis se desincronizan en Play mode (modelo se ve desfasado del hitbox). Hipótesis principal: el modelo del DeLorean post-procesado mira hacia `-Z` mientras Jolt `VehicleConstraint` está configurado en `mForward = +Z` ([PhysicsWorld_Vehicle.cpp:126-127](../src/engine/physics/world/PhysicsWorld_Vehicle.cpp#L126-L127)). Esto explica el offset 180° que originalmente el dev compensaba con `rotationEuler: [0, 180, 0]` en el moodmap. **Esto va en Bloque D (convención de modelos)** — documentar convención forward = +Z + script de preprocesado que normaliza el modelo.

2. **Sigue atravesando al NPC en Play**: chasis físico no detecta al NPC sensor o no transiciona ragdoll. Diagnóstico abierto. Posible causa relacionada al bug 1 (chasis físico está en una orientación distinta a la visual). **Validar después del fix del forward axis.**

3. **Auto se mueve con toque mínimo y fricción muy baja**: `makeDefaultSA()` actual tiene tuning arbitrario (no derivado de specs físicas). Cualquier toque de W/S mueve el auto y la fricción baja lo deja deslizando. **Lo arregla el sistema data-driven (Bloque C+E)** porque las specs reales de un DeLorean (mass 1230 kg, peak torque 208 Nm) producen un feel realista, no arcade-floppy.

---

## Referencia industrial — Source Engine / Valve

**Filosofía a copiar (no inventar)**: el sistema de vehículos del Source Engine (Half-Life 2: Episode One/Two, Garry's Mod, etc.) es el estándar de la industria para vehículos data-driven en juegos no-sim. Su pipeline:

- **1 archivo por vehículo** en `scripts/vehicles/<vehicle>.txt` (formato KeyValues estilo VDF — análogo a JSON pero más compacto). El dev lo edita en cualquier editor de texto.
- **Spec declarativa por AXLE, no por wheel individual**: la mayoría de los autos son simétricos L/R. Source pide configurar 1 axle delantero + 1 axle trasero (cada uno con su `wheel { radius, mass, friction }`, `suspension { springConstant, dampingCompression }`, `torque_factor`, `brake_factor`). Mucho más conciso que 4 entradas separadas y semánticamente correcto (los 2 wheels de un axle SIEMPRE comparten config).
- **Bloques semánticos**: `body { massCenterOverride, countersteer, ... }`, `engine { horsepower, maxRPM, maxSpeed, axleratio, gear[N] ratio }`, `steering { degreesSlow, degreesFast, throttleSteeringRestRateSlow, ... }`.
- **Hot reload**: edits al `.txt` se aplican sin recompilar; el dev itera tuning en runtime.
- **Convenciones documentadas**: el Valve Developer Wiki tiene una página oficial ("Vehicle scripting") que describe cada key y su rango sano. Source ships con `scripts/vehicles/jeep_test.txt`, `airboat.txt`, `jalopy.txt` (Episode Two) como referencia + plantilla.

**Aplicación al `.moodvehicle` de MoodEngine**: el schema v2 debe replicar esa estructura:

```jsonc
{
  "schemaVersion": 2,
  "metadata": { "name": "DeLorean DMC-12", "make": "DMC", ... },
  "model": { "mesh_path": "delorean.glb", "forward_axis": "+Z", ... },
  "body": {
    "dimensions_mm": [1857, 1140, 4216],
    "mass_kg": 1230,
    "mass_center_override": [0, -70, -400]   // mm, local; negativo Z = atrás (rear-engine)
  },
  "axle_front": {
    "offset_z_mm": 1205,            // wheelbase/2 hacia adelante
    "track_mm": 1588,                // distancia rueda izq-der del axle
    "wheel": {
      "radius_mm": 330,
      "width_mm": 195,
      "friction_long": 1.0,
      "friction_lat": 0.9
    },
    "suspension": {
      "frequency_hz": 1.8,
      "damping": 0.6,
      "max_length_mm": 300
    },
    "torque_factor": 0.0,            // RWD → eje delantero NO recibe torque
    "brake_factor": 0.6,             // 60% del freno principal va al eje delantero
    "steered": true,
    "handbraked": false
  },
  "axle_rear": {
    "offset_z_mm": -1205,
    "track_mm": 1588,
    "wheel": { "radius_mm": 330, "width_mm": 235, "friction_long": 1.0, "friction_lat": 0.95 },
    "suspension": { "frequency_hz": 1.8, "damping": 0.6, "max_length_mm": 300 },
    "torque_factor": 1.0,            // RWD → eje trasero recibe el 100% del torque
    "brake_factor": 0.4,             // 40% del freno principal
    "steered": false,
    "handbraked": true
  },
  "engine": {
    "horsepower": 130,
    "peak_torque_nm": 208,
    "peak_torque_rpm": 2750,
    "redline_rpm": 5500,
    "idle_rpm": 800,
    "transmission": "5-speed-manual",
    "gear_ratios": [3.36, 2.06, 1.38, 1.00, 0.82],
    "reverse_ratios": [3.10],
    "final_drive_ratio": 3.444
  },
  "brakes": {
    "deceleration_target_mps2": 7.0,
    "handbrake_ratio": 0.5
  },
  "steering": {
    "max_angle_deg_slow": 30,
    "max_angle_deg_fast": 12,        // Source pattern: menos angle a alta velocidad
    "throttle_steering_rest_rate_slow": 4.0,
    "throttle_steering_rest_rate_fast": 1.0
  }
}
```

**Razones del refactor a axle-based**:
- **Conciso**: 2 axles vs 4 wheels reduce duplicación. Modificar el `friction_long` de las dos ruedas traseras a la vez (cambio típico para tuning RWD oversteer) es 1 edit, no 2 ediciones idénticas.
- **Semánticamente correcto**: `torque_factor` por axle modela exactamente lo que pasa en un diff físico (el diferencial reparte torque al axle, no a wheels individuales).
- **Documentación reusable**: dev que viene del modding de Source (mucha gente — Gmod, HL2 mods) reconoce el patrón instantáneamente.
- **Internamente sigue mapeado a 4 wheels en Jolt** (Jolt no soporta axles nativos): el loader expande `axle_front` → wheels FL+FR con la misma config + `offset_z` espejado.

**Lo que NO copiamos de Source**:
- Formato KeyValues VDF — nuestro `.moodvehicle` queda en JSON (consistente con el resto del proyecto y mejor toolchain `nlohmann/json`).
- Hot reload runtime — F2H70 lo cubre solo si emerge demanda (no es scope inicial).
- Phys system mismo de Havok que usa Source — nosotros seguimos con Jolt, que es superior y soporta el mapping del axle.

**Pipeline asset-centric estilo Source** (con nuestra estructura):

```
assets/vehicles/<vehicle_name>/
├── <vehicle_name>.glb             # modelo
├── <vehicle_name>.moodvehicle      # spec axle-based
├── thumbnail.png                    # 256x256
└── LICENSE.txt
```

Análogo a Source: `models/<vehicle>/<vehicle>.mdl` + `scripts/vehicles/<vehicle>.txt`.

---

## Diseño revisado — Sistema data-driven

### Estado actual de la infra (lo que ya existe)

- ✅ `.moodvehicle` JSON format: ya hay parser en `AssetManager_Vehicle.cpp`. Schema aditivo: campos faltantes caen al default `makeDefaultSA()`.
- ✅ `VehicleConfig` puro (sin Jolt) materializable desde JSON.
- ✅ `Mood::vehicle::isValid()` valida rangos sanos.
- ✅ `VehicleSystem::resolveConfig` consulta `configPath` del componente, fallback al default si vacío/inválido.

### Qué falta

Lo que NO existe:

1. **Catálogo de assets `.moodvehicle` reales** — el demo usa `configPath: ""` que cae a `makeDefaultSA()` (números inventados).
2. **Schema ampliado** con metadata (name/make/model/year/source/license) + units amigables al dev (HP, km/h, kg, mm) con conversión interna.
3. **Estructura de carpetas asset-centric**: 1 carpeta por vehículo con `.glb + .moodvehicle + thumbnail` autocontenido (pattern Unity Prefab / Unreal Blueprint).
4. **Presets/templates** (`sport_coupe`, `sedan`, `truck`, `motorcycle`) para que el dev clone en lugar de partir de cero.
5. **Calculadora de tuning derivado** (mass + peak_hp → brakeTorque + friction + gear_ratios estimados).
6. **Validación física plausible** (mass 500–5000 kg, wheelbase < chassis.Z, etc.).
7. **Vehicle Browser en el editor** (panel para listar/crear/editar `.moodvehicle` con UI amigable).
8. **Convención de modelos documentada** (forward = +Z, units = m, origin libre absorbido por engine, naming canónico de wheel sub-meshes).
9. **Scripts de preprocesado oficiales** en `tools/` (no `c:/tmp/`) versionados (scale, flatten, center, reorient).

---

## Bloques

### Bloque A — Engine: auto-spawn-height ✅ COMPLETO

Ver § Handoff. Listo en `main`.

### Bloque B — Engine: quat sync sin gimbal ✅ COMPLETO

Ver § Handoff. Listo en `main`. **Limitación opción A**: la conversión quat→euler en el Inspector (display) sigue siendo ambigua. Suficiente para v1; si emerge edge case, considerar opción C (matrix-first transform) en F2H71+.

### Bloque C — Schema `.moodvehicle` v2 axle-based estilo Source (PENDIENTE)

Ver el schema completo en § Referencia industrial — Source Engine / Valve arriba. Resumen de la implementación:

- Schema v2 con `body / axle_front / axle_rear / engine / brakes / steering` (analogo al `.txt` de Source).
- Units amigables al dev: mm, kg, HP, Nm @ RPM. Conversion interna al SI (m, kg, Nm) en el loader.
- `axle_front/rear` → expandido a wheels FL+FR / RL+RR con `attachLocal` derivado de `offset_z_mm + ±track_mm/2` y `torque_factor / brake_factor` heredados.
- `transmission: "5-speed-manual"` como preset que carga `gear_ratios` default (override con array explicito si el dev quiere).
- `schemaVersion` bump a 2; v1 sigue funcionando (forward-compat).

**Implementación**:
- `AssetManager_Vehicle.cpp` parser actualizado con conversiones y expansion axle→wheels.
- `validateVehicleSpecs()` con rangos: mass 500–5000 kg, wheelbase < chassis.Z, peak_torque > 0, etc. Warnings (no errores) — el config puede ser raro adrede.
- `gear_ratios` desde preset cuando no se override: 5-speed-manual usa typical `[3.36, 2.06, 1.38, 1.00, 0.82]` (DeLorean stock), 6-speed-auto otra preset, etc.

### Bloque D — Convención de modelos + scripts de preprocesado oficiales (PENDIENTE)

1. **Doc `docs/asset_conventions.md`** (o sección en CONTEXTO_TECNICO):
   - Modelos: 1u = 1m, `+Y` up, `+Z` forward (convención glTF estándar).
   - Origin libre — el engine absorbe vía `pivotYOffset` (Bloque A ✅).
   - Sin mirrors baked (`det(M) > 0` en todos los nodes).
   - Naming canónico de sub-meshes: `chassis`, `wheel_FL`, `wheel_FR`, `wheel_RL`, `wheel_RR`, `interior`, `door_*`.

2. **Mover los scripts existentes** de `c:/tmp/` a `tools/glb/` versionados en git:
   - `tools/glb/scale.py` — bake escala uniforme.
   - `tools/glb/flatten.py` — bake node matrices + fix det<0 (mirrors).
   - `tools/glb/center_y.py` — origin al centro vertical.
   - `tools/glb/reorient.py` — rotar para que mire a +Z (heurística por wheel positions).
   - `tools/glb/verify.py` — AABB + dimensiones + warn si no cumple convención.
   - `tools/glb/diag.py` — detecta mirror nodes.
3. **Pipeline recomendado** documentado: dev baja un GLB → corre `verify.py` → si falla, aplica los scripts en orden → re-verifica → drop en `assets/vehicles/<name>/`.

### Bloque E — Catálogo de assets reales (PENDIENTE)

Estructura:

```
assets/vehicles/
├── _presets/
│   ├── sport_coupe.moodvehicle           # DeLorean-like, RWD, ~1200 kg
│   ├── sedan.moodvehicle                  # familiar 4-door, FWD, ~1400 kg
│   ├── sport_car.moodvehicle              # supercar AWD, ~1600 kg
│   ├── truck.moodvehicle                  # pickup, AWD, ~2500 kg
│   └── motorcycle.moodvehicle             # 2 wheels (out of scope F2H70, F2H7X)
└── delorean_dmc12/
    ├── delorean_dmc12.glb                 # modelo cumpliendo convención
    ├── delorean_dmc12.moodvehicle         # specs reales DMC-12
    ├── thumbnail.png                       # preview 256x256
    └── LICENSE.txt                         # CC-BY u otra
```

**Implementación**:
- Crear `_presets/` con 4 vehículos plausibles (specs realistas de su categoría).
- DeLorean queda como `assets/vehicles/delorean_dmc12/` (refactor del current `assets/vehicles/delorean/`).
- `vehicle_demo.moodmap` apunta a `delorean_dmc12.moodvehicle`.

### Bloque F — Vehicle Browser + Inspector en el editor (PENDIENTE)

- Pestaña nueva en `AssetBrowserPanel` para `.moodvehicle` (thumbnail + name + categoría).
- Doble-click → `VehicleInspectorPanel` con campos por sección (chassis / wheels / engine / brakes / steering). Inputs en unidades amigables (mm, kg, HP).
- Botón "Spawn in scene" → crea entity con `VehicleComponent` apuntando al `.moodvehicle`.
- Botón "Clone preset" → crea nuevo `.moodvehicle` desde `_presets/` con nombre user-input.

### Bloque G — Refactor: `makeDefaultSA()` → fallback genérico, no DeLorean-specific (PENDIENTE)

- Renombrar `makeDefaultSA()` → `makeFallbackGenericSedan()` o similar.
- Valores conservadores (~1500 kg, sedan FWD, friction estándar) — solo se usa cuando un `.moodvehicle` no carga.
- Log warn cuando el fallback se usa (alerta al dev que algo está mal en el `.moodvehicle`).
- Specs reales del DeLorean **NO viven en código** — están en `delorean_dmc12.moodvehicle`.

### Bloque H — Split-by-node de wheels (heredado del F2H70 original) (PENDIENTE)

Ver plan F2H70 original pre-pivotada (sección Bug 3 del plan original). Sigue siendo necesario para que las ruedas roten visualmente bajo throttle. Convención de naming queda definida en Bloque D.

### Bloque I — Validación + cleanup + cierre (PENDIENTE)

- Tests verdes (incl. nuevos del schema v2 + validator).
- Validar visualmente: DeLorean drop-in (`.glb + .moodvehicle`), spawn OK, mueve realista, atropella NPC.
- `docs/hitos/F2H70.md`, `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- Tag `v1.57.0-fase2-hito70` + push.

---

## Priorización sugerida (sub-fases F2H70.1 / F2H70.2 / F2H70.3)

F2H70 crecio a 9 bloques. Si emerge presión de tiempo, splittear:

- **F2H70.1 — Sistema base data-driven** (cierra el principio operacional):
  - Bloque C (schema ampliado v2)
  - Bloque D (convención + scripts oficiales)
  - Bloque E (catálogo: 1 preset + DeLorean refactorizado)
  - Bloque G (fallback genérico)
  - Bloque I parcial (validar end-to-end con DeLorean, sin presets adicionales)

- **F2H70.2 — Editor UI**:
  - Bloque F (Vehicle Browser + Inspector)
  - Bloque E completo (4 presets sport/sedan/sport_car/truck)

- **F2H70.3 — Wheels visuales**:
  - Bloque H (split-by-node)
  - Validación end-to-end de un vehicle agregado por el dev de cero (5 min target).

---

## Riesgos / open questions

- **Unit convention**: ¿mm/kg/HP en el JSON o m/kg/W internamente serializados? **Recomendación**: input dev-friendly (mm/kg/HP) con conversión al parseo. Internamente `VehicleConfig` queda en SI (m, kg, Nm).
- **Schema migration v1→v2**: archivos `.moodvehicle` existentes (banshee_sa.moodvehicle ya borrado en F2H69) son cero. Migration trivial — start clean en v2.
- **Cómo validar el forward axis del modelo**: heurística (wheel centers en Z, gigualdad sospechosa con el `forward_axis` declarado del `.moodvehicle`) + warning si no matchean. No bloquear, solo advertir.
- **Performance del schema parsing**: 1 parse por load del moodmap, cacheado en AssetManager. No es hot path.

---

## Cosas que NO entran en F2H70 (incluso completo)

- Motorcycle physics (2 wheels) — necesita refactor del `WheelCount` constante.
- Vehículos con `WheelCount != 4` (camiones 6+ wheels).
- Damage model (deformación del chassis, vidrios rotos).
- Tire wear, fuel consumption, engine heat.
- Multi-seat passengers (solo 1 driver + 1 passenger).
- Tuning UI con sliders interactivos en runtime (debug only).
- Asset import directo desde formato `.car` de juegos comerciales.
