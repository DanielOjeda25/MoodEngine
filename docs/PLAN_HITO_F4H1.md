# PLAN F4H1 — Sistema de salud/daño (cimiento del combate)

**Estado:** 📝 EN CURSO.
**Predecesor:** F3H31 (Sub-fase 3.4 cerrada) + tag `v3.0.0` (Fase 3 CERRADA).
**Sub-fase:** 4.1 — Núcleo de combate (¿se siente bien disparar?).
**Origen:** §6 del [`PLAN_FASE4.md`](PLAN_FASE4.md) — cimiento del combate antes de armas (F4H2) y enemigos (F4H6).

---

## Norte

Sin sistema de salud, no hay daño. Sin daño, no hay combate. Sin combate, no hay shooter. F4H1 entrega la primitiva más chica que habilita el resto: una entidad con vida que recibe daño y "muere". El maniquí de testing es el primer feedback satisfactorio del proyecto Fase 4 — un cubo que reacciona cuando le pegás.

NO se construyen armas ni enemigos en F4H1. Solo el sistema de salud y la forma de aplicarle daño desde código/Lua/consola.

---

## Mecánicas cerradas (vista del dev)

1. **Spawnear maniquí** — `Menú "+ Crear Entidad" > Maniquí`. Aparece un cubo con `HealthComponent` (vida=100/100), `RigidBody` Kinematic (no se cae mientras vive), `MeshRenderer` con material default.
2. **Aplicar daño** — desde la consola Lua del editor en Play mode: `health.damage(entityId, 25)`. La vida baja, el cubo hace **flash blanco breve (~80ms)** encima de su color base.
3. **Heal** — `health.heal(entityId, 50)` sube la vida (clamp a max).
4. **Get** — `health.get(entityId)` devuelve `{ current, max, dead }`.
5. **Muerte** — al llegar a 0: `dead=true`, se cambia a `RigidBody Dynamic` (gana gravedad) → **cae con física**. El flash blanco final dura más largo (~250ms) para marcar el momento.
6. **Inspector** — sección Health en categoría Object con `current/max` sliders + checkbox `dead` (debug, no-op si ya está muerto) + reset.
7. **Defaults editables sin recompilar** — `Project Settings > Gameplay > Salud máxima por defecto (100)`. Convención Fase 3 "nada hardcodeado".

---

## Decisiones cerradas (vía AskUserQuestion)

**D1 — Feedback visual al recibir daño: flash blanco breve (~80ms).** Convención Half-Life / Quake / Doom — el jugador ve impacto contundente per-hit. Alternativas descartadas: (a) color shift gradual a rojo según vida; (b) ambos. **F4H1 backend-only**: el `HealthComponent.hitFlashTimer` se setea correctamente (0.08s on-hit, 0.25s al morir) y el sistema lo decae cada tick — el shader render del flash queda **agendizado al backlog** (require edit de `pbr.frag` + uniform per-draw). Mientras tanto el feedback del daño vive en Console log + Inspector (current baja) + cae con física al morir. Implementable cuando emerja necesidad de polish visual (F4H5 game feel pass).

**D2 — Muerte: cae con física.** Plan F4H1 lo sugiere explícito. Convención FPS arcade. Implementación: al transicionar `dead=false → true`, sistema convierte el RigidBody a Dynamic (Jolt re-creates body) + impulso vertical mínimo para marcar el momento. Alternativas descartadas: (a) desactivar invisible (sin feedback, anti-satisfacción); (b) fijo + gris (útil para testing pero rompe el feel — agendizable a opción debug por preferences si emerge demanda); ragdoll real queda agendizado a F4H9 (enemigos), F4H1 es solo cubos.

**D3 — Spawn: Item "Maniquí" en modal "+ Crear Entidad".** Convención del editor existente (modal pick con TabBar). Tab "Primitivas" o tab nuevo "Gameplay" para futuras (enemigo de prueba, pickup). Alternativas descartadas: (a) botón temporal en menu Debug (se vuelve scaffolding muerto en F4H2); (b) solo Lua (más fricción para iterar el feel).

**D4 — Defaults en `Project Settings > Gameplay`, NO hardcoded.** Convención "nada hardcodeado" de Fase 3 explícita en el plan. `GameplaySettings` ya existe (F3H4 con walk/crouch/jump); F4H1 agrega `maxHealthDefault = 100.0f` + sanitize clamp `[1, 10000]`. Inspector del HealthComponent muestra el default cuando current==max==default (helper `resetButton` template de F3H4). Tab Gameplay del Project Settings panel gana 1 slider nuevo + tooltip i18n + reset.

---

## Implementación (5 phases)

### Phase 1 — `HealthComponent` + Project Settings default

**Archivos:**
- `src/engine/scene/components/Components_Gameplay.h`: agregar `struct HealthComponent { f32 current=100.0f; f32 max=100.0f; bool dead=false; f32 lastDamageTime=-1.0f; f32 hitFlashTimer=0.0f; }`.
- `src/engine/project/ProjectSettings.h`: extender `GameplaySettings` con `f32 maxHealthDefault = 100.0f`. Sanitize clamp `[1, 10000]` en `fromJson`. Persistir solo si distinto del default (back-compat).
- `src/editor/panels/project/ProjectSettingsPanel.cpp`: tab Gameplay gana 1 slider "Salud máxima por defecto" + reset button + tooltip i18n.

**Tests:** roundtrip toJson/fromJson + clamp en `test_project_settings.cpp`.

### Phase 2 — `applyDamage` API + flash on-hit + cae al morir

**Archivos:**
- `src/engine/gameplay/Health.{h,cpp}` NEW (~120 LOC):
  - `void applyDamage(Scene&, Entity target, f32 amount, glm::vec3 dir = {0,1,0})`.
  - `void heal(Scene&, Entity target, f32 amount)`.
  - Resta `current`, clampea a 0, marca `dead=true` la primera vez, setea `hitFlashTimer = (dead ? 0.25f : 0.08f)`, lastDamageTime = scene time.
  - Log al engine channel: `[health] entity '<tag>' damage=10 (current=90/100)` + `[health] entity '<tag>' MUERTO`.
- `src/engine/gameplay/HealthSystem.{h,cpp}` NEW (~80 LOC):
  - `update(Scene&, float dt)`: decae `hitFlashTimer` a 0; al transicionar `dead=false → true`, marca RigidBody Dynamic (gana gravedad) + impulso `(0, 2, 0)` para liftoff visual.
  - Llamado desde `EditorPlayMode::tick` (Play mode only — Editor mode no apply damage runtime).
- Shader `assets/shaders/pbr.frag`: agregar `uniform float uHitFlash` (default 0), mix final `outColor = mix(outColor, vec3(1), uHitFlash)`.
- `src/engine/render/scene_renderer/SceneRenderer_Render.cpp`: por entidad con `HealthComponent`, upload `uHitFlash = clamp(hitFlashTimer / 0.08f, 0, 1)`.

**Tests:** `test_health.cpp` NEW — applyDamage normal/clamp/muerte una vez/heal/respeta `max`/transition dead trigger flash 0.25s.

### Phase 3 — Inspector + serialización

**Archivos:**
- `src/editor/panels/scene/InspectorPanel_Health.cpp` NEW (~80 LOC):
  - Sección con SliderFloat current/max + checkbox dead (debug) + reset to `Project Settings > Gameplay > maxHealthDefault` + tooltip i18n.
  - Categoría Object del Inspector (chasis F3H22).
- `src/engine/scene/serialization/EntitySerializer.cpp` + `_Parse.cpp` + `SavedEntity` (`SceneSerializer.h`): persist `current/max/dead` (lastDamageTime/hitFlashTimer son transients).
- `SceneLoader.cpp`: applySavedEntity → si saved.health.has_value() add HealthComponent con valores.

**Tests:** roundtrip serialización en `test_health.cpp` (cubre defaults + valores no-default).

### Phase 4 — Lua bindings

**Archivos:**
- `src/engine/scripting/bindings/LuaBindings_Health.cpp` NEW (~60 LOC):
  - `health.damage(entityId, amount)`, `health.heal(entityId, amount)`, `health.get(entityId)` (devuelve table `{current, max, dead}`), `health.is_alive(entityId)`.
- `LuaBindings.cpp` registra la tabla.
- Smoke test en script Lua de demo: `scripts/test_health.lua` (no-op runtime, sirve como ejemplo + auto-validate al cargar).

### Phase 5 — Pick modal Maniquí + tests

**Archivos:**
- `EditorProjectActions_CreateEntity.cpp`: nuevo handler `spawnDummy(...)` que crea Entity con TagComponent="Dummy_<N>" + TransformComponent + MeshRendererComponent (cubo default) + HealthComponent (defaults Project Settings) + RigidBodyComponent Kinematic.
- `EditorProjectActions_CreateEntity_PickModal.cpp`: agregar tab "Gameplay" con item "Maniquí" (icon `ICON_FA_BULLSEYE` o similar). Click → `spawnDummy(pickPos)` + push history + select.
- `assets/i18n/{es,en}.json`: nuevas keys (~12).

**Tests:** ya cubiertos en phases 1-3 (no test de spawn — UI puro).

---

## Lo que NO toca F4H1

- Armas / hitscan / proyectiles → F4H2 (hitscan), F4H4 (proyectiles).
- HUD de combate (salud del jugador on-screen) → F4H3.
- Headshots / damage zones → F4H2+ si emerge demanda; agendizable a F4H5.
- Ragdoll al morir → F4H9 (cubo cae con física simple es suficiente para F4H1).
- Sistema de eventos (event bus de "OnDamage"/"OnDeath") → no es necesario para F4H1, el log + el flag `dead` cubren. Si emerge demanda (callbacks Lua), agregable a F4H3+.
- Enemigos / IA → F4H6+.
- Sonido de impacto → F4H5 (game feel pass).

---

## Backlog post-F4H1

- Event bus OnDamage/OnDeath para callbacks Lua per-entity.
- Sound on damage / sound on death (F4H5 game feel pass).
- Headshot multiplier (F4H2+ si la query de raycast retorna parte del cuerpo).
- Pain animation hook (F4H6+ cuando enemigos tengan animator).
- Resistencias por tipo de daño (fuego, eléctrico, etc.) — solo si el diseño del juego lo pide.
