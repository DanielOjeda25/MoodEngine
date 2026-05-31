# Plan F4H7 — Enemigo básico con máquina de estados

> **Primer hito de Sub-fase 4.2** "¿es divertido pelear?". Arranca tras
> F4H6 (cierre Sub-fase 4.1 game feel). El enemigo es la otra mitad del
> loop de combate — el jugador dispara, el enemigo recibe, reacciona,
> muere. F4H7 entrega la INFRAESTRUCTURA del enemigo (state machine +
> data-driven via `.moodenemy` + integración con Health) sin movimiento
> ni ataque. F4H8 trae navegación A* + chase real; F4H9 trae ataques que
> dañan al jugador.

## Norte

Al cerrar F4H7 quiero poder: crear un enemigo via "Crear Entidad", soltarlo
en el mapa, entrar a Play, acercarme y verlo cambiar de estado (Idle →
Alert) cuando entro en su radio, dispararle, ver el flash del Pain, seguir
disparando hasta matarlo (estado Dead + cae con física). El enemigo
sigue siendo cubo placeholder — el visual pass es Sub-fase 4.3.

Engine-generic: el `.moodenemy` puede ser un grunt de PANDEMONIUM o un
NPC de otro juego. Nada hardcodeado.

## Decisiones cerradas (AskUserQuestion)

- **D1 — Esqueleto completo de 6 estados desde F4H7.** Implemento los 6
  estados (`Idle/Alert/Chase/Attack/Pain/Dead`) aunque `Chase` y `Attack`
  queden no-op hasta F4H8/F4H9. **Razón**: F4H8 no toca la state machine,
  solo activa el branch de movimiento dentro de `Chase`. Si stripeo a
  Idle/Pain/Dead y agrego estados después, F4H8 refactoriza la máquina
  (cambio de signature de los callbacks de transición). Mejor armar la
  estructura completa ahora.

- **D2 — Detección solo por proximidad (esfera de aggro).** Sin
  line-of-sight raycast. El enemigo te detecta si entrás a `aggroRange`
  metros (configurable per-`.moodenemy`, default 12m). No le importa si
  hay pared en el medio. **Razón**: convención Doom/Serious Sam — hordas
  tontas pero divertidas. LoS opt-in queda agendizado a F4H7.1 si emerge
  demand (sigilo o niveles con pasillos cerrados).

## Decisiones convencionales (no preguntadas)

- **D3 — `.moodenemy` paralelo a `.moodweapon`.** Mismo patrón de asset
  data-driven establecido en F4H2. `EnemySpec` con
  `{health, aggroRange, attackRange, moveSpeed, damage, attackCooldown,
  painThreshold, painDuration, viewmodelMesh, hitSound, deathSound,
  displayName}`. AssetManager `loadEnemy/getEnemy/enemyPathOf` +
  scan de `assets/enemies/*.moodenemy`. JSON roundtrip + clamps.

- **D4 — `EnemyComponent` engine-generic.**
  `{enemyAssetId u32, state EnemyState, stateTime f32, targetEntity u32,
  lastAttackTime f32, painsTotal int}`. Sin `EnemyKind` enum
  (PANDEMONIUM-specific) — el "tipo" sale del asset. Sin lógica
  acoplada a viewmodel/mesh — separación clara.

- **D5 — Pain trigger via polling de `HealthComponent.hitFlashTimer`.**
  Mismo pattern R4 F4H4 — `Enemy::tickSystem` polla el `hitFlashTimer`
  del frame anterior; si subió → entra estado `Pain` (con
  `pain_state_t` cooldown configurable, default 0.3s). Sale a `Alert`
  si tenía target, sino `Idle`. **Razón**: mantiene `engine/gameplay/Health.cpp`
  decoupled de `engine/gameplay/enemy/`. No callbacks.

- **D6 — Dead state transition auto-add `RigidBodyComponent::Dynamic`.**
  Mismo pattern F4H1 `Health::tickSystem` — cuando state pasa a `Dead`,
  `EnemySystem` agrega `RigidBody Dynamic` si el enemy no lo tenía
  (back-compat con enemies pre-existentes Static). Cae con física.

- **D7 — `handleAddEnemy` via Crear Entidad → Gameplay → "Enemigo (cubo)".**
  Mismo pattern F4H1 `AddDummy`. Spawn Tag="Enemy_<N>" + Transform +
  MeshRenderer cubo + HealthComponent (default Project Settings >
  Gameplay > maxHealthDefault) + EnemyComponent con primer `.moodenemy`
  alfa del catálogo (auto-rescan filesystem, mismo pattern F4H3
  `handleAddPlayer`). Sin RigidBody — Static por default; cae al morir
  via D6.

- **D8 — InspectorPanel_Enemy en categoría Gameplay.** Combo `.moodenemy`
  + Spec read-only + display runtime (state actual + stateTime + target
  tag + lastAttackTime). Dropdown debug para forzar transition state
  (testing). Reset buttons en sliders runtime.

- **D9 — Demo `grunt.moodenemy` en `assets/enemies/`.** Engine ejemplo
  data-driven (no hardcoded). HP=50, aggro=12m, attackRange=2m,
  moveSpeed=4, damage=15, attackCooldown=1.0s, painThreshold=10dmg,
  painDuration=0.3s. Sin viewmodel mesh (placeholder cubo).

- **D10 — Lua bindings `enemy.*`.** `enemy.get_state(tag) →
  "idle"|"alert"|"chase"|"attack"|"pain"|"dead"`, `enemy.set_state(tag, state)`
  (debug), `enemy.kill(tag)` (apply daño masivo via Health),
  `enemy.spec(tag) → {aggro_range, attack_range, ...}`. Mirror del
  pattern `weapon.*` / `health.*`.

## Sub-tareas

### Sub-1 — `EnemyComponent` + `EnemyState` enum (engine-generic)
- `src/engine/scene/components/Components_Gameplay.h`:
  - `enum class EnemyState { Idle, Alert, Chase, Attack, Pain, Dead }` (6 estados).
  - `struct EnemyComponent { u32 enemyAssetId = 0; EnemyState state = Idle;
    f32 stateTime = 0.0f; u32 targetEntity = 0; f32 lastAttackTime = 0.0f;
    int painsTotal = 0; }`. Engine-generic, raw `u32` for entities (forward-decl-friendly, mismo
    pattern `TriggerComponent.bodiesInside` / `ProjectileComponent.owner`).

### Sub-2 — `.moodenemy` asset type + `EnemySpec`
- `src/engine/gameplay/enemy/EnemySpec.{h,cpp}` (~180 LOC):
  - `struct EnemySpec { std::string displayName; f32 health, aggroRange, attackRange,
    moveSpeed, damage, attackCooldown, painThreshold, painDuration;
    std::string viewmodelMesh, hitSound, deathSound; }`.
  - `static EnemySpec defaults()` con HP=50, aggro=12m, attack=2m, speed=4, dmg=15,
    cooldown=1.0s, painThr=10dmg, painDur=0.3s.
  - `nlohmann::json toJson() const` + `static EnemySpec fromJson(const json&)`.
  - Clamps: HP/aggro/attackRange >= 0.01; moveSpeed [0, 50]; damage >= 0;
    attackCooldown >= 0.05; painThreshold >= 0; painDuration >= 0.05.
  - `bool saveToFile(const std::filesystem::path&) const` + `loadFromFile(...)`.
- `src/engine/assets/AssetManager.{h,cpp}`:
  - `u32 loadEnemy(const std::string& logicalPath)` (mismo patrón loadWeapon).
  - `const EnemySpec& getEnemy(u32 id) const` + `enemyPathOf(u32)` + `missingEnemyId()`.
  - `std::vector<EnemyEntry> enumerateEnemies(bool rescanFromDisk = false)`.
  - Slot 0 fallback con `EnemySpec::defaults()`.

### Sub-3 — `EnemySystem` (state machine engine-generic)
- `src/engine/gameplay/enemy/EnemySystem.{h,cpp}` (~280 LOC):
  - `void Enemy::tickSystem(Scene& scene, f32 dt, Entity playerEntity,
    AudioDevice* audio, const AssetManager& assets)`.
  - Por cada `EnemyComponent`:
    - `if (state == Dead) skip` (already done — `Dead` is terminal).
    - Resolver `playerPos` desde `playerEntity.Transform`.
    - Resolver `enemyPos` + `enemyHealth`.
    - Calcular `dist = horizontalDistance(playerPos, enemyPos)`.
    - **Pain trigger polling**: si `enemyHealth.hitFlashTimer > prev_hitFlashTimer`
      → transition to `Pain` state (reset `stateTime=0`). El frame anterior se
      guarda en field nuevo `EnemyComponent.prevHitFlashTimer` (similar al
      `m_f4h4_prevHitFlashTimer` del bridge F4H4, pero per-enemy).
    - **State machine** (switch):
      - `Idle`: si `dist <= spec.aggroRange` → `Alert` (target = player).
      - `Alert`: F4H8 trae transición a `Chase`. F4H7 queda en Alert estable.
        Log `[enemy] X entered Alert (dist=N)` una vez al transicionar.
      - `Chase`: F4H8 implementa movimiento A*. F4H7 no-op (no se mueve).
        Si `dist <= attackRange` → `Attack`.
      - `Attack`: F4H9 implementa daño al player. F4H7 cooldown timer
        no-op; vuelve a `Alert`/`Chase` cuando termina.
      - `Pain`: cooldown `spec.painDuration` → vuelve a `Alert` si tenía
        target, sino `Idle`.
      - `Dead`: terminal, ignore.
    - **Dead transition**: si `health.dead && state != Dead` →
      `state = Dead`, `stateTime = 0`, log `[enemy] X died`. Auto-add
      `RigidBodyComponent::Dynamic` si no lo tenía (mismo pattern
      Health F4H1).
    - Incrementar `stateTime += dt`.

### Sub-4 — Health integration polish
- Health::applyDamage NO se toca (engine-generic, no acopla a Enemy).
- El polling de `hitFlashTimer` en Sub-3 cubre el Pain trigger sin
  ensuciar Health.
- `prevHitFlashTimer` field nuevo en EnemyComponent (no transient — se
  trackea per-frame, no se serializa).

### Sub-5 — Serialization + Inspector
- `src/engine/scene/serialization/SceneSerializer.h`:
  - `struct SavedEnemy { std::string enemyPath; std::string state; }`
    (no `stateTime`/`targetEntity`/`lastAttackTime`/`painsTotal` — todos
    transients en runtime).
  - Optional `std::optional<SavedEnemy> enemy` en `SavedEntity`.
- `EntitySerializer.cpp::writeEnemy` + `EntitySerializer_Parse.cpp`
  parsers (mismo patrón writeWeapon/writeHealth).
- `SceneLoader.cpp` aplica `SavedEnemy` cargando spec + restoring state
  desde string.
- `src/editor/panels/scene/InspectorPanel_Enemy.cpp` (~120 LOC):
  - Combo `.moodenemy` enumerando catálogo + tooltip logicalPath.
  - Spec read-only display (HP / aggroRange / attackRange / moveSpeed /
    damage / attackCooldown / painThreshold / painDuration).
  - Runtime debug: dropdown EnemyState force-set + `stateTime` read-only +
    target tag read-only + lastAttackTime read-only + painsTotal.
- `InspectorPanel.cpp`:
  - Sección Enemy en categoría Gameplay.
  - Entry en `Add Component` menu (Logic): Enemy.

### Sub-6 — Crear Entidad + demo
- `EditorProjectActions_CreateEntity.cpp::handleAddEnemy`:
  - Pick modal tab "Gameplay" → "Enemigo (cubo)".
  - Spawn entity Tag=`"Enemy_<N>"` + Transform + MeshRenderer cubo +
    HealthComponent (default Project Settings) + EnemyComponent con
    primer `.moodenemy` alfa del catálogo (auto-rescan filesystem).
  - Si catálogo vacío → spec defaults inline (no-fail).
- `assets/enemies/grunt.moodenemy`:
  - `displayName="Grunt"`, HP=50, aggro=12, attack=2, speed=4, dmg=15,
    cooldown=1.0, painThr=10, painDur=0.3, mesh="" (placeholder cubo),
    sounds="".
- i18n keys `editor.panel.inspector.enemy.*` (es+en) + entry "Enemigo"
  en pick modal.

### Sub-7 — Tests
- `tests/test_enemy_spec.cpp` (~10 cases):
  - defaults / roundtrip grunt / sin bloques → defaults / clamps
    HP+aggro+attack+speed+damage+cooldown+pain / saveToFile-loadFromFile.
- `tests/test_enemy_system.cpp` (~13 cases):
  - Estado inicial Idle / transition Idle→Alert al entrar aggroRange /
    no transition fuera de aggro / transition Pain on damage hit / Pain
    cooldown vuelve a Alert con target / Pain cooldown vuelve a Idle sin
    target / Dead state terminal / Dead auto-add RigidBody Dynamic / Dead
    no-double-add / state machine no crash sin player / state machine no
    crash con player sin Transform.

### Sub-8 — Wireup + Lua + close
- `EditorApplication_Run::tickSystems`: `Enemy::tickSystem(scene, dt,
  playerEntity, m_audioDevice.get(), *m_assetManager)` después de
  `Pickup::tickSystem`, antes de `Projectile::tickSystem` (orden:
  pickups → enemy AI → projectiles vuelan).
- `src/engine/scripting/bindings/LuaBindings_Enemy.cpp` (~120 LOC):
  - `enemy.get_state(tag) → string` (lowercase).
  - `enemy.set_state(tag, state)` (debug, accept lowercase string).
  - `enemy.kill(tag)` (apply 99999 dmg via Health::applyDamage).
  - `enemy.spec(tag) → {display_name, health, aggro_range, ...}`.
- CMakeLists.txt + tests/CMakeLists.txt updates.
- Build verde → suite verde.
- docs ESTADO_ACTUAL + HITOS + DECISIONS + Cierre en PLAN_HITO_F4H7.md.
- Commit + tag `v3.7.0-fase4-hito7` con "Chequear:" section.

## Backlog (NO entra a F4H7)

- **F4H7.1** — Line-of-sight raycast opt-in (`spec.requireLineOfSight: bool`).
- **F4H8** — Movimiento Chase con A* (el motor ya lo tiene desde Hito 23).
- **F4H9** — Ataques melee + ranged que dañan al player.
- **F4H10** — Animaciones de impacto (Mixamo) + ragdoll on death.
- **F4H11** — Variedad: 2-3 tipos `.moodenemy` (rusher / tirador / tank).
- **F4H12** — Spawner zones + waves data-driven (`.moodwave`).
- EnemyKind enum (categorización tactical) — solo si emerge demand
  desde encounter design.

## Riesgos

- **Sub-fase 4.2 entera con cubos placeholder** (strategic deferral del
  usuario en cierre F4H6) — el dev confía en que el visual pass de
  Sub-fase 4.3 con NPC Mixamo + ragdoll + animaciones impacto va a
  cerrar la sensación. Validar con encounter pequeño (3-5 enemies) al
  cierre de F4H7 para confirmar que la state machine se siente bien
  visualmente aunque sea cubo.
- **EnemyComponent forward-decl `u32 targetEntity`** — preservar
  convención de Components_Gameplay.h (no `entt::entity` por
  forward-decl-friendly, mismo patrón F4H5 `ProjectileComponent.owner`).
- **`prevHitFlashTimer` per-enemy** — incrementa size de EnemyComponent.
  Mitigación: float = 4 bytes, irrelevante para hordas <100 enemies.

---

## Cierre — 2026-05-31 (tag `v3.7.0-fase4-hito7`)

**Suite full 1441/12320 verde** (+23 cases / +80 asserts vs F4H6: 1418 → 1441). 0 regresión.

### Entregables

1. **`EnemyComponent` + `EnemyState` enum** (`Components_Gameplay.h`) — 6 estados (Idle/Alert/Chase/Attack/Pain/Dead) + `k_noTarget=0xFFFFFFFFu` sentinela + transients (stateTime, targetEntity, lastAttackTime, painsTotal, prevHitFlashTimer).
2. **`EnemySpec` + `.moodenemy` asset** (`engine/gameplay/enemy/EnemySpec.{h,cpp}` ~180 LOC) — schema JSON + roundtrip + clamps.
3. **`AssetManager_Enemy.cpp`** (~110 LOC) — loadEnemy/getEnemy/enemyPathOf/missingEnemyId/enumerateEnemies + slot 0 fallback + sentinela `__empty_enemy`.
4. **`EnemySystem::tickSystem`** (`engine/gameplay/enemy/EnemySystem.{h,cpp}` ~210 LOC) — state machine + Pain polling + Dead auto-add RB Dynamic.
5. **Serialization** `SavedEnemy { enemyPath, state }` en SceneSerializer.h + writeEnemy/parseEnemy + SceneLoader.cpp aplica al cargar.
6. **`InspectorPanel_Enemy`** (~170 LOC) — combo `.moodenemy` + Spec read-only + runtime debug.
7. **Add Component menu** entrada "Enemy" en Logic (InspectorPanel.cpp).
8. **`handleAddEnemy`** via Crear Entidad → Gameplay → "Enemigo (cubo)" + `ICON_FA_SKULL` (EditorProjectActions_CreateEntity.cpp + ProjectAction::AddEnemy + dispatch en EditorApplication_Run.cpp).
9. **`grunt.moodenemy`** demo en `assets/enemies/`.
10. **i18n** 24 keys nuevas (es+en): pick modal + Inspector sections + component name/desc.
11. **Lua bindings `enemy.*`** (`LuaBindings_Enemy.cpp` ~110 LOC): get_state / set_state / kill / spec.
12. **Wireup** `EditorApplication_Run::tickSystems` — `Enemy::tickSystem` despues de Pickup, antes de Projectile + MOOD_PROFILE_SCOPE.
13. **23 tests verdes** en `test_enemy_spec.cpp` (9) + `test_enemy_system.cpp` (14).

### Decisiones cerradas

- **D1** — Esqueleto completo 6 estados desde F4H7 (vs minimo Idle/Pain/Dead — F4H8/F4H9 activan branches sin refactor).
- **D2** — Detección solo por proximidad (esfera de aggro, vs LoS raycast — convencion Doom/Serious Sam, F4H7.1 si emerge demand).
- **D3** — `.moodenemy` paralelo `.moodweapon` data-driven engine-generic.
- **D4** — `EnemyComponent` sin enum `EnemyKind` PANDEMONIUM-specific.
- **D5** — Pain trigger via polling `hitFlashTimer` (vs callbacks — mantiene Health decoupled).
- **D6** — Dead auto-add RigidBody Dynamic (mismo patron F4H1 Health).
- **D7** — `handleAddEnemy` via Crear Entidad (mismo patron AddDummy).
- **D8** — InspectorPanel_Enemy combo + Spec + debug runtime.
- **D9** — `grunt.moodenemy` demo (50 HP, 12m aggro, 15 dmg).
- **D10** — Lua bindings `enemy.*` mirror del pattern `weapon.*`/`health.*`.

### Ajustes reactivos

- **R1** — `k_noTarget = 0xFFFFFFFFu` sentinela porque EnTT handle 0 es valido (descubierto via tests "Idle→Alert" y "Pain → Alert con target").
- **R2** — Dead transition auto-add RB Dynamic en MISMO tick (descubierto via test "Dead auto-add RigidBody"; sin esto el RB aparecia recien al segundo tick post-mortem).
- **R3** — Test "enemy sin Transform" eliminado y reemplazado por "multiples enemies independent state machines" — `Scene::createEntity` siempre agrega TransformComponent; guard defensivo nunca dispara.

### Bug build-time

- **B1** — `StubTexture` API drift en test_enemy_system.cpp (usaba `mipLevels/rendererHandle` no existentes en ITexture). Fix via copy del stub de test_weapon_system.cpp con signatures correctas.

### Próximo hito

**F4H8** — Navegacion del enemigo. Integrar `NavAgentComponent` (Hito 23 A*) con `EnemyComponent.state == Chase`. El enemy persigue al player con A* sobre el GridMap; transition a Attack si entra `attackRange` (sigue no-op hasta F4H9). Activa branch ya armado en F4H7 — sin refactor de state machine. Logic-only, cubos.

