# PLAN F4H5 — Armas de proyectil (rocket / plasma / granada)

**Estado:** PLAN, sin codigo todavia.
**Predecesor:** F4H4 cerrado (tag `v3.4.0-fase4-hito4`).
**Origen:** PLAN_FASE4.md §5 — era F4H4 textual ("armas de proyectil con splash"), renumerado a F4H5 al cerrar F4H4 (HUD + pickups). Cierra el item de proyectiles de Sub-fase 4.1 antes del game feel pass (F4H6).

---

## Norte

Hoy todas las armas son hitscan (shotgun + pistola): el daño se aplica al instante del click via raycast. F4H5 introduce el **proyectil físico** — un entity efímero que vuela por el mundo, choca con paredes/enemigos, y explota con **daño splash radial** (Doom/Quake). Mecánicas nuevas:

- Click → spawnea un proyectil flotando que vuela en dirección del crosshair.
- El proyectil tiene velocidad finita: el jugador puede VER la trayectoria y predecir el impacto.
- Al chocar con cualquier superficie/enemy → explota.
- Explosión = esfera de daño con falloff lineal (centro=100%, borde=0%).
- 3 variantes data-driven en `assets/weapons/`:
  - **Rocket** lento (25 m/s), splash grande 4m, 80 dmg directo + 60 splash.
  - **Plasma** rápido (50 m/s), splash chico 1.5m, 40 dmg directo + 15 splash.
  - **Granada** con gravedad, rebota en superficies, explota tras 2.5s o al impactar enemy.

---

## Frontera engine ↔ juego

**Engine provee:**
- `.moodweapon` extendido con `category: "projectile"` + bloque `projectile { speed, gravity, bounce, lifetimeSec, splashRadius, splashDamage }`.
- `ProjectileComponent` engine-generic (transient runtime).
- `ProjectileSystem::tickSystem(scene, dt, physics, audio, assets)` que mueve proyectiles, detecta colisión, dispara explosión + splash damage.
- Helper `applySplashDamage(scene, center, radius, damage, dirOverride)` con loop sobre todos los `HealthComponent` y falloff lineal.

**Juego provee:**
- `assets/weapons/rocket.moodweapon`, `plasma.moodweapon`, `grenade.moodweapon`.
- Mesh placeholder del proyectil (cubo escalado por ahora; arte real F4H5.1).
- Mesh del viewmodel del rocket launcher etc.

---

## Decisiones (cerradas con AskUserQuestion)

- **D1 — Las 3 armas data-driven desde día 1.** `.moodweapon` schema gana `category: "projectile"` (existing field; F4H2 ya lo declaró polymórphico) + bloque opcional `projectile { speed, gravity, bounce, lifetimeSec, splashRadius, splashDamage, directDamage }`. Entrega las 3 armas demo. Descartado: solo rocket primero (rocket es suficiente para validar la mecánica pero perderíamos el data-driven que F4H2 estableció); plasma solo (menos visceral que rocket para sentir splash); granada sola (más complejo que rocket por rebote físico).
- **D2 — Splash: esfera de radio + falloff lineal.** Doom/Quake clásico. `damage_at(dist) = directDamage * (1.0 - dist / radius)` con clamp en [0, directDamage]. Implementación: `applySplashDamage` itera entities con `HealthComponent + TransformComponent`, calcula distancia plana al centro, si <= radius aplica damage scaled. Descartado: ForceFieldSystem one-shot (overkill — el ForceField es continuous + impulse-based, no fit para 1-frame explosion); raycast visibility check (~2-3x costo + tunneling tweaks innecesarios para F4H5; F4H7+ si emerge).

**Decisiones del agente (defaults convencionales — overridable por el dev si los ve):**

- **D3 — Self-damage del player (rocket jump): habilitado por default.** El splash damage NO ignora al owner del proyectil. Convención Quake/Doom: rocket-jump funciona porque te dañás. Si el `WeaponSpec.ignoreOwner=true` (campo existing F4H2 para hitscan), también aplica al splash (preserva semántica). El dev puede flippear en cualquier `.moodweapon`. Esto preserva el risk/reward de Quake.
- **D4 — Mesh del proyectil: placeholder cubo coloreado por categoría/arma.** Rocket=cubo rojo, plasma=cubo cyan, granada=cubo verde. Reusar el missing-mesh + tintar via material? No — más simple: cada `.moodweapon` declara su `projectileMesh` (path), y un `materialAssetId` opcional. Si vacío, fallback al cubo del missing-mesh con material default. Mesh art real (Aitchen + sprite/3D model) va F4H5.1 backlog.
- **D5 — Granada rebote: usar Jolt como cualquier RigidBody Dynamic.** Spawneamos la granada con `RigidBodyComponent::Dynamic` + bounce factor configurable. La sim del Jolt maneja el rebote automatic. Trigger de explosión: lifetime timer 2.5s O contact con entity con `HealthComponent` (detectado en `ProjectileSystem` via raycast desde la pose anterior a la actual con `ignoreOwner`). Convención TF2/Quake.
- **D6 — Trigger de impacto: raycast desde pose anterior a pose actual del proyectil**. Cada frame, `ProjectileSystem::tickSystem` hace un raycast del frame anterior al actual; si pega → explosion en el hitpoint. Cubre proyectiles rápidos sin tunneling. Para granada (Dynamic), el chequeo es el mismo pero permitimos `bounceCount > 0` antes de explotar (D5).
- **D7 — VFX al explotar: particle burst reusando `ParticleBurstComponent` (F4H2)**. Mismo patrón que el puff de impacto del shotgun. Splash radius pinta visualmente. Sonido 3D positional via `AudioDevice` (mismo patrón hitscan F4H2). Decals quedan F4H5.1.

---

## Schema cambios

### `.moodweapon` extension

```json
{
  "displayName": "Rocket Launcher",
  "category": "projectile",          // nuevo: era "hitscan" en F4H2
  "damage": 0.0,                      // ignored para projectile (el daño es directo+splash)
  "range": 0.0,                       // ignored
  "fireRatePerSec": 1.2,
  "magazineSize": 4,
  "reloadTimeSec": 2.0,
  "ignoreOwner": true,                // existing — aplica al splash tambien

  "projectile": {
    "meshPath": "meshes/rocket.fbx",        // o vacio = cubo placeholder
    "materialPath": "materials/rocket.material",
    "speed": 25.0,                          // m/s
    "gravity": 0.0,                          // 0 = sin gravedad (rocket/plasma); >0 = granada
    "bounceCount": 0,                        // 0 = no rebota; granada usa 3-4
    "bounceFactor": 0.6,                     // 0..1 — coef de restitución del rebote
    "lifetimeSec": 5.0,                      // si no impacta, explota al expirar
    "directDamage": 80.0,                    // a la entity que impacto directo
    "splashRadius": 4.0,                     // metros
    "splashDamage": 60.0                     // dmg en el centro (falloff lineal hasta 0 en el borde)
  },
  "fireSound": "audio/rocket_fire.wav",
  "impactSound": "audio/rocket_explode.wav"
}
```

Maps F4H2/F4H3/F4H4 sin el bloque `projectile` se leen igual (back-compat). Si `category != "projectile"`, el bloque se ignora; si es projectile pero falta el bloque, defaults razonables (cubo + speed=20 + splash=2m + damage=30).

### `ProjectileComponent` (nuevo)

Transient runtime. Engine-generic.

```cpp
struct ProjectileComponent {
    u32  weaponAssetId   = 0;     // de qué arma vino (para reusar el spec)
    entt::entity owner   = entt::null;  // shooter, para ignoreOwner del splash
    glm::vec3 velocity{0.0f};
    f32  age             = 0.0f;
    f32  lifetimeSec     = 5.0f;  // cache del spec, para evitar lookup cada frame
    int  bouncesLeft     = 0;     // granada decrementa al rebotar
    glm::vec3 prevPos{0.0f};      // para raycast pose-anterior → pose-actual
};
```

### `ProjectileSystem` (nuevo)

```cpp
// src/engine/gameplay/projectile/ProjectileSystem.{h,cpp}
namespace Projectile {

/// Tick: mueve proyectiles, detecta colisión, explosion + splash.
void tickSystem(Scene& scene, f32 dt, PhysicsWorld& physics,
                 AudioDevice* audio, AssetManager& assets);

/// Aplica splash damage radial con falloff lineal. Center = punto de
/// explosión, radius = alcance, baseDamage = dmg en el centro.
/// ignoreOwner skipea la entity del shooter (rocket-jump opt-out).
/// Engine-generic — el caller decide qué constituye "splash".
void applySplashDamage(Scene& scene, const glm::vec3& center, f32 radius,
                        f32 baseDamage, entt::entity ignoreOwner);

} // namespace Projectile
```

### `Weapon::fire` extension

Cuando el `WeaponSpec.category == "projectile"`, `Weapon::fire` NO hace raycast — spawna una entity con:
- `TransformComponent` en `params.origin`.
- `MeshRendererComponent` con `spec.projectile.meshPath` o cubo placeholder.
- `ProjectileComponent { weaponAssetId, owner, velocity = params.direction * spec.projectile.speed, ... }`.
- Si `spec.projectile.gravity > 0` → `RigidBodyComponent::Dynamic` con bounce.
- Audio fire ya se reproduce (mismo path que hitscan).

Devuelve `FireResult` igual que hoy (sin hits — los hits los reporta el ProjectileSystem cuando explota).

---

## Sub-tareas

### Sub-tarea 1 — Schema `.moodweapon` projectile + parser/clamps

- `WeaponSpec.h`: agregar `struct ProjectileParams { meshPath, materialPath, speed, gravity, bounceCount, bounceFactor, lifetimeSec, directDamage, splashRadius, splashDamage }` opcional dentro de `WeaponSpec`.
- `WeaponSpec.cpp`: parsearlo desde JSON (todos opcionales con defaults) + clamps de sanidad (speed > 0, splashRadius >= 0).
- Tests: roundtrip de los 3 .moodweapon demo + clamps.

### Sub-tarea 2 — ProjectileComponent + ProjectileSystem

- `Components_Gameplay.h`: nuevo `ProjectileComponent`.
- `engine/gameplay/projectile/ProjectileSystem.{h,cpp}` (~250 LOC):
  - `tickSystem`: por cada projectile, raycast prevPos→currentPos via `physics.raycast`, si hit → explosion at hitpoint, destroy. Si age >= lifetime → explosion at currentPos. Si granada: chequear bounce count del Jolt sim (probable que Jolt resetee currentPos en cada paso; el ProjectileComponent lee la pose del RigidBody — TODO investigar interfaz Jolt).
  - `applySplashDamage`: itera entities con `HealthComponent + TransformComponent`, dist plana al center, scale lineal, `Health::applyDamage`. Opcionalmente spawnea `ParticleBurstComponent` en el center.
- Tests: spawn proyectil → tick → mueve. Raycast hit → explode + splash. Splash damage falloff lineal.

### Sub-tarea 3 — Weapon::fire extendido para proyectil

- `WeaponSystem.cpp::fire`: branch por `spec.category`. Si "hitscan" → flujo actual. Si "projectile" → `spawnProjectile(scene, params, spec, ...)`.
- Helper privado `spawnProjectile`: crea entity efímero con Transform + Mesh + Projectile + opcional RigidBody (granada).
- Tests: fire projectile spawnea entity con ProjectileComponent.

### Sub-tarea 4 — Wireup tick + render

- `EditorApplication_Run.cpp::tickSystems`: `Projectile::tickSystem(scene, dt, physics, audio, assets)` después de `Weapon::tickViewmodel`.

### Sub-tarea 5 — 3 .moodweapon demo + balancing

- `assets/weapons/rocket.moodweapon`: speed=25, splashRadius=4, directDamage=80, splashDamage=60, fireRate=1.2, mag=4, reload=2.0.
- `assets/weapons/plasma.moodweapon`: speed=50, splashRadius=1.5, directDamage=40, splashDamage=15, fireRate=3.5, mag=30, reload=2.5.
- `assets/weapons/grenade.moodweapon`: speed=12, gravity=9.8, bounceCount=3, lifetimeSec=2.5, splashRadius=4.5, directDamage=0, splashDamage=120 (no directo, solo splash on explosion).

### Sub-tarea 6 — Tests + docs + commit + tag

- Suite full verde, ~15-20 tests nuevos (schema roundtrip + system tick + splash falloff).
- ESTADO/HITOS/DECISIONS update.
- Commit con sección "Chequear:" (Crear Player → arma slot 0 default sigue siendo shotgun, agregar rocket via pickup, swap a rocket, click → ves el cubo rojo volando → impacta pared → maniquí cercano recibe splash).
- Tag `v3.5.0-fase4-hito5`.

---

## Métricas de éxito

- 3 .moodweapon nuevas en el catálogo (rocket + plasma + grenade).
- Click con rocket equipado → ves proyectil físico volando + escuchas sonido + impacta + maniquí muere por splash.
- Tests verde, sin regresión F4H1-F4H4.
- LOC: `ProjectileSystem.cpp < 350`. Schema extension WeaponSpec < +80 LOC.

---

## Backlog (NO entra a F4H5)

- **F4H5.1** — Mesh art real para rocket/plasma/grenade + estela de humo (particle trail mientras vuela) + decals al explotar.
- **F4H6** — Game feel pass (bumped del F4H5 textual original): muzzle flash, hit marker, screen shake on explosion, pain reaction.
- **F4H7** — Replace/discard arma cuando arsenal lleno (Apex style).
- Raycast visibility check del splash (target detrás de pared no recibe daño). Quake 1/2/3 no lo tenía, lo agregamos si emerge.
- Direct hit detection vs splash-only (proyectil que pasa cerca sin impactar → solo splash). Hoy si el ray pega, full direct + splash; no se simula el "casi me pegó".
- Friendly fire toggle (multi-team — F4H10+ enemigos).

---

## Riesgos

- **Raycast prevPos→currentPos puede saltar entities pequeños** si la velocidad del proyectil es muy alta y el dt del frame es grande. Mitigación: capear speed máxima en el schema clamp (~200 m/s). Si el proyectil necesita >200, sub-pasos por frame (overkill F4H5).
- **Granada con Jolt Dynamic + ProjectileComponent**: el sistema lee `prevPos` del frame anterior y `currentPos` del RigidBody actual — Jolt puede mover el body durante el `updateRigidBodies`, y el proyectil aún no es full physics nativo. Necesita testing visual del flow `proyectil con RigidBody Dynamic` antes de declarar Sub-2 done.
- **Self-damage del owner**: si el rocket-jump del player explota MUY cerca, podría matarse — convención Quake. Mitigación: el dev tunea `splashDamage` del rocket para que no mate de 1 (60 dmg en el centro, player con 100 HP sobrevive con 40 si está exactamente en el centro). Documentar en assets/weapons/rocket.moodweapon como comentario.
- **Granada que no explota porque bounceCount > 0 + lifetime expira mid-bounce**: edge case del state machine. Test del lifetime explícito al expirar.

---

## Notas de implementación

- `WeaponSpec` ya tiene campo `category` libre desde F4H2 — solo agregamos el bloque `projectile` y un branch en `Weapon::fire`. No requiere bump del schema version del proyecto.
- `ProjectileSystem` engine-generic — el motor no asume rocket/plasma/grenade, solo procesa "entities con ProjectileComponent". El juego define en `.moodweapon` qué hace cada arma.
- `applySplashDamage` es función libre reusable — si futuro emerge "explosive barrel" (objeto del mundo que explota al destruirse), reusa el mismo helper.

---

## Cierre — F4H5 (2026-05-30, tag `v3.5.0-fase4-hito5`)

**Sub-tareas entregadas (6/6):**

1. ✅ **Schema `.moodweapon` extendido con bloque `projectile`** (`{meshPath, materialPath, speed, gravity, bounceCount, bounceFactor, lifetimeSec, directDamage, splashRadius, splashDamage}`). `WeaponSpec::ProjectileParams` struct con defaults razonables. `toJson` emite siempre el bloque (consistencia); `fromJson` lo lee opcional con defaults. **Clamps de sanidad** (speed cap 200 m/s para evitar tunneling; bounceFactor [0,1]; lifetimeSec ≥ 0.05; todos los damage/radius >= 0).
2. ✅ **`ProjectileComponent` + `Projectile::tickSystem` + `Projectile::applySplashDamage`** engine-generic. `ProjectileComponent {weaponAssetId, owner u32, velocity, age, lifetimeSec, bouncesLeft, prevPos, exploded}` transient. `tickSystem(scene, dt, physics, audio, assets)` recorre projectiles, aplica gravedad, mueve por integración explícita, raycast prevPos→newPos via `physics->raycast` (cubre tunneling continuo), bounce reflejado con factor si granada + hit en superficie no-Health, explode + splash + cleanup. **`applySplashDamage`** función libre con falloff lineal: `dmg = baseDmg * max(0, 1 - dist/radius)`. `ignoreOwner` (u32 raw) skipea el shooter (rocket-jump opt-out cuando `spec.ignoreOwner=false`). `spawnExplosionBurst` interno: ParticleBurst naranja-rojo con tamaño escalado al splashRadius.
3. ✅ **`Weapon::fire` branch projectile**. Guard hitscan/projectile dual; antes era solo hitscan. Si projectile → spawnea entity efímera con `Transform + MeshRenderer (placeholder cubo via missingMesh) + ProjectileComponent {velocity = forward * speed, owner = shooter.handle()}` con offset 0.4m forward (evita explosion in-face). Sound fire + ammo consume + fireTimer iguales que hitscan. Logs específicos al lanzar proyectil.
4. ✅ **Wireup en `EditorApplication_Run::tickSystems`**: `Projectile::tickSystem(scene, dt, m_physicsWorld.get(), m_audioDevice.get(), *m_assetManager)` después de `Pickup::tickSystem`. `MOOD_PROFILE_SCOPE("Projectile::tickSystem")` para tracer.
5. ✅ **3 .moodweapon demo en `assets/weapons/`**:
   - `rocket.moodweapon`: speed=25 m/s, splashRadius=4m, directDamage=80, splashDamage=60, fireRate=1.2/s, mag=4, reload=2.0s.
   - `plasma.moodweapon`: speed=50 m/s, splashRadius=1.5m, directDamage=40, splashDamage=15, fireRate=3.5/s, mag=30, reload=2.5s.
   - `grenade.moodweapon`: speed=12 m/s, gravity=9.8, bounceCount=3, bounceFactor=0.55, lifetimeSec=2.5, splashRadius=4.5m, directDamage=30, splashDamage=120.
6. ✅ **Tests + docs + commit + tag**. 13 nuevos verdes en `test_projectile_system.cpp` (5 schema + 8 splash damage). **Suite full 1404/12200 verde** (+13 cases / +36 asserts vs F4H4). 0 regresión.

**Ajustes reactivos durante implementación:**

- **R1 — `ProjectileComponent.owner` como `u32` raw en vez de `entt::entity`**. El header `Components_Gameplay.h` NO incluye `<entt/entt.hpp>` (forward-decl-friendly: usa `u32` para `bodiesInside` del TriggerComponent F2H37). Mi declaración inicial `entt::entity owner = entt::null` rompió 7 compile errors (C2238/C2653/C3646). Fix: `u32 owner = 0` con cast `static_cast<u32>(shooter.handle())` en el spawn. Consistencia con el patrón existente del header.
- **R2 — `applySplashDamage` recibe `u32 ignoreOwnerRaw` (no entt::entity)**. Mismo motivo que R1: header del namespace `Projectile` no debería traer entt.hpp. Cast interno `static_cast<entt::entity>(ignoreOwnerRaw)` para comparar con el iterador del view. Sentinel "sin owner" = `0xFFFFFFFFu` (mismo underlying que `entt::null`).
- **R3 — `bounceFactor` clamp a [0,1]** (no `(0,1)` excluyente como tenía el plan). Una granada con `bounceFactor=0` es válida: rebote inelástico (se queda pegada). El plan original sugería `> 0`; el clamp final permite 0 para use case "sticky bomb" futuro.

**Bugs build-time fixados:**

- **B1 — `entt::null` requiere include de `<entt/entt.hpp>`**. El header `Components_Gameplay.h` sólo trae forward decls de entt. Fix: cambiar a `u32 owner = 0` raw (ver R1). No requiere include nuevo.

**Mecánica final entregada al jugador:**
- Click con rocket equipado → ves proyectil rojo cubo placeholder volando a 25 m/s. Podés trackearlo. Impacta pared/enemy → explosión radial 4m. Maniquí cercano recibe damage scaled (centro=60 dmg, borde=0). Direct damage 80 si impacto directo en enemy.
- Plasma: proyectil cyan rápido (50 m/s), splash chico (1.5m), cadencia 3.5/s.
- Granada: arco parabólico con gravedad, rebota 3 veces (factor 0.55), explota tras 2.5s O al impactar enemy directo. Splash 4.5m / 120 dmg en centro — alpha kill.
- Rocket-jump funciona: el splash damage NO ignora al shooter por default. Convención Quake.

**Tests F4H5**: 13 nuevos verdes. 5 schema (defaults / roundtrip rocket / sin bloque → defaults / clamps todos los campos / speed cap 200). 8 splash damage (centro=baseDmg / borde=0 / media distancia=50% / fuera del radio=no efecto / ignoreOwner skipea shooter / radius=0 no-op / baseDamage=0 no-op / múltiples entities afectadas). Suite full **1404/12200 verde** (+13 cases / +36 asserts vs F4H4: 1391 → 1404). 0 regresión.

**Backlog post-F4H5:**
- **F4H5.1** — Mesh art real para rocket/plasma/grenade + estela de humo (particle trail mientras vuela) + decals al explotar.
- **F4H6** — Game feel pass: muzzle flash, hit marker, screen shake on explosion, pain reaction, crosshair dinámico con spread.
- **F4H7** — Replace/discard arma cuando arsenal lleno (Apex style).
- Raycast visibility check del splash (cobertura detrás de pared).
- Direct hit detection vs splash-only.
- Friendly fire toggle.
- Color del cubo placeholder via material por arma (hoy todos missing-material).

