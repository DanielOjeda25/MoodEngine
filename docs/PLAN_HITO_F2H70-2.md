# PLAN_HITO_F2H70.2 — Tuning físico del vehicle (damping + spawn elevation + controles)

> **Estado:** En curso (2026-05-20)
> **Predecesor:** F2H70.1 (sistema data-driven `.moodvehicle` v2 axle-based + DeLorean DMC-12 catalog).
> **Motivación:** F2H70.1 cerró el schema parsing + integración del DeLorean, pero dejó 3 bugs físicos pendientes. Este sub-hito los resuelve para que el auto se sienta como un vehículo real (apoya en piso, frena naturalmente, controles consistentes con el .glb).

---

## Objetivo

Pulir el feel físico del vehicle al nivel arcade Source/Valve: ruedas apoyando, momentum natural, controles consistentes con el modelo importado siguiendo la convención industrial (+Z forward).

## Estado pre-hito

3 bugs documentados en [`hitos/F2H70.md` § Issues pendientes a F2H70.2`](hitos/F2H70.md#issues-pendientes-a-f2h702):

| ID | Síntoma | Causa raíz | Workaround actual |
|---|---|---|---|
| **A** | WASD invertidos cuando el .glb está reorientado a +Z forward | Cámara FPS del seat mount asume -Z forward (convención vieja) mientras Jolt usa +Z | `.glb` revertido + `rotationEuler: [0, 180, 0]` en moodmap |
| **B** | Chassis flota 7-10cm sobre el piso al spawn | `pivotYOffset` = `-aabbMin.y` no considera spring settle | Ninguno (visible al spawn) |
| **C** | Auto rueda infinito sin acelerador | Schema v2 no expone `linear_damping` / `angular_damping`; defaults a 0 en `JPH::BodyCreationSettings` | Ninguno |

---

## Bloques

### Bloque C — Chassis damping en schema v2 (PRIMERO)

**Mecánica del jugador:** soltás W y el auto desacelera gradualmente hasta parar, como en GTA SA. Sin tener que apretar freno todo el tiempo.

**Cambios:**

1. **`VehicleConfig.h`**: agregar `f32 linearDamping = 0.5f` y `f32 angularDamping = 0.5f` (defaults SA-ish; arcade no-sim).
2. **`VehicleConfig.cpp::makeFallbackGenericSedan`**: setear defaults.
3. **`AssetManager_Vehicle.cpp::parseVehicleConfigJsonV2`**: parse `chassis.linear_damping` y `chassis.angular_damping` opcionales (con fallback a defaults del config).
4. **`PhysicsWorld_Vehicle.cpp::createVehicle`**: aplicar a `chassisSettings.mLinearDamping` + `mAngularDamping` (ya están seteados en `0.05f` — reemplazar con el del config).
5. **`delorean_dmc12.moodvehicle`**: agregar bloque `"chassis": { "linear_damping": 0.5, "angular_damping": 0.5 }`. Tunear si el dev valida que se siente raro.
6. **Doc**: actualizar `asset_conventions.md` (sección schema v2) con los nuevos campos.

Validación: spawn DeLorean → soltar W → el auto desacelera y para. Tunear damping si arcade pierde "deslice".

### Bloque B — Spawn elevation spring-aware

**Mecánica del jugador:** apenas spawneás el auto, las ruedas apoyan en el piso. No hay un brinco inicial ni el auto queda flotando.

**Cambios:**

1. **`VehicleSystem::chassisRenderYOffset`**: cambiar la fórmula de `-mesh->aabbMin.y` a `-mesh->aabbMin.y + wheelRestCompression`, donde `wheelRestCompression` es el length que el spring comprime bajo gravedad en equilibrio.
2. La fórmula del equilibrium: `compressionAtRest = (mass/4 * g) / springStiffness`, con `springStiffness = (2π * freqHz)² * mass/4`. Resolviendo: `compressionAtRest = g / (2π * freqHz)²` (independiente de mass/N_wheels, solo depende de freq).
3. Pero **eso es para 1 wheel**. Para el chassis 4-wheel, el offset que el chassis "baja" desde el spawn = `compressionAtRest` (porque las 4 wheels comprimen igual con CoM centrado).
4. Función helper `vehicle::wheelRestCompression(const WheelConfig&)` en `VehicleConfig.cpp` con la fórmula.
5. `chassisRenderYOffset` recibe el `VehicleConfig` (lookup vía `resolveConfig` o pasaje desde tick).
6. Alternative más simple si lo anterior se vuelve complejo: usar `suspensionMaxLength - suspensionMinLength` (range del spring) como aproximación. Menos físicamente exacta pero pragmática.

Validación: spawn DeLorean → chassis apoya exacto en el piso. Ruedas sin gap visible al floor.

### Bloque A — Verificar (y eventualmente arreglar) controles invertidos

**Mecánica del jugador:** roto el .glb del DeLorean al estándar industrial (+Z forward) sin agregar `rotationEuler: [0, 180, 0]` en el moodmap. WASD funcionan correctamente.

**Proceso:**

1. **Test runtime sin tocar código**: tools/glb/reorient.py al .glb del DeLorean (crear copia `_reoriented.glb`). Cargar como mesh. Sacar `rotationEuler` del moodmap. Test WASD.
2. **Si funciona**: bug A ya resuelto colateralmente por el quat sync del Bloque B de F2H70.1 (`useQuaternion=true` evita drift de euler). Actualizar el .glb original al reoriented + sacar workaround del moodmap + commit.
3. **Si NO funciona**: investigar el seat mount cam (probablemente `EditorPlayMode.cpp::updatePlayer` línea de mount). Identificar si la cam tiene un forward vector hardcoded `-Z`. Fix mínimo: alinear con `+Z`.

### Bloque D — Cierre

- Tests verdes (1029+ casos).
- `docs/hitos/F2H70-2.md` con detalle (formato análogo a F2H70.md).
- `docs/HITOS.md` entry one-liner.
- `docs/ESTADO_ACTUAL.md`: section 0.1 → F2H70.2, 0.2 → F2H70.1.
- `docs/DECISIONS.md`: decisión clave de la fórmula spring-aware del Bloque B + tradeoff exact vs pragmático.
- Move `docs/PLAN_HITO_F2H70-2.md` → `docs/archive/plans/`.
- Commits agrupados + tag `v1.59.0-fase2-hito70-2`.

---

## Riesgos / open questions

- **Bloque B fórmula exacta vs pragmática**: la fórmula exacta `g/(2πf)²` requiere meter el `VehicleConfig` a `chassisRenderYOffset`. La pragmática (`suspensionMaxLength - min`) no. Si el resultado pragmático luce OK visualmente, evitamos coupling extra.
- **Bloque A puede explotar a scope mayor**: si la cámara FPS del seat mount tiene asunciones más profundas de `-Z forward`, el fix toca `EditorPlayMode.cpp` + posibles refactors del input dispatch. Si toma > 1h, abrir como hito propio.
- **Bloque C damping muy alto** rompe la sensación arcade: el auto se "pega" al piso. Tunear con valores chicos (`0.3-0.6`) y validar runtime.
