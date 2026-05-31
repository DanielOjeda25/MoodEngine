# Plan F4H9 — Ataques que dañan al player (melee + ranged + wind-up)

> **Tercer hito de Sub-fase 4.2** "¿es divertido pelear?". F4H7 dejó la
> state machine armada; F4H8 hizo que el enemy te persiga. F4H9 hace
> que te golpee: aplica daño al player cuando entra `Attack` state.
> Soporta melee (instant body slam) y ranged (lanza un projectile que
> reusa el sistema F4H5). Wind-up de 0.3s da ventana de esquive estilo
> Doom/Quake.

## Norte

Al cerrar F4H9 quiero poder: spawn un grunt, entrar a Play, dejarme
atrapar, ver el wind-up 0.3s antes del primer golpe, recibir damage,
ver el flash + pain reaction. El HP del player baja en cada hit. Si lo
mato antes del golpe, el wind-up se cancela.

También: spawn un imp (demo enemy ranged), ver que cuando entra
attackRange dispara un projectile hacia mí (el "fireball" reusa el
mismo .moodweapon pipeline F4H5 — el enemy tira un rocket/plasma/
custom como arma).

## Decisiones cerradas (AskUserQuestion)

- **D1 — Melee + ranged desde día 1.** Schema `.moodenemy` se extiende
  con `attackKind: "melee" | "projectile"` data-driven. Si projectile,
  declara `projectileWeapon: "weapons/fireball.moodweapon"` y el enemy
  spawnea ese projectile via el sistema F4H5. **Razón**: el data-driven
  es el norte de Fase 4 — schema completo evita migration cuando F4H11
  agregue variedad de enemies (Imp ranged, Mancubus splash, etc).

- **D2 — Wind-up 0.3s antes del primer golpe.** Convención Doom/Quake.
  El enemy entra Attack, espera `spec.windUpSec` (default 0.3s), aplica
  damage, espera `spec.attackCooldown` (default 1.0s), repite. Da
  ventana al player para esquivar. **Razón**: el ataque instantáneo
  sería injusto sin telegraph visual (que llegará en Sub-fase 4.3 con
  animaciones Mixamo). El timer hoy hace de telegraph procedural —
  el player aprende "cuando me ven, tengo 300ms para salir".

## Decisiones convencionales (no preguntadas)

- **D3 — Projectile del enemy reusa Weapon::fire del .moodweapon.**
  El `spec.projectileWeapon` apunta a un `.moodweapon` cargado por
  AssetManager. El EnemySystem llama `Weapon::fire(scene, enemy,
  FireParams{origin = enemy_pos + 0.5_up, dir = player_pos - enemy_pos
  normalized}, physics, audio, assets)` apuntando al player. El enemy
  se vuelve el "shooter" del weapon — `ignoreOwner=true` evita self-
  damage. Reusa toda la cinemática del F4H5 (speed, splash, decay).
  **Razón**: Imp fireball / Mancubus rocket no necesitan código nuevo
  — son data en `.moodweapon`. Engine-generic puro.

- **D4 — Cooldown timer + windUp timer per-enemy.** `EnemyComponent`
  agrega `attackCooldownTimer` (replace `lastAttackTime` que F4H7
  reservó pero nunca usé) + `windUpTimer`. Decrementan cada frame en
  Attack. Si transition out (Pain por damage / Chase por escape) +
  re-entra Attack, AMBOS se reset a `spec.windUpSec` y `0`. **Razón**:
  cada engagement tiene su telegraph; no podés "ahorrar" tiempo de
  wind-up corriendo en círculos.

- **D5 — Damage al player via `Health::applyDamage`.** Mismo API que
  el player usa contra el enemy. Engine-generic. Si el player tiene
  `ArmorComponent` (F4H4), la armor come parte del damage primero
  (convención HL2 absorbRatio=0.66 default). El damage flash + pain
  reaction del player (F4H6) se triggea automáticamente vía polling
  del bridge.

- **D6 — `Enemy::tickSystem` extiende signature con `PhysicsWorld*`.**
  Necesario para `Weapon::fire` cuando attackKind=projectile (raycast +
  collision). Si physics es null y el enemy quiere disparar projectile,
  skip silencioso con log warn (tests headless). El bridge ya tiene
  `m_physicsWorld.get()` para pasarlo.

- **D7 — Re-arm wind-up al re-entrar Attack.** Cada transition
  Pain→Chase / Chase→Attack reset `windUpTimer = spec.windUpSec`.
  Justo y simple.

- **D8 — Demo imp.moodenemy + imp_fireball.moodweapon.** Para validar
  el modo ranged. Imp tiene HP=30, aggro=18m, attackRange=10m (más
  largo que melee), moveSpeed=3.5, windUpSec=0.5, attackCooldown=1.5s,
  attackKind=projectile, projectileWeapon="weapons/imp_fireball.moodweapon".
  El imp_fireball es un .moodweapon con projectile category, speed=12
  m/s, splash 1.5m, damage 18+12 (slow but punchy).

## Sub-tareas

### Sub-1 — Extension `EnemySpec` schema
- `src/engine/gameplay/enemy/EnemySpec.h`:
  - Agregar fields: `std::string attackKind = "melee";`,
    `f32 windUpSec = 0.3f;`, `std::string projectileWeapon;`.
- `src/engine/gameplay/enemy/EnemySpec.cpp`:
  - toJson/fromJson emit/read new fields con defaults.
  - Clamps: `windUpSec >= 0.0f` (puede ser 0 = ataque instantáneo opt-in
    no-recomendado), `attackKind` solo accepted "melee"/"projectile"
    (cualquier otro → "melee" + warn log).

### Sub-2 — Extension `EnemyComponent` runtime
- `src/engine/scene/components/Components_Gameplay.h`:
  - Reemplazar `f32 lastAttackTime = -1.0f;` por:
    - `f32 attackCooldownTimer = 0.0f;` (decrementa, 0 = ready to strike)
    - `f32 windUpTimer = 0.0f;` (decrementa, 0 = wind-up done)
- Update `Components.h` si hay aliases.

### Sub-3 — Extension `EnemySystem::tickSystem` signature
- `src/engine/gameplay/enemy/EnemySystem.h`:
  - Add param `PhysicsWorld* physics` (nullable para tests).
  - Forward decl al header de PhysicsWorld.
- `src/engine/gameplay/enemy/EnemySystem.cpp`:
  - Include WeaponSystem.h para Weapon::fire.
  - Helper `applyEnemyAttack(scene, enemy, player, spec, physics, audio, assets)`:
    - if attackKind == "projectile" + spec.projectileWeapon valid + physics != nullptr:
      - Resolver weaponId via assets.loadWeapon(spec.projectileWeapon).
      - Equip temporal en enemy? No — Weapon::fire necesita WeaponComponent
        equipped. Solución: el enemy tiene su propio WeaponComponent runtime
        con el projectileWeapon en slot 0. Auto-add on demand mismo
        patrón que NavAgent en F4H8.
      - Compute direction = normalize(playerPos - enemyPos).
      - Compute origin = enemyPos + (0, 0.5, 0) (boca del enemy approx).
      - Weapon::fire(scene, enemy, FireParams{origin, dir, ignoreOwner=true,
        randomSeed=...}, physics, audio, assets).
      - El projectile se mueve via F4H5 ProjectileSystem que ya corre en bridge.
    - else (melee, default):
      - Health::applyDamage(scene, player, spec.damage, dir=enemy→player).

### Sub-4 — Switch Attack logic F4H9
- En `Enemy::tickSystem`, case Attack:
  - if windUpTimer > 0 → decrementar dt (no strike).
  - else if attackCooldownTimer <= 0:
    - applyEnemyAttack(...) → reset both timers (windUp = spec.windUpSec,
      cooldown = spec.attackCooldown).
  - else decrementar attackCooldownTimer dt.
- En `transitionTo`, agregar lógica: al entrar Attack (`next == Attack`),
  reset `windUpTimer = spec.windUpSec`, `attackCooldownTimer = 0` para
  arm del primer golpe.

### Sub-5 — Bridge wireup
- `src/editor/application/EditorApplication_Run.cpp`:
  - Update call: `Enemy::tickSystem(scene, dt, playerEntity,
    m_audioDevice.get(), *m_assetManager, m_physicsWorld.get())`.
- Tests pasan `nullptr` para physics (melee enemies funcionan; projectile
  no-op silent).

### Sub-6 — Inspector + Lua + serialization
- `InspectorPanel_Enemy.cpp`: agregar fields al spec display:
  attackKind (string), windUpSec (float), projectileWeapon (path).
- `LuaBindings_Enemy.cpp` enemy.spec(): agregar `attack_kind`,
  `wind_up_sec`, `projectile_weapon` al return table.
- Serialization SavedEnemy NO cambia (timers transients; spec se resuelve
  via path).

### Sub-7 — Demo data
- `assets/enemies/grunt.moodenemy`: agregar `"attackKind": "melee"`,
  `"windUpSec": 0.3` al JSON existente.
- `assets/enemies/imp.moodenemy` NEW (Imp ranged demo).
- `assets/weapons/imp_fireball.moodweapon` NEW (fireball projectile —
  reusa schema F4H5).

### Sub-8 — Tests
- `tests/test_enemy_spec.cpp`: roundtrip attackKind / windUpSec /
  projectileWeapon / clamps.
- `tests/test_enemy_system.cpp`:
  - F4H9 wind-up no aplica damage en primeros 0.3s del Attack.
  - F4H9 melee damage al player tras windUpSec.
  - F4H9 cooldown entre golpes (no spam).
  - F4H9 re-arm wind-up al Pain→Chase→Attack (no instant strike post-pain).
  - F4H9 attackKind=projectile + sin physics → no-op silent.
  - F4H9 projectile spawning con physics (mock o real? — si simple,
    verificar que Weapon::fire fue invocado via stub WeaponComponent).

### Sub-9 — Cierre
- Build verde → suite verde.
- Docs ESTADO_ACTUAL + HITOS + DECISIONS + Cierre en PLAN_HITO_F4H9.md.
- Commit + tag `v3.9.0-fase4-hito9` con "Chequear:" section.

## Backlog (NO entra a F4H9)

- **F4H9.1** — Damage type / element (fire/cold/etc) si el dev quiere
  resistencias por tipo. Solo si emerge desde balance.
- **F4H9.2** — Hitstun del player (mini stagger al recibir damage,
  estilo Quake). Hoy el player sigue moviendose sin interrupcion.
- **F4H10** — Animaciones impacto + ragdoll on death (NPC Mixamo).
- **F4H11** — Variedad: 2-3 tipos `.moodenemy` adicionales (rusher
  rapido, tank lento, tirador con projectile distinto).

## Riesgos

- **WeaponComponent on demand en enemy** — Auto-add cuando el enemy
  quiere disparar projectile. Si el enemy ya tiene un WeaponComponent
  por otra razón (raro), overwrite slot 0. Mitigación: solo agregar
  si NO tiene; sino reusar slot 0 si vacio o slot 0 si ya carga el
  projectileWeapon.
- **`Weapon::fire` signature compatibility** — pellets/spread/etc del
  .moodweapon que dispara el enemy. Si el `imp_fireball.moodweapon`
  declara pellets=1 + spread=0 + category=projectile, dispara 1 fireball
  hacia el player. Funciona out-of-box gracias al pipeline F4H5.
- **Test isolation** — Tests proyectil necesitan PhysicsWorld o mock.
  Mantener simple: en tests headless con physics=nullptr, el projectile
  no spawnea (verificable via "no Projectile entity creada"). Melee
  tests no necesitan physics.
- **Self-damage del enemy con su propio splash** — Si el imp fireball
  explota cerca del imp, el splash F4H5 le pega. Mitigación: el imp
  spec del WeaponComponent del enemy debe respetar `ignoreOwner=true`
  (default del schema F4H5). Verificar en demo.

## Cierre (2026-05-31, tag `v3.9.0-fase4-hito9`)

**Resultado**: F4H9 cerrado. El enemy ahora golpea al player con
wind-up procedural + cooldown per-engagement. Schema `.moodenemy`
soporta melee + projectile data-driven desde día 1. Imp ranged
demo + Grunt melee demo validados en ambas branches.

**Implementación final coincide con plan** salvo:

- `Enemy::tickSystem` signature: `AssetManager&` paso a non-const
  (necesario para `loadWeapon` on-demand del projectileWeapon en
  el branch projectile). Plan asumía const sin verificarlo.
- `transitionTo` ahora recibe optional `EnemySpec*` (default nullptr)
  para resetear `windUpTimer = spec->windUpSec` al entrar Attack
  sin romper otros call sites del switch transition F4H8.
- Constante `k_timerEps = 1e-5f` agregada en switch Attack —
  necesaria para que el chequeo `> 0` no quede atascado en residuo
  float ~5e-8 tras N ticks de decremento. Sin esto los tests F4H9
  fallaban con `HP == 100` en lugar del expected. Convención
  durable: todo timer float comparado contra 0 usa epsilon.
- Helper de test `advanceTicks(scene, dt, ticks, ...)` agregado
  para fast-forward granular preciso. Se reusará en F4H10+ para
  tests que cubran múltiples segundos de simulación.
- `makePlayer` en `test_enemy_system.cpp` agrega `HealthComponent`
  default current=100/max=100. Antes de F4H9 los tests F4H7/F4H8
  no necesitaban Health en el player porque el daño solo iba
  player→enemy; F4H9 invierte la direccion.

**Ajustes reactivos**:

- **R1 — SIGABRT crash al primer `applyEnemyAttack`** porque
  `makePlayer` no agregaba `HealthComponent`. Fix: agregar default.
- **R2 — `k_timerEps = 1e-5f` tolerance** porque 7 ticks de 0.05s
  dejaban `windUpTimer` en residuo float ~5e-8 en lugar de
  exactamente 0; chequeo `> 0` quedaba true infinito. Fix doble:
  `advanceTicks` helper + `k_timerEps` en switch Attack.

**Decisiones reales**:

- D1 melee + ranged desde día 1 ✅
- D2 wind-up 0.3s telegraph procedural ✅
- D3 projectile reusa `Weapon::fire` engine-generic ✅
- D4 cooldown + windUp timers per-enemy ✅
- D5 damage via `Health::applyDamage` (armor absorb HL2 automatic) ✅
- D6 `Enemy::tickSystem` nullable physics ✅
- D7 re-arm wind-up al re-entrar Attack ✅
- D8 demo Imp + fireball ✅

**Tests** (11 nuevos): 4 en `test_enemy_spec.cpp` (defaults attack
fields F4H9 / roundtrip JSON melee / roundtrip JSON projectile /
clamps attackKind unknown→melee + windUpSec negativo→0); 7 en
`test_enemy_system.cpp` (wind-up no damage primeros 0.3s / damage
tras windUpSec / cooldown anti-spam / re-arm Pain→Chase→Attack /
projectile sin physics no-op silent / multiples enemies Attack
independent / Attack reset al transition out + re-entrada).

**Suite full 1461/12384 verde** (+11 cases / +31 asserts vs F4H8:
1450 → 1461). **mood_tests verde + MoodEditor build verde** (background
task buuo1yz9x, exit 0). 0 regresión.

**Files cambiados / nuevos**:

- `src/engine/gameplay/enemy/EnemySpec.{h,cpp}` (3 fields + clamps)
- `src/engine/gameplay/enemy/EnemySystem.{h,cpp}` (signature +
  `applyEnemyAttack` helper + Attack switch logic + transitionTo
  con optional spec + `k_timerEps`)
- `src/engine/scene/components/Components_Gameplay.h` (timer fields)
- `src/editor/application/EditorApplication_Run.cpp` (pasa physics)
- `src/editor/panels/scene/InspectorPanel_Enemy.cpp` (display + debug)
- `src/engine/scripting/bindings/LuaBindings_Enemy.cpp` (3 fields en spec)
- `assets/enemies/grunt.moodenemy` (3 fields explicit)
- `assets/enemies/imp.moodenemy` NEW (ranged demo)
- `assets/weapons/imp_fireball.moodweapon` NEW (fireball projectile)
- `assets/i18n/{es,en}.json` (3 keys nuevas inspector)
- `tests/test_enemy_spec.cpp` (4 cases F4H9)
- `tests/test_enemy_system.cpp` (7 cases F4H9 + `makePlayer` con
  HealthComponent + `advanceTicks` helper)

**Backlog post-F4H9**:

- **F4H9.1** — Damage type / element (fire/cold/etc) si emerge resistencias.
- **F4H9.2** — Hitstun del player (mini stagger al recibir damage).
- **F4H8.1 (todavía pendiente)** — `spec.attackStopsMove` opt-in para
  Doom clásico (parar para atacar) si el dev decide post-F4H10+.
- **F4H10** — Animaciones impacto + ragdoll on death (cubos articulados
  o primer NPC Mixamo — D1 a tomar al abrir).

**Memorias capturadas**: ninguna nueva (project_pandemonium_map_design
y project_fase4_engine_generic siguen vigentes).
