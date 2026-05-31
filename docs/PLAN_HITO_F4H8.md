# Plan F4H8 — Chase con A* + Attack tracking

> **Segundo hito de Sub-fase 4.2** "¿es divertido pelear?". F4H7 dejó la
> state machine armada con `Chase` y `Attack` como branches no-op. F4H8
> activa el movimiento real conectando `EnemySystem` con `NavAgent` +
> `NavSystem` (Hito 23 A*). El enemy persigue al player y mantiene
> tracking mientras lo golpea (Doom Eternal-style).

## Norte

Al cerrar F4H8 quiero poder: spawn un Enemy, entrar a Play, ver que el
enemy se mueve hacia mí cuando entra Alert, llega a attackRange y
sigue moviendose conmigo aunque esté en Attack (telegraph del ataque
seguirá no-op hasta F4H9 que trae el daño real). El movimiento es A*
puro grid — los enemies pueden amontonarse si varios persiguen al
mismo player (anti-clumping queda F4H8.1).

## Decisiones cerradas (AskUserQuestion)

- **D1 — Chase + Attack tracking en un solo hito.** El enemy se mueve
  en Chase Y en Attack. NavAgent queda `active=true` en ambos estados.
  **Razón**: feel Doom Eternal — el enemy "te marca" siempre, sin
  ventanas de descanso. Doom clásico (parar para atacar) queda F4H8.1
  opt-in si el dev decide después que prefiere el telegraph clásico.

- **D2 — A* puro sin anti-clumping.** Si 3 grunts persiguen al mismo
  player, los 3 van al mismo tile destino. Pueden quedar overlap.
  **Razón**: convención Doom/Serious Sam (hordas tontas). Anti-clumping
  (personal-space radius / flocking) queda F4H8.1 si emerge friccion
  visual en validacion con > 5 enemies simultaneos.

  **Nota arquitectónica capturada**: el dev expreso que NO quiere mapas
  planos tipo "mesa de ajedrez" — quiere niveles con zonas variadas y
  enemies en puntos diferentes. Eso es level design (F4H13), NO entra a
  F4H8. Memoria `project_pandemonium_map_design` capturada. El A*
  actual es 2D grid; si emerge demand de mapas con altura
  (rampas/plataformas), navmesh 3D queda backlog post-F4H13.

## Decisiones convencionales (no preguntadas)

- **D3 — EnemySystem maneja su propio NavAgent.** En Chase/Attack:
  ensure component + `nav.target = playerPos` + `nav.speed =
  spec.moveSpeed` + `nav.active = true`. En Idle/Alert/Pain/Dead:
  si tiene NavAgent → `nav.active = false`. **Razón**: el patron del
  bridge actual (forEach<NavAgentComponent> seteando target=playerPos
  para TODOS) es demasiado agresivo — futuros NavAgents non-enemy
  (companions, patrol NPCs, vehiculos AI) querran target distinto.
  EnemySystem es el único responsable de los NavAgents enemy.

- **D4 — Bridge wireup: skip enemies en el forEach genérico.** El
  forEach actual seguira corriendo para back-compat (Lua script futuro
  puede crear NavAgent standalone), pero saltea entities con
  `EnemyComponent` (EnemySystem las maneja). Comment migra de "futuro"
  a "F4H8: EnemySystem hace su propio target".

- **D5 — Alert→Chase transition inmediata.** F4H7 dejó Alert como
  estado estable. F4H8: si en Alert + `dist > attackRange` → transition
  a Chase. (Si dist ≤ attackRange → Attack directo, ya estaba en F4H7).
  El enemy NO se queda parado en Alert — al detectarte, persigue
  inmediatamente. Estilo Doom clasico.

- **D6 — `nav.speed = spec.moveSpeed` cada frame.** El spec puede
  cambiar (Inspector live edit, Lua script). Setting cada tick mantiene
  sync sin watcher. Costo: 1 float write per enemy per frame, irrelevante.

- **D7 — Auto-add NavAgentComponent on demand.** Al entrar Chase por
  primera vez, si no tiene NavAgent, se agrega. NO lo borramos al salir
  de Chase — solo lo desactivamos (`active=false`) para que NavSystem
  lo skipee. Razon: re-allocar componente cada Idle↔Chase transition es
  churn innecesario; la entity los mantiene hasta morir.

- **D8 — Dead removeOrSkip NavAgent.** Cuando el enemy muere, el
  Dynamic RB auto-added pelearía con NavAgent por el Transform (el
  comment de NavAgentComponent.h lo flagea explicito). Fix:
  `nav.active=false` al transicionar a Dead. El componente NO se
  borra (para no romper undo).

## Sub-tareas

### Sub-1 — Integrar NavAgent en `Enemy::tickSystem`
- En el switch de estados:
  - `Alert`: si `dist > attackRange` → transition a Chase (D5).
  - `Chase`: ensure NavAgent + target = playerPos + speed = spec.moveSpeed + active = true.
  - `Attack`: ensure NavAgent + target = playerPos + speed = spec.moveSpeed + active = true (D1 tracking).
  - `Idle/Alert/Pain`: si tiene NavAgent → active = false.
  - `Dead`: si tiene NavAgent → active = false (D8 — evita pelea con Dynamic RB).
- Helper inline `ensureNavAgent(reg, e, target, speed)`:
  - Si no tiene component → `reg.emplace<NavAgentComponent>(e, ...)`.
  - Sino, asigna target/speed/active=true.

### Sub-2 — Update bridge en `EditorApplication_Run`
- forEach<NavAgentComponent> agregar `if (e.hasComponent<EnemyComponent>()) return;` (D4).
- Comment update: "F4H7+: EnemySystem maneja NavAgents enemy. Aca quedan los standalone (futuros patrol/companion)".

### Sub-3 — Orden de tickSystems
- Verificar `Enemy::tickSystem` corre ANTES de `NavSystem::update` para que NavSystem agarre los targets actualizados. F4H7 ya pone Enemy entre Pickup y Projectile. NavSystem corre mas abajo. ✅ Orden OK.

### Sub-4 — Tests F4H8
- `tests/test_enemy_system.cpp` agregar:
  - "Chase ensure NavAgentComponent": enemy en Idle, player en aggro → tick → state Chase + tiene NavAgentComponent + active=true + target=playerPos + speed=spec.moveSpeed.
  - "Chase mantiene NavAgent target updated": player se mueve → 2do tick → target actualizado.
  - "Attack mantiene NavAgent active (tracking)": enemy a 1.5m de player → state Attack + nav.active==true (D1).
  - "Pain desactiva NavAgent": Pain trigger → nav.active==false.
  - "Idle desactiva NavAgent existente": enemy con NavAgent, player se aleja → Idle → nav.active==false.
  - "Dead desactiva NavAgent": Dead transition → nav.active==false (D8).
  - "Alert→Chase si dist > attackRange": enemy en Alert dentro aggro pero > 2m → Chase (D5).
  - "NO double-add NavAgent": Chase, Idle, Chase de nuevo → solo 1 NavAgent.

### Sub-5 — Cierre
- Build verde → suite verde.
- docs ESTADO_ACTUAL + HITOS + DECISIONS + Cierre en PLAN_HITO_F4H8.md.
- Commit + tag `v3.8.0-fase4-hito8` con "Chequear:" section.

## Backlog (NO entra a F4H8)

- **F4H8.1 (opt-in)** — Doom-clasico Attack stop (parar para atacar)
  como spec opcional `.moodenemy.attackStopsMove: bool` (default false =
  Doom Eternal tracking).
- **F4H8.2** — Anti-clumping si emerge friccion: personal-space radius
  o flocking simple.
- **F4H8.3** — Spec del `.moodenemy` pathfinding params: turnRate (que
  tan rapido cambia de direccion), repathHysteresisDist (cuando re-A*),
  acceleration (no instant top speed). Hoy todos default.
- Navmesh 3D — backlog post-F4H13 si los mapas con altura emergen.
- LoS raycast opt-in F4H7.1 sigue agendizado.
- Ataque que daña al player → F4H9.

## Riesgos

- **NavAgent vs Dynamic RB pelea de autoridad** — al morir el enemy
  agrega Dynamic RB; D8 mitiga via active=false. Hot path
  (HP→0 mientras NavSystem corre en este frame): NavSystem ya leyo
  active=true. Verificar que ese frame extra de movimiento post-Dead
  no rompa el ragdoll (test "Dead desactiva NavAgent" cubre el flag).
- **Auto-add NavAgent puede romper undo** — si el dev hace undo de
  la transition Chase, el NavAgent agregado queda huerfano. F4H8 NO
  pushea el add al HistoryStack (runtime spawn no es editable). El undo
  del Play mode no debería revertir transitions de state machine.
- **forEach genérico vs EnemySystem race** — orden de ejecucion del
  bridge es Pickup → Enemy → Projectile → ... → forEach NavAgent →
  NavSystem. EnemySystem setea target/speed primero; el forEach genérico
  con guard de EnemyComponent skipea. NavSystem agarra el estado final.

---

## Cierre — 2026-05-31 (tag `v3.8.0-fase4-hito8`)

**Suite full 1450/12353 verde** (+9 cases / +33 asserts vs F4H7: 1441 → 1450). 0 regresión.

### Entregables

1. **Helpers `activateNavAgent(reg, e, target, speed)` + `deactivateNavAgent(reg, e)`** inline en `EnemySystem.cpp` (D7 auto-add on demand, sin remove).
2. **State machine F4H8** (`EnemySystem.cpp`):
   - Alert (D5): `dist > attackRange` → Chase inmediato.
   - Chase + Attack: activate NavAgent target=playerPos + speed=spec.moveSpeed.
   - Idle/Alert/Pain: deactivate NavAgent.
   - Dead: deactivate ANTES de auto-add Dynamic RB (D8).
   - Pain → Chase (no Alert) si tiene target valido al recuperarse.
3. **R1 — Post-switch NavAgent side-effects block** (refactor reactive descubierto via tests):
   - Switch 1: evalúa transitions (state PRE).
   - Switch 2: aplica activate/deactivate basado en state POST-transition (mismo tick).
4. **Bridge `EditorApplication_Run`** (D4): `forEach<NavAgentComponent>` skipea entities con EnemyComponent.
5. **9 tests nuevos verdes** + 1 update (F4H7 "Pain → Alert" → F4H8 "Pain → Chase").

### Decisiones cerradas

- **D1** — Chase + Attack tracking continuo (Doom Eternal-style) vs Doom clásico parar para atacar (F4H8.1 opt-in).
- **D2** — A* puro sin anti-clumping (convención Doom/Serious Sam) vs personal-space radius (F4H8.2 si emerge fricción).
- **D3** — EnemySystem maneja propio NavAgent (no forEach genérico).
- **D4** — Bridge skip enemies en forEach (back-compat con futuros NavAgents standalone).
- **D5** — Alert→Chase inmediato (Alert deja de ser estado estable).
- **D6** — `nav.speed = spec.moveSpeed` cada frame (sync con live edits).
- **D7** — Auto-add NavAgent on-demand sin remove (solo desactivar).
- **D8** — Dead deactivate ANTES de Dynamic RB add (evita pelea Transform).

### Ajuste reactivo

- **R1** — Post-switch NavAgent side-effects block (separar transitions del switch principal del activate/deactivate side-effects en switch separado evaluando state POST-transition). Descubierto via tests `REQUIRE(hasComponent<NavAgentComponent>())` fallando tras 2 ticks Idle→Alert→Chase porque case Chase no se ejecutaba en mismo tick. Patron reusable para F4H9 cuando agregue applyDamage side-effects.

### Decisión out-of-scope capturada (memoria)

El dev expresó al responder D2 (verbatim): *"no quiero un mapa como una mesa de ajedrez, la idea no es que sea un mapa plano, sino que tambien esten en zonas alejadas en puntos diferentes, ni siquiera estoy hablando de un generador de mapas como el de unreal imaginate"*. NO entra a F4H8 (es level design F4H13). Memoria `project_pandemonium_map_design` guardada para que no se pierda: A* actual es 2D grid → navmesh 3D queda backlog post-F4H13 si emerge fricción con mapas multi-altura (rampas/plataformas).

### Próximo hito

**F4H9** — Ataques que dañan al player. El enemy en `Attack` state golpea al player aplicando `spec.damage` HP cada `spec.attackCooldown` segundos via `Health::applyDamage`. Sub-decisión con el dev: melee solamente vs melee + ranged proyectil data-driven (`spec.attackKind`).

