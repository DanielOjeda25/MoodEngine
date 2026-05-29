# PLAN F4H2 — Primera arma hitscan (data-driven `.moodweapon`)

**Estado:** PLAN, sin código todavía.
**Predecesor:** F4H1.5 (audit + 3 splits, tag `v3.1.1-fase4-hito1-5`).
**Origen:** §5 PLAN_FASE4.md + decisión arquitectónica del dev al cerrar F4H1.5:
> *"no quiero nada hardcodeado, que pueda ser dinamico para reutilizar a futuro en otro juego"*

---

## Norte

Primer "se siente bien disparar?" del juego. Pulso el clic, sale un disparo, hay impacto en lo que mire, y si era el maniquí (F4H1) le baja vida y lo tira al piso.

**Pero** la escopeta concreta NO vive en C++. Vive en un archivo `assets/pandemonium/weapons/shotgun.moodweapon` con su daño, alcance, dispersión, sonidos y partículas. El engine solo provee el **sistema** (raycast + aplicar daño + spawn feedback). El juego PANDEMONIUM provee la **data**.

Beneficio inmediato: F4H3, F4H4 (siguientes armas) son solo agregar archivos JSON nuevos. Beneficio largo: otro juego sobre MoodEngine recibe el sistema de armas gratis.

---

## Frontera engine ↔ juego

**Engine (`src/engine/`):**
- `WeaponSpec` struct + loader JSON.
- `AssetManager::loadWeapon(path) → WeaponAssetId`.
- `WeaponComponent` (asset ref + runtime state: ammo, fireTimer).
- `WeaponSystem::tick(scene, dt)` + `WeaponSystem::fire(scene, entity)`.
- Bindings Lua `weapon.equip/fire/ammo/can_fire`.
- Inspector panel + AssetBrowser tab.

**Juego (`assets/pandemonium/` — fuera del engine):**
- `weapons/shotgun.moodweapon` (la escopeta concreta).
- Mesh + sound + vfx referenciados por el JSON.
- Script Lua del player que conecta input → `weapon.fire(playerTag)`.

Engine NUNCA referencia "shotgun" por nombre. Engine SIEMPRE trabaja con `WeaponAssetId` opaco.

---

## Schema `.moodweapon` (propuesta)

```json
{
  "version": 1,
  "displayName": "Escopeta",
  "category": "hitscan",

  "damage": 15.0,
  "range": 25.0,
  "pellets": 8,
  "spreadDeg": 6.0,

  "fireRatePerSec": 1.5,
  "magazineSize": 6,
  "reloadTimeSec": 1.8,

  "viewmodelMesh": "viewmodel/shotgun.obj",
  "viewmodelMaterial": "materials/shotgun.moodmaterial",

  "fireSound": "sfx/shotgun_fire.ogg",
  "impactSound": "sfx/bullet_impact.ogg",
  "muzzleVfx": "vfx/muzzle_flash.moodvfx",
  "impactVfx": "vfx/bullet_impact.moodvfx",

  "ignoreOwner": true
}
```

**Categorias soportadas en F4H2:** solo `hitscan` (raycast instantáneo). `projectile` (proyectil físico) y `melee` se agendizan a hitos propios. El loader debe ignorar gracefully campos desconocidos para forward-compat.

**Pellets = 1** equivale a pistola. **Pellets > 1** + spreadDeg > 0 da escopeta. Un solo schema cubre los 2 casos.

---

## Plan en sub-tareas

### Sub-tarea 1 — Asset type `.moodweapon`
- `src/engine/assets/types/WeaponAsset.h`: struct `WeaponSpec` con todos los campos del schema + asset IDs resueltos (`MeshAssetId viewmodelMesh`, `AudioAssetId fireSound`, etc.).
- `src/engine/assets/loaders/WeaponLoader.{h,cpp}`: parse JSON → `WeaponSpec`. Resuelve sub-assets via `AssetManager` recursivo (mirror `AssetManager_Material.cpp`).
- `src/engine/assets/manager/AssetManager_Weapon.cpp`: `loadWeapon(path) → WeaponAssetId`, `getWeapon(id) → const WeaponSpec*`, `missingWeaponId()`, `weaponPathOf(id)`.
- Header `AssetManager.h`: agregar `using WeaponAssetId = u32;` + métodos públicos.
- Tests: `tests/test_weapon_asset.cpp` (parse roundtrip, missing fields → defaults sanos, clamp `damage>=0`, `fireRate>0`).

### Sub-tarea 2 — `WeaponComponent` + serialización
- `Components_Gameplay.h`: agregar
  ```cpp
  struct WeaponComponent {
      u32 weaponAssetId = 0;
      u32 currentAmmo = 0;
      f32 fireTimer = 0.0f;
      f32 reloadTimer = 0.0f;
      bool firing = false;
  };
  ```
- `SceneSerializer.h`: `SavedWeapon { weaponPath, currentAmmo }`.
- `EntitySerializer.cpp` + `_Parse.cpp` + `SceneLoader.cpp`: round-trip. Solo `weaponPath` y `currentAmmo` persisten — `fireTimer/reloadTimer` son transients.
- Tests: `tests/test_scene_serializer_gameplay.cpp` extendido con weapon roundtrip.

### Sub-tarea 3 — `WeaponSystem` engine-generic
- `src/engine/gameplay/WeaponSystem.{h,cpp}` (~150-200 LOC):
  - `tick(scene, dt)`: decrementa timers; auto-reload cuando `currentAmmo == 0`.
  - `fire(scene, entity, camera) → bool`: valida `fireTimer<=0 && currentAmmo>0`; lee spec; raycast N veces (1 por pellet) con dispersión cónica; aplica `Health::applyDamage` al hit; spawna feedback; consume munición; reset `fireTimer = 1/fireRate`.
  - `reload(scene, entity) → bool`: arranca `reloadTimer = spec.reloadTimeSec`.
- Dispersión: para N pellets, generar N direcciones random dentro de cono de `spreadDeg` (usar `glm::angleAxis` random uniform sobre disco perpendicular al forward).
- Feedback al hit:
  - Sonido one-shot 3D positional via `AudioDevice::play(impactSound, vol, false, true, hitPoint)`.
  - Particles via emplace temporal de `ParticleEmitterComponent` (config viene del `.moodvfx` referenciado).
  - Decal **deferred** a backlog (no existe DecalComponent — ver decisiones).
- Tests: `tests/test_weapon_system.cpp` (rate limit, ammo decrement, raycast hit damage, ignore owner).

### Sub-tarea 4 — Bindings Lua
- `src/engine/scripting/bindings/LuaBindings_Weapon.cpp` (mirror `LuaBindings_Health.cpp`):
  - `weapon.equip(tag, "weapons/shotgun.moodweapon") → bool`
  - `weapon.fire(tag) → bool`
  - `weapon.reload(tag) → bool`
  - `weapon.can_fire(tag) → bool`
  - `weapon.ammo(tag) → int`
  - `weapon.spec(tag) → table` (read-only: damage, range, name, etc.)
- Usar `bindings::findEntityByTag` de `BindingsCommon.h` (F4H1.5).

### Sub-tarea 5 — Input bridge Play mode
- Polling fire via mouse-left en `PlayerApplication::updateCamera` / dedicado `updateWeapons`.
- **NO hardcoded GLFW_MOUSE_BUTTON_LEFT** — expone keybinding en UserSettings:
  - `UserSettings.input.keybindings.fire = "mouse_left"`.
  - Default = mouse_left.
  - Se lee del Welcome modal / Settings menu (post-F4 amplía UI).
- Lua puede sobrescribir: `Input.is_action_pressed("fire")` (binding nuevo si no existe).

### Sub-tarea 6 — Inspector + AssetBrowser
- `InspectorPanel_Weapon.cpp` (~70 LOC, mirror `InspectorPanel_Health.cpp`):
  - Drag-drop `.moodweapon` desde browser → `weaponAssetId`.
  - Visualizar spec read-only (damage/range/ammo/rate).
  - Edit `currentAmmo` (debugging).
  - Reset → vacía asset ref.
- `AssetBrowserPanel`: nueva tab "Weapons" filtrada por `.moodweapon`. Icono dedicado.
- Drag-drop payload type: `"MOOD_WEAPON_ASSET"` (u32).

### Sub-tarea 7 — Crear Entidad "Pistola" / "Escopeta"
- ¿Va en "+ Crear Entidad" → tab Gameplay? **NO** — porque esos items meten data hardcodeada en C++. En su lugar:
  - El "+ Crear Entidad" → tab Gameplay → "Arma (vacía)" crea entidad con `WeaponComponent.weaponAssetId = 0` (slot vacío).
  - El dev drag-drop'ea el `.moodweapon` al slot en el Inspector.
  - Alternativa: dejarlo como `PrefabLinkComponent` apuntando a `pandemonium/prefabs/shotgun.moodprefab` que ya tiene su WeaponComponent + asset linkeado.
- Decisión D3 abajo.

### Sub-tarea 8 — Demo data PANDEMONIUM
- Crear `assets/pandemonium/weapons/shotgun.moodweapon`.
- Audio stubs: `assets/pandemonium/sfx/shotgun_fire.ogg` (placeholder — el dev provee real o sintetizado).
- Si no hay assets reales, weapon usa fallback sound silencioso + log warn.

### Sub-tarea 9 — Tests + docs
- Suite full verde post-cambios.
- Commit con "Chequear:" bullets (entrar al editor, abrir proyecto, agregar Player + WeaponComponent, drag escopeta, Play, click → maniquí cae).
- Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.
- Tag `v3.2.0-fase4-hito2`.

---

## Decisiones (cerradas con AskUserQuestion)

- **D1 — Escopeta primero (8 pellets + dispersión).** Más visceral, expone dispersión desde día 1. Pistola queda para F4H3 si emerge. Schema soporta ambas vía `pellets: N`.
- **D2 — Sonido + partícula de impacto siempre.** Pegás pared/piso → chispas + ruido. Decal queda agendado a backlog (no existe DecalComponent).
- **D3 — Slot vacío + drag-drop.** "+ Crear Entidad" → tab Gameplay → "Arma (vacía)" emplaza WeaponComponent con `weaponAssetId=0`. Dev drag-drop'ea `.moodweapon` del browser al Inspector. Prefabs armados listos quedan a backlog (F4H3+).
- **D4 — Data-driven keybindings desde día 1.** `UserSettings.input.keybindings.fire = "mouse_left"` leído por engine. UI rebindable se difiere a sub-hito propio.

---

## Métricas de éxito

- 1 `.moodweapon` parse roundtrip verde.
- Raycast aplica damage al maniquí (F4H1) y lo tira al piso.
- Rate limit: hold-fire no dispara más rápido que `fireRatePerSec`.
- Sonido + particle disparan en posición del hit.
- 0 strings "shotgun" / "pistol" en `src/engine/` (todo via asset ID).
- Tests pasan: suite verde post-cambios.
- LOC: WeaponSystem.cpp < 300, WeaponLoader.cpp < 200, todos los nuevos archivos bajo soft cap 500.

---

## Backlog (NO entra a F4H2)

- **Decals de impacto** — no existe DecalComponent en engine. Sub-hito F4H2.1 o agendizar a F4H10 (polish).
- **Reload animation** — viewmodel mesh animado, requiere skeletal anim hook desde script.
- **Recoil / camera shake** — efecto cinético del disparo. Post-F4H2.
- **Bullet trace / tracer** — línea visible del raycast. Post-F4H2.
- **Categorías `projectile` / `melee`** — schema preparado, sistema NO. Hitos propios.
- **Settings UI para rebindear "fire"** — F4H3+.

---

## Riesgos

- **Particle emitter como entity temporal** puede leakear entities si lifetime no se respeta. Mitigación: WeaponSystem tracks particle entities spawned y las destruye al expirar.
- **AudioClip por path**: si `.ogg` no existe, log warn + fallback silencioso (no crash).
- **Raycast layer mask**: el viewmodel del jugador NO debe ser hit. `ignoreOwner=true` en el spec + pasar `ignoredBodyId` del player a `PhysicsWorld::raycast`.
- **Reload concurrente con fire**: estado claro — `reloadTimer>0` bloquea fire.

---

## Cierre — F4H2 Bloque A (2026-05-29, tag `v3.2.0-fase4-hito2-A`)

**Status:** ✅ **Bloque A CERRADO**. Infraestructura data-driven completa: asset type, component, system, lua bindings, inspector, tests, demo data, wiring tick + lua. Sub-tareas 5/6.b/7 + AudioDevice cascada split a [PLAN_HITO_F4H2_B.md](PLAN_HITO_F4H2_B.md).

**Suite:** **1342/12036 verde** (+33 tests / +105 asserts vs F4H1.5).

**Archivos nuevos (Bloque A):**
- `src/engine/gameplay/weapon/WeaponSpec.{h,cpp}` (~180 LOC) — schema + parse + clamps.
- `src/engine/gameplay/weapon/WeaponSystem.{h,cpp}` (~310 LOC) — fire / reload / tickSystem / equip / canFire / ammoLeft.
- `src/engine/assets/manager/AssetManager_Weapon.cpp` (~55 LOC).
- `src/engine/scripting/bindings/LuaBindings_Weapon.cpp` (~140 LOC).
- `src/editor/panels/scene/InspectorPanel_Weapon.cpp` (~130 LOC).
- `assets/weapons/shotgun.moodweapon` (data).
- `tests/test_weapon_asset.cpp` (18 cases / 60 asserts).
- `tests/test_weapon_system.cpp` (14 cases / 45 asserts).
- `docs/PLAN_HITO_F4H2_B.md` (plan próximo bloque).

**Archivos modificados:**
- `AssetManager.{h,cpp}` — `WeaponAssetId` + métodos públicos + slot 0 fallback.
- `AssetManager_Rename.cpp` — rename helper para `.moodweapon`.
- `Components_Gameplay.h` — `WeaponComponent` + `ParticleBurstComponent` (transient).
- `SceneSerializer.h` — `SavedWeapon { weaponPath, currentAmmo }`.
- `EntitySerializer.cpp` + `EntitySerializer_Parse.cpp` + `SceneLoader.cpp` — write/parse/restore.
- `InspectorPanel.{h,cpp}` — `renderWeaponSection` + dispatch en gameplay.
- `LuaBindings.{h,cpp}` — declaración + invocación de `setupWeaponBindings`.
- `EditorApplication_Run.cpp` — `Weapon::tickSystem` en Play loop.
- `CMakeLists.txt` (root + tests) — sources.

**Cómo probarlo HOY (vía Lua manual):**
```lua
-- En el editor, scriptear una entidad player con tag "player":
weapon.equip("player", "weapons/shotgun.moodweapon")
weapon.fire("player", {origin_x=0, origin_y=2, origin_z=0,
                       dir_x=0, dir_y=0, dir_z=-1})
print(weapon.ammo("player"))   -- 5 (consumió 1 bala)
weapon.reload("player")
```

**Pendiente para "click → mata maniquí" sin Lua manual:** Bloque B.
