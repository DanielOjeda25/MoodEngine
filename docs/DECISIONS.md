# Log de decisiones técnicas — Fase 2

Registro cronológico de decisiones arquitectónicas no triviales tomadas
durante la **Fase 2** (F2H1 en adelante). Formato por entrada: contexto,
decisión, razones, alternativas descartadas, condiciones de revisión.

> **Decisiones de Fase 1 (Hito 0 → Hito 42)** archivadas en
> [`archive/DECISIONS_Fase1.md`](archive/DECISIONS_Fase1.md). Split
> aplicado en F2H26 cierre (2026-05-08) cuando este documento superó
> las 4000 líneas.

---

## 2026-05-31: F4H6 cierre — Game feel pass: muzzle flash + hit marker + screen shake + crosshair dinámico + pain reaction + tracer

**Contexto.** Cierre de Sub-fase 4.1 "¿se siente bien disparar?". F4H1-F4H5 dieron el combate **funcional**: salud, armas hitscan, swap multi-slot + viewmodel, HUD + pickups, proyectiles con splash. Pero al validar visualmente el dev reportó verbatim: *"sabes que pasa con el tema de daño, y armas es que todo es invisible no veo nada ni ningun diferencia entre ninguno, creo hasta el dummy o enemigo es un cubo no se porque no es un npc de los que tenemos con ragdoll, porque no bajamos animaciones de impacto o algo asi, y vamos mejorando"*. El combate funciona pero no se siente. F4H6 convierte combate funcional → combate que SE SIENTE.

**Decisiones de scope (AskUserQuestion al dev)**:

- **D1 — Bundle de 6 feedback layers en un solo hito.** Hito atómico cubre los 6 efectos del cierre de Sub-fase 4.1: (1) muzzle flash; (2) hit marker; (3) screen shake; (4) crosshair dinámico con spread; (5) pain reaction; (6) tracer del proyectil. Descartado *solo recoil* (M1/COD-style — único efecto cierra Sub-fase 4.1 a medias); descartado *solo shake* (Doom-Eternal-style — falta feedback de hit/spread/pain). El cierre de Sub-fase requiere el paquete completo. **Razón**: si el dev no siente diferencia entre las armas tras F4H6, el problema NO es game feel sino visual pass (Sub-fase 4.3). Mejor validar todos los efectos juntos para diagnosticar correctamente.

- **D2 — Intensidad HL/COD sutil.** Cada efecto va al **80% inferior** del rango de la industria. Shake amplitudes [0.015, 0.2] (mu, ground truth Quake III es 0.1; Doom Eternal 0.4-0.7). Pitch pain 2° (Quake 5°; Doom Eternal 8°). Crosshair gap expand max 16px (CS:GO max 30; Apex max 25). Descartado *Doom-Eternal-style screen-wide shake* (overkill primer hito de feel) y *Quake hyper-arcade* (descontrola FOV percepción). **Razón estratégica**: primero validar suavidad — el dev puede pedir +50% intensidad fácilmente con sliders en Project Settings cuando confirme que el feel se siente correcto. Bajar intensidad ya validada es más costoso emocionalmente que subirla.

**Decisiones técnicas (convención agente, no preguntadas)**:

- **D3 — Procedural-only sin assets art.** F4H6 entrega los 6 efectos con: `ParticleBurst` ya existente (F4H2 spawnImpactBurst pattern) + cosenoidal/exponencial decay + xorshift32 randomness + state lifecycle en `HudState`. Cero texturas, cero sonidos, cero mesh assets nuevos. **Razón**: el strategic deferral del usuario (verbatim) *"antes de lo visual falta algo mas en el sistema? cuando tengamos toda la logica implementada ahi podemos ver lo visual"* defiere TODO el visual pass (texturas muzzle/impact + decals + viewmodel meshes + ragdoll NPC Mixamo + animaciones impacto) a Sub-fase 4.3 unificada DESPUÉS de Sub-fase 4.2 (enemies F4H7-F4H12 all logic con cubos). F4H6 entrega la **infraestructura procedural completa** del feel; el dev valida que la lógica está bien aunque visualmente sigan siendo cubos.

- **D4 — Polling vs callbacks en bridge (R4 F4H4 preservado).** El bridge `EditorApplication_Run::tickSystems` POLLA state del frame anterior para detectar transitions (hitFlashTimer subió → disparar pain reaction; activeSlot cambió → arsenal overlay; targetsHit > 0 → hit marker). NO emite callbacks engine→game. **Razón**: mantiene `engine/gameplay/Health.cpp` y `engine/gameplay/projectile/ProjectileSystem.cpp` desacoplados de `engine/game/state/GameState.h`. Si emitiera callbacks rompería layering — `gameplay/` NO puede incluir `game/state/`. Polling permite que F4H6 wirree los efectos sin tocar la signature de funciones gameplay puras.

- **D5 — Eje shake aleatorio per-frame via xorshift32 deterministic.** El shake no es solo `sin(t * freq) * amp` en una sola dirección — eso parece "vibración de vibrador". Se necesita rotación de eje per-frame para parecer impacto. Implementación: `static u32 shakeRoll = 0xD3B2F1A7u` xorshift32 → `vec3 axis = normalize(vec3(...))` y `shakeOffset = axis * sin(t * 80) * amp * (t / max_t)`. **Razón vs `rand()`**: thread-safety (game loop puede tickear en thread aparte futuro) + reproducibilidad para tests (test_game_feel_f4h6 con seed conocida puede validar offset shape). Mismo patrón aplicado en `triggerPainReaction.pain_roll_offset` para roll determinismo.

- **D6 — Tracer preset por heuristic de `displayName` (no field schema).** En `Weapon::fire` projectile branch, después de spawnear el entity, se busca `spec.displayName` y se aplica preset heuristic: contains("rocket") → gris denso humo; contains("plasma") → cian aditivo; contains("grenade") → naranja con gravity sparks. **Razón vs field schema explícito `projectile.tracer { color, density }`**: no requiere migration de `.moodweapon` existentes (`rocket.moodweapon`, `plasma.moodweapon`, `grenade.moodweapon` ya creados en F4H5). Field opt-in queda agendizado a F4H6.1 si el dev quiere armas custom con tracer custom (e.g. mod del juego con "freeze gun" → cyan slow).

**Ajustes reactivos durante implementación**:

- **R1 — `triggerCameraShake` anti-spam timer > 50ms.** Versión inicial sobrescribía `shake_amp/shake_t` siempre. Test reactive durante validación con granadas multi-bounce (3-4 explosiones en 0.5s) generaba stutter visual feo — cada bounce dispara shake nuevo que cortaba el decay anterior, parecía bug. Fix: si shake actualmente activo + nuevo shake más débil + timer remaining > 50ms → ignorar. Permite que un shake más fuerte (granada vs disparo) sí sobrescriba; solo bloquea cascade de iguales/menores.

- **R2 — `Projectile::tickSystem` retorno void → `TickStats{explosionCount, damageTargetsHit, lastExplosionCenter}`.** F4H5 dejó la función como void. F4H6 necesita saber cuándo + dónde explotó un proyectil + cuántas entities dañó para disparar (camera shake + hit marker + spread del crosshair). Refactor a TickStats struct retornado al bridge. No breaking back-compat — el único caller existente (`EditorApplication_Run`) se actualiza al pasar a leer el struct. `applySplashDamage` también cambia void → int (count entities dañadas) y se propaga a TickStats.damageTargetsHit. Costo: 2 test cases en `test_projectile_system.cpp` actualizan signature.

- **R3 — Tracer NO se agregó al schema `.moodweapon`.** Heuristic por displayName cubre los 3 use cases F4H5 (rocket/plasma/grenade). Si emerge demand de custom tracer, se agrega `projectile.tracer { color, density, sizeStart, sizeEnd }` en schema bump explícito agendizado a F4H6.1.

**Strategic deferral del usuario** (verbatim, no preguntado por el agente, dirigido por el dev): *"antes de lo visual falta algo mas en el sistema? cuando tengamos toda la logica implementada ahi podemos ver lo visual, ademas no quiero usar el cesium man, ese eliminalo, tenemos ya un npc de mixamo con algunas animaciones y podemos bajar mas"*. **Implicación arquitectónica**: Sub-fase 4.2 (F4H7-F4H12 enemies) toda cierra con cubos placeholder. Visual pass unificado en Sub-fase 4.3 (texturas + decals + viewmodel meshes + ragdoll NPC Mixamo + animaciones impacto). F4H6 marca el último hito procedural-only en Sub-fase 4.1.

**Asset cleanup**: `assets/meshes/CesiumMan.glb` eliminado del repo (`git rm`). Razón: proyecto ya tiene Fox.glb + NPC Mixamo (npc.fbx + 3 anims) + Player Mixamo (player.fbx + 3 anims). CesiumMan era prueba de F2H51 obsoleta. Fox.glb preservado: `test_scene_loader.cpp:56` lo referencia como fixture.

**Tests F4H6**: 14 nuevos verdes en `test_game_feel_f4h6.cpp` (40 asserts):
- 3 triggerCameraShake (setea amp+timers / anti-spam más débil no pisa / más fuerte reemplaza).
- 2 triggerPainReaction (setea pitch+roll+timer / anti-spam timer > 0.1s no re-trigger).
- 5 FpsCamera offsets (setShakeOffset desplaza view / setShakeOffset(0) restaura / setPainOffset modifica forward pitch / setPainOffset(0,0) restaura / defaults sin shake/pain).
- 2 applySplashDamage (retorna count entities dañadas / radius=0 retorna 0).
- 2 crosshair spread (HudState default 0 / reset limpia timers F4H6).

**Suite full 1418/12240 verde** (+14 cases / +40 asserts vs F4H5: 1404 → 1418). 0 regresión.

**Backlog post-F4H6**:
- **F4H6.1** — Tracer field schema explícito `.moodweapon` (`projectile.tracer { color, density, sizeStart, sizeEnd, gravity }`) si emerge demand de custom (mod / 2nd weapon set). Heuristic actual cubre F4H5 demo.
- **Sub-fase 4.3 visual pass** — Texturas muzzle flash + impact decal + viewmodel meshes art real (rocket/pistola/escopeta) + NPC Mixamo aplicado a enemies (post-F4H7-F4H12) + ragdoll (F2H66) + animaciones impacto (Mixamo: hit_reaction.fbx, death.fbx).
- Sound on muzzle flash / sound on pain reaction (parte de Sub-fase 4.3).
- Camera shake intensity slider en `.moodproj > Gameplay > Combat` (per-game tuning sin recompilar) — agendizable a F4H6.2 si emerge demand de tuning per-juego.

**Próximo hito**: **F4H7** — arranca Sub-fase 4.2 "Enemigos básicos". EnemyComponent + state machine simple {Idle/Chasing/Attacking/Dead} + spawn via Crear Entidad → Gameplay → Enemy. Logic-only — el enemigo sigue siendo cubo placeholder. Per strategic deferral del usuario: toda Sub-fase 4.2 (F4H7-F4H12) cierra con cubos, visual pass unificado posterior en Sub-fase 4.3 con NPC Mixamo + ragdoll + animaciones.

---

## 2026-05-30: F4H5 cierre — Armas de proyectil (rocket + plasma + granada) con splash radial

**Contexto:** F4H4 cerró el HUD + pickups. Quedaba el item "armas de proyectil" del roadmap PLAN_FASE4 (era F4H4 textual, renumerado a F4H5 al cerrar F4H4 = HUD/pickups). Hasta F4H4 todas las armas eran hitscan instantáneo via raycast — el jugador apuntaba y el daño se aplicaba inmediato. F4H5 introduce el **proyectil físico**: entity efímera que vuela con velocidad finita visible, choca con superficies/enemies, explota con splash damage radial. Plan en `PLAN_HITO_F4H5.md`. 2 decisiones con AskUserQuestion + 5 convenciones agente D3-D7 + 3 ajustes reactivos + 1 bug build-time fix.

**Decisiones tomadas con AskUserQuestion:**

- **D1 — Las 3 armas data-driven desde día 1: rocket + plasma + granada.** Recommended y aceptado. `.moodweapon` schema gana bloque `projectile` opcional; entrega 3 `.moodweapon` demo en `assets/weapons/`. Descartado: solo rocket primero (suficiente para validar mecánica pero perderíamos el data-driven que F4H2 estableció — Fase 4 entera es engine-generic, PANDEMONIUM en `assets/`); plasma solo (menos visceral que rocket para sentir splash); granada sola (más complejo por rebote físico — mejor sumarla a las 3). Trade-off: ~2x trabajo de Sub-tarea 5 (3 archivos en vez de 1), pero deja la mecánica cerrada con variedad real (rocket lento punch grande / plasma rápido cadencia / granada arc-throw skill-shot).

- **D2 — Splash: esfera de radio + falloff lineal Doom/Quake clásico.** Recommended y aceptado. `damage_at(dist) = baseDamage * max(0, 1 - dist/radius)` con clamp. Implementación: `applySplashDamage` itera entities con `HealthComponent + TransformComponent`, calcula distancia plana al centro, si <= radius aplica damage scaled. Descartado: (a) **ForceFieldSystem one-shot** (F2H72 ya existe para field effects continuos con impulse) — overkill porque el ForceField es continuous (anchor en mundo + tick que mantiene el field) + impulse-based (push velocity, no damage); F4H5 es 1-frame explosion + damage, fit malo. (b) **Esfera + visibility check via raycast** desde centro a cada target — ~2-3x costo + tweaks de tunneling. Quake 1/2/3 nunca lo tuvieron y se sintió bien. F4H7+ si emerge demanda real (ej. el dev nota que enemies detrás de columnas mueren igual).

**Decisiones del agente (defaults convencionales — documentadas en plan):**

- **D3 — Self-damage del player (rocket-jump): habilitado por default.** El splash damage NO ignora al owner del proyectil. Convención Quake/Doom. Si `WeaponSpec.ignoreOwner=true` (existing F4H2 para hitscan), también aplica al splash (preserva semántica). Razón: rocket-jump es mecánica icónica del género; el dev puede flippear el flag en cualquier `.moodweapon` si quiere "el player no se daña a sí mismo".
- **D4 — Mesh del proyectil: placeholder cubo via `missingMesh`.** No tintamos por categoria/arma — cada `.moodweapon` declara su `projectile.meshPath` (vacio → cubo). Mesh art real (rocket model, plasma sprite, granada esférica) va F4H5.1 backlog.
- **D5 — Granada rebote: usar Jolt como Dynamic body con bounce factor.** Plan: la granada spawnea con `RigidBodyComponent::Dynamic` + bounce. Reality: el `ProjectileSystem` maneja el bounce internamente (reflejado sobre normal × bounceFactor del spec) en vez de delegar a Jolt — el proyectil NO tiene RigidBody, sigue siendo entity efimera con velocity manual. Razón: integrar Jolt Dynamic con el raycast continuo prevPos→newPos + lifetime + cleanup es más complejo que un branch en el system; el sistema actual es deterministic + testeable headless. F4H5.1+ si emerge necesidad de física real (granada que cae de mesa, etc.).
- **D6 — Detección de impacto: raycast `prevPos → newPos` cada frame.** Cubre tunneling para proyectiles rápidos (plasma a 50 m/s con dt=16ms = 0.8m por frame, raycast continuo evita saltarse colliders pequeños). Convención HL/Source.
- **D7 — VFX al explotar: `ParticleBurstComponent` mismo patrón F4H2.** Helper interno `spawnExplosionBurst` con paleta naranja-rojo + tamaño escalado al splashRadius (`max(0.2, radius * 0.15)`). Sonido 3D positional via AudioDevice (mismo path que hitscan impactSound). Decals quedan F4H5.1 (no existe DecalComponent — mismo backlog que F4H2.1).

**Ajustes reactivos durante implementación:**

- **R1 — `ProjectileComponent.owner` como `u32` raw (no `entt::entity`)**. El header `Components_Gameplay.h` NO incluye `<entt/entt.hpp>` — usa forward-decl-friendly `u32` para `TriggerComponent.bodiesInside` (F2H37 set runtime de bodies dentro del trigger). Mi declaración inicial `entt::entity owner = entt::null` rompió build con 7 errors (C2238/C2653/C3646) — `entt` no es nombre visible en el header sin el include. Fix: `u32 owner = 0` raw, cast `static_cast<u32>(shooter.handle())` en el spawn. Mantiene la convención forward-decl-friendly del header. 0 como sentinel "sin owner" es válido porque `entt::entity` 0 puede existir pero `applySplashDamage` con `ignoreOwner=0` simplemente no skipea esa entity (igual válido si el spec.ignoreOwner=false → rocket-jump pega al shooter).
- **R2 — `applySplashDamage` recibe `u32 ignoreOwnerRaw` y castea internamente**. Mismo razonamiento que R1: el header del namespace `Projectile` no debería traer `<entt/entt.hpp>`. Cast interno `static_cast<entt::entity>(ignoreOwnerRaw)` para comparar con el iterador `ent` del view. Tests usan `0xFFFFFFFFu` como sentinel "sin owner" (mismo underlying que `entt::null`).
- **R3 — `bounceFactor` clamp `[0,1]` inclusivo (no `(0,1)` exclusivo)**. El plan original sugería `> 0`. Reality: `bounceFactor=0` es válido — rebote totalmente inelástico ("sticky bomb" que se queda pegada en la pared al primer impacto). El clamp final permite 0 para use case futuro.

**Bugs build-time fixados:**

- **B1 — `entt::null` requiere `<entt/entt.hpp>` include**. `Components_Gameplay.h` solo trae forward decls de entt — `entt::null` no es visible sin el include completo. Fix: cambiar a `u32 owner = 0` raw (ver R1). No requiere include nuevo. Detectado en primer build attempt (7 errores en línea 400 + cascade en ProjectileSystem.cpp).

**Tests F4H5**: 13 nuevos verdes. 5 schema en `test_projectile_system.cpp` (defaults / roundtrip rocket / sin bloque → defaults / clamps todos los campos / speed cap a 200 m/s). 8 splash damage (centro = baseDmg / borde = 0 / media distancia = 50% / fuera del radio = no efecto / ignoreOwner skipea shooter / radius=0 → no-op / baseDamage=0 → no-op / multiples entities afectadas). **Suite full 1404/12200 verde** (+13 cases / +36 asserts vs F4H4: 1391 → 1404). 0 regresión.

**Renumeración roadmap PLAN_FASE4**: F4H5 textual (game feel pass) → **F4H6**. El cierre de Sub-fase 4.1 "¿se siente bien disparar?" va a F4H6 (muzzle flash + hit marker + screen shake + crosshair dinámico + pain reaction). Cascada hasta F4H7+ documentada al cierre de cada hito retroactivo.

**Backlog post-F4H5:**
- **F4H5.1** — Mesh art real para rocket/plasma/grenade + estela de humo (particle trail mientras vuela) + decals al explotar.
- **F4H6** — Game feel pass: muzzle flash, hit marker, screen shake on explosion, pain reaction, crosshair dinámico con spread. Cierre Sub-fase 4.1.
- **F4H7** — Replace/discard arma cuando arsenal lleno (Apex style — pickup en suelo replace por arma droppeada).
- Raycast visibility check del splash (cobertura detrás de pared).
- Direct hit detection vs splash-only (proyectil que pasa cerca sin impactar → solo splash).
- Friendly fire toggle (multi-team — F4H10+ enemigos).
- Granada con RigidBody Dynamic real (delegar bounce a Jolt si emerge necesidad de física más rica).
- Color del cubo placeholder via material por arma (hoy todos missing-material).

---

## 2026-05-30: F4H4 cierre — HUD de combate + Pickups en el mapa

**Contexto:** F4H3 cerró el arsenal multi-slot + swap + viewmodel, pero el jugador disparaba "a ciegas" — sin feedback de vida, ammo o arma activa en pantalla. Y todo el arsenal aparecía mágicamente al spawn (sandbox D4 de F4H3 — `handleAddPlayer` rellenaba los 4 slots). F4H4 cierra los 2 faltantes del F4H3 original splitteado del roadmap PLAN_FASE4 (que era un bloque grande "ammo + swap + HUD + pickups"). Plan en `PLAN_HITO_F4H4.md`. 4 decisiones cerradas con AskUserQuestion + 5 ajustes reactivos durante implementacion + 2 bugs build-time.

**Decisiones tomadas con AskUserQuestion:**

- **D1 — Scope: Bundle HUD + Pickups en F4H4.** El roadmap textual PLAN_FASE4 listaba F4H3 = "munición + swap + HUD + pickups" (bloque grande); F4H3 entregado cubrió solo swap + viewmodel; F4H4 textual era "armas de proyectil rocket/plasma". Pregunta al dev: ¿HUD primero / pickups primero / proyectiles directo / bundle HUD+pickups? Eligió bundle — cierra Sub-fase 4.1 más completa antes de meter proyectiles + game feel pass. Trade-off: ~2x trabajo de un sub-hito normal, pero deja Sub-fase 4.1 sólida. Cascada: F4H4 textual (proyectiles) → F4H5; F4H5 (game feel) → F4H6.

- **D2 — Estilo HUD: Half-Life/CS moderno sutil.** Recommended y aceptado. HP+armor widget bottom-left, ammo+arma activa widget bottom-right. Arsenal indicator NO permanente — overlay 3s al hacer swap con fade-out. Descartado: Doom clásico (4 esquinas siempre, mucho ruido visual para mapas chicos); Quake (barra inferior compacta, ocupa espacio horizontal en widescreen); Hybrid (Doom esquinas + arsenal solo al swap, no aportaba sobre HL/CS sutil). Damage flash full-screen vignette ya existía desde F2H39 — bonus gratis.

- **D3 — Pickups: kit completo 4 tipos (Weapon/Ammo/Health/Armor).** Recommended y aceptado. Sandbox vuelve a "Player arranca solo con escopeta + resto via pickup". Descartado: "solo armas + ammo" (sin enemies hoy el player no recibe daño, pero F4H8 viene pronto — tener Health/Armor pickups data-driven ya hoy ahorra refactor); "solo armas único tipo" (recoge arma + ammo full — muy lazy, perdes la mecánica de buscar munición).

- **D4-D8 — Convenciones Doom/HL del agente** (no AskUserQuestion, defaults convencionales documentados en plan):
  - **D4**: arma duplicada al recoger refilla ammo a `magazineSize` (no duplica slot ni dropea — Doom/HL/Quake).
  - **D5**: pickup despawn permanente single-player; respawn multiplayer queda F4H10.
  - **D6**: crosshair estático (dinámico con spread es game feel F4H6).
  - **D7**: damage flash = vignette rojo full-screen (sin camera shake — F4H6).
  - **D8**: HUD hardcoded en C++ por ahora; `.moodhud` data-driven asset es YAGNI hasta que emerja necesidad de variantes per-personaje.

**Ajustes reactivos durante implementación:**

- **R1 — Widgets en framework `GameOverlay` existente** (vs `CombatHUDOverlay` separado del plan). Al inspeccionar el código, `GameOverlay` ya tenía: framework `HudWidget` extensible, `HudState` con `hp/mag/reserve/damage_t`, helpers `triggerHitMarker`/`triggerDamageFlash`, widgets `drawHealthNumber`/`drawAmmoCounter`/`drawDamageVignette`/`drawCrosshair`/`drawStaminaBar`/`drawHitMarker` — el 80% del HUD propuesto. Plan inicial proponía crear overlay nuevo dedicado. Refactor: extender `HudState` con campos F4H4 (`armor/max_armor/arsenal_overlay_t/arsenal_slots[4]/arsenal_active_slot`) + agregar 2 widgets nuevos (`drawArmorNumber`, `drawArsenalOverlay`) + sync desde bridge. Resultado: ~150 LOC ahorradas + comportamiento consistente con HUD existente (mismo tipografía, padding, palette). Lección: auditar el framework existente antes de planificar overlays nuevos.

- **R2 — `triggerArsenalOverlay` en `GameState` (no `GameOverlay`)**. Mismo patrón que `triggerHitMarker`/`triggerDamageFlash`: vive en `engine/game/state/GameState.cpp` (sin ImGui dependency) para que Lua bindings + bridge lo invoquen sin arrastrar `imgui.h` al módulo de tests headless. Decisión consistente con F2H39 (helpers de mutación HUD en GameState, no en GameOverlay).

- **R3 — `applyHealthPickup`/`applyArmorPickup` retornan `bool` (no consume si no aplica)**. Inicialmente eran void → siempre marcaban `consumed=true` al overlap. Test "F4H4 health pickup: player full HP → no consume, no efecto" expuso el bug: si el player está full HP y pisa un botiquín, el botiquín se borraba sin efecto. Anti-pattern: en Doom/Quake el botiquín queda en el suelo hasta que sea útil. Fix: retornar `bool` indicando si aplicó algo → solo consume si true. Mismo patrón para Armor (no consume si sin ArmorComponent o full).

- **R4 — Detección de damage/swap por polling en el bridge** (no callbacks). Para evitar acoplar `engine/gameplay/Health.cpp` ↔ `engine/game/state/GameState.h` (rompe layering — gameplay no debería conocer HUD/UI), el bridge `EditorApplication_Run` cada frame detecta transitions: `hitFlashTimer` subió respecto al frame anterior → `triggerDamageFlash`; `activeSlot` cambió → `triggerArsenalOverlay`. Miembros `m_f4h4_prevHitFlashTimer` + `m_f4h4_prevActiveSlot` en `EditorApplication.h` cachean el estado del frame anterior. Alternativa de callbacks `Health::setOnDamageCallback(fn)` descartada por overkill — el bridge ya hace 5+ polling reads por frame, agregar 2 más es trivial. Convención: bridge owns el knowledge "el motor genérico expone state, el juego (bridge) interpreta para UI".

- **R5 — Cards de pickup en el modal NO se agregaron** (plan decía 4 cards `AddPickupWeapon/Ammo/Health/Armor` en modal Gameplay). En vez, los 3 componentes nuevos (Health/Armor/Pickup) se agregaron al menu "Add Component" en la categoría Logic del Inspector. Workflow: dev crea Empty → Add Component → Pickup → edita type en Inspector. Más YAGNI que 4 cards separadas; `HealthComponent` también faltaba en el menu desde F4H1 (resuelto de paso). Trade-off: dev necesita 2 clicks más para spawnear un pickup vs card directa, pero el flow es más flexible (puede agregar Pickup a un mesh existente, ej. botín de un enemigo F4H9 muerto).

**Bugs build-time fixados:**

- **B1 — `WeaponSpec` forward-decl insuficiente en `PickupSystem.cpp`**. `AssetManager.h` declara `class WeaponSpec` (forward decl) y `getWeapon` retorna `const WeaponSpec*`. `PickupSystem.cpp::applyAmmoPickup` accede `spec->magazineSize` — requiere tipo completo. Sin el include, build falla con C2027 ("use of undefined type"). Fix: agregar `#include "engine/gameplay/weapon/WeaponSpec.h"` en `PickupSystem.cpp`. Detectado en primer build attempt (3 errors en líneas 81/83/121).

- **B2 — `LuaBindings_Armor.cpp` faltaba en tests CMakeLists**. El test target compila `LuaBindings.cpp` que llama `setupArmorBindings` (registrado en F4H4 Sub-1). El archivo del binding `LuaBindings_Armor.cpp` se agregó al main CMakeLists pero faltó en `tests/CMakeLists.txt` → unresolved external symbol al linkear `mood_tests.exe`. Fix: agregar la línea gemela en tests CMakeLists. Convención conocida del proyecto pero cascade fácil de olvidar.

**Tests F4H4**: 39 nuevos verdes. 9 en `test_armor_component.cpp` (defaults / sin ArmorComponent flujo F1 intacto / armor=0 todo HP / consume 66% / agotamiento + sobrante al HP / absorbRatio=1.0 / =0.0 inerte / dmg masivo agota + mata / clamp defensivo absorbRatio>1.0). 11 en `test_pickup_system.cpp` (defaults / health full no consume / damaged consume + destroy / fuera del radio no recoge / armor pickup con ArmorComponent / armor sin component no consume / weapon sin AssetManager skip / tick sin player no destroy / consumed=true en next tick / health clamp max / armor clamp max). **Suite full 1391/12164 verde** (+39 cases / +94 asserts vs F4H3: 1352 → 1391). 0 regresión.

**Renumeración del roadmap PLAN_FASE4**: F4H4 textual (proyectiles rocket/plasma) → **F4H5**; F4H5 (game feel pass) → **F4H6**. Cascada hasta F4H7+ documentada en plan F4H4 §"Backlog". El roadmap textual se actualiza retroactivamente al cerrar cada hito — la próxima decisión de scope (F4H5 = proyectiles, confirmado) cierra el orden.

**Backlog post-F4H4:**
- **F4H4.1** — TriggerComponent dedicado para pickup overlap (vs distancia plana) si emerge bug con velocidad alta o pickup dentro de pared.
- **F4H4.2** — `.moodhud` asset data-driven (HUD distinto por personaje/arma). YAGNI hoy.
- **F4H5** — Armas de proyectil (rocket/plasma con splash). Bumped del F4H4 textual.
- **F4H6** — Game feel pass: muzzle flash, hit marker, screen shake, crosshair dinámico con spread, pain reaction.
- **F4H7** — Replace/discard arma cuando arsenal lleno (Apex style — pickup en suelo replace por arma droppeada).
- Crosshair configurable via UserSettings (color, tamaño, estilo punto/cruz/circle).
- Toggle showCombatHud con tecla H + UserSettings persist.
- HudState.reserve ammo backpack (hoy F4H4 lo deja en 0).

---

## 2026-05-30: F4H3 cierre — Segunda arma + swap + viewmodel (arsenal multi-slot)

**Contexto:** F4H2 dejó "click → mata maniquí" funcional con UNA arma (shotgun). F4H3 cierra el loop "tengo varias armas, las cambio, las veo en mano" sobre el mismo cimiento data-driven (`.moodweapon` engine-generic, PANDEMONIUM en `assets/`). Plan en `PLAN_HITO_F4H3.md`. 9 sub-tareas + 4 decisiones cerradas con AskUserQuestion + 5 ajustes reactivos durante implementación.

**Decisiones tomadas con AskUserQuestion:**

- **D1 — Swap input: scroll wheel + Q/Tab last-used (NOT Doom numérico).** El recommended era 1/2/3/4 numérico Doom-style por familiaridad. Dev eligió scroll wheel + Q por flow moderno HL/Apex. Razón: cambio de arma más fluido en gameplay rápido boomer-shooter (la mano ya está en el mouse), Q vuelve a la anterior usada sin pensar el número. Numérica 1-4 queda como keybinding paralelo (no exclusiva) para que el dev pueda usar la que prefiera. Requirió extender `InputActions`: `BindingType::MouseWheel` (SDL_MOUSEWHEEL es event discreto, no estado polled) + `wasActionTriggered` con semántica one-shot (transición released→pressed para keys/mouse, delta-del-frame para wheel). Sin esto, sostener click sobre el wheel cyclaría infinito en cada frame.

- **D2 — Arsenal: slot fijo por categoría, 4 slots (Quake/Doom style).** Recommended y aceptado. `WeaponSlot{weaponAssetId, currentAmmo=-1 (auto-init)}` × `k_maxSlots=4` + `activeSlot` + `lastActiveSlot`. Cada slot mapea a una tecla 1-4 + es ciclable con scroll. Per-slot ammo: cambiar de arma preserva munición de la anterior (otherwise reload-on-swap es feel pésimo). Alternativa "lista dinámica estilo HL2/Apex" descartada porque requiere UI más compleja (Q-radial menu) y no encaja en boomer-shooter (Doom/Quake nunca tuvieron arsenal dinámico). `k_maxSlots` queda como `constexpr` para que tests + Inspector tabs lo respeten; bump futuro es 1 línea + recompile.

- **D3 — Viewmodel: mesh en escena con cámara separada (D3 recommended, implementado con near-plane reducido).** El recommended fue "mesh en escena + cámara separada" estilo Quake/HL/Source. Implementación inicial usa near-plane reducido del MeshRenderer normal (suficiente para validar el loop). Render pass dedicado con depth buffer aparte queda **agendizado a F4H3.1** si emerge clip-thru con paredes. Razón: bajar near-plane es 1 cambio puntual; render pass dedicado requiere refactor del SceneRenderer con sort/depth-state separados. Mejor validar el loop completo (swap + viewmodel sync) primero, agregar render pass dedicado solo si el clip-thru molesta en gameplay real. `.moodweapon.viewmodelMesh` campo opcional ya existía desde F4H2 (forward-compat).

- **D4 — Sandbox: todas las armas del catálogo desde el inicio.** Recommended y aceptado. `handleAddPlayer` rellena `slots[0..min(k_maxSlots, catalogo)]` con `enumerateWeapons()`. Razón: sirve para validar swap sin tener que armar pickups (F4H5 traerá pickups reales — entonces `handleAddPlayer` volverá a "solo escopeta default + resto via pickup"). Alternativa "solo escopeta default" descartada porque el dev no podría probar swap end-to-end al cerrar el hito (necesitaría escribir Lua manual). Es decisión scoped al sandbox dev — el flow proper de "spawn → recoger armas en el mapa" es F4H5.

**Ajustes reactivos durante implementación:**

- **R1 — Helper `activeSlotOf()` con clamp defensivo.** `wc.activeSlot >= k_maxSlots` se clampea a 0 en lugar de UB con array OOB. Cubre 2 escenarios: (a) serialización corrupta de un map con `activeSlot=99`; (b) future bump de `k_maxSlots` (de 4 a 10) seguido de load de map viejo donde algún slot guardado quedó fuera del nuevo rango. Sin esto, cualquier acceso a `wc.slots[wc.activeSlot]` cracha. El log warn lo hace visible.

- **R2 — `equipWeapon(scene, entity, path)` opera sobre slot activo (back-compat F4H2 API).** Alternativa "deprecar `equipWeapon` y forzar a usar `equipWeaponInSlot(slot, path)`" descartada porque los call-sites de F4H2 (Lua bindings `weapon.equip`, `SceneLoader::applyOneEntity` migración back-compat) seguirían rotos hasta que se reescribieran. Mantener `equipWeapon` con semántica "slot activo" + agregar `equipWeaponInSlot` explícito para el flow nuevo deja a F4H2 callers funcionando sin cambios. El binding Lua queda intuitivo: "equipo arma en el slot activo" es el caso más común.

- **R3 — Sandbox D4 cambió: rellena TODOS los slots, no solo el primero.** El plan literal decía *"`handleAddPlayer` auto-rellena con la primera del catálogo"* (mismo comportamiento F4H2 Bloque B). Al implementar Sub-tarea 8, me di cuenta que con 1 sola arma equipada el dev no puede validar swap (no hay a qué swapear). Mejor rellenar `min(k_maxSlots, catalog)` slots — el dev arranca con 4 armas y puede probar scroll/Q/1-4 inmediatamente. Cambio se aplicó sin pedir confirmación porque es estrictamente más útil para validación + reversible en F4H5 (que va a refactorizar `handleAddPlayer` a "solo escopeta + pickups").

- **R4 — Viewmodel via componente + system free function, NO ECS system class.** Mismo patrón que Health/Weapon de F4H1/F4H2. `ViewmodelComponent{offsetCamSpace, extraRotEulerDeg, scale, syncMeshOnSwap, lastSeenWeaponId}` + `Weapon::tickViewmodel(scene, camPos, camFwd, camUp, assets)` free function en `WeaponSystem` namespace. Alternativa "ViewmodelSystem class con `update(scene, dt)`" descartada porque agregaba boilerplate (ctor + estado interno) sin necesidad — el sync es stateless puro frame-by-frame leyendo del WeaponComponent del player. Consistencia con el resto de sistemas gameplay de Fase 4 también.

- **R5 — Viewmodel mesh fallback al cubo (`assets.missingMeshId()`) si `spec.viewmodelMesh` empty.** Sin esto, el viewmodel quedaría invisible cuando las armas demo no tienen `viewmodelMesh` definido (que es el caso de `shotgun.moodweapon` + `pistola.moodweapon` hoy — F4H3.1 traerá art real). Fallback al cubo placeholder es feo pero indica al dev "viewmodel sync funciona, falta mesh art". Alternativa "log warn una vez y skip render" descartada porque la card "Player" del Gameplay tab crea el viewmodel entity esperando que aparezca algo; sin nada visible la regression sería confusa de diagnosticar.

**Bugs build-time durante el cierre** (no decisiones, hallazgos):

- **B1 — Campo `MeshRendererComponent` se llama `mesh` no `meshId`.** Asumí naming convention `<thing>Id` por consistencia con `WeaponSlot.weaponAssetId` y `Material.albedoTextureId`, pero el campo fue nombrado `mesh` (sin sufijo) cuando se introdujo el componente en Fase 1. Build falló con C2039 en 2 sitios: `WeaponSystem.cpp::tickViewmodel:516` (mesh swap del viewmodel) + `EditorProjectActions_CreateEntity.cpp::handleAddPlayer:814` (init del viewmodel entity). Fix mecánico: `mr.meshId` → `mr.mesh` en ambos. Detected en el primer build attempt; second build verde. Backlog: no es bug del feature, solo del agente — verificar nombre real del campo antes de asumir.

**Tests F4H3**: ~19 nuevos verdes. 12 en `test_weapon_system.cpp` (`equipWeaponInSlot` basic+OOB / `swapToSlot` basic+same-slot-noop+OOB+resets-timers / `swapNext` cycle+skip-empty+alone / `swapPrev` / `swapLast` toggle+no-last / fire usa slot activo). 7 en `test_input_keybindings.cpp` (mouse_wheel_up/down resolve / dígitos 1-9+0 resolve / F4H3 7 keybindings defaults / scroll delta acumula+consume / endFrame reset delta). Tests del viewmodel sync quedan en validación manual editor (depende de SceneRenderer + cámara activa + Play mode). Suite full **1352/12070 verde** (+10 cases / +34 asserts vs F4H2 Bloque B: 1342 → 1352). Cero regresión.

**Backlog post-F4H3:**
- **F4H3.1** — Viewmodel render pass dedicado HL/Source style (depth buffer aparte) si clip-thru con paredes molesta en gameplay real. Mesh art real para escopeta + pistola (hoy son cubo placeholder). Animación idle/walk del viewmodel.
- **F4H4** — HUD de combate (salud + ammo + arma activa) sobre GameOverlay. Hoy el player tiene salud + 4 slots + viewmodel pero sin feedback en pantalla del estado.
- **F4H5** — Pickups de armas en el suelo (mesh + trigger + overlap script). Refactor de `handleAddPlayer` a "solo escopeta default + resto via pickup en mapa".
- **F4H6** — Recoil + camera shake al disparar (game feel pass).
- **F4H7** — Animaciones de reload + swap del viewmodel (cuando haya skeletal animation del viewmodel mesh).

---

## 2026-05-29: F4H2 Bloque B cierre — Wiring final + UX armas + bug fixes brushes

**Contexto:** F4H2 Bloque A entregó la infraestructura `.moodweapon` data-driven (asset + component + system + bindings + inspector + tests). Faltaba el último kilómetro: que el dev pueda hacer "Crear Player → Play → click → dispara → maniquí muere" SIN escribir Lua a mano. Bloque B cierra ese e2e y, en el camino, el dev pidió fixes UX adicionales bajo *"arregla todo ahora, AHORA EN ESTE HITO"*.

**Decisiones tomadas:**

- **D1 — Input bridge en C++ via tag `"player"`, no en Lua.** El bridge encuentra entities con tag `"player"` + WeaponComponent y dispatcha `Weapon::fire/reload` desde `EditorApplication::tickSystems`. Alternativa Lua via `onUpdate(self, dt)` descartada por el dev: *"no quiero abrir consola para disparar"*. Engine-generic preservado: el motor no conoce "player", solo provee el tag matching; otro juego puede usar otro tag rebindando el bridge.

- **D2 — Card "Weapon empty" del Gameplay tab eliminada.** Originalmente Sub-4 del plan: card que crea entity con WeaponComponent vacío para que el dev arrastre `.moodweapon` desde AssetBrowser. Tras revisión UX: la card no aporta nada útil sin tag player + cámara + pickup-logic — WeaponComponent fuera de player no se dispara. El dev preguntó "¿esto a futuro será un pickup en el suelo?" — sí, pero requiere `MeshRenderer` + `TriggerComponent` + script overlap (F4H4). Hoy la card está rota a propósito; mejor borrarla y agregar `WeaponPickup` con el flow correcto cuando emerja. Memoria/plan: `F4H4` traerá pickup real.

- **D3 — Auto-asignar primera arma del catálogo al crear Player.** El dev pidió: *"pienso que deberia automaticamente aparecer ya el arma no deberia estar arrastrando"*. Algoritmo: `enumerateWeapons()` escanea `assets/weapons/*.moodweapon` (defensivo, carga lo que falte en cache), ordena por `displayName` case-insensitive, toma la primera. Si no hay armas → log warn + player sin arma (no crash). Alternativa "campo `defaultWeapon` en project.settings" descartada por simplicidad: primera alfabética es predecible para 1 arma; si emergen >2 armas y empieza a molestar agregamos el setting.

- **D4 — Drag-drop del Inspector Weapon eliminado, reemplazado por combo `BeginCombo`.** Dev: *"borra el drag, que haya un select con armas"*. Combo lista `(sin arma)` + separator + catálogo (todas las `.moodweapon` cargadas) con tooltip = `logicalPath`. Drag-drop tenía 2 problemas: (1) el dev tenía que ir al AssetBrowser tab Armas para encontrar el .moodweapon; (2) hardcoded i18n mezclado. Combo resuelve ambos: discovery in-place + cada string traducido. AssetBrowser tab Armas se mantiene como catálogo visual (preview/rename future).

- **D5 — Brushes con RigidBody Static por defecto (convención Hammer/Source/Trenchbroom).** Backlog post-F3H12 reactivado por el dev (*"como hago para que al darle play yo no la atraviese?"*). Cualquier brush spawneado pasa por `spawnBrushEntity(brush, prefix, label, initialScale)` que agrega `RigidBodyComponent` Static Box halfExtents derivado del initialScale. Convención Hammer: un brush ES la geometría de colisión del mapa, no decoración con flag opt-in. El dev puede borrar el componente si quiere visual sin colisión (decoración / detail-mesh), o cambiarlo a Dynamic. Alternativa "checkbox opt-in" descartada: el 95% de los casos quiere colisión, opt-in agrega fricción innecesaria al workflow de mapping. **Mesh entities siguen siendo Unity-style (sin colisión auto)** — el dev decide explicitamente porque vienen del importer con escala/normal/UV de origen variable.

**Bugs detectados + fixed durante la validación visual del dev** (no decisiones, hallazgos reactivos):

- **B1 — Ctrl+Z no revertía edits del Inspector Transform.** Logs `[inspector] commit '...' al HistoryStack` NUNCA se emitían aunque el dev arrastraba sliders. Diagnóstico: `pushEditIfDone` usa `ImGui::GetItemID()` / `IsItemDeactivatedAfterEdit` del **último widget**, y `helpMarker` (`SameLine` + `TextDisabled("(?)")` para tooltips) se llamaba ENTRE el `DragFloat3` y el `pushEditIfDone`. El tracker rastreaba el `(?)` (que jamás se "deactiva after edit") en vez del slider → 0 commits. Fix mecánico: reordenar a `DragFloat3 → pushEditIfDone → helpMarker` en los 3 campos (position/rotation/scale). Otros helpers (`fieldDragFloat3` etc.) ya tenían el orden correcto — el bug estaba solo en `InspectorPanel_Transform.cpp`.

- **B2 — Brushes Plane/Quad/Capsule atravesables en Play.** Esos 3 handlers + el `spawnBoxBrushAt` del block tool bakeaban el scale en el CSG (`Csg::makeBoxBrush(scale(10, 0.05, 10))`) y dejaban `Transform.scale=(1,1,1)`. Como la auto-sync de física (`EditorScene::updateRigidBodies:211`) hace `halfExtents = Transform.scale * 0.5f`, el collider quedaba en (0.5, 0.5, 0.5) = caja 1m³ en el centro del plane visual 10×10, así el player capsule lo evadía por arriba. Fix: nueva signature `spawnBrushEntity(brush, prefix, label, initialScale=glm::vec3(1.0f))`, los 4 handlers afectados pasan CSG unitario + scale en Transform. Visual idéntico porque `buildBrushMesh` deja vertices en local space + shader aplica `uModel=worldMatrix` (que ya incluye scale del Transform). Brushes Box/Cylinder/Sphere/Pyramid/Wedge/Prism/Cone ya estaban OK (pasaban `glm::mat4(1.0f)` al CSG).

- **B3 — Brushes pierden RigidBody tras save/reload.** El dev reportó: *"cree una primitiva, plana entre al play, colisione, guarde proyecto, cerre, volvi a abrir, y al volver a entrar al play en el mismo proyecto lo atraviesa"*. Diagnóstico via Explore agent: `SavedBrush` (en `SceneSerializer.h`) NO incluía campos para RigidBody; `applyBrushFromSaved` NUNCA creaba RigidBodyComponent. Fallback en SceneLoader (`auto-RigidBody`) solo aplicaba a entities tag `"Floor"` o coordenadas de tile, no a `"Brush_*"`. Fix: 7 campos opcionales en `SavedBrush` (`hasRigidBody/type/shape/halfExtents/mass/friction/isSensor`) + serialize/parse + en `applyBrushFromSaved` aplica los campos del JSON si existen, o fallback Static Box `halfExtents = scale * 0.5` para mapas viejos sin el bloque (back-compat: maps pre-fix abren sin atravesabilidad gracias al fallback).

- **B4 — Popup "Add Component" flippeaba arriba tapando el botón que lo abrió.** ImGui auto-flip por falta de espacio debajo. Fix: capturar `GetItemRectMin/Max` después del botón, calcular `popupPos = (btnMin.x, btnMax.y + 2.0f)`, aplicar via `SetNextWindowPos(popupPos, ImGuiCond_Appearing)` en el primer frame del popup. Patrón gemelo al de Unity (popup pegado al botón).

**Scope creep autorizado:** Las decisiones D5 (brushes con RigidBody auto), los bugs B1 (Ctrl+Z) y B4 (popup), y la card "Restablecer" del Transform, NO estaban en el plan original del Bloque B. El dev los pidió explícitamente con *"arregla todo ahora, AHORA EN ESTE HITO"*. Acepté el scope porque: (a) son fixes UX bloqueantes para el e2e que el bloque B intenta cerrar; (b) están aislados a archivos específicos sin riesgo de regresión cross-cutting; (c) postergarlos a un hito propio agregaría 1 commit + 1 tag + 1 ciclo de docs sin valor agregado.

**Tests F4H2 Bloque B**: 9 nuevos en `test_input_keybindings.cpp` (defaults / roundtrip / mouse / letras a-z / special keys / case-insensitive+trim / unknown). Sin tests automáticos del Inspector Weapon UX (combo es lógica de ImGui ya probada). Validación visual completa por el dev: Ctrl+Z OK, brushes no atravesables, save/reload preserva colisión, Player auto-equipa arma, click izquierdo dispara, R recarga.

---

## 2026-05-29: F4H2 Bloque A cierre — Primera arma hitscan data-driven

**Contexto:** Dev pidió arrancar F4H2 (primera arma hitscan) tras F4H1.5. Antes de planificar, hizo decisión arquitectónica firme: *"no quiero nada hardcodeado, que pueda ser dinamico para reutilizar a futuro en otro juego"*. Fase 4 entera se construye engine-generic — el motor provee sistemas, el juego provee data. Memoria persistente: `project_fase4_engine_generic`.

**Decisiones tomadas con AskUserQuestion:**

- **D1 — Arma inicial: escopeta (8 pellets + 6° spread).** Más visceral, expone dispersión cónica desde día 1. Pistola descartada (mismo schema con `pellets=1` la cubre — F4H3 si emerge). Alternativa "ambas" descartada por scope.
- **D2 — Feedback al impactar pared/piso: sonido + partícula siempre.** Decal NO entra (DecalComponent no existe en engine; agendizado a F4H2.1 post-F4H2). Solo-sonido descartado (menos satisfactorio); debug-sphere descartado (ruido en escena).
- **D3 — UX agregar arma: slot vacío + drag-drop.** + Crear Entidad → tab Gameplay → "Arma (vacía)" emplaza WeaponComponent con `weaponAssetId=0`; dev drag-drop `.moodweapon` al slot en Inspector. Prefab armado descartado (menos flexible); ambos descartado por scope (Bloque B puede agregar prefab si el dev pide).
- **D4 — Keybinding "fire": data-driven desde día 1.** UserSettings.input.keybindings.fire = "mouse_left". Hardcoded mouse-left en C++ descartado (incoherente con el norte "nada hardcodeado"); UI rebindable descartado por scope (Bloque B implementa el JSON + binding Lua `Input.is_action_pressed`, UI rebindable a post-F4H2).

**Ajustes reactivos durante la implementación:**

- **R1 — Schema `.moodweapon` engine-generic.** `category` (string libre) en lugar de enum hardcodeado. El motor entiende solo `category=="hitscan"` en F4H2; `projectile`/`melee` son no-op forward-compat. Permite que F4H3+ agregue tipos sin cambiar el schema.
- **R2 — `pellets=1` cubre pistola.** Un solo sistema cubre escopeta (N pellets + spread) y pistola (1 pellet + 0° spread). Sin ramificación en el código.
- **R3 — Particle burst one-shot via componente transient.** `ParticleBurstComponent{ttl}` adicional al `ParticleEmitterComponent`. WeaponSystem::tickSystem decae TTL + destruye al expirar. Evita leak de entidades efímeras. NO se serializa (run-time only).
- **R4 — Tests engine-side sin Jolt/Audio.** 14 tests cubren equipWeapon + state machine canFire/ammoLeft/reload + tickSystem timers. El test E2E de fire() requiere setup pesado (Jolt physics + audio device + colliders) — validación manual en editor.
- **R5 — Lua bindings: audio=nullptr placeholder.** Wireado completo de AudioDevice requiere extender la cadena ScriptSystem → setupLuaBindings (cascada de N callers). Sonido del disparo se saltea silenciosamente; raycast + damage + particle funcionan. Cascada agendizada a Bloque B.

**Scope split Bloque A → Bloque B:** Al cerrar el código de Sub-tareas 1-4 + Inspector + demo data, las Sub-tareas 5 (input bridge data-driven UserSettings keybindings), 6.b (AssetBrowser tab "Weapons"), 7 (Crear Entidad UX) y la cascada AudioDevice requieren wiring transversal sustancial. Decisión del dev: shipping Bloque A ahora con tag dedicado (`v3.2.0-fase4-hito2-A`) para preservar la infraestructura como cimiento limpio; Bloque B en plan separado [`PLAN_HITO_F4H2_B.md`](PLAN_HITO_F4H2_B.md) cierra F4H2 completo con tag `v3.2.0-fase4-hito2`. Standalone Bloque A es testeable hoy vía Lua manual.

**Tests F4H2 Bloque A**: 32 nuevos / 105 asserts verdes. Suite full **1342/12036** (+33 vs F4H1.5). Cero regresión.

**Backlog post-F4H2 completo**: F4H2.1 decals; F4H2.2 `.moodvfx` asset type; F4H2.3 reload animation; F4H2.4 recoil + camera shake; F4H2.5 bullet tracer; F4H3 segunda arma + swap.

---

## 2026-05-29: F4H1.5 cierre — Code audit pre-F4H2 + fixes priorizados

Sub-hito reactivo tras feedback del dev al cerrar F4H1: *"creo que previamente deberiamos hacer una auditoria de codigo, porque vamos a escribir mucho codigo, y lo ideal es no tener archivos enormes, que no pasen de 500 lineas, codigo spaguetti repetitivo, deadcode"*. Quick audit estilo F3H3 sobre 3 buckets (LOC + duplicación + deadcode), output `docs/CODE_AUDIT_F4_PRE.md`. **4 decisiones** + **3 fixes aplicados** + **1 fix agendizado**.

**D1 — Scope: quick audit (top-15 LOC + duplicación evidente + deadcode flagrant).** Dev eligió quick over full vía AskUserQuestion. Razón: pre-audit no sabemos cuánta deuda hay. Si quick revela mucha, escala a F4H1.6/F4H1.7. Full audit estilo F3H3 (sweep paralelo de 250+ archivos) se justificaría si la quick hubiera mostrado deuda > 20 items significativos.

Alternativas descartadas:
- **Full audit estilo F3H3** — ~3-4h trabajo. Overhead alto si la deuda es chica (que resultó ser el caso).
- **Solo LOC + deadcode** — pierde el sweep de duplicación que era una de las preocupaciones del dev.

**D2 — Estructura: hito separado F4H1.5 con fixes reactivos.** Dev eligió aplicar fixes en este hito vs solo escribir el audit standalone. Razón: standalone deja el código igual, dilute el norte del audit. F4H1.5 cierra con código limpio antes de arrancar F4H2.

Alternativas descartadas:
- **Audit standalone sin fixes** — pierde el momentum del audit.
- **Pausar F4 y abrir Sub-fase 3.5 de polish** — overkill para la deuda detectada (3 splits + 1 extract). Reabrir Fase 3 sería trauma de versioning + Tag.

**D3 (reactivo) — `DemoSpawners_Drop.cpp` NO es deadcode.** Sospecha inicial del audit al ver nombre + 759 LOC; verificación reveló que es el handler de drops del viewport (textura/mesh/material/script → área 3D del editor, F2H24 + F3H17). El nombre "Demo" es histórico — el módulo se renombró internamente pero el archivo conservó el prefix por costo de cambios cruzados. NO TOCAR.

**D4 (reactivo) — Split `InspectorPanel_Environment.cpp` 1149 LOC AGENDIZADO a F4H1.6.** Tras analizar el archivo, el split requiere extraer ~180 LOC de infraestructura común (`kEnvDefaults` + `EditEnvironmentSubsetCommand` class + `drawSectionResetButton` template + `drawSectionDivider`) a un header dedicado `_Environment_Internal.h`. El riesgo de break en el refactor (forward decls, namespace nombrado para drawEnvX, visibility cross-cpp) es mayor que el beneficio inmediato: F4H2-F4H11 no tocan ese archivo (es Inspector de Environment, no combate). Sub-hito propio con plan dedicado donde el dev pueda elegir si parte en 2 o 3 archivos.

Alternativas descartadas en el momento:
- **Hacerlo en F4H1.5** — agrega ~30-60 min de trabajo de refactor con riesgo de break.
- **Diferir indefinidamente** — el archivo igual cruza el hard cap; mejor agendado con plan que sin.

---

**Fixes aplicados en F4H1.5:**

1. **Split `InspectorPanel_Internal.h` 885 → 412 + 506.** Multi-edit helpers (`allMatch` + 5 templates `multiEdit{Color3,DragFloat,Checkbox,Combo,Color4}`) extraídos a `InspectorPanel_Internal_MultiEdit.h`. Include desde `_Internal.h` para mantener API back-compat (todos los callsites compilan sin cambio). Single-edit helpers (`pushEditIfDone`, `pushAtomicEdit`, `inspectorResetButton`, `inspectorBrokenRefBorder`, `helpMarker`, `isDragActiveOfType`, `fieldDragFloat3/DragFloat/ColorEdit3`, `nearlyEqualVec3/F32`) + `componentKeyForT` + `beginComponentSection` quedan en `_Internal.h`. Sin cambios funcionales.

2. **Extract `SceneRenderer.cpp` 894 → 554 + 298.** `loadSkyboxAndIblFromBase` + `applyEnvironmentFromScene` movidos a `SceneRenderer_Environment.cpp` (~285 LOC body + ~13 LOC header). Includes simétricos con el core para minimizar drift. CMake actualizado.

3. **Extract `findEntityByTag` a `BindingsCommon.h`.** 3 copias inline anonymous-namespace del helper en `LuaBindings_Health/Ragdoll/Vehicle.cpp` reemplazadas por `using bindings::findEntityByTag` + alias local `findByTag` donde existían call-sites internos (back-compat call-site sin tocar el cuerpo de las funciones que lo invocan). Beneficio futuro: F4H2 weapons + F4H6 enemies van a sumar `LuaBindings_Weapon.cpp` + `_Enemy.cpp` que pueden reusar el helper sin re-implementar. Si cambia la convención de tags (ej. tags duplicados con escenarios, fuzzy match) toca 1 archivo en lugar de N.

**Verificación:** suite full `1309/11931` verde post-refactors. Build verde MoodEditor + MoodPlayer.

**LOC residual sobre hard cap 800** (2 archivos restantes, ambos con razón documentada):
- `InspectorPanel_Environment.cpp` 1149 → F4H1.6 (plan dedicado).
- `SceneLoader.cpp` 819 (solo 1 LOC sobre cap, marginal) → F4H6 cuando enemy serialization lo empuje sobre 850.

**Backlog post-F4H1.5:**
- **F4H1.6** — Split `InspectorPanel_Environment.cpp` 1149 → 3 archivos por familia (Sky+Fog / PostFX (Tonemap+Bloom+SSAO+SSR) / Visual (Shadows+ColorGrading)). Plan necesita header `_Environment_Internal.h` con forward decls + helpers comunes (`kEnvDefaults`, `EditEnvironmentSubsetCommand`, `drawSectionResetButton`, `drawSectionDivider`) como inlines/templates.
- **F4H1.7** (opcional) — Audit hardcoded post-F3H29 si emerge demanda (extender F3H3 con valores agregados en sub-fases 3.3/3.4).

---

## 2026-05-29: F4H1 cierre — Sistema de salud/daño + maniquí de testing (cimiento combate Fase 4)

Primer hito de Fase 4. 3 decisiones cerradas vía AskUserQuestion + 1 decisión arquitectónica de la regla "nada hardcodeado" de Fase 3.

**D1 — Feedback visual al recibir daño: flash blanco breve (~80ms).** Convención Half-Life / Quake / Doom — el jugador ve impacto contundente per-hit. **F4H1 backend-only**: el `HealthComponent.hitFlashTimer` se setea correctamente (0.08s on-hit, 0.25s al morir) y el sistema lo decae cada tick — pero el shader render del flash queda agendizado al backlog (require edit de `pbr.frag` + uniform per-draw; el feedback del daño mientras tanto vive en Console log + Inspector + cae con física al morir). Implementable cuando emerja necesidad de polish visual (F4H5 game feel pass).

Alternativas descartadas:
- **Color shift gradual a rojo** — útil para combate largo pero no marca cada hit. Mala fit para hitscan donde cada disparo cuenta.
- **Ambos (flash + tinte gradual)** — más código, beneficio marginal en F4H1. Agendizable.

**D2 — Muerte: cae con física.** Convención FPS arcade. Implementación: el sistema Health detecta `dead && !RigidBodyComponent` y agrega Dynamic; en el próximo frame `EditorApplication::updateRigidBodies` (que ya existe) materializa el body Jolt y la gravedad actúa. Aprovecha infraestructura existente sin tocar physics.

Alternativas descartadas:
- **Desactivar invisible** — sin feedback, anti-satisfacción.
- **Fijo + cambio a gris** — útil para testing del Inspector pero rompe el feel de combate.
- **Ragdoll real** — agendizado a F4H9 (cuando enemigos sean humanoides riggeados). F4H1 = cubos simples.

**D3 — Spawn via "+ Crear Entidad" tab nueva "Gameplay" > Maniquí.** Convención del editor existente (modal pick con TabBar). Tab "Gameplay" creada para futuros items (enemigos de prueba F4H6+, pickups F4H3+).

Alternativas descartadas:
- **Botón temporal en MenuBar > Debug** — se vuelve scaffolding muerto en F4H2 cuando hay armas reales.
- **Solo Lua `scene.spawn_dummy(pos)`** — más fricción para iterar el feel del combate.

**D4 — Defaults en `Project Settings > Gameplay`, NO hardcoded.** Convención "nada hardcodeado" de Fase 3 explícita en `PLAN_FASE4.md §6`. `GameplaySettings` ya existía (F3H4 con walk/crouch/jump); F4H1 agrega `maxHealthDefault = 100.0f` con sanitize clamp `[1, 10000]`. Project Settings panel tab Gameplay gana 1 slider + tooltip i18n + reset.

**Backlog post-F4H1:**
- Shader flash render del `hitFlashTimer` (uniform `uHitFlash` per-draw en `pbr.frag`). Agendizable a F4H5 game feel pass.
- Inspector reset max usa default canónico 100 — wirear lectura live de `Project Settings.gameplay.maxHealthDefault` desde EditorUI (F4H1.5+).
- Event bus OnDamage / OnDeath para callbacks Lua per-entity.
- Sound on damage / sound on death (F4H5 game feel pass).
- Headshot multiplier (F4H2+ si el raycast retorna parte del cuerpo).
- Resistencias por tipo de daño (fuego, eléctrico, etc.) — solo si el diseño del juego lo pide.

---

## 2026-05-29: F3H31 cierre — Sky Atmosphere Hosek-Wilkie + GPU IBL bake runtime (Sub-fase 3.4 CIERRE REAL 12/12 🏁)

Duodécimo y último hito de Sub-fase 3.4 — re-cierre 12/12. Sub-fase 3.4 estaba cerrada 11/11 con F3H30 (texturas), pero el dev pidió retomar el stub original "HDRI dinámico + ciclo día/noche" con scope más ambicioso: *"quiero algo útil y profesional como el del Unreal"* + *"me gusta lo del bake en real time, en runtime, mientras sea procedural"*. **7 decisiones** + **4 ajustes reactivos** post-validación visual.

**D1 — Hosek-Wilkie 2012 (analítico) vs Hillaire 2020 (LUT-based).** Elegido HW por simplicidad de port + 95% del look. Hillaire requiere 4 LUTs precomputadas (transmittance, multiscattering, sky-view, aerial perspective) — overkill para use case actual de juego de pasillos HL1-style.

Alternativas descartadas:
- **Hillaire 2020 completo** — multi-scattering correcto + aerial perspective + paneles atmosféricos coherentes. Costo: 4 shaders LUT + tabla de transmittance bilineal + sky-view por frame. Use case del juego (arenas chicas) no lo aprovecha.
- **Preetham 1999** — predecesor de HW, más simple pero look menos pulido al horizonte. Si fuéramos por simplicidad iríamos directo a Preetham; HW es el punto dulce.

**D2 — GPU IBL bake runtime (no offline).** Pedido literal del dev. Trigger: cuando `EnvironmentComponent` con `skyboxSource=Procedural` cambia params (timeOfDay/turbidity/groundAlbedo), marca `skyDirty=true`. En `tickFrame`, si dirty + procedural → re-render sky cubemap + re-bake IBL (irradiance + prefilter). Auto, no manual.

Alternativas descartadas:
- **Bake offline a `.cubemap` files** — único pre-bake al inicio, sin runtime. Pierde el feature del time-of-day dinámico que es el norte del hito.
- **Bake runtime async/job-system** — complejidad de threading sin beneficio claro: el budget ~150ms es aceptable porque corre out-of-frame solo en transitions del slider, no every-frame.

**D3 — Stack: diharaw/sky-models (MIT) + escrito desde cero el IBL bake.** El IBL bake de Khronos `glTF-IBL-Sampler` (Apache-2.0) es monolítico y diseñado para Vulkan MRT — para nuestro stack OpenGL 4.3 con patrón face-by-face que ya existe, escribimos 2 shaders simples (irradiance.frag + prefilter.frag, ~80 LOC c/u) desde el paper de Epic UE4 "Real Shading" (concepto público, no copia de código NC).

**LearnOpenGL DESCARTADO** por licencia CC BY-NC (no comercial). Aunque el proyecto hoy es personal, la licencia NC contamina el header del shader si lo copiáramos — preferimos cero ambigüedad. El paper UE4 es enseñable, no licenciable: la implementación es nuestra.

Alternativas descartadas:
- **glTF-IBL-Sampler (Apache-2.0)** — bake correcto pero Vulkan multi-render-target; portar a OpenGL 4.3 cuesta más que escribir 2 shaders desde el paper.
- **LearnOpenGL IBL chapter** — código de referencia super claro pero CC BY-NC. Inutilizable.
- **Bake con CPU computado** — orders of magnitude más lento, descartado por D2.

**D4 — `skyboxSource` enum extiende `EnvironmentComponent`, NO reemplaza.** Back-compat: maps pre-F3H31 con `skyboxPath` siguen cargando como `skyboxSource=HDRI`. JSON `skybox_source` opcional con default `"hdri"`. `skyboxPath` se preserva (HDRI sigue siendo modo válido). El dev elige source via dropdown del Inspector; los params del otro modo se mantienen escondidos pero persistidos (toggle entre HDRI/Procedural NO destruye config).

Alternativas descartadas:
- **Reemplazar `skyboxPath` por un union** — rompe maps existentes + invalida feature HDRI presets cargados.
- **Componente separado `ProceduralSkyComponent`** — duplica el rol del Environment, dev no entiende cuál usa.

**D5 — Sun direction calculada del time-of-day, opt-in sync con LightComponent.** Arco N-S simple: `azimuth = π` (sur), `elevation = sin((timeOfDay - 6) * π / 12) * π/2`. Si hay un `LightComponent` directional con flag nuevo `bindToSky=true`, su `direction` se override del time-of-day. Sin lat/long astronómica real (over-engineering para juego de pasillos).

Alternativas descartadas:
- **Posición astronómica real con lat/long** — pedido sería "este nivel ocurre a 40°N en marzo 14:00 UTC". Use case marginal HL1-style.
- **Sun direction independiente de timeOfDay** — el dev configura el sol en un sitio y el time-of-day en otro; desincronización garantizada. El opt-in `bindToSky=true` da control si lo querés desacoplar.

**D6 — Cubemap procedural en GPU: 256×6 RGBA16F.** Match con resolución de los IBL prefilter mip0. Costo GPU: ~1.5MB por cubemap. Re-render ~3-5ms.

Alternativas descartadas:
- **128×6** — IBL prefilter mip0 también caería a 128 → irradiance pierde detalle en specular highlight.
- **512×6** — 4× más memoria + 4× más cost render sin mejora visible a la resolución de game pantalla.
- **RGBA32F** — float32 innecesario (rango HW estable con 16F + no hay overflow en sky luminance típica).

**D7 — Re-bake cost budget: ~150ms total.** Irradiance 32×32×6 + Prefilter 5 mips (128/64/32/16/8) × 6 faces × 1024 samples GGX = ~100-150ms en GPU mid-range. Re-bake corre OUT OF FRAME (no every-frame), solo en transitions de slider time-of-day.

Trade-off honesto: durante el drag del slider time-of-day en el Inspector, el editor puede stuttear ~150ms por update. Aceptado porque (a) no es path crítico (el dev tunea sky en sesión de iluminación, no en gameplay loop), (b) el feel es similar a Unity/Unreal cuando hacés bake de lighting, (c) la alternativa (re-bake cada frame mientras drageas) caería FPS a 6-10.

Alternativas descartadas:
- **Re-bake cada frame durante drag** — FPS unplayable.
- **Re-bake deferred hasta soltar slider** — UX raro (cielo no responde durante drag).
- **Bake incremental por mip** — complejidad de threading sin beneficio claro al budget.

---

**Ajustes reactivos post-validación visual.**

**(R1) Outline amarillo reflejado/blooming/oscurecido.** El dev reportó (con luz puntual + SSR/Bloom/AO activos) que el outline amarillo del cubo seleccionado aparecía "fantasma" reflejado en el piso (SSR), con halo (Bloom) y oscurecido (SSAO). Causa diagnosticada: `debugRenderer->flush()` (dibuja outlines/AABBs/gizmos) se ejecutaba al INICIO de `endFrame()` sobre el `m_sceneFb` HDR — los pases SSR/Bloom/SSAO leían ese FB con los overlays dentro, tratándolos como geometría legítima.

Fix: mover el flush al FINAL de `endFrame()` (post-tonemap) bindeando `m_viewportFb` LDR + blit del depth de `m_sceneFb` (para que el z-test del debug shader funcione vs la geometría real). `OpenGLFramebuffer` gana `GLuint glHandle()` getter para `glBlitFramebuffer` con bindings READ/DRAW separados.

**(R2) Dropdown "Origen del cielo" mostraba "????".** Bug de lifetime en C++ — `I18n::T(...).c_str()` devuelve puntero al `std::string` temporal; el array initializer `const char* sourceLabels[] = { I18n::T(...).c_str(), ... }` quedaba con punteros a memoria liberada apenas cerrado el bloque. **Idéntico al bug documentado en líneas ~779-790 del mismo archivo (`InspectorPanel_Environment.cpp`)** para el preview del LUT preset de Color Grading.

Fix: capturar `lblHdri` / `lblProc` como `std::string` locales con lifetime que cubre el `ImGui::Combo`. Patrón a sweep en el resto del Inspector si emergen "????" similares — el patrón `I18n::T(...).c_str()` en array initializer es fragile y debe evitarse.

**(R3) Header "Origen del cielo" vivía bajo "Niebla".** Tras fix R2, el dev pidió separar visualmente — convención Unity Lighting / Unreal Sky & Atmosphere: sky es su propia sección top-level del Environment, no sub-header de fog. Refactor `drawEnvSkyAndFog` → `drawEnvSky` + `drawEnvFog` con headers independientes.

Reset global por sección preservado, ahora resetea sky-only (skyboxSource + skyboxPath + 3 params procedural) o fog-only. Beneficio adicional: el dev puede resetear sky sin perder tuning del fog (caso común al iterar look).

**(R4) Mini-reset (↺) por slider individual.** Pedido del dev: *"falta los botones de reset al final de cada slider"* — patrón ya establecido en F3H30 UV reset (`InspectorPanel_Brush.cpp`). Helper lambda `skyResetFloat` / `skyResetVec3` inline en `drawEnvSky`: `SameLine + SmallButton(ICON_FA_ROTATE_LEFT)` que pushea `pushAtomicEdit<T>` con el default de `kEnvDefaults`. Cada mini-reset es undoable como cualquier edit manual; coexiste con el reset global de la sección (que sigue restaurando todos los fields a la vez).

**Backlog del hito (no cerrado en F3H31).**
- **F3H32** — Volumetric clouds raymarched (CaptainProton42 o portar Schneider HZD), ~15-18h.
- **F3H33** — Weather presets (clear/overcast/storm/sunset) + auto-cycle día/noche en gameplay runtime, ~6-10h.
- **Hillaire 2020 upgrade** — si emerge demanda de aerial perspective o atardeceres ultra-saturados, portar `JolifantoBambla/webgpu-sky-atmosphere`. Backlog.
- **Sun disc visible** — hoy el sol es solo punto de iluminación, no disco renderizado. Agregable con `step(cos(M_PI/360), dot(dir, sun_dir))` en el sky shader.
- **Stars de noche** — cuando sol Y<-0.1, fade-in de starfield. Hito propio.
- **Latitud/longitud astronómicas** — sun rota arco N-S simple. Lat/long real es backlog (use case marginal).

---

## 2026-05-29: F3H30 cierre — Texture pack HL1-style procedural + AssetBrowser recursivo + reset UV brush

Undécimo y último hito de Sub-fase 3.4 → cierre 11/11. El stub original era "HDRI dinámico + ciclo día/noche" pero el dev pivoteó tras F3H29 (*"creo que lo de las texturas es más rápido, vamos por eso"*) porque las 4 texturas legacy de Fase 1 (`brick.png`, `grid.png`, `missing.png`, `particle_fire.png`) no alcanzaban para construir un mapa real. **6 decisiones cerradas**.

**D1 — Set mínimo de 5 texturas (no 8).** Pedido del dev: *"necesito las necesarias para crear un mapa"*. Pensando como mapper Hammer/HL1, con 5 materiales se cubre 80% de cualquier mapa básico (interior + exterior). Set elegido: `concrete_wall` (paredes interiores #1), `concrete_floor` (piso interior, variante más clara), `dirt_ground` (exterior universal), `metal_panel` (puertas/paneles/detalles), `brick_old` (variación de pared, alt al concrete). Descartadas (wood_planks, gravel, tile_floor, lab_panel) son sustituibles por variantes — agregables como F3H31+ si emerge demanda.

Alternativa descartada:
- **8 texturas (set propuesto inicial)**: el dev fue explícito en mínimo viable; +3 más era scope creep para *"cerremos definitivamente"*.

**D2 — Stack Pillow + numpy puro (sin opensimplex, sin scipy).** El look HL1 se logra con: (a) ruido gaussiano cuantizado, (b) paleta indexed 16-32 colores, (c) patches manuales con `ImageDraw`. Pillow + numpy cubren todo — ambas ya disponibles en el entorno del repo (mismo stack que `gen_brick_texture.py` de F2H5). Sin deps nuevas.

Alternativa descartada:
- **opensimplex / scipy**: noise más sofisticado pero deps nuevas. Si emerge demanda de perlin/simplex/curl noise (materiales orgánicos tipo rock con vetas, mud con flow), agregar después.
- **Pack CC0 downscaleado** (Kenney, AmbientCG): el dev quería look HL1 indexed específico — los packs CC0 modernos son PBR realistas, no calzan out-of-the-box. Habría requerido pipeline de "downscale + cuantizar + recolorear" igual de costoso que generar from scratch.

**D3 — Sub-carpeta `assets/textures/library/` + AssetBrowser recursivo.** Las 4 legacy quedan en `assets/textures/` flat por compat con paths persistidos en .moodmap viejos (`textures/brick.png`). Las nuevas viven en `assets/textures/library/` → logicalPath `textures/library/concrete_wall.png`. AssetBrowserPanel migrado de `directory_iterator` a `recursive_directory_iterator` (mismo patrón que meshes desde F2H26). El `displayName` usa path relativo (`library/concrete_wall.png`) para distinguir sub-packs.

Alternativa descartada:
- **Flat en `assets/textures/`**: el dev tendría 9 PNGs mezclados sin agrupación. Sub-carpeta da semántica clara "esto es el pack base de F3H30 reproducible".

**D4 — Tileable via blend lineal de bordes opuestos.** En `tools/_texture_lib.py::make_tileable(img, blend=N)`: para cada par (`col[i]`, `col[w-1-i]`) y análogo filas, se calcula `mix = 0.5 * (1.0 - i/blend)` y se reemplazan ambos bordes por `pixel * (1-mix) + opuesto * mix`. En `i=0` (borde mismo) ambos pixels devuelven el promedio (matchan exactamente, costura invisible); en `i=blend` vuelven al original (`mix=0`). Función reusable para los 5 materiales con `blend` ajustable por material (concrete=24-32 suave, brick=6 nítido, metal=12 medio).

Alternativa descartada:
- **Offset + blend del Photoshop Offset Filter**: `np.roll(w/2, h/2)` mueve costuras al centro, smooth franja central, rotar de vuelta. Implementación inicial tenía bug en el blend (convex combination mal calculada con doble multiplicación). El approach actual es más simple, más robusto, y permite control fino del width del blend.
- **`np.tile` + crop centrado**: pierde variedad porque la imagen se vuelve simétrica.

**D5 — Reset buttons del UV brush undoable.** Pedido del dev: *"a la parte de UV le falta los botones de reset"*. Gap del F3H29 round-2 que solo agregó reset buttons (↺) a sliders PBR de materiales — los del editor UV del brush (uv scale / uv rotation / uv offset) quedaron sin ellos. Implementación: helper lambda `uvResetButton` inline en `InspectorPanel_Brush.cpp` (~25 LOC) que (i) captura `BrushUVSnapshot` pre, (ii) aplica default via `applyToScope` (`uvScale=(1.0, 1.0)`, `uvRotation=0.0`, `uvOffset=(0.0, 0.0)`), (iii) pushea `EditBrushUVCommand` con snapshot post (undoable como cualquier edit manual). Tooltip `editor.common.reset_default` reutilizado. El checkbox `lockToWorld` no lleva reset (boolean — destildar = "default").

Alternativa descartada:
- **`detail::inspectorResetButton<T>`** (helper genérico): asume target per-entity con setter `(Entity, T)`. El UV brush es per-face / multi-face con `applyToScope` (que maneja Object Mode vs Face Mode + N caras seleccionadas) — necesita lógica custom que el helper genérico no cubre. Crear un overload sería over-engineering para 3 invocaciones.

**D6 — Botón header AssetBrowser: "R" → ícono FA rotate.** Pedido del dev: *"el botón de recargar podemos cambiarlo por un botón de reload"*. Cambio puntual: `SmallButton("R")` → `SmallButton(ICON_FA_ROTATE "##reload_assets")`. Consistente con el resto del editor que usa iconos FA (gizmos, snap, reset). Tooltip preservado (`editor.panel.assets.reload_tooltip`). Misma macro que ya se usa en Toolbar, InspectorPanel_Environment, ShaderGraph rotate ops.

---

**Lección del cierre de Sub-fase 3.4**: la sub-fase arrancó (F3H20) con un audit grande "11 issues de UX del editor" y cerró 11 hitos después. **5 de los 11 hitos cerraron con scope expandido mid-hito** (F3H22 chasis Properties Editor expandido 3 veces; F3H26 polish UX absorbió 5 issues paralelos; F3H27 parenting con 4 reactivos post-validación; F3H28 grupos+tools cambió 4 decisiones por research previo; F3H29 expandido 2× — 14 decisiones finales). La regla "expandir solo si el dev pide explícitamente + razón temporal explícita + plan actualizado" del cierre F3H29 se sostuvo en F3H30 también (el pivote del stub HDRI→texturas fue pedido explícito, no inferencia mía).

Backlog que NO se llegó a cerrar en Sub-fase 3.4 (priorizado para Sub-fase 3.5 o Fase 4):
- **HDRI dinámico + ciclo día/noche** (stub original F3H30, pivoteado a texturas).
- **Reverse-Z infinito** (F3H29 D4).
- **OrthoCamera eye dist 1024 hardcoded** (F3H29 backlog).
- **Welcome modal title** "MoodEngine - bienvenida" ID interno mixto ES/EN.
- **Tooltips tuning fino import vehicle** (~10 keys de bajo impacto).
- **Migrar physics/picking/editor tools a `worldMatrixOf`** (F3H27 backlog).
- **Compound atomic delete** del padre + descendants en 1 undo step (F3H27 backlog).
- **Asset pack expansion** (F3H30 backlog): wood_planks, gravel, tile_floor, lab_panel + normal maps procedurales.

---

## 2026-05-29: F3H29 cierre — Camera limits + polish play mode + audit traducciones

Décimo hito de Sub-fase 3.4. Arrancó como split del item 3 del stub original F3H27 ("Mundo grande"). **Scope expandido mid-hito** tras validación visual del Play mode: el dev pidió integrar 3 polish issues no relacionados al far plane en el mismo hito (decisión literal *"perdemos tiempo si separamos en otro [hito]"*) en lugar de abrir F3H30 separado. 7 decisiones — 4 cerradas pre-implementación (D1-D4) + 3 reactivas post-validación (D5-D7).

**D1 — Storage de camera limits: `UserSettings::EditorSettings` (per-instalación), no `.moodproj > World`.** El dev seleccionó literalmente ".moodproj > World" en el AskUserQuestion inicial pero clarificó verbalmente *"es no por un tema de jugabilidad, solo de desarrollo, he testeado crear un brush enorme y no llego a verlo, o alejarme para trabajar en los detalles"*. La justificación apunta a **preferencia ergonómica del dev** (per-instalación), no decisión arquitectónica del proyecto. Implementación efectiva: 2 fields nuevos en `EditorSettings` (`editorCameraFarPlane`, `editorCameraMaxOrbitRadius`).

Alternativas descartadas:
- **`.moodproj > World` (per-proyecto)**: tendría sentido si la escala del mundo fuera decisión arquitectónica del proyecto (un FPS de pasillos vs open-world). Pero el dev fue explícito en que no — es preferencia del dev (cuánto quiere ver), no del proyecto.

**D2 — Defaults conservadores 10×.** `editorCameraFarPlane = 1000.0f` (era 100), `editorCameraMaxOrbitRadius = 500.0f` (era 50). Cubre mapas urbanos / arenas medianas estilo HL2 (500-1000m). Z-precision OK con 24-bit depth buffer + near=0.1 (granularidad ~10cm a 1km). Compatible con engines sin reverse-Z.

Alternativas descartadas:
- **Agresivo 100× (far=10000, orbit=5000)**: cubre open-world GTA-style. Z-precision degrada cerca del far plane (z-fighting > 5km). El dev eligió conservador.
- **Extremo 1000× (far=100km)**: flight-sim. Fuera del scope MoodEngine.

**D3 — `CameraComponent::farPlane` default sube a 1000m también (no solo el EditorCamera).** Coherencia visual: lo que ves moviendo la cámara del editor = lo que ves por una cámara gameplay nueva sin tunear. El campo sigue editable en el Inspector (slider 1-10000). Override por entity preservado via `value("farPlane", 1000.0f)` en el parse — proyectos pre-F3H29 que persistieron `farPlane=100` lo mantienen.

Alternativas descartadas:
- **`gameCameraFarPlane` separado**: campo distinto del editor far. Más complejo de mantener en sync, valor agregado marginal.
- **No tocar (default 100m queda)**: cada gameplay camera se ajusta a mano. Default desfasado del workflow del editor.

**D4 — Reverse-Z infinito: out-of-scope.** Reverse-Z resuelve precision Z a cualquier distancia (perfecta en near, degradada-pero-aceptable hacia far). Requiere tocar TODOS los shaders (depth comparisons `< → >`), pipeline state (`glDepthFunc`, `glClearDepth`, `glDepthRange`), debug overlays, shadow passes. ~3-4 días de trabajo, hito propio si emerge demanda (mapas ≥ 5km con z-fighting reportado). F3H29 resuelve 90% del bug con D2 sin tocar shaders.

**D5 — Overlays del editor ocultos en Play mode (REACTIVA).** Detectado durante validación visual del Play mode: la barra cyan de viewport render modes (top-right), el tools overlay (top-left), y el snap status bar seguían visibles en Play mode pese a que el render del Play ignora el `viewportRenderMode` (decisión F3H21/F3H22 — Play=MaterialPreview-equivalente para game-feel real). Pedido literal del dev: *"en play mode no debe mostrar nada externo solo el HUD del juego, los modos de viewport solo en el editor no en play mode"*. Implementación: en `ViewportPanel::onImGuiRender`, las 3 sub-windows skipean su render si `m_editorUi->mode() == EditorMode::Play`.

Alternativas descartadas:
- **Disable + grayed-out**: dejar los botones visibles pero deshabilitados. El dev fue explícito en que NADA externo debe verse en Play. Hide gana.
- **Cambiar el shading mode del Play para respetar la barra**: rompería el "test del game-feel real" — el dev quiere que Play se vea como el juego empacado, no como el preview del editor.

**D6 — Botón cerrar explícito en popovers de configuración (REACTIVA).** Pedido del dev: *"falta el botón de cerrar"* en el popover de Ajustes del snap. ImGui `BeginPopup` cierra solo con click-afuera por default — el dev espera el patrón estándar de ventana con "X" arriba derecha. Implementación: `SmallButton("X")` + `CloseCurrentPopup()` en el header del popover (`InspectorPanel_MapTools.cpp:146-167`). Pattern reutilizable si emergen más popovers con scroll o contenido extenso.

Alternativas descartadas:
- **Solo "click afuera"** (default ImGui): no es obvio para devs que vienen de Hammer/Blender (que sí tienen X explícita en popovers persistentes).

**D7 — Keys i18n compartidas para acciones comunes (REACTIVA).** Audit detectó 25+ strings hardcoded en español/inglés + un valor ES en inglés en `es.json`. Para no terminar con `editor.foo.cancel` / `editor.bar.cancel` / `editor.baz.cancel`, se introdujo namespace `editor.modal.common.*` con 8 keys reutilizables (`cancel`, `save`, `save_as`, `delete`, `edit`, `new`, `no_scene`, `no_scene_assets`). Fija el vocabulario UI del editor (Guardar/Cancelar/Eliminar/Editar/Nuevo) en un solo lugar; cambiar la traducción de "Cancelar" toca 1 string, no N. Para strings no-comunes (tooltips de tuning vehicular, mensajes de error contextuales) se crearon namespaces específicos (`editor.shader_graph.*`, `editor.inspector.vehicle.*`, `editor.import_vehicle.*`).

Alternativas descartadas:
- **Una key por sitio de uso**: lleva a inconsistencias ("Cancelar" en un sitio, "Anular" en otro, "Cancela" en un tercero). El namespace común previene el drift.
- **Migrar TODOS los hardcoded restantes** (incluyendo los ~10 tooltips de tuning fino de import vehicle): bajo impacto (solo se ven al importar un .glb), backlog.

Lección: cuando una validación visual detecta polish issues no relacionados al núcleo del hito, expandir el hito en lugar de abrir uno nuevo SI el dev lo pide explícitamente y el alcance se mantiene contenido. Si supera ~5 issues, abrir hito polish dedicado. Esta vez F3H29 cerró con 1 feature core + 3 polish + 1 audit — sigue contenido.

**(Edit post-cierre 2026-05-29): la lección no aguantó la prueba.** Después de cerrar D7 hubo **un segundo round de validación visual** del Inspector que abrió 7 nuevas issues (D8-D14). El dev volvió a decir *"perdemos tiempo si separamos en otro [hito]"*. Resultado: F3H29 final = 1 feature core + 13 polish/bugfixes. **Lección revisada**: cuando el dev pide expansión explícita, expandir. La regla "≤5 issues" no es regla — es estimación inicial. Lo importante es que cada expansión venga con: (a) pedido literal del dev, (b) razón temporal explícita (vs. dejarlo flotando), (c) actualización inmediata del plan. Si el hito termina con 14 decisiones eso es OK; lo que NO es OK es expandir sin actualizar docs en el camino.

---

## 2026-05-29: F3H29 polish round-2 — Inspector UX Blender-style + sweep textos + bug fixes brush + mundo vacío + texture drop sin tile-grid

Segunda expansión de scope F3H29 tras un segundo round de validación visual del Inspector. 7 decisiones nuevas (D8-D14). El dev volvió a pedir *"perdemos tiempo si separamos en otro [hito]"*.

**D8 — Inspector UX Blender-style.** Pedido del dev: *"en object esta agregar component, en render tambien, confunde... la parte de materiales me parece muy confusa entre mesh y brushes, no se enseñan bien, veo mas texto que lugares de edicion, te dije que me gustaria que sea mas como blender, donde agrupa los materiales en una lista y cada uno tiene su nivel de edicion"*. Cambios:
- **`+ Agregar Componente` solo en categoría Object** (no se repite en Render/Materials/etc).
- **Materiales unificados Blender-style** (MeshRenderer + Brush): ListBox vertical de slots + panel de edición del slot seleccionado. Helpers compartidos `drawPbrMultipliers/drawShaderGraph/drawBlending` en archivo nuevo `InspectorPanel_Materials.cpp/.h`.
- **Colapsables defaults**: Surface/Shader/Blending/UV/Info colapsados por default (antes abiertos = ruido visual).
- **Reset buttons (↺)**: en cada slider PBR.
- **Menos texto**: removidos status hints `albedo:0 MR:0`, `(graphs en assets/shaders/graphs/...)`, IOR presets, `Slot N — material asignado`, `(id N)`, `UV (Brush)`.
- **Info colapsable del brush eliminado** (dev: *"eso me parece innecesario"*).

Alternativas descartadas:
- **Mantener UI separada MeshRenderer vs Brush**: redundancia visual + mantenimiento doble. Unificación con helpers compartidos ahorra ambos.
- **Dejar colapsables abiertos por default**: el dev fue explícito *"mientras haya menos texto posible mejor"* — colapsado por default empuja al usuario a abrir solo lo que necesita.

**D9 — Sweep textos colgados.** Pedido del dev: *"ahora quiero que vayas por cada panel y revises si hay textos asi de molestos e innecesarios y los elimines"*. Cleanup:
- `InspectorPanel_Environment.cpp`: 3 hints debug (skybox_hint, csm_hint, ssr_hint).
- `InspectorPanel_Physics.cpp`: body_id_hint (RigidBody) + ragdoll state_hint.
- `InspectorPanel_Joint.cpp`: fixed_help (párrafo informativo) + constraint_id_hint.
- `UserPreferencesPanel.cpp`: 4× live_apply_hint duplicado (`"Los cambios se aplican al soltar el slider"` por sub-sección).

Criterio mantenido: status text **funcional** se preserva (alive_count en particles, no_scene errors). Lo que se borra es text **decorativo o debug** (IDs internos, párrafos explicativos que duplican docs).

**D10 — Persistencia materiales Brush + Undo delete brush (bug fixes).** Reportados por el dev: *"le acabo de poner 2 materiales a un brush, guardé, cerré el proyecto y volví a abrir no persistió los materiales / borré un brush y no apreté ctrl+z y no volvió"*.

Bug 1 — persistencia rota: `BrushComponent.materials` guarda material wrappers con path `__runtime_tex#N` (cuando el dev dropea una textura en una cara, se crea un wrapper material in-memory que apunta a la textura). Al serializar, ese path se guardaba como-is. Al reload, `loadMaterial("__runtime_tex#42")` no resuelve (el path no es un .material en disco) → wrapper desaparece. Fix:
- `SceneSerializer::serializeBrush` ahora resuelve paths runtime al path real de la textura del wrapper (`MaterialAsset.albedo`).
- `SceneLoader::applyBrushFromSaved` detecta si el path termina en `.png/.jpg/.jpeg/.tga/.dds` → usa `loadTexture + createMaterialFromTexture`; si termina en `.material` → usa `loadMaterial`.

Bug 2 — undo delete brush vacío: `serializeEntityToJson` skipea brushes porque la serialización viaja por `serializeBrush` separado (BrushComponent no entra en el JSON estándar de entity). `DeleteEntityCommand` capturaba un SavedEntity sin info de BrushComponent → al undo, el brush volvía como un Empty sin geometría. Fix: capturar también un `SavedBrush` paralelo en el ctor (`serializeBrush + parseBrush`) + aplicar via `SceneLoader::applyBrushFromSaved` en `undo()`.

Alternativas descartadas:
- **Unificar serialización en `serializeEntityToJson` y dropear `serializeBrush`**: refactor grande, riesgo alto, no necesario para fix puntual.
- **Persistir paths runtime como-is + resolver en load**: opaco, el .moodmap quedaría con paths que no significan nada fuera del editor.

**D11 — Material Preview sin fog.** Reportado por el dev: *"porque en material preview si alejo la camara comienza a ponerse blanco?"*. Root cause: la fog se computa en el lighting pass (uniform `uFogMode` aplicado por fragment shader cuando el pixel pasa lighting), NO en post-process. Por lo tanto `m_skipPostPasses = true` (que apaga bloom/SSR/etc en Material/Solid/Wireframe modes, F3H22) NO desactivaba la fog. El "blanco a la distancia" era fog en el uniform. Fix: `uFogMode = m_skipPostPasses ? 0 : static_cast<int>(m_fog.mode)` en `SceneRenderer_Render_Lighting.cpp`. Fog visible **solo** en modo Rendered (que es lo que pidió el dev: *"el fog es cosa solo del render preview"*).

Alternativa descartada:
- **Mover fog a un post-process stage**: refactor del pipeline, riesgo alto, valor agregado nulo (el comportamiento actual con el fix ya es correcto).

**D12 — Environment singleton auto-recover (Blender World Properties pattern).** Pedido del dev: *"porque otra vez dice que debo agregar el componente enviroment? no se supone que venía incrustado automáticamente... COMO BLENDER, para que debería agregar el componente si es obvio que al final del día lo agregarán debe estar implícito"*. Cambios:
- `handleNewProject` ahora llama `ensureEnvironmentExists()` después de `rebuildSceneFromMap()` (gap del flow nuevo-proyecto: el Environment se generaba al cargar un .moodmap pero no al crear uno desde cero).
- **Inspector auto-recover**: si el dev entra a categoría 🌍 Environment y el singleton no existe, se auto-crea silenciosamente — request/consume pattern (`EditorUI::requestEnsureEnvironment()` + `EditorApplication_Run::pumpUiRequests()` lo consume con `ensureEnvironmentExists()`).

Cubre: proyectos pre-F3H22 sin Environment serializado, dev hace delete del singleton (Ctrl+Z no aplica al singleton), scene reset. Pattern Blender World Properties: no se puede "no tener world".

Alternativa descartada:
- **Mostrar botón "Crear Environment" en el Inspector** (lo que existía pre-D12): obvio que el dev lo va a apretar siempre → fricción innecesaria. Auto-crear es Pareto-correcto.

**D13 — Mundo vacío default (Floor entity eliminado).** Pedido del dev: *"este es el mesh que se importa automáticamente en cada mapa por defecto, en los programas industriales, comienza el mundo vacío no?"* (con screenshot de Floor 8×8 grid tablero). Pre-F3H29 cada mapa nuevo arrancaba con un cubo Floor 12×0.1×12m con `grid.png` como albedo simulando un tablero de tiles. Fix: bloque de generación del Floor eliminado de `EditorScene::rebuildSceneFromMap`. Industria estándar: Unity / Unreal / Hammer / Godot arrancan sin floor pre-spawneado — el dev arma su piso con un Box Brush.

Trade-off documentado: en Play mode sin piso los objetos físicos caen al vacío — comportamiento estándar de los engines mencionados. El grid del viewport (helper visual) sigue presente, así que el dev tiene referencia espacial.

Alternativa descartada:
- **Mantener Floor opcional via UserSettings**: setting más, friction más. Si el dev quiere un piso por default, lo crea con un Box Brush + lo persiste en su template de proyecto.

**D14 — Texture drop sin tile-grid legacy.** Reportado por el dev: *"yo arrastro esta textura, pero paso de cierto límite, y se desaparece es como que el tile no tiene lugar, las texturas no deberían ser cubos, deberían ser opciones válidas para a futuro construir algo usando brushes"*. Root cause: `processViewportTextureDrop` tenía un fallback a `pickTile` del tile-grid legacy de Fase 1 — si la textura no caía sobre un brush, pintaba un cubo-pared (`SetTileCommand` con `TileType::SolidWall`). Si pasaba el límite del grid, `pickTile` no hacía hit y la textura "desaparecía". Fix:
- Eliminado fallback al tile-grid legacy + include de `SetTileCommand` huérfano.
- **Drop sobre MeshRenderer (nuevo)**: la textura se aplica al slot 0 del mesh via wrapper material — simétrico al `processViewportMaterialDrop`. Cubre el caso natural "tirar textura a un `.glb`".
- **Drop sobre nada**: log silencioso (`"Drop textura id={}: sin brush/mesh bajo el cursor — drop ignorado"`), no se crea entidad.

Las primitivas del **modal de entidades** (Cube/Sphere/Plane/etc) siguen spawneables — el dev clarificó: *"las primitivas están bien"*.

Alternativa descartada:
- **Mantener el tile-grid como fallback opcional**: legacy de Fase 1 (gameplay 2D-grid retro). Hoy MoodEngine es 3D-first, el tile-grid sigue solo por compat con mapas viejos. El comportamiento de "pintar tile-cubo desde el browser" no se alinea con el modelo mental del dev (texturas = recursos para construir, no entidades).

---

## 2026-05-29: F3H28 cierre — Grupos + Map Tools como categorías del Properties Editor

Octava decisión: integrar Empty Group_<N> de F3H27 + reemplazar MapEditorTopBar por categorías del Inspector chasis F3H22. Hallazgo crítico pre-implementación cambió scope: el stub asumía paneles flotantes "Grupos" (en realidad VisGroupsPanel/F2H33, sistema visibility-toggle) y "Map Tools" (en realidad MapEditorTopBar/F2H30, toolbar como panel ImGui-dockable), y "toolbar lateral persistente" que no existía. 8 decisiones efectivas — 4 pre-implementación vía AskUserQuestion + 4 reactivas (post-research / mid-implementación).

**D1 — A1+A2: "toolbar lateral" + "top bar separada" no existen como entidades distintas.** El stub asumía 2 superficies pero el research reveló que ambas se mapeaban al mismo `MapEditorTopBar` (panel ImGui-dockable que aparecía como tab "Map Tools" en el dock derecho del workspace map_editor). Decisión: las preguntas A1 y A2 del stub se respondieron implícitamente al confirmar D5 (eliminar MapEditorTopBar). Atajos teclado (1/2/3, V, W/E/R, Ctrl+G) preservados intactos — eran ortogonales al panel.

Alternativas descartadas:
- **Inventar una "toolbar lateral" nueva** desde cero — scope inflation. El dev confirmó que su mental model era "los maptools que estén más insertados en menús" — sin demanda explícita de tener AMBOS (toolbar + categoría).
- **Mantener MapEditorTopBar como redundancia** — decisión C1 lo descartó: eliminar.

**D2 — A3: Boolean ops quedan SOLO en context menu del Outliner (no migrar a categoría Map Tools).** Confirmado vía AskUserQuestion: el dev validó esa ubicación en F3H26 ("me gusta como esta"). Migrar a la categoría agrega redundancia sem ántica (context = "sobre el seleccionado", categoría = "global del editor") sin valor agregado. Pattern Blender: las Modifier Stack operan sobre el activo, no en una categoría global de "tools".

Alternativas descartadas:
- **Duplicado en ambos lugares** (Blender modifiers pattern). Más descubrible para devs nuevos pero ruido para devs experimentados que ya saben dónde está.
- **Migrar exclusivamente a categoría** (eliminar del context menu). Pierde el acceso contextual rápido del right-click.

**D3 — B1: Backend de categoría "Grupos" = Empty-as-parent de F3H27 (no sistema paralelo).** Reusar lo implementado: lista de Empty Group_<N> del mapa con descendants, acciones via `GroupSelectionCommand` / `UngroupSelectionCommand` existentes. Cero código nuevo de backend; UI puro. Memoria `feedback_no_reinventar_rueda` aplicada. Implica B2 (categoría lista TODOS los Empty con descendants — mismo filtro que el marker XYZ del overlay F3H27 R7) y B3 (NO multi-membership porque Empty es single-parent por definición).

Alternativas descartadas:
- **VisGroup paralelo (Source SDK style)** — visibility-toggle sin parent transform. Útil para "esconder todo el 2do piso" sin mover. Backlog si emerge demanda (hito propio).
- **Híbrido: Empty + flag VisGroup** — agregar bit de visibility al Empty. Combina ambos pero rompe la separación de conceptos.

**D4 — C1: MapEditorTopBar ELIMINADO (no ocultar).** Decisión reactiva del dev tras research que aclaró la confusión semántica: el panel "Map Tools" del dock NO era una toolbar fija de F2H30 — era un panel flotante completo. Decisión: borrar archivos `MapEditorTopBar.h/cpp`, quitar del registry de `EditorUI`, quitar de `CMakeLists.txt`. Para no perder funcionalidad: el popover `drawSnapPopoverContent` (F3H6) se extrae a `SnapPopoverContent.{h,cpp}` como helper compartido reusable + la categoría Map Tools incluye Clip tool + botón Ajustes con el popover migrado.

Alternativas descartadas:
- **Ocultar por default (hiddenByDefault)** — el panel queda como código muerto. El dev fue explícito: eliminar.
- **Mantener ambos visibles** (coexistencia indefinida) — el dev quiere reducir clutter, no agregar.

**D5 — C2: VisGroupsPanel "Grupos" → "Visibilidad" (rename, no eliminar).** Decisión reactiva tras la colisión de nombres descubierta: VisGroupsPanel y la nueva categoría del Inspector ambos se llamaban "Grupos" pero son conceptos distintos (VisGroup = visibility-toggle de capas; Group = Empty-as-parent transform). Solución mínima: cambiar el `name() const` del VisGroupsPanel a "Visibilidad". Backend de VisGroups (F2H33) intacto — solo el label visible cambia.

Alternativas descartadas:
- **Ocultar también el VisGroupsPanel** — perderíamos la UI del sistema VisGroups sin reemplazo. El dev rechazó la opción.
- **Migrar VisGroups también a categoría del Inspector** — scope inflation (3 categorías nuevas en un hito). Queda como backlog si emerge demanda.

**D6 — D1: Workspace map_editor reorganizado, Viewport 3D en top-left.** Pedido reactivo del dev mid-implementación: *"en el editor de mapas, me gustaría que el 3D este primero, de todos"*. Pre-F3H28 el Viewport 3D estaba en top-right (top-left era el orto Top XZ, convención Hammer). Cambio mínimo: intercambiar Viewport ↔ Top XZ; ortos Front+Side quedan abajo. Adicionalmente: el `dockRightBar` (10% → 22%) ahora aloja Inspector + Visibilidad como tabs (antes eran "Map Tools" + "Grupos" que ya no existen como nombres). Inspector pasa de oculto-by-default a visible en este workspace para que las nuevas categorías sean accesibles.

Implica bump `imgui_layout_v8.ini` → `v9.ini` (patrón establecido del proyecto cuando cambia el dock layout de un workspace).

Alternativas descartadas:
- **3D protagonista grande, ortos chicos a la derecha** — más espacio para el 3D pero el dev eligió cambio mínimo (mantener 2x2).
- **Solo el 3D con ortos como tabs apilables** — pierde vista simultánea de los 3 ortos (anti Hammer-style).
- **No tocar el layout** — pero el dev pidió explícito que el 3D esté primero.

**D7 — Sin tests UI nuevos.** Las 2 categorías son UI puro sobre comandos backend ya testeados (F3H27 tiene tests de `GroupSelectionCommand` / `UngroupSelectionCommand` / `SetParentCommand` + parenting state). El chasis F3H22 del Inspector no tiene tests UI directos por convención del proyecto. Suite **1283/11841 verde** post-F3H28 confirma sin regresión.

Alternativas descartadas:
- **Tests de integración via ImGui test engine** — overhead alto, F3H22 no los tiene, no agregar como precedente sin demanda explícita.
- **Tests del state de la categoría activa** (UserSettings.editor.inspectorActiveCategory acepta "groups"/"maptools") — el campo es un std::string libre; agregar tests del enum value sería over-engineering.

**D8 — Backend de requests group/ungroup sigue el patrón `requestX/consumeX`.** Decisión arquitectónica menor: en lugar de que la categoría Inspector construya el `GroupSelectionCommand` y lo empuje al history directamente (patrón VisGroupsPanel), agregamos 2 nuevos request/consume en EditorUI (`requestGroupSelection` / `requestUngroupSelection`). Razón: paralelismo con `requestToggleSnapToVertex` / `requestCarve` / etc. El handler en `EditorApplication_Run.cpp` ya tiene el contexto (Scene + AssetManager + SelectionSet + history) — más fácil delegar a `groupSelectedEntities()` que duplicar la lógica de `topLevelAncestors` + `GroupSelectionCommand` ctor en el panel.

Alternativas descartadas:
- **Panel construye el command directo** (patrón VisGroupsPanel). Funciona pero duplica la lógica del filtro top-level. El handler de EditorApplication la centraliza.
- **Atajo de teclado expone un slot que el panel emite** — más complejidad sin upside.

---

## 2026-05-28: F3H27 cierre — Parenting jerárquico de transforms

Splitted del stub original F3H27 (4 items → 4 hitos separados): F3H27 hoy = parenting-only; F3H28-F3H30 = Grupos+Tools / Mundo grande / HDRI dinámico. 4 decisiones pre-implementación + 5 ajustes reactivos post-validación visual.

**D1 — Delete sobre padre = cascada destructiva (vs detach hijos + borrar solo padre).** Convención Unity/Unreal/Blender: borrar padre borra hijos. Mental model "scene graph" — un sub-tree es una unidad lógica del nivel. Alternativa "detach + borrar padre" rompe el principio de menor sorpresa: el dev espera que Delete sobre "el edificio" borre el edificio entero, no que aparezcan 6 cubos sueltos en root del Outliner. Implementación simple: N `DeleteEntityCommand` individuales (children → parent, DFS reverse order) pushed al `m_history`. Trade-off honesto: Ctrl+Z deshace de a uno (el dev hace N undos para recuperar el sub-tree completo). Compound atomic delete (1 comando con snapshot de todo el sub-tree) queda en backlog F3H28+.

Alternativas descartadas:
- **Detach + borrar solo padre** — sorpresa. El edificio no se va, solo se descompone visualmente. Anti-mental-model.
- **Modal de confirmación "borrar N hijos también?"** — interrumpe el flow del level designer. Unity/Unreal no lo hacen.
- **Compound atomic delete en F3H27** — scope inflation. El dev cierra ya parenting básico, compound delete = optimización si emerge demanda.

**D2 — Multi-select gizmo con padre + hijo selected: filtro top-level (hijos se ignoran).** Convención Blender/Maya/Unity. Si el dev seleccionó padre + hijo y mueve el gizmo, aplicar delta a ambos por separado duplica el movimiento del hijo (recibiría delta del padre — que la jerarquía propaga — + delta propio aplicado por el gizmo). Filtro: `Scene::topLevelAncestors(selected)` devuelve solo entities cuyo ancestor NO está también en el set. Aplicado en `EditorOverlay_Gizmo::populateOtherStarts` para que los "otros" del multi-drag (todos menos el active) skipen hijos cuyo padre/abuelo está selected.

Edge case documentado: si el `active` (primary del SelectionSet) es UN HIJO con su padre también selected, el active igual recibe delta del gizmo y el padre lo propaga → doble movimiento del hijo. Aceptado por simplicidad. Si emerge demanda, filtrar también el active.

Alternativas descartadas:
- **No filtrar (cada selected recibe delta)** — duplica movimiento de hijos. Bug visible.
- **Detectar conflicto y modal "querías mover solo padre?"** — interrumpe. Ningún editor lo hace.
- **Filtrar al construir el SelectionSet** — pierde la información de qué seleccionó el dev. El SelectionSet es UX-puro; filtros van en los consumidores (gizmo, comandos).

**D3 — Posición del Empty al agrupar: centroide del AABB combinado.** Calculado vía `brushAabbWorld` (brushes) / `meshAabbWorld` (meshes) / pivot position (point entities como Light/Audio). Es lo que hace Blender (Object > Set Origin > Origin to Geometry, default al crear empty parent) y Unity (Create Empty Parent al seleccionar N — Unity 2020+).

Alternativas descartadas:
- **Origen del mundo (0,0,0)** — disruptivo. Si los hijos estaban a (100, 0, 0), tras agrupar quedan con offsets enormes; mover el padre = los hijos se separan del world origin. Mental model roto.
- **Pivot del primer seleccionado** — sesga hacia el orden de selección. Si Shift+click en orden A, B, C, el centroide queda en A — no es predictible.
- **Centroide de pivots (no AABB)** — sesga hacia objetos con pivot off-center. Un brush con pivot en una esquina daría centroide mal posicionado vs la "masa visual" del set.

**D4 — Serialización: `parent_tag` (string) vs `parent_handle` (u32 raw del registry).** Tags son estables entre saves; handles del `entt::registry` cambian arbitrariamente (orden de creación, undo/redo que recrea entities con handles distintos, rebuildSceneFromMap). Patrón gemelo a F2H65 (Joint targetEntity usa tag, no handle).

2-pass resolve en `SceneLoader::applyEntitiesToScene` post-Joint resolution: itera saved entities → busca child por tag → busca parent por tag → setea `tc.parent`. Si el padre fue borrado entre saves (parent_tag apunta a tag inexistente), child queda root + log warn. Si hay 2 entities con el mismo tag (caso patológico), first match wins (mismo patrón que el resolve del Joint).

Alternativas descartadas:
- **`parent_handle: u32`** — handles cambian entre saves. Bug garantizado.
- **UUID per-entity** — overkill para un campo que ya tenemos (tag). Schema bump, migration de proyectos viejos, no aporta nada nuevo sobre tag.
- **Path-like "/Group_1/Tile_4_5"** — si renombrás el padre, todos los paths quedan stale. Tag es 1 reescritura; path-like serían N (cubierto por F3H19 rename pero más fricción).

---

**Ajustes reactivos post-validación visual.**

**(R1) Icon del Group en Outliner — tofu `?`.** Primer intento usaba `ICON_FA_OBJECT_GROUP` (0xF247, "object mode" en FA6 — icon canónico para grupos). El dev reportó tofu `?` al ver el `Group_1` en el Outliner. Investigamos: el range del atlas FA es `0xE005-0xF8FF` (cubre 0xF247), pero el TTF `fa-solid-900.ttf` del proyecto no rasteriza ese glyph específico (subset free solid posiblemente reducido). Fallback final: `ICON_FA_FOLDER` (📁, 0xF07B) — garantizado en el atlas porque lo usa el MenuBar "Archivo" todos los días. Semántica "carpeta = contenedor de hijos" es Unity GameObject empty / Hammer group estándar. Trade-off: el icon no comunica "agrupación de transforms" tan literalmente como object-group, pero al ser un glyph confirmado, evita tofu y mantiene la semántica honesta.

**(R2) Arrows ▶/▼ del expand/collapse — tofu `?`.** Usábamos `\xE2\x96\xB6` (U+25B6 BLACK RIGHT-POINTING TRIANGLE) + `\xE2\x96\xBC` (U+25BC BLACK DOWN-POINTING TRIANGLE). El font Lato del proyecto cubre Basic Latin + Latin-1 + General Punctuation (0x2010-0x2027), NO Geometric Shapes (0x25A0-0x25FF). Tofu garantizado. Fix: macros nuevas `ICON_FA_CARET_DOWN` (0xF0D7, "\xef\x83\x97") + `ICON_FA_CARET_RIGHT` (0xF0DA, "\xef\x83\x9a") en `IconsFontAwesome6.h` + usar en `HierarchyPanel.cpp`. Convención Hammer/Unreal/Maya para tree expand: caret triangular sólido. Beneficio adicional: caret es visualmente más liviano que black triangle, encaja mejor con el resto de los icons FA del Outliner.

**(R3) Outline del Group no envolvía a los hijos.** Al seleccionar el Empty `Group_1`, el outline dibujaba un cubito de 0.5m centrado en el centroide (point marker fallback para entities sin BrushComponent/MeshRenderer). El dev pidió que envolviera ambos cubos hijos (mental model "selecciono el grupo, veo el grupo entero highlighteado"). Fix en `EditorRenderPass_Overlay.cpp::drawEditorScene3DOverlay`: si la entity selected tiene descendants (`Scene::descendantsOf` no vacío), computar el AABB axis-aligned combinado de su geometría propia + la de todos sus descendientes vía helper local `computeOwnGeomAabbWorld(handle)` (transforma el AABB local del brush/mesh por `worldMatrixOf` recursivo y proyecta los 8 corners a world axis-aligned). Sin descendants → OBB orientado original (mantiene el comportamiento pre-F3H27 para meshes rotados, donde el OBB es visualmente más informativo que un AABB envolvente).

Trade-off: para meshes rotados con hijos, el outline pasa de OBB orientado (visualmente preciso al objeto) a AABB axis-aligned (envuelve más espacio del estrictamente necesario). Aceptado porque cuando hay hijos el dev típicamente está manipulando "el grupo entero" como unidad, no inspeccionando la orientación del padre.

**(R4) Persistencia rota: Group_1 + tiles agrupados no se guardaban.** Después de Ctrl+G → save → reopen, el Group_1 desaparecía y los Tile_4_5/Tile_4_2 volvían a ser roots. Dos bugs encadenados:

1. `SceneSerializer.cpp:256-258` filtraba entities que no tuvieran ≥1 componente "serializable" (MeshRenderer/Light/RigidBody/Environment/Script/ParticleEmitter/Inventory/Vehicle/ForceField/Trigger/Cloth). El Empty `Group_1` (solo Tag + Transform por createEntity default) cae fuera del filtro → no se escribe al `.moodmap`.

2. `TilePersistence::isTileModified` no chequeaba `tc.parent`. Tiles agrupados con scale (1m, 1m, 1m) + material default (textura del grid) se consideraban "no modificados" → no se persisten → al cargar, `rebuildSceneFromMap` los regenera del grid como roots SIN parent → 3-pass resolve del parent_tag falla porque los tiles SÍ tienen parent_tag en el .moodmap (de cuando se guardó la subtree), pero el resolve busca tag en `saved.entities` — y los tiles no están ahí.

Fix dual:
- En `SceneSerializer`, post checks de componentes "serializables", agregar check `isParent` via `mutableScene->registry().view<TransformComponent>().each([&](h, tc){ if (tc.parent == e.handle()) isParent = true; })`. Persistir si `isParent` aunque no tenga componentes propios. Empty Group_1 ahora se persiste cuando es referenciado.
- En `TilePersistence::isTileModified`, agregar `if (tile.getComponent<TransformComponent>().parent != entt::null) return true`. Cualquier parent (Empty grupo o otro tile) marca el tile como modificado y fuerza su persistencia con position local + parent_tag.

Cobertura del fix: agrupar tiles default (caso del bug original), agrupar mezcla tiles + meshes (los meshes ya se persisten por filtro de MeshRenderer), agrupar mezcla con luces / triggers (todos cubiertos).

**(R5) Modal welcome decía "Versión 2.25.0 — Fase 3 cerrada".** Mientras debugueaba persist, el dev pidió sacar "Fase 3". Aplicando memoria `feedback_no_internal_milestone_refs_in_ui`. Cambio: `editor.modal.about.version` "Versión 2.25.0 — Fase 3 cerrada" → "Versión 2.27.0" (es) / "Version 2.27.0" (en). Cubre tanto el welcome modal (`EditorUI.cpp:425`) como el About modal del MenuBar (`MenuBar.cpp:354`) — comparten la misma i18n key.

**Backlog del hito (no cerrado en F3H27).**
- **Compound atomic delete** — 1 `CascadeDeleteCommand` que snapshot todo el sub-tree y Ctrl+Z lo restaura en 1 step (vs N hoy).
- **Auto-borrar Empty huérfano post-ungroup** — opt-in en User Preferences. Hoy el Shift+Ctrl+G deja el Empty vacío, anti-sorpresa.
- **Inspector parent display** — campo "Parent" readonly en el body del Inspector con click → seleccionar el padre. Hoy se ve solo por indent del Outliner.
- **Top-level filter del active del multi-select** — si el active es hijo con padre selected, evitar doble delta. Aceptado como edge case en D2.
- **Migrar physics/picking/editor tools a `worldMatrixOf`** — 54 callsites de `TransformComponent::worldMatrix()` no migrados (local-as-world). Si parents tienen rotación/scale no-identity, el picking de hijos puede ser impreciso. Agendizar si el dev nota fricción.

---

## 2026-05-28: F3H26 cierre — Polish UX del editor (post-F3H25)

Hito insertado tras validación visual de F3H25. 7 items pequeños + 1 decisión arquitectónica (Union CSG) cerrados en una pasada.

**D1 — Sacar "Hito 3" del modal About + "(Hito 4)" del window title.** Strings desactualizados desde Fase 1/Hito 4. Confirmado al revisar UI: `editor.modal.about.version` "Versión 0.3.0 (Hito 3)" → "Versión 2.25.0 — Fase 3 cerrada"; `spec.title` "MoodEngine Editor - v0.4.0-dev (Hito 4)" → "MoodEngine Editor". Razón: nueva memoria `feedback_no_internal_milestone_refs_in_ui` aplicada a strings que no se mostraban directo al dev pero seguían apareciendo en el About.

**D2 — MenuBar reorden: Archivo > Editar > Mapa > Ver > Debug > Ayuda.** El dev: *"arriba dice archivo y luego mapa, usualmente es archivo luego editar"*. Estándar VSCode/Unity/Office: Editar segundo. Cambio físico mínimo (mover bloque del Editar antes del Mapa).

**D3 — Brush top-level removido → context menu del Outliner (right-click sobre brush).** El dev: *"el de brush solo tiene las operaciones booleans no se como eso no esta como algun modificador como los de blender o algo en lugar de ocupar una seccion arriba"*. Investigamos 4 patrones de la industria (Blender modifier no-destructivo, Hammer Carve simple, Unreal Modeling Mode dedicado, Maya submenu contextual) + 3 propuestas (Inspector, right-click Outliner, Modifier component). El dev eligió right-click Outliner. Iteración intermedia "Editar > Brushes (booleanas)" rejected post-validación. Gate por `e.hasComponent<BrushComponent>()` en `HierarchyPanel.cpp:299`. `EditorUI::drawBooleanOpMenu` reusado tal cual (ya es un `BeginMenu` con sus items, validación interna `>= 2 brushes` se preserva).

**D4 — Union CSG removida del UI (Hammer-style).** El dev al probar: *"he dado click en el mas chico y luego el mas grande y hago union, pero asi funciona la union den hammer? porque me termino creando 4 piezas separadas, es raro"*. Investigación del comportamiento: `Csg::unionOp(A, B)` con overlap parcial retorna `(A \ B) ∪ {B}` porque `A ∪ B` matemáticamente NO es convexo. El motor solo soporta brushes convexos (necesario para colisión Jolt, BSP, plane-clips) — descomponer en N convexos es la ÚNICA opción correcta. No es bug del algoritmo.

Estado de la industria:
- **Hammer (Source)**: NO ofrece Union — exactamente por esta razón. Solo Carve (substract) + meshes externos para geometría no-convex.
- **Unreal Modeling Mode**: Sí Union, también descompone en piezas convexas (mismo problema).
- **Blender**: Modifier Union es no-destructivo con representación BMesh no-convex. Otro modelo de geometría que el motor NO usa.

3 opciones presentadas al dev: (A) quitar Union del UI; (B) toast explicativo "Union creó N piezas convexas (CSG convex-only)"; (C) agrupar piezas en entidad padre. Dev eligió **A: quitar**. Junto con explicación del workflow brush-based: para "juntar" 2 brushes (ej. base + torre) NO se hace Union — quedan como entidades hermanas visualmente adyacentes; el render los pinta juntos. Las booleanas son para CORTES (ventana en pared, arco en columna). El `Csg::unionOp` en `engine/world/csg/BrushOps.cpp` **se mantiene intacto** — código sin uso pero correcto, forward-compat con modelo no-convex futuro (mesh editing real, BMesh-style). i18n key `editor.menu.boolean.union` se preserva por back-compat (sin uso UI hoy).

**D5 — UserPreferences sidebar de categorías estilo Blender.** El dev: *"algo que no me gusta de mi panel es que lo veo poco categorizado, te pongo el de blender alado para que veas que esta mas organizado"*. Reemplazo de `TabBar` horizontal (2 tabs: General + Editor) por split **sidebar (150px, border, Selectable list) + content (scroll vertical)**. 5 categorías: General / Viewport / Assets / Performance / Notificaciones. Window 540×360 → 720×480. `drawEditorTab` viejo refactorizado en 4 métodos por categoría (`drawViewport` / `drawAssets` / `drawPerformance` / `drawNotifications`) que comparten `cfg`/`defaults`/`dirty`/`saveNow` por ref desde el `switch (m_activeCategory)` del onImGuiRender. Sub-secciones internas con `SeparatorText` (Viewport tiene 3: "Cámara ortográfica" / "Gizmos" / "Interacción"; Performance tiene 2: stats overlay + "Profiler"). Sin cambios al storage (`UserSettings::EditorSettings` igual).

Razón: el TabBar horizontal con 2 tabs gigantes era difícil de scanear; al crecer (F3H23 stats + F3H24 toasts + F3H25 autosave) el "Editor tab" se volvió un dump de 4-5 sub-secciones internas. El sidebar de Blender Preferences (8+ categorías) es el patrón natural cuando hay > 4 secciones.

**D6 — Toast parpadeo en el frame de aparición — ID estable.** El dev: *"el toast a veces en lo que aparece, parpadea multiples veces rapidamente"*. Diagnóstico: `ToastsOverlay::draw` usaba `&t` (dirección del Toast en el snapshot temporal del vector) como ID de la ventana ImGui. Cada `Toasts::snapshot()` retorna un `std::vector<Toast>` NUEVO por valor → las direcciones de los elementos cambian frame a frame → ImGui veía un ID distinto cada frame → recreaba la ventana SIN cache de size del frame anterior → `AlwaysAutoResize` requería 2 frames para estabilizar el size → flicker visible en el frame de aparición (1er frame con size=0, 2do frame con size correcto).

Fix: `Toasts::Toast` gana `u64 id` monótonamente creciente asignado en `push()` bajo el mutex (counter `s_nextId` global en `Toasts.cpp`). `ToastsOverlay::draw` usa `##toast_<id>` estable. ImGui mantiene el cache de size correctamente y el primer frame de aparición ya tiene el tamaño calculado del primer Begin (consecutivo con el frame anterior que tenía el mismo ID).

**D7 — Ctrl+Z gateado por modificadores en cycle render mode.** El dev: *"si doy ctrl + z, me esta cambiando entre tipos de render, entiendo que el z cambia pero choca una cosa con otra"*. Diagnóstico: `EditorOverlay.cpp:520` usaba `ImGui::IsKeyPressed(ImGuiKey_Z, false)` sin chequear modificadores; el handler de undo en `EditorApplication.cpp` (que sí chequea KMOD_CTRL) Y el de cycle render mode disparaban juntos con Ctrl+Z — el undo aplicaba pero también ciclaba el modo.

Fix: gate por `!io.KeyCtrl && !io.KeyShift && !io.KeyAlt` antes del `IsKeyPressed(Z)`. "Z desnuda" sigue ciclando; Ctrl+Z solo deshace. Patrón aplicable a otros hotkeys de una sola tecla si aparecen colisiones similares.

**Ajuste reactivo**: `ImGuiChildFlags_Border` no compila en la versión de ImGui del proyecto (docking branch ~1.92). Fix trivial: usar overload `BeginChild(id, size, /*border=*/true, flags)` que sí está disponible.

**Backlog del hito (no cerrado en F3H26):**
- Iteración del context menu para que el right-click cuando hay 1 solo brush brush ofrezca un menú diferente (hoy queda disabled con tooltip implícito de drawBooleanOpMenu).
- Csg::unionOp código sin usar — se mantiene; si Fase 4 confirma que no se reactiva (no hay modelo no-convex), candidato a borrar.
- Window title sigue mostrando el nombre del proyecto + " *" si dirty (sin cambios).

---

## 2026-05-28: F3H25 cierre — Crash recovery + autosave

Cierra el plan original de Sub-fase 3.4 (luego F3H26/F3H27 insertados post-validación). 4 decisiones cerradas pre-implementación vía AskUserQuestion.

**D1 — Ubicación del autosave: subcarpeta oculta `.autosave/` dentro del proyecto.** Confirmado por el dev (vs sufijo `level1.moodmap.autosave` al lado del original / `%LOCALAPPDATA%/MoodEngine/autosave/<hash>/`). Path: `<projectRoot>/.autosave/<mapname>.moodmap`. Ventajas:
- No contamina la lista de mapas del proyecto (los path-globs de `project.maps` no incluyen `.autosave/`).
- Fácil de `.gitignore` con una sola línea.
- Acompaña al proyecto si se mueve a otra máquina (no como `LOCALAPPDATA` que requiere hash del project path para identificar).
- Patrón estándar (Unity `Library/AutoSave/`, JetBrains `.idea/`, VSCode `.vscode/`).

Atomic write: `.autosave/<map>.moodmap.tmp` + `std::filesystem::rename(tmp, final)`. En Windows el rename es atómico si origen y destino están en el mismo volumen (siempre cierto acá — ambos en `<projectRoot>`).

**D2 — Trigger: solo si dirty + N min.** Confirmado (vs "cada N min siempre" / "dirty + N min de idle"). Lógica:
```
tick(dtMs):
  if !prefs.autosaveEnabled return
  timerMs += dtMs
  if timerMs < intervalMin * 60_000 return
  if !dirtyFn():     // skip silencioso sin spamear el check
    timerMs = 0
    return
  writeFn(tmpPath) + rename(tmp, final)
  timerMs = 0
```

Razón: el caso común es el dev editando activamente y guardando manualmente cada N min. El autosave es safety net. Si NO hubo cambios desde el último write (manual o auto), no tiene sentido reescribir bytes idénticos. El reset del timer al detectar `dirty=false` evita evaluar dirtyFn cada frame tras cumplir N min.

Sin idle tracking (overkill para v1). El "dirty + idle de M segundos" se evalúa si emerge demanda — hoy el dev no reportó molestias por writes mientras edita.

**D3 — Recovery UX: modal blocking al abrir proyecto.** Confirmado (vs toast persistente con botón "Recuperar" / auto-cargar autosave sin preguntar). Razón: el aviso no se debe perder accidentalmente — un toast con autoclose podría desaparecer mientras el dev está mirando otra cosa, perdiendo la oportunidad de recuperar. El auto-cargar silente fue rechazado porque el dev pierde control (qué pasa si el autosave estaba corrupto a medias).

Implementación: `EditorApplication_RecoveryModal.cpp` (sigue split pattern de `_Init`/`_Run`/`_FileIO`). 440×auto-resize centrado, `ImGuiWindowFlags_AlwaysAutoResize | NoSavedSettings`. Botones "Restaurar" (carga via SceneSerializer + marca dirty porque canónico stale) / "Descartar" (borra autosave). Flag `m_recoveryModalPending` consumido en `pumpUiRequests` primera línea.

**D4 — Lock file format: JSON con PID + timestamp + engine_version.** Confirmado (vs touch del archivo / solo PID en plain text). Estructura:
```json
{"pid": 12345, "started_at": "2026-05-28T15:32:10Z", "engine_version": "v2.24.x"}
```

Razón: la info adicional (`started_at`, `engine_version`) ayuda al debugging si el dev reporta crashes — al ver el lock huérfano sabemos qué versión del editor crasheó y cuándo. El overhead de parse JSON es trivial (lock se lee 1 vez al abrir).

Detección de PID huérfano:
- Windows: `OpenProcess(SYNCHRONIZE, FALSE, pid)` + `WaitForSingleObject(handle, 0)`. SYNCHRONIZE basta para chequear existencia (sin requerir PROCESS_QUERY_INFORMATION que falla cross-session). WaitForSingleObject distingue PID libre (OpenProcess=NULL) de PID zombie (handle válido + WAIT_OBJECT_0 inmediato).
- POSIX: `kill(pid, 0)` no envía señal, solo chequea permisos + existencia. `errno=ESRCH` es el único "muerto definitivo"; otros errnos (EPERM = otro user) ambiguos pero asumimos vivo.

PID propio (caso raro: misma sesión reabriendo el proyecto sin cerrarlo limpiamente) → tratado como Clean. Razón: no queremos auto-disparar recovery sobre nosotros mismos.

`Status::InUse` (lock con PID vivo de OTRO proceso) → hoy se trata como Clean (overwrite del lock). El soporte real de 2 editores abriendo el mismo proyecto en paralelo (warning, lock compartido, sync) queda como hito propio. La consecuencia: si dos instancias abren el mismo proyecto, la 2da pisa el lock y la 1ra ya no podrá detectar crash propio. Aceptable para v1.

**Ajuste reactivo post-implementación**: crash silencioso del editor con `Fatal: <garbage>` (1 char distinto cada run) al lanzar tras F3H25. Diagnóstico largo (debug logs binary-search entre OpenGL init y loop start) → causa real fuera de F3H25: shaders `thumbnail_bg.vert/frag` (necesarios por `MaterialPreviewRenderer` ctor) NO deployados en `build/.../shaders/` porque `cmake --build --target MoodEditor` no ejecuta el target `mood_runtime_files ALL`. El ctor lanzaba `runtime_error` al fallar `OpenGLShader("shaders/thumbnail_bg.vert", ...)`; el `e.what()` retornaba string corrupta tipo "Fatal: 7" / "Fatal: d" (stack-use-after-free pattern del runtime al stringificar el path no abierto).

Fix: rebuild explícito de `mood_runtime_files`. Sin cambios de código. **Lección documentada**: al validar visualmente tras agregar nuevos shaders/assets, usar siempre `--target mood_runtime_files` (no solo `MoodEditor`). Si solo se modificó código C++ sin nuevos assets, `--target MoodEditor` alcanza.

**Backlog del hito (no cerrado en F3H25):**
- Concurrent-editors warning (hoy `Status::InUse` se trata como Clean).
- Asset import + shader compile toasts (herencia F3H24).
- Autosave incremental para mapas grandes (hoy reescribe completo cada N min).
- Recovery modal con preview/diff de "última sesión vs canónico".
- Lock con file lock OS (`flock` / `LockFileEx`) además del PID file.

---

## 2026-05-28: F3H24 cierre — Comunicación al dev (Console mejorada + Toasts)

**Contexto:** Quinto hito de Sub-fase 3.4. Implementación lineal en una sola tanda con las 4 decisiones cerradas pre-implementación vía AskUserQuestion al arrancar el hito (estilo overlay + click `.lua` + emisiones automáticas + search Console). Sin ajustes reactivos post-validación — el dev confirmó al cerrar con "todo ok".

**D1 — Estilo de los toasts: VSCode (bottom-right + slide-in).** Confirmado por el dev (vs `Unity Editor: top-right + fade` / `Centro inferior: fade`). Implementado en `ToastsOverlay` con stack vertical desde esquina inferior derecha (más nuevos abajo, push hacia arriba), slide-in horizontal en primeros 200 ms con easing `easeOutCubic` (desacelera al llegar), visible 100% mientras vida > 400 ms, fade-out lineal en últimos 400 ms. Razón: convención familiar para devs (VSCode es la app más usada hoy en programación), no choca con el viewport principal (que está arriba). Alternativa descartada (Unity top-right + fade): top-right ya tiene el viewport render mode bar de F3H21; el bottom-right está libre.

**D2 — Click en `file:line` del Console: solo `.lua` con sistema externo.** Confirmado por el dev (vs `No implementar en F3H24, diferir`). Implementado con helper `findLuaPath()` que detecta paths conteniendo `.lua` (con o sin `:N` opcional) escaneando hacia atrás desde el match hasta delimitador (`whitespace / ( / [ / ' / " / <`); si encuentra, agrega `ImGui::SmallButton(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE)` que dispara `ShellExecuteA(nullptr, "open", filePath, ...)`. Razón: cubre el caso 80% (errores del Lua VM mencionan paths) con código mínimo (~30 líneas) y sin acoplar al editor a un editor externo específico. Out-of-scope: paths `.material` / `.moodprefab` / `.json` (extensión trivial — agregar más extensiones al helper); saltar a línea N específica (VSCode tiene `code -g file:line` pero requiere detectar VSCode instalado y bypass del ShellExecute genérico). Diferidos a backlog.

**D3 — Emisiones automáticas de toasts: save + asset import + shader compile + prefs update (4 sitios).** Confirmado por el dev (vs `Solo errores + save (2)` / `Todo warn+err+critical automático`). Implementación efectiva en F3H24:
- **Save project** + **save map as** + **open map** + **open project**: call-sites directos en `EditorProjectActions_FileIO.cpp` y `EditorProjectActions_Map.cpp` — 4 sitios concretos con manejo de error (catch en save → Error toast).
- **Preferences saved**: implementado con tracking `m_changedSinceOpen` + `m_wasVisibleLastFrame` en `UserPreferencesPanel`. Setea true en `saveNow=true` de cualquier toggle/slider/reset (gemelo del flag `dirty` ya existente) + en cambios discretos del general tab (tema, idioma). Al detectar transición visible: true→false con changes pendientes, emite Success "Preferencias guardadas" + resetea el flag. Razón: 1 toast por sesión de edición (no 1 por slider movido — sería spam ruidoso).
- **Asset import** + **shader compile**: diferidos a backlog explícito por falta de single call-site limpio. El flujo de import va por copy al filesystem + rescan del AssetBrowser sin un trigger único; el shader compile vive dentro del MaterialEditor / ShaderGraph sin un único exit point post-compile. Cuando emerja un `AssetImporter` centralizado o un hook `MaterialEditor::onCompileSuccess` explícito, agregar 1 línea de `Toasts::pushSuccess`. Alternativa descartada (`Todo warn+err+critical automático`): demasiado ruidoso — el motor logea decenas de líneas por frame en operaciones normales (cache misses, sentinel paths, etc); los toasts dejarían de tener señal.

**D4 — Console+ con search case-insensitive sobre message + channel (reemplaza filter por channel).** Decisión menor de implementación tomada en el momento. El filter pre-F3H24 buscaba substring del **channel** solamente (`engine`, `render`, `script`, etc); poco útil day-to-day porque el dev no sabe en qué channel se loggeo cada cosa. Reemplazado por search **case-insensitive** sobre `text + " " + channel` (cubre ambos sin requerir prefijos como `chan:` o sintaxis especial). Buffer agrandado de 32 a 64 chars (keywords del bug suelen ser frases cortas, no palabras sueltas). Renamed `m_channelFilter` → `m_messageFilter`. Lower-case del filter una vez fuera del loop por perf; helper `matchFilter()` per-entry construye `text + " " + channel` lowercased y hace `find()`. Key i18n `editor.panel.console.filter_hint` queda obsoleta pero se preserva por back-compat con i18n JSON viejos.

**Backlog del hito (no cerrado en F3H24):**
- **Asset import** + **shader compile** toast emisiones — diferidos por falta de single call-site. Cuando se introduzca un `AssetImporter` centralizado (probablemente en Fase 4 con sistema de plugins de import), agregar 1 línea.
- **Click `.lua:N` salta a línea N** — `ShellExecute` open solo abre el archivo. VSCode tiene `code -g file:line`; podríamos detectar VSCode (registry check `HKCR\\.lua\\OpenWithProgids` o env var `VSCODE_*`) y bypass al binario directo. Hito propio si emerge demanda real.
- **Click en paths `.material` / `.moodprefab` / `.json`** — extender `findLuaPath` a detector multi-extensión. Trivial (~5 LOC).
- **Toasts con acción** (botones "Undo" / "Ver detalle" embebidos en el toast) — útil para "Project save failed: <error> [Reintentar]". Requiere refactor del overlay para callbacks (la `Toasts::Toast` struct hoy es POD sin función pointer). Hito propio si emerge demanda.
- **Dedup de toasts idénticos** — hoy 2 pushes del mismo message generan 2 chips. Convención VSCode/Mantine: refrescar el lifetime del existente. Hito propio si el dev nota spam.

---

## 2026-05-28: F3H23 cierre — Performance feedback (Profiler + Stats overlay)

**Contexto:** Cuarto hito de Sub-fase 3.4. Consolidado del plan original ex-F3H23 Profiler + ex-F3H24 Stats overlay (mismas métricas runtime FPS/drawcalls/tris/mem). El dev confirmó al arrancar 2 decisiones críticas (estilo + orden), las otras 4 quedaron como sub-decisiones de implementación. Implementación en 2 pasos validados visualmente (StatusBar primero, luego ProfilerBuffer + Panel) con 2 ajustes reactivos.

**D1 — Stats overlay = StatusBar global del editor (Unity-style bottom bar).** Validado por el dev al arrancar (vs `Quake r_speeds` multiline / Unreal chips numéricos). El primer iter dibujaba un single-line overlay al pie del viewport image — el dev rechazó visualmente al ver superposición con el header del Asset Browser cuando los paneles compartían fila. **Refactor reactivo**: mover los chips a la `StatusBar` global (la barra que ya tenía FPS + Modo + Proyecto sin guardar). Beneficio adicional: la StatusBar es global (no per-viewport), siempre visible aunque el dev cierre el viewport principal o cambie de workspace.

**D2 — Toggleable per-widget (no all-or-nothing global key).** Cada chip tiene su propio bool en `UserSettings.editor.statsOverlay.show<X>` (7 flags). Defaults: `showFps`/`showDrawcalls`/`showTris` = true; `showMemGpu`/`showMemCpu`/`showLights`/`showEntities` = false. El dev arma su HUD desde Preferences > Editor. Alternativa descartada (key F11 togglea todo el HUD): menos flexible — el dev típicamente quiere "FPS siempre, drawcalls al optimizar, RSS al cazar leak", no un on/off binario.

**D3 — Profiler ring buffer single-thread (no per-thread map).** Tracy ya maneja per-thread internamente vía `ZoneScopedN`. El ring in-engine de F3H23 vive en el main thread del editor — donde corren TODOS los `MOOD_PROFILE_SCOPE` existentes (~50 call-sites en SceneRenderer/AssetManager/Editor). Single-thread elimina contención de locks en hot path (push del scope ocurre miles de veces por frame). Si emerge demanda (futuro worker thread del asset import o physics async), se agrega `std::unordered_map<std::thread::id, ProfilerBuffer>` en hito propio.

**D4 — Hook al ring vía RAII en `MOOD_PROFILE_SCOPE` existente (no macro nueva).** La macro emite ahora Tracy zone (`ZoneScopedN`, cuando `TRACY_ENABLE`) **+** RAII `Mood::detail::ScopeTimer` declarado en `core/Profiler.h` con dtor out-of-line en `ProfilerBuffer.cpp` (chrono::steady_clock + pushScope al global). Beneficio masivo: los ~50 scopes ya instrumentados desde F2H2 alimentan el ProfilerPanel automáticamente sin tocar call-sites. Alternativa descartada (`MOOD_PROFILE_SCOPE_RING` macro paralela): requeriría agregar 1 línea en cada call-site existente. Cuando Tracy está OFF, el ring sigue corriendo (el panel funciona sin Tracy build); cuando `MOOD_PROFILE=OFF` en CMake, ambas macros caen a no-op total — cero overhead en release sin profiling.

**D5 — GPU markers `glBeginQuery(GL_TIME_ELAPSED)` OUT-OF-SCOPE en F3H23.** El ProfilerPanel muestra CPU time únicamente. GL_TIME_ELAPSED per-pass requiere driver sync que tira FPS ~5%+ sin throttling (medir 1/30 frames). Cuando el dev quiera optimizar shadow / SSAO / SSR de verdad, se agrega como toggle en Preferences "GPU markers (cuesta FPS)" en hito propio. Tracy + RenderDoc cubren este caso hoy si el dev tiene Tracy build prendido.

**D6 — VRAM NVIDIA-only via `GL_NVX_gpu_memory_info`.** El helper devuelve `(TOTAL_AVAILABLE - CURRENT_AVAILABLE) * 1024` bytes consumidos por el proceso. Sin la extensión (drivers AMD/Intel/Mesa), `GLAD_GL_NVX_gpu_memory_info` es 0 → return 0 → UI muestra "—" sin fallar. AMD tiene `GL_ATI_meminfo` (semántica distinta: reporta tamaño del pool libre por heap); se agrega al helper cuando un dev con AMD reporte el "—" en VRAM. Para Mac/Linux: equivalentes futuros con `MTLDevice.currentAllocatedSize` / DRI3 query.

**Ajuste reactivo A — Overlay del viewport → StatusBar inferior.** Documentado en D1.

**Ajuste reactivo B — `kLabelColumnWidth` 160→240 px en `UserPreferencesPanel`.** Los labels largos en español ("Tamaño gizmo (mover/escalar)" ≈ 200 px, "Retraso preview al pasar el cursor" ≈ 230 px) pisaban la columna del slider con el ancho original. Subido a 240; cabe holgado en el modal de 540 px de ancho (240 label + 200 control + 30 reset = 470). Sin restricción de scrollbar horizontal. Polish menor, registrado acá porque se descubrió al agregar los toggles + slider de F3H23.

**Backlog del hito:**
- **GPU markers** vía `glBeginQuery(GL_TIME_ELAPSED)` con throttling (1/30 frames default). Toggle en Preferences "GPU markers (cuesta FPS)". Cuando el dev quiera optimizar el render pipeline a fondo.
- **VRAM AMD** via `GL_ATI_meminfo` — trivial ~10 LOC en `vramUsedBytes()` con preferencia a NVX si está disponible. Diferido hasta que un dev con AMD reporte "—".
- **Per-scope histograma** en el ProfilerPanel — hoy el histograma muestra el último frame agregado. Hover sobre row → mini-chart de los últimos N samples de ese scope individual. Hito propio si el dev quiere tendencias visuales por scope.
- **Export CSV del Profiler** — gemelo del PerformanceHud snapshot existente (F2H2 Bloque G). Botón "Export" → `<project>/.cache/profiler/snapshot_<timestamp>.csv` con scope/avg/min/max/hits por frame.
- **Markers de eventos** (asset load, scene save, dev acciona play, etc) overlay sobre el histograma para correlacionar spikes con acciones.

---

## 2026-05-27: F3H22 cierre — Properties Editor con icons laterales (Blender style)

**Contexto:** Tercer hito de Sub-fase 3.4. Inspector reescrito de "lista plana scrollable" a "categorías filtradas por icons laterales" estilo Blender Properties Editor. El plan llegó con 6 decisiones cerradas (D1-D6 con investigación industrial Blender/Unreal/Unity); la implementación + 5 rondas de validación visual con el dev forzaron 6 ajustes reactivos que sobreescriben parcialmente esas decisiones. Documento solo los **finales post-ajuste**.

**D1 — Single categoría activa, SIN botón "All".** Click cycle entre categorías; nunca hay "modo todo apilado" (legacy). Razón post-ajuste: el plan inicial incluía "All" al final de la barra como escape al modo legacy; el dev rechazó al validar (*"el all solo confunde, prefiero separar"*) — con 7 categorías y default colapsado, el modo todo-en-uno perdía valor y agregaba ruido cognitivo. Alternativa descartada (Unreal Details Panel single-scroll): rompía el mental model Blender que el dev pidió explícitamente.

**D2 — Barra vertical lateral izquierda, NO horizontal.** 36px ancho × altura del Inspector, botones 28×28 uno por línea. Razón post-ajuste: primera implementación usó barra horizontal arriba del Inspector; el dev rechazó al validar (*"prefiero que se mas en vertical un minipanel vertical"*) — Blender Properties Editor es vertical (Navigation Bar), Unreal Details Panel no tiene barra (sin referencia). Vertical aprovecha que el Inspector es **tall + narrow** (los componentes ocupan ancho útil); una columna lateral suma sin restar. Default a la izquierda; flippable a la derecha sería follow-up Blender-like.

**D3 — Categorías sin componentes se ocultan (Blender pattern).** Solo aparece un icon si la entity tiene ≥1 componente de esa categoría. **Object siempre presente** (toda entity tiene Transform). Razón: reducir ruido visual (un cubo con solo Transform+MeshRenderer ve 2 icons, no 7 grises). Blender oculta dinámicamente las tabs irrelevantes al tipo del objeto. Alternativa descartada (Unreal: mostrar todas las categorías declaradas siempre): convención que el dev no usa.

**D4 — Environment es scene-wide implícito (auto-spawn + oculto del Outliner).** El plan inicial trataba EnvironmentComponent como cualquier otro componente per-entity ("singleton" pero requería seleccionar la entity portadora). El dev pidió pattern Blender estricto (*"hay que tirar mas a blender, en blender es automatico, ya esta en el rendeer view"*). Implementación final:
- **Auto-spawn** `ensureEnvironmentExists()` silent variant llamado post-load (`loadProject` + `openMap`) — todo proyecto tiene Environment desde el arranque.
- **Categoría Environment siempre visible** (scene-wide) — accesible sin selección, busca el singleton en cualquier entity de la scene.
- **Delete bloqueado** en `EditorScene::deleteSelectedEntity` (singleton inviolable + status message i18n).
- **Removido del menú "Add Entity"** (no tiene sentido crear uno cuando ya existe — el card de Environment del array `kLights[]` del PickModal eliminado).
- **Oculto del Outliner** (filtro en `HierarchyCollect` igual que `VehicleWheelMarker`) — el dev nunca ve Environment como entity selectable, solo como categoría del Inspector.

Mental model **Blender World Properties**: el "mundo" es scene-wide implícito, no una entity más en la jerarquía. Alternativa descartada (manual create + selectable + deletable como otras entities): el dev pidió ocultar la mecánica de la entity y mostrar solo la propiedad scene-wide.

**D5 — Rendered NO fuerza post passes (respeta Environment).** El plan inicial de F3H21 (D4) decidía "Rendered fuerza bloom + SSAO con defaults conservadores; SSR opt-in". El dev rechazó al validar F3H22 (*"estas forzando alguna configuracion en el render preview, que luego le quita al usuario el poder de activar o no, como el AO"*). Implementación final: **removido `m_forcePostPasses`** del `SceneRenderer`. Material vs Rendered se diferencia **solo** por `skipPostPasses` (Material salta TODO el post; Rendered respeta los flags del Environment). El dev controla AO/Bloom/SSR/Color Grading desde el EnvironmentComponent — pattern Blender (el World Properties tiene los toggles, los viewport shading modes solo cambian si los aplican o no). Sobreescribe parcialmente la decisión F3H21-D4 — registrado acá porque el cambio se aplicó en F3H22.

**D6 — Default colapsado en `beginComponentSection` + eliminar toolbar "Plegar/Expandir todo".** Razón: con categorías filtrando (cada vista muestra pocos componentes), la toolbar global de F2H81 perdía valor. El dev lo confirmó al validar (*"el de plegar todo o expandir no me interesa, ese sacalo, por defecto deben estar colapsados"*). Removido el `ImGuiTreeNodeFlags_DefaultOpen` + el método `renderSectionToolbar()` + miembro `m_forceSectionState`. Alternativa descartada (mantener toolbar como toggle global): UI clutter sin upside.

**D7 — Botones del viewport render mode bar: cuadrados 28×28 + transparente + tooltips de una palabra.** El dev pidió en esta tanda (*"el fondo negro no me gusta, sacalo que sea transparente"*, *"podes hacer que los iconos de wireframe, sean mas cuadrados, ademas hay mucho texto, prefiero que diga 'wireframe' 'solid', 'material' 'render'"*). Implementación: `SetNextWindowBgAlpha(0.0)` + `ImGuiWindowFlags_NoBackground`; `Button(label, ImVec2(28, 28))` (no `SmallButton`); `FramePadding=(0,0)` + `ButtonTextAlign=(0.5, 0.5)` explícito para centrar glyphs FontAwesome con metrics dispares (sin esto, el círculo y el cubo aparecen a alturas distintas). Tooltips simplificados de descripción larga a una palabra (Wireframe/Solid/Material/Render). Sobreescribe el comportamiento UX de F3H21 (que usaba `SmallButton` con tooltips largos descriptivos). Registrado acá porque la pulida se aplicó en F3H22.

**Backlog del hito (no cerrado en F3H22):**
- Soporte para flippear la barra de categorías a la derecha (Blender-like, pref de UserSettings).
- "+ Add Component" per-categoría (Render → +Mesh / +Light; Physics → +Collider; etc.) — hoy es popup unificado, podría filtrar por categoría activa.
- Filtro de texto del Inspector (Unreal-like search bar) — out-of-scope D6 del plan original, mantener como hito propio si emerge demanda.
- Color-coding tipo Blender (Render=blanco, Material=fuchsia, Physics=azul) — requeriría pack de iconos custom, diferido a Fase 4.

---

## 2026-05-27: F3H21 cierre — Viewport pro (numpad views + render modes)

**Contexto:** Segundo hito de Sub-fase 3.4. Plan tenía 4 decisiones cerradas pre-implementación (D1-D4 con investigación industrial Blender/Unreal/Unity); durante la implementación + validación visual con el dev emergieron 2 más (D5-D6).

**D1 — Lerp Blender 200ms vs teleport Unity/Unreal.** Numpad views animadas con smoothstep + yaw shortest-path, default ON, duración configurable en `UserSettings.editor.smoothViewDurationMs` (clamp 0-1000, 0=teleport). Razón: el dev ya usa Blender como referencia mental (memoria `feedback_no_reinventar_rueda`); el lerp evita la desorientación que el teleport genera al saltar entre vistas ortogonales. Alternativa descartada (teleport default): Unity/Unreal lo hacen pero el dev no viene de ese workflow.

**D2 — Render modes en perspectiva + ortho ambos.** Los 4 modes (Wireframe/Solid/MaterialPreview/Rendered) funcionan en viewport perspectivo + los 3 ortográficos del map_editor. Sin gating. Razón: convención industrial 100% (Blender shading modes en ambos, Unreal Alt+1/2/3/4 en perspective + 4 ortho, Unity Shaded modes en ambos). Restringir generaría fricción sin upside.

**D3 — Numpad 0 = pose-copy a la primera CameraComponent.** El editor camera salta a la pose (yaw/pitch desde rotationEuler, target a 5m frente) de la primera entity con CameraComponent. Si no hay → log "no hay CameraComponent en la escena". Alternativa descartada (view-through dual como Blender — entrar a modo "viendo a través de la cámara activa"): requeriría estado dual del editor (cam editor vs cam scene), invalidar el orbit, manejar input switching. Pose-copy es 1 setter + 0 estado. Si emerge demanda real de view-through, hito propio.

**D4 — Rendered fuerza bloom + SSAO con defaults conservadores; SSR opt-in vía Environment.** El modo Rendered usa OR con flags de Environment + intensidades default sensatas cuando el Env tiene 0 (bloom intensity 0.6/threshold 1.5/radius 1.0; SSAO intensity 1.0/radius 0.5). SSR NO se fuerza. Razón: la primera implementación forzaba SSR ON con defaults, generaba artifacts garantizados sin tuning del Environment (bandas verticales en el cielo por rayos escapando del FB, ghost reflections). El dev validó visual y reportó "en el modo render final hay artifacts". Bloom threshold subido de 1.0 a 1.5 (1.0 sobre-brighteaba cielos claros). SSR es opt-in: el dev lo activa en Environment con tuning propio. Alternativa descartada (forzar SSR con defaults agresivos): el approach correcto sería autodetect de scale del scene para tunear SSR — out-of-scope de F3H21, sería hito propio.

**D5 — Solid mode = branch en `pbr.frag` (uniform `uSolidShading`) vs material override CPU.** El shader chequea al inicio del main; si on, devuelve `vec3(0.65) * (ambient 0.35 + NdotL * 0.65)` con light hardcoded y `NormalRT = vec4(0.0)` para que SSR descarte, saltando TODO el PBR loop. Razón: branch shader es ~5% costo del PBR completo (las texturas no se samplean, no hay loop de luces, no IBL), cero state per-draw call, escala a N entidades trivialmente. Alternativa descartada (sobrescribir cada material en CPU con `albedoTint=gris + uHasAlbedoMap=0`): invasivo, costo per-draw, requeriría capturar/restaurar state al toggle entre modes.

**D6 — F2H30 sub-mode keys 1/2/3 → top-row only.** Pre-F3H21 los sub-mode keys aceptaban `ImGuiKey_1 || ImGuiKey_Keypad1` (idem 2/3). F3H21 los limita a top-row only para liberar Numpad 1/3/7 para views. Razón: convención Blender pura — top-row reservado para sub-modes (Vertex/Edge/Face), numpad reservado para views (Front/Right/Top). El dev ya está acostumbrado al Blender mental model. Trade-off: dev que usaba numpad para sub-modes pierde ese atajo — pero gana las 7 numpad keys para views. Hito-net positivo en hotkey budget.

**Polish reactivo F3H20:** floating text del delta en translate drag (`X +1.500 (grid 0.5)`) gateado al `snapGridEnabled` — sin snap activo, drag silencioso. El dev reportó al cerrar F3H21 que era ruido visual cuando no usaba snap.

**Backlog del hito:**
- Solid con texture passthrough opcional: hoy es gris uniforme. Texture opt-in (mostrar albedo sin lighting completo) sería útil para ver UV layout — hito propio si emerge demanda.
- Rendered con SSR auto-tuneado por scale del scene: autodetect de bounding del Environment + tune `maxSteps`/`thickness`/`stepSize`. Out-of-scope F3H21 (decisión D4) — hito propio cuando el dev quiera SSR confiable en Rendered sin tocar Environment.
- **Properties Editor con icons laterales tipo Blender**: rediseño de los 15+ paneles del Inspector a "categorías" con icons verticales clickeables (Render / World / Object / Mesh / Material / Particles / Physics / Script / etc) estilo el Properties Editor de Blender. El dev lo pidió al validar F3H21 ("podriamos mejorar exponencialmente esto", "esto se que es un hito mas grande"). Anotado en memoria `backlog-ux-gaps-editor` como hito propio de Sub-fase 3.4 (si emerge demanda antes de cerrar Fase 3) o Fase 4.

---

## 2026-05-27: F3H20 cierre — Snapping configurable (Hammer-style)

### Decisión 1 — Hammer-style grid snap reemplaza vertex snap perspectivo

**Contexto:** F3H20 arrancó con un plan de 4 features (grid + vertex + angle + face-align). El dev eligió en planeación inicial "Vertex snap extendido" como feature principal del translate gizmo. Tras 3 iteraciones implementando vertex snap (pivot-to-vertex → source-at-drag-start → dynamic Closest mode con marcadores yellow source/target) y validación visual, el dev reportó *"es medio raro"* y preguntó *"se usa en Hammer?"*. Honesto: vertex snap perspectivo con marcadores yellow es UX cuestionable (los corners que se alinean no son obvios, los marcadores agregan ruido visual). Hammer-style grid snap es más predictible: cuantizá el delta a múltiplos del step y listo.

**Decisión:** Reemplazar vertex snap perspectivo por grid snap Hammer-style. Borrar las funciones `snapToVertexInScene` / `findSnapTargetForGizmo` / `closestVertexOnEntityToWorld` y el code de marcadores yellow. Mantener `snapToVertexEnabled` como field para el feature ortho original de F2H31C (workspace "Editor de mapas" — funcionalidad distinta, preservada intacta).

**Razones:**
- Predictibilidad: grid snap es determinístico (no depende de cursor hovering sobre vertex). El dev sabe que va a moverse en saltos de N unidades.
- Sin ruido visual: sin marcadores ni labels SNAP. Solo el gizmo brinca de a step.
- Match con el workflow Hammer/Source que el dev quiere reproducir (brushes en grid).
- El vertex snap orto ya cumple el rol de "alinear con geometría existente" en el contexto donde tiene sentido (pincel/block tool de mapas).

**Alternativas descartadas:**
- Mantener ambos snap modes mutuamente exclusivos (toggles V y G como radio buttons): viola "menos UI", ningún engine lo hace de esta forma, vertex snap perspectivo no agrega valor.
- Keep vertex snap como toggle adicional opt-in (G + V combinable): el dev dijo explícito que prefiere Hammer-style. Ese workflow está descartado para el perspective viewport.

**Revisión:** estable. Si en el futuro emerge demanda de vertex snap perspectivo (e.g., para meshes importadas exactas), se puede reintroducir como toggle separado sin romper el grid snap.

### Decisión 2 — Snap al delta del drag, no a la posición absoluta

**Contexto:** Cuando el grid snap está activo y el dev arrastra un objeto colocado off-grid (posición no múltiplo del step), hay dos comportamientos posibles: (a) snap absoluto — el objeto salta al múltiplo del step más cercano al iniciar el drag (Hammer "puro"); (b) snap al delta — el objeto se mueve en saltos limpios de N unidades pero conserva su offset original (Blender/Unity-style).

**Decisión:** Snap al delta. `newPos = startValue + axis * round(delta/step)*step`.

**Razones:**
- Sin "jolt" inicial: objetos importados, prefab spawns, o entities placed sin grid quedan donde estaban — el dev no se sorprende.
- Cuando ambos objetos arrancan on-grid, se mantienen on-grid (caso común en map editing).
- Para "limpiar" objetos off-grid el dev tiene la opción "Alinear al grid" (backlog) — comando one-shot explícito.

**Alternativas descartadas:**
- Snap absoluto puro: rompe el caso de meshes importadas (saltan al activar snap). En Hammer puro esto no es problema porque los brushes se crean ON grid; en MoodEngine las entidades vienen de prefabs/scripts.
- Modo configurable (absolute vs delta): otro toggle, complica la UI. La regla "menos UI" gana — preferimos el comportamiento default sano y un comando explícito para el caso del jolt.

**Revisión:** estable. Si emerge backlog "Alinear al grid", ese feature cubre el caso "quiero todo on-grid" sin necesitar absolute-snap mode.

### Decisión 3 — Scale snap removido

**Contexto:** Iter1 implementé scale snap (toggle `S` + chip `Scale 0.1` + lógica en gizmo scale uniform + per-axis + modal E). En iter7 el dev preguntó *"se usa en Hammer?"*. Honesto: no. Hammer no escala brushes (los modifica vertex por vertex); los props tienen `modelscale` tipeado a mano sin gizmo. Unity y Unreal tienen scale snap pero casi nadie lo usa — los artistas tipean valores exactos en el Inspector. Casos de uso reales (kits modulares con scale `0.5x/1x/1.5x`, tiles escalables por enteros) son raros y se resuelven con typing directo.

**Decisión:** Remover scale snap por completo. Borrar `snapScaleEnabled` + `snapScaleIncrement` de SnapSettings, toggle `S` del overlay, chip `Scale` del status bar, lógica en gizmo + modal, i18n keys. Forward-compat: load ignora keys `scale_*` en `.moodproj` viejos sin crash.

**Razones:**
- Menos UI, menos código, menos cognitive load.
- Workflow real: scale se tipea en Inspector, no se arrastra con gizmo + snap.
- Honestidad sobre qué features valen su peso. Mantener "por si las moscas" agrega entropy sin uso medible.

**Alternativas descartadas:**
- Dejarlo "por compleción": no agregaba valor real, agregaba ruido.
- Reusar S para "snap to surface" (raycast hacia abajo): scope distinto, agendizado al backlog `align-and-drop-backlog`.

**Revisión:** estable. Si emerge feedback "quería scale snap para X", reintroducirlo es trivial (re-añadir field + toggle + lógica idempotente).

### Decisión 4 — Status bar arriba del viewport (no inline bajo cada toggle)

**Contexto:** Iter5 agregué un step picker bajo el toggle G en el side toolbar (botoncito `0.5` clickeable). El dev lo vio en validación: *"queda re mal que quede ahi, seria mejor que se vea como un texto en la zona superior, como lo suele hacer blender"*. Blender muestra el snap config en una status bar arriba del 3D viewport (chips horizontales con orientation, pivot, snap mode + step).

**Decisión:** Status bar arriba-centro del viewport con chips horizontales (`Grid 0.5` / `Angle 15°`) que aparecen solo cuando ese snap está activo. Click sobre el chip cicla su step. Sin snaps activos → no se renderiza nada (sin ruido visual).

**Razones:**
- Patrón estándar Blender/Maya/Unreal (Modify panel del viewport).
- Side toolbar queda limpio (solo toggles, no clutter de step pickers).
- Status bar es compacta y solo aparece cuando informa algo útil.
- Clickear el chip para ciclar evita el patrón "abrir menú contextual con 6 opciones" que sería más clicks.

**Alternativas descartadas:**
- Inline bajo cada toggle (lo que probé en iter5): el dev lo rechazó visualmente. El side toolbar se vuelve un mosaico de cosas distintas (toggles + valores numéricos).
- Combo/dropdown ImGui::Combo en el side toolbar: ocupa más espacio vertical, no es plug-and-play con el iconBtn helper.
- No mostrar el step en ningún lado (solo Ctrl++/Ctrl+- + log): el dev no sabe en qué step está sin activarlo + dragear. Mal feedback.

**Revisión:** estable. Si en el futuro emerge más config relacionada al viewport (pivot point / orientation / snap target type), va al mismo status bar como chips adicionales.

---

## 2026-05-27: F3H19 cierre — Rename con cascada

### Decisión 1 — Cobertura backend completa, UI inicial en AssetBrowser principal

**Contexto:** F3H19 debe entregar rename con cascada — renombrar un asset actualiza todas las refs en la escena + cache del AssetManager + en disco. Cobertura posible: subset Tier 1 (Texture/Mesh/Material/Script, 80% del caso), cobertura completa (todos los tipos: Texture/Mesh/Material/Script/Animation/Prefab/Dialog/Item/Vehicle/Audio + dependencias derivadas), o mínimo viable (solo Material + Texture).

**Decisión:** Cobertura backend completa (todos los tipos). UI inicial limitada al AssetBrowser principal (8 tabs); rename desde ItemBrowserPanel/DialogBrowserPanel/QuestPropertyEditorPanel queda como follow-up mecánico cuando emerja demanda.

**Razones:**
1. **Backend completo no infla complejidad significativamente** vs subset: el patrón es `if (ext == X) m_xxx.rename(id, newPath)` repetido por familia. 10 familias = 10 if blocks. El subset Tier 1 sería 4 if blocks. Marginal.
2. **Refs id-based unifica el flow**: los componentes que apuntan por id (MeshRenderer.mesh, Audio.clip, etc) NO se reescriben individualmente — el cache del AssetManager hace todo el trabajo. Cubrir más familias = más entradas en `renameLogicalPath`, no más complejidad del comando.
3. **El AssetRefIndex es agnóstico al tipo**: la función `findRefs(scene, assets, path)` retorna refs de TODOS los tipos sin distinguir. El subset Tier 1 introduciría arbitrary cutoffs en el walk.
4. **UI inicial en AssetBrowser cubre el 90% del UX caso real**: el dev típicamente renombra desde el browser donde explora los assets. Browsers especializados (ItemBrowser/DialogBrowser/etc) son para editar UN asset — agregar "Renombrar" ahí es follow-up natural cuando el dev lo pida.

**Alternativas descartadas:**
- Subset Tier 1: ahorra ~30 LOC pero pierde 6 tipos de assets sin razón estructural. Si el dev renombra un `.moodquest` y se queda huérfano del cache, hay que volver a F3H19 a expandir. Cobertura completa lo cierra de una.
- Cobertura completa + UI en TODOS los browsers (incluido ItemBrowser/DialogBrowser/QuestPropertyEditorPanel): infla diff sin agregar capacidad técnica nueva. Cada panel es 1 wire trivial — diferir hasta que emerja demanda.

**Revisión:** Memoria backlog [[asset_rename_browser_coverage]] agenda los 3 paneles diferidos. Activar cuando el dev pida "renombrar desde aquí".

### Decisión 2 — Abort si el nombre destino ya existe

**Contexto:** Al renombrar, el dev puede ingresar un nombre que ya está usado por otro archivo. Opciones: error + abort (cancelar la operación), sufijo automático `_2` / `_3` (renombrar el destino), confirmar overwrite (preguntar al dev si quiere sobreescribir).

**Decisión:** Abort + mensaje de error rojo "Ya existe un archivo con ese nombre" en el modal. Botón Renombrar reapply el check al click — no destruye el archivo destino.

**Razones:**
1. **Patrón más seguro para operación destructiva**: el rename mueve un archivo en disco. Si el destino ya existe, hay riesgo de pérdida de datos (sobrescritura silenciosa). Abort elimina ese riesgo.
2. **Sufijo automático es sorprendente**: el dev escribe "hero.lua" y termina con "hero_2.lua" sin darse cuenta. UX trap.
3. **Confirm overwrite agrega clicks sin reducir riesgo**: si el dev confirma por accidente, mismo problema que el sufijo. Mejor que decida con un nombre limpio.
4. **El dev sabe mejor**: si quería renombrar el destino primero, puede hacerlo + retry. Si quería overwrite, puede borrar el destino + retry. El abort respeta agency.

**Alternativas descartadas:**
- Sufijo automático: descartado por UX trap (descrito arriba).
- Confirm overwrite: agrega 1 click sin reducir riesgo real — el dev clickea Sí por inercia.

**Revisión:** Si el dev pide bulk rename con conflict resolution (sufijo o overwrite por defecto), agendar F3H_bulk_rename.

### Decisión 3 — RenameAssetCommand confía en pre-conditions del caller

**Contexto:** El RenameAssetCommand recibe (oldDiskPath, newDiskPath, oldLogical, newLogical, refs) en su ctor. ¿Dónde se valida que newDiskPath no exista en disco? Opciones: en el ctor del comando (rechaza si no se cumple), en el caller (modal UI), o en ambos lados (defensive double-check).

**Decisión:** El caller (modal del Asset Browser) valida pre-construct. El comando NO duplica validation — confía.

**Razones:**
1. **El modal es la única fuente del comando**: no hay otros call sites. Validar en el modal es suficiente para el flow real.
2. **El comando NO tiene UI para reportar errores**: si el ctor rechaza por pre-condition no cumplida, ¿cómo le decimos al dev? El modal es donde el error tiene sentido (mensaje rojo inline).
3. **Defensive double-check es redundante**: poner el mismo check en 2 lugares aumenta probabilidad de drift (uno mejora, otro queda viejo).
4. **Si por bug el caller no valida y fs::rename sobreescribe** (en Windows lo hace silenciosamente): el comando loguea pero no rollback. Trade-off explícito — un bug en el modal puede causar pérdida de datos. Aceptable porque el modal está testeado.

**Alternativas descartadas:**
- Validar en el ctor del comando + throw: rompe ICommand interface (no se espera que tire). Y el modal igual debería validar para mostrar el error al dev pre-click.
- Validar en ambos lados: redundancia + drift.

**Revisión:** Si emerge un segundo caller del RenameAssetCommand (ej. CLI batch rename), evaluar si vale la pena agregar pre-validation en el comando.

### Decisión 4 — Refs id-based NO se reescriben en componentes — solo cache del AssetManager

**Contexto:** Los componentes que referencian assets lo hacen de 2 formas: por string path (ScriptComponent.path) o por AssetId resoluble via AssetManager (MeshRendererComponent.mesh, AudioSourceComponent.clip, etc). Al renombrar un asset, ¿cómo se actualizan ambos tipos de refs?

**Decisión:** Las refs id-based NO se reescriben en cada componente. Solo se actualiza el path interno del AssetManager via `renameLogicalPath(oldPath, newPath)`. El id no cambia; el componente sigue apuntando al mismo id. La próxima llamada a `pathOf(id)` devuelve el path nuevo automáticamente. Solo las refs string-path (7 tipos específicos) se reescriben directamente en el componente.

**Razones:**
1. **Rendimiento masivo**: si 500 entities tienen MeshRendererComponent apuntando al mismo mesh `barrel.fbx`, renombrar a `crate.fbx` con id-based update requiere TOCAR 1 línea (el `m_meshes.rename(id, "crate.fbx")` en el AssetManager). Con string-path update, requeriría iterar 500 componentes. El factor 500x es claramente la decisión correcta.
2. **Path único de verdad**: el AssetManager es el único que sabe el path real de un id. Reescribir en componentes duplicaría el path → drift posible.
3. **Save round-trip preserva el rename**: cuando el SceneSerializer escribe el .moodmap, escribe el path actual del AssetManager (`pathOf(id)`). El nuevo path queda persistido automáticamente.

**Alternativas descartadas:**
- Reescribir refs id-based en componentes (uniformidad teórica): factor 500x peor en rendimiento sin beneficio real. Si el cache del AssetManager está actualizado, el resto sigue.
- Solo reescribir refs string-path + ignorar refs id-based (cache sin update): rompe el round-trip — al save, el path viejo se persiste y al load próximo, el AssetManager carga desde el path viejo (que ya no existe en disco). Falla.

**Revisión:** Patrón validado con tests (rename de Mesh + entity con MeshRendererComponent → tras execute, `assets.meshPathOf(mr.mesh)` devuelve el nuevo path sin tocar el componente). Si emerge un caso edge donde el id apunta a un asset deleted, el comportamiento ya está cubierto por el fallback al slot 0 del AssetRegistry.

### Decisión 5 — Side-effects al reescribir paths string en componentes

**Contexto:** Cuando el comando reescribe `ScriptComponent.path = newPath`, ¿qué pasa con el estado runtime derivado? El ScriptComponent tiene `loaded = true` si el script fue cargado por ScriptSystem en el path viejo. Si solo cambiamos el path, el sistema no detecta el cambio y sigue ejecutando el script viejo.

**Decisión:** Algunos componentes con string-path tienen side-effects extra al reescribir:
- `ScriptComponent.loaded = false` → fuerza ScriptSystem a recargar el .lua con el path nuevo en el próximo tick.
- `VehicleComponent.dirty = true` → fuerza VehicleSystem a rematerializar el physics body con el config nuevo.
- `DialogComponent` / `ItemPickupComponent` / `EnvironmentComponent` / `PrefabLinkComponent`: NO tienen estado derivado de invalidar. Los sistemas correspondientes leen lazy del path y el path nuevo entra en el primer uso.

**Razones:**
1. **Consistencia entre rename + reload**: el dev espera que al renombrar un script, el sistema use el archivo nuevo. Si `loaded` queda en true, el ScriptSystem cree que ya cargó este componente y skip.
2. **Side-effect mínimo y localizado**: la invalidación es 1 boolean flip por componente afectado. No re-correr lógica pesada en el comando.
3. **Asymmetría reconocida explícitamente**: cada componente decide qué necesita invalidar — no hay regla genérica "siempre invalidar". El comando hace switch sobre RefKind y aplica los side-effects necesarios case-by-case.

**Alternativas descartadas:**
- No invalidar nada: el rename funciona "técnicamente" (path actualizado, archivo movido) pero el sistema runtime sigue con el estado viejo. UX incoherente.
- Invalidar todos los componentes uniformemente (reset de cualquier flag derivado): rompe el comportamiento de los que no necesitan invalidar (PrefabLink no tiene estado).

**Revisión:** Si se agrega un nuevo componente con string-path + estado runtime derivado, el switch del RenameAssetCommand necesita una nueva case. Patrón claro — agregar es 3-4 LOC.

---

## 2026-05-26: Consolidación de Sub-fase 3.4 (8 hitos → 5 hitos)

### Decisión 1 — Consolidación agresiva post-F3H18

**Contexto:** Al cerrar F3H18 (5/6 de Sub-fase 3.3), el dev pidió revisar los hitos restantes del plan F3 para detectar oportunidades de unir hitos y reducir el overhead de "cierre + commit + tag" por hito (cada cierre toma ~30-60 min de docs + validation). El plan original tenía 27 hitos totales (F3H1-F3H27), de los cuales 18 están cerrados y 9 pendientes (F3H19 + F3H20-F3H27 = Sub-fase 3.4 completa).

**Decisión:** Consolidación agresiva validada via `AskUserQuestion`. Aplica 3 uniones a la Sub-fase 3.4, llevando de 8 hitos a 5:
- F3H21 (ex-Cámaras numpad) + F3H22 (ex-Modos visualización) → **F3H21 Viewport pro**.
- F3H23 (ex-Profiler) + F3H24 (ex-Stats overlay) → **F3H22 Performance feedback**.
- F3H25 (ex-Console mejorada) + F3H26 (ex-Toasts) → **F3H23 Comunicación al dev**.
- F3H27 (Crash recovery) → **F3H24** sin cambios.

F3H19 (Rename con cascada, cierre de Sub-fase 3.3) + F3H20 (Snapping configurable) quedan sin cambios. **Total Fase 3: 27 → 24 hitos.**

**Razones:**
1. **Infra compartida real** (F3H22 + F3H23): Profiler y Stats overlay leen las mismas métricas runtime (FPS/drawcalls/triangle count/GPU mem); Console y Toasts comparten el pipeline de log severity. Unirlos evita duplicar lectura/wiring de métricas + duplicar handlers de log filter.
2. **Proximidad UX** (F3H21): Cámaras numpad + Modos visualización son ambos features del viewport para workflow del dev — el dev típicamente está agregando "comandos del viewport" en una sesión cuando agrega uno, agregar el otro en la misma sesión es ergonómico.
3. **Hitos siguen chicos** (~1-1.5 días cada uno): la consolidación NO infla cada hito a tamaño inmanejable. El F3H22 (Profiler + Stats) sigue siendo ~1.5 días, dentro del rango del hito mediano de F3 (compare F3H9, que tomó varios días por el bundle de 9 stages).
4. **Reduce overhead de cierre**: cada hito requiere actualizar PLAN_HITO + ESTADO_ACTUAL + HITOS + DECISIONS + tag + commit + validation con dev. 8 cierres vs 5 cierres = ahorro ~3 horas de pure overhead.

**Alternativas descartadas:**
- Sin cambios (9 hitos): cero riesgo de scope creep, pero overhead alto. El dev pidió explícitamente ver opciones de consolidación, descartando esta.
- Consolidación máxima (4 hitos): unir también F3H19 (Rename) + F3H20 (Snapping) o F3H22 + F3H23 en uno solo. Rechazada porque rompería el principio "hitos chicos y enviables" — F3H22 con todo (Profiler + Stats + Console + Toasts) sería ~3 días, demasiado grande para iteración rápida.

**Implicaciones de versionado:**
- Tags: F3H19, F3H20, F3H21, F3H22, F3H23, F3H24 — tags futuros usan los nuevos números (no los ex-números del plan original).
- PLAN_HITO_F3H<N>.md: documentos para hitos cerrados (F3H1-F3H18) NO se renumeran. Los pendientes (F3H19-F3H24) usan numeración nueva.
- `v3.0.0` (cierre de Fase 3) será al cerrar F3H24 (era F3H27).

**Revisión:** Si al arrancar un hito consolidado (ej. F3H22) se descubre que las 2 partes pelean por scope (cada una creció más de lo previsto), partir back to 2 hitos. La consolidación es decisional pre-implementación, no obligatorio durante.

---

## 2026-05-26: F3H18 cierre — Validador de assets rotos

### Decisión 1 — Cobertura inicial Tier 1 (broken refs only) vs ampliada (4 tipos)

**Contexto:** F3H18 quiere detectar problemas de refs entre assets. El plan stub identificaba 4 tipos posibles: broken refs (path no existe), load failed (carga runtime falló), schema mismatch (`.moodmap` viejo), oversized files (>N MB). Cada uno requiere lógica distinta + UI distinta + tests distintos.

**Decisión:** Tier 1 = broken refs (paths que no resuelven en disco) + LoadFailed reservado como enum value para Tier 2 futuro pero no detectado todavía. Validado con el dev via `AskUserQuestion` antes de empezar.

**Razones:**
1. **Cubre el 90% del caso real**: el dev pierde refs al renombrar / mover / borrar archivos externamente. Eso es lo que el validador necesita cazar primero.
2. **Schema mismatch requeriría infra propia**: cada schema tiene su versión + upgrader. Detectar mismatch requiere reflexionar el schema versioning + plantear policy de migración. Scope hito propio.
3. **Oversized files es policy decisional**: ¿cuál es el cap razonable? 50 MB PNG? 200 MB FBX? Sin demanda concreta del dev, hardcodear cualquier número es premature. Backlog si emerge.
4. **Flujo end-to-end testeable**: F3H18 entrega el flujo completo (scanner → panel → badge → highlight inline). Cobertura ampliada hereda esa infra cuando emerja sin re-diseñar nada.

**Alternativas descartadas:**
- Tier 1 + LoadFailed detectado: el LoadFailed real requiere instrumentar el AssetManager para registrar paths que intentaron cargar pero cayeron al fallback (`missingX()`). Hoy esa info se pierde después del log warn. Refactorear AssetManager para retener fail-tracking es scope mayor.
- Cobertura ampliada toda junta: 4 features distintos = 4 sub-hitos. Inflaría F3H18 a 2 semanas. El plan F3 prefiere hitos chicos y enviables.

**Revisión:** si el dev abre un proyecto pre-F3 y se queja "no veo qué assets están viejos del schema", agendar F3H_schema_validator. Si abre un proyecto con `assets/textures/4k_uncompressed.png` y nota lag → F3H_oversized.

### Decisión 2 — Engine `AssetValidator` agnóstico a i18n

**Contexto:** El `AssetIssue.detail` necesita ser texto legible que el dev vea en el panel ("Script no encontrado en disco" / "Script not found on disk"). Opción 1: el engine resuelve i18n al armar el issue. Opción 2: el engine devuelve la i18n key (string como `"editor.asset_validator.detail.script"`), el panel UI resuelve a runtime.

**Decisión:** Opción 2. `AssetIssue.detail` es la i18n key. El panel resuelve via `I18n::T(detail.c_str())`.

**Razones:**
1. **Engine no depende de I18n**: la capa `src/engine/` no incluye `core/i18n/`. Mantenerla agnóstica permite que `AssetValidator` sea reusable por MoodPlayer (no necesita texto), por CLI tooling (validar headless en CI), o por scripts de migración batch.
2. **El locale activo NO viaja con el AssetManager**: el AssetManager corre por instancia del editor, pero un mismo proyecto podría validarse desde Spanish + English + headless CI. El detail como key separa el dato del idioma.
3. **Panel UI ya está integrado a I18n**: resolver `I18n::T(key)` es 1 llamada por issue durante el render del panel. N pequeño (típicamente <20), no es bottleneck.
4. **Cambiar la key rompe el panel ↔ permite freeze del contrato**: el test `F3H18: AssetIssue.detail es i18n key estable` valida que el código nunca cambie la key silenciosamente — si alguien la renombra, el test falla y obliga sync con `es.json` / `en.json`.

**Alternativas descartadas:**
- Engine resuelve i18n: rompe capa + duplica el work en cada AssetValidator instance + el reusable cross-frontend muere.
- Engine devuelve enum tipado (`IssueKind::ScriptMissing` etc): mejor que string pero infla el header con N variantes. La string-as-key es indirecta natural para i18n.

**Revisión:** si emerge demanda de MoodPlayer mostrando issues en runtime con i18n localizado, el panel ya enseña el patrón. Si CI validation reporta plain English, podemos agregar un `formatIssue(issue, locale)` helper.

### Decisión 3 — Scan on-demand (refresh manual + post-open) vs continuo

**Contexto:** El validador puede correr en distintos momentos: cada frame (continuo), cuando algo cambia en la Scene (selectivo invalidate), al abrir proyecto (one-shot), o solo cuando el dev pide (manual refresh).

**Decisión:** Scan on-demand. Se ejecuta SOLO al abrir proyecto (`tryOpenProjectPath` post-load) + cuando el dev clickea "Refrescar" en el panel.

**Razones:**
1. **Costo del scan es O(entities + materials)**: ~5 ms para proyectos medianos (200 entities + 50 materials). Aceptable de pagar 1 vez al abrir; inaceptable como 60 ms/segundo continuo.
2. **El dev no necesita feedback frame-perfect**: las refs muertas son condición statica del proyecto. Renombrar un archivo externamente NO se notifica automáticamente al editor (filesystem watch sería otro feature). El dev sabe cuándo cambió algo y puede refresh.
3. **Si el dev edita un InputText path en el Inspector ↔ el badge queda stale unos segundos**: trade-off explícito. La alternativa "invalidate al editar" requiere instrumentar cada InputText con un callback al validator — complejidad alta para un caso edge (el dev típicamente abre el panel después de editar para verificar).
4. **Post-open es donde más valor agrega**: cuando el dev abre un proyecto dormido o que recibió cambios desde otra rama, la primera vista del editor incluye el badge si hay issues. UX óptimo para el caso primario.

**Alternativas descartadas:**
- Scan continuo: O(N) per frame es overhead permanente sin valor proporcional. Profiler mostraría 0.1-0.5% del frame budget gastado en algo que no cambia.
- Invalidate selectivo (al editar InputText, marcar dirty + refresh next frame): complejidad alta, beneficio marginal. Agendar si dev se queja del stale.
- Background polling cada N segundos: thread complexity por feature de baja prioridad. No vale la pena.

**Revisión:** si el dev reporta "edité el path y el badge no actualiza, me confunde", agregar `requestRefresh()` desde el handler del InputText de path strings. Trivial — el helper `requestRefresh` ya existe en `AssetIssuesPanel`.

### Decisión 4 — Helper inline solo en 2 sites (Script + Vehicle) en F3H18, diferir el resto

**Contexto:** El helper `detail::inspectorBrokenRefBorder(EditorUI*, Entity, const std::string& path)` es reusable en cualquier widget drop-target / InputText del Inspector. Sites posibles: Script.path, Vehicle.configPath, MeshRenderer materials/mesh, Animation externalClips, Inventory items, Audio.clip combo, Dialog.dialogPath, ItemPickup.itemPath. Total ~8 sites.

**Decisión:** F3H18 instrumenta solo en 2 sites (Script + Vehicle). Los otros 6 quedan como backlog mecánico.

**Razones:**
1. **Extender es 1 LOC por site**: `detail::inspectorBrokenRefBorder(m_ui, e, path);` después del widget. No agrega capacidad nueva — el helper ya existe.
2. **Inflar el diff sin agregar capacidad** dificulta el code review. F3H18 entrega: validator + panel + badge + helper + 2 sites como prueba de concepto. Extender a los 6 restantes vale por sí solo cuando emerja demanda.
3. **Algunos sites tienen UI compleja (BeginCombo + Selectable inline)** donde el `IsItemHovered` para tooltip puede colisionar con tooltips existentes. Hacerlo bien requiere caso-por-caso. F3H18 prefiere 2 sites bien hechos a 8 sites con bugs.
4. **Backlog explícito en código + memoria**: si el dev pide "extender el highlight a MeshRenderer slots", el follow-up es trivial.

**Alternativas descartadas:**
- Extender a los 8 sites en F3H18: scope creep. Cada site agregaría 5-15 LOC al hito sin tests propios (el helper ya está testeado).
- No agregar inline highlight en F3H18 (solo panel + badge): pierde el UX feedback de borde rojo en el field donde el dev edita. El panel está OK pero el dev tendría que abrir el panel + recordar qué arreglar.

**Revisión:** memoria `[[asset_validator_inline_coverage]]` agenda los 6 sites diferidos. Activar cuando un site específico genere fricción.

### Decisión 5 — Reporte por entity (con "Ir a" entity), Material issues quedan con botón disabled

**Contexto:** Los issues vienen de 2 fuentes: (a) componentes con refs en una entity (Script/Dialog/etc — entity asociada); (b) Material assets cacheados con texture refs muertas (sin entity directa). El panel necesita acción de "Ir a" para que el dev navegue al fix.

**Decisión:** El `AssetIssue.entity` lleva la entity source (cuando aplica). El botón "Ir a" funciona en entities (selecciona en Hierarchy + Inspector). Para issues de Material cacheado, `entity` queda falsy y el botón "Ir a" queda disabled con tooltip ("Esta referencia no pertenece a una entidad — revisar en el panel Asset Browser").

**Razones:**
1. **El dev típicamente repara desde la entity**: cambia el path en el Inspector (InputText con border rojo de F3H18), o reemplaza el componente. El "Ir a entity" cubre ese flow directo.
2. **Material refs no tienen flow "Ir a entity" obvio**: un Material vive en `AssetManager`, no en Scene. Si el dev quiere reparar la textura del material, debe abrir el Material Editor — flow distinto. El `usedBy = "Material: <path>"` da al dev el nombre del material para abrir manualmente.
3. **Disable + tooltip honesto > botón clickeable que no hace nada**: ofrecer una acción que no funciona confundiría más que no ofrecerla. El tooltip explica por qué.

**Alternativas descartadas:**
- "Ir al asset" (abrir Material Editor con ese material): scope hito propio. Requiere wire del panel al Material Editor + foco al material. Por ahora "abrir Asset Browser y buscarlo manualmente" es aceptable.
- No mostrar issues de Material cacheado: cubrir todos los issues es valor — el dev quiere saber qué necesita arreglar, incluso si requiere acción manual.

**Revisión:** si el dev pide "click en este issue de material me debería llevar al Material Editor", agendar `pendingSelectAsset` + wire al panel correspondiente. Patrón gemelo de `pendingSelect` entity.

---

## 2026-05-26: F3H17 cierre — Drag & drop con feedback visual

### Decisión 1 — Helper compartido `DragDropFeedback` vs duplicar en cada panel

**Contexto:** F3H17 agrega 4 piezas de feedback visual de drag&drop usadas en ≥8 sites del editor:
1. Halo overlay (cyan/verde) alrededor del último ítem dibujado — usado en 6 Inspector slots + Material Editor.
2. Halo de rect (similar) — primer intento incluía borde del viewport, descartado tras feedback.
3. Detección de "drag activo de tipo X" — usado en cada slot drop target.
4. Cancel con Esc — usado globalmente en EditorApplication.

Opciones: (a) duplicar las 4 lambdas en cada panel; (b) helper compartido en un header reusable.

**Decisión:** Helper compartido — `src/editor/ui/DragDropFeedback.h` (header-only, ~100 LOC). 5 funciones inline: `isViewportDragActive`, `drawDropHalo`, `isDragActiveOfType`, `drawItemDropHalo`, `cancelDragOnEscape`.

**Razones:**
1. **Cero duplicación**: el color (`IM_COL32(80, 180, 255, 200)` cyan + `IM_COL32(80, 230, 130, 230)` verde), el thickness (3 px viewport / 2 px slots), y los `kViewportSupportedTypes[7]` viven en UN solo lugar.
2. **Mantenible**: cuando un hito futuro cambie la convención (ej. color), se cambia una sola constante.
3. **Header-only**: 5 funciones cortas, ningún state. No paga linker overhead; cada cpp que incluya recibe inlines.
4. **Reusable fuera del Inspector**: `Material Editor` también lo usa para los texture slots — sin el helper habría que importar `InspectorPanel_Internal.h` cross-module, lo cual rompía capa.

**Alternativas descartadas:**
- Duplicar en cada panel: 4-5 LOC × 8 sites = ~32 LOC de duplicación + drift inevitable cuando alguien polish uno y se olvida del resto.
- Meterlo en `InspectorPanel_Internal.h`: el Material Editor no es parte del Inspector; importar `Internal.h` cross-module ensucia el header del Inspector y rompe encapsulación. F3H17 quería un helper que pueda usar el Material Editor sin importar Inspector internals.

**Revisión:** si el helper crece > 200 LOC, partir a `DragDropFeedback.h` + `DragDropFeedback.cpp` (state si emerge).

### Decisión 2 — Cancel con Esc vía API interna de ImGui

**Contexto:** F3H17 quiere que Esc cancele un drag activo (convención universal: Photoshop / Blender / Unity / VS Code). ImGui no expone API pública para cancel — su filosofía es "el dev puede soltar el botón del mouse" (declinación documentada en https://github.com/ocornut/imgui/issues/1717).

**Decisión:** Acceder a la API interna via `<imgui_internal.h>` + setear `GImGui->DragDropActive = false` + limpiar `DragDropPayload` + resetear `DragDropAcceptIdCurr/Prev` + resetear `DragDropSourceFlags`.

**Razones:**
1. **Convención universal**: todos los editores DCC tienen Esc-cancel-drag. Aceptar la limitación de ImGui rompe expectation del dev.
2. **API interna estable**: `DragDropActive` existe sin breaking change desde la inception del drag&drop module en ImGui 1.66 (2019). Acceder a internals es un trade-off documentado y aceptado por la comunidad de ImGui (el header `imgui_internal.h` está públicamente disponible precisamente para esto).
3. **Cleanup completo**: solo poner `DragDropActive = false` deja state residual (payload + acceptId del frame anterior). Limpiar los 4 fields garantiza que el frame siguiente ningún target acepta el payload "fantasma".

**Alternativas descartadas:**
- Sin Esc-cancel: requiere mover el mouse fuera del editor antes de soltar (frágil — un click accidental sobre target válido = drop no deseado).
- Forkear ImGui para exponer `CancelDragDrop()` público: maintain overhead enorme para 5 líneas de cambio.
- PR upstream a ImGui: agendar para futuro (issue #1717 lleva 5 años abierta — no es probable que se merge en el corto plazo).

**Revisión:** si una versión futura de ImGui cambia el nombre o struct de `DragDropActive`, romper compile time → adaptar. Riesgo bajo dada la estabilidad histórica.

### Decisión 3 — Halo del borde del viewport eliminado tras feedback del dev

**Contexto:** El primer intento de F3H17 dibujaba un halo cyan/verde sobre el **borde del panel viewport** (`drawDropHalo` llamado desde `ViewportPanel.cpp` tras `EndDragDropTarget`) — pensé que daba feedback complementario al highlight 3D existente. Dev reportó visualmente al validar: *"el halo cyan no debería aparecer sobre el área que afectaré, me refiero si arrastro una textura y la idea es que un plano tome esa textura no debería ese plano tener el halo cyan?"*.

**Decisión:** Eliminar el halo del borde. El feedback de drag-over-viewport pasa exclusivamente por el **highlight 3D sobre el target específico** (entity bajo cursor con AABB cyan, o tile bajo cursor con cubo cyan). Mantener halo solo en Inspector slots (donde sí tiene sentido — los slots no son obvios sin la pista visual).

**Razones:**
1. **Doble feedback = ninguno**: el halo del borde era visualmente prominente y el dev no veía el highlight 3D ya pintado. Quitar lo prominente para que lo informativo se vea.
2. **Lenguaje del editor coherente**: "el target específico se ilumina" >> "el panel acepta". Otros editores (Unreal, Substance) usan target-specific feedback, no panel-level glow.
3. **Inspector slots SÍ necesitan halo**: a diferencia del viewport (área grande y obvia), un slot del Inspector es un cuadrito chico y el dev no sabe si acepta este tipo de payload sin la pista visual.

**Alternativas descartadas:**
- Halo del borde sutil (thickness 1 px): aún distrae sin agregar info útil.
- Halo del borde solo cuando el target 3D NO está pintado: complejidad innecesaria — si no hay target debajo, el dev igual ve el cursor de ImGui con el payload preview, eso basta.

**Revisión:** si el dev reporta "no veo cuándo entró el cursor al viewport durante drag", reconsiderar — quizás un cambio sutil del cursor (sombra/glow del payload preview de ImGui) es mejor que el halo del borde.

### Decisión 4 — Lenguaje visual unificado a cyan brillante para todos los drag targets

**Contexto:** Pre-F3H17 el editor tenía 2 convenciones distintas para drag-over feedback:
- **Cubo cyan** (`vec3(0.2, 0.9, 1.0)`) sobre tile bajo cursor — para Texture/Mesh/Prefab drag.
- **OBB amarillo** (`vec3(1.0, 0.95, 0.15)`) sobre entity bajo cursor — para Material/Script drag.

El amarillo era residual de F2 cuando el highlight de entity era visualmente distinto del de tile.

**Decisión:** Unificar a **AABB cyan brillante** (`vec3(0.30, 0.85, 1.0)`) en ambos casos. Eliminar el OBB amarillo + el loop manual de 12 líneas; usar `dbg.drawAabb(world_aabb, kDragCyan)` directo (3-4 líneas).

**Razones:**
1. **Lenguaje visual coherente**: 1 color = drop target. El dev no piensa "amarillo significa entity, cyan significa tile" — piensa "cyan = ahí cae el drop".
2. **Match con halo de Inspector slots**: los slots también usan cyan (`IM_COL32(80, 180, 255, 200)`). Mismo lenguaje en todo el editor.
3. **Menos código**: el helper `brushAabbWorld` / `meshAabbWorld` (públicos en `ScenePick.h` desde F2H31 Bloque B) + `dbg.drawAabb` reemplaza ~20 LOC de matriz × 8 corners + 12 drawLine.
4. **Amarillo es color de selección de cara** (Face Mode F2H17). Reservar el amarillo para selección — no para drag-over.

**Alternativas descartadas:**
- Mantener 2 colores distintos: el dev ya tenía drag&drop functioning, pero el lenguaje no era enseñable ("cuándo es amarillo vs cyan, otra vez?"). El dev se beneficia de una sola convención.
- Highlight diferente (outline, glow): los 3 estilos los probó F2 y el AABB ganó por simplicidad de implementación. F3H17 no necesita rediseñar el estilo, solo el color.

**Revisión:** si un futuro hito agrega multi-target drag (ej. brush paint sobre N tiles a la vez), considerar shade gradient entre targets — pero un solo color sigue siendo correcto.

### Decisión 5 — Texture drag highlight extendido a Brush, NO a MeshRenderer suelto

**Contexto:** Pre-F3H17 el flow real de `processViewportTextureDrop` (`DemoSpawners_Drop.cpp:206`) tenía 2 paths: (a) si hit entity con `BrushComponent` → asigna textura al material del brush; (b) sino → tile pick → pinta tile con la textura. Mesh entities sueltas con `MeshRendererComponent` (sin BrushComponent) caen al path (b) tile pick.

F3H17 quería extender el highlight visual a algún target además de tile. Opciones:
- (i) Highlight solo Brush — match exacto del behavior del handler.
- (ii) Highlight Brush O MeshRenderer — sugiere al dev que mesh entities también aceptan, aunque el handler no asigne.
- (iii) Highlight Brush + extender el handler a también asignar a MeshRenderer.

**Decisión:** Opción (i). Highlight visual solo cuando hit Brush; mesh entities sueltas siguen cayendo al cubo cyan del tile.

**Razones:**
1. **Consistencia visual ↔ behavior**: si pintamos highlight cyan sobre un mesh entity, el dev espera que al soltar se asigne. Si el handler ignora y cae al tile, el dev se confunde ("¿por qué se asignó al tile y no al mesh?"). Mantener visual = behavior es respeto al modelo mental.
2. **Cambio de comportamiento es scope de hito propio**: extender Texture drop a MeshRenderer es decisión arquitectónica con preguntas abiertas (¿reemplazar slot 0 del MeshRenderer? ¿crear material wrapper como con Brush? ¿afecta otros slots?). F3H17 es "drag&drop con feedback visual" — no extender funcionalidad.
3. **El feedback de tile sigue funcionando**: el dev no pierde info — el cubo cyan sobre el tile aparece donde su drop caerá. Solo no se pinta sobre el mesh suelto (correcto, no recibe el drop).

**Alternativas descartadas:**
- Opción (ii) (highlight sin behavior): rompe la regla "visual = behavior".
- Opción (iii) (extender handler): scope de F3 hito propio si el dev lo pide. No es trivial — Brush usa `createMaterialFromTexture` que es helper específico del brush flow.

**Revisión:** si el dev pide "quiero que al arrastrar textura sobre un mesh asigne al primer slot", agendar como hito (probablemente F3H17b o F3H_extras).

---

## 2026-05-26: F3H16 cierre — Hover preview ampliada del Asset Browser

### Decisión 1 — Timer manual vs `ImGuiHoveredFlags_DelayNormal`

**Contexto:** ImGui 1.89+ tiene `ImGuiHoveredFlags_DelayNormal` que retorna `true` desde `IsItemHovered()` solo después de `style.HoverDelayNormal` (~0.4s). Sería la solución "obvia" para implementar hover delay.

**Decisión:** Manual timer. Trackeamos `m_hoverItemKey` + `m_hoverTimerSec` en el panel, acumulamos `DeltaTime` cuando `IsItemHovered()` retorna `true` con el mismo item, reseteamos al cambiar de item o salir.

**Razones:**
1. **Delay configurable per-usuario**: queremos que `UserSettings.editor.hoverPreviewDelayMs` controle el delay. `HoveredFlags_DelayNormal` lee de `style.HoverDelayNormal` que es GLOBAL — mutarlo cada frame afecta TODOS los hovers del editor (combos, tooltips de otras secciones, etc).
2. **Configuración granular**: una pref controla el delay del Asset Browser específicamente. Si en el futuro otros panels quieren hover preview con diferente delay, cada uno tiene su lógica sin afectar al otro.
3. **0 ms a 3000 ms range**: el manual timer soporta 0 (instantáneo) que `HoveredFlags_DelayNormal` no haría — ese flag siempre espera el delay configurado en style.
4. **Sin dependencia de versión específica de ImGui**: el flag requiere 1.89+. Manual funciona con cualquier versión.

**Alternativas descartadas:**
- `HoveredFlags_DelayNormal` con mutación de `style.HoverDelayNormal` antes de cada `IsItemHovered`: ensucia el style global; afecta cualquier otro lugar que lea ese valor en el mismo frame.
- Tracker per-item (cada thumb su propio timer): solo uno puede estar hovered a la vez en ImGui — un single tracker en el panel es suficiente y más simple.

**Revisión:** si emerge UX donde el dev quiere "delay default ImGui para algunos tooltips, delay propio para otros", revisitar. Hoy un solo delay para los thumbs del Asset Browser cubre el 100% del caso.

### Decisión 2 — `kLargePreviewSize = 384` hardcoded vs configurable

**Contexto:** El tooltip ampliado renderea el thumb a 384×384 px. Opción alterna: exponer una pref `UserSettings.editor.hoverPreviewSize` (clamp 256-768, default 384) con slider.

**Decisión:** Hardcoded `static constexpr u32 kLargePreviewSize = 384u` en ambos renderers. Sin pref.

**Razones:**
1. **UX consistente**: el dev no quiere pensar "¿cuán grande debería ser mi preview?" — 384 es un sweet spot probado (Substance Designer ~ 360, Marmoset ~ 400). Una sola UI menos.
2. **Cache disco consume espacio**: cada size genera PNGs nuevos. Permitir al dev cambiar 384→512→640 deja 3 sizes en disco sin invalidar los viejos (filename incluye size). Hardcoded mantiene el footprint chico.
3. **Sin caso de uso identificado**: nadie pidió "tooltip más grande" o "más chico". Si emerge, mover a pref después es trivial — los thumbs grandes ya están parametrizados internamente.
4. **Tooltip ya respeta layout de ImGui**: 384px se ve bien en pantalla 1080p (no ocupa más de la mitad) y en 4K (no se ve micro porque el viewport del thumb es proporcional al espacio del cursor).

**Alternativas descartadas:**
- Pref con slider: agrega complejidad por configurabilidad sin demanda.
- Auto-calcular según resolución de pantalla: heurística sin caso de uso claro; el dev podría tener 4K con el Asset Browser en una ventana chica donde 384 quedaría grande.

**Revisión:** si un dev pide explícitamente "preview más chico para mi laptop" o "más grande para mi 4K", agregar `hoverPreviewSize`.

### Decisión 3 — Helper compartido `loadOrRenderThumb`/`loadOrRenderMatThumb` vs duplicar el flow load/render/cache

**Contexto:** F3H14 había implementado el flow load disco / render / cache memoria / store disco en `thumbnailFor`. F3H16 necesita el mismo flow para `thumbnailLargeFor` con un size + cache map distintos. Opciones: (A) duplicar el flow entero copy-pasted; (B) extraer a helper privado que toma `size` + `cache map` como parámetros.

**Decisión:** Opción B — `loadOrRenderThumb(meshId, assets, size, cacheMap)` privado, llamado por ambos `thumbnailFor` y `thumbnailLargeFor`. Mismo patrón en `MaterialPreviewRenderer` con `loadOrRenderMatThumb`.

**Razones:**
1. **DRY estricto**: el flow es 100% idéntico — solo varían los 2 parámetros explícitos. Duplicar ~80 LOC por método nuevo significa que cada mejora futura del flow (compresión, logging, async) requiere 2 toques en lugar de 1.
2. **`size` ya estaba parametrizado en disco**: `AssetThumbnailDiskCache::pathFor` desde F3H14 acepta `size` en su signature. Pasarlo desde más arriba en la stack es 1 línea más.
3. **Cache map por puntero/referencia**: pasar `std::unordered_map&` permite que el caller decida qué cache poblar. No hay encapsulación rota — el helper es privado.

**Alternativas descartadas:**
- Duplicar el flow: bug-prone (cada vez que un branch se actualiza, hay que recordar tocar las 2 copias).
- Macro o template: overkill para 2 call-sites con un solo tipo (`u32` cache key, `unique_ptr<OpenGLFramebuffer>` value).

**Revisión:** si emerge un tercer caso de uso (ej. thumb extra small para list compacto), el helper escala — solo agregar otra pareja `m_xsCache` + `thumbnailXsFor`.

### Decisión 4 — Cobertura inicial mesh + material (no animaciones/prefabs/texturas/audio)

**Contexto:** El Asset Browser tiene tabs adicionales (Animations, Prefabs, Textures, Audio, Scripts, Vehicles). El plan de Sub-fase 3.3 menciona "hover preview ampliada" sin detallar qué tipos.

**Decisión:** F3H16 cubre **solo mesh + material**. Animations/Prefabs/Textures/Audio/Scripts/Vehicles quedan en backlog.

**Razones:**
1. **Mesh + material son los 2 paneles más visitados**: el dev pasa más tiempo eligiendo meshes para spawnear y materiales para asignar que escogiendo prefabs/animations/etc. Cobertura del caso 80%.
2. **Texturas ya tienen preview real**: la textura es su propia imagen. Hover ampliado para texturas es trivial (sería `ImGui::Image` con la GLuint de la textura cargada). Lo dejo para un follow-up de 1 línea cuando emerja.
3. **Animations tienen un preview LIVE distinto**: el `AnimationPreviewRenderer` de F2H81 ya renderea el NPC posado en tiempo real cuando seleccionás un clip — el "hover ampliado" requeriría un preview pose-estático, distinto al render dinámico que ya existe.
4. **Audio + Scripts no tienen preview visual obvio**: audio waveform o script syntax-highlighted son features de hito propio.
5. **Infraestructura lista para extender**: agregar otro tipo es solo un nuevo prefix en `AssetThumbnailDiskCache::pathFor` + helper analógico en el respectivo render.

**Alternativas descartadas:**
- Cobertura total en F3H16: infla scope sin payoff visible para tipos poco visitados.
- Solo mesh (sin material): F3H15 acababa de agregar la infraestructura paralela; tener mesh sin material rompe la simetría que se acababa de establecer.

**Revisión:** cuando un dev mencione "quiero hover de X en el Asset Browser" para un tipo nuevo, agregar el helper espejo.

---

## 2026-05-26: F3H15 cierre — Mejoras del MaterialPreviewRenderer (cache disco compartido + resolución compartida + gradiente reusado)

### Decisión 1 — Helper compartido `AssetThumbnailDiskCache` con `prefix` vs caches separadas

**Contexto:** F3H14 creó `MeshThumbnailDiskCache` específico para meshes. F3H15 necesita una cache disco análoga para materiales. La opción "obvia" era crear `MaterialThumbnailDiskCache` espejo, copiando el algoritmo entero.

**Decisión:** Renombrar `MeshThumbnailDiskCache` → `AssetThumbnailDiskCache` (más genérico) + agregar parámetro `prefix` en `pathFor` para discriminar tipos. Filename: `<prefix>_<hash>_<size>.png` con `"mesh"` o `"mat"` (o futuros `"anim"`, `"audio"`).

**Razones:**
1. **Algoritmo 100% idéntico**: FNV-1a, mtime check, stbi_load/write, flip vertical. La única diferencia entre las 2 caches es el filename prefix.
2. **Evitar drift**: si en el futuro se agrega features al cache (compresión PNG distinta, sidecar opcional, etc.), un solo lugar para tocar. Caches separadas habrían divergido por inevitable copy-paste mantenido a mano.
3. **Extensibilidad gratis**: agregar un thumb de animaciones o de audio waveforms = 1 nuevo prefix, cero código nuevo de cache.
4. **Migración trivial**: el call-site existente del mesh (`MeshThumbnailRenderer`) pasa de `pathFor(root, logical, size)` a `pathFor(root, "mesh", logical, size)` — 1 token de diff.

**Alternativas descartadas:**
- Caches separadas (`MeshThumbnailDiskCache` + `MaterialThumbnailDiskCache`): duplicación pura, ~200 LOC para mantener sincronizadas.
- Cache abstracta con jerarquía de clases: overengineering para un detalle (prefix) que solo varía como literal.

**Revisión:** si emerge una distinción real entre cómo se cachea meshes vs materiales (ej. el material necesita lighting state serializado adentro del archivo), revisitar split.

### Decisión 2 — Una sola pref `thumbnailResolution` afecta meshes + materiales

**Contexto:** F3H14 introdujo `UserSettings.editor.thumbnailResolution` para los thumbs de meshes. Una opción para F3H15 era agregar una pref hermana `materialThumbnailResolution` (config independiente per-tipo).

**Decisión:** Reusar la misma pref. Un solo slider en User Preferences afecta los DOS renderers a la vez.

**Razones:**
1. **UX consistente**: el dev no piensa "qué resolución para meshes / qué resolución para materiales" — piensa "quiero thumbs más grandes". Un slider con un significado claro.
2. **Menos ruido en Preferences**: 1 slider en lugar de 2.
3. **Caso de asimetría hipotético**: no hay caso de uso actual donde el dev quiera meshes a 96 y materiales a 256. Si emerge, agregar la pref específica después.
4. **Implementación más simple en `EditorApplication_Run.cpp`**: el bloque de live recreate se amplía a recrear AMBOS renderers en un solo `if (desired != m_lastThumbnailResolution)`. Dos prefs separadas requerirían tracking de `m_lastMeshRes` + `m_lastMatRes` independientes.

**Alternativas descartadas:**
- Pref por tipo: aumenta superficie de configuración sin caso de uso identificado.
- Pref global + override per-tipo: más complejo aún, sin payoff visible.

**Revisión:** si un dev pide explícitamente "materiales más grandes que meshes en mi 4K", agregar `materialThumbnailResolutionOverride` (opcional, default = global pref).

### Decisión 3 — Shader `thumbnail_bg` compartido entre los 2 renderers, instancias separadas

**Contexto:** F3H14 creó `shaders/thumbnail_bg.vert/frag` para el gradient del fondo del MeshThumbnailRenderer. F3H15 también necesita ese gradient para el MaterialPreviewRenderer. Opciones: (A) singleton del shader compartido; (B) cada renderer carga su propia instancia.

**Decisión:** Opción B — instancias separadas. Cada renderer hace `std::make_unique<OpenGLShader>("shaders/thumbnail_bg.vert", "shaders/thumbnail_bg.frag")` en su propio constructor.

**Razones:**
1. **Shader trivial**: vert + frag son ~30 LOC totales, instanciar 2 veces es ~ms de overhead al boot.
2. **Lifecycle simple**: cada renderer maneja su `unique_ptr<IShader>` y lo destruye con su `~` propio. No hay que sincronizar quién es dueño del shader compartido.
3. **Patrón consistente con `m_pbrShader`**: ambos renderers YA tenían su propio `m_pbrShader` por instancia (no compartido). Mantener el mismo patrón para `m_bgShader`.
4. **Recreación al cambiar resolución (decisión D3 de F3H14)**: al cambiar la pref, los renderers se destruyen y recrean. Un singleton del shader sobreviviría la recreación; instancias propias mueren con el renderer y se reconstruyen — simpler reasoning.

**Alternativas descartadas:**
- Singleton del shader: complica el ownership; el shader vive más allá del último renderer que lo usa.
- Compartir vía puntero crudo inyectado: el caller (EditorApplication) tendría que cargar el shader y mantenerlo vivo. Más wiring.

**Revisión:** si el editor crece a tener 10+ renderers que reusan este shader, considerar shader registry o pre-warm cache.

### Decisión 4 — Filename con prefix `mat_` (no `material_`)

**Contexto:** Prefix del filename para discriminar materiales de meshes en el cache disco. Opciones: `mat_<hash>_<size>.png` vs `material_<hash>_<size>.png`.

**Decisión:** `mat_`. 4 caracteres ahorrados x N archivos en el directorio.

**Razones:**
1. **Listing más legible**: con 50+ thumbs en `<proyecto>/.cache/thumbs/`, columnas alineadas se ven mejor con prefijos cortos. `mesh_` y `mat_` son visualmente parejos.
2. **Tradicion**: shorts prefixes tipo `mat`, `tex`, `obj` son convención en muchos engines (Substance, Unreal asset naming).
3. **Sin colisión**: `mat` no se confunde con otros tipos planeados (`mesh`, `anim`, `audio`).

**Alternativas descartadas:**
- `material_`: más explícito pero infla el filename sin valor de claridad (el contexto del directorio `thumbs/` ya hace obvio que son thumbs).
- `m_` o `t_`: demasiado críptico, futuro grep difícil.

**Revisión:** si emerge ambigüedad (ej. `mat_` confunde con "matrix"), renombrar y agregar comando de migración.

---

## 2026-05-26: F3H14 cierre — Mejoras del MeshThumbnailRenderer (cache disco + resolución + gradiente + mtime)

### Decisión 1 — Filename incluye `_<size>` (coexistencia de resoluciones)

**Contexto:** El cache disco persiste PNGs entre sesiones. Al cambiar `thumbnailResolution` (64 → 128 → 256), ¿qué pasa con los PNGs viejos del size anterior?

**Decisión:** El filename incluye el size: `mesh_<hash>_<size>.png`. Cambio de resolución NO invalida nada — los PNGs viejos quedan en disco bajo otro nombre. Hit/miss se evalúa por (logicalPath, size).

**Razones:**
1. **Idempotencia trivial**: cambiar el slider 128→256 no requiere "limpiar el cache" — los nuevos PNGs se nombran distinto y conviven sin chocar.
2. **Reversibilidad gratuita**: si el dev vuelve a 128 después de probar 256, los 128 viejos siguen ahí (cache HIT instantáneo).
3. **Simpleza**: alternativa "borrar PNGs del size viejo al cambiar" agrega manejo de errores y lifecycle que el hito chico no justifica.

**Alternativas descartadas:**
- Filename sin size (`mesh_<hash>.png`) + invalidación masiva al cambiar resolución: requiere iterar el directorio + delete, propenso a fallar si hay file locks o permisos.
- Subdirectorios por size (`.cache/thumbs/128/mesh_<hash>.png`): más jerarquía sin ventaja real; el filename plano es más fácil de inspeccionar manualmente.

**Revisión:** si el cache crece descontroladamente (dev probando muchos sizes), agregar comando "Limpiar cache de thumbnails" en menu Debug. Backlog si el dev lo nota.

### Decisión 2 — Sin sidecar `.meta` para metadata (mtime check via filesystem)

**Contexto:** Para validar staleness del cache (mesh modificado después del PNG), una opción es escribir un sidecar `mesh_<hash>_<size>.meta` con el mtime serializado. La otra es usar `last_write_time(cachePng)` directamente del filesystem.

**Decisión:** Sin sidecar. La validez del cache se chequea con `last_write_time(cachePng) >= last_write_time(meshSource)`.

**Razones:**
1. **Menos archivos por mesh**: 1 PNG en vez de 2 archivos (PNG + meta).
2. **Atomicity natural**: `stbi_write_png` actualiza el mtime del PNG al rato de escribir. No hay window donde el PNG existe sin meta o vice versa.
3. **Race conditions evitadas**: si el dev modifica el mesh source justo cuando el editor está escribiendo el sidecar, podría quedar inconsistente. El mtime del filesystem es atómico por archivo.
4. **Patrón distinto a LodCache**: LodCache sí tiene magic + version + mtime adentro del binario porque sus archivos `.moodlod` son formato propio (versionable). Los PNGs son blobs opaque renderizables.

**Alternativas descartadas:**
- Sidecar `.meta` con JSON: agrega parsing + I/O extra; el mtime del filesystem ya tiene esa info.
- Header binario propio antes del PNG: rompe la convención `*.png` (los archivos no abrirían en visores externos para inspección manual del dev).

**Revisión:** si el comportamiento es muy sensible a relojes desincronizados entre máquinas (pull-and-test entre 2 PCs con offset NTP), reconsiderar. Por ahora una sola máquina por edit, sin issue.

### Decisión 3 — Recrear el renderer al cambiar resolución vs setter `setSize`

**Contexto:** Cambio dinámico de `thumbnailResolution` requiere que los FBOs internos se rehagan con el nuevo size. Dos opciones: (A) `m_meshThumbnails->setSize(newSize)` que internamente clear cache + actualiza miembro; (B) destruir `m_meshThumbnails` y `make_unique<MeshThumbnailRenderer>(newSize)`.

**Decisión:** Opción B — destruir y recrear el `unique_ptr<MeshThumbnailRenderer>`.

**Razones:**
1. **Constructor único como fuente de verdad**: el size se fija al construir (FBOs lazy van con `m_size`). No hace falta lógica de "qué hacer si size cambió mid-life" — el renderer nuevo arranca limpio con el size correcto.
2. **Cache memoria implícitamente limpia**: el destructor del `unique_ptr` libera los FBOs antiguos. No hace falta `clear()` explícito antes del cambio de size.
3. **Reinyección obligatoria**: IBL + diskCacheRoot + AssetBrowser apuntan a `m_meshThumbnails.get()`. Como el puntero cambia, el caller (EditorApplication) tiene que reinyectar todo — y eso fuerza a NO olvidar ningún wire (si algo nuevo se inyectara en el futuro, falla rápido).
4. **Cache disco persiste**: como el filename incluye `_<size>` (decisión D1), no hay churn en el directorio.

**Alternativas descartadas:**
- `setSize(u32)` mutador: requiere clearear cache memoria + invalidar FBOs internamente + manejar que el size puede cambiar mid-render. Más superficie de bugs.
- "Aplicar al reiniciar el editor" (Unity-style para algunas opciones): UX pobre — el dev mueve el slider y no ve nada.

**Revisión:** si recrear el renderer es notablemente lento (FPS hitch al mover el slider), considerar setSize. Hoy es ~ms por la inicialización de shaders compartidos, no se nota.

### Decisión 4 — Cache en `<proyecto>/.cache/thumbs/` vs `assets/.cache/thumbs/`

**Contexto:** `LodCache` (existente) usa `assets/.cache/lods/`. Para mantener consistencia, lo natural era poner los thumbs en `assets/.cache/thumbs/`. Pero el dev pidió explícitamente `<proyecto>/.cache/thumbs/` (raíz del proyecto, no dentro de `assets/`).

**Decisión:** Cache en `<projectRoot>/.cache/thumbs/`. NO dentro de `assets/`.

**Razones:**
1. **Pedido explícito del dev** en la decisión de scope.
2. **Gitignore limpio**: una sola línea `.cache/` en `.gitignore` del proyecto ignora todo el cache (thumbs + futuros caches). Si estuviera en `assets/.cache/`, habría que listar `assets/.cache/` específicamente (o aceptar que `assets/.cache/` aparezca como dir bajo `assets/` aunque vacío en commit).
3. **Mental model más claro**: assets son inputs del proyecto, cache es output transitorio del editor. Separar refleja la intención.
4. **LodCache es legacy**: la convención `assets/.cache/lods/` viene de F2; podría migrarse en un hito de cleanup futuro. F3H14 NO toca LodCache para no inflar scope.

**Alternativas descartadas:**
- `assets/.cache/thumbs/`: consistencia con LodCache pero contra el pedido del dev.
- `%APPDATA%/MoodEngine/thumbs/<proyecto_hash>/`: per-instalación; rechazada en la pregunta de scope inicial.

**Revisión:** si LodCache se migra a `<proyecto>/.cache/lods/` en un hito futuro, las dos caches quedan en el mismo padre.

---

## 2026-05-26: F3H13 cierre — Reset to default per-field del Inspector + cierre Sub-fase 3.2

### Decisión 1 — Helper en `InspectorPanel_Internal.h` vs duplicar el `resetButton<T>` de `ProjectSettingsPanel`/`UserPreferencesPanel`

**Contexto:** Sub-fase 3.1 ya introdujo un helper `resetButton<T>` en `ProjectSettingsPanel` (F3H4) y `UserPreferencesPanel` (F3H7) para los sliders de configuración. La opción "obvia" era extraer ese helper a un header compartido y reusarlo en el Inspector.

**Decisión:** Crear un helper **separado** `detail::inspectorResetButton<T>` en `src/editor/panels/scene/InspectorPanel_Internal.h`, distinto del de Settings/Preferences.

**Razones:**
1. **Contextos distintos**: el Inspector edita `Entity`s a través de `EditorUI` + `HistoryStack` — cada reset es un `EditPropertyCommand<T>` en el stack (Ctrl+Z debe revertir). Los reset de Settings/Preferences operan sobre **copias locales** del struct + flags `dirty/saveNow` que se persisten al disco al soltar el slider. No tocan `HistoryStack`.
2. **Firmas naturales distintas**: el del Inspector recibe `(ui, entity, idSuffix, current, default, setter, cmdLabel)`; el de Settings recibe `(buttonId, current, default, settersWithDirtyFlag)`. Generalizarlo implica un helper con `std::variant` o `if constexpr` que cubra ambos contextos — más complejo que duplicar 20 líneas.
3. **Estabilidad**: los call-sites de Settings/Preferences son estables (4 paneles, ~30 sliders) y los del Inspector también (6 paneles, ~24 fields). No hay riesgo de "ay, cambiar la API del helper compartido rompe los dos lados".

**Alternativas descartadas:**
- Helper genérico en `editor/ui/`: agregaría dependencias cruzadas (Inspector incluye un helper de UI que sabe de `EditorUI`/`HistoryStack`; Settings que NO los necesita los heredaría inadvertidamente).
- Heredar uno de otro: vínculo conceptual débil — son convenciones UI parecidas, no la misma operación.

**Revisión:** si emergiera un tercer contexto (ej. Asset Browser con reset-to-default de import settings) y los 3 compartieran más del 80% de la firma, refactorear a un helper base. Mientras tanto, duplicar 20 LOC es más legible.

### Decisión 2 — Reuso de `pushAtomicEdit<T>` (de F3H12) vs helper nuevo

**Contexto:** El reset hace `current → defaultValue` y eso es un cambio atómico (1 click, sin frames intermedios). F3H12 introdujo `pushAtomicEdit<T>` exactamente para eso (combos, checkboxes, file pickers).

**Decisión:** Reusar `pushAtomicEdit<T>` directamente — no crear un wrapper específico de "reset".

**Razones:**
1. **Semántica idéntica**: el reset es indistinguible de un checkbox toggle o un combo change desde la perspectiva del HistoryStack — captura `before`, aplica `after`, push como `EditPropertyCommand<T>`. Inventar otro helper es ceremonial.
2. **Mensaje del command**: el caller pasa el `cmdLabel` ("Reset Light enabled", "Reset RigidBody mass", etc) — eso ya identifica la operación si el dev mira el HistoryStack.

**Alternativas descartadas:**
- `pushResetEdit<T>` específico: cero valor agregado, mismo cuerpo, mismo signature menos legibilidad por separación artificial.

**Revisión:** si el reset eventualmente necesitara batch (un solo Ctrl+Z deshace 7 fields a la vez como hizo F3H12 con `EditEnvironmentSubsetCommand`), revisar el diseño. Por ahora cada reset es per-field — un Ctrl+Z deshace UN reset, alineado con la convención Unity (no Unreal — Unreal a veces agrupa).

### Decisión 3 — No-render del botón cuando `current == defaultValue`

**Contexto:** En cada `renderXxxSection` el botón ↺ se evalúa para todos los fields que tienen reset. La opción alternativa: renderear siempre el botón, pero gris/disabled cuando ya está en default.

**Decisión:** **No renderear nada** cuando `current == default`. El botón solo aparece para campos con override.

**Razones:**
1. **Convención Unity/Unreal**: Unity (>2020) muestra el ↺ solo si hay override; Unreal lo muestra cuando el field difiere del template/parent. Es la UX esperada.
2. **Surface de overrides**: el dev escanea el Inspector y ve **dónde tocó** — no se pierde entre 24 botones grises que dicen "no hay nada que resetear".
3. **Visual noise zero**: en un componente recién creado con todos los defaults, no hay un solo ↺ — el panel se ve limpio.

**Alternativas descartadas:**
- Botón siempre visible (gris/enabled según override): aumenta ruido visual sin ganancia (la decisión "puedo resetear esto" es informacionalmente vacía si ya está en default).
- Botón visible solo en hover de la fila: cambia el patrón de descubribilidad — el dev no sabría a priori qué campos son reseteables. Worse than current.

**Revisión:** si emergiera un caso donde el dev quiere "indicador visual de que un field tiene default conocido" (ej. para distinguir entre "campo con default 0" y "campo sin default"), reabrir. Por ahora todos los fields del Inspector tienen default conocido (la construcción `{}` del componente).

### Decisión 4 — Scope acotado a 6 paneles (Light/Trigger/ForceField/Particle/Audio/RigidBody)

**Contexto:** El plan original mencionó cobertura amplia ("cada Inspector field"). Cubrir TODOS los paneles incluye Cloth/Joint/Ragdoll/MeshRenderer/Brush/Vehicle/Script/Animator/Inventory — cada uno con sus propios fields.

**Decisión:** F3H13 cubre los 6 paneles más usados (Light/Trigger/ForceField/ParticleEmitter/AudioSource/RigidBody). Cloth/Joint/Ragdoll/MeshRenderer/Brush quedan diferidos con backlog explícito en memoria `project_reset_button_coverage`. Script/Animator/Inventory/Vehicle/Camera no se tocaron — sus fields son configuración compleja, no edits frecuentes per-field.

**Razones:**
1. **Plan discipline**: el hito cierra Sub-fase 3.2; el alcance es "introducir el patrón + cubrir el grueso del Inspector", NO "cobertura 100%". Inflar a 11 paneles añade horas sin valor proporcional (los paneles diferidos se editan raramente; cuando se necesite, el helper ya está listo).
2. **Defaults no triviales para los diferidos**:
   - **Cloth**: defaults de mass/damping/iterations dependen del mesh (un cloth grande necesita más iterations); reset a "1.0/0.5/10" puede romper la simulación visualmente.
   - **Joint**: anchor/axis/limits son función del tipo de joint (hinge vs ball vs slider) — defaults universales no existen.
   - **Ragdoll**: replicable trivialmente con el helper, pero uso bajo justifica diferir.
   - **MeshRenderer**: defaults de material son per-slot (textura/color por slot), no un "default canónico genérico".
   - **Brush**: vertices/faces no son property-drawer — el "reset" semántico es vaciar el brush, ya cubierto por otro flow.
3. **Plan discipline + memoria explícita**: cumple la regla "pendings futuros esperan su hito" — el backlog vive en `project_reset_button_coverage.md` con replicación del patrón ya documentada.

**Alternativas descartadas:**
- Cobertura total: 4-6 horas adicionales con valor marginal; arriesga romper Cloth/Joint con defaults incorrectos.
- Solo helper + 1 panel (Light) como demo: subutiliza el helper, deja pelado al resto del Inspector.

**Revisión:** si el dev al usar el editor pide reset en Cloth/Joint/Ragdoll/MeshRenderer/Brush, extender en un hito propio (estimado: 1-2h con el helper ya en su lugar).

---

## 2026-05-26: F3H12 cierre — Undo coverage audit del Inspector + fixes en 8 paneles

### Decisión 1 — `pushAtomicEdit<T>` helper separado de `pushEditIfDone<T>`

**Contexto:** El Inspector tiene dos clases de widgets:
- **Drag**: DragFloat/SliderFloat/ColorEdit3 — el dev arrastra durante N frames, suelta al final. El undo necesita capturar el `before` al click inicial y el `after` al `IsItemDeactivatedAfterEdit` → 1 sola entrada del HistoryStack por gesto. `pushEditIfDone<T>` con tracker hace esto.
- **Atómico**: Checkbox, Combo, Selectable, file picker — 1 click = 1 cambio. No hay drag, no hay frames intermedios.

Pregunta: ¿extender `pushEditIfDone` para soportar también el caso atómico (detectando que el widget no usa drag), o crear un helper nuevo `pushAtomicEdit<T>`?

**Decisión:** **helper nuevo separado**. `pushAtomicEdit<T>(ui, e, before, after, setter, label)` recibe ambos valores explícitamente (sin tracker), chequea `before != after`, crea + pushea el `EditPropertyCommand<T>`.

**Razones:**
- **Semánticas distintas que confunden si se mezclan**: el tracker drag tiene state (activeId, before en variant); el atómico no necesita state porque el cambio se detecta en el frame del click.
- **Call-site más legible**: el caller del helper atómico pasa explícitamente before+after, evita tener que saber que el tracker capturó algo "antes".
- **Independiente del tracker**: el atómico funciona con o sin tracker — no requiere que el panel tenga `m_editTracker`.

**Alternativas descartadas:**
- Unificar en `pushEditIfDone`: detectar si `IsItemActivated`/`IsItemDeactivatedAfterEdit` disparan en el mismo frame para inferir atómico vs drag. Frágil — depende de comportamiento interno de ImGui que puede cambiar entre widgets.
- Helper `pushComboEdit` y `pushCheckboxEdit` específicos: granularidad excesiva — el patrón `before/after/setter` es el mismo, no justifica un helper por widget.

### Decisión 2 — Commands custom file-local (`EditEnvironmentSubsetCommand` + `EditVehicleConfigCommand`)

**Contexto:** Environment reset buttons reasignan 3-7 fields a defaults; el preset combo del Vehicle reasigna ~10 fields del `VehicleConfig` (asset compartido). Cada operación es lógicamente 1 acción del dev pero N escrituras de campo. Pregunta: ¿pushear N commands de `EditPropertyCommand<T>` (uno por field — N Ctrl+Z para revertir un solo "reset"), agregar un command genérico al namespace `commands/`, o hacer commands file-local específicos por panel?

**Decisión:** **commands custom file-local** dentro del `.cpp` del panel. `EditEnvironmentSubsetCommand` vive en `InspectorPanel_Environment.cpp`; `EditVehicleConfigCommand` en `InspectorPanel_Vehicle.cpp`. Captura `std::function<void(Component&)>` para `applyBefore` + `applyAfter`.

**Razones:**
- **1 click = 1 entrada del HistoryStack**: el dev clicka "Restablecer fog" y Ctrl+Z revierte los 5 fields en una sola acción. UX consistente con Unity Reset y Unreal Reset to Default.
- **Uso file-local exclusivo**: ningún otro panel necesita un command que reescribe N fields de `EnvironmentComponent` o `VehicleConfig`. Promover a `commands/` ensucia el namespace global sin justificación.
- **Captura por lambda**: las lambdas `applyBefore`/`applyAfter` permiten que el call-site exprese exactamente qué fields toca, sin tener que parametrizar un command genérico con un map<string, value> o similar.

**Alternativas descartadas:**
- N commands separados: 1 reset del fog = 5 commands = 5 Ctrl+Z. UX horrible.
- Command genérico `EditComponentSnapshotCommand<T>` con `T` por valor: requiere `T` copyable + operator== para isNoOp. `EnvironmentComponent` no tiene operator==, agregarlo sería trabajo extra que no se reutiliza.
- Public `EditEnvironmentResetCommand` en `commands/`: 0 callers fuera del panel. Solo agregaría header churn.

### Decisión 3 — Scope acotado en Inventory (operaciones estructurales sin undo)

**Contexto:** `InventoryComponent` tiene tanto edits "tipados" (mode combo, max_items/grid_w/grid_h InputInt, slot name/tag InputText, entry qty/slot_index InputInt) como operaciones "estructurales" (add slot, remove slot, add entry, remove entry, drop ITEM, clear). Las tipadas encajan con `EditPropertyCommand<T>`/`pushEditIfDone`/`pushAtomicEdit`. Las estructurales requerirían snapshot-based command (`EditInventoryStateCommand` que captura `Inventory::State` entero antes/después).

**Decisión:** **undo solo para edits tipados**. Operaciones estructurales sin undo + comentario explícito en el código documentando el follow-up.

**Razones:**
- **Scope F3H12 = "undo coverage audit"**: el patrón usado por el resto del Inspector aplica a edits tipados. Diseñar un command snapshot-based para Inventory expande scope a "diseño de un command nuevo" que aplica a 1 sólo componente.
- **Frecuencia de uso**: el dev típicamente arma el inventory una vez al setup del proyecto. Add/remove slot/entry son ops poco frecuentes. La friction de "no puedo Ctrl+Z después de borrar un slot" es real pero baja.
- **Re-aplicar es trivial**: si el dev borra accidentalmente un slot, agregarlo de nuevo es 1 click + retipar el nombre. No es como perder 30 min de tuning fino.

**Cuándo revisar:** si el dev pide el undo después de un accidente real, o si Inventory crece a tener edits más caros que justifiquen el `EditInventoryStateCommand`.

### Decisión 4 — Multi-edit innecesario en Environment

**Contexto:** Los nuevos helpers `multiEditCheckbox/multiEditCombo` soportan multi-edit (N entidades). Environment no se usa multi-entity típicamente — 1 EnvironmentComponent por escena (típico de Unity/Unreal Post Process Volume).

**Decisión:** **single-entity helpers** (`pushAtomicEdit<T>`) para Environment en lugar de `multiEditCheckbox/multiEditCombo`.

**Razones:**
- **Caso de uso real**: el dev solo tiene 1 entity con `EnvironmentComponent` por mapa. Multi-edit es feature muerta para este panel.
- **Menos código**: helpers single-entity no requieren getter callback, son más cortos en el call-site.
- **Helpers multi-edit siguen disponibles** para los otros paneles donde sí aplican (Light, Trigger, ForceField, Cloth, ParticleEmitter).

**Cuándo revisar:** si en el futuro el engine permite múltiples Environment con scoping (zonas con distinto fog), promover los call-sites a `multiEditCheckbox/multiEditCombo`. La signatura es compatible.

---

## 2026-05-25: F3H11 cierre — Persistencia Audio/Camera + refactor Brush + clipboard Tier 3

### Decisión 1 — `AudioAssetId` runtime vs `clipPath` string en `SavedAudio`

**Contexto:** F3H11 cierra el gap F2 de no-persistencia de `AudioSourceComponent`. El componente runtime usa `AudioAssetId` (u32 inestable entre sesiones porque depende del orden de load del `AssetManager`). Pregunta: persistir el `AudioAssetId` directo (mismo valor numérico) o el `clipPath` lógico (string)?

**Decisión:** **`clipPath` (string)** en `SavedAudio`. Al cargar, `SceneLoader::applyOneEntity` lo re-resuelve a `AudioAssetId` via `AssetManager::loadAudio(clipPath)`. Fallback a `missingAudioId()` si el string está vacío.

**Razones:**
- **Consistencia con F2**: todos los componentes con asset refs ya usan path puro (`DialogComponent.dialogPath`, `ItemPickupComponent.itemPath`, `VehicleComponent.configPath`). Audio sigue el patrón.
- **Estabilidad cross-session**: paths son estables (el archivo `.wav`/`.ogg` no cambia de nombre); IDs no (el cache del AssetManager se reordena al cargar otros assets antes).
- **Cross-project resilience**: si el clipboard cross-project se permite en el futuro, el path tiene chances de seguir siendo válido (si el otro proyecto tiene un asset con el mismo logical path); el ID sería garbage.

**Cómo aplica:** mismo patrón sirve para futuras extensions del clipboard a componentes con asset refs no triviales.

### Decisión 2 — Persistir `CameraComponent` aunque sea stub en el editor

**Contexto:** El `CameraComponent` es stub desde el audit F2H85 — el editor usa su propia cámara, no la del componente. Persistir es trabajo "por completeness" sin payoff inmediato en el flujo del editor.

**Decisión:** **persistir igual** (con `SavedCamera` minimal: fovDeg + nearPlane + farPlane). Schema sin bump.

**Razones:**
- **Habilita objetivo del hito**: sin persistencia, el clipboard de Camera tampoco funcionaría (el serializer reusa el path del .moodmap schema). F3H11 quiere cerrar los 3 types pendientes del Hierarchy paste — Camera es uno de ellos.
- **MoodPlayer futuro**: si en el futuro las cámaras del componente se usan para cinemáticas (no hay nada que lo impida en el engine), los valores ya estarán persistidos.
- **Costo bajo**: 3 fields scalar, 4 funciones triviales (write/read/apply + branch en el clipboard).

**Alternativa descartada:**
- **Diferir Camera al hito que active el componente en MoodPlayer**: rompe la coherencia del bundle Tier 3 (Audio/Camera/Brush). Mejor cerrar los 3 juntos.

### Decisión 3 — Brush dispatch especial en el ComponentClipboard (no SavedEntity wrapper)

**Contexto:** Los demás types del clipboard usan el wrapper SavedEntity — `serializeEntityToJson` produce el sub-object del componente, `parsePayloadAsSavedEntity` envuelve el payload en un fake-entity y delega a `parseEntityFromJson`. Para Brush, el schema persistido es **distinto**: `SavedBrush` vive en `SavedMap.brushes`, no en `SavedEntity` — tiene su propio top-level tag/position/transform/materialPaths/faces (no anidado bajo `brush`). Pregunta: forzar Brush al schema `SavedEntity` (refactor del `.moodmap` para inflar `SavedEntity::brush`) o dispatch especial en el clipboard?

**Decisión:** **dispatch especial** en `ComponentClipboard::serializeComponent` y `applyPayload`. Si `componentKey == kKeyBrush`, llama directamente a `serializeBrush(entity, assets)` (público desde F3H11 — Parte C del hito) y `parseBrush(payload)` + `SceneLoader::applyBrushFromSaved`.

**Razones:**
- **Zero churn al schema `.moodmap`**: los Brushes han vivido en `SavedMap.brushes` (top-level) desde F2H11. Migrarlos a `SavedEntity::brush` requeriría refactor del schema + upgrader + back-compat con todos los `.moodmap` existentes — scope grande para un payoff cosmético (consistencia del dispatch interno).
- **Helpers existen**: `serializeBrush` y `parseBrush` ya tienen toda la lógica robusta (UV defaults canónicos, material indices, visgroup membership). Exponerlas al público (Parte C) + dispatch especial en clipboard es mínimo trabajo.
- **Conceptualmente honesto**: Brush ES un caso especial del modelo `.moodmap` (vive aparte por razones históricas de F2H11). Reflejar eso en el clipboard es más claro que disimularlo.

**Cómo aplica:** si futuras extensions del clipboard necesitan tipos con schema separado (ej. compiled mesh data), el patrón "branch antes del SavedEntity dispatch" es reusable.

---

## 2026-05-25: F3H10 cierre — ComponentClipboard Tier 2 + 9 kits nuevos en convert_entity_modal

### Decisión 1 — Scope acotado a "fruta accesible" (5 types con SavedX) vs forzar refactor del serializer

**Contexto:** F3H10 originalmente planteaba extender el clipboard a 8 types nuevos (`mesh_renderer/audio_source/dialog/item_pickup/brush/vehicle/camera/environment`). Al revisar el código, 3 quedaron con dependencias técnicas no triviales: Audio + Camera **no están serializados al `.moodmap`** (gap F2 — necesitan `SavedAudio`/`SavedCamera` structs + writes/reads en `EntitySerializer`); Brush tiene `serializeBrush`/`parseBrush` en **namespace anónimo** de `SceneSerializer.cpp` y el applier está inline en el loop de `SceneLoader::applyMap`.

**Decisión:** **diferir los 3 a F3H11** y cerrar F3H10 con los 5 types restantes (`mesh_renderer/dialog/item_pickup/vehicle/environment` — todos con `SavedX` ya existente en `SavedEntity`).

**Razones:**
- **Cluster técnico común**: los 3 diferidos comparten "modificación al `EntitySerializer`/`SceneLoader`" — sumarlos juntos en un hito dedicado es más coherente que mezclar en F3H10.
- **Entrega de valor consistente**: F3H10 cierra con 5 types que cubren los EntityType más comunes (Mesh/NPC/Pickable/Vehicle/Environment). El dev ve impacto inmediato sin que F3H10 se infle.
- **Riesgo de scope creep**: forzar Brush en F3H10 era un refactor del namespace anónimo del serializer + extraer ~75 LOC del SceneLoader. Mezclar con la mecánica del clipboard rompe el principio "un hito = un cambio coherente".

**Alternativas descartadas:**
- **Hacer los 8 en F3H10**: sumaba 3-5h de refactor del serializer encima del scope ya validado por el dev.
- **Dropear F3H10 y arrancar F3H11 directamente**: pierde el valor de tener 5 types operativos antes de tocar el serializer.

**Cómo aplica:** memoria `clipboard-brush-audio-camera` documenta el plan técnico detallado para F3H11+ (no se pierde contexto entre hitos).

### Decisión 2 — Mantener convert_modal aditivo (confirmar la D1 de F3H9)

**Contexto:** F3H9 D1 propuso "ampliar kits, no rework destructivo" como path para F3H10. Al implementar los 9 kits nuevos, se reconfirmó al toque la decisión.

**Decisión:** **mantener aditivo**. Cada kit AGREGA su componente base + setea el `entityType` sin tocar el resto de componentes existentes.

**Razones:**
- **Caso de uso real**: el dev a veces necesita acumular kits (ej. NPC con AudioSource — el modelo "NPC" no necesariamente excluye tener un audio loop ambiental). Forzar borrado destructivo rompe ese flow.
- **Undo destructivo es riesgoso**: borrar un componente con todos sus valores tuneados sin que el dev pueda undo trivial es mala UX. El dev limpia manualmente lo que no le sirve con click-derecho + Remove (que ya es undoable via `RemoveComponentCommand`).
- El popup "Add Component" filtrado por type (F3H9 Stage 6) impide combinaciones absurdas en entidades NUEVAS. Las viejas mezcladas son responsabilidad del dev.

**Revisar si:** emerge demanda real del dev por behavior destructivo. Probable F3H11+ o futuro.

### Decisión 3 — El patrón "placeholder UX honesto" paga al extender el set

**Contexto:** F3H9 D5 decidió que el Hierarchy "Copiar valores" muestra el menú item **grisado con tooltip honesto** ("pendiente F3H10+") para types fuera de Tier 1, en lugar de ocultarlos. La pregunta retrospectiva al implementar F3H10: ¿paga este patrón al extender?

**Decisión (retrospectiva):** **sí, pago directo**. Al sumar los 5 types a `supportedKeys()`, el Hierarchy automáticamente dejó de grisar para esos types — cero líneas de código tocadas en `HierarchyPanel.cpp`. La UI ya consulta `ComponentClipboard::isSupported(baseKey)` como condición del grisado.

**Razones:**
- **Single source of truth**: el `supportedKeys()` set es la verdad. UI, applier y tests todos lo consultan. Extender = 1 línea en el set + branches en 4 dispatchers (componentNameKey/entityHasComponent/applyPayload/removeComponent).
- **Cero churn en presentación**: tooltips, grisado, separadores — todo sigue consistente sin tocar UI.
- **Plan futuro automático**: cuando F3H11 sume Brush/Audio/Camera, el Hierarchy también se actualiza sin diff de UI.

**Lección:** placeholders UX honestos con check al backend pagan exponencialmente al cerrar el backlog. Vale más que ocultar items por completo (que sugiere "no existe").

---

## 2026-05-24: F3H9 cierre — EntityType model + popup remake + Material Inspector Blender-style

### Decisión 1 — `EntityType` como campo de `TagComponent` (no componente separado)

**Contexto:** F3H9 introduce el modelo "tipo de entidad" (Blender Object Type / Hammer entity class / Unreal Actor class) que define qué componente base no se puede quitar + qué extensions se pueden agregar. Pregunta: storage como (a) campo en `TagComponent`, (b) componente nuevo `EntityTypeComponent`, (c) registry global externo al ECS.

**Decisión:** **Campo `entityType` en `TagComponent`** (junto al `name`).

**Razones:**
- **Co-localización semántica**: name + type son ambos metadata de identidad de la entidad — el dev piensa "esta luz se llama PointLight_A" como una cosa, no dos. Co-localizar matchea cómo se piensa.
- **Cero proliferación de componentes** en el ECS — `TagComponent` ya existe en TODAS las entidades del proyecto, agregar un campo es trivial y no cambia la arquitectura.
- **Serialización trivial**: `TagComponent` ya es persistido por `EntitySerializer`, agregar `"entity_type"` al sub-object es one-liner sin nuevos handlers.
- Schema sin bump (mismo patrón que F3H4/F3H5/F3H6): el campo solo se escribe si != Generic (default), `.moodmap` pre-F3H9 cargan limpios.

**Alternativas descartadas:**
- **`EntityTypeComponent` separado**: agregar un componente que va en todas las entidades duplica el footprint del `TagComponent` (también en todas). El ECS premia escasez de componentes.
- **Registry global `unordered_map<EntityHandle, EntityType>`**: rompe el modelo "todo en el ECS" del resto del editor. Lookups extra y sincronización manual al destruir entities.

### Decisión 2 — Inferencia para back-compat pre-F3H9 (no migración forzada)

**Contexto:** los `.moodmap` guardados pre-F3H9 no tienen la key `entity_type`. ¿Migrar al cargar (escribir el campo derivado de los componentes presentes y forzar el save al siguiente cierre) o inferir on-the-fly cada vez sin tocar el JSON?

**Decisión:** **inferir on-the-fly al `SceneLoader::applyOneEntity`** vía `EntityTypeTable::inferFromEntityWithTag`. No se fuerza save. Al próximo Save manual del dev, el campo se persiste con su valor inferido.

**Razones:**
- **Cero cambio destructivo**: el dev abre un proyecto viejo, NO se le modifica el `.moodmap` sin haber decidido guardar. La modificación llega cuando el dev guarda activamente.
- **Recuperable**: si la inferencia es errónea (ej. una entity "Foo" con MeshRenderer+VehicleComponent que el dev pensó como Mesh pero quedó como Vehicle por orden de chequeo), el dev cambia el type via convert_modal y guarda — la inferencia se desactiva en la siguiente carga.
- **Mismo patrón que F3H4** (gameplay tier 1): default values en el struct, JSON solo escribe lo no-default, back-compat trivial.

**Cómo aplica:** orden de inferencia (de más específico a más genérico) en `inferFromEntity`: Brush > NPC (Trigger+Dialog) > Pickable (Trigger+ItemPickup) > Environment > Light > Camera > ParticleEmitter > ForceField > Audio > Trigger solo > Vehicle > Mesh > Generic. Tile se detecta por tag (Floor / Tile_X_Y).

### Decisión 3 — Vehicle ANTES de Mesh en orden de inferencia

**Contexto:** un vehicle típicamente tiene `MeshRendererComponent` (visual del chasis) además del `VehicleComponent` (mecánica). Si chequeamos Mesh antes que Vehicle, todos los vehicles quedan inferidos como Mesh — incorrecto.

**Decisión:** chequear `VehicleComponent` ANTES de `MeshRendererComponent` en `inferFromEntity`.

**Razones:**
- VehicleComponent es la mecánica **definitoria** — sin él, un vehicle no es vehicle. Mesh es secundario (visual).
- Mismo principio que NPC (Trigger+Dialog) y Pickable (Trigger+ItemPickup) chequeados ANTES que Trigger solo — el "definitorio" gana.

**Cómo aplica:** se mantiene el principio "specific-first" para tipos compuestos: si emergen futuros types que combinen componentes existentes (ej. AnimatedNPC = NPC + Animator), agregar también primero del orden.

### Decisión 4 — Material Inspector Blender-style: sticky-slot con clamp anti-overflow

**Contexto:** Stage 9 (bundle agregado tarde al hito por pedido del dev al ver la "lista infinita" de slots en un mesh complejo). Reemplaza el loop vertical de N paneles por una lista compacta arriba + panel del slot seleccionado debajo. ¿Mantener la selección entre frames (sticky) o resetear a 0 cada vez?

**Decisión:** **sticky** entre frames vía `int m_selectedMaterialSlot` en `InspectorPanel`, **clampeado** contra `mr.materials.size()` cada frame para sobrevivir cambios de entity o reducción de slots.

**Razones:**
- **UX matching Blender**: al cambiar de slot el panel cambia pero el dev espera que al volver a la entity siga en el slot que estaba editando. Reset a 0 es frustrante en un workflow de tunear varios materiales.
- Cambiar de entity con menos slots se manejaría con un crash o panel vacío sin el clamp.
- Trade-off aceptable: el dev cambia entity a entity y "pierde" el slot — pero **no es realmente perdida**: si la entity nueva tiene 5 slots y estabas en 3, sigues en 3 (overlap). Solo se resetea si la nueva tiene < 4 slots y el clamp lo arrastra a 0.

**Alternativas descartadas:**
- **Per-entity state** (map<Entity, int>): overhead innecesario, el dev rara vez vuelve exactamente al mismo slot+entity en el mismo session.
- **Reset a 0 siempre**: rompe el sticky workflow.

### Decisión 5 — Hierarchy "Copiar valores" grisado vs ocultar para types no soportados

**Contexto:** Stage 8 agrega "Copiar valores de <Tipo>" en el click derecho del Hierarchy. `ComponentClipboard` soporta Tier 1 (Light/Trigger/ForceField/ParticleEmitter); el resto (Mesh/Vehicle/Brush/Audio/Camera/Environment/NPC=Dialog/Pickable=ItemPickup) está pendiente F3H10+. ¿El menu item para esos types (a) se oculta (no aparece), (b) se muestra grisado con tooltip explicando, (c) hace no-op silencioso?

**Decisión:** **(b) grisado con tooltip honesto** — "Copiar/pegar de {Tipo} aun no implementado (pendiente F3H10+)".

**Razones:**
- **Transparencia de capacidades**: el dev sabe qué está implementado vs lo que viene. Esconder el menu sugiere "no existe esta operación", grisar dice "existe pero todavía no".
- **Trail visible para el roadmap**: el dev ve al pasar el mouse que ese type está en lista. Genera presión productiva para cerrar el backlog.
- No-op silencioso es peor: el dev clickea, nada pasa, no entiende si funcionó o no.

**Cómo aplica:** cuando `ComponentClipboard::isSupported(baseKey)` empiece a devolver true para más types (F3H10+ trabaja en [[component-clipboard-expand]]), la UI se actualiza automáticamente sin tocar `HierarchyPanel.cpp` — la condición que grisa consulta el clipboard.

---

## 2026-05-24: F3H8 cierre — Multi-edit del Inspector (Light) + arranque Sub-fase 3.2

### Decisión 1 — Snapshot semantics vs delta semantics para multi-edit

**Contexto:** F2H23 iter 5 introdujo multi-edit del Transform via gizmo usando **delta semantics**: cada entidad se mueve por `delta = active.after - active.before` (mantiene offsets relativos entre entidades del set). F3H8 amplía multi-edit al Inspector para Light (color + intensity + radius). Pregunta: ¿usar delta (cada luz se desplaza su propio offset desde sus betters) o snapshot (todas las luces se homogenizan al valor del active)?

**Decisión:** **Snapshot semantics** para F3H8 — todas las entidades del set reciben el mismo `after` value al commit. Undo restaura cada una a su `before` individual.

**Razones:**
- Para propiedades **semánticas** (color, intensity, range) la operación natural es "homogenizar al active" — el dev tiene 3 luces de colores distintos, mueve el slider a rojo, las 3 quedan rojas. "Mantener offsets" no tiene sentido (¿el offset de qué? los colores no son aditivos).
- Para Transform (vec3 position/rotation/scale) **sí** tiene sentido delta — el dev tiene 3 cajas en grid 1m, las mueve 2m a la derecha, todas mantienen el grid (no se colapsan a una sola posición). F2H23 iter 5 ya lo resolvió bien.
- Implementación más simple: `MultiEditPropertyCommand<T>` guarda 1 solo `after` compartido + N `before` individuales. Delta requeriría guardar el delta y aplicarlo per-entity con su before — más memoria + más cómputo per execute.

**Alternativas descartadas:**
- **Solo delta**: rompe la semántica de "homogenizar" para casos donde es lo correcto (color, etc).
- **Switch user-elegible**: complejidad extra sin valor — los call-sites saben qué semántica aplica al field.

**Cómo aplica:** Transform en Inspector futuro (si emerge el caso de editar DragFloat3 multi-entity con offsets) sigue usando delta vía `MultiEditTransformCommand` de F2H23. Los demás campos (color/intensity/audio volume/particle rate/etc) usan snapshot via `MultiEditPropertyCommand<T>` de F3H8.

### Decisión 2 — Fallback automático a single-entity cuando `selectionSet.size() <= 1`

**Contexto:** los helpers existentes en `InspectorPanel_Internal.h` (`fieldColorEdit3`, `fieldDragFloat` de F2H74) están en uso en ~30 call-sites a lo largo de los 10 partials del Inspector. Cualquier refactor que rompa su firma afecta a todos. F3H8 introduce versions multi-edit-aware (`multiEditColor3`, `multiEditDragFloat`). Pregunta: ¿reemplazar los single-entity (migrar TODOS los call-sites) o agregar las multi-versions y mantener los single-entity intactos?

**Decisión:** **Agregar las multi-edit-aware como nuevas funciones** que delegan al single-entity cuando `selectionSet.size() <= 1`. Los call-sites single-entity siguen funcionando sin cambios. Sólo migrar a multiEdit los call-sites que el dev decida ampliar a multi-edit (F3H8 = Light; futuros hitos = Audio/Particles/Trigger).

**Razones:**
- **Zero-cost back-compat**: los ~30 call-sites existentes no se tocan. Tests existentes siguen pasando sin actualización.
- **Migración granular per-componente**: F3H8 ataca Light. Tier 2 (Audio/Particles/Trigger) se migra cuando emerja necesidad — no hay big-bang refactor.
- **Comportamiento idéntico para single-select**: el dev no nota diferencia. UX preservado.

**Alternativas descartadas:**
- **Reemplazar in-place**: forzar refactor cascada de 30 call-sites. Innecesario por la mayoría que se quedará single-entity (algunos componentes nunca tendrán multi-edit semántico — ej. ScriptComponent con path único).
- **Templatizar más fuerte**: complicaría las firmas. La duplicación de la lógica de detect/track/push es ~80 LOC, aceptable.

**Cómo aplica:** los nuevos call-sites multi-edit usan `multiEdit*`; los antiguos siguen con `field*`. Si en el futuro emerge un patrón "multi-edit por default" se puede consolidar; por ahora la separación es clara.

### Decisión 3 — Lambdas con `hasComponent<T>` guard per call-site (vs predicate parameter en el helper)

**Contexto:** los helpers multi-edit iteran sobre `selectionSet.selected` para detect "valor común" + propagar live preview + snapshot before. Si la selección mezcla tipos (ej. light + box), las entidades sin el componente harían crash en `getComponent<LightComponent>()`. Hay 2 formas de prevenirlo: (a) lambdas en el call-site guardan con `hasComponent<T>()` inline; (b) helper acepta predicate `hasComponent` como parámetro adicional.

**Decisión:** **opción (a)** — lambdas del call-site guardan. Más verbose por call-site pero localiza el conocimiento del tipo en un solo lugar.

**Razones:**
- **Localización**: el call-site ya sabe qué componente edita (es Inspector_Light, Inspector_Audio, etc). Agregar el guard inline es trivial (1 línea por lambda).
- **Sin templates extra**: el helper queda no-templatizado sobre `ComponentType` — más simple de mantener, menor cost de compilación.
- **No cambia la firma del helper**: si futuros componentes requieren guards distintos (ej. tener Light + check específico), no hay que tocar el helper.
- **Getter con fallback al active value** mantiene `allMatch` consistente — un peer sin el componente no rompe la detección de "valor común" (se "ve" idéntico al active).

**Alternativas descartadas:**
- **Predicate parameter**: 3 lambdas por callsite ya es mucho. Una 4ta empeora la legibilidad sin beneficio claro. Cuando se generalice a 5+ tipos vale la pena reconsiderar; hoy son 3 (color, intensity, radius).
- **Templatizar sobre ComponentType**: forzaría tener una version por componente, multiplicaría headers. F3H8 está en Internal.h ya cerca del cap.

**Cómo aplica:** futuros call-sites multi-edit (Audio volume, Particles rate, Trigger enabled) copian el patrón de Light — lambdas guardan con `hasComponent<AudioSourceComponent/ParticleEmitterComponent/TriggerComponent>()`. Si emerge un caso donde el guard es complejo (ej. requiere Light + ciertos flags), el call-site lo expresa inline.

---

## 2026-05-24: F3H7 cierre — UserSettings > Editor (zoom orto + gizmo + click/drag), cierre de Sub-fase 3.1

### Decisión 1 — Tests aislando `editorSettingsToJson/fromJson` del filesystem (opción C del plan)

**Contexto:** F3H2 explícitamente NO agregó tests porque `UserSettings::init/save` escriben a `%APPDATA%\MoodEngine\settings.json` real y contaminarían el state del dev. F3H7 introduce `EditorSettings` con sanitize logic (clamp zoom factor, threshold, etc) que merece tests. La decisión registrada en F3H2: *"si el módulo crece (F3H6 shortcuts, F3H7 autosave/font/density), refactorear ahí con tests aislados"*.

**Opciones:** (a) sin tests, validación visual; (b) refactor `UserSettings` para aceptar path inyectable (default APPDATA, override en tests); (c) split de responsabilidad — `init/save` siguen tocando APPDATA, pero `editorSettingsToJson/fromJson` son funciones libres que operan sobre `nlohmann::json` puro y se testean sin filesystem.

**Decisión:** opción **(c)**. Las funciones libres viven en el namespace `UserSettings` (no en una clase nueva) y se testean en `tests/test_user_settings_editor.cpp` (10 cases: defaults, non-default, roundtrip, empty/non-object, clamps, valores negativos, back-compat partial, forward-compat).

**Razones:**
- Mismo patrón que `ProjectSettings::toJson/projectSettingsFromJson` — funciones libres sobre `nlohmann::json`, testeadas en `test_project_settings.cpp`. F3H4/F3H5/F3H6 lo usan con éxito.
- El sanitize logic (clamp factor `>= 1.05`, sizes `> 0`, threshold `>= 1`) es donde más errores pueden colarse — testearlo aislado da confianza sin tocar disco.
- Refactor de `init/save` con path inyectable requiere cambiar firma + propagar a `MoodEditor` y `MoodPlayer` bootstrap — out of scope F3H7. Si en algún futuro hito (F3H8+) `UserSettings` crece a 4-5 sub-structs, vale la pena.

**Alternativas descartadas:**
- **(a)** sin tests: el sanitize de `orthoZoomFactor <= 1.0` es lo que distingue "settings.json válido" de "settings.json roto que rompe la cámara" — no testearlo es regresión esperando suceder.
- **(b)** path inyectable: invasivo, cambia firma de `init()/save()/settingsPath()` que ya están en producción desde F2H43. Costo-beneficio malo para F3H7.

**Condiciones de revisión:** si `UserSettings` suma 2+ structs (ej. `ShortcutsSettings`, `AutosaveSettings`) en hitos futuros, considerar opción (b) — el `s_editor` global empieza a oler a singleton mutable cross-test.

### Decisión 2 — Reads LIVE en los 5 call-sites (no snapshot al startup)

**Contexto:** `UserSettings` históricamente tiene un patrón de "snapshot al startup": `language()` y `theme()` se leen en `init()` y los listeners notifican via callbacks (F2H43, F2H76). El dev cambia el idioma desde el menu → `setLanguage` + `save()` + `I18n::setLanguage` aplica live. Decisión a tomar para F3H7: ¿seguir snapshot o leer cada frame?

**Decisión:** **LIVE** — los 5 call-sites llaman `UserSettings::editor().*Field*` cada vez que lo necesitan (zoom factor en el wheel handler, gizmo size en el draw del overlay, threshold en el click-vs-drag check).

**Razones:**
- **Feel coherente con F3H4/F3H5/F3H6**: en Project Settings el dev mueve un slider y siente el cambio al próximo tick (capsule capsule cambia, walk speed cambia, snap step cambia). Spec UX: F3H7 hace lo mismo.
- **Costo negligible**: `UserSettings::editor()` retorna `const EditorSettings&` a un global statico — un lookup por field. Inferior a 1 µs por frame total.
- **Sin invalidación de listeners**: el patrón snapshot+callback funciona para idioma/tema porque son cambios discretos. Para sliders continuos requeriría callbacks per-field y la complejidad no compensa.

**Alternativas descartadas:**
- **Snapshot al ctor del editor**: el dev cambia el slider y NO ve el cambio sin reabrir el editor. UX pobre — esperamos que F3H4/F3H5/F3H6 enseñaron al dev a esperar live previews.
- **Cache local en cada call-site**: complejidad extra para ningún beneficio medible.

**Cómo aplica:** futuros campos de `UserSettings::editor()` (shortcuts, autosave interval, font size) siguen el mismo patrón — read live, sin callbacks. Si emerge un caso donde el cambio requiere rebuild de algo costoso (ej. recreate font atlas al cambiar font size), ese campo específico justifica un listener.

### Decisión 3 — TabBar en `UserPreferencesPanel` (no SeparatorText apilado)

**Contexto:** F3H2 dejó el `UserPreferencesPanel` con UNA sola sección "General" (tema + idioma) sin TabBar. F3H7 agrega la segunda sección "Editor" con 5 sliders. Opciones: (a) seguir con `SeparatorText("General")` + `SeparatorText("Editor")` apilados vertical; (b) introducir TabBar con 2 tabs.

**Decisión:** **TabBar** (opción b), consistente con `ProjectSettingsPanel` (Performance + Gameplay + Character).

**Razones:**
- **Convención de engines reales**: Unity Preferences, Unreal Editor Preferences, Godot Editor Settings — todos usan tree/tabs lateral o pestañas superior, NO scroll vertical infinito. La sub-fase 3.1 quiere replicar el UX que el dev espera de un editor "serio".
- **Escala**: si Sub-fase 3.2+ agrega Shortcuts (potencial gran sección con N keybindings), Autosave, Font size — cada uno tiene su tab. Sin TabBar la ventana se vuelve un menú de scroll en pocas adiciones.
- **Simetría con ProjectSettingsPanel**: ambos paneles tienen TabBar = mismo lenguaje visual = menos cognitive load para el dev.

**Alternativas descartadas:**
- **SeparatorText apilado**: 2 secciones se ven bien hoy, 4+ se vuelven scrollables y feas. Decisión proactiva mientras es barata.
- **Tree lateral estilo Unity**: más espacio horizontal requerido (la ventana hoy es 540×360 fija). Para 7 categorías futuras sí compensa, para 2-3 no.

**Cómo aplica:** las próximas secciones (Shortcuts F3H8+, Autosave, Font size) cada una agrega su tab. Si llegamos a 5+ tabs, considerar tree lateral. Por ahora 2-4 tabs en tabbar horizontal es estándar y suficiente.

### Decisión 4 — Unificar `clickDragThresholdPx` para ortho + perspectiva (no 2 settings separados)

**Contexto:** el audit F3H3 bucket 8 reportaba **dos** thresholds distintos: `OrthoViewportPanel.cpp:217 → 16.0f` y `ViewportPanel.h:200 → 4 px`. Al implementar F3H7 descubrí que ambos viewports usan la misma fórmula `dx*dx + dy*dy >= 16.0f` (comparación al cuadrado para evitar sqrt). El "16" del audit ortho era el cuadrado de 4; lineal eran ambos 4 px. **El audit confundió valor comparado con valor lineal**.

**Decisión:** **una sola setting `clickDragThresholdPx = 4`** que ambos viewports leen y comparan como `dx*dx + dy*dy >= threshold*threshold`.

**Razones:**
- En la práctica ambos son 4 px. No hay user need legítimo para que ortho y perspectiva tengan thresholds distintos — son la misma decisión UX ("cuánto puede moverse el mouse antes de que cuente como drag").
- Una setting es menos cognitive load para el dev. "Quiero clicks tolerantes con trackpad" → sube un slider, no dos.
- Si en el futuro emerge una diferencia real (ej. ortho tiene snap-to-vertex visual que justifica un threshold mayor), partir en 2 settings es trivial.

**Alternativas descartadas:**
- **Mantener 2 settings**: respeta el audit literal pero no hay use case real distinto. La decisión "limpiar el audit" gana.
- **Threshold derivado por viewport**: complejidad sin beneficio.

**Cómo aplica:** corrección retroactiva al `HARDCODED_AUDIT.md` (bucket 8): los 2 sites se fusionan en uno. Sirve de aprendizaje para futuros audits: distinguir valores lineales de cuadrados al catalogar.

---

## 2026-05-24: F3H6 cierre — Snap UI movida a popover MapEditorTopBar + atajo Shift+Wheel

### Decisión 1 — Snap UI vive en popover de MapEditorTopBar, no en Project Settings

**Contexto:** F3H6 inicialmente migró Snap a `.moodproj > Snap` y agregó una tab "Snap" al `ProjectSettingsPanel` siguiendo el patrón de F3H4 (Gameplay) y F3H5 (Character). En la validación visual el dev objetó: *"habria que mejorar un poco mas la UI y no se si esto deberia estar como un settings pero para la parte de mapas no en proyecto"*. Pregunta válida: ¿Snap es un setting "del proyecto" como Gameplay/Character, o una herramienta del map editor?

**Decisión:** mover la UI de Snap del Project Settings al **popover de MapEditorTopBar** (botón "Settings" en la sección Snap). Storage sigue en `.moodproj > settings.snap` (per-project es conceptualmente correcto — distintos proyectos pueden tener distintos steps según escala del mundo). Solo cambia la UI.

**Razones:**
- Research de engines reales: Unity tiene Snap Settings como **ventana dedicada** (`Edit > Snap Settings...`, no Project Settings); Unreal pone snap en `Editor Preferences > Viewports`; **Hammer (Source)** lo tiene como dropdown en la **toolbar del editor de mapas**. Ninguno lo trata como setting genérico del proyecto.
- Principio "la herramienta está donde se usa": Gameplay (walk/jump speed) afecta el feel del juego cuando le das Play — pertenece a un panel de "configurá el juego". Snap step afecta lo que estás modelando ahora mismo — pertenece al editor de mapas. Tener que abrir `Edit > Project Settings > Snap` interrumpe el flow de modelado.
- El patrón Hammer/Source es el más cercano a lo que estamos construyendo (CSG-based map editor), entonces es razonable copiarlo.

**Alternativas descartadas:**
- **Mantener tab en Project Settings:** consistencia con F3H4/F3H5 pero contradice la convención de los engines y crea fricción UX.
- **Ventana dedicada estilo Unity:** más espacio pero requiere otro panel registrado, otro item de menú. El popover es más liviano y vive justo arriba del viewport donde el dev ya tiene el cursor.
- **Tab en User Preferences (per-instalación):** Snap es **per-proyecto** (Foo Cyberpunk usa steps grandes, Bar Sokoban usa steps chicos); poner en UserSettings rompería esa granularidad.

**Cómo aplica:** futuras herramientas del map editor (vertex weld threshold, edge bevel default, etc) viven en este mismo popover de MapEditorTopBar, no en Project Settings. Si el popover crece demasiado, partir en sub-popovers o promover a ventana dedicada. Project Settings queda reservado para configs que afectan el **runtime del juego** (Gameplay/Character/Performance/Physics/Quality) o decisiones globales del proyecto.

**Condiciones de revisión:** si emerge un setting que es claramente "del proyecto" pero también del editor (ej. unidades de medida default), reconsiderar. Por ahora la regla "afecta runtime → Project Settings, afecta editor → toolbar/menú dedicado" es clara.

### Decisión 2 — Atajo Ctrl+Wheel → Shift+Wheel para ciclar snap step (compatibilidad con trackpads)

**Contexto:** desde F2H44, el atajo para ciclar snap step en los orto viewports era `Ctrl+Wheel`. F3H6 lo mantenía. Durante validación, el dev (usando un trackpad de notebook) reportó: *"el scroll aumenta el grid size, y si yo alejo para crear un elemento grande, al mismo tiempo muevo el zoom del ortoview y eso me confunde"*. Wheel solo + Ctrl+Wheel ambos cambiaban el grid. Los logs del editor confirmaron que **cada wheel del trackpad llegaba como `Ctrl+wheel`** — el driver estaba inyectando `KMOD_CTRL` para todos los scrolls.

**Decisión:** cambiar el atajo de `Ctrl+Wheel` a **`Shift+Wheel`**. Ctrl+= / Ctrl+- por teclado siguen igual. Zoom del orto gateado con `!io.KeyShift` en `OrthoViewportPanel` para desacoplar (cambiar el grid no zoomea la cámara al mismo tiempo).

**Razones:**
- **Causa raíz (driver, no nuestro código):** los trackpads modernos (Windows Precision Touchpad, Synaptics, Elan) mapean el gesto de **pinch-zoom a `Ctrl+Wheel`** automáticamente. Lo hacen para que navegadores (Chrome/Edge/Firefox) y apps tipo VS Code/Word zoomeen con pinch sin que las apps necesiten soporte explícito de gestos. Es convención del sistema operativo.
- Con esa convención, `Ctrl+Wheel` como atajo de aplicación es **inutilizable en notebook**: cualquier scroll de dos dedos llega como Ctrl+Wheel sin que el usuario presione Ctrl físico.
- `Shift+Wheel` es el siguiente candidato natural: ningún gesto estándar de trackpad lo simula. En algunas apps Shift+Wheel hace scroll horizontal, pero en un orto viewport no hay scroll horizontal nativo, así que no hay conflicto.
- Es preferible cambiar **una sola tecla del atajo** vs eliminar el gesto wheel (que sería pérdida de UX para usuarios de mouse físico) o intentar distinguir Ctrl físico vs Ctrl simulado (no hay API confiable para esto).

**Alternativas descartadas:**
- **Alt+Wheel:** similar a Shift pero Alt suele estar reservado en otros editores para snap-to-vertex live (Maya/Blender). Reservamos Alt para un atajo paralelo si emerge.
- **Solo teclado (sin wheel):** pierde el gesto natural sobre el viewport. Ctrl+= en español requiere Shift+0 que es awkward en teclados 80% sin numérico; ya teníamos workarounds (Ctrl++, Ctrl+KP_PLUS) en F2H33.
- **Detectar trackpad vs mouse:** SDL2 puede distinguir `SDL_TOUCH_MOUSEID` pero la mayoría de los trackpads modernos reportan como ratón normal con KMOD_CTRL inyectado. No es confiable.
- **No hacer nada y documentar:** el dev no puede usar la feature en su flow actual. Inaceptable.

**Cómo aplica:** futuros atajos sobre wheel en el editor deben evitar Ctrl como modifier obligatorio. Si necesitamos un segundo atajo de modifier+wheel (ej. ciclar entre tools), usar Shift, Alt o tap-toggle de tecla. La convención queda: **Ctrl+Wheel está reservado al pinch-zoom del SO; no es usable como atajo de aplicación en este editor**.

**Condiciones de revisión:** si SDL2/Windows agrega una API confiable para distinguir Ctrl físico vs Ctrl simulado por gesto, reconsiderar. Mientras tanto, esta es la convención.

---

## 2026-05-24: F3H5 cierre — Character (capsule + eye + headbob) + bug latente cerrado

### Decisión 1 — Defaults del struct = valores Editor F2H41 (no los del Player)

**Contexto:** el plan F3H5 sugería defaults `headbobFrequency=5.0` y `headbobAmplitude=0.04`, basado en el sweep A del audit F3H3 que reportó esos valores. PERO durante la implementación, la lectura del Editor (`EditorScene.cpp:379-381`) reveló que el Editor usaba **3.5 Hz / 0.05 m** desde F2H41, con un comentario explícito: *"a walkSpeed 5.5 m/s, 3.5 Hz da ~1.6 m por paso — stride humana realista. Amplitud subida a 5 cm para compensar la menor frecuencia y mantener visibilidad"*. El Player quedó en los valores legacy 5.0/0.04 (igual que el walk speed que cerró F3H3 — mismo bug latente).

**Decisión:** defaults del struct = **3.5 / 0.05** (valores Editor F2H41), NO 5.0 / 0.04 (valores Player legacy).

**Razones:**
- F2H41 documentó explícitamente el tuning como intencional, con razón mecánica (stride humana realista). Los valores Player son "lo que quedó", no una decisión.
- Mismo patrón que F3H3 fix de walk speed: cuando hay 2 sources of truth desincronizadas, gana la que tuvo tuning intencional.
- Al ser defaults del struct, ambos call-sites (Editor + Player) heredan los valores correctos cuando el dev no edita.
- Cualquier proyecto pre-F3H5 (sin `character` subkey) carga con los defaults F2H41 — `mood_player.exe` empieza a sentir el bob igual que PlayInEditor sin que el dev tenga que hacer nada.

**Alternativas descartadas:**
- Defaults Player (5.0/0.04): perpetúa el bug, requiere que cada dev edite manualmente para corregir.
- Promediar (4.25 / 0.045): inventar un valor que nadie eligió.

### Decisión 2 — Eye height como fields independientes (vs derivar de capsule)

**Contexto:** el código pre-F3H5 calculaba eye height como `halfHeight + radius - 0.2f` (en Player y Editor). Opciones para F3H5: (a) exponer `eyeHeight` como fields absolutos (default 0.7/0.3), independientes del capsule; (b) exponer `eyeOffsetFromTop` (default 0.2, el `-0.2` del cálculo) y derivar eye height de capsule + offset.

**Decisión:** opción (a) — `eyeHeightStand`/`eyeHeightCrouch` como fields absolutos independientes.

**Razones:**
- El "feel" del eye height puede ser intencionalmente desacoplado del shape físico. Ejemplos: capsule alto + ojos en el centro para POV bajo arcade-style; capsule chico + ojos arriba para sensación de "personaje alto" en POV.
- El dev arma su propio "preset" coherente — si quiere paridad eye-capsule, pone los valores manualmente.
- Más simple en el panel UI: cada slider es independiente.
- Dual-source-of-truth aceptable porque cambiar `radius` no afecta `eyeHeight` automáticamente (el dev tiene que actualizarlo si quiere consistencia — explícito > implícito).

**Trade-off:** si el dev cambia `radius` o `halfHeight` sin actualizar `eyeHeight`, los ojos pueden quedar "flotando" o "enterrados" en el capsule. Aceptable — es una decisión artística que el dev controla.

### Decisión 3 — Duplicar el helper `drawSlider` lambda vs promoverlo a helper compartido

**Contexto:** F3H4 introdujo un lambda `drawSlider` dentro de `drawGameplaySection`. F3H5 lo necesita para `drawCharacterSection`. Opciones: (a) duplicar el lambda en cada sección (idéntico, ~20 LOC repetidos); (b) promover a método privado de clase o helper en anonymous namespace; (c) método free en `editor/panels/project/SectionHelpers.h`.

**Decisión:** opción (a) — duplicar por ahora. F3H4 hace lo mismo, F3H5 confirma el patrón.

**Razones:**
- 2 secciones × 20 LOC = 40 LOC, manageable. Premature refactoring sería overkill.
- Cada lambda local captura `m_ui` por referencia y vive en el scope de su función — extraer a método requiere agregar member function + cambiar header.
- Regla "scope chico per hito" — F3H5 no es el momento de refactorear infra del panel.
- Cuando F3H6/F3H7 sumen más secciones (3+ tabs), el dolor del copy-paste justifica el refactor. Hasta entonces, claridad local > DRY.

**Condiciones de revisión:** si F3H7 cierra con 4 secciones que duplican el lambda, refactorear a helper de clase o `SectionHelpers.h`.

---

## 2026-05-24: F3H4 cierre — Gameplay tier 1 + polish reactivo (reset buttons, dock revert)

### Decisión 1 — Reset buttons (↺) per-field en Project Settings

**Contexto:** durante validación visual de F3H4, el dev notó que faltaba forma rápida de volver un slider a su default ("le falta un boton para resetear los valores por default"). Sin él, si tocás walk speed y querés volver a 5.5 hay que saber el número exacto y reescribirlo.

**Decisión:** botón pequeño con ícono `↺` (`ICON_FA_ROTATE_LEFT`, agregado al subset curado) al lado de cada control, **solo visible cuando `current != default`** (sin visual noise para fields no-modificados). Helper template `resetButton<T>` en el anonymous namespace de `ProjectSettingsPanel.cpp`. Aplicado a Target FPS (Performance) y los 4 sliders Gameplay; el patrón se reusa cuando F3H5+ agreguen fields nuevos.

**Razones:**
- Es UX estándar en Unity (right-click → Reset on field), Unreal (small reset arrow visible only when overridden), Godot (revert icon). El dev tiene la expectativa internalizada.
- El "visible solo cuando difiere del default" es key — surfacea el override sin ensuciar la vista cuando todo está en estado canónico.
- Helper template-based para reusar con int (Target FPS) y f32 (sliders) sin duplicación.
- i18n: 1 key compartida (`editor.project_settings.reset_default`) — reusable por cualquier panel futuro que copie el pattern.

**Alternativas descartadas:**
- Right-click context menu (Unity classic): más oculto, requiere descubrimiento, peor para devs novatos en el editor.
- "Reset all" botón a nivel sección: menos granular, fuerza reset de todo o nada.
- Mostrar siempre el botón (incluso si == default): visual noise innecesario.

### Decisión 2 — Project Settings vuelve a flotante NO dockeable (revert del intento polish)

**Contexto:** durante validación de F3H4, el dev pidió poder editar valores **mientras juega** ("recontra antiintuitivo abrir panel, cambiar, cerrar, poner play y testear"). Mi primer fix fue hacer el panel dockeable (quitar `NoDocking | NoResize`) para que pudiera quedar al costado del Inspector. El dev cuestionó: *"no me gusta que pueda agregarlo a un panel, ni redimensionar, dime en los motores graficos reales donde tienen estas opciones?"*.

**Respuesta honesta (engines reales):**
- **Unity** (2022+): `Edit > Project Settings` abre una **ventana flotante independiente**, NO dockeable al main editor. Tree de categorías a la izquierda + properties a la derecha. Cerrás cuando terminás.
- **Unreal**: Igual — Project Settings es ventana separada, NO parte del docking del editor.
- **Godot**: `Project > Project Settings...` es un **modal dialog** centrado de tamaño fijo. No dockeable, no redimensionable libre.

El común denominador: **Project Settings es SET-AND-FORGET, no para tunear en vivo.** Lo que el usuario tunea live durante Play va en otra superficie (Inspector sobre un component, debug HUD).

**Decisión:** revertir el dockable. Project Settings vuelve a `NoResize | NoCollapse | NoDocking`, centrado, tamaño fijo 540×360 (mismo que F3H1 polish). Los reset buttons se mantienen (esos sí están en convención de engines).

**Razones:**
- Honesto con la convención de la industria — devs que vienen de Unity/Unreal encuentran lo que esperan.
- Separación clara de paradigmas: panel de config defaults vs. live tuning son problemas distintos con UI distinta.
- El intento de "dockable para live tuning" mezclaba responsabilidades y ya estaba creando fricción (el dev intuitivamente quería cerrarlo antes de Play).

**Lo que NO se resuelve acá (diferido a hito futuro):**
- **Live tuning durante Play** queda como necesidad real pero sin solución en F3H4. Camino convencional: cuando exista un `PlayerControllerComponent` (per-entidad en escena), el Inspector dockeado lo edita en Play mode (Unity-style — `PlayerApplication` actual no tiene componente, es código directo). Alternativa: HUD overlay "Quick Tuning" en Play mode con sliders rápidos.
- Anotar como pendiente cuando el dev encuentre fricción real al tunear gameplay. Probable F3H5 o un mini-hito dedicado.

**Aplicación retroactiva:** la decisión F3H1 D1 ("dockable → floating modal-like") queda consolidada — esto NO la contradice, la confirma. Lo que se aprendió en F3H4 es que el caso de uso "tunear live durante Play" no es responsabilidad de Project Settings.

### Decisión 3 — Schema `.moodproj` con nested struct para Gameplay (no flat keys)

**Contexto:** F3H4 agregó 4 fields nuevos al `.moodproj`. Opciones de schema: (a) flat keys (`"walk_speed": 5.5, "crouch_speed": 3.0, ...`) al mismo nivel que `target_fps`; (b) nested subobject (`"gameplay": {"walk_speed": 5.5, "crouch_speed": 3.0, ...}`).

**Decisión:** nested subobject — `settings.gameplay.walk_speed`.

**Razones:**
- Escala mejor: cuando F3H5 sume Character (capsule/eye/headbob) tendrá su propio subobject `"character": {...}` sin chocar con `"gameplay": {...}`. Idem F3H6 Shortcuts, F3H7 Snap.
- Refleja la organización del UI (tabs del panel) directamente en el JSON.
- Forward-compat: agregar fields nuevos en cualquier subobject no contamina los hermanos.
- Match con la estructura de C++: `ProjectSettings::gameplay::walkSpeed`.

**Alternativa descartada:** flat keys con prefijo (`"gameplay.walk_speed"`) — funciona pero menos navegable en JSON, y rompe la convención nlohmann::json de pure objects.

---

## 2026-05-24: F3H3 cierre — auditoría hardcoded values + 2 fixes reactivos

### Decisión 1 — Audit-only NO: fix reactivo de bugs durante el sweep

**Contexto:** F3H3 estaba planeado como audit puro (sin código nuevo, solo `HARDCODED_AUDIT.md`). Durante los 3 sweeps paralelos se detectaron 2 bugs reales: (a) walk/crouch speeds Player↔Editor desincronizados (4.0/2.0 vs 5.5/3.0 — el tuning de F2H41 nunca llegó al Player); (b) gravity con 3 sources of truth (PhysicsWorld + VehicleConfig + VehicleConfigWriter, los 3 con literal `9.81f`). El dev preguntó: *"antes de hacer todo eso, porque no lo arreglamos? en lugar de hacer otro HITO aparte"*.

**Decisión:** fix reactivo durante el cierre F3H3. No esperar a F3H4 para cerrar bugs que ya están identificados y son triviales de arreglar.

**Razones:**
- Regla `feedback_plan_discipline`: "solo cambios reactivos a bugs". Los 2 hallazgos son bugs (no "el user no puede tunear" — sino "el sistema se comporta inconsistente").
- Walk/crouch: 4 líneas de cambio + un comentario. Fix trivial. Esperar a F3H4 = el usuario sigue sintiendo Player más lento que Editor por días/semanas innecesariamente.
- Gravity: refactor de literal a constante con `Mood::physics::kEarthGravityMagnitude`. Sin cambio de API pública, sin riesgo de regresión. El bug latente (suspensión mal calibrada si gravity cambia) sigue ahí pero ahora hay un solo lugar para cambiar.
- El audit doc sigue siendo el output principal del hito (~22 candidatos catalogados para F3H4-F3H7). Los 2 fixes son side-effect del proceso, no reemplazan el catálogo.

**Alternativas descartadas:**
- Audit-only puro (plan original): mantiene "scope chico" pero deja bugs detectados sin arreglar por dogma. Peor outcome para el usuario.
- Mergear F3H3+F3H4: el dev también propuso esta opción ("hacer la migración real ahora"). Descartada porque migrar walk/crouch a `.moodproj` requiere: agregar fields a `ProjectSettings`, JSON serialization, sección "Gameplay" en el panel con UI, reads en ambos call-sites, test roundtrip, validación visual. Eso es un hito real (F3H4), no un fix. Fix + migración separados mantiene scope chico per hito.

### Decisión 2 — `Mood::physics::kEarthGravityMagnitude` en PhysicsWorld.h (owner natural)

**Contexto:** los 3 sites con `9.81f` literal necesitaban una única constante. Opciones: (a) nuevo header `engine/physics/Gravity.h` solo para la constante; (b) constante en `core/Constants.h` (no existe); (c) constante en `PhysicsWorld.h` como member del namespace `Mood::physics`.

**Decisión:** opción (c). `constexpr float kEarthGravityMagnitude = 9.81f;` en namespace `Mood::physics` dentro de PhysicsWorld.h.

**Razones:**
- PhysicsWorld es el owner natural — el que llama `physicsSystem->SetGravity(...)` en init. La constante vive donde se usa primero.
- PhysicsWorld.h NO incluye Jolt (forward-decls). Incluirlo desde VehicleConfig.cpp + VehicleConfigWriter.cpp es lightweight (~5 forward decls).
- Crear nuevo header `engine/physics/Gravity.h` para una sola constante es overkill (regla CLAUDE.md: "prefer editing existing files").
- Cuando F3H4 migre gravity a `.moodproj > Physics`, este valor pasa a ser el default expuesto en la UI. Las fórmulas de suspensión necesitarán recibir el live value (no la constante) para calibrar bien — pero eso ya es trabajo de F3H4, no F3H3.

### Decisión 3 — Walk/crouch fix sin migrar (paridad inmediata)

**Contexto:** alternativa al fix simple (4 → 5.5, 2 → 3 en Player) era migrar walk/crouch directamente a `.moodproj > Gameplay` ahora, cerrando el bug y la migración en un solo paso.

**Decisión:** fix simple ahora. Migración real va en F3H4.

**Razones:**
- Migrar requiere agregar fields a ProjectSettings, sección "Gameplay" en el panel UI, JSON keys nuevas, lecturas en ambos call-sites, test, validación visual. Eso es F3H4 (un hito chico-mediano).
- F3H3 cierra el bug en 4 líneas. El usuario que corra el Player runtime mañana siente la paridad. F3H4 después le da la capacidad de tunear.
- Mantiene "scope chico per hito" — F3H3 = audit + 2 fixes reactivos triviales, F3H4 = migración completa de bucket.
- Los nuevos literales (5.5/3.0) van a desaparecer en F3H4 cuando lean de `.moodproj`. Hardcodeo intermedio aceptable.

---

## 2026-05-24: F3H2 cierre — User Preferences panel + popup viejo eliminado

### Decisión 1 — Mirror exacto de F3H1 (no innovar en patrón)

**Contexto:** F3H2 podía elegir entre (a) explorar un patrón distinto al de F3H1 (ej. modal con título dinámico tipo "Settings >" estilo VSCode), (b) replicar F3H1 al pie de la letra (mismas flags, mismo accessor `requestShow*`, mismo folder, misma estructura de sección única).

**Decisión:** mirror exacto. Mismas flags `NoResize|NoCollapse|NoDocking`, mismo tamaño 540×360, mismo folder `editor/panels/project/`, mismo accessor `requestShowUserPreferences()` que mutea `m_userPreferences.visible = true` directo (sin pasar por request/consume porque el panel state vive en EditorUI, no en EditorApplication).

**Razones:**
- Consistencia para el dev: aprende el patrón con F3H1, lo aplica con F3H2 sin re-leer.
- Consistencia para el usuario: los dos panels de "ajustes" del editor se ven iguales y se comportan igual (la única diferencia es que Project Settings exige proyecto activo).
- Reduce surface de bugs: copiar un patrón validado en producción es más seguro que inventar uno nuevo.

**Alternativas descartadas:** modal con título dinámico (gana flexibilidad pero pierde simplicidad — no hay demanda).

### Decisión 2 — Eliminar el popup F2H76 en vez de coexistir con el panel nuevo

**Contexto:** el popup modal `BeginPopupModal("###preferences_modal")` de F2H76 funcionaba bien. Opción: dejarlo como "legacy backup" y agregar el panel nuevo en paralelo.

**Decisión:** eliminar el popup completo (90 LOC de body en MenuBar.cpp + flags `m_showPreferencesPopup`/`m_prefsOpen` en MenuBar.h + 5 keys i18n huérfanas). `Edit > Preferences...` ahora siempre abre el panel nuevo.

**Razones:**
- Pulir = simplificar, no agregar paralelo. Tener dos formas de abrir Preferences = confusión.
- Code rot: el popup quedaría sin tocar y eventualmente nadie sabría cuál usar.
- Sin riesgo de regresión: la persistencia de tema+idioma vive en `UserSettings` (no en el popup), el panel nuevo reusa exactamente la misma infra.

**Aplicación:** mismo principio que F3H1 polish (eliminar tabs placeholder en vez de dejarlos esperando) — pulir = sustraer.

### Decisión 3 — Sin tests automáticos por path real APPDATA

**Contexto:** el plan F3H2 sugería "tests headless: roundtrip `UserSettings::save/load`". Pero `UserSettings::save()` escribe a `%APPDATA%\MoodEngine\settings.json` real (sin inyección de path posible sin refactorear el módulo). Un test que llame `save()/init()` contaminaría el state real del dev.

**Decisión:** no agregar tests automáticos en F3H2. La persistencia está validada en producción desde F2H43+F2H76 (idioma + tema funcionan hace meses). La verificación aquí es **visual** (cerrar editor + reabrir → preferencias persisten).

**Razones:**
- F3H2 no agrega fields nuevos a `UserSettings` (solo expone los existentes). No hay superficie nueva sin cobertura.
- Refactorear `UserSettings` para inyectar path es scope out (cambio de API que afecta `MoodEditor` + `MoodPlayer` + bootstrap). Si el módulo crece (F3H6 shortcuts, F3H7 autosave/font/density), refactorear ahí con tests aislados.
- Suite verde sin sumar fragilidad.

**Condiciones de revisión:** cuando F3H6 o F3H7 agreguen fields no triviales (ej. struct de shortcuts con N keybindings), considerar refactor + tests inyectando path.

### Decisión 4 — Panels coexisten (no mutuamente exclusivos)

**Contexto:** tras validación visual del dev, surgió la pregunta: "puedo abrir 2 paneles simultáneamente?" (refiriéndose a Project Settings + User Preferences abiertos a la vez, ambos centrados, solapados visualmente en el primer spawn). Tres opciones: (a) dejar como está (UX Unity/Unreal — panels coexisten), (b) mutuamente exclusivos (abrir uno cierra el otro), (c) offset al spawn (cada panel arranca con offset distinto).

**Decisión:** dejar como está. Los panels son ventanas flotantes (no modales bloqueantes); pueden coexistir igual que en Unity (Project Settings + Preferences) y Unreal (Project Settings + Editor Preferences).

**Razones:**
- Antes (F2H76): popup `BeginPopupModal` bloqueaba todo. UX limitada.
- Ahora (F3H1+F3H2): `Begin` flotante, panels son singleton (un solo miembro = no se pueden abrir 2 instancias del mismo), y el solape visual inicial se resuelve arrastrando una vez (ImGui recuerda la posición en `imgui.ini`).
- Mutuamente exclusivos sería más rígido que Unity/Unreal, sin ganancia clara.

**Condiciones de revisión:** si el solape inicial molesta como fricción real (no como observación), evaluar offset al spawn (opción C). Sin demanda concreta, no se cambia.

---

## 2026-05-24: F3H1 cierre — polish reactivo del panel + regla "no internal refs en UI"

### Decisión 1 — Panel "dockeable" → "floating modal-like" (revisión de D3 del plan)

**Contexto:** el plan F3H1 (sección 6.2 / Bloque D) eligió panel dockeable estilo Inspector. Tras validar el panel en vivo con el dev, el feedback fue: *"que aparezca mas en el centro, no permitas el resize"*. La intuición del dev coincide con el patrón Unity/Unreal: Project Settings es un dialog de configuración, no un panel del workspace.

**Decisión:** cambiar a ventana flotante centrada + `ImGuiWindowFlags_NoResize | NoCollapse | NoDocking`, tamaño fijo 540×360, `SetNextWindowPos` con `ImGuiCond_Appearing` + pivot 0.5,0.5.

**Razones:**
- Project Settings se abre, se edita, se cierra — no se "vive" en él como en el Inspector.
- Unity Project Settings y Unreal Project Settings ambos son ventanas no-dockeables.
- Sin resize, el layout del panel es predecible (mejor UX para diseñar fields).
- Centrar en cada `Appearing` (no `Once`) — si el dev cierra y reabre, vuelve al centro (no donde lo dejó la última vez).

**Alternativas descartadas:**
- Mantener dockeable + sin resize — choca: docks gobiernan el tamaño, NoResize en un dock se ignora silenciosamente.
- Modal estilo `pfd::open_file` — bloquea el editor entero; UX peor (el dev no puede ver el efecto del cambio en el viewport).

### Decisión 2 — Target FPS como Combo de presets en vez de DragInt

**Contexto:** el plan original usaba `ImGui::DragInt` con clamp [10, 240]. El dev pidió *"da unos valores por defecto, sea 30 o 60fps"* — quería opciones rápidas de seleccionar, no libertad numérica completa.

**Decisión:** Combo con presets fijos (30/60/120/144 FPS). Si el `.moodproj` trae un valor non-preset (caso edge: edición manual del JSON), aparece una entry extra al tope "Personalizado (N FPS)" que preserva el valor.

**Razones:**
- Patrón Unity Quality > Target Frame Rate (presets).
- Reduce decisiones del dev (lo más común es 60 → preset elegido en 1 click).
- El fallback "Personalizado" evita destruir datos cuando se carga un .moodproj editado a mano.

**Alternativas descartadas:**
- DragInt + Combo — duplica UI para el mismo field.
- Solo presets sin fallback "Personalizado" — silenciosamente snapearíamos valores legítimos al cargar (perdida de datos).

### Decisión 3 — Regla durable: "no internal milestone refs en UI"

**Contexto:** el dev observó al validar F3H1 que el panel mostraba *"Próximamente en F3H4+"* y el hint del Target FPS decía *"F3H1 only stores the value; the real cap lands in F3H4+"*. Feedback explícito: *"no nombres textos como F3H4, etc luego nos olvidamos de eliminar"*.

**Decisión:** regla durable — strings user-facing (i18n.json, tooltips, hint text, scaffold templates, dialog boxes) NUNCA referencian hitos internos (F2H1, F3H4, etc). Referencias a hitos viven solo en código (comments), `docs/`, y mensajes de commit.

**Razones:**
- Cuando un hito futuro cierra, las referencias en i18n quedan podridas (deuda de doc-cleanup acumulada en cada hito).
- End users (devs externos) no conocen nuestra nomenclatura interna.
- Las strings deben hablar del **feature en sí**, no de su origen.

**Memoria asociada:** `feedback_no_internal_milestone_refs_in_ui.md` (indexada en `MEMORY.md`).

**Aplicación retroactiva:** los placeholders eliminados de F3H1 + el hint reescrito. **Sin sweep histórico** (no entran a Fase 3 más cleanups proactivos del i18n existente sin demanda del dev) — la regla aplica de aquí en adelante.

### Decisión 4 — Eliminar campo "description" de ProjectSettings

**Contexto:** el plan F3H1 incluía `description: string` como segundo field prueba (junto a `targetFps`). Tras ver el panel, el dev: *"la descripcion del proyecto lo veo innecesaria, eliminala"*.

**Decisión:** borrado entero — struct + JSON + tests + UI + 2 keys i18n.

**Razones:**
- Metadata del proyecto (autor, notas) puede vivir en un `README.md` dentro del proyecto si emerge la necesidad — no requiere un field schema.
- Reducir scope = reducir mantenimiento.
- F3H1 queda con un único field (`targetFps`) — más limpio como chasis (un solo example pattern para que los hitos siguientes lo copien).

---

## 2026-05-23: Arranque Fase 3 — pulido, UX, nada hardcodeado

### Decisión 1 — Fase 3 = pulido, no features

**Contexto:** tras cerrar `v2.0.0` (88 hitos) + `v2.0.1-break-auditoria` + `v2.0.2-break-deferreds`, el dev me pidió analizar Fase 3 con el lente "ingeniero gráfico + UX". Su feedback explícito: *"realmente hay que pulir muchas de las herramientas que tenemos actualmente, desde lo mas basico, hasta lo mas avanzado"*.

**Decisión:** Fase 3 es *fase de pulido*. **Regla dura**: si un hito agrega un sistema nuevo, no es Fase 3 (defer a Fase 4). Si mejora algo que ya existe (UX, performance, configurabilidad, consistencia), es Fase 3.

**Razones:**
- Acumulamos 88 features en Fase 2 — el dev tiene fricción real con lo existente, no falta de features.
- Apilar más sobre un editor con UX inconsistente solo aumenta la deuda.
- Establecer disciplina ahora evita scope creep a lo largo de los ~27 hitos planificados.

**Alternativas descartadas:**
- "Mix de pulido + features sueltas" — diluye el norte; el dev rechazaría hitos pero perderíamos tiempo en debate por cada uno.
- Saltar a Fase 4 con features nuevas — ignora la fricción documentada en validaciones de F2H81/82/86.

**Condiciones de revisión:** si después de Sub-fase 3.1 emerge necesidad de feature crítica (ej. un sistema de combate para validar la sub-fase 3.4 de profiling), evaluar agendar como hito separado fuera de Fase 3.

### Decisión 2 — "Nada hardcodeado" como espina dorsal

**Contexto:** el dev fue explícito: *"algo que realmente no deseo en este futuro programa es tener valores hardcodeados, osea entiendes que este programa debera darle la libertad al usuario de editar lo que le plazca"*. Filosofía de producto: MoodEngine es un motor que terceros van a usar para hacer juegos diversos; hardcodear defaults a gusto personal restringe usabilidad.

**Decisión:** principio cross-cutting de toda Fase 3. Cuando aparezca un magic number / default / límite / color en código, evaluar dónde debería vivir (Project Settings / UserSettings / Inspector field / dejar en código solo si es matemática, magic number de algoritmo justificado, o límite duro del runtime).

**Razones:**
- Permite que el motor crezca como herramienta de terceros sin recompilación.
- Forza el diseño "data-first" — toda config es data editable, el código es sólo dispatch.
- Reduce deuda nueva durante Fase 3 (vs. arreglar settings hardcoded post-hoc en Fase 4).

**Alternativas descartadas:**
- "Solo configurable lo que el dev pida explícitamente" — no escala; cada vez que un usuario externo pida un setting, hay deuda nueva.
- "Refactor masivo de hardcodes en un solo hito" — bloque demasiado grande, alto riesgo de regresión. Por eso F3H3 audita + cataloga, y F3H4+ migra incremental.

**Memoria asociada:** `feedback_no_hardcoded_values.md` (indexada en `MEMORY.md`).

**Condiciones de revisión:** la regla NO aplica a (1) constantes matemáticas (PI, conversiones grados↔rad), (2) magic numbers de algoritmos con justificación documentada (epsilon GGX 0.05 para evitar NaN, threshold de Forward+ tile), (3) límites duros del runtime (max bodies de Jolt, max lights por tile).

### Decisión 3 — Orden de sub-fases: 3.1 PRIMERO (infra de configuración), 3.2-3.4 después

**Contexto:** mi propuesta inicial al dev fue 4 sub-fases ordenadas por "frecuencia de uso diario" (3.1 = Inspector daily, 3.2 = Asset workflow, 3.3 = Viewport pro, 3.4 = Performance). Tras incorporar el principio "nada hardcodeado", la 3.1 se convirtió en "El editor te respeta" (defaults configurables) y se promovió al inicio.

**Decisión:** Sub-fase 3.1 construye Project Settings + User Preferences. Las sub-fases 3.2-3.4 *consumen* esa infraestructura. Sin 3.1 primero, las siguientes acumularían deuda nueva (más hardcodes a la pasada).

**Razones:**
- Quality-of-life features en Inspector / Asset Browser / Viewport van a *querer* exponer sus defaults al dev — si no existen los paneles de Settings, esos defaults nacen hardcoded.
- "Spawn inteligente" (F3H5: posicionar entidades nuevas en cursor 3D vs (0,4,0) hardcoded) requiere que la posición default sea editable per-proyecto.
- Profiler (F3H23) y stats overlay (F3H24) tienen N decisiones de "qué mostrar por default" que deberían ser preferences.

**Alternativas descartadas:**
- "3.2 (Inspector polish) primero porque es lo que el dev usa todo el día" — tentador, pero deja la deuda de hardcode-creep durante 6+ hitos.
- "Sub-fase 3.0 dedicada solo a la infra de settings sin migrar nada" — overlap conceptual; preferimos que 3.1 migre 2 fields prueba para validar la infra end-to-end.

### Decisión 4 — Schema `.moodproj`: sin bumps explícitos, back-compat por defaults

**Contexto:** F3H1 introduce sección `"settings": {...}` en `.moodproj`. Decisión técnica: ¿bumpear schema version cada vez que agreguemos un setting, o forward+backward compatible por defaults?

**Decisión:** sin bumps. Aplicar el patrón validado en el cleanup de `HudState.ammo` post-v2.0.2: si una key no existe en disco, usar default; si una key existe en disco pero el código no la lee, ignorar.

**Razones:**
- Fase 3 va a agregar fields incrementalmente en cada hito (3.1 → 3.4). Bumpear cada vez es verboso e innecesario.
- Patrón ya validado: el cleanup de `ammo` no rompió saves v4 (json silenciosamente ignora keys extra; defaults cubren keys faltantes).
- Reduce burden de migration code que nadie revisa.

**Alternativas descartadas:**
- (a) Bump explícito cada vez (`v1` → `v2` → ...) — verboso, no protege contra nada que los defaults no cubran ya.
- (c) Versión por-sección (`settings.general.v=2`) — agrega complejidad sin caso de uso que justifique.

**Condiciones de revisión:** si emerge un cambio *incompatible* (renombrar key, cambiar tipo de value), ahí sí se bumpea + se escribe upgrader. La regla es solo para *additive* changes.

---

## 2026-05-23: F2H86 — Environment como entidad de primera clase + HDRI swap

### Decisión 1 — Cerrar 1.0 + 1.-1 en un mismo hito en vez de dos chicos

**Contexto:** Releí el BACKLOG con el dev pidiendo crítica honesta. Los únicos items con fricción real + precedente industrial eran 1.0 (entry point del Environment, fricción documentada en F2H61) y 1.-1 (HDRI switcher, pedida durante validación de F2H64). Mi propuesta inicial fue cerrar **solo 1.0** porque descubrí durante el plan que el `skyboxPath` estaba serializado pero **no consumido** por el renderer (placebo) — el HDRI switcher requería más scope que la estimación de 2-3h del BACKLOG.

**Decisión:** El dev autorizó el scope grande (1.0 + HDRI real, 6-8h).

**Razones:**
- Ambos items son del mismo subsistema (`EnvironmentComponent`). Cerrarlos juntos evita dos pasadas al `SceneRenderer` + `InspectorPanel_Environment.cpp`.
- Si se cerraba solo 1.0, la UX quedaba mocha (Environment como entidad sin poder cambiar el skybox = fricción persistente).
- El refactor `loadSkyboxAndIblFromBase` queda como infra reusable para futuros features de IBL dinámico.

### Decisión 2 — Auto-detect equirect vs cubemap dir en el loader

**Contexto:** Hay 2 formatos coexistiendo: `sky_kloofendal.png` (equirect, 1 archivo) y `sky_day/{px,nx,py,ny,pz,nz}.png` (cubemap dir, 6 archivos). `SkyboxRenderer` ya soportaba ambos modos (Hito 15).

**Decisión:** `loadSkyboxAndIblFromBase(base)` chequea `<base>.png` primero, si no `<base>/px.png`. El usuario no se entera del formato — solo elige preset.

**Razones:**
- Compatibilidad histórica: HDRIs de Polyhaven vienen como equirect, cubemaps procedurales son dir.
- Una única convención de path mataría el otro formato (perderíamos assets ya generados).

### Decisión 3 — BRDF LUT global, no parte del swap

**Contexto:** El IBL son 3 cubemaps + 1 LUT (irradiance, prefilter 5 mips, brdf_lut). El BRDF LUT es **tabular** — función de `(N·V, roughness)` precomputada, idéntica para todo environment.

**Decisión:** BRDF LUT se carga **una vez en el init**, no por skybox.

**Razones:**
- No depende del HDRI → recargarlo es waste.
- Reduce GL state churn per-swap.
- Mismo patrón que Unity / Unreal (BRDF LUT es global del engine).

### Decisión 4 — File picker custom sin auto-bake del IBL

**Contexto:** El usuario puede elegir un HDRI custom via file picker. Bakear el IBL **en runtime** requiere compute shaders (importance sampling + prefilter convolution) — 4-6h de implementación adicional + scope distinto.

**Decisión:** Aceptar el path, log warn si el IBL bake no existe, hint en el Inspector con el comando offline (`python tools/bake_ibl.py <path>`). El skybox visualmente cambia; los reflejos IBL caen a ambient escalar hasta que se bakee.

**Razones:**
- Workflow industrial: Unity y Unreal también bakean IBL offline (Reflection Probe = on-demand, no por frame).
- Runtime bake = scope F2 o F3 (cuando se agregue compute shaders).
- El bake offline ya existe (`tools/bake_ibl.py`) y es razonable (~30s/HDRI).

### Decisión 5 — Cambio del default `skyboxPath` de `sky_day` a `sky_kloofendal`

**Contexto:** Pre-F2H86, el componente decía `"skyboxes/sky_day"` pero el renderer cargaba `kloofendal` hardcoded — mismatch silencioso. Post-F2H86 el path se respeta runtime → si dejaba sky_day como default, todo Environment recién creado pediría swap a sky_day al cargar (innecesario).

**Decisión:** Cambiar default del struct a `"skyboxes/sky_kloofendal"`. Test nuevo guarda la regresión.

**Razones:**
- Coherencia con el bootstrap del renderer.
- `.moodmap` antiguos con `sky_day` explícito siguen funcionando (el parse respeta el JSON, default solo aplica cuando la key falta).
- Tests existentes (test_scene_serializer, test_package_builder) setean valores explícitos — no afectados.

---

## 2026-05-23: F2H85 — Save As contextual + Shift+D duplicate

### Decisión 1 — Save As con el mismo patrón de F2H78 (cada panel en su render)

**Contexto:** Una alternativa era centralizar Save As en `EditorApplication` con un dispatcher (`saveAsFromShortcut()`) que pregunte a cada panel.

**Decisión:** Mantener el patrón de F2H78 — cada editor maneja su Ctrl+Shift+S en su render (`drawSaveBar` detecta `KeyCtrl && KeyShift && IsKeyPressed(S)`); el handler global solo gatea el project-saveAs vía `IPanel::consumesSaveAsShortcut()`.

**Razones:**
- Idiomático ImGui: el foco se consulta donde ImGui lo expone (dentro del render del panel).
- Menos invasivo: no hay que reescribir lo que ya funciona.
- Consistencia con Ctrl+S existente.

### Decisión 2 — Diferir Material / Script / Shader Save As

**Contexto:** El hito original quería los 5 editors guardables.

**Decisión:** Solo Item + Quest. Documentar el resto en BACKLOG.

**Razones:**
- Material: el path lo administra el AssetManager (`saveMaterial(id)` usa el path interno cacheado). Necesita `saveMaterialAs(id, newPath)` — scope del AssetManager.
- Script + Shader: afectan `ScriptComponent.path` de una entity (side-effect en el componente). Save As cambia el path del componente, lo cual a su vez debe pasar por undo/serialización.
- Item + Quest tienen `saveToFile(path)` que toma cualquier ruta — drop-in inmediato.

### Decisión 3 — Offset Shift+D fijo en +X vs cursor-relative

**Contexto:** Blender usa modal mouse-tracked: la copia "sigue" al cursor hasta confirmar con click. Implementar eso en Mood requiere modal state + Enter/Esc handlers.

**Decisión:** Offset fijo `(+0.5 m, 0, 0)`. El usuario mueve la copia con el gizmo / G shortcut después.

**Razones:**
- El backlog (1.-3 desde F2H64) pedía "Shift+D duplicate", no modal mouse-tracked.
- 0.5 m es suficiente para distinguir visualmente la copia del original.
- Mouse-tracked es un upgrade futuro si emerge fricción concreta.

### Decisión 4 — Skip tiles del GridMap silenciosamente

**Contexto:** Una entity con tag `Tile_X_Y` es el render de una celda de `m_map` (GridMap). Duplicarla crea una entity huérfana fuera del grid; al rebuild de `m_scene` desde `m_map`, desaparece.

**Decisión:** Skip silencioso. Si la selección era 100% tiles, `duplicateSelectedEntities` es no-op.

**Razones:**
- Tile_X_Y no es un asset; es la representación visual de un dato en `m_map`. Duplicar es semánticamente nada.
- Mismo trato que `deleteSelectedEntity` (que trata tiles como caso especial → `SetTileCommand "Vaciar tile"`).
- Log no-op evita ruido en console; el usuario probablemente seleccionó un tile sin querer duplicarlo.

---

## 2026-05-23: F2H84 — Undo unificado en Material / Item / Quest editors

### Decisión 1 — Setter sin entity, captura el path al campo via lambda

**Contexto:** `EditPropertyCommand<T>` (Inspector, Hito 32 D) toma `Entity` + `Setter(Entity&, const T&)`. Los assets editados por Material/Item/Quest no son entities — viven en `AssetManager` (MaterialAsset) o en buffers internos del panel (m_loaded para Item/Quest). Adaptar el comando existente a "cualquier asset" sería invasivo.

**Decisión:** Nuevo `EditAssetPropertyCommand<T>` con Setter `(const T&)`. El callsite captura el resto via lambda:
- Material: `[mat](const f32& v) { mat->metallicMult = v; }`
- Item: `[this](const std::string& v) { m_loaded.icon_path = v; m_dirty = true; }`

**Razones:**
- Un solo comando genérico para los 3 editors.
- El callsite ya sabe qué campo está editando — abstraerlo no aporta.
- `m_dirty = true` queda dentro del setter para que execute() y undo() ambos marquen el panel como sucio.

**Trade-off:** los lambdas capturan `this` o `mat`. Si el panel descarga el asset entre push y undo, el lambda apunta a memoria inválida → ver Decisión 3.

### Decisión 2 — Diferir vectores / maps (tags, stats, objectives, rewards)

**Contexto:** Item editor tiene `tags` (vector<string> con add/remove inline) + `stats` (map<string,float>). Quest tiene `objectives` y `rewards` (vector de structs anidadas).

**Decisión:** No incluir en F2H84. La infra de `EditAssetPropertyCommand<T>` cubre el caso de campo simple (90%). Vector/map mutations son un patrón distinto (snapshot del contenedor o diff insert/erase respetando orden post-insert).

**Razones:**
- Scope: hito propio si emerge fricción real.
- Riesgo: vector commands con iteradores invalidantes son trampa para undo.

**Revisión:** atacar si el dev pierde una mutación grande y reporta dolor concreto.

### Decisión 3 — Limpiar history en cambio de asset vs migrar comandos

**Contexto:** Los lambdas capturan punteros (`mat`, `this->m_loaded`) que dejan de ser válidos cuando cambia el asset cargado. Dos opciones: (a) cada comando detecta "ya no soy relevante" y se vuelve no-op; (b) limpiar history al cambio de asset.

**Decisión:** (b). Patrón ya usado en `NodeGraphSandboxPanel` y `ShaderGraphEditorPanel`.

**Razones:**
- Cross-asset undo no es feature pedida.
- Migrar comandos agrega complejidad: rastrear identidad path↔asset, etc.
- Limpiar history es comportamiento predecible: Ctrl+Z opera sobre lo que estás viendo.

---

## 2026-05-23: F2H83 — Refactor de archivos grandes del editor (hot path render)

### Decisión 1 — `.inl` partial dentro de la clase, no header con structs top-level

**Contexto:** `EditorApplication.h` tenía 9 structs `private:` nested (sessions / gizmo state). Para sacarlas del header (835→673 LOC), tres opciones:
- (a) Header con structs en namespace top-level.
- (b) Header forward-declarado + definiciones en `.cpp`.
- (c) `.inl` partial incluido desde la sección `private:` del header.

**Decisión:** (c). El preprocesador inserta las structs como nested types de `EditorApplication`, sin cambiar nada externamente.

**Razones:**
- Cero superficie de cambio externa = cero riesgo de break en los `.cpp` siblings que ya referencian las structs por su nombre bare (`OrthoDragSession`, no `Mood::OrthoDragSession`).
- Preserva la encapsulación: las structs siguen siendo `private:` de la clase, no expuestas al namespace `Mood`.

**Alternativas descartadas:**
- (a) Promover a top-level: expone visibilidad innecesariamente y requiere actualizar todos los call-sites con el qualifier.
- (b) Forward-decl + def en `.cpp`: las structs son values (no pointers) en miembros del header → necesitan ser completas en el header.

### Decisión 2 — Métodos miembro vs helpers estáticos para los overlays F1

**Contexto:** 5 overlays F1-debug extraídos de `drawEditorScene3DOverlay`. Dos formas: (a) helpers estáticos en anonymous namespace tomando `(OpenGLDebugRenderer&, Scene&, AssetManager*, PhysicsWorld*)`, o (b) métodos privados de `EditorApplication` tomando solo `(OpenGLDebugRenderer&)` y accediendo a `m_scene` etc. via miembros.

**Decisión:** (b) métodos miembro.

**Razones:**
- Firma del caller queda 4x más corta. El overlay no se reusa fuera del editor — encapsular como métodos privados es lo correcto.
- Sigue el patrón del archivo original (`drawEditorScene3DOverlay` ya es método miembro).

### Decisión 3 — Diferir el refactor de `SceneRenderer_Render.cpp` con criterios explícitos de revisión

**Contexto:** El archivo está en 978 LOC, sobre el cap de 800. El comentario del propio archivo (heredado de F2H62) ya advertía: *"El frame loop es una unidad cohesiva con muchas variables locales compartidas entre pases — partir más fino requeriría extraer métodos privados con todas las dependencias como parámetros, lo cual no aporta legibilidad"*. F2H83 confirma con análisis detallado: las 2 lambdas centrales capturan 9+ locales (`view`, `projection`, `cameraPos`, `fbW`, `fbH`, `lights`, `iblOk`, `prefilterMaxLod`, `shadowEnabled`) usados transversalmente por todos los passes (instanced / static / skinned / brush / compiled-mesh / OIT).

**Decisión:** Diferir. Documentar como deuda activa en `BACKLOG.md §4` con criterios explícitos de cuándo atacarla.

**Razones:**
- Sin tests visuales (golden-pixel comparison) el riesgo de regresión silenciosa al tocar uniform bindings o GL state es alto.
- El refactor responsable es ~2h: definir `FrameRenderContext` struct, promover las 2 lambdas a métodos privados con el contexto como param, extraer pass-por-pass con verificación visual entre cada uno.
- Hoy no hay infra para validar visualmente cambios de render por código. El refactor es mejor aplazarlo a un momento donde haya esa cobertura, o donde el rediseño del backend (Vulkan/D3D12) lo fuerce naturalmente.

**Criterios de revisión** (cuando volver a evaluar):
- El archivo crece más allá de 1100 LOC.
- Emerge un bug gráfico que requiere modificar 3+ passes (señal de que el código es difícil de mantener).
- Se agrega cobertura de tests visuales al pipeline.
- Comienza un rediseño del renderer.

**Alternativas descartadas:**
- Hacer el split parcial sólo de OIT pass: ahorraría ~100 LOC pero deja el resto igual + introduce inconsistencia (1 pass extraído, 5 inline).
- Bajar el cap de LOC para que el archivo deje de violarlo: deshonesto.

---

## 2026-05-23: F2H82 — Modal Importar vehículo + bake GLB + anti-roll

### Decisión 1 — Anti-roll bars: Jolt built-in, no inventar nada

**Contexto:** Tesla + armor-car del backlog volcaban en cualquier curva mínima. Falta el componente que en autos reales evita el roll: la barra estabilizadora.

**Decisión:** Poblar `JPH::VehicleConstraintSettings::mAntiRollBars` con dos `VehicleAntiRollBar` (eje delantero FL↔FR, eje trasero RL↔RR, `mStiffness=3000`). Cero código nuevo de física.

**Razones:**
- Jolt expone la primitiva como parte estándar de `WheeledVehicleController` — es el mismo modelo que Unity Wheel Collider (`anti-roll bar` setting) y Chaos Vehicle de Unreal.
- Memoria `feedback_no_reinventar_rueda`: buscar estándar antes de codear.

**Alternativas descartadas:** simular el efecto con torques manuales sobre el chassis → reinventar mal lo que la lib ya resuelve.

### Decisión 2 — Bajar el CoM a mano, no introducir "anti-roll arcade"

**Contexto:** Aún con anti-roll bars, el Tesla tendía al vuelco porque su `mass_center_override_mm.y` estaba en 514 mm (más alto que el centro físico del modelo).

**Decisión:** Solo data — tunear el `.moodvehicle` (Tesla 514→150, armor-car 569→200). Sin código nuevo.

**Razones:**
- Es el truco clásico GTA SA / Burnout / NFS: bajar el CoM **por debajo** del centro físico colapsa la tendencia al vuelco. Modelado correctamente físico (el momento de inercia recibe menos torque de roll).
- Una sola línea de JSON por auto, reversible, no afecta a otros vehículos.

### Decisión 3 — Bake del scale en el GLB root node, no `mesh_scale` en runtime

**Contexto:** Modelos Sketchfab/FBX→glTF típicos vienen a escala 1/100 o 1/1000 (vértices en cm/mm). Hay que escalarlos al importar. Dos rutas:
- (a) Persistir un `mesh_scale: 100.0` en el `.moodvehicle` y multiplicar el Transform al spawnar.
- (b) **Hornear** el scale en el `.glb` copiado (root node), dejando el `.moodvehicle` con `scale=1.0`.

**Decisión:** (b). Tras intentar (a) en una iteración y notar que introduce **dos convenciones de unidades en el proyecto** (algunos assets a escala real y otros con un multiplicador implícito), pivot definitivo a hornear.

**Razones:**
- Una sola fuente de verdad: el GLB en disco está a escala real.
- AABB / colisión / iluminación / analyzer coinciden con el modelo en disco sin truco.
- El `.moodvehicle` queda comparable con los demás (mismos rangos numéricos).

**Alternativas descartadas:**
- Persistir `mesh_scale` permanente: dos convenciones = bug fest a futuro (cualquier código nuevo de física tiene que recordar consultar el campo).
- Re-exportar el GLB con todos los buffers re-escalados: orden de magnitud más complejo, sin beneficio frente al root node.

**Revisión:** si un modelo trae transform en cada nodo (no solo en root), el bake al root no escala todo. No es el caso de los exports comunes (Sketchfab / Mixamo / Blender export). Se mantiene la convención.

### Decisión 4 — Soporte de `matrix` y `scale` en el bake, no solo `scale`

**Contexto:** El DELOREAN.glb que el dev intentó importar tenía el transform del root como `matrix` 4x4 (típico de FBX→glTF pipeline de Sketchfab). El parser inicial solo soportaba la forma `scale: [sx,sy,sz]` y fallaba con "root node usa matrix (no soportado)".

**Decisión:** Extender `injectGlbRootScale` para detectar y multiplicar `matrix` también. Indices afectados: 0,1,2 (col0), 4,5,6 (col1), 8,9,10 (col2) — el rectángulo 3x3 — más 12,13,14 (traslación). Se skipean 3,7,11,15 (última fila `(0,0,0,1)` debe quedar intacta en column-major).

**Razones:**
- La spec de glTF permite ambas formas. Cualquiera de las dos es legal.
- Sin esto, ~50% de los modelos descargados de la web no entran.

**Alternativas descartadas:** convertir la matrix a `scale: [s,s,s]` y reescribir el nodo → pierde la rotación inicial del modelo (la matrix muchas veces trae también rotación de Z-up→Y-up).

### Decisión 5 — Wheel-entity skip por componente, no por tag

**Contexto:** `SceneSerializer` skipeaba las 4 wheel-entities matcheando `e.tag == "wheel_FL" || ... "wheel_RR"`. Funciona con autos que **nosotros nombramos** (delorean, banshee_sa) pero falla con autos importados cuyas ruedas tienen tags reales del modelo (`f_t_l`, `b_t_r`, `RUEDA_DEL_IZQ`). Al recargar el mapa: 4 ruedas viejas (persistidas) + 4 ruedas nuevas (respawneadas por `VehicleSystem`) = 8.

**Decisión:** Agregar `VehicleWheelMarker { chassisHandle, wheelIndex }` (`Components_Physics.h`). `VehicleSystem::spawnPendingWheels` lo añade a cada wheel entity. `SceneSerializer` skipea por `e.hasComponent<VehicleWheelMarker>()`. `ScenePick` y `HierarchyCollect` también lo consumen.

**Razones:**
- El marker es invariante (no depende del nombre del modelo).
- El tag vuelve a ser **solo display**, sin semántica oculta.
- Robusto frente al pipeline futuro de imports.

**Test impactado:** `test_scene_serializer_lighting_physics.cpp` (caso F2H70.3 H) creaba wheel entities con tag canónico pero sin el marker → fallaba tras la migración. Actualizado para añadir el marker. **No se afloja el test**: sigue verificando que las wheel-entities no se serializan, solo que la condición es "tiene marker" no "tag matchea".

### Decisión 6 — Live tuning edita el VehicleConfig compartido, no una copia per-entity

**Contexto:** Cuando el dev tunea un auto en Play y le gusta el resultado, quiere que el cambio quede para todos los spawns futuros, no solo esta entity. Dos formas:
- (a) Copiar el `VehicleConfig` a la entity y editar la copia.
- (b) Editar el config compartido del `AssetManager`.

**Decisión:** (b). `AssetManager::getMutableVehicleConfig(id)` devuelve un puntero al config compartido (nullptr para el slot 0 fallback). El Inspector edita ese puntero. Cualquier cambio marca `veh.dirty=true` y el `VehicleSystem` reaplica al body Jolt next frame.

**Razones:**
- Hoy un `VehicleComponent` solo guarda `configPath` + `dirty`, no una copia del config. Cambiar a "copia per-entity" implica tocar serialización + reload del config + flujos de assets — scope mucho mayor.
- Tunear el compartido es lo que el dev quiere: *"que el siguiente spawn también salga así"*.

**Trade-off aceptado:** si se spawnean dos autos del mismo modelo, tunear uno cambia el otro. Razonable para esta etapa — el dev valida un auto a la vez. Si emerge dolor real, F3 mete copy-on-write per-entity.

### Decisión 7 — Diferir extracción de texturas embebidas a Fase 3 (no como hito propio)

**Contexto:** El BTTF DeLorean importado entró bien geométricamente pero las texturas se ven rosa-grid (fallback de material faltante). Causa: las texturas vienen embebidas en el GLB y el loader las nombra `__runtime_tex#N` en memoria; al fallar la asociación material→texture cae a `missingMaterialId()`. Estándar industrial (Unity gLTFast, Unreal glTF Importer) extrae las texturas a disco en `assets/<asset>/textures/` al importar.

**Decisión:** No atacarlo como hito propio (F2H83). Entra a Fase 3 dentro del **pipeline industrial de imports** completo (extracción de texturas + materiales + LODs + colliders, todo en un solo paso).

**Razones (cita verbatim del dev al cerrar F2H82):** *"ese de extraccion de texturas eliminalo, porque a futuro deberemos si o si importar modelos de manera industrial, con sus texturas aparte, etc"*. Hacerlo como hito puntual ahora deja la mitad del trabajo + bloquea naturalmente al pipeline grande de Fase 3.

**Cita verbatim previa al cerrar F2H82:** *"lo logrado hasta ahora esta bien, cerremos aca para terminar con esto y luego en la fase 3 veremos como mejorar esto"*.

---

## 2026-05-21: F2H79 — Pulido de modales + hover circular + Welcome

### Decisión 1 — Hover circular de la X: parche idempotente a ImGui en configure-time, no fork

**Contexto:** El dev pidió que el hover del botón de cerrar (la X) de las ventanas sea un **círculo** en vez de un cuadrado, en todos los modales. ImGui dibuja ese fondo (`CloseButton` y `CollapseButton` en `imgui_widgets.cpp`) con `AddRectFilled(bb.Min, bb.Max, bg_col)` **hardcodeado** — no hay hook en `ImGuiStyle` para cambiar la forma.

**Decisión:** Parchear la fuente que baja CPM en **configure-time** desde `CMakeLists.txt`: `file(READ)` + `string(REPLACE)` cambia ambas ocurrencias a `AddCircleFilled(bb.GetCenter(), ImMax(2.0f, g.FontSize*0.5f), bg_col)`, con un comentario-marca `MOOD_CIRCLE_CLOSE`. Antes de parchear se busca el marcador (`string(FIND)`): si ya está, no se re-aplica.

**Razones:**
- No mantener un fork de imgui (CPM apunta al `docking` upstream).
- Idempotente y auto-sanador: si el build dir se limpia, CPM re-baja la fuente original y CMake la vuelve a parchear; si ya está parcheada, el marcador lo evita.
- `g.FontSize` y `bb` están en scope en ambas funciones → el reemplazo compila sin tocar nada más.

**Alternativas descartadas:** (a) fork de imgui — costo de mantenimiento. (b) reimplementar `CloseButton` propio y reemplazar las llamadas — invasivo y se desincroniza con upstream.

**Revisión:** Si un upgrade de imgui cambia la firma de esas líneas, el `string(REPLACE)` deja de matchear (no rompe el build, solo no aplica el círculo) → revisar el marcador.

### Decisión 2 — Selector de workspace = hamburguesa, no dropdown con nombre

**Contexto:** El primer intento (dropdown que mostraba el workspace activo y abría la lista) abría el popup **encima** del propio botón, tapando "Layout". 

**Decisión:** Botón hamburguesa ☰ de ancho fijo a la derecha; el popup cae **debajo** (`SetNextWindowPos` con pivote arriba-derecha). Se acepta perder el nombre del workspace activo en la barra a cambio de un selector que no se solapa y no cambia de ancho.

### Decisión 3 — El Welcome sigue bloqueado (sin descartar)

**Contexto:** Tras el remake, el dev notó que no hay forma de cerrar el modal y entrar al editor.

**Decisión:** Mantener el bloqueo (convención Unity/Godot: sin proyecto no hay escena que editar). Para salir se elige un reciente o se crea/abre uno. Se evaluó permitir descartar (entrar con escena vacía) y se descartó por dejar estado raro.

---

## 2026-05-21: F2H78 — Ctrl+S contextual

### Decisión 1 — Gatear el Ctrl+S global en vez de centralizar el guardado

**Contexto:** Se quiere que Ctrl+S guarde el panel con foco (script/shader/item/quest) y, si no hay editor enfocado, el proyecto. Los editores Script/Shader ya manejaban Ctrl+S ellos mismos en su render (vía `ImGui::IsWindowFocused`). El Ctrl+S global vive en el handler de eventos SDL (`EditorApplication`), **fuera** del frame ImGui — ahí no se puede consultar foco.

**Decisión:** Cada editor sigue guardando lo suyo en su propio render (patrón idiomático ImGui, ya usado por Script/Shader). El handler global solo **consulta** `IPanel::consumesSaveShortcut()` sobre los panels `visible`: si alguno lo consume, no dispara el project-save. Item/Quest ganan el self-save (no existía).

**Razones:**
- Menos invasivo: no hay que mover los 4 saves a un dispatcher central ni reescribir Script/Shader.
- Idiomático: el foco se consulta donde ImGui lo expone (dentro del render del panel), no en el handler SDL.

**Alternativas descartadas:** Centralizar todo en un dispatcher (`saveFromShortcut()` por panel llamado desde el handler) — más acople y reescritura de los que ya funcionaban.

### Decisión 2 — El flag de foco es del frame previo (aceptable)

**Contexto:** El handler SDL de Ctrl+S corre **antes** del render del frame; `m_windowFocused` se actualiza **durante** el render. Así que el handler lee el foco del frame anterior.

**Decisión:** Aceptarlo. El foco no cambia entre el keydown y el render del mismo frame, y el editor enfocado procesa su propio Ctrl+S en ese render con el foco actual. La única discrepancia posible (el frame exacto en que el foco cambia) es inocua y se autocorrige.

---

## 2026-05-21: F2H74 — Cleanup UX + capa de field-helpers del Inspector

### Decisión 1 — Auditar antes de "reorganizar": la UX de paneles ya seguía el estándar

**Contexto:** El pedido fue "reorganización de UX + limpiar que tenemos muchos imgui". Tentación: rediseñar la disposición de paneles.

**Decisión:** Auditar primero. Los 21 paneles ya están repartidos en 6 workspaces curados (patrón Blender Workspaces / Unity Layouts / Unreal Modes). No se tocó la disposición — habría sido inventar churn. El único gap real era el menú **Ver** (categorías mal mapeadas).

**Razones:** No reinventar lo que ya sigue el estándar industrial. El valor estaba en (a) el menú Ver y (b) la dispersión de código, no en mover ventanas.

### Decisión 2 — Field-helpers en `InspectorPanel_Internal.h`, no en `editor/ui/widgets/`

**Contexto:** Para colapsar el triplete *label+widget+undo* repetido ~76 veces, ¿una librería de widgets genéricos o helpers en el header del Inspector?

**Decisión:** Helpers en `InspectorPanel_Internal.h` (`fieldDragFloat/3`, `fieldColorEdit3`), namespace `Mood::detail`.

**Razones:**
- Están **acoplados al `InspectorEditTracker`** (toman tracker+ui+entity para el undo) — no son widgets reutilizables fuera del Inspector. Llamarlos "widgets genéricos" sería deshonesto.
- Cero includes nuevos: los 14 partials ya incluyen ese header.
- La reducción de dispersión ocurre en los **call sites** (de ~6 líneas a 1), que es donde estaba el problema.

**Alternativas descartadas:** `editor/ui/widgets/PropertyField.h` genérico — el acople al tracker lo haría un mal "widget genérico"; más archivos sin beneficio.

### Decisión 3 — Migración behaviour-preserving (no agregar undo donde no había)

**Contexto:** Al migrar, varios campos sin `pushEditIfDone` (checkboxes, light direction) podrían "ganarse" undo gratis con el helper.

**Decisión:** Migrar SOLO los campos que ya tenían `pushEditIfDone`. Los demás quedan raw, sin cambio de comportamiento.

**Razones:** Es un cleanup, no un cambio funcional. Agregar undo a campos que no lo tenían es una decisión aparte (y arriesgada en masa).

### Decisión 4 — El `helpMarker` entre widget y tracker mataba el undo; fix por reorden, salvo Transform

**Contexto:** `trackPropertyEdit` lee `GetItemID()`/`IsItemDeactivatedAfterEdit` del **último item dibujado**. Donde el `helpMarker` (`TextDisabled("(?)")` no interactivo) iba entre el widget y el `pushEditIfDone`, el tracker leía el ID del `(?)` → undo nunca disparaba (Joint, Trigger requiredTag, ForceField strength).

**Decisión:** Reordenar — `pushEditIfDone` inmediatamente tras el widget, `helpMarker` después (no dibuja widget, su `SameLine` sigue pegándose al widget). **Excepto Transform**: ahí el undo real viene de `applyDeltaToSelection` (multi-select); el `pushEditIfDone` final es dead code redundante — reordenarlo daría **doble-undo**. Se deja intacto.

**Razones:** Fix mínimo y quirúrgico del bug real, sin introducir un doble-registro donde ya hay otro mecanismo.

**Condiciones de revisión:** Si Transform deja de usar `applyDeltaToSelection`, revisar su `pushEditIfDone` muerto.

---

## 2026-05-21: F2H73 — Triggers avanzados (filtro por tag + one-shot + enabled)

### Decisión 1 — `requiredTag` filtra solo bodies, no al player

**Contexto:** El trigger reacciona a dos fuentes: el player char (único) y los `RigidBody` (N). Al agregar un filtro por tag, ¿debería aplicar también al player?

**Decisión:** `requiredTag` filtra **solo los bodies físicos** (compara contra `TagComponent.name`). El player se togglea por separado con el bool `triggersOnPlayer`.

**Razones:**
- El player es único; "filtrarlo por tag" no tiene caso de uso — o cuenta o no cuenta, eso lo decide un bool.
- Mezclar ambos (exigirle un tag al player) confundiría la API sin agregar expresividad.

**Alternativas descartadas:** Un `requiredTag` que aplique a player y bodies por igual — el player no tiene un tag de gameplay significativo en este modelo.

### Decisión 2 — `oneShot` se arma al primer enter de cualquier fuente válida

**Contexto:** Un trigger one-shot (checkpoint, cinematic) debe dispararse una vez. Pero hay dos fuentes (player / bodies) y dos filtros (`triggersOnPlayer` / `requiredTag`).

**Decisión:** El primer enter de **cualquier fuente que pase los filtros** (el player si `triggersOnPlayer`, o un body si matchea `requiredTag`) setea `fired=true` y mata el trigger hasta recargar el mapa.

**Razones:**
- `triggersOnPlayer` + `requiredTag` ya acotan **qué** puede armar el trigger; un "one-shot solo para X" sería redundante.
- Simple de razonar: "dispara una vez con lo que sea que lo active".

### Decisión 3 — Campos avanzados se serializan solo si difieren del default

**Contexto:** Agregar 4 campos al JSON del trigger podría romper mapas viejos o ensuciar el formato.

**Decisión:** `required_tag` / `triggers_on_player` / `one_shot` / `enabled` se escriben **solo si != default**. Mapas pre-F2H73 (sin las claves) cargan con los defaults correctos.

**Razones:**
- Back/forward compatible **sin bump de versión** del `.moodmap`.
- JSON limpio: un trigger común (sin flags) serializa igual que antes.

**Condiciones de revisión:** Ninguna — patrón ya usado en `ForceFieldComponent` (F2H72).

### Decisión 4 — Agregar `TriggerComponent` al gate del `SceneSerializer` (tercer caso standalone)

**Contexto:** El `SceneSerializer` solo persiste una entity si tiene un componente "ancla" reconocido. Un trigger suelto (sin mesh) no se guardaba — mismo bug que tuvieron `InventoryComponent` y `ForceFieldComponent`.

**Decisión:** Agregar `hasTrig` al gate, igual que `hasInv` / `hasFF`.

**Razones:** Un trigger es legítimamente una entity standalone (volumen invisible). Es el tercer componente standalone que cae en esta trampa — patrón ya conocido.

**Condiciones de revisión:** Si aparece un cuarto componente standalone, evaluar invertir el gate (lista de componentes que NO anclan, en vez de los que sí).

---

## 2026-05-21: F2H72 — Force fields / zonas de fuerza física

### Decisión 1 — Reusar el overlap del TriggerSystem (iterar entities) vs broadphase de Jolt

**Contexto:** Para aplicar fuerza a los bodies dentro de una zona hay que saber cuáles están adentro. Dos caminos: iterar las entities con `RigidBodyComponent` y testear su posición (como hace el `TriggerSystem`), o pedirle a Jolt un query de broadphase (`CollideSphere`/`CollideAABox`).

**Decisión:** Iterar las entities con `RigidBodyComponent` Dynamic y testear inclusión (OBB para Box, distancia para Sphere), reusando el mismo patrón del `TriggerSystem`.

**Razones:**
- **Consistencia** con cómo el trigger ya detecta bodies — un solo modelo mental en el código.
- **Sin API nueva** en `PhysicsWorld` (el broadphase query no está expuesto hoy).
- **Costo trivial** para el caso real (pocas zonas × pocos bodies dynamic).

**Condiciones de revisión:** Si emergen cientos de bodies y varias zonas, migrar a `BroadPhaseQuery::CollideSphere`/`CollideAABox` de Jolt (O(log n) vs O(n) por zona).

### Decisión 2 — `ignoreMass` = aceleración (no una fuerza fija)

**Contexto:** Una zona de viento o de gravedad debería mover todos los objetos igual sin importar su masa; un empuje "físico" (un chorro de aire a presión) debería mover menos a los objetos pesados.

**Decisión:** `strength` es Newtons por default (`addForce` directo → los pesados se mueven menos). Con `ignoreMass = true`, el sistema multiplica la magnitud por la masa del body → la fuerza produce la misma **aceleración** sin importar la masa (modelo de viento / gravedad de zona).

**Razones:**
- Cubre los dos casos reales (Unity expone ambos via `ForceMode.Force` vs `ForceMode.Acceleration`).
- Barato: la masa del body ya está en `RigidBodyComponent`.

### Decisión 3 — Aplicar la fuerza ANTES del step de física

**Contexto:** Jolt acumula las fuerzas de `AddForce` y las integra en su `Update`, limpiándolas después. El orden del sistema vs el step importa.

**Decisión:** Invocar el `ForceFieldSystem` justo **antes** de `updateRigidBodies` (que stepea), solo en Play.

**Razones:**
- La fuerza actúa ese mismo frame (sin lag de 1 frame).
- El primer frame de Play los bodies aún no están materializados (se crean dentro de `updateRigidBodies`), así que la zona empieza a actuar de frame 2 — invisible en la práctica.

### Decisión 4 — La zona de fuerza es una entity standalone (gate del serializer)

**Contexto:** El `SceneSerializer` solo persiste entities que tienen un componente "ancla" (mesh, light, rigidbody, etc.). Una zona de fuerza no tiene mesh.

**Decisión:** Agregar `ForceFieldComponent` al gate del serializer (igual que `InventoryComponent`) — una zona se persiste por sí sola aunque sea la única cosa en la entity.

**Razones:**
- Una zona de fuerza es un objeto de nivel de pleno derecho (como un trigger o un cofre), no un accesorio de un mesh.

---

## 2026-05-21: F2H71 — Slider + Fixed joints

### Decisión 1 — `FixedConstraint` con `mAutoDetectPoint = true` (sin pivot)

**Contexto:** Un Fixed joint suelda dos cuerpos (los 6 DOF locked). Jolt permite o bien especificar los puntos/ejes de anclaje, o auto-detectar la pose relativa actual de los bodies.

**Decisión:** Usar `mAutoDetectPoint = true` — el constraint fija la pose relativa **actual** de A y B al crearse. El Inspector no muestra pivot para Fixed.

**Razones:**
- **Estilo Unity** (su Fixed joint hace exactamente esto): el dev posiciona los cuerpos donde quiere y el joint los pega. Un campo menos que tocar.
- **Menos superficie de error**: no hay que alinear pivots a mano para que no haya un "salto" al materializar.

**Alternativas descartadas:**
- Pedir pivot explícito: redundante para un weld; el caso común es "pegá estos dos donde están".

### Decisión 2 — Invertir el signo de los límites del slider en el wrapper

**Contexto:** Jolt mide la posición del `SliderConstraint` como `(point2 − point1) · sliderAxis` = body2(B) menos body1(A). Cuando A (el dueño del joint) se mueve a FAVOR del `axis`, el valor de Jolt baja (negativo). Resultado pre-fix: los límites del Inspector funcionaban al revés (un `[-2, 0]` con eje +Y dejaba el cuerpo clavado arriba).

**Decisión:** Invertir al pasar a Jolt: `mLimitsMin = -userMax`, `mLimitsMax = -userMin`. La API pública queda intuitiva: **límite = cuánto desliza A a lo largo de `+axisLocal`** (+ = a favor del eje).

**Razones:**
- **El dev no debería aprender la convención interna de Jolt.** Que el signo coincida con "a favor del eje = positivo" es lo esperable.
- Atrapado y bloqueado por un test (`Slider desliza hasta el tope min y frena`) — sin él el bug pasaba (los otros tests solo cubrían lock total / lateral).

**Condiciones de revisión:** Si se agrega motor/spring al slider, revisar que el signo del target también quede consistente con esta convención.

### Decisión 3 — Re-sync de bodies + joints en `enterPlayMode` (no sync continuo en Editor)

**Contexto:** Bug general de física: `updateRigidBodies` materializa el body en su pose inicial y solo crea bodies con `bodyId == 0` — no reposiciona los existentes. Mover una entidad ya materializada y dar Play hacía que el body (pose vieja) pisara al Transform en el sync `body→Transform` → el objeto saltaba. Con un slider/fixed se notaba más (el constraint anclaba su reposo en la pose vieja).

**Decisión:** Al entrar a Play, re-sincronizar cada `RigidBody` a su `Transform` actual (`setBodyPositionRot`) + marcar todos los `JointComponent` dirty (re-materializan capturando la pose visual).

**Razones:**
- **Mínimo y correcto**: una pasada al entrar a Play garantiza "lo que ves es donde arranca", sin el costo/complejidad de un sync continuo body↔Transform en Editor Mode.
- **Acotado**: solo toca entidades con `RigidBodyComponent`. Player char (CharacterVirtual), vehículos (VehicleConstraint) y ragdolls usan otros bodies y no se ven afectados.

**Alternativas descartadas:**
- Sync continuo en Editor Mode (el body sigue al Transform frame a frame): cambio mayor, sin demanda concreta más allá de este caso.
- Reset completo del PhysicsWorld al entrar a Play: más caro y con riesgo de efectos colaterales (floor tiles, vehículos, ragdolls).

---

## 2026-05-21: F2H70.4 — Ruedas que rotan (split-by-node) + HUD de conducción

### Decisión 1 — Sub-mesh selector (include + exclude) en vez de splittear el `.glb`

**Contexto:** Para que las ruedas roten independientes del chassis, cada una tiene que ser geometría posicionable por separado. Dos caminos: (a) splittear el `.glb` en 5 meshes (chassis + 4 ruedas) en disco, o (b) mantener un mesh consolidado y seleccionar sub-meshes en el render.

**Decisión:** Mesh consolidado + selección por nombre de `SubMesh`. El chassis usa `MeshRendererComponent.hideSubMeshPrefix = "wheel_"` (exclude); cada wheel-entity usa `subMeshName = "wheel_FL/FR/RL/RR"` (include-only, ya existía). Los 4 wheel-entities + el chassis comparten el mismo `MeshAssetId` + materiales.

**Razones:**
- **Sin duplicar geometría** en disco ni en GPU (una sola carga de mesh).
- **`subMeshName` ya existía** como include-only; agregar el exclude complementario (`hideSubMeshPrefix`) es un cambio chico y simétrico.
- El render ya conoce `SubMesh.name` (= nombre del aiNode dueño), así que el filtro es trivial.

**Alternativas descartadas:**
- Split del `.glb` en meshes separados: duplica datos, complica el pipeline de assets, y obliga a cargar/trackear 5 meshes por auto.

**Condiciones de revisión:** Si un auto tuviera muchísimos sub-meshes y el filtro lineal por nombre pesara, se podría precomputar el set de índices a saltear. No es el caso hoy.

### Decisión 2 — Naming canónico de ruedas como único requisito asset-side

**Contexto:** El engine tiene que saber cuáles sub-meshes son ruedas y cuál es cuál (FL/FR/RL/RR) para posicionarlas en los attach points de Jolt.

**Decisión:** El engine es agnóstico al `.glb`; lo único que exige es que las 4 ruedas se llamen `wheel_FL/FR/RL/RR` y estén centradas en su hub. `tools/glb/split_wheels.py` produce eso desde cualquier auto, clasificando **por posición física** (no por el nombre original del nodo). Degradación elegante: un auto sin procesar simplemente no spawnea ruedas independientes (no crashea, no se rompe el render).

**Razones:**
- **Sistémico** (memoria `feedback-vehicle-sistemico`): el engine absorbe la variación del asset; el dev no tunea valores por-auto en C++.
- **Clasificación por posición** tolera modelos cuyas ruedas se llamen `RUEDRA_*`, `b_t_l`, etc. — el nombre original es irrelevante.

**Alternativas descartadas:**
- Hardcodear nombres de nodo por-auto: anti-sistémico, no escala a 10+ autos.
- Detección puramente geométrica (sin naming): más robusta pero más compleja; diferida al gestor de vehículos in-editor (backlog).

**Condiciones de revisión:** El gestor in-editor debería hacer la detección geométrica y absorber `split_wheels.py`.

### Decisión 3 — wheel-entities NO se serializan

**Contexto:** Las wheel-entities que spawnea el `VehicleSystem` tienen `MeshRenderer`, así que entraban por el check `hasMr` del serializer y se guardaban. Pero `VehicleComponent.wheelEntities[]` no persiste → al recargar el sistema respawnea 4 nuevas → **8 ruedas** (4 huérfanas guardadas + 4 respawneadas).

**Decisión:** El `SceneSerializer` saltea explícitamente las entities con tag `wheel_FL/FR/RL/RR` antes del resto de checks. Son estado runtime derivado del `VehicleComponent` del chassis; el `VehicleSystem` siempre las rematerializa al cargar (igual que ya hace con `vehicleId`). Se eliminó el `isWheelTag` capital (`Wheel_*`), código muerto del diseño placeholder de F2H67.

**Razones:**
- **Fuente de verdad única**: el auto se define por su chassis + `.moodvehicle`; las ruedas son consecuencia, no dato a persistir.
- **Evita el bug de duplicación** sin agregar lógica de reconexión de handles al loader.

**Alternativas descartadas:**
- Persistir las ruedas + reconectar `wheelEntities[]` por tag en el loader: más código, más superficie de bugs, y contradice que ya son derivables.

**Condiciones de revisión:** Cubierto por test de regresión (`test_scene_serializer_lighting_physics.cpp`).

---

## 2026-05-20: F2H70.3 — Vehicle Browser + drag-and-drop de vehículos al viewport

### Decisión 1 — Mesh declarado en el `.moodvehicle` (`body.mesh_path`)

**Contexto:** En el `.moodmap`, un vehículo es un entity con `mesh_renderer` (el `.glb`) + `vehicle` (el `.moodvehicle`) como componentes separados. Para que arrastrar un `.moodvehicle` al viewport spawnee un auto completo, el spawn necesita saber qué mesh usar.

**Decisión:** El `.moodvehicle` declara su propio mesh visual via `body.mesh_path` → `VehicleConfig.meshPath`. El config se vuelve self-contained: arrastrar uno solo basta para tener un auto andando.

**Razones:**
- **Estilo Source/Valve**: el script del vehículo (`scripts/vehicles/<car>.txt`) referencia su `.mdl`. Misma filosofía: tuning + modelo viven juntos.
- **Drag-and-drop de un solo archivo**: el dev no tiene que cablear el `mesh_renderer` aparte tras soltar.
- **Coherente con el modelo data-driven**: el `.moodvehicle` ya es la fuente de verdad del auto; el mesh es parte de esa identidad.

**Alternativas descartadas:**
- Mesh asignado aparte al spawnear (el drop crea solo el `VehicleComponent`, el dev asigna el mesh después): menos cómodo, rompe el "drop = auto completo". El dev eligió explícitamente la opción self-contained.

**Condiciones de revisión:** Si emergen vehículos sin mesh (puro proxy físico), `meshPath` vacío ya lo soporta (el drop omite el `MeshRenderer`).

### Decisión 2 — Drop al viewport como flujo primario (no al Inspector)

**Contexto:** El primer intento puso el drop-target en el campo `configPath` del Inspector. El dev señaló que el patrón natural es soltar en el viewport, como mesh/prefab/script/item.

**Decisión:** El flujo primario es drag-and-drop al **viewport** (spawnea un entity nuevo bajo el cursor). El drop en el Inspector queda como vía **secundaria** para reasignar el config a un vehículo ya existente.

**Razones:**
- **Consistencia**: todos los demás assets spawneables se sueltan en el viewport. Un flujo distinto para vehículos sería sorpresa.
- **Mental model claro**: viewport = "crear en el mundo"; Inspector = "editar lo seleccionado".

### Decisión 3 — Botón dedicado como drop-target en el Inspector

**Contexto:** El drop-target del Inspector se puso primero sobre el `InputText` del `configPath`. No disparaba.

**Decisión:** Usar un botón dedicado ("Soltar .moodvehicle aquí") como zona de drop, no el `InputText`.

**Razones:**
- `ImGui::BeginDragDropTarget()` sobre un `InputText` no funciona: el widget es activo y consume el drag internamente.
- Patrón ya validado en el repo: `InspectorPanel_Animation` usa exactamente un botón como drop-zone.

---

## 2026-05-20: F2H70.2 — Tuning físico del vehicle (damping + spawn elevation + frame consistente)

### Decisión 1 — `meshYawOffsetDeg` data-driven en lugar de bakear el .glb

**Contexto:** En F2H70.1 reorienté el `.glb` del DeLorean a +Z forward (bake con `reorient.py`) para cumplir la convención industrial. Eso rompió los controles (W/A/D invertidos) porque la cámara FPS del seat mount asume la convención vieja. El workaround fue revertir el .glb + `rotationEuler: [0, 180, 0]` en el moodmap (asset-specific).

**Decisión:** En lugar de bakear el yaw al vertex buffer, la convención del modelo se declara en el `.moodvehicle` via `body.mesh_yaw_offset_deg` (número) o `body.mesh_forward_axis` (`"+Z"/"-Z"/"+X"/"-X"` como azúcar). El engine compensa visualmente con `TransformComponent::pivotYawOffsetDeg` (runtime-only, post-multiply `Ry` en `worldMatrix()`) — afecta solo lo visual, NO la física.

**Razones:**
- **El asset queda intacto**: bakear modifica el .glb permanentemente; declarar la convención lo deja reusable.
- **Drag-and-drop friendly**: dev arrastra un .glb nuevo, genera `.moodvehicle` base; si al subirse los controles salen invertidos, edita un campo. Sin tocar el moodmap.
- **Persiste con el vehículo, no con la escena**: vale en cualquier mapa donde aparezca el auto. El `rotationEuler` del moodmap era per-instancia (se duplicaba por cada spawn).
- **No rompe la cámara mount**: la física (chassis Jolt) sigue en +Z forward; solo el mesh visual se rota. La cámara, anclada a la física, no se desfasa.

**Alternativas descartadas:**
- Bakear el .glb (F2H70.1): rompía cámara mount + requería revertir.
- `rotationEuler` en el moodmap: per-instancia, se duplica, mezcla convención-de-modelo con pose-de-escena.
- Arreglar la cámara mount para que detecte el forward del modelo: scope mayor, frágil (¿cómo detecta el forward de un .glb arbitrario?).

**Condiciones de revisión:** Si emergen vehículos con forward en ejes no-cardinales (diagonal), `mesh_yaw_offset_deg` numérico ya lo cubre (acepta cualquier ángulo).

---

### Decisión 2 — `wheelRestCompression` mass-independent (`g/(2π·f)²`)

**Contexto:** El chassis flotaba/brincaba al spawn porque `pivotYOffset = -aabbMin.y` no consideraba que Jolt spawnea las wheels en `suspensionMaxLength` extendida y el primer step las comprime hasta equilibrium (bajando el chassis).

**Decisión:** Helper `wheelRestCompression(WheelConfig)` retorna `g/(2π·f)²` (la compresión del spring bajo gravedad en equilibrio). Sumado al `pivotYOffset` para spawnnear el chassis más alto, de modo que tras el settle quede en su lugar.

**Razones:**
- **Mass-independent**: en `k·x = m·g` con `k = (2π·f)²·m`, la masa se cancela → `x = g/(2π·f)²`. Solo depende de la frecuencia del spring. Esto evita meter `chassisMass` al cálculo del offset y mantiene la fórmula pura.
- Físicamente correcto: f=1.5 Hz → ~11cm; f=1.8 Hz (DeLorean) → ~7.7cm; f=2.5 Hz (sport stiffer) → ~4cm. Coincide con la intuición (springs más rígidos comprimen menos).

**Alternativa descartada:** aproximación pragmática `suspensionMaxLength - suspensionMinLength` (range del spring) — menos exacta físicamente; la fórmula cerrada es igual de simple y correcta.

---

### Decisión 3 — `attach_y_mm` como knob de grounding (no auto-derivar)

**Contexto:** El spring-aware (Decisión 2) arregló el brinco al spawn pero no el float en equilibrio: el chassis físico equilibrium quedaba con su centro a `attach_y_abs + spring_rest + radius = 0.85m`, pero el modelo (centrado, half-height 0.568m) se renderea en ese centro → su base flotaba `0.85 - 0.568 ≈ 0.285m`.

**Decisión:** Exponer `attach_y_mm` como knob data-driven (ya existía en el schema v2). Para el DeLorean se tuneó de -300 a **-15** (`-(half_height - spring_rest - radius)`), de modo que el chassis físico equilibrium quede con su centro exacto a la half-height del modelo → base apoyada.

**Razones:**
- **Auto-derivar requiere conocer la posición visual de las wheels** (sub-meshes `wheel_*`), que necesita split-by-node (Bloque H de F2H70.3, no implementado). Hasta entonces, el knob manual es el camino.
- `attach_y_mm` es exactamente el grado de libertad correcto: define dónde se monta el shock en el chassis local, lo que determina la altura del equilibrium.
- Las wheels físicas (invisibles, Jolt) y las visuales (parte del .glb) ambas terminan apoyando con el valor correcto.

**Condiciones de revisión:** Cuando F2H70.3 implemente split-by-node, auto-derivar `attach_y` desde el centro visual de cada wheel sub-mesh → el dev no tiene que tunearlo manualmente.

---

### Decisión 4 — S-key brake-stick con edge-detection (patrón GTA/Forza)

**Contexto:** Sin lógica de modo, apretar S mientras el auto avanza alternaba entre frenar y meter reverse según la velocidad instantánea cruzara 0.5 m/s — feel errático (el auto entraba a reverse antes de detenerse del todo).

**Decisión:** Edge-detect en S: al press inicial decide UNA vez brake-vs-reverse según `forwardSpeed > 0.5 m/s`. Mientras siga pressed, mantiene el modo aunque la speed cruce el umbral. Reverse requiere release + repress.

**Razones:**
- Patrón estándar GTA/Forza/etc.: soltar W + apretar S sigue siendo "freno" hasta detenerse por completo; para ir en reverse hay que soltar y volver a apretar.
- Evita el "auto entra a reverse mientras todavía estás frenando" que confunde al jugador.

---

## 2026-05-20: F2H70.1 — Sistema data-driven de vehículos (Source/Valve-style)

### Decisión 1 — Pivot mid-hito: "fix 3 bugs" → "sistema data-driven completo"

**Contexto:** El plan original de F2H70 era resolver 3 bugs sistémicos del VehicleSystem identificados en F2H69 (auto-spawn-height, quat sync, split-by-node). Mid-sesión, propuse agregar una `makeDeLoreanDMC12()` con specs reales (mass 1230 kg, peak torque 208 Nm @ 2750 rpm, etc.) hardcoded en C++ junto al `makeDefaultSA()` genérico.

**Objeción verbatim del dev:** *"qué pasa si mañana yo agrego 10 autos más? entiendo que tendremos que tener algo más complejo como un sistema para trabajar otros autos... tener los valores hardcodeados, no lo veo realmente viable."*

**Decisión:** Pivot F2H70 de "fix 3 bugs" a "sistema data-driven completo estilo Source Engine/Valve". Los Bloques A+B (engine fixes spawn-height + quat sync) se conservan pero ahora sirven al sistema general, no al DeLorean-specific. Bloques C-I son nuevos: schema v2 axle-based, catálogo de assets reales, rename del fallback genérico, convenciones documentadas, scripts `tools/glb/` versionados. F2H70 se subdivide en .1 (sistema base) + .2 (tuning físico residual) + .3 (wheels visuales con split-by-node).

**Razones:**
- Hardcodear specs en C++ no escala a 10+ vehículos: cada uno requiere recompile + cambio de código vs simple `.moodvehicle` JSON drop-in.
- Source Engine ships con `scripts/vehicles/jeep_test.txt`, `airboat.txt`, etc. — patrón battle-tested en 200+ mods de Half-Life 2 / Garry's Mod.
- Engineering profesional ≠ parches a corto plazo. El dev explicitó: *"comencemos a pensar como verdaderos ingenieros y no juniors en este tema"*.
- Alineación con la memoria `feedback-no-reinventar-rueda`: buscar standard industrial antes de diseñar algo propio.

**Alternativas descartadas:**
- Mantener plan original "fix 3 bugs": un hito que solo arregla bugs sin proveer el catálogo escalable. El dev hubiera tenido que crear `makeBanshee()`, `makeBuggy()`, etc. en C++ para cada vehículo futuro.
- Hardcodear como struct C++: similar problema; obliga a recompile + linker dependency entre gameplay code y vehicle data.

**Condiciones de revisión:** Si el patrón Source Engine resulta demasiado pesado en F2H70.2/.3 (al construir el Vehicle Browser UI) podemos simplificar — el schema v2 ya es backward-compat con v1 flat, así que devolver al patrón antiguo es trivial.

---

### Decisión 2 — Schema v2 axle-based en lugar de wheels-individuales

**Contexto:** El v1 del `.moodvehicle` (F2H67) listaba las 4 wheels individualmente como array. Para refactor v2, había dos opciones: (a) ampliar v1 con más campos (PBR-like wheel granularity), (b) replicar el patrón axle-based de Source (1 axle delantero + 1 trasero, cada uno con su wheel/suspension/torque_factor/brake_factor).

**Decisión:** v2 axle-based. El loader expande `axle_front` → wheels FL+FR con `attachLocal = [-track_mm/2000, attach_y_mm/1000, offset_z_mm/1000]` y mirror para FR. Similar para `axle_rear` → RL+RR.

**Razones:**
- **Conciso**: 2 axles vs 4 wheels reduce duplicación. Modificar `friction_long` de las dos ruedas traseras (tuning RWD oversteer típico) es 1 edit, no 2 ediciones idénticas.
- **Semánticamente correcto**: `torque_factor` por axle modela exactamente lo que pasa físicamente en un diff (el diferencial reparte torque al axle, no a wheels individuales).
- **Reusabilidad de modders**: devs viniendo del modding de Source (Gmod, HL2 mods) reconocen el patrón instantáneamente.
- **Internamente sigue mapeado a 4 wheels en Jolt** (Jolt no soporta axles nativos): el loader hace la expansión, el resto del engine ve la misma estructura `wheels[4]`. Zero impact en `PhysicsWorld_Vehicle.cpp` y `VehicleSystem`.

**Alternativas descartadas:**
- Schema v1 ampliado con más campos por wheel: hubiera mantenido la verbosidad de 4 entradas para autos simétricos (mayoría).
- Tomar el JSON literal de KeyValues VDF: incompatible con el resto del proyecto (nlohmann/json).

**Condiciones de revisión:** Si emergen vehículos asimétricos (autos custom de drift con track distinto front/rear, motos con 2 wheels) reevaluar el patrón. Para autos estándar simétricos (95% del use case), axle-based es óptimo.

---

### Decisión 3 — Workaround `rotationEuler [0, 180, 0]` en moodmap del DeLorean (pragmático)

**Contexto:** En Bloque D apliqué `reorient.py yaw=180°` al `delorean.glb` para alinearlo a +Z forward (convención glTF estándar). Al validar visualmente, emergió bug: TODOS los controles del auto quedaron invertidos (W/A/D al revés). El frame visual rotó pero algo del code-path sigue con convención vieja (probable: la cámara FPS del seat mount asume `-Z forward` mientras Jolt usa `+Z`).

**Decisión:** Revertir el `.glb` reorientado al estado original (modelo mira -Z) + mantener `rotationEuler: [0, 180, 0]` en el moodmap del DeLorean específicamente. Asset-specific workaround. Bug sistémico agendado para F2H70.2.

**Razones:**
- **Cierre práctico de F2H70.1**: pelear el bug sistémico ahora alarga la sesión indefinidamente sin garantía de fix correcto. El dev explicitó cierre hoy.
- **Workaround acotado**: el moodmap es 1 archivo, 1 línea modificada. El asset (`.glb`) queda intacto y reusable para cuando se arregle el bug.
- **Visibilidad del problema**: dejar el workaround en el moodmap mantiene la "deuda" visible (el dev y agentes futuros ven el `[0, 180, 0]` y saben que es deuda técnica).
- **Cualquier vehículo nuevo no-DeLorean** debería poder hacer drop-in sin esto si está procesado con `reorient.py` correctamente y el bug sistémico del seat-mount se arregla en F2H70.2.

**Alternativas descartadas:**
- Atacar el bug sistémico ahora: estima 1-3h adicionales (necesita debuggear cámara FPS + `VehicleSeatComponent` + Jolt forward convention + posible cambio en `MountSystem`).
- Aceptar controles invertidos en F2H70.1: feel inutilizable para el dev al probar el sample.

**Condiciones de revisión:** F2H70.2 debe atacar el bug y permitir borrar el `[0, 180, 0]` del moodmap del DeLorean. Memoria asociada: `feedback-vehicle-sistemico`.

---

### Decisión 4 — `tools/glb/` versionado en repo, no en `c:/tmp/`

**Contexto:** En F2H69 creé scripts Python ad-hoc en `c:/tmp/` para procesar el `.glb` del DeLorean (scale, flatten, center_y). Funcionales pero con paths hardcoded, sin args, sin docstrings claros.

**Decisión:** Promoción a `tools/glb/` versionado en git con 6 scripts genéricos (`scale.py / flatten.py / center_y.py / reorient.py / verify.py / diag.py`) + `common.py` (helpers compartidos) + `README.md` (pipeline recomendado).

**Razones:**
- Pipeline reusable: cualquier asset nuevo necesita el mismo flujo (verify → scale → flatten → reorient → verify). Scripts efímeros = re-escribirlos cada vez.
- Engineering profesional: las herramientas del pipeline forman parte del proyecto, no del scratch del dev.
- Alineación con `docs/asset_conventions.md`: el doc define la convención (1u=1m, +Z forward, etc.) y los scripts son la herramienta para conformar assets externos a esa convención.
- argparse + sin paths hardcoded → reusables para futuros vehículos sin modificar el script.

---

### Decisión 5 — Rename `makeDefaultSA()` → `makeFallbackGenericSedan()`

**Contexto:** El nombre `makeDefaultSA` (de F2H67) sugería que era el "default ideal" para vehículos del estilo GTA SA. Con el sistema data-driven, el rol real es **fallback genérico** cuando un `.moodvehicle` no carga (path vacío, JSON inválido).

**Decisión:** Rename a `makeFallbackGenericSedan`. Sentinel interno `__default_vehicle_sa` → `__fallback_generic_sedan`. 47 ocurrencias en 9 archivos (src + tests). El log path emite warn cuando se cae al fallback ("vehicle '<path>' no carga, fallback a generic sedan") — alerta al dev que algo está mal con el config.

**Razones:**
- Specs reales del DeLorean (o cualquier otro vehículo) NO viven en C++ — están en `.moodvehicle` files. C++ solo provee fallback inocuo.
- El nombre comunica el contrato: NO usar como punto de partida deliberado; SI usar implícitamente cuando todo lo demás falla.
- Warn en logs detecta configs rotos temprano.

---

### Decisión 6 — Build + abrir editor obligatorios antes de cerrar bloque

**Contexto:** Durante la sesión el dev abrió `MoodEditor.exe` Release con timestamp del **10 de mayo** (9 días antes de los commits de Bloques A+B). Cuando reportó visualmente "modelos enormes + sin texturas", asumí regresión de mis cambios; en realidad el binario ni los contenía. Diagnóstico llevó 30+ minutos hasta comparar timestamps. El dev objetó: *"no se supone que ante cada cambio deberías hacer un build como es que vengo probando cosas con 10 días de retraso?"*.

**Decisión:** Cada cambio a `.cpp` o `.h` del engine requiere **rebuild + abrir editor + validar visualmente** antes de marcar bloque completo. Tests verdes (1029/10227) NO son suficientes — las suites no cubren render visual / asset loading / material binding.

**Razones:**
- Confianza basada en evidencia: tests headless solo validan contratos unitarios, no integration visual.
- El dev confía en lo que ve en el editor, no en exit codes de tests.
- El workflow correcto está alineado con [memoria `feedback-auto-accept`](../C:/Users/Daniel/.claude/projects/c--Users-Daniel-Documents-GitHub-MoodEngine/memory/feedback_auto_accept.md): el dev autoriza builds/tests automáticos; solo pedir permiso para correr el editor.

**Memoria asociada:** [`feedback-build-validar-siempre`](../C:/Users/Daniel/.claude/projects/c--Users-Daniel-Documents-GitHub-MoodEngine/memory/feedback_build_validar_siempre.md).

---

## 2026-05-19: F2H69 — Trigger NPC debug + pipeline glTF multi-node + DeLorean swap

### Decisión 1 — Sensor bodies fuerzan `mAllowSleeping=false` independiente del MotionType

**Contexto:** El sample F2H68 (vehicle vs NPC sensor) no transicionaba a Ragdolling. Logs en runtime revelaron que `JPH::Body::IsSensor()` auto-fuerza `mAllowSleeping=false` solo si el body se crea Dynamic. Sensores Kinematic (caso del NPC: kinematic + `is_sensor: true`) heredan el default `mAllowSleeping=true` → entran a sleep tras ~5s sin movimiento → Jolt no genera `OnContactAdded` para sensores dormidos (optimización del broadphase).

**Decisión:** En `PhysicsWorld::createBody`, si `isSensor==true`, force `mAllowSleeping=false` independiente del MotionType. Pattern Unity (`Collider.isTrigger` triggers permanentes) / Unreal (`Overlap` actors).

**Razones:**
- Un sensor que duerme deja de funcionar como trigger — semánticamente roto.
- Cost: bodies sensor Kinematic estáticos consumen un slot del broadphase que no entra a sleep. Trade-off aceptable: triggers son contados (NPCs interactivos, zonas de daño), no son cientos.
- Match con convención industrial: Unity/Unreal docs explícitamente recomiendan que triggers stay-awake siempre.

**Alternativas descartadas:**
- Wake periódico desde gameplay code: brittle (¿cada cuánto?), introduce dependencia del trigger en el script.
- Setear `mAllowSleeping=false` solo en el callsite del NPC (no en `createBody`): asset-specific, próximo trigger sufre el mismo bug.

### Decisión 2 — `impactSpeedThreshold` default 4.0 → 1.0 m/s

**Contexto:** F2H68 dejó el threshold en 4 m/s. Validación en runtime: el dev necesitaba arrancar el auto con throttle pleno desde lejos para activar el trigger. El feel arcade-ish (estilo GTA SA) querido necesita activación a velocidades de maniobra normal.

**Decisión:** Default permanente `impactSpeedThreshold = 1.0 m/s`. Atropellar caminando funciona; falsos positivos por contacto inicial (chassis tocando NPC al spawn) quedan filtrados por margen mínimo.

**Razones:**
- Alineado con tuning `makeDefaultSA()` (alta tracción + brakes 4500 Nm + handbrake 6000 Nm) — auto arcade que decelera/acelera rápido, no es realista esperar 4 m/s de cierre para que "sienta" el contacto.
- Sobreescribible runtime con `setRagdollImpactSpeedThreshold(...)` si una escena requiere mayor robustez contra contactos accidentales (un sigilo donde el roce no activa, p. ej.).

**Alternativas descartadas:**
- Dejar 4 m/s y documentar: el dev tendría que recordar setearlo en cada mapa nuevo. Sin valor por defecto razonable.
- 0.5 m/s: demasiado sensible — chassis empujando al NPC al spawn lo activaría.

### Decisión 3 — `MeshLoader` para glTF/GLB hace **consolidation**, no split-by-node

**Contexto:** Plan F2H69 Bloque C original incluía opt-in `splitByNode` flag para preservar cada node con mesh como `SubMesh` independiente (permitiendo, p. ej., rotar las ruedas del DeLorean separadas del chassis). Durante implementación, decidido consolidation only (todos los nodos en 1 MeshAsset con vertices pre-transformados, AABB local consolidado).

**Decisión:** En `.gltf` / `.glb`, aplicar las node transforms acumuladas root→owner al vertex data (position + normal) + recalcular AABB local. FBX intacto. Sin split-by-node. Split real diferido a F2H70 si emerge demanda concreta (ruedas rotantes visuales independientes).

**Razones:**
- Consolidation alcanza el goal visual unitario del Bloque D (1 DeLorean entero, sin necesidad de rotar wheels independientes — Jolt simula la rotación física sin sync visual; el dev acepta wheels estáticas hasta F2H70).
- Split-by-node implica refactor en cascada: `SceneSerializer` (¿`<gltf_path>#<node_name>` como reference? schema bump del moodmap?), `MeshRendererComponent` (¿múltiples mesh_ids o un MeshAsset con array de SubMeshes nombrados consumidos por filter?), `VehicleSystem` (cómo encuentra los wheel meshes para sincronizar — naming convention canónica). Hito propio.
- Pattern hereda lo que motores grandes hacen al import default (Unity glTF importer "Combine Meshes" toggle, Unreal "Combine Meshes" en glTF/FBX import options) — el split es opt-in, no el path principal.

**Alternativas descartadas:**
- Implementar split desde F2H69: scope blow-up; el hito ya cubría 5 bloques (A-E) con un asset swap visible al usuario.
- Skip consolidation también (cargar tal cual y dejar al renderer aplicar matrices): rompería `MeshRendererComponent` que asume vertices ya en world del mesh. Refactor mayor del render path.

### Decisión 4 — `configPath: ""` → fallback automático a `makeDefaultSA()` (drop del `.moodvehicle` redundante)

**Contexto:** F2H67 + F2H68 mantenían `assets/vehicles/banshee_sa/banshee_sa.moodvehicle` como archivo JSON con el tuning SA-style. Pero `AssetManager::loadVehicleConfig` ya tenía fallback al slot 0 (default SA) cuando el path no existe o está vacío. El JSON externo era redundante con `VehicleConfig::makeDefaultSA()` en código (que define los mismos valores).

**Decisión:** Drop del `.moodvehicle` en `vehicle_demo.moodmap` (`configPath: ""`). El VehicleSystem usa `makeDefaultSA()` directo. La estructura para `.moodvehicle` queda disponible en `AssetManager_Vehicle.cpp` para overrides futuros (otro vehículo con tuning distinto: crear un `.moodvehicle` y referenciarlo), pero el default no necesita archivo aparte.

**Razones:**
- Fuente única de verdad para el tuning default: el código. Editar dos archivos para cambiar el default era un trap.
- `configPath: ""` es semánticamente claro: "usá el default que ya conocés". No requiere recordar el path del `.moodvehicle`.
- Reduce surface area: 1 archivo menos en el repo, 1 archivo menos que mantener consistente con `makeDefaultSA()`.

**Alternativas descartadas:**
- Mantener `.moodvehicle` solo para documentación: comments en `VehicleConfig.cpp` cumplen el rol sin file extra.
- Hacer el `.moodvehicle` obligatorio: rompe drop-in (cada nuevo vehículo necesitaría ambos archivos al spawn).

**Revisar si:**
- Llega un caso con tuning per-vehículo no derivable de `makeDefaultSA()` (p. ej. una moto con `wheels=2`): crear `.moodvehicle` para ese vehículo y referenciarlo. El default sigue siendo `""` para los autos comunes.

### Decisión 5 — DeLorean GLB procesado headless con scripts `pygltflib` (no Blender)

**Contexto:** El modelo descargado por el dev venía con tres problemas estándar de assets Sketchfab/CGTrader exportados desde 3DS Max: escala incorrecta (2.27 m vs DeLorean real 4.22 m), mirror matrices baked en 7 de 17 nodos (`det(M) < 0` causa render con caras "hacia adentro" al pasar por backface culling), origin en la base del modelo (Y=0 = ruedas). Workflow tradicional: abrir Blender → escalar/Apply Transforms/centrar/re-export.

**Decisión:** Procesar todo headless con scripts Python `pygltflib`, sin requerir Blender. Scripts persisten en `c:/tmp/` (no en repo) para reuso con futuros assets.

**Razones:**
- Repetibilidad: el flujo es scripted, no manual. Otros assets con mismos problemas se procesan con `python scale_glb.py modelo.glb x1.5` (etc.).
- Sin dependencia de DCC tool: el dev no necesita tener Blender instalado o conocer su UI.
- Determinismo: el script siempre produce el mismo output dado el mismo input. Blender Apply Transforms tiene variaciones por configuración de unidades / convención de ejes.
- Velocidad: el ciclo "edit → verify dimensions → reapply" toma 30s en Python vs 2-3 minutos en Blender.

**Alternativas descartadas:**
- Blender CLI (`blender --background --python script.py`): requiere Blender 4.x instalado + script con bpy API. Más pesado.
- `gltf-transform` npm CLI: tampoco genérico para "flip winding cuando det<0"; habría que extenderlo con plugin.
- Bake en el motor (MeshLoader detect det<0 + flip): considerado follow-up — corregir el asset es one-time, corregir el motor es defensivo + benefit a todos los assets futuros con mismo problema. Diferido a F2H70.

**Revisar si:**
- Los scripts se vuelven inestables o frágiles con la siguiente versión de `pygltflib`: portar a `gltf-transform` o `assimp` Python bindings.

### Decisión 6 — 3 bugs sistémicos del VehicleSystem diferidos a F2H70

**Contexto:** Validación visual del DeLorean reveló tres bugs del engine (no del asset): (1) spawn flotando si `position.y < halfHeight del modelo`, (2) modelo rota distinto al chassis (gimbal lock por `extractEulerAngleXYZ` en el sync post-step), (3) ruedas no rotan visualmente con la simulación física. Workarounds asset-specific aplicados al moodmap para "que ande esta noche": `position.y=0.568`, `rotationEuler=[0,180,0]`, wheels estáticas aceptadas.

**Decisión:** Cerrar F2H69 con esos workarounds documentados pero no atacar los bugs en este hito. Plan F2H70 propio para los tres fixes.

**Razones:**
- El dev explicitó durante la sesión: *"tenemos que hacer que esto funcione para cualquier auto que se integre no podemos estar arreglando valores arbitrarios para un simple delorean"*. Principio operacional: el engine absorbe las variaciones del asset, no el dev en el moodmap. Memoria `feedback-vehicle-sistemico` capturó la regla.
- Los tres bugs comparten componente (`VehicleSystem.cpp` + integración con `TransformComponent`) y conviene atacarlos juntos para evitar parches incrementales que conflicten.
- Scope alcanzable en hito propio: split-by-node solo es ~1-2 sesiones. Auto-spawn-height + quat sync requieren leer el AABB del MeshAsset desde el VehicleSystem (decisión arquitectónica sobre acceso del system layer al asset layer — discutible).
- F2H69 ya cubre 5 bloques (A-E) con un asset visible al usuario. Apilar 3 fixes más es scope blow-up tras una sesión larga.

**Alternativas descartadas:**
- Atacar bug 2 (quat sync) en F2H69 porque es el más visible: parche aislado sin context del bug 1 (los tres están enlazados por cómo VehicleSystem inicializa+sincroniza el chassis con el Transform). Apilados conviene.
- Aceptar workarounds permanentemente: viola el principio que el dev explicitó. Confunde a quien lea el moodmap (¿por qué position.y=0.568 y no 0? — la respuesta correcta es "no debería ser 0.568, hay un bug").

**Revisar si:**
- En F2H70 emerge que alguno de los 3 bugs es más complicado de lo estimado y conviene re-scopear.

---

## 2026-05-19: F2H68 — Auto-ragdoll por impacto (infra completa, sample con bug conocido)

**Contexto:** El dev preguntó al cerrar F2H67: *"si un vehiculo, si choca un NPC con trigger ragdoll, este caera o sentira el impacto?"*. Verificado que NO funcionaba (activación de ragdoll era 100% manual). Este hito monta la infra completa siguiendo standard industry. Tag `v1.55.0-fase2-hito68`. Detalle en [`hitos/F2H68.md`](hitos/F2H68.md).

**Decisión clave 1 — Standard industry: `ContactListener` + `Sensor body` (no inventar pattern propio).**

- **Approach:** `physics_internal::ContactListener` derivado de `JPH::ContactListener` registrado en `PhysicsWorld` via `SetContactListener`. Detecta contactos físicos reales (no overlaps de sensor virtual como `TriggerVolumeComponent`). Para NPCs hitbox sin rebote, se usa `mIsSensor=true` en `BodyCreationSettings` (Unity `Collider.isTrigger` / Unreal `CollisionResponseChannel::Overlap`).
- **Razón:** El dev pidió textualmente *"enserio no quiero que reinventemos la RUEDA, osea reusemos conceptos que existan en internet"* tras un intento previo de "proxy hitbox Kinematic + destroy on ragdoll spawn" ad-hoc. ContactListener es el pattern documentado en Jolt / PhysX / Box2D / Bullet; sensor body es el pattern Unity/Unreal/Source de hace 15 años.
- **Documentado en memoria** del agente como `feedback_no_reinventar_rueda.md`: regla permanente para el resto del proyecto.

**Decisión clave 2 — Mapa `BodyID → entt::entity` mantenido por los sistemas dueños, NO auto-registrado por `createBody`.**

- **Approach:** `PhysicsWorld::Impl::bodyToEntity` (raw `u32 → u32`). API pública `registerBodyEntity` / `unregisterBodyEntity` / `entityOfBody`. `createBody` queda agnostic a ECS — los callers que conocen la entity (PhysicsSystem para RigidBody, VehicleSystem para chassis, RagdollSystem para parts) registran tras crear. Cleanup automático en `destroyBody/Vehicle/Ragdoll`.
- **Razón:** `PhysicsWorld` vive en capa `engine/physics/`, NO debe conocer EnTT. Si `createBody` auto-registrara, requeriría passar el handle como parámetro adicional (cae al mismo problema) o crear acoplamiento físico→ECS. Mantener PhysicsWorld puro permite tests headless sin Scene.

**Decisión clave 3 — Cola deferred + mutex para mutar fuera del callback (regla Jolt documentada).**

- **Approach:** `OnContactAdded` SOLO encola `ImpactEvent { victimBodyId, impulseWorld, impactSpeed }` en `Impl::impactQueue` bajo `std::mutex`. `RagdollSystem::tick` drena la cola pre-materialize via `drainImpactEvents()` (swap-out atómico).
- **Razón:** Jolt prohíbe explícitamente mutar bodies / crear/destruir entities / cambiar shapes dentro del callback (puede correr en threads del `JobSystem` con substeps paralelos). Patrón "command queue" estándar de game engines (similar a `Unity.Mathematics.Job` con barriers, `Bevy ECS` events).

**Decisión clave 4 — Tuning: `closingSpeed >= 4 m/s` umbral + `impulseFactor = 0.3` arcade.**

- **Approach:** Sin masa (físicamente exacto manda volar absurdo). `impulseMag = closingSpeed * impactImpulseFactor`. Defaults expuestos en API `setRagdollImpactSpeedThreshold/Factor`.
- **Razón:** `4 m/s` evita falsos disparos por roce (caminata = 5.5 m/s, OK; gravity-fall slow = no triggea). `0.3` da feel GTA SA tras tests subjetivos. Configurable runtime para tunear sin recompilar.

**Decisión clave 5 — Banshee tuning derivado de docs públicos `handling.cfg` de GTA SA (no inventar números).**

- **Valores derivados**: `maxTorque 500→800 Nm`, `brakeTorque 1500→4500 Nm` (3× motor, frenado snappy), `handbrakeTorque 4000→6000 Nm`.
- **Razón:** Aplicación de la regla "no reinventar". El `handling.cfg` de SA está documentado en wikis/foros — los valores reales (Mass 1700, BrakeBias 50/50, all-wheel-drive) dan el feel que el dev pidió.

**Decisión clave 6 — Revert del sub-mesh selector per-wheel; adoptado "1 entity por mesh-part" estándar Unity/Unreal/GTA.**

- **Approach inicial F2H67:** `MeshRendererComponent::subMeshName` permitía que 5 entities compartieran 1 FBX (chassis "body" + 4 "wheel-*"). Render filtraba sub-meshes por nombre.
- **Bug emergente:** Kenney `sedan.fbx` exporta sub-meshes con vértices "baked" en world del modelo (wheel-front-left tiene vértices en `(-0.85, 0.3, 1.5)` en vez de centrados en origen). Cuando el `VehicleSystem` sync-ea cada wheel-entity a su TF del physics, las wheels aparecen lejos del chassis.
- **F2H68 intento polish:** calcular `SubMesh::pivotOffset` (centro AABB del sub-mesh) y aplicar `model * translate(-pivotOffset)` en el render. Funcionaba para wheels pero rompía el chassis "body" (re-centraba un mesh ya posicionado correctamente).
- **Decisión final:** REVERT. Mantener `subMeshName` field como filter cosmético, pero abandonar el caso de uso "vehicle assembly via sub-meshes". **Standard industry**: 1 GameObject/Actor por mesh-part (Unity, Unreal, GTA, todos lo hacen así). El demo F2H68 usa 1 entity por vehículo. Las wheels visuales no rotan independientemente — limitación aceptada hasta F2H69 (pipeline glTF multi-node que separa nodes en `MeshAsset`s independientes al cargar).

**Bug conocido — NPC sample no transiciona end-to-end.**

La infra tiene 6 tests unit verde, pero el integration sample (atropellar al NPC con el Banshee) NO dispara el ragdoll. Documentado como Bug Conocido en [F2H68.md](hitos/F2H68.md) con hipótesis priorizadas a debugar en F2H69.

---

## 2026-05-19: F2H67 — Vehicle physics estilo GTA San Andreas

**Contexto:** cerrar plan original F2H25 (Vehicle physics) dentro de Sub-fase 2.4. El dev pidió específicamente "autos estilo GTA San Andreas — se manejan fácil, físicas más que suficiente". Cita: *"me gustan los autos de gta san andreas, se manejan facil y tienen fisicas mas que suficiente"*. Tag `v1.54.0-fase2-hito67`. Detalle en [`hitos/F2H67.md`](hitos/F2H67.md).

**Decisión clave 1 — Backend `JPH::WheeledVehicleController` nativo (no rolling-our-own).**

- **Approach:** Wrapper sobre `JPH::VehicleConstraint` + `WheeledVehicleControllerSettings` con engine + transmission + differentials + per-wheel suspension + tire friction curves.
- **Razón:** Jolt expone API completa y testeada para vehículos arcade/realistic. Reimplementarlo desde joints + raycasts sería 3-4x más LOC con bugs sutiles de stability (suspension oscilating, lateral grip blowups, etc.). El controller también maneja el auto-shift por RPM.
- **Alternativa descartada:** Ensamblar con `HingeConstraint` por wheel + raycasts manuales. Funcional pero pierdes el auto-shift + friction curves + slip ratio que ya están en `WheeledVehicleController`.
- **Cost:** Acoplamiento a la API de Jolt. Si en el futuro hay que swapear physics engine, este wrapper es lo único a re-escribir.

**Decisión clave 2 — Defaults SA-style baked en `makeDefaultSA()`.**

- **Approach:** Función pura que construye un `VehicleConfig` con números calibrados para feel arcade: tracción alta (lateral 1.4, longitudinal 1.6), CoM bajo (Y -0.20m → no flipea), motor responsivo (500 Nm @ 4000 RPM, 5 marchas, final drive 3.42), brakes fuertes (1500 Nm regular, 4000 Nm handbrake), steering 35° con lerp 4.0/s, 4WD por default.
- **Razón:** GTA San Andreas tiene física arcade-ish: no es simulación realista, pero responde a inputs de gameplay (acelera fuerte, no patina sin handbrake, brakes potentes, doblar es responsivo, no flipea por golpes laterales). Estos números reproducen ese feel sin requerir tuning del dev. `.moodvehicle` JSON con campos opcionales permite override per-auto si emerge.
- **4WD por default** sobre RWD/FWD: más estable para arcade (menos spin con throttle full + steering). Reduce frustración con autos que patinen en cada cruce.

**Decisión clave 3 — Ensamblaje multi-entity (1 chassis + 4 wheels children).**

- **Approach:** Una entity con `VehicleComponent` representa el chasis. Las 4 wheels son child entities separadas con tags fijos `Wheel_FL/FR/RL/RR`. El `VehicleSystem` las busca por tag al primer materialize y cachea los handles en `VehicleComponent.wheelEntities[4]`. Cada frame, escribe la Transform de cada child con la wheel pose post-suspension.
- **Razón:** Permite swappear meshes de wheels independientes en el editor (ej. wheels de carrera para un modelo, off-road para otro) sin tocar physics ni recompilar config. Las child entities tienen `MeshRendererComponent` propio. Si el dev borra una wheel entity, el log warn pero el physics sigue (no crashea).
- **Alternativa descartada:** Skinned mesh único con bones `chassis/wheel_*` y un Animator que rote las wheel bones. Más complejo (requiere import pipeline + bone mapping), menos flexible.

**Decisión clave 4 — Sub-mesh selector en `MeshRendererComponent`.**

- **Approach:** Nuevo campo `std::string subMeshName` opcional en `MeshRendererComponent` + `std::string name` en `SubMesh` poblado desde `aiMesh->mName` por MeshLoader. Render path en `SceneRenderer_Render.cpp` skipea sub-meshes cuyo `name != subMeshName` cuando el filtro está seteado. `RenderBatching` excluye estas entities del path instanced (van a `drawMeshRenderer` no-batched).
- **Razón:** El Kenney Car Kit (CC0, único modelo de auto encontrado con partes separadas accesible) trae el FBX con 5 sub-meshes nombrados (`body`, `wheel-front-left`, etc.). Sin sub-mesh selector, la única opción era split a 5 archivos FBX en Blender (workflow extra para el dev cada vez que importa un modelo). El selector permite que 5 entities compartan UN solo MeshAsset, cada una renderizando su parte. Beneficio futuro: kitbashing modular en arquitectura/props (un mesh con varias partes nombradas → varias entities que cada una pinta su parte).
- **Alternativa descartada:** Auto-spawn de 5 child meshes al cargar el FBX (cada sub-mesh = mesh asset separado). Más complejo para el AssetManager + entidades fantasma en el catalog que confunden al dev. El selector es opt-in y no afecta nada si está vacío.
- **Trade-off:** Las entities con sub-mesh filter no van al path instanced. Para un auto (1 chassis + 4 wheels), son 5 draws no-batched por frame por vehicle — aceptable (pocos vehicles simultáneos en juego típico). Si emerge un caso de "100 props modulares con sub-mesh filter", agendar instancing-aware del filter.

**Decisión clave 5 — Mount/dismount con tecla F + radio 3m.**

- **Approach:** Flanco up→down de F detectado en `EditorPlayMode::updatePlayer`. Si on-foot, busca entities con `VehicleComponent` dentro de radio 3m del char position; primer match gana → mount. Si mounted, segundo F → dismount con teleport al costado del auto.
- **Razón:** Convención de FPS games (Half-Life, GTA). 3m radio es generoso pero no excesivo — el dev tiene que caminar al auto. Sin componente Trigger del engine: el detect es directo en C++ con `forEach<VehicleComponent, TransformComponent>` (rápido). Lua tampoco interviene — input dispatch a `VehicleComponent.input*` es C++ directo, evita la latencia de pasar por la VM cada frame.
- **Alternativa descartada:** Trigger volume custom alrededor del auto que dispara un Lua callback. Funcional pero más capas (engine → trigger → Lua → componente). El approach directo es más fácil de razonar.

**Decisión clave 6 — Chase cam reusa `FpsCamera` con position-override.**

- **Approach:** Cuando mounted, cada frame: `applyMouseMove` actualiza yaw/pitch de `m_playCamera` normalmente. Luego override de position: `chassisPos - forward() * 5m + (0, 1.5m, 0)`. El `forward()` del FpsCamera ya apunta a donde el dev mira → al teleportar la cam al "behind" del chassis, naturalmente mira hacia el chassis sin lookAt.
- **Razón:** Evita crear una `ChaseCam` class nueva con su propia matemática + duplicar el handling de input. Reusa el `FpsCamera` que ya tiene yaw/pitch + applyMouseMove. El override de position cada frame es 1 línea — no hay state extra. Mouse rota el orbit sin necesidad de "modos" en la cámara.
- **Gate adicional:** `EditorScene::updateRigidBodies` tiene un sync cam→char (Hito 30) que pisaba la chase cam con la pos del player. Agregué guard `m_playerMountedVehicleEntity == 0` para que el sync solo corra on-foot.

**Decisión clave 7 — Asset `.moodvehicle` JSON aditivo con slot 0 = `makeDefaultSA()`.**

- **Approach:** Nuevo asset type en AssetManager mirror de Dialog/Quest. Slot 0 lazy-init con `makeDefaultSA()`. `loadVehicleConfig(path)` parsea JSON con todos los campos opcionales (caen al default SA correspondiente). Si el archivo no parsea o `isValid()` rechaza, fallback a slot 0 + log warn.
- **Razón:** Permite tener N autos con tunings distintos sin duplicar código. El JSON es human-editable, no requiere recompilar. Slot 0 garantiza que `getVehicleConfig(0)` nunca null — el `VehicleSystem` puede pedir el default sin checks.
- **Sample shipado:** `assets/vehicles/banshee_sa/banshee_sa.moodvehicle` es una replica exacta de `makeDefaultSA()` — sirve como referencia del formato para el dev.

**Decisión clave 8 — Persistencia aditiva, runtime no persiste.**

- **Approach:** `VehicleComponent` aditivo en `.moodmap` v14. `configPath` persiste (que es lo único que define la identidad del vehicle entre sesiones). `vehicleId`, `wheelEntities[]`, `input*` NO persisten (runtime puro). Al cargar, `dirty=true` para que el `VehicleSystem` materialice. Mapas pre-F2H67 cargan sin el campo `vehicle` → entities sin `VehicleComponent`.
- **Razón:** Mismo patrón aditivo que F2H65/F2H66. Los handles entt + Jolt nunca son estables entre sesiones; intentar persistirlos sería bug-bait. Input no tiene sentido persistir (el script o el player lo escribe cada frame).
- **Gate extra del SceneSerializer:** Extendí el filtro `hasMr || hasLi || hasRb || ...` con `hasVeh` + reconocimiento de tags `Wheel_FL/FR/RL/RR`. Sin esto, las wheel entities placeholder se filtraban al guardar.

---

## 2026-05-18: F2H66 — Ragdolls auto-build sobre `JPH::Ragdoll` (Mixamo)

**Contexto:** continuar Sub-fase 2.4 (Física avanzada) inmediatamente después de F2H65. El dev quería flopping físico al matar a un NPC, estilo HL2 (cadáver vuela con impulse + sleeping nativo de Jolt cuando se queda quieto). Cierra plan original F2H24. Tag `v1.53.0-fase2-hito66`. Detalle en [`hitos/F2H66.md`](hitos/F2H66.md).

**Decisión clave 1 — Backend `JPH::Ragdoll` nativo, no rolling-our-own.**

- **Approach:** wrap sobre `JPH::Ragdoll` + `JPH::RagdollSettings`. Storage interno: `unordered_map<u32, { Ref<JPH::Ragdoll>, Ref<JPH::Skeleton> }>`.
- **Razón:** Jolt expone API completa y testeada para ragdolls: declarativo (Parts + constraints + collision groups), activación con `AddToPhysicsSystem`, lectura de poses con `GetBodyState`, estabilización con `Stabilize` (requiere `JPH::Skeleton` paralelo a `mParts`). Reinventarlo desde `HingeConstraint`/`SwingTwistConstraint` sueltos sería trabajo redundante + perderíamos la estabilización implícita de Jolt.
- **Alternativa descartada:** ensamblar el ragdoll manualmente desde la API de F2H65 (CreateHinge + CreatePoint por cada par). Funcional pero re-implementa lo que Jolt ya empaqueta y hay que mantener el bookkeeping de constraints + collision groups + activate-all-bodies-atomically.
- **Cost:** un `JPH::Skeleton` espejo del `RagdollLayout` se construye al `createRagdoll` y queda en memoria mientras el ragdoll exista. Trivial (~14 bones × ~50 bytes).

**Decisión clave 2 — Auto-build sobre convención Mixamo (`mixamorig:*`).**

- **Approach:** `ragdoll::buildMixamoLayout(skeleton, totalMass, limbRadius)` retorna `RagdollLayout` con 14 bones fijos. Mapping hardcoded: Hips → Spine/Spine1/Spine2 → Neck → Head; Spine2 → LeftArm/RightArm → LeftForeArm/RightForeArm; Hips → LeftUpLeg/RightUpLeg → LeftLeg/RightLeg. Wrist/Foot/Shoulder/fingers NO se ragdollean (heredan del parent en el sync).
- **Razón:** Mixamo es la convención canónica para humanoides en pipelines de gameplay (F2H49 ya importa Mixamo). El dev no quiere taggear bones a mano por NPC ("auto-build" = imperativo del scope). El layout hardcoded gana simplicidad + zero-config para el caso común; perder flexibilidad para esqueletos custom es aceptable.
- **Cap defensivo:** si el esqueleto NO tiene prefijo `mixamorig:` en ningún bone, retornamos `RagdollLayout{}` vacío + `Log::warn`. El `RagdollSystem` chequea `empty()` y revierte `state = Animated` sin crashear. Esto cubre el caso Fox.glb o esqueletos custom: el `enable` se vuelve no-op visible en logs.
- **Alternativa descartada:** layout configurable via JSON per-mesh. Más flexible pero exige UI nueva + serialización per-skeleton + el caso común (Mixamo) queda detrás de boilerplate. Diferido como follow-up si emerge presión.

**Decisión clave 3 — 14 bones, no 22+ (wrist/foot/shoulder/fingers fuera).**

- **Approach:** las extremidades terminales se montan sobre el parent ragdolleado (heredan su pose en el sync). El skeleton sigue teniendo 50+ bones; solo 14 tienen body físico.
- **Razón:** cada body extra = 1 rigid body en simulación + 1 constraint + 1 entry en collision filter. Para NPCs distantes la diferencia visual entre "mano floppea independiente" y "mano sigue al antebrazo rígidamente" es imperceptible. Reducimos costo CPU (~36% menos bodies) sin pérdida visual relevante.
- **Alternativa descartada:** full 22+ bones (manos + dedos + pies). Útil si el dev quiere close-ups con detalle de manos (third-person inspection); v1 NO lo necesita.
- **Cuándo revisar:** si emerge un caso de gameplay donde el detail de extremidades importa (cinematic, photo-mode con close-up). Agendable como override per-component (flag `ragdollFingers = true`).

**Decisión clave 4 — Mass distribution proporcional al volumen del capsule.**

- **Approach:** cada body recibe `mass[i] = totalMass · (volume[i] / Σvolume)`. Volumen del capsule = cilindro central + 2 hemisferios = `π·r²·(2·h) + (4/3)·π·r³`.
- **Razón:** más realista que `mass = totalMass / N`. El torso (capsule más grueso + largo) absorbe más impulse; la cabeza (capsule chico) se mueve "ligero" como debería. La estabilidad numérica de Jolt mejora cuando los ratios de masa entre bodies conectados quedan en rangos sanos.
- **Alternativa descartada:** masa fija configurable por bone (anatómicamente exacta: head ~5 kg, torso ~35 kg, etc.). Más realista pero exige tabla con magic numbers que invitan a desincronizarse con `limbRadius`. La proporcional auto-tracks cualquier ajuste de radius.

**Decisión clave 5 — Sin vuelta animación↔ragdoll en v1 (convención HL2).**

- **Approach:** una vez `state = Ragdolling`, queda así para siempre. `ragdoll.is_ragdolling("Tag")` permite a Lua chequear sin re-llamar `enable`. Reset requiere recargar el mapa.
- **Razón:** el return-from-ragdoll de calidad (NPC "se levanta") exige pose-matching (encontrar la keyframe del Animator más cercana al estado actual del ragdoll) + IK transition (blend continuo entre ragdoll y animator pose). Es scope de un hito propio. Implementarlo a medias produce snap-back desagradable.
- **Cuándo revisar:** si emerge gameplay donde NPCs noqueados se levantan (RPG con knockdown temporal). Agendable como F2H7x con plan dedicado.
- **Alternativa descartada:** mid-quality return (snap a la pose más cercana sin IK). Mejor no tenerlo que tenerlo mal.

**Decisión clave 6 — Disparo via Lua `ragdoll.enable(tag, {x,y,z})`, no via componente Trigger del engine.**

- **Approach:** Lua API en tabla global `ragdoll`. Lookup por TAG (no por handle entt). El componente solo expone el estado + impulse pending; la materialización física la hace `RagdollSystem` en el siguiente tick (lazy, mismo patrón `JointComponent.dirty` de F2H65).
- **Razón:** el control queda en la gameplay layer (script del NPC, sistema de combate). Mismo patrón que `inventory.give(tag, item, qty)` de F2H52 — consistencia API. El engine no decide CUÁNDO un NPC muere ni con qué impulse.
- **Trade-off:** primer match gana en lookup por tag (convención Hammer-style). Si hay 2 NPCs con tag `"Enemy"`, solo uno se ragdollea. Aceptable v1; si emerge dolor, el dev usa tags únicos.

**Decisión clave 7 — Frame-order fix: `AnimationSystem` skipea entities ragdolleando.**

- **Approach:** guard al inicio del lambda en `AnimationSystem::update`. Si `entity.hasComponent<RagdollComponent>() && state == Ragdolling`, return inmediato.
- **Razón:** **bug descubierto en validación runtime**. El frame loop es `AnimationSystem → tickPhysics (= step + RagdollSystem) → render`. Aunque pongamos `anim.playing = false` al transicionar a Ragdolling, el AnimationSystem **igual** evalúa la pose congelada en `t = último frame` y la escribe a `skel.skinningMatrices`, pisando lo que el `RagdollSystem` escribió. Resultado: ragdoll creado en Jolt correcto, pero el mesh visual seguía en idle ("ni se inmutaba").
- **Lección general:** `playing = false` significa "no avanzar el tiempo", NO "no escribir matrices". Esa distinción no era obvia leyendo el código del Animator (ambas lecturas son razonables). Cuando 2 sistemas escriben el mismo storage, el "más reciente del frame" gana — el bug es del orden, no del Animator.
- **Alternativa descartada:** reordenar sistemas (RagdollSystem post-Animation). Funcionaría pero el `tickPhysics` debe correr atómicamente (step + sync) y separarlos invita a otros bugs de ordering.

**Decisión clave 8 — Persistencia aditiva, `state` NO persiste.**

- **Approach:** `RagdollComponent` opcional en `.moodmap` v14 (sin schema bump). Persiste `totalMass`, `limbRadius`, `useGravity`, `spawnImpulse`. NO persiste `state` ni `ragdollId`. Al cargar, `state = Animated`.
- **Razón:** los ragdolls mid-flop no son save-able de forma significativa. Si el dev quiere "este NPC ya estaba muerto al cargar el mapa", la solución correcta es spawneal lo con `state = Ragdolling` desde Lua en `on_map_load`. Persistir `state = Ragdolling` exige también persistir las world poses de cada body (otherwise el ragdoll re-arranca en la bind pose). Scope inflación que no se justifica.
- **Trade-off:** si el dev `save && quit && load` mid-combate, los NPCs muertos resucitan vivos. Aceptable v1; el dev guarda PRE-evento si quiere preservar el estado.

**Decisión clave 9 — Split `PhysicsWorld.cpp` en 4 archivos antes de agregar API nueva.**

- **Approach:** core + Constraints (F2H65) + Ragdoll (F2H66) + Internal PIMPL header. `PhysicsWorld.cpp` bajó 850 → 611 LOC.
- **Razón:** misma regla soft 500 / hard 800 LOC que aplica para otros .cpp. `PhysicsWorld.cpp` ya estaba sobre el hard cap antes de F2H66; agregar la API ragdoll lo habría llevado a 1100+. Mejor hacer el refactor antes (commit "preparatorio" con 0 cambio funcional) que mezclar refactor + feature en el mismo commit.
- **Patrón usado:** mismo `EditorUI_*.inl`, `InspectorPanel_*.cpp`, `EditorRenderPass*.cpp`. PIMPL via header interno (`PhysicsWorld_Internal.h`) que centraliza el Jolt setup + storage shared entre los .cpp.

---

## 2026-05-18: F2H65 — Jolt constraints (Hinge / Distance / Point)

**Contexto:** abrir Sub-fase 2.4 (Física avanzada) del plan original. El dev quería *"que este sea un motor físico realista, como el source"*. Tag `v1.52.0-fase2-hito65`. Detalle en [`hitos/F2H65.md`](hitos/F2H65.md). 3 tipos de constraint en v1 (Hinge para puertas/brazos, Distance para cuerdas/varillas, Point para ball joints) — Slider y Fixed quedan para sub-hito si emergen necesidades de gameplay.

**Decisión clave 1 — Un solo `pivotLocal` por joint (en local de A), body B usa su origen.**

- **Approach:** `JointComponent.pivotLocal` único. El segundo pivot de Hinge/Distance se asume en `(0, 0, 0)` local de B, que en world es `entityB.Transform.position`.
- **Razón:** simplifica la UX (un DragFloat3 + un drop target en el Inspector). El caso "offset propio en B" se puede expresar moviendo el Transform de B; pocas situaciones reales necesitan más. Reduce el footprint del componente (de 6 floats a 3) y el modelo mental ("este es el punto donde se ancla A").
- **Alternativa descartada:** `pivotA` + `pivotB` por separado. Realista pero infla la UI y el caso common (puerta-marco) solo necesita uno.
- **Cuándo revisar:** si emerge un caso de gameplay real (ej. cable entre 2 puntos arbitrarios en bodies dinámicos sin reordenar Transforms), volver al diseño dual pivot.

**Decisión clave 2 — Target body B referenciado por TAG en persistencia.**

- **Approach:** `SavedJoint.targetTag : string` en el `.moodmap`. Al cargar, `SceneLoader::applyEntitiesToScene` hace una segunda pasada que resuelve `tag → entt::entity` y escribe el handle en `JointComponent.targetEntity`.
- **Razón:** los handles `entt::entity` no son estables entre sesiones (la generación cambia al crear/destruir). Mismo patrón paths-no-ids que ya usamos en `AnimatorComponent.externalClips`, `InventoryComponent.entries`, `ItemPickupComponent.itemPath`, etc.
- **Limitación conocida:** tags no son únicos (Hammer-style con `targetname` enforce-unique no se aplicó acá). En caso de colisión, el `forEach` retorna el primer match — log warn si emerge ambigüedad real.
- **Alternativa descartada:** UUIDs persistentes por entity. Más correcto pero exige un campo nuevo en TODOS los componentes/SavedEntities — refactor cross-cutting que no se justifica para este hito.

**Decisión clave 3 — Sentinel `kJointNoTarget = UINT32_MAX` (== `static_cast<u32>(entt::null)`), no `0`.**

- **Approach:** `constexpr u32 kJointNoTarget = ~u32{0}` en `Components.h` junto a la struct. Default de `JointComponent.targetEntity` + check `== kJointNoTarget` para "sin asignar".
- **Razón:** **bug de diseño descubierto en Bloque E**. El intento inicial usó `targetEntity == 0` como sentinel; el primer test de round-trip falló porque `frame` (creada PRIMERO en la scene fresca) tiene raw `entt::entity{0}` → `static_cast<u32>(handle) == 0` → colisión con el sentinel. El check `targetEntity != 0` filtraba el target legítimo.
- **Alternativa descartada:** cambiar `targetEntity` de `u32` a `entt::entity` directamente (donde `entt::null` es sentinel natural). Hubiera contaminado headers de componentes con `entt::entt.hpp` y exigido cast en cada `static_cast<u32>(handle)` del Inspector/serializer.
- **Generalización:** **regla para futuros sentinels que viajen como `u32`**: usar `UINT32_MAX` o un valor obviamente fuera del rango legítimo. El `0` es válido para entt y conviene reservarlo.

**Decisión clave 4 — Drag-drop entity payload `MOOD_ENTITY` reusable.**

- **Approach:** `HierarchyPanel` emite un drag source por cada row con payload `MOOD_ENTITY` cargando `entt::entity` raw (4 bytes). `InspectorPanel_Joint` lo acepta vía `BeginDragDropTarget` + `AcceptDragDropPayload("MOOD_ENTITY")` + `memcpy` con validación de `DataSize`. Self-link bloqueado en el handler.
- **Razón:** la convención `MOOD_<TIPO>_ASSET` ya existía para texture/mesh/material/script/item/prefab; faltaba el equivalente para entities. Cualquier componente futuro que linkee entities entre sí (parent-child, follow-target, AI attention) puede consumir el mismo payload sin tocar HierarchyPanel.
- **Trade-off:** el payload lleva el RAW handle, no la tag. El consumer hace lookup vía `Scene::entityFromHandle` (que es lo correcto para uso interno mientras la scene esté viva). La persistencia es responsabilidad del componente (en F2H65 = serializer resuelve handle → tag al guardar).

**Decisión clave 5 — Tag resolution en 2 pasadas (eager + post-load batch).**

- **Approach:** `SceneLoader::applyOneEntity` hace lookup inmediato con la scene parcialmente poblada (suficiente para undo de delete, donde B ya existe). `applyEntitiesToScene` hace una **segunda pasada** después del loop principal: itera entities con joint pendiente y resuelve el tag con todas las entities ya creadas.
- **Razón:** el primer pase puede crear el owner del joint ANTES que el target (orden del array en el `.moodmap`). Sin la segunda pasada, el owner quedaría con `targetEntity = kJointNoTarget` permanente hasta que el dev re-asigne via Inspector.
- **Alternativa descartada:** agregar un campo runtime `pendingTargetTag : string` al `JointComponent` y resolver lazily en el PhysicsSystem por tick. Funcional pero contamina el componente con state transient solo útil al cargar; la 2-pasadas es self-contained al loader.

**Decisión clave 6 — Debug overlay solo bajo F1 (no toggle independiente).**

- **Approach:** el `forEach<JointComponent, TransformComponent>` que dibuja anchor + línea + flecha vive dentro del bloque `if (m_debugDraw)` del overlay 3D, junto a las AABBs de tiles + OBBs de triggers + paths de NavAgents.
- **Razón:** alinear con el patrón existente. Toggle único para "ver el cableado físico/lógico de la escena" en lugar de N toggles independientes que el dev tendría que recordar. Si emerge necesidad de granularidad (ej. "solo joints, no triggers"), agendar polish.
- **Colores por tipo** (Hinge azul, Distance verde lima, Point magenta): convención visual que también puede aplicarse al Inspector header si el dev pide consistencia cross-panel en el futuro.

---

## 2026-05-18: F2H64 — OIT Weighted Blended + sombras tintadas

**Contexto:** completar la línea de transparencia abierta en F2H63 (split de scope decidido en 2026-05-17). F2H63 entregó base visual; F2H64 entrega correctness: no flicker apilando translúcidos + vidrios proyectan sombras tintadas. Tag `v1.51.0-fase2-hito64`. Detalle en [`hitos/F2H64.md`](hitos/F2H64.md). El dev cerró con *"todo se ve ok"* tras tour visual (incluyó screenshot de cubo rojo + cubo verde con sombras del mismo color sobre suelo blanco: *"de 10!"*).

**Decisión clave 1 — OIT Weighted Blended (McGuire & Bavoil 2013) reemplaza el sort de F2H63.**

- **Approach:** 2 attachments nuevos (`accumColor` RGBA16F + `revealage` R16F) en un FB separado `m_oitAccumFb`, depth compartido con el scene FB. Shader emite `(rgb * α, α) * weight` al accum y `α` al revealage. Composite final divide `accum.rgb / accum.a` y mezcla con `glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA)`.
- **Razón:** una sola forma de hacer las cosas. F2H63 sortaba back-to-front con tiebreaker por entity ID, suficiente para casos simples pero con flicker garantizado cuando 2 vidrios estaban a casi misma distancia. OIT es order-independent por construcción — el orden de iteración del registry de entt ya no importa.
- **Alternativas descartadas:** mantener sort como fallback opcional → 2 paths de render que envejecen mal. Depth-peeling → más correcto pero N passes per N layers (cuello CPU/GPU); WBOIT es el sweet spot de calidad / costo.
- **Limitación:** el weight es heurístico (McGuire); en escenas con rangos de profundidad extremos puede haber objetos cercanos que dominen demasiado. La fórmula del paper funciona bien para la mayoría; se ajusta si emerge artefacto concreto.

**Decisión clave 2 — Uniform branching `uOitPass` sobre compilar 2 variantes del shader.**

- **Approach:** un solo `pbr.frag` con `uniform int uOitPass`. Default 0 (path forward F2H63). Cuando 1, el branch translucent emite a accum/revealage en lugar de FragColor.
- **Razón:** simplicidad runtime. La rama OIT solo se ejecuta en translucents (que son pocos), el costo del branch dinámico es despreciable. Compilar 2 variantes hubiera duplicado los OpenGLShader instances + el ShaderGraphCache + el path skinned + el path instanced. Trade-off claro a favor de simplicidad para este caso.
- **Cuándo revisar:** si profileo el frame y aparece como cuello el branch en GPUs viejas, se puede compilar dual con `#ifdef` controlado por defines del shader graph cache.

**Decisión clave 3 — FB OIT comparte depth con el scene FB (no propio).**

- **Approach:** `OpenGLOitFramebuffer` recibe un `GLuint sharedDepthTextureId` externo y lo attachea via `glFramebufferTexture2D` durante el `invalidate()`/`resize()`. El caller (SceneRenderer) es dueño del depth.
- **Razón:** depth-test contra opacos funciona automáticamente (los opacos ya escribieron su depth en el scene FB). Sin esto habría que hacer blit o resolve del depth, agregando cost por frame.
- **Trade-off:** acoplamiento al lifecycle del scene FB. Si el scene FB recrea su depth en un resize, el OIT FB tiene que re-attachear el id actualizado. Resuelto con `ensureOitFb(w, h)` idempotente que re-attachea en cada resize.

**Decisión clave 4 — Shadow atlas RGB con sub-pase tinted separado del opaque.**

- **Approach:** `OpenGLShadowMapArray` extendida con un color attachment RGBA8 array paralelo a la depth array. `ShadowPass::recordCsm` hace 2 sub-pases por cascada: opaco (depth-only, filtra Translucent) + tinted (depth-write OFF, blend `ZERO/SRC_COLOR` multiplicativo, escribe `albedoTint * (1 - opacity)`).
- **Razón fisica:** vidrios no ocluyen luz, la tintan. Si el opaque pass escribiera depth para Translucent, la luz quedaría bloqueada y el tinte sería sombra negra coloreada (no realista). Filtrar Translucent del opaque + agregarlos al tinted da el resultado correcto: luz pasa atenuada y coloreada.
- **Trade-off:** cada Translucent con `castTranslucentShadow=true` agrega un draw call por cascada al shadow pass. Para 50+ vidrios + 4 cascadas se nota. Mitigación: opt-out per material via checkbox.

**Decisión clave 5 — Default ON para `castTranslucentShadow` (opt-out, no opt-in).**

- **Approach:** `MaterialAsset.castTranslucentShadow = true` por default. Inspector muestra checkbox "Proyectar sombra tintada" en Translucent (disabled en Opaque/Additive).
- **Razón:** el look realista debe ser automático. El dev pone un material Translucent y "funciona": sombra tintada sin tener que recordar prender un checkbox. El escape hatch existe para escenas con muchos vidrios donde la perf manda.
- **Persistencia aditiva:** solo se escribe `cast_translucent_shadow` al `.moodmat` cuando `blendMode == Translucent && castTranslucentShadow == false`. Los `.moodmat` históricos no se contaminan con la key nueva.

**Decisión clave 6 — OIT compatible con shader graph desde día 1.**

- **Approach:** el template `pbr_graph_template.frag` también emite ambos paths controlados por `uOitPass`. Los samples shipados (water/glass/hologram) funcionan sin migración.
- **Razón:** si no lo hacíamos ahora, los samples que el dev validó hoy (transparencia) iban a flickearse al apilar. No tendría sentido cerrar F2H64 dejando el caso "shader graph + translúcido" roto.

**Decisión clave 7 — Composite pass escribe solo a loc 0 del scene FB.**

- **Approach:** `glDrawBuffers(1, { GL_COLOR_ATTACHMENT0 })` antes del fullscreen tri del composite. Restaurar a `(0, 1)` después.
- **Razón:** el scene FB es MRT (color + normal para SSR). El composite no debe pisar las normales de los opacos en loc 1, sino el SSR vería "pixels sin normal PBR" donde hay translucents → no se reflejaría correctamente.

**Decisión clave 8 — Brushes translúcidos quedan agendados (no entran a F2H64).**

- **Contexto:** durante validación el dev preguntó por brushes con material Translucent. Confirmamos: `RenderBatching::groupByBatch` solo itera MeshRenderer; los brushes van por el `brushPass` en medio del opaque path. Con material Translucent hoy se ven mal.
- **Decisión:** documentar como limitación + agendar como **F2H65 — Brushes translúcidos OIT** (~2-3h). Razón: no inflar F2H64 con scope adicional. La base actual ya cierra el caso 95%.
- **Workaround mientras tanto:** convertir brush a mesh (Hammer-style "make detail").

**Tests post-F2H64:** suite global **975/9927 verde** (971 previos F2H63 + 4 nuevos: cast_translucent_shadow roundtrip + back-compat). Test del sort F2H63 eliminado (1 case neto +3).

---

## 2026-05-17: F2H63 — Transparencia base (alpha + refracción screen-space)

**Contexto:** pedido del dev al cierre F2H62: *"un motor grafico debe tener transparentes"*. Discusión de scope arrancó con un approach minimal (BlendMode + sort back-to-front), pero el dev pidió *"transparencia o traslucido realista, nada de simulaciones que no tengan sentido, esto es un motor realista"*. Mega-hito de ~30h (OIT + refracción + sombras translúcidas + UI + Shader Graph extensión) era riesgo de regresión. Tag `v1.50.0-fase2-hito63`. Detalle en [`hitos/F2H63.md`](hitos/F2H63.md).

**Decisión clave 1 — Split en F2H63 (base visual) + F2H64 (correctness multi-vidrio).**

- **F2H63 entrega:** BlendMode (Opaque/Translucent/Additive), opacity + IOR + refractionStrength en MaterialAsset, refracción screen-space del backbuffer copy, Fresnel-driven auto-opacity, UI Inspector Blending, Shader Graph 6º input Opacity, 3 samples (water/glass/hologram), tests.
- **F2H64 entregará:** Weighted Blended OIT (McGuire/Bavoil 2013), sombras translúcidas (extender shadow atlas a RGB), opcionalmente refracción full Snell.
- **Razón:** validar la base visual con el dev antes de moverse a correctness con muchos translúcidos solapados. La base ya cubre el caso 95% (escenas con <50 vidrios no-solapados).
- **Cuándo revisar el split:** si emerge una demo con muchos vidrios intersectados o follaje denso que flickerea, priorizar F2H64.

**Decisión clave 2 — Screen-space distortion sobre full Snell refraction.**

- **Approach:** sample del backbuffer copy con offset `nViewXY * (uIor - 1) * uRefractionStrength * 0.1`. Convención Unity URP / Unreal "Distortion".
- **Razón:** resultado visual indistinguible para el ojo en geometría plana / convexa simple; ~zero costo extra (1 texture sample por fragment). Trazar el rayo a través del material según grosor estimado es ~3-5x más caro y solo se nota en geometría compleja transparente (cristales tallados, agua con curvatura fuerte).
- **Limitación documentada:** borde de pantalla puede dar coords fuera del backbuffer copy. Mitigado con `clamp(uv, 0.001, 0.999)`. Mejora futura: fade en bordes — solo si emerge demanda.

**Decisión clave 3 — Fresnel-driven auto-opacity integrado, no opt-in.**

- **Approach:** shader hardcoded `fresnel = pow(1 - NdotV, 5)` + `effectiveOpacity = mix(uOpacity, 1, fresnel * 0.5)` para todos los Translucent. Bordes más opacos, frente más transparente.
- **Razón:** look "vidrio real" sin que el dev tenga que armar el grafo Fresnel cada vez. Si el dev quiere opacidad uniforme, baja Opacity al valor target — el factor 0.5 hace el efecto sutil.
- **Bug cazado validando:** los samples originales (glass + hologram) conectaban un Fresnel node al input Opacity del OutputPBR. Resultado: face-on → opacity=0 → cubo invisible. Fix: literal opacity en los samples (el Fresnel hardcoded del shader ya hace su parte).

**Decisión clave 4 — Translucent pass antes de SSAO/SSR/bloom.**

- **Approach:** orden de frame: opaque → skybox → brushes → translucent → SSAO → SSR → bloom → particles.
- **Razón:** los translúcidos LEEN del backbuffer copy (color de los opacos). Los screen-space passes posteriores no afectan al copy (ya hecho). Translúcidos NO contribuyen a SSAO/SSR (leen depth attachment del pase opaco — convención estándar Unity URP / Unreal, aceptable v1).
- **Trade-off conocido:** un vidrio no contribuye a su propio AO ni se refleja en SSR. Para mejorarlo habría que hacer un re-render del depth con translúcidos antes de SSAO/SSR — costo significativo, sin demanda concreta.

**Decisión clave 5 — Backbuffer copy 1x por frame, no per-translucent.**

- **Approach:** blit del scene FBO al `m_backbufferCopyFb` (RGBA16F preserva HDR) UNA vez antes del translucent pass. Todos los vidrios sortean back-to-front contra el mismo snapshot.
- **Razón:** sort estable + 1 blit + N draws es ~10-20x más barato que N blits + N draws. Para refracción multi-layer correcta (vidrio detrás de vidrio refractando el segundo correctamente) habría que blittear per-translucent. Aceptable v1.
- **Limitación conocida:** dos vidrios alineados refractan los OPACOS de atrás correctamente, pero el más cercano NO refracta al más lejano (el copy fue tomado antes de los translucents). Mitigación futura: re-blit si emerge necesidad — agendado a F2H64 si el OIT necesita el mismo workaround.

**Decisión clave 6 — Persistir blending solo si `blendMode != Opaque`.**

- **Approach:** `AssetManager_Material::save` omite las keys `blend_mode`/`opacity`/`ior`/`refraction_strength` si el material es Opaque.
- **Razón:** los `.moodmat` históricos no tenían estos campos. Si los persistiéramos siempre, abriríamos un material viejo en el editor → Save → diff "fantasma" de 4 keys nuevas. Solo se contamina el JSON cuando el dev decide pasar a Translucent/Additive.
- **Trade-off:** un material que el dev pasa a Translucent y vuelve a Opaque sí deja las keys en disco. Es aceptable (raro flow, y la lectura sigue siendo correcta: keys ignoradas en Opaque).

**Decisión clave 7 — OutputPBR 6 inputs con back-compat literal `1.0`.**

- **Approach:** el `ShaderGraphAsset::createNode(OutputPBR)` arma 6 inputs siempre. Los shaders `.moodshader` viejos con 5 inputs cargan ok; el generator emite el literal `1.0` para el input Opacity faltante.
- **Razón:** evita migración masiva de assets. Los 3 samples shipados en F2H62 (water/gold/hologram) y futuros assets de usuarios siguen funcionando sin tocar disco.
- **Test regression:** `test_shader_graph.cpp` valida que un OutputPBR legacy compile sin error con opacity default.

---

## 2026-05-17: AUDIT-3 cierre — Tanda de audits completa

**Contexto:** tercer y último audit de la tanda inicial (~10h totales tras AUDIT-1 y AUDIT-2). El dev pidió cerrar la profesionalización del código antes de seguir con features. Tag `v1.49.3-audit-3`. Reporte completo en [`audits/AUDIT_3.md`](audits/AUDIT_3.md).

**Decisión clave 1 — Convención `_<Family>.inl` para inline bodies de clases grandes.**

- **Contexto:** `EditorUI.h` estaba en 836 LOC HARD-cap por ~30 pares `request/consume` con bodies inline (4-6 LOC c/u + comentarios). pImpl era invasivo (~1 día de refactor + riesgo de regresión en panels).
- **Solución:** declaraciones en el `.h`, inline bodies movidos a archivos `EditorUI_<Family>.inl` sibling (Spawn / Tools / Entity / Project), `#include`d al final del `.h` después del `} // namespace`. Mantiene API exacta (siguen siendo inline para el linker), zero impact runtime.
- **Resultado:** `EditorUI.h` 836 → 483 LOC (-42%). HARD cap = 0 en todo el codebase.
- **Convención formalizada:** `<Nombre>_<Family>.inl` para grupos de inline methods de una clase grande. Coexiste con los patterns previos: `<Nombre>_Internal.h` (helpers compartidos entre `.cpp` siblings) y `<Nombre>_<Family>.cpp` (split de implementaciones).
- **Cuándo aplicar:** clases que crezcan a 700+ LOC con muchos accessor/request inlines repetitivos. Para clases con lógica genuinamente compleja en el header, el split por categoría no aplica — pImpl o reorganización real.

**Decisión clave 2 — Primer pure helper en `core/math/Ray.h` como warmup DOD.**

- **Contexto:** AUDIT-2 Bloque D identificó `pickRayFromNdc` con 5+ ocurrencias inline del mismo patrón (NDC unprojection). AUDIT-3 lo extrajo como prueba del flow DOD completo.
- **API:** `unprojectNearFar(invVP, ndcX, ndcY)` devuelve near/far sin normalizar (caller decide); `pickRayFromNdc(invVP, ndcX, ndcY)` devuelve un `Ray { origin, direction }` con dir normalizada (caso 95%). Ambos retornan `std::optional` para clip.w=0 / dir cero.
- **Tests:** 7 cases headless cubren perspective / ortho / esquinas / NDC arbitrarios / `invVP` degenerada. Suite global 949 → 956 tests verde.
- **Trade-off aceptado:** un helper extra requiere un include extra en 5 archivos (`#include "core/math/Ray.h"`) — costo trivial vs ~42 LOC de duplicación eliminada + testabilidad.
- **Próximos candidatos** (de AUDIT-2 Bloque D, pendientes): `aabbFromTwoPoints`, `snapWorldPositionToGrid`, `groupClickedFaceByBrush`. Extraer solo si emerge demanda — no urgente.

**Decisión clave 3 — Patrón `_helpers.h` para fixtures de test compartidas.**

- **Contexto:** split de `tests/test_scene_serializer.cpp` 934 LOC en 3 archivos por familia (core / lighting_physics / gameplay). Los 3 compartían `NullTexture` + `nullFactory()` + `tempPath()`.
- **Solución:** `tests/test_scene_serializer_helpers.h` con namespace dedicado (`Mood::SceneSerializerTests`), free functions `inline` para evitar ODR violations. Análogo del `_Internal.h` para sources.

**Decisión clave 4 — `engine/project/Workspace.h` + `core/i18n/` — capas estrictas.**

- **Contexto:** AUDIT-2 detectó 2 violaciones cross-layer: `engine` → `editor` (`ProjectSerializer.h` incluía `editor/workspace/Workspace.h`) y `core` → `engine` (`UserSettings.h` incluía `engine/i18n/I18n.h`).
- **Solución:** mover el `Workspace` *struct* (puro, sin ImGui) a `engine/project/`. El `WorkspaceManager` (UI de switching) queda en `editor/workspace/`. Mover `I18n.h/cpp` a `core/i18n/` (genuinamente shared — solo JSON + std).
- **Resultado:** 0 violaciones de layer en el grep cruzado. La jerarquía `core/` ← `engine/` ← `editor/|player/|systems/` se respeta estrictamente.

**Decisión clave 5 — Cadencia de audits ahora reactiva, no proactiva.**

- **Contexto:** La cadencia original era "1 audit cada ~5 hitos". Tras 3 audits consecutivos, el HARD cap está limpio, el layer audit en cero, y los pendientes futuros (DOD profiling, dead code, backfill F2H1-F2H54) requieren tooling o demanda real que no existe hoy.
- **Nueva regla:** próximo audit **solo si emerge dolor concreto**. No agendar audits vacíos por cumplir la cadencia. El sizeometer queda como herramienta operativa (`tools/sizeometer.sh` antes de cada cierre de hito) — detecta el dolor si reaparece.

**Estado final post-tanda:**

```
                      AUDIT-1   AUDIT-2   AUDIT-3
HARD cap (>800):        5         2         0
Layer violations:       —         2         0
Pure helpers (+tests):  0         0         1 (+7 tests)
```

24 archivos en SOFT cap (501-800) — bajo umbral pero no zona roja. Top candidatos de split natural si crecen: `SceneRenderer_Render.cpp` (788) y `EditorApplication.h` (786).

---

## 2026-05-17: F2H62 cierre — Shader Graph runtime + migración a imnodes + polish UX

**Contexto:** sexto hito de **Sub-fase 2.6 — Render polish**, = F2H18 del plan original Fase 2. **Cierra Sub-fase 2.3 (Renderer) del plan original 100%** junto con los anteriores (Hito 17 PBR, F2H55 bloom, F2H56 SSAO, F2H60 CSM, F2H61 SSR). Tag `v1.49.0-fase2-hito62`. Dev validó tras tour del hologram sample (cyan + Fresnel rim en aristas): *"creo que esta bien"*.

**Decisión clave 1 — Migración mid-hito `imgui-node-editor` → `imnodes` (Nelarius).** Durante Bloque C el dev reportó bugs persistentes de UX al mover nodos cerca de otros (cita: *"si quiero mover el nodo 2... estos bugs no me dejaron tranquilo"*).

- **Root cause:** `imgui-node-editor` (ax::NodeEditor de thedmd) tiene bug de hit-test de proximidad. Al arrastrar un nodo cerca de otro, el editor "salta" la captura al vecino. Conocido upstream, sin fix corto plazo.
- **Razón para migrar mid-hito:** el shader graph editor sería el panel más usado de F2H62. Convivir con bugs persistentes contaminaba la validación de las demás piezas (cache, samples, integración).
- **Solución:** rewrite completo del wrapper `NodeGraphEditor.{h,cpp}` usando imnodes (`Nelarius/imnodes`, pinneado a master `eb36902c892548ef94f88f51ad7e7c9c7058a71c`). API más declarativa → ~70% menos código que la implementación previa.
- **Trade-offs aceptados:** imnodes v0.5 (último release) era incompatible con ImGui docking; tuvimos que pinear a HEAD del master. `target_compile_definitions(imnodes PUBLIC IMGUI_DEFINE_MATH_OPERATORS)` requerido por la API de imnodes. Lazy `ImNodes::CreateContext` via `atomic_bool` para el lifecycle. Per-instance `EditorContext`. Drag detection via diff de mouse-down state (imnodes no expone callback de "drag-end"). Resync de posiciones gated por "mouse idle ambos frames" (evita race entre drag y resync).
- **Cero impacto al DialogEditorPanel** (F2H46) — mismo wrapper `NodeGraphEditor`, mismo API. Migración transparente.
- **Memoria persistente:** memory entry `project_node_editor_lib` guardada para futuras sesiones.

**Decisión clave 2 — Solo fragment graph v1; vertex sigue siendo `pbr.vert` estático.**

- **Razón:** el motor tiene 3 variants del vertex (`pbr.vert` static, `pbr_instanced.vert` instanced, `pbr_skinned.vert` skinned). Cada uno define `vViewSpaceNormal`, `vWorldPos`, etc. con interfaces compatibles entre sí pero distintas en la entrada (atributos de instancia, bones). Un vertex graph runtime tendría que generar las 3 variants + cachearlas → triplica complejidad.
- **Alternativa diferida:** vertex graph como hito separado si emerge demanda específica (displacement, wave/vegetación procedural).

**Decisión clave 3 — SSA naming `v_<socketId>` para variables intermedias del GLSL generado.**

- **Razón:** los socket IDs del `NodeGraph::Graph` son únicos por asset (`next_socket_id` counter). Reusarlos como SSA names garantiza unicidad sin tracking adicional. Trivial debugging: si el GLSL falla, el socket ID está en el log y en el JSON.
- **Trade-off:** los nombres no son human-readable (`v_14` en vez de `v_lerp_emissive`). Aceptable porque el GLSL generado se ve solo en compile output cuando falla.

**Decisión clave 4 — Markers `__SHADERGRAPH_<TAG>__` con post-substitution validation.**

- **Contexto:** descubrimos durante validación que el template `pbr_graph_template.frag` documentaba los markers en comentarios de cabecera Y los usaba como puntos de inyección en `main()`. `replaceOnce` por marker pisaba el COMENTARIO (primera ocurrencia) → puntos de inyección reales quedaban como `__SHADERGRAPH_DECLS__` literales → GLSL `ERROR: 0:9: '-' syntax error`.
- **Fix triple:** (1) renombré markers en comentarios a tokens sin `__` doble (no matchea); (2) el generator valida post-substitución que no queden `__SHADERGRAPH_` residuales y devuelve error claro `"el marcador X aparece mas de una vez (o no fue substituido)"` en vez de mandar GLSL roto al driver; (3) 2 tests nuevos en `test_shader_graph.cpp` cubren el caso (template con marker duplicado debe fallar limpio + template real del repo no debe dejar residuales).
- **Alternativa rechazada:** `replaceAll` por marker. Falla para `__SHADERGRAPH_DECLS__` que se reemplaza con MULTI-LÍNEA — el comentario `// __SHADERGRAPH_DECLS__ -> docs` se rompe porque el `\n` de las decls hace que `-> docs` quede como código (no comentario) → syntax error de otro tipo.

**Decisión clave 5 — Materiales con shaderGraphPath caen a `nonBatchable` (path A.2 no-instanced).**

- **Contexto:** el `SceneRenderer` tiene 3 paths: A.1 instanced (`pbr_instanced.vert`), A.2 nonBatchable (`pbr.vert`), B skinned (`pbr_skinned.vert`). El `ShaderGraphCache` v1 solo soporta `pbr.vert` estático (decisión 2). Si un material con shader graph cae al path instanced, el cache no tiene shader compatible y el grafo no se renderea.
- **Razón:** en `RenderBatching.cpp::groupByBatch`, después del cull pero antes de decidir batching, chequeo `material->shaderGraphPath` — si no vacío, ruteo a `nonBatchable`. El draw del nonBatchable usa `drawMeshRenderer` que pide al cache el shader específico.
- **Trade-off:** pierde batching para esos materiales (cada entidad con shader graph = 1 draw call). Aceptable porque shader graphs son para efectos especiales (pocos en la práctica — water surface, hologramas, glow), no para terrain tiles repetidos que necesitan instancing.
- **Alternativa diferida:** soportar `pbr_instanced.vert` en el cache + variant per-graph. Triplica el cache size y la complejidad. Solo justifica si emerge demanda de "100 cubos con shader graph idéntico" (poco probable).

**Decisión clave 6 — Cache keyed por logical path + hash GLSL.**

- **Razón:** la key del cache es el `logicalPath` (`"shaders/graphs/rojo.moodshader"`). La entry guarda `glslHash` (std::hash<std::string> del GLSL generado). Cada `getOrCompile` regenera el GLSL (baratísimo: ~17K chars, walk topológico de pocas decenas de nodos) y compara hash. Si difiere, recompila el program OpenGL.
- **Alternativa rechazada — timestamp del archivo:** el editor edita en memoria y guarda explícitamente; entre saves el archivo en disco está stale. Hash del GLSL es la fuente de verdad: si el dev cambia el grafo y NO guarda, el hash igual cambia y el cache recompila → el viewport actualiza en vivo.
- **Alternativa rechazada — recompilar cada frame:** ~17K chars de string hashing + comparación de unsigned long. Trivial. Pero el `OpenGLShader::buildProgram` sí cuesta (compilación driver + link). Sin hash, recompilaríamos cada frame.

**Decisión clave 7 — Custom ImGui save modal como triple-fallback sobre `pfd::save_file`.**

- **Contexto:** `pfd::save_file` (portable-file-dialogs, dependency externa) falla silencioso en Windows con paths relativos o caracteres especiales. Cita del dev al detectarlo: *"el boton de guardar sigue sin funcionar"*. Logs mostraban `Save As cancelado por el usuario` instantáneo (sin que el dev tocara cancelar).
- **Razón fix triple-fallback:** (1) intentar pfd nativo (path absoluto + `.string()` en vez de `.generic_string()`); (2) si retorna vacío sin excepción, custom ImGui modal con `InputText` simple que escribe al subdir `shaders/graphs/` del proyecto + sufijo `.moodshader` automático; (3) Ctrl+S directo si ya hay path conocido.
- **Trade-off:** el modal custom no respeta la convención del SO (no muestra el árbol de directorios). Aceptable porque los `.moodshader` siempre viven en `assets/shaders/graphs/`.
- **Lección durable:** las dependencias de file dialog son frágiles cross-platform. Siempre tener fallback in-engine para acciones críticas de save.

**Decisión clave 8 — Dropdown estilo Blender en Inspector sobre InputText manual.**

- **Contexto inicial:** Bloque D entregó un InputText donde el dev tipeaba el path al .moodshader. Cita del dev al validar: *"queda muy contra intuitivo que deva escribir el path para ver si el color anda, debemos buscar una forma de hacer que todo este sistema de texturas procedurales, sera lo mas blender, porque en ese programa funciona excelente"*.
- **Razón:** convención industria (Blender Principled BSDF, Unity Shader Graph asset dropdown, Unreal Material picker). Scan del dir `assets/shaders/graphs/` per-frame (decenas de archivos, baratísimo) → `ImGui::Combo` con "(PBR estándar)" + listado ordenado de `.moodshader`. Item huérfano al final si el path actual del material no existe en disco (archivo borrado, path mal escrito).
- **Trade-off:** un InputText es 5 LOC; el dropdown con scan es ~60 LOC. Vale la pena por UX claramente superior.

**Razones de revisión (cuándo volver a discutir):**
- Si emerge demanda de vertex shader graph → revisar decisión 2 (variants instanced/skinned/depth).
- Si emerge demanda de "100 entidades con shader graph idéntico" → revisar decisión 5 (soporte instanced en cache).
- Si emerge demanda de `.cube` LUT-style format para subgraphs → revisar el schema del .moodshader.
- Si pfd::save_file mejora o lo migramos a `nfd-extended` → revisar decisión 7 (mantener fallback como safety net igual).

---

## 2026-05-17: F2H61 cierre — SSR (Screen-Space Reflections) + G-buffer parcial via MRT

**Contexto:** quinto hito de **Sub-fase 2.6 — Render polish**, = F2H20 del plan original Fase 2 (Sub-fase 2.3 — Renderer). Tag `v1.48.0-fase2-hito61`. Dev validó tras tour visual: *"se ve decente"*.

**Decisión clave 1 — MRT G-buffer parcial vs depth-prepass separado.** Para SSR necesitamos sample de normal en view-space + depth + color. Dos approaches estándar:
- **A. MRT** sobre el sceneFb existente: segundo color attachment (RGBA16F) que recibe el normal. El PBR shader emite ambos outputs en el mismo draw call.
- **B. Pre-pass dedicado** depth+normal: shader nuevo que dibuja toda la geometría a un FB separado solo para llenar el G-buffer.

Elegimos **A (MRT)**.

- **Razón:** A duplica memoria (~8MB extra a 1080p para el normal RT en RGBA16F) pero NO duplica draw calls. B duplica draw calls (cada mesh se rendea 2 veces). Para N draws, A es 50% más barato GPU.
- **Trade-off:** shaders no-PBR (skybox/particles/debug) NO escriben el location 1 → quedarían con basura. Mitigación: `glClearBufferfv(GL_COLOR, 1, zero)` específico al location 1 después del clear global. SSR descarta pixels con `alpha < 0.5`.
- **Alternativa diferida:** si emerge presión de memoria (4K), packing del normal en R10G10B10A2 o reconstrucción del normal del depth (como hace el SSAO de F2H56).

**Decisión clave 2 — Algoritmo linear DDA view-space vs perspective-correct DDA NDC vs Hi-Z.** McGuire 2014 ofrece 3 variantes; elegimos **linear view-space DDA** para v1.

- **Razón:** ~30 líneas de GLSL vs ~80 perspective-correct vs ~150+ Hi-Z. Calidad "decente" según validación del dev. McGuire concede que linear es "good enough" para charcos/pisos/agua.
- **Trade-off:** lejano refleja menos preciso (pasos grandes generan miss/hit irregulares). Para v1 OK.
- **Alternativa diferida:** Hi-Z si emerge demanda de reflejos en grandes superficies de agua o perf (Hi-Z reduce pasos típicos 30→5).

**Decisión clave 3 — SSR después de SSAO en el pipeline.** Pipeline final: `sceneFb → SSAO → afterSsao → SSR → afterSsr → Bloom → ColorGrading → PostProcess`.

- **Razón:** SSR lee color del `afterSsao` (con AO ya aplicado). El reflejo, al sumarse a baseColor, también se "moja" del AO. Físicamente plausible: reflejo en esquina ocluida debería ser sutilmente más oscuro.
- **Trade-off:** el reflejo en sí NO recibe AO de la zona donde apunta. Visualmente casi imperceptible.
- **Alternativa rechazada:** SSR antes de SSAO requeriría refactor del SSAOPass para aceptar `colorSrc` + `depthSrc` por separado. Demasiado scope para marginal correctness.

**Decisión clave 4 — Firma del SSRPass con 2 FBs source.** El `apply()` toma `srcColor` + `srcGbuffer` por separado.

- **Razón:** el color procesado viene post-SSAO (con AO); depth y normal viven en el sceneFb original. Pasar 2 FBs es más limpio que copiar depth/normal al ssrFb cada frame.
- **Trade-off:** la firma rompe la simetría de los otros pases (1 src + 1 dst). Aceptable porque expresa el concepto natural: SSR opera sobre un G-buffer compartido + un color buffer downstream.

**Decisión clave 5 — Sin per-material SSR toggle en v1.** El SSR aplica a TODOS los pixels con `alpha > 0.5` del normal RT con la misma `intensity` global.

- **Razón:** per-material requiere extender MaterialAsset + propagar al PBR shader + emitir bit al alpha del normal RT (packing trick) o crear un 3er G-buffer attachment. Triplica complejidad. Para v1 el dev controla con la intensity global.
- **Alternativa diferida (BACKLOG):** si emerge demanda "solo metálicos reflejan", emitir flag al alpha (`alpha = (metallic > 0.5) ? 1.0 : 0.5`) + SSR ramp por alpha.

**Decisión clave 6 — `ssrMaxSteps` como `u32` en lugar de `i32`.** Tipo en C++ es u32, GLSL es int. Cast en el setter: `setInt("uMaxSteps", static_cast<i32>(maxSteps))`.

- **Razón:** `InspectorEditTracker::before` es un `std::variant<f32, glm::vec3, glm::vec4, bool, std::string, u32, std::pair<f32, f32>>`. NO incluye `i32`. Extender el variant compilaría templates en muchos lugares; más simple usar `u32` para ints positivos (mismo criterio que `csmCascadeCount`).
- **Trade-off:** `SliderInt` requiere `int&`, así que el InspectorPanel usa un proxy `int steps = static_cast<int>(...); SliderInt(..., &steps); env.ssrMaxSteps = static_cast<u32>(steps)`. 3 líneas de boilerplate, aceptable.

**Decisión clave 7 — Default OFF + fallback al cubemap IBL implícito.** SSR off por default. Cuando un fragmento no encuentra hit, el `ssr.frag` retorna `baseColor` inalterado — y `baseColor` ya tiene el cubemap IBL specular bakeado por el PBR pass.

- **Razón:** efectos visuales opt-in (criterio iter1 F2H60: *"todo deberia estar desactivado por defecto"*). El cubemap como fallback significa que pixels sin ray hit conservan el look pre-F2H61 sin necesidad de samplear el cubemap explícitamente en el SSR shader.
- **Trade-off:** físicamente sub-óptimo. El reflejo "real" (SSR hit) se SUMA al cubemap spec en lugar de reemplazarlo. "Double-counting" para superficies altamente metálicas. Solución correcta requeriría que el PBR shader emita el cubemap spec a un G-buffer separado y el SSR mezcle `lerp(cubemap, ssr, hit_confidence)`. Para v1 el additive simple "se ve decente".

**Lecciones cruzadas para futuros pases de post-process:**
- **Bug recurrente del clear global + MRT:** cuando un FB tiene 2 color attachments, `glClear(GL_COLOR_BUFFER_BIT)` aplica el `glClearColor` a AMBOS. Si el clearColor tiene `alpha = 1` (típico sky color), el alpha del normal RT queda en 1.0 → rompe el flag "pixel no-PBR". Mitigación: `glClearBufferfv(GL_COLOR, 1, zero)` específico al RT 1 después del clear global. Pattern aplicable a cualquier MRT futuro (G-buffer completo en F2H62+).
- **F2H61 cerró clean al primer intento** — sin pivots de scope durante el desarrollo. Validación visual del dev clean ("se ve decente") + 1 feedback de UX (Environment entry point) que NO ataca el render sino la UX del editor. F2H58/F2H59/F2H60 tuvieron pivots significativos. Posible explicación: SSR es feature contenida en el pipeline render, no toca UX del editor. **Generalizable: hitos "puramente técnicos" (sin nuevas UI) cierran cleaner que hitos UX**.

---

## 2026-05-17: F2H60 cierre — Cascade Shadow Maps (CSM) + 5 iteraciones de polish UX

**Contexto:** cuarto hito de Sub-fase 2.6 (Render polish). Plan original = CSM clásico, sucesor de Hito 16 (single shadow map estático). El agente propuso pivotar al "Source paradigm + meshes procedurales" como F2H60, pero el dev pushback con *"no quiero alejarme tanto de la vision y el plan original"* y *"hagamos todo lo que falta que venia del plan original, y guardemos lo que falta, de pendientes"* → reset a CSM clásico. Follow-ups descartados acumulados en `BACKLOG.md`. Tag `v1.47.0-fase2-hito60`. Hito mediano técnico (Bloques A-F) + iteración intensa de UX (iter1-5) descubierta durante el tour visual.

**Decisión clave 1 — Gate de sombras solo per-light (eliminado `csmEnabled` global).** Mi implementación inicial tenía un master switch `EnvironmentComponent::csmEnabled` Y el `LightComponent::castShadows` per-light, requiriendo ambos = true para que se vieran sombras. El dev activaba CastShadows en la luz directional pero nada aparecía hasta que también activaba CSM en Environment.

- **Razón:** doble gate = confusión cognitive load alta y feedback no inmediato (¿por qué no veo sombras si la luz tiene castShadows?). El gate per-light es el modelo mental natural: "esta luz proyecta sombras o no". El panel Environment debe contener solo knobs de **calidad** (cantidad de cascadas + lambda), no on/off.
- **Cambio aplicado:** eliminado `csmEnabled` del componente, del SavedEnvironment, del read/write del EntitySerializer, del SceneRenderer, del Inspector. La sección CSM del Inspector muestra un hint "Activá CastShadows en una Luz Direccional para que se vean".
- **Alternativa descartada:** auto-activar `csmEnabled` cuando se enable CastShadows. Rechazada por complejidad innecesaria (el campo legacy debe quedar) y por seguir requiriendo el toggle en el Inspector como UX.

**Decisión clave 2 — `uModel = worldMatrix` para brushes en el shadow pass.** Mi primer fix de iter3 usaba `uModel = identity` argumentando que `Csg::buildBrushMesh` "hornea el worldMatrix en los vertices". Bug visible: la sombra del cubo quedaba clavada en el origen del mundo (0,0,0); al mover el cubo se "desconectaba" de su sombra; al bajar el cubo encima del origen la sombra "desaparecía" porque se solapaba con el caster.

- **Razón:** lectura cuidadosa de `BrushMesh.cpp:154` muestra `v.position = p` SIN multiplicar por worldMatrix. El argumento worldMatrix solo se usa para UVs `lockToWorld` (proyección al world space del vertex antes de evaluar los UV axes). Las vertices del brush quedan en LOCAL space. El PBR pass usa `uModel = worldMatrix` para transformarlas; el shadow pass debe hacer lo mismo.
- **Lección general:** "el cache hornea X" requiere verificación en el source. La asunción cuesta horas de debug cuando se invalida silenciosamente. Documentado el bug en el comentario del fix en `ShadowPass.cpp` para que el próximo dev no repita.

**Decisión clave 3 — `applyEnvironmentFromScene` movido ANTES del shadow pass.** Pre-iter4 la llamada estaba en `renderScene` línea 136 (después del shadow pass). Los miembros `m_csmCascadeCount` / `m_csmSplitLambda` que el shadow pass consumía eran del frame anterior → cambios del Inspector tenían 1 frame de delay.

- **Razón:** `applyEnvironmentFromScene` solo modifica miembros del SceneRenderer (no toca GL state), así que es seguro moverla al tope del frame. El delay frame-vs-frame es invisible en condiciones normales (60 FPS = 16ms), pero para sliders en tiempo real el dev espera feedback inmediato y la asunción de "no responde" se construye rápido.
- **Pattern:** las funciones que solo configuran state desde una fuente de verdad (componentes del scene) deberían ejecutarse al inicio del frame, antes de cualquier pase que las consuma. Tomar nota para futuras refactorizaciones del frame loop.

**Decisión clave 4 — Logs diagnósticos conservados como deuda buena.** Iter5 destapó un bug crítico que llevaba semanas sin descubrirse: las fórmulas para extraer `cameraNear/Far` del proj matrix GLM right-handed estaban INVERTIDAS:

```cpp
// Pre-fix (INCORRECTO):
n = m32 / (m22 + 1)    // -> daba far
f = m32 / (m22 - 1)    // -> daba near

// Post-fix (CORRECTO):
n = m32 / (m22 - 1)
f = m32 / (m22 + 1)
```

El log `ShadowPass params: near=100, far=0.1, splits=[100.00 | 100.25 | ... | 101.00]` lo hizo trivial (los splits "fuera de cuadro" son visibles a simple vista). Sin el log el bug habría sobrevivido porque mover lambda **sí cambia el cálculo** — solo que el resultado sigue en rango basura, y el efecto visual es invisible.

- **Decisión:** conservar los 3 logs diagnósticos (`applyEnvironmentFromScene LEE Environment.CSM`, `ShadowPass params` con splits, `ShadowPass casters` por tipo) como infraestructura permanente. Solo logean al cambiar valores (low-spam). Costo: nulo en runtime estable, gigantesco beneficio cuando regresa un bug.
- **Pattern:** "imprimir los números clave" es la primera herramienta de debug. Logs diagnósticos de orden de magnitud (¿el near es 0.1 o 100? los splits cubren la escena o no?) atrapan bugs que tests unitarios no atrapan porque el bug está en el wireup (matriz pasada, no calculada).

**Decisión clave 5 — PSSM lambda hybrid 0.5 default + bias 0.0015 con escalado por cascada (×1, ×2, ×3, ×4).** Trade-off de calidad sweet spot.

- **Lambda 0.5** (Zhang 2006 PSSM hybrid): lambda 0 puro lineal subutiliza cascadas cerca (todas cubren áreas similares); lambda 1 puro log subutiliza cascadas lejos (cascadas 2-3-4 colapsan). 0.5 es el balance práctico documentado en el paper.
- **Bias 0.0015** sobre el 0.005 inicial: era ~0.25m de "lift" en NDC depth → peter-panning brutal al apoyar caster sobre receptor. 0.0015 elimina el problema sin generar shadow acné perceptible (en combinación con el cull `GL_FRONT` que descarta caras frontales del caster).
- **Escalado por cascada (×1, ×2, ×3, ×4)** porque cascadas lejanas tienen texeles más grandes; el bias absoluto NDC apropiado para cascada 0 es insuficiente para cascada 3.

**Decisión clave 6 — Modal Crear Entidad: tab "Luces" como ciudadano de primera clase.** Pre-F2H60 iter2 las luces solo se podían crear vía "Convertir entidad" sobre un mesh existente, lo cual era confuso para arrancar una escena (no hay mesh todavía). Iter2 agrega `ProjectAction::AddDirectionalLight` + `AddPointLight` + handlers que spawnean entidad solo con Tag + Transform + LightComponent (sin mesh; el icono 2D del overlay del editor la hace visible).

- **Razón:** el dev *"deberia poder crearse luces desde el panel de entidades"*. Workflow Hammer/SFM: 1 click → modal → tab → click. Consistente con la convención de F2H59 (modal único para crear geometría) extendida ahora a luces.
- **Defaults sensatos**: Directional spawn (0,3,0), dir (-0.3,-1,-0.2), **castShadows=true** (engine-grade out-of-the-box). Point spawn (0,2,0), radius 10m, color cálido.

**Decisión clave 7 — CSM clásico sobre Source paradigm (reset al plan original).** El agente propuso F2H60 = Source paradigm + meshes procedurales con física + UX polish acumulado (overlay context-aware, Map Tools híbrido Hammer+Blender, modifiers Blender-style, iconos homogéneos). El dev rechazó con *"no quiero alejarme tanto de la vision y el plan original... me estoy desviando muchisimo"*.

- **Razón:** disciplina de scope. El plan original Fase 2 tiene 16 hitos pendientes después de F2H60; cada pivot agrega complejidad y push out de los objetivos originales. Los follow-ups son valiosos pero no urgentes.
- **Mecánica:** `BACKLOG.md` creado al inicio de F2H60 (Bloque pre-A) para capturar todos los follow-ups sin priorización. El próximo hito (F2H61 SSR) sigue el plan original; los items del BACKLOG quedan disponibles para cuando emerja la demanda.

**Alternativas descartadas en CSM core:**
- Frustum cull per-cascada — diferido. Todas las cascadas dibujan toda la escena en v1. Si N-pass impacta perf medible, evaluar AABB frustum test por cascada antes del draw.
- VSM (Variance Shadow Maps) o ESM (Exponential Shadow Maps) — más complejos, requieren blur pass, light leak en grietas. PCF estándar 3×3 con hardware 4 taps = 36 muestras efectivas, calidad decente sin complejidad extra.
- Shadow map size dinámico per-cascade (cascadas cerca con mayor resolución) — viable pero requiere N texturas separadas en vez de un array. Optimización futura si emerge demanda.
- Cascada exponencial vs lineal vs PSSM — PSSM (Zhang 2006) es el estándar industria. Stable Cascaded Shadow Maps (Microsoft) requieren recompute por frame con cierto overhead; la implementación actual ya tiene texel snapping para evitar shimmering al rotar cámara.

**Condiciones de revisión:**
- Si en F2H61+ (SSR) o follow-ups posteriores el rendering pipeline cambia significativamente, revisar si el order de `applyEnvironmentFromScene` antes del shadow pass sigue siendo el correcto.
- Si emerge demanda de sombras para luces puntuales (point lights), implementar cube map shadows en hito propio (no encadenar a CSM).
- Si el bias 0.0015 + ×N por cascada empieza a mostrar acné en mapas reales, considerar slope-scale bias en el shader (depender del ángulo entre `N` y `L`).

---

## 2026-05-16: F2H59 cierre — Primitivas clásicas + reorg UX (modal Crear entidad + toolbar flotante)

**Contexto:** pivot temporal de Sub-fase 2.6 (render polish) a UX del editor entre F2H58 (color grading) y F2H6X (siguiente render polish), motivado por el dev que durante el tour visual de F2H58 detectó que estaba usando un Box brush aplastado como suelo y pidió la primitiva Plano + primitivas clásicas adicionales. La conversación escaló a UX reorg general. Tag `v1.46.0-fase2-hito59`. Hito mediano (Bloques A-G).

**Decisión clave 1 — Modal "+ Crear Entidad" como punto único de entrada para crear geometría.** Pre-F2H59 las primitivas vivían en 3 lugares: menú top-level "Brush > Añadir" (11 items), Toolbar lateral (Box + Cylinder), y modal "+ Crear Entidad" (que solo manejaba meshes importados). F2H59 consolida los 3 en el modal con TabBar (Meshes del proyecto / Primitivas).

- **Razón:** workflow Hammer/SFM consistente. 1 click en panel Escena → modal → click en primitiva. El dev no tiene que aprender 3 caminos distintos para crear geometría. Convención que ya empezamos a usar en F2H57 ("+ Crear Entidad" como punto único de entrada SFM-style).
- **Alternativa descartada:** mantener las primitivas en el menú Brush para "Hammer-purists" + el Toolbar como "atajos rápidos". Rechazada porque genera tres puntos de mantenimiento UI sincronizados. Si emerge demanda futura de atajos rápidos a primitivas comunes, agregamos un atajo de teclado o configurable.
- **Trade-off documentado:** el dev pierde 1 click vs el menú directo (modal → tab → click vs menú → click). Aceptable porque el modal también ofrece "Importar..." + "Empty" en el mismo flow.

**Decisión clave 2 — Toolbar como overlay flotante sobre el viewport, estilo Blender / Unity / Unreal moderno / Godot 4.** El panel Toolbar pre-F2H59 era una ventana dockable lateral con Box/Cylinder + gizmo modes + Face toggle. F2H59 lo migra a una sub-window ImGui flotante en la esquina superior-izquierda de la imagen del viewport (background transparente alpha 0 + border 0 + NoBackground), 4 botones icon-only: Mover / Rotar / Escala / "F".

- **Razón:** convención industria moderna. Hammer 2004 usa toolbar lateral fijo (paradigma MFC de Windows), pero Blender / Unity 2024 / Unreal 5 / Godot 4 usan overlay flotante porque libera espacio dockable para paneles más útiles (Hierarchy / Inspector / Asset Browser). El espacio horizontal del editor es escaso.
- **Limitación documentada:** el overlay actual NO es movible, NO es context-aware del workspace. F2H60 candidate evalúa overlay context-aware (botones específicos según workspace: narrative vs map_editor vs gameplay) y posiblemente movible con SetWindowPos.
- **Toolbar como panel queda dead-code linkeable** — no en `m_panels` pero el struct `m_toolbar` sigue declarado. Cleanup completo (eliminar `Toolbar.h/.cpp`) diferido por minimal-risk; si emerge demanda futura de reactivarlo como panel, está intacto.

**Decisión clave 3 — Background del overlay totalmente transparente** (alpha 0 + border 0 + flag `NoBackground`). Pedido explícito del dev: *"le podemos dejar el fondo transparente para que solo sean botones flotantes?"*.

- **Razón:** estética Blender — los botones de tools "flotan" sobre el viewport sin marco visual que los separe. Mejor integración visual con el render 3D detrás.
- **Bug pre-detectado:** con solo `SetNextWindowBgAlpha(0.0f)` quedaban líneas finas del frame de la sub-window. Fix: `PushStyleVar(WindowBorderSize, 0.0f)` + flag `NoBackground` explícito redundante con el alpha pero protege si el alpha falla por theme. Pedido del dev: *"se sigue viendo unas lineas finas"*.

**Decisión clave 4 — "F" como label del botón Face toggle, no `ICON_FA_VECTOR_SQUARE`.** Pedido explícito del dev: *"creo que deberia ser F de faces, ya que solo se usa en la edicion de texturas"*.

- **Razón:** la letra F como label es semánticamente clara (Face = F). El icono cuadrado FA es genérico y se confundía con Box / Quad. En el overlay flotante con 4 botones chicos (36×36 px), claridad > consistencia con icon set FA.
- **Aplicabilidad:** homogeneización general del sistema de iconos del editor agendada como follow-up F2H60+. Por ahora el botón F es la excepción minimal.

**Decisión clave 5 — Footer del modal con vocabulario universal de engines** ("Importar..." + "Empty"). Pre-F2H59 los labels eran "Importar desde archivo..." + "Crear vacía (placeholder)" — verbosos y específicos del workflow. F2H59 los acorta + adopta términos universales.

- **Razón:** "Empty" es vocabulario universal — Unity Empty GameObject, Unreal Empty Actor, Godot Empty Node. El dev que viene de cualquier engine moderno reconoce inmediatamente el botón. "Importar..." con elipsis es convención UI estándar para "abre file picker" (Unity Import / Unreal Import / Godot Import). Pedido explícito del dev: *"al termino que me referia era a Empty"*.
- **Trade-off i18n:** "Empty" queda en inglés también en `es.json` (no se traduce a "Vacío"). Convención: términos universales del vocabulario engine quedan en inglés (matchea expectativa del dev que viene de tutoriales en inglés). Si emerge demanda de full translation, evaluar.

**Decisión clave 6 — Toro skipped en v1.** Las primitivas pedidas eran Plano / Quad / Cono / Cápsula / Toro. Toro NO entró.

- **Razón técnica:** un toro NO es geométricamente convexo (tiene un agujero en el medio), y el sistema CSG de MoodEngine asume brushes convexos para todas las operaciones (boolean, picking, clipping). Implementar Toro como brush único es imposible sin refactor del CSG core.
- **Workaround documentado:** spawn 2 cilindros (outer + inner) + Brush > Operaciones Booleanas > Resta del inner. 3 clicks pero da un anillo CSG editable.
- **Alternativa diferida:** implementar Toro como N segmentos curvos auto-spawneados + group. Scope mayor, requiere primitive groups (no implementado). F2H6X+ evalúa cuando se vea modificadores Blender-style para booleans.

**Decisión clave 7 — Cápsula = Sphere dodecaédrica estirada Y 2×.** No es cápsula técnica (cilindro + 2 hemisferios con paredes laterales rectas) sino elipsoide alargado.

- **Razón:** trade-off de simplicidad. La cápsula como `makeSphereBrush(scale Y=2)` es 0 código nuevo. La cápsula técnica requiere nueva función `makeCapsuleBrush()` con geometría híbrida (cilindro central + 2 calotas hemisféricas preservando convexidad). v1 cubre el caso común "personaje proxy / pildora / pilar redondeado".
- **Revisión:** si emerge demanda de cápsula con paredes laterales rectas (gameplay con player controller cápsula), follow-up implementa `makeCapsuleBrush()` real.

**Decisión clave 8 — Brushes son geometría estática del mapa, NO objetos físicos dinámicos. Source/Hammer paradigm explícito agendado a F2H60.** Durante el tour el dev creó un cubo brush, le agregó RigidBody Dynamic, dio Play y no cayó. Causa raíz: el mesh cache del brush se construye con `worldMatrix` incrustado en los vértices y solo se rebuilda si `bc.dirty=true` — al cambiar `t.position` el cache queda desactualizado. El body de física Jolt SÍ cae internamente, pero el visual no se mueve.

- **Decisión arquitectónica (F2H60):** adoptar explícitamente la separación que Source/Hammer 2004 hizo con Half-Life 2 (Havok Physics). **Brushes** = estructura estática del mapa (paredes, pisos, columnas) — nunca dinámicos. **Meshes (props)** = objetos del juego (cajas, barriles, NPCs) — pueden ser Static / Kinematic / Dynamic libremente. Source separó porque BSP/CSG no se lleva bien con simulación rígida — la geometría de un brush se materializa al compile-time del mapa, no a runtime per-frame.
- **Implicancias en MoodEngine F2H60:** rename UI "Brush" → "Estructura" / "Geometría del nivel" (el término técnico "brush" queda solo en código + docs). Tab "Primitivas" del modal Crear Entidad se divide en 2 sub-secciones: **Estructura** (los 11 brushes actuales) + **Objetos** (5-6 meshes procedurales: Cubo, Esfera, Cilindro, Cápsula, Cono — generados en runtime con vertices/normales/UVs puros, soportan física dinámica nativamente). Warning en Inspector si el dev pone RigidBody Dynamic sobre un Brush — slider Type Dynamic queda disabled con tooltip.
- **Cita del dev:** *"que es mejor usar meshes o brushes? para nuestro editor me refiero, porque me gusta lo de los brush, por ahi confunde el termino o su definicion pero si es util"*. Tras explicación del Source paradigm: *"sigamos esa direccion"*.
- **Alternativa descartada:** refactor del render de brushes para que rebuilden el mesh cache cada frame si tienen RigidBody Dynamic (1 línea de código: `bc.dirty = (rb.type == Dynamic);`). Rechazada porque mezcla los paradigmas — el dev podría poner RigidBody Dynamic sobre brushes complejos (boolean trees, polígonos arbitrarios) y el rebuild per-frame del mesh cache impactaría performance. La separación Source es más limpia conceptualmente.

---

## 2026-05-16: F2H58 cierre — Color grading LUT-based + consolidación Environment + UX polish

**Contexto:** tercer hito de Sub-fase 2.6 (Render polish): bloom (F2H55) → SSAO (F2H56) → **color grading (F2H58)** → god rays / shadow polish (F2H6X+). Tag `v1.45.0-fase2-hito58`. Hito mediano que creció de 7 bloques planeados (A-G) a 10 (A-J + fix lateral) por feedback iterativo del dev durante el tour visual.

**Decisión clave 1 — Pre-tonemap LUT (convención Unity URP) sobre post-tonemap (Unreal).** El plan original (PLAN_HITO_F2H58 línea 171) ya documentaba las dos escuelas. v1 va con pre-tonemap clamp `[0,1]` por simplicidad del color space: la LUT opera en el rango que cualquier herramienta de cine (Photoshop, GIMP, DaVinci) le pasa al colorista por default.

- **Razón:** Unity URP usa el mismo path y es el flow más documentado entre los motores open-source que portamos shaders (Filament, Godot 4). Cambiar a post-tonemap requeriría re-arquitectar la posición del pase en el pipeline y posiblemente convertir las LUTs existentes.
- **Alternativa diferida:** si emerge feedback de coloristas de que las LUTs de DaVinci Resolve (que asumen input post-tonemap) no quedan bien aplicadas pre-tonemap, evaluar switch. **Mitigación:** los 4 LUTs sample que shippeamos (`identity`, `cinema_warm`, `matrix_cool`, `noir_high_contrast`) están generados con `tools/gen_luts.py` operando sobre la identity table — son coherentes con el path pre-tonemap del shader y se pueden regenerar trivialmente si se cambia el approach.

**Decisión clave 2 — Color grading default OFF, a diferencia de bloom/SSAO.** Bloom y SSAO en F2H55/F2H56 quedaron default ON con valores razonables porque sin ellos la escena se ve "plana" — ambos efectos aportan grounding visual incluso con intensidades bajas.

- **Razón:** color grading sin LUT (con la identidad) es no-op. Con una LUT cargada el efecto cambia el look entero de la escena — eso es una decisión de art direction que el motor no debe imponer. Engine-grade significa proveer defaults sensatos, no opiniones de director artístico. El dev del juego elige el look conscientemente.
- **Trade-off de discoverability:** el dev podría no enterarse de que existe color grading si nunca prende el checkbox. **Mitigación:** los presets en el dropdown (Cálido / Frío / Noir) son visualmente obvios y nombrados con la mood asociada — eso da exposure UX al feature sin imponer el efecto por default.

**Decisión clave 3 — Path field como mecánica interna + preset dropdown como UX.** El dev pidió explícitamente: *"lo de LUT no lo veo viable, a menos que tenga pressets incluidos"*. Pre-feedback la UI mostraba un `InputText` con el path lógico (ej. `luts/cinema_warm.png`) — UX técnica más que de director artístico.

- **Razón:** el dev del juego piensa en términos de "look" ("quiero un atardecer cálido", "quiero un noir") no de paths de archivo. Pre-feedback el flow obligaba a explorar `assets/luts/` con el file picker para descubrir qué hay; post-Bloque H el flow es 1 click en el dropdown.
- **El path sigue siendo la mecánica de persistencia** — el `.moodmap` guarda `colorGradingLutPath: "luts/cinema_warm.png"` y los devs pueden inspeccionar/versionar/editar manualmente. El preset dropdown es resolución bidireccional: lee el path del componente y lo matchea contra los built-ins; si no matchea, muestra "Personalizado: \<filename\>". Best of both worlds.
- **Alternativa descartada:** ocultar el path completamente y usar IDs internos del preset (`PresetId::CinemaWarm`). Rechazada porque ata el formato del .moodmap a un enum que rompería si en el futuro removemos un built-in. El path tiene la ventaja de ser self-documenting y forward-compatible.

**Decisión clave 4 — Reset per-section sin undo en v1.** Los 5 botones `⟲ Restablecer` (uno por sección Sky+Fog, Tonemap, Bloom, SSAO, Color Grading) asignan los campos desde una instancia `kEnvDefaults` default-constructed.

- **Razón:** snapshot multi-campo undoable requeriría un `ResetSectionCommand` custom que serializa el `EnvironmentComponent` entero pre-reset y deserializa en undo. Es complejidad significativa para una operación claramente intencional (el dev clickea explícitamente).
- **Trade-off documentado:** si el dev resetea por error, no hay Ctrl+Z. **Mitigación:** los edits individuales sobre los sliders post-reset SÍ son undoable (mismo `pushEditIfDone` pre-existente). En el peor caso el dev reescribe los valores a mano — fricción aceptable para no inflar v1.
- **Revisión:** si emergen reportes de "reseteé y perdí mi config", priorizar `ResetSectionCommand` como sub-hito puntual.

**Decisión clave 5 — Post-Process wrapper con `ImGui::Indent/Unindent` sobre TreeNode anidado nativo.** Bloque I reorganiza el Inspector a 2 headers top-level (Sky+Fog y Post-procesado) con los 4 sub-pases (Tonemap / Bloom / SSAO / Color Grading) dentro de Post-procesado.

- **Razón:** ImGui no anida `CollapsingHeader` nativamente con un solo chevron — usar `TreeNode` anidado da un look-and-feel diferente (chevron doble) que no matchea la convención visual del resto del Inspector. `CollapsingHeader` plano + `Indent/Unindent` da el efecto visual de subordinación sin alterar el control de colapso.
- **Patrón Unity Volume / Unreal PPV:** ambos usan algún tipo de indent visual o group headers — F2H58 elige la opción más simple compatible con el resto del panel.
- **Aplicabilidad a otros paneles:** si emerge la necesidad de jerarquía visual similar en otros sub-paneles del Inspector (Mesh con submesh details, Animation con timeline groups), el patrón `Indent/Unindent` queda como template reusable.

**Decisión clave 6 — IDs únicos por sufijo `##envreset_<id>` para los 5 botones reset.** Bug descubierto al primer tour: los 5 botones con mismo label visible "Restablecer" colisionaban en el ImGui ID interno → solo uno respondía al click.

- **Razón:** convención ImGui — labels visibles iguales necesitan IDs distintos. Las opciones son: (a) labels visibles diferentes ("Restablecer Sky+Fog" / "Restablecer Bloom" — verboso), (b) sufijos `##id` que ImGui usa internamente sin mostrar (mantiene texto visible limpio), (c) `PushID/PopID` por sección (más invasivo). Elegimos (b) por mínima fricción.
- **Generalización:** cualquier helper que pinte widgets con label visible idéntico debe tomar un `idSuffix` parameter — patrón aplicable a futuros helpers del Inspector.

**Decisión clave 7 — Fix MenuBar pre-existente incluido en F2H58 (bug de F2H57).** Durante el tour Bloque F el editor crashea con assert ImGui `EndMenuBar`. Root cause: F2H57 Bloque E borró un `EndMenu()` del menu Help junto con el submenu Demos eliminado. El bug NO se manifestó en F2H57 porque el test de ese hito no hizo click sobre Help.

- **Decisión:** fix de una línea (`ImGui::EndMenu();` en `MenuBar.cpp:256`) incluido en F2H58 en vez de hito separado. **Razón:** sin el fix, F2H58 no es testeable visualmente (el crash bloquea el tour). Hito de fix-only para una línea es overhead burocrático. **Mitigación:** commit separado con prefijo `fix(F2H57 followup):` para que git blame del MenuBar atribuya correctamente el origen del bug y del fix.
- **Aplicabilidad futura:** patrón de "fix lateral de hito anterior incluido en hito siguiente con commit separado" queda como precedente — preferible a inflar el hito siguiente con tag separado para una línea de fix.

---

## 2026-05-15: F2H57 cierre — Workflow Crear+Convertir entidad estilo Hammer/SFM

**Contexto:** pivot temporal de Sub-fase 2.6 (render polish) a UX del editor entre F2H56 y F2H58, motivado por 3 bugs UX detectados durante el tour visual de F2H56. Tag `v1.44.0-fase2-hito57`. Dev validó: *"lo demas anda perfecto, me gusta como esta"* tras el followup del modal SFM + welcome centering.

**Decisión clave 1 — Workflow SFM-style sobre Hammer puro.** El plan original era abrir directo el file picker del SO (convención Hammer Editor). El dev rechazó esa UX cuando vio la primera versión: *"creo que el crear entidad deberia darme la opcion de usar un mesh que este dentro del sistema, por ejemplo el hammer usa el modelo base de freeman… te cuesta leer lo que hay internamente"*. Pivot a Source Film Maker: el modal muestra primero la lista de meshes ya importados al proyecto, con botón secundario para importar uno nuevo desde el SO.

- **Razón:** el caso común del dev no es "tengo un FBX nuevo en mi escritorio", es "tengo 10 FBX ya en mi proyecto, quiero reusarlos". SFM optimiza ese flujo. El file picker del SO sigue accesible pero deja de ser el path por default.
- **Alternativa descartada:** popup con 3 opciones (Vacía / Desde modelo en el proyecto / Desde archivo del SO) antes de abrir el modal. El dev pidió eliminarla: *"que directamente al dar click en crear entidad, ya habra el modal interno"*. La razón es UX: 1 click directo al modal es menos fricción que click → popup → click → modal.
- **Final UX:** click en `+ Crear Entidad` → modal SFM con lista de meshes + dos botones de acción al pie ("Importar desde archivo..." + "Crear vacía (placeholder)") + "Cerrar". Punto único de entrada.

**Decisión clave 2 — Kits del modal Convertir son aditivos no-destructivos.** Convertir una entidad agrega los componentes del kit sin remover los existentes. Si la entidad ya tiene `DialogComponent`, el botón "NPC con diálogo" queda disabled.

- **Razón:** modo destructivo con confirm ("te voy a sacar X / Y / Z componentes — ¿confirmás?") es más predecible pero también más annoying. Aditivo permite stack: el dev puede aplicar "Item pickeable" + "Luz puntual" + "NPC con diálogo" en la misma entidad sin cerrar el modal entre clicks.
- **Trade-off de undo:** el paso convert NO es undoable en v1. Snapshot pre/post de componentes requeriría serializar la entidad completa (vía `EntitySerializer::serialize`) antes y restaurar el JSON en undo. Diferido por scope. **Mitigación:** edits individuales post-convert SÍ son undoable via `InspectorEditCommand` F2H32, así que sacar manualmente un componente del kit que aplicaste por error sigue siendo trivial.
- **Revisión:** si emergen reportes de "apliqué un kit por error y tuve que sacar 3 componentes a mano", priorizar snapshot undoable como sub-hito puntual.

**Decisión clave 3 — Demos removal minimal-risk.** Eliminamos solo las entries del menú `Ayuda > Demos` en `MenuBar.cpp` (~80 líneas). Los `DemoSpawners_*.cpp` quedan como dead code en `CMakeLists.txt` para no romper helpers compartidos como `ensureDemoIntroDialogExists`.

- **Razón:** el cleanup completo (borrar los .cpp + extraer helpers compartidos a sitio nuevo) infla la diff con cambios que pueden romper algo lateral. Engine-grade prioriza minimizar regresión por hito. El dead code no tiene cost runtime — solo compila y queda sin caller.
- **Alternativa diferida:** sub-hito de cleanup completo de los DemoSpawners + extracción de los helpers a `EditorHelpers` o equivalente, una vez confirmado que ningún consumer interno los necesita.
- **Revisión:** flagueado como tech-debt en `MoodEngine`. Si emerge demanda (alguien intenta agregar otro demo o un test rompe), atacar entonces.

**Decisión clave 4 — Welcome modal recentra cada frame (`ImGuiCond_Always` + `WorkPos + WorkSize/2`).** Pre-F2H57 el welcome usaba `ImGuiCond_Appearing` con `viewport->GetCenter()`. En pantallas >720p el viewport del primer frame todavía no tiene su tamaño definitivo → modal queda off-center.

- **Razón:** el costo de recalcular center cada frame es despreciable (suma de 2 floats + dos puntos flotantes) y elimina la categoría completa de bugs de "modal off-center cuando la ventana del OS cambia de tamaño".
- **WorkPos + WorkSize sobre `GetCenter()`:** WorkSize excluye la menubar — sin esto el modal queda visualmente alto (porque el GetCenter incluye el espacio de menubar). WorkPos cubre el caso de multi-viewport ImGui (cuando un docking layout mueve el work area).
- **Aplicabilidad a otros modales:** si emerge el mismo bug en convert/pick mesh modals, repetir el mismo patrón. v1 los deja con `ImGuiCond_Appearing` porque arrancan después del welcome (viewport ya estable).

**Lecciones cruzadas para futuros pivots UX:**
- **Tour visual del dev > validación headless cuando se trata de UX**: las 3 issues que motivaron F2H57 ninguna era detectable por suite de tests — emergieron porque el dev usó el editor por 20 minutos para crear una escena de testing del bloom de F2H55. Pattern: priorizar tour visual como criterio de aceptación de cierres de hitos que tocan render/editor.
- **El plan inicial del hito no sobrevive al primer feedback real**: PLAN_HITO_F2H57.md draft tenía Hammer file picker como Bloque B. Tras la primera demo el dev pidió SFM-style + popup intermedio + después pidió eliminar el popup. El plan se refinó en flight. Engine-grade no significa "spec inmutable" — significa "cada cambio del plan se documenta en DECISIONS".

---

## 2026-05-14: F2H56 cierre — SSAO + depth-texture FB + per-scene settings

**Contexto:** segundo hito de **Sub-fase 2.6 — Render polish** (F2H55 = bloom). Continúa el orden de impacto visual planeado: bloom → AO → color grading → god rays. Tag `v1.43.0-fase2-hito56`. Dev validó: *"el SSAO funciona bien"* — esquinas y debajo de objetos se oscurecen sutilmente al default; subiendo intensity a 3.0 el efecto se vuelve marcado.

**Decisión clave 1 — Port de Filament/Godot vs adoptar AMD FidelityFX CACAO.** Mismo dilema que tuvimos con bloom F2H55, pero esta vez SÍ existe una lib externa real (CACAO de AMD, MIT, mantenida, alta calidad). Elegimos **portar** igual.

- **Razón:** consistencia con la filosofía aplicada en F2H55 (no-reinventar = portar de open-source comprobado, no agregar dependencia externa). El port de Filament/Godot 4 cubre el caso base (~150 líneas GLSL) y CACAO sería overkill para v1 cuando ni siquiera sabemos si el dev necesita más calidad.
- **Alternativa diferida:** CACAO como upgrade futuro si emerge calidad pobre. El swap sería un sub-hito puntual (reemplazar el `ssao.frag` + ajustar uniforms; el wireup C++ no cambia porque el shader interfaz es la misma).
- **Trade-off documentado:** SSAO básico de Filament tiene noise inherente que el blur 4x4 disimula parcialmente — visualmente queda "OK" pero no "premium". Si el dev pide "se ve raro / noisy", swap a CACAO.

**Decisión clave 2 — Depth attachment como textura solo en modo HDR de OpenGLFramebuffer.** El FB principal de scene render (HDR RGBA16F) ahora crea su depth como `GL_TEXTURE_2D` con formato `GL_DEPTH24_STENCIL8` en lugar de `GL_RENDERBUFFER`. LDR FB (viewport final que muestra ImGui) mantiene renderbuffer.

- **Razón:** SSAO necesita samplear depth desde un fragment shader → requiere textura. LDR FB no necesita samplear su propio depth (solo se usa para Z-test al renderizar, y al final el `glColorTextureId` es lo que ImGui muestra).
- **Trade-off de memoria:** ~24-32 MB extra a 1920x1080 (textura vs renderbuffer ambos son lo mismo en memoria, el cambio es de tipo de objeto OpenGL, no de tamaño). Sin overhead en runtime.
- **Compatibilidad:** Shadow pass usa su propio FB (no `OpenGLFramebuffer`), no se afecta. Ortho viewports usan LDR, mantienen renderbuffer.
- **Revisión:** si en el futuro algún pass LDR necesita samplear depth, agregar flag al constructor de `OpenGLFramebuffer` para forzar textura en LDR. No emerge caso de uso todavía.

**Decisión clave 3 — SSAO multiplica el color HDR final, no solo el término ambient del PBR shader.** Esto es **conscientemente incorrecto físicamente** pero simplifica drásticamente la integración v1.

- **Razón pragmática:** integrar AO en el PBR shader (`pbr.frag`) implica agregar un sampler nuevo + branch para AO opcional + cambiar el flow de iluminación. Tocar el shader crítico del PBR es riesgoso (regresiones en todas las escenas). El composite separado en SSAOPass mantiene el cambio aislado.
- **Consecuencia visual:** la luz directa también recibe oclusión (las sombras de cualquier directional light se OSCURECEN MÁS donde hay AO), lo cual no es físicamente correcto — solo el ambient/indirect debería ocluirse. En la mayoría de escenas la diferencia visual es sutil (el ojo no nota el over-darkening).
- **Cuándo refactorizar:** si emerge complaint del dev sobre "los objetos se ven planos / lavados en zonas con AO" — síntoma de over-darkening. El refactor sería pasar el AO texture al PBR shader como uniform + multiplicar solo `iblContribution + ambient * ao` antes del directional lighting. Hito propio (~2-3h).

**Decisión clave 4 — Half-res AO buffer en lugar de full-res.** Los 2 FBs internos de SSAO (raw + blurred) son la mitad del ancho/alto del scene FB. El composite es full-res.

- **Razón:** convención industria — 16 samples por pixel a full-res es caro (~1080p × 16 samples = 32M texture reads por frame solo para SSAO). El blur 4x4 que viene después disimula la pérdida de resolución del downsample. Filament, Unreal, Godot, todos hacen half-res por default.
- **Trade-off:** en patrones de muy alta frecuencia (líneas finas, texto en el mundo), la resolución de AO puede notarse. No emerge caso de uso real.
- **Revisión:** si el dev pide "AO más definido en bordes finos", agregar opción full-res al SSAOPass. Improbable para una escena de juego típica.

**Decisión clave 5 — Defaults SSAO ON con intensity=1.0.** Mismo criterio que bloom en F2H55.

- **Razón:** engine-grade no toca semánticas de gameplay pero SÍ provee defaults visuales sensatos. SSAO al default añade "peso" a los objetos sin estilo intrusivo. El dev del juego apaga el slider si quiere look plano vintage.
- **Alternativa descartada:** default OFF — descartado por la misma razón que bloom (el dev quería "lo visual que podemos sacar bien" — defaults aspiracionales).

**Bugs UX detectados durante el tour (no scope F2H56, flagueados para F2H57):**

- **Vista SIDE (ZY) ortho dibuja brushes con eje invertido.** Arrastrar izquierda-a-derecha mapea al revés en la matemática del editor. Probable causa: confusión de signo en la conversión screen-coords → world-coords del eje Z en la vista lateral. Investigación dirigida al `OrthoViewportPanel.cpp` cuando lleguemos al hito.
- **Falta "Crear Entidad" button.** El editor no expone un workflow directo para crear entidades — el dev cita Hammer Editor (Source Engine) como referencia: botón "Create Entity" → elegir tipo + importar modelo opcional → editar propiedades en Inspector. Workflow actual obliga a spawn vía Demos del menú Ayuda, lo cual es muleta.
- **Demos como muleta a eliminar.** Por la falta del workflow anterior, los Demos del menú Ayuda son la única forma práctica de poblar una escena nueva. Pendiente eliminarlos una vez exista el workflow real (F2H57). Mantenerlos hasta entonces para que el editor sea usable.

Estos 3 ítems componen el scope de **F2H57 — Workflow de creación de entidades estilo Hammer + fix SIDE ortho + remove demos**. Pivot temporal de la Sub-fase 2.6 (render polish) a UX del editor antes de continuar con color grading (F2H58) / god rays (F2H59).

---

## 2026-05-14: F2H55 cierre — Bloom (glow) + Environment per-scene (apertura Sub-fase 2.6 Render polish)

**Contexto:** primer hito de **Sub-fase 2.6 — Render polish** post-cierre de Sub-fase 2.5 (Diálogos / Inventario / Quests, F2H53). El dev pidió "lo visual que podemos sacar bien" — entre 4 candidatos (bloom / AO / color grading / god rays) eligió bloom por mayor impacto inmediato + scope acotado. Tag `v1.42.0-fase2-hito55`. F2H54 quedó **skip** (laptop-only descartado por divergencia con desktop al cerrar F2H53).

**Decisión clave 1 — No-librería externa para bloom: portar shaders open-source en lugar de adoptar SDK.** Bloom no tiene "lib plug-and-play" en la industria — Unreal Engine, Unity HDRP, Godot 4, id Tech, Frostbite todos lo implementan inline porque debe estar pegado al pipeline del motor (formatos HDR específicos, orden de passes, mip chain de FBs). El algoritmo (downsample con Karis + upsample tent + composite) viene de una presentación de Sledgehammer/Activision 2014 ("Next Generation Post Processing in Call of Duty Advanced Warfare", Jorge Jimenez) — es el estándar de facto desde hace 10+ años. Lo que cambia entre motores es el tuning, no la matemática.

- **Razón:** integrar una lib externa terminaría siendo más trabajo que las ~80 líneas de GLSL. Y "no reinventar" en este dominio significa **portar de referencia open-source comprobada** (Godot MIT, Filament Apache 2.0) — no escribir de cero.
- **Alternativas evaluadas:** (a) **AMD FidelityFX SDK** — descartado para bloom: el SDK trae CAS/FSR/CACAO pero no bloom como módulo discreto. Re-evaluable cuando lleguemos a AO en F2H56 (CACAO sí es código AMD mantenido). (b) **bgfx examples** — sólo reference code, no library. (c) **Adoptar bgfx o Filament completos** — descartado: implicaría reemplazar el motor entero (RHI propio). (d) **Reinventar from scratch** — descartado: alta probabilidad de bug, sin upside.
- **Atribución en código:** headers de los 4 shaders mencionan Godot 4 / Filament + algoritmo COD AW 2014. Documenta que NO es trabajo propio + da pista al próximo agente sobre dónde buscar si necesita modificar.
- **Revisión:** si Godot/Filament evolucionan su algoritmo (ej. dual-Kawase vs mip-chain), re-evaluar el port.

**Decisión clave 2 — Settings de bloom (y futuros polish) per-mapa en `EnvironmentComponent`, NO global.** Extender el componente existente con 4 campos nuevos en lugar de inventar un struct `EnvironmentSettings` separado.

- **Razón:** el motor YA tiene `EnvironmentComponent` con skybox + fog + exposure + tonemap + IBL intensity (F2H15/F2H18). Bloom es semánticamente el mismo dominio ("cómo se ve este mapa"). Sumar un struct paralelo violaría YAGNI + duplicaría serialización + duplicaría UI. Per-mapa permite que la cueva tenga bloom alto + el desierto bajo (mood diferencial).
- **Consecuencia futura:** F2H56 (AO) suma `ssao*` al mismo componente. F2H57 (color grading) suma `colorGrading*`. F2H58 (god rays) suma `godRays*`. El componente crecerá ~12-16 campos más durante Sub-fase 2.6 — aceptable mientras siga siendo conceptualmente coherente. Si emerge presión, split a subcomponente (p.ej. `PostFXComponent`) con migración aditiva.
- **Alternativa descartada:** global setting en `config.json` o `UserSettings` — apaga el caso de uso principal (variedad mood por mapa). Per-cámara override quedó para Sub-fase 3 si emerge demanda (cinemáticas custom).

**Decisión clave 3 — Defaults aditivos en JSON: solo persistir campos que difieren del default.** Cuando `EntitySerializer` escribe el `environment` block, los 4 campos bloom solo aparecen si difieren de su default. El parser lee con `je.value(key, default)` — campos ausentes resultan en el mismo valor que campos presentes con el default explícito.

- **Razón:** mapas `.moodmap` pre-F2H55 round-tripean SIN ensuciarse con los 4 campos nuevos. Sólo los mapas que el dev edita activamente con bloom custom acumulan los campos en disco. Mantiene los diffs de git limpios.
- **Patrón establecido:** mismo enfoque que `ibl_intensity` (Hito 18) — ese campo también se persiste sólo si `!= 1.0`. F2H55 extiende el patrón a 4 campos más.
- **Trade-off:** si el dev EXPLÍCITAMENTE pone `bloomIntensity = 0.6` (igual al default), no se persiste — el .moodmap no refleja la "intención de set". Aceptable mientras el default sea estable. Si emerge presión, refactor a "siempre persistir todos".

**Decisión clave 4 — Cero regresión visual con bloom apagado: si `apply()` falla o intensity=0, post-process lee directo del scene FB.** El `endFrame()` del SceneRenderer mantiene dos rutas:

- **Razón:** F2H55 toca el flujo crítico del frame (post-process). Una regresión silenciosa donde "sin bloom" se ve distinto a "antes de F2H55" sería un bug difícil de detectar. La regla *"con bloom apagado, idéntico al frame pre-F2H55"* es testeable visualmente por el dev.
- **Implementación:** `BloomPass::apply` retorna bool. `endFrame` solo apunta `postProcessSrc` al `m_bloomFb` si el apply retornó true. Si bloom está deshabilitado, no se invoca al pass; `postProcessSrc` queda en `m_sceneFb` directo.
- **Bug descubierto durante tour por NO seguir esta regla en v1:** primera implementación cambiaba `postProcessSrc = m_bloomFb` ANTES de invocar `apply()` — si apply early-returnaba (mip chain no construido), el FB destino quedaba con contenido stale → pantalla negra. Fix aplicado durante el tour del dev.

**Decisión clave 5 — Defaults bloom ON (no OFF).** Cuando el dev crea un proyecto nuevo o agrega `EnvironmentComponent`, bloom arranca enabled con intensity=0.6.

- **Razón:** engine-grade no toca semánticas de gameplay pero SÍ provee defaults visuales sensatos. Igual que el motor arranca con ACES tonemap default (no None) o IBL intensity 1.0 (no 0). El dev del juego que quiera look plano vintage apaga el slider — pero el motor no DEBE servir "indie sin pulir" como default.
- **Revisión:** si emerge complaint del dev sobre que el bloom "interfiere" con su look, agregar flag en UserSettings global para apagar el default.

---

## 2026-05-14: F2H53 cierre — Quest System engine-grade (schema + state machine + tick + Browser/Editor + Lua + HUD + persistencia)

**Contexto:** cierra el Bloque 2 (Quests) del `PLAN_SUBFASE_2_5.md`. Sub-fase 2.5 Bloque 1 (Inventario) ya estaba completo con F2H52; F2H53 abre y cierra el sistema de quests aprovechando las primitivas de F2H48 (dialog vars) y F2H52 (inventory bindings). Tour visual validado por dev: *"funcionó todo"*.

**Decisión clave 1 — Predicates declarativos sobre primitivas existentes (NO sistema de eventos custom).** Los 3 predicate types (`Collect`, `Talk`, `Reach`) se compilan a strings Lua que consumen primitivas YA registradas: `inventory.count('items/x') >= N` (F2H52) y `dialog.has_var('foo')` (F2H48). El motor NO inventó un sistema de eventos paralelo "quest.event_listen(...)".

- **Razón:** engine-grade strict. La cantidad de items en el inventario YA es queryable; las dialog vars YA son el event log del juego. Sumar un tercer mecanismo de eventos sería redundancia + más superficie de bug. El dev expresa cualquier condición complejas con `CustomLua` (escape hatch).
- **Alternativas descartadas:** (a) Sistema de eventos pub/sub propio (`quest.on_event("kill_enemy", ...)`) — descartado: el motor no conoce "enemy"; sería otra capa de game-specific. (b) Predicates como C++ función (`std::function<bool(...)>`) — descartado: requeriría re-compilación cada vez que el dev cambia un predicate. Strings Lua tienen hot-reload gratis.
- **Revisión:** si emerge un caso de uso que el `CustomLua` no cubre cómodamente (ej. "matar 5 enemigos de tipo X en menos de 30s"), bumpear el schema con un cuarto type. La mayoría de quest types en Skyrim/Witcher caen en alguno de los 4 actuales.

**Decisión clave 2 — `LuaEvaluator` + `LuaExecutor` inyectables (NO sol::state directo en el motor).** El `QuestSystem` corre el `tick()` y necesita evaluar predicates + aplicar rewards-como-código-Lua. NO importa sol2 directamente. Tiene 2 `std::function` inyectados que el script host (LuaBindings) instala apuntando a su propia sol::state.

- **Razón:** mismo patrón que `DialogSystem::setEvaluator/Executor` (F2H48). Permite que `mood_tests` testee el QuestSystem headless sin sol2 (los tests inyectan mocks). Mantiene el `engine/quest/` libre de incluir sol2 en su header.
- **Lifetime trap descubierto en F2H53:** los hooks globales (`g_evaluator`, `g_onComplete`, etc.) capturan referencias/punteros a la `sol::state` del ScriptSystem. Si la state muere antes que los hooks (orden default de destrucción), al terminar el proceso `~std::function` destruye una `sol::function` colgante → `lua_unref` sobre VM muerto → SIGSEGV. **Fix**: llamar `QuestSystem::clearHooks()` + `Inventory::Hooks::clearAll()` ANTES de `m_scriptSystem.reset()` en `~EditorApplication` y `~PlayerApplication`. Mismo bug pattern que F2H52 J — ahora documentado como invariante de shutdown.

**Decisión clave 3 — Identificación por path lógico en persistencia (NO por id).** El schema `.moodsave` v3 guarda `quests[].path` (`"quests/q_fetch.moodquest"`) en lugar del `QuestAssetId` numérico.

- **Razón:** paridad con `SavedInventory` de F2H51 I. Los IDs son volátiles entre runs — dependen del orden de `loadQuest` del AssetManager (primero en cargarse = id 1, etc.). El path es estable mientras el archivo exista.
- **Consecuencia:** si el dev borra un `.moodquest` entre save y load, ese quest aparece como "huérfano" en log warn y se skipea silencioso en restore. NO crash. Mismo trade-off que `SavedInventory`.

**Decisión clave 4 — `QuestSystem::restore(...)` separado del lifecycle normal.** En lugar de re-usar `start()` + setear progreso, hay una API dedicada `restore(id, state, objectiveDone, am)` que:
- Inserta directo al `g_active` sin chequear validez de transición.
- NO dispara hooks (es restauración, no transición).
- Trunca/padea `objectiveDone` si el asset cambió entre save y load.

- **Razón:** semánticamente diferente. `start()` es "el jugador empieza un quest" (dispara cinematic, SFX, etc. via hooks); `restore()` es "el game state se está reconstruyendo" (silencioso). Mezclarlos llevaría a callbacks disparándose al cargar partida — UX espantosa.
- **No expuesto a Lua:** sólo callable desde C++ (`SaveLoad::load → applyLoadedSave`). Si el dev hace `quest.start("...")` desde Lua, dispara los hooks normalmente.

**Decisión clave 5 — Re-start permitido tras `Failed`, bloqueado tras `Active`/`Complete`.** Cuando el dev llama `quest.start(path)` y el quest ya está registrado, el comportamiento depende del state actual.

- **Razón:** retry tras fallar es UX común en RPGs ("¡puedes intentar la misión otra vez!"). Re-start tras complete sería duplicar progreso (one-shot por design). Active doble es no-op evidente.
- **Revisión:** si emerge un caso de uso para "repeatable quest" (daily quests, side jobs), agregar flag `repeatable` al schema y permitir re-start tras Complete cuando esté activado.

**Decisión clave 6 — Tracker HUD reutiliza el widget `objective_text` (NO nuevo widget).** El widget `drawObjectiveText` (F2H41) ahora tiene 2 modos: Quest Tracker (preferido si hay tracked) vs Legacy text (fallback F2H41).

- **Razón:** compatibilidad con scripts existentes que usen `hud.set_widget("objective_text", false)` o `hud.setObjective("...")`. El nombre del widget en el registry queda igual; sólo cambia el render interno según el state del QuestSystem.
- **Alternativa descartada:** widget nuevo `quest_tracker` separado — implicaría que el dev tenga DOS widgets que pueden estar enabled simultáneamente y compitiendo por la misma posición top-left.

**Decisión clave 7 — Quest Log panel como widget separado con toggle por tecla (NO ventana ImGui::Begin standalone).** El panel se dibuja con `ImDrawList` directo en el viewport overlay (paridad con `inventory_panel` de F2H52). Toggle por **J** (convención RPG Skyrim/Witcher "Journal").

- **Razón:** consistencia con los demás widgets HUD. No queremos ventanas ImGui flotantes en Play Mode — el HUD es overlay no-interactivo via mouse capture. Click sobre quest = mouse no capturado en Play (porque el panel está en `widget_enabled["quest_log_panel"]` que el dev togglea con J y respeta `isInputBlocked`).
- **Limitación v1:** sin scroll. Si hay más de ~10 quests visibles, los siguientes quedan fuera del panel. Migrar a ImGui::Begin + ScrollRegion cuando emerja necesidad.

---

## 2026-05-12: F2H52 cierre — Inventory runtime (pickup + HUD + Lua + container split + renderer override)

**Contexto:** cierra el Bloque 1 (Inventario) del `PLAN_SUBFASE_2_5.md`. F2H51 entregó autoría + state + persistencia; F2H52 cierra el lado runtime. Sub-fase 2.5 Bloque 1 ✅ completo.

**Decisión clave 1 — Hooks Lua único callback por evento (no veto, sí after-success).** Los 3 hooks (`on_pickup` / `on_drop` / `on_use`) se sobrescriben con warn si el dev registra dos veces. Motor completa la operación primero (add/remove) y DESPUÉS dispara el callback como "after-success notification". El dev NO puede vetar — si necesita veto, hace `inventory.has` check antes de la acción.

- **Razón:** simplicidad > composability en v1. Patrón identico a `DialogSystem::setEvaluator/Executor` (F2H48.1). Si emerge necesidad de N callbacks, sumar `add_listener(...)` aparte sin deprecar el setter único.
- **Trade-off aceptado:** el dev tiene que decidir en su Lua dónde meter toda su lógica de pickup (en un solo callback). Para hot reload, el sol::function vieja queda colgando hasta que el nuevo script se cargue + se re-registre.

**Decisión clave 2 — `inventory.use(entity, path)` NO auto-consume.** El motor invoca el hook `on_use(entity, path)` y vuelve. Es el callback del dev quien decide si llama `inventory.remove(...)` adentro (caso poción) o no (caso arma equipable). Idem `inventory.drop` desde el HUD widget: motor remueve + spawnea pickup + invoca `on_drop` notificación.

- **Razón:** engine-grade strict. El motor no conoce "poción se consume al usar". El sample `inventory_demo.lua` muestra el patrón: `USE_HANDLERS` map por path con `health_potion` haciendo `inventory.remove(ent, path, 1)` adentro, y `iron_sword` NO (caso equipar).
- **Alternativa rechazada:** flag `consume_on_use` en el `.mooditem`. Hubiera obligado al motor a saber qué hacer post-consume (¿devuelve un wrapper vacío? ¿restaura HP automáticamente?). Más simple ceder al script.

**Decisión clave 3 — `Inventory::spawnPickupInWorld` helper compartido (3 callsites → 1).** Misma lógica de "crear entity con Transform + MeshRenderer + Trigger 0.5³ + ItemPickupComponent" se necesitaba en: Lua binding `inventory.spawn_pickup`, drop del HUD widget, drag-drop del Item Browser al viewport. Extraída a `engine/inventory/ItemSpawn.h/cpp`.

- **Razón:** evitar drift entre los 3 callsites cuando el schema del pickup evolucione. Mesh derivado del item: `model_path` → loadMesh + createMaterialsForMesh; sino `icon_path` → cubo + textura icon como albedo; sino default. El dev puede actualizar el helper en un lugar.
- **Tests:** `test_item_spawn.cpp` con 8 tests cubre creación exitosa, qty custom, scene/assets null, item inexistente, Trigger halfExtents, multi-spawn.

**Decisión clave 4 — Container split visualmente FlatList para ambos lados, sin importar el `mode` del state.** Cuando `hud.open_container(entity)` se llama, el widget renderea 2 columnas (player izq, container der) usando FlatList visual. La layout config del container's `InventoryComponent.state.mode` se IGNORA en el render del split.

- **Razón:** el loot UX (cofre/comercio/drop pile) rara vez quiere el grid spatial del container. Casos como "open chest in RE4 → see chest's grid" son raros; lo común es "open chest → see flat list of items, drag to player".
- **Limitación documentada:** si el dev quiere split visual respetando el mode del container, registra su renderer custom via `inventory.set_renderer`. El motor expone primitivas; el dev decide presentación.

**Decisión clave 5 — `HudState::container_open` bool + `container_target` u32 (NO sentinel value).** Descubierto bug en testing: `entt::entity` 0 es un handle VÁLIDO (la primera entity creada). No podemos usar `container_target = 0` como sentinel "no container". Refactor a dos campos: `bool container_open` + `u32 container_target`. El widget chequea ambos.

- **Razón:** sin el bool, el primer `hud.open_container(player)` (caso degenerado pero válido) hubiera sido indistinguible de "no hay container abierto". El bool es cero overhead + máxima claridad.
- **Pattern replicable:** cualquier campo que store un entt handle como "opcionalmente seteado" debería usar un bool paralelo, no un valor sentinel.

**Decisión clave 6 — `inventory.set_renderer(callback)` cede TODO al dev (modes + tooltip + context menu skipeados).** Cuando el dev registra un renderer custom, el motor no dibuja nada del default — el callback recibe `(player, container_or_nil)` y se encarga.

- **Razón:** engine-grade extremo. No tendría sentido mezclar "el motor dibuja la mitad y el dev la otra". Cede o no cede — sin estados intermedios.
- **Limitación v1 documentada:** sin bindings ImGui mínimos en Lua, el callback solo puede dibujar usando bindings del dev (UI custom Dead Space-style). Cuando emerja necesidad de devs sin sus propios bindings → hito de "bindings ImGui mínimos para Lua".

**Decisión clave 7 — Drop horizontal (forward proyectado al plano XZ), no diagonal.** Primera versión hizo `cameraPos + cameraForward * 1.8`. Si el jugador miraba hacia abajo (cosa común al ver su propio inventario), forward apuntaba al piso → item atravesado. Fix: proyectar al plano horizontal (`forward.y = 0` + normalize) antes de multiplicar. Y offset -1.1 desde camera height.

- **Razón:** UX consistente independiente del pitch de la cámara. El jugador SIEMPRE ve el item caer enfrente, mire donde mire.
- **Alternativa rechazada:** raycast contra el piso para apoyar el item exactamente al nivel. Sobre-engineering para v1; si el piso es irregular, el item flotará un poco — aceptable para un sistema de inventario, no es un physics-driven drop.

**Decisión clave 8 — Bug fix descubierto en tour M: `MousePos = -FLT_MAX` machacado cada frame.** Código viejo de F2H41 forzaba `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)` cada frame con condición `Play && !paused`. La condición NO consideraba inventory_panel abierto, así que cuando el dev abría Tab durante Play, el cursor era liberado por updateCameras pero machacado por beginFrame — ImGui no recibía mouse, ni siquiera el botón Stop respondía.

- **Fix:** nuevo helper `GameState::isInputBlocked()` returns `paused() || widget_enabled["inventory_panel"]`. Reemplaza el chequeo de `!paused()` en beginFrame + el de `paused()` en updateCameras de editor + player. Generalizable: cualquier overlay UI futuro (trade menu, quest log, etc.) se suma al helper sin tocar los 4 callsites.
- **Lección aprendida:** cuando un fix lateral (F2H41) cambia comportamiento global del input, dejar siempre un helper centralizado en lugar de inlinear el chequeo. El inlining causó que F2H52 no anticipara la interacción.

---

## 2026-05-12: F2H51 cierre — Inventario engine-grade (autoría + state + persistencia)

**Contexto:** Bloque 1 del `PLAN_SUBFASE_2_5.md`, sexto hito real de Sub-fase 2.5. F2H50 cerró el flow narrativo end-to-end con NPC tangible; F2H51 abre el sistema de inventario (Bloque 1 del plan macro, pedido del dev: *"recuerda la idea es crear una base solida para que a futuro cualquiera pueda crear su sistema de conversaciones, misiones, o inventario y asignar a modelos 3D"*). Split editor/runtime aplicado: F2H51 = autoría + state + persistencia; F2H52 = runtime (pickup + HUD + Lua).

**Decisión clave 1 — Engine-grade strict: motor sin semántica hardcoded de "weapon"/"potion"/"armor".** El schema `.mooditem` tiene `tags` (`std::vector<std::string>` libre) + `stats` (`std::map<string, float>` libre). El motor NO interpreta ni "damage" ni "heal_amount" — solo los almacena. Cada dev del juego define su propia ontología.

- **Razón:** principio #2 del marco estratégico de Sub-fase 2.5 (*"Sin semántica hardcodeada de gameplay"*). Devs futuros de juegos distintos (RPG/shooter/walking sim/etc) configuran su propia semántica sin recompilar.
- **Tensión con UX:** el dev nuevo no sabe qué stats ponerle a un arma. Resuelto en Bloque K post-validación con **plantillas** (dropdown "Plantilla" en `+ Nuevo Item` con `Vacío`/`Arma`/`Poción`/`Armadura`/`Quest item`/`Objeto`). Las plantillas son presets del editor (`applyTemplate` en anonymous namespace de `ItemBrowserPanel.cpp`) — el motor sigue sin conocerlas. Como punto de partida, no semántica.
- **Alternativa rechazada:** hardcodear categorías como enum en el schema. Hubiera roto principio #2 + obligado a tocar C++ cada vez que un dev quiere una categoría nueva ("alchemy_reagent", "spell_focus", "trinket"). Imposible engine-grade.

**Decisión clave 2 — Split editor/runtime F2H51 vs F2H52 (mismo patrón F2H47→F2H48).** F2H51 cierra: ItemAsset schema, AssetManager loader, InventoryState pure logic, InventoryComponent, Item Browser, Property Editor, Inspector section, persistencia `.moodmap`, workspace "Gameplay". F2H52 hará: ItemPickupComponent, HUD widget, Lua bindings, integración con DialogScriptHost.

- **Razón:** checkpoint natural de validación del schema antes de atarlo al runtime. Si el runtime emerge con requisitos extras del schema, bumpear `.mooditem` v1→v2 cuesta poco antes de tener saves reales. Hitos mantenibles (~10-12h cada uno) en lugar de uno gigante (~20h).
- **Validado en Bloque K:** el dev pudo probar el flow end-to-end de autoría (crear, editar, persistir items + inventarios en entidades + roundtrip save/load) sin necesitar el runtime de pickup. Validación temprana.

**Decisión clave 3 — 3 layout modes (FlatList / Grid2D / EquipmentSlots) en v1, NO solo FlatList.** Resistí la tentación de hacer solo FlatList para v1 y agregar los otros cuando emerjan.

- **Razón:** PLAN_SUBFASE_2_5 sección 1.3 los lista como fundacional. Implementar solo FlatList sería deuda inmediata cuando F2H52 HUD widget necesite Grid2D estilo Resident Evil para el caso "inventario tetris" + EquipmentSlots para el caso "RPG slots de armadura". Costo extra ~3-4h vale la pena.
- **Default:** FlatList max_items=20. Universal, se entiende sin contexto del género del juego.
- **Trade-off polimorfismo simple:** la lógica de `add`/`remove`/`placeAt` tiene 3 ramas (switch sobre `LayoutMode`). Encapsulado en `InventoryState`. Si emerge complejidad real, refactor a `std::variant` + visitor.

**Decisión clave 4 — Paths-no-ids en persistencia `.moodmap` (mismo patrón F2H50 AnimatorComponent).** El `SavedInventoryEntry` persiste `itemPath` (string lógico) y NO `itemId` (`ItemAssetId`).

- **Razón:** los IDs no son estables entre sesiones (dependen del orden de loads del AssetManager). Persistir el path lógico (`assets.itemPathOf(id)`); al cargar, `assets.loadItem(path)` lo re-resuelve y reconstruye el `Entry`.
- **Bump aditivo:** mapas pre-F2H51 cargan sin componente (`std::optional<SavedInventory>` ausente — no se auto-agrega). Sin regression.
- **Limitación conocida:** si el dev borra un `.mooditem` entre save y load, el path persistido pierde info (queda como `__empty_item`). Mismo patrón F2H50 — sin caso real todavía.

**Decisión clave 5 — `InventoryComponent` agregado al whitelist de `SceneSerializer::save`.** Antes el whitelist solo incluía Mesh/Light/RigidBody/Environment/Script/Particle. Una entidad con solo `InventoryComponent` no se persistía.

- **Razón:** principio engine-grade — un entity puede ser un chest/container/vendor SIN mesh visible (logic-only entity). No asumir que el inventario implica un visual.
- **Trade-off:** abre la puerta a entidades "fantasmas" (logic-only) en el .moodmap. Acepto — es la dirección correcta. Si emergen issues (entidades huérfanas), se filtran en futuro.

**Decisión clave 6 — `slot_size > 1x1` IGNORADO en v1 del `InventoryState`.** El schema persiste `slot_size {width, height}` pero la lógica de `add`/`placeAt` para Grid2D asume cada item ocupa 1 cell.

- **Razón:** el packing rectangular real (Resident Evil 4 inventory style) requiere algoritmo de "bin packing" que no es trivial. Implementarlo bien suma ~3-4h adicionales para un caso de uso que ningún demo necesita todavía. YAGNI v1.
- **Schema persiste el campo** para roundtrip — cuando v2 lo implemente, no rompe assets existentes. Documentado en hint del Property Editor: *"Sólo se respeta en layout grid_2d. v1 ignora width/height (cada item ocupa 1 cell)."*

**Decisión clave 7 — 3 fixes UX post-validación del dev (Bloque K).** El dev validó visualmente el flow + reportó 4 issues. Decisiones:

- **Issue 1 — checkbox "Usar i18n key" confuso:** el dev no entendía qué hacía. **Fix:** tooltip al hover sobre los checkboxes name/description que explica el use-case (juego multi-idioma vs mono-idioma). Mantengo el checkbox (el motor soporta i18n, eso es engine-grade); no lo escondo porque sería romper esa capability.
- **Issue 2 — i18n del Property Editor:** los strings UI ya se traducen (es.json/en.json). El dev preguntó si traducción automática (Google Translate / DeepL). **Respuesta diferida:** las grandes empresas (Riot/Blizzard/Nintendo) NO usan auto-translation para texto in-game — usan plataformas pro (Crowdin/Lokalise/Phrase) con traductores humanos. Para MoodEngine queda como hito propio "Localization Pipeline" en Sub-fase 3 (~6-8h): dev escribe en su locale → editor auto-genera la i18n key → UI lista keys sin traducir para que un humano complete los otros locales. NO entra en F2H51. Agregado a `PENDIENTES.md`.
- **Issue 3 — InputFloat `%.3f` mostraba "20.000" leído como "20000":** el separador decimal de Windows en español es coma, y `%.3f` siempre rellena 3 decimales. **Fix:** `%g` smart-format ("20" si entero, "12.5" si decimal real).
- **Issue 4 — dev pedía categorías predefinidas tipo "armas/pociones/atuendos":** ver Decisión 1. Resuelto con plantillas en `+ Nuevo Item` que pre-pueblan tags + stats típicos. Engine-grade preservado.

**Alternativas descartadas (no entran en F2H51)**:
- **Iconos en cards del Item Browser:** requieren wireup AssetManager texture + ImTextureID handle. Costo ~1h. Diferido — el dev no lo pidió y es nice-to-have.
- **3D preview del modelo en Property Editor:** requiere Bloque 0.2 del PLAN_SUBFASE_2_5 (widget reusable de render-to-texture con cámara orbital). Diferido a hito propio cuando emerja Material Editor pro.
- **Drag-source desde Item Browser → spawn pickup en Viewport:** requiere `ItemPickupComponent` (F2H52). Lógico que vaya juntos.

**Revisión:** si en F2H52 emerge un requisito que invalida alguna de estas decisiones (ej. el runtime de pickup necesita un campo nuevo en el schema), bumpear `.mooditem` v1→v2 con fallback en `fromJson`.

---

## 2026-05-11: F2H50 cierre — Demo narrativa end-to-end + persistencia AnimatorComponent + regen materiales auto

**Contexto:** F2H49 cerró el pipeline de animaciones standalone (FBX anim-only). F2H50 cierra el Bloque 2.5 del `PLAN_SUBFASE_2_5.md` atando esas animaciones al sistema de diálogo + character controller para validar el flow narrativo end-to-end con un NPC tangible (dev: *"que sentido tiene crear un sistema de dialogo, sino tenemos a quien asignar"*).

**Decisión clave 1 — Auto-attach de sibling `anim_*.fbx` al spawn de mesh skinneado:** cuando el dev dropea un FBX con skeleton en el viewport, el motor escanea la carpeta padre del mesh y enchufa todos los `anim_<alias>.fbx` siblings al `AnimatorComponent.externalClips` automáticamente, defaulteando `clipName = "idle"` si existe ese alias.

- **Razón:** sin esto, el dev tiene que drag-drop manualmente cada clip desde el Inspector tras spawnear un rig. Para un demo de 3-6 clips por personaje es tedioso. La convención de filename (`anim_*`) ya estaba documentada en `assets/characters/README.md`.
- **Generalización (no exclusivo de Mixamo):** funciona con cualquier rig (`assets/heroes/hero/hero.fbx` + `anim_*.fbx` siblings). Convención por filename, no por carpeta.
- **No-op para FBX sin siblings (Fox.glb, Kenney props):** la lista queda vacía → comportamiento Hito 19 preservado (primer clip embedded).
- **Default `clipName = "idle"`:** asume convención que "idle" es la pose neutral. Para chars sin idle, queda `""` (primer embedded, típicamente T-pose para Mixamo "With Skin").

**Decisión clave 2 — Condición de auto-add de AnimatorComponent al spawn relajada a solo `hasSkeleton()`:** antes era `hasSkeleton() && !animations.empty()`. Ahora cualquier rig con esqueleto recibe AnimatorComponent + SkeletonComponent.

- **Razón:** el log existente `"(skinned + animator)"` ya usaba la condición floja (visible incluso para meshes sin embedded anims), creando inconsistencia entre log y comportamiento. Más importante: con el auto-attach (decisión 1), un rig sin embedded anims + con sibling `anim_*.fbx` puede igual usar esos clips externos. La condición vieja le negaba el AnimatorComponent y perdía esos clips.
- **Trade-off:** rigs con skeleton + sin clips de ningún tipo tienen AnimatorComponent inerte. Cero costo runtime (el AnimationSystem retorna early si no hay clip activo).

**Decisión clave 3 — `SavedAnimator` agrega persistencia completa del `AnimatorComponent` en el `.moodmap`:** bump aditivo del schema. Persiste `clipName`, `speed`, `playing`, `loop`, y la lista `externalClips` como pares `{alias, path}`.

- **Razón:** pre-F2H50 el AnimatorComponent NUNCA se serializaba — el SceneLoader auto-agregaba uno con defaults (`clipName=""`, `playing=true`, `loop=true`) cuando el mesh tenía skeleton + embedded anims. Eso significaba que editar `anim.speed = 2.0` en el Inspector, save, load → speed back to 1.0. Más crítico: los `externalClips` agregados via drop o via inspector se perdían en cada save.
- **Path persistido vs ID:** los `AnimationClipAssetId` no son estables entre sesiones (dependen del orden de loads del AssetManager). Persistimos el path lógico (`assets.animationClipPathOf(clipId)`); al cargar, `loadAnimationClip(path)` lo re-resuelve. Mismo patrón que MeshRenderer.meshPath.
- **`externalBindCache` NO se persiste:** runtime state que `AnimationSystem` regenera al primer evaluate via `bindClipToSkeleton`. Mismo criterio que `cachedDialogId` (F2H48.1) — IDs cacheados no sobreviven sesiones.
- **`time` NO se persiste:** siempre arranca en 0 al cargar. Convención "respawn" de motores 3D. Si emerge necesidad de "remember anim time" (ej. cinemática pausable), agregar como flag.
- **Fallback path para mapas pre-F2H50:** si el JSON no trae `animator`, el SceneLoader cae al auto-add con primer embedded — same behavior as Hito 19. Cero regression.

**Decisión clave 4 — Regen lazy de materiales auto en SceneLoader cuando el slot tiene path vacío:** descubierto al validar el flow save→load del demo F2H50. Los X/Y Bot aparecían magenta tras un roundtrip.

- **Causa:** el EntitySerializer (línea ~86) nukea cualquier path que empiece con `__` a string vacío (handling de paths internos). Eso incluye los `__runtime#<id>` que `createMaterialsForMesh` asigna a materiales auto-generados con diffuse color de F2H49.1. El SceneLoader al ver path vacío caía a `missingMaterialId()` (magenta).
- **Fix:** en SceneLoader, cuando un slot tiene path vacío Y la mesh tiene auto-materials disponibles, regenerar lazy via `createMaterialsForMesh(meshId)`. Eso preserva textures embedded del FBX (Fox.glb) + diffuse colors (X/Y Bot) sin perderlos en el roundtrip — y sin tener que serializar el material como JSON inline (overhead).
- **Determinismo:** `createMaterialsForMesh(meshId)` es determinístico para un mesh dado — el resultado es función de los `materialAlbedoTextures` + `materialAlbedoColors` del MeshAsset. Reload del mismo mesh produce los mismos materiales.

**Decisión clave 5 — Demo scene minimalista (sin walls CSG):** el handler genera SOLO el NPC con sus componentes. Sin floor brushes, sin walls, sin lighting custom.

- **Razón:** filosofía engine-grade — el `.moodmap` es DATA. Hardcodear walls en el generator obliga al dev a editarlas si quiere distinta arquitectura. Mejor: minimal scaffold que el dev decora editando el .moodmap como cualquier mapa. La tile floor implícita es suficiente para el demo.
- **Alternativa descartada:** room rectangular con 4 walls + floor brush + RigidBody Static. Hubiera requerido también integración con el "compile map" flow para que las walls colisionen. Scope creep para el demo.

**Decisión clave 6 — Helper `ensureDemoIntroDialogExists` extraído del handler F2H47:** refactor del `processSpawnDialogDemoRequest` para reuso entre "Cargar diálogo demo" (que abre el DialogEditor) y "Cargar demo narrativo" (que lo usa como dependencia del NPC, sin side effects de UI).

- **Razón:** evitar copy-paste de ~50 LOC de generación de `.mooddialog`. El helper devuelve `bool` (existía ya / se creó OK / falla) para que ambos handlers tomen decisiones de continuar.

**Alternativas descartadas:**

- **Walls CSG + RigidBody Static en el demo (room cerrada):** scope creep + complica el flow (los brushes no colisionan automáticamente; necesitan "compile map"). Sin ganancia para el demo.
- **Animator state machine / blending en F2H50:** prematuro. El demo solo necesita un loop de idle. Locomotion blending (idle ↔ walk ↔ run) emerge cuando el player se mueva como char skinneado, no como capsule.
- **Serializar materiales runtime como JSON inline:** considerado para fixear la magenta de X/Y Bot. Descartado en favor de la regen lazy — JSON inline duplica datos del FBX (que ya tiene la info en `aiMaterial`), y obliga a versionar el formato del material en el `.moodmap`. La regen es declarativa y determinística.
- **Preservar path original cuando un clip standalone está missing al save:** requiere extender `AnimatorComponent.externalClips` de `pair<alias, id>` a `tuple<alias, path, id>`. Limitación que aparece solo si un asset referenciado fue borrado entre save y load. Sin caso real todavía — backlog si emerge.
- **Drop-on-viewport para anim clips:** nice-to-have UX. Drag clip → soltar sobre la entidad bajo el cursor → add a su `externalClips`. Requiere extender picking del viewport para detectar entity durante drag activo. Backlog.

**Condiciones de revisión:**

- Si emerge un caso donde el dev quiere "remember anim time" entre save/load (ej. cinemática que pausa + retoma), agregar `time` al SavedAnimator.
- Si la convención `anim_<alias>.fbx` se vuelve restrictiva (ej. dev quiere `walk_north.fbx` sin prefijo), agregar una segunda heurística (file inspection para "es anim-only") como complemento.
- Si los rigs futuros tienen embedded anims que el dev quiere usar EN LUGAR de siblings con mismo alias, agregar un toggle "embedded prioritario" o pedir al dev que use aliases distintos para los siblings.

---

## 2026-05-11: F2H49 cierre — Animaciones standalone (FBX anim-only) + bind pass + tab Animations + Inspector external clips

**Contexto:** el plan original de F2H49 era "Demo characters Mixamo + escena narrativa completa" (Bloque 2.5 del `PLAN_SUBFASE_2_5.md`). Al intentar cargar los `*.fbx` de Mixamo descargados a `assets/characters/`, descubrimos que el motor no soportaba clips de animación sin malla adjunta (caso "Without Skin" de Mixamo, 1 FBX por clip). Sin esto el NPC se quedaba congelado en T-pose durante el diálogo. F2H49 se re-scopea a habilitar el pipeline standalone; la demo narrativa pasa a F2H50.

**Decisión clave 1 — Cache externo (`externalBindCache` en `AnimatorComponent`) vs mutar `track.boneIndex`:** el bind pass calcula `outRemap[i] = skel.boneIndex(clip.tracks[i].boneName)` para cada track. El remap NO se escribe sobre `clip.tracks[i].boneIndex` (que queda permanentemente en `-1` para clips standalone) — se guarda en un `unordered_map<AnimationClipAssetId, vector<int>>` por entidad.

- **Razón vs mutar el clip:** mutar haría el clip skeleton-específico. Caso de uso central de F2H49: el mismo `anim_idle.fbx` se reusa entre player y npc (mismo `mixamorig:*` naming pero esqueletos físicamente distintos en memoria). Si mutamos el primer bind contamina al segundo personaje.
- **Razón vs vivir en una global por path:** el cache vive en el componente porque la invalidación es por-entidad (si una entidad swappea su mesh con un skeleton incompatible, solo SU cache es inválido). Cache global por clip-asset no podría discriminar.
- **Trade-off aceptado:** cache duplicado entre N entidades con mismo (clip, skeleton). Despreciable — 1 entry × ~50 ints × N entidades es trivial.
- **Invalidación:** responsabilidad del consumidor cuando swappea mesh (`anim.externalBindCache.clear()`). El runtime no detecta automáticamente — chequear `bones.size()` mid-frame por cada entidad sería caro y el caso de mesh-swap mid-frame es raro.

**Decisión clave 2 — 2 funciones libres en `AnimationClip.h` (`bindClipToSkeleton` + `evaluateClipWithRemap`) vs nuevo método en `AnimationClip`:** las funciones quedan como inline en el namespace, no como miembros del struct.

- **Razón:** mantener `AnimationClip::evaluate()` con su contrato Hito 19 intacto (lee `track.boneIndex` interno). Agregar un parámetro opcional `remap` mutaría su firma y obligaría a fixear callers downstream.
- **Bonus:** las funciones libres siguen el patrón existente de `samplePosition/sampleRotation/sampleScale` del mismo header — naturalmente extensible.
- **Header-only:** ambas funciones son inline, sin dependencias nuevas. Testeables en `mood_tests` sin enlazar `AnimationSystem`.

**Decisión clave 3 — `resolveActiveClip` devuelve `ClipResolution { clip*, remap* }` en lugar de mutar `AnimatorComponent`:** la función signature pasa de `const AnimationClip*` a un struct con 2 punteros (clip + remap opcional).

- **Razón:** keep `update()` con un único punto de decisión sobre qué `evaluate*` llamar. Si retornara solo el clip, habría que duplicar el lookup del remap en `update()`.
- **`remap=nullptr` semantica:** "este es un clip embedded, usá el `evaluate()` legacy". `remap=&cacheEntry` → "este es standalone, usá `evaluateClipWithRemap`". Reglas claras.

**Decisión clave 4 — Drop target en Inspector, NO en Viewport:** el `MOOD_ANIMCLIP_ASSET` payload se acepta solo desde el botón "Arrastrá un clip aquí" dentro del Inspector.

- **Razón:** un clip standalone no es entidad espacial — no se puede "spawnear" en el mapa. Necesita un personaje al que animar. Drop en viewport no tiene semántica clara (¿crear entity nueva? ¿asignar a la entity bajo el cursor?).
- **Alternativa diferida:** drop sobre la entidad bajo el cursor → add a `externalClips` de esa entity. Es nice-to-have UX, queda en backlog. Requiere extender el picking del viewport para detectar entity durante drag activo.

**Decisión clave 5 — Alias auto-derivado del filename con strip `anim_` prefijo:** al droppear `characters/player/anim_walk.fbx`, el alias defecto es `walk` (no `anim_walk` ni el path completo).

- **Razón:** el alias es lo que el gameplay/scripts usan para pedir reproducción (`animator.play("walk")`, `clipName = "walk"`). Mantenerlo corto y semántico mejora ergonomía. La convención `anim_<accion>.fbx` está documentada en `assets/characters/README.md`.
- **Defensa anti-colisión:** si el alias defecto ya existe en `externalClips`, NO se agrega (tooltip "alias ya en uso"). El user puede renombrar el conflicto primero.

**Decisión clave 6 — Filtro `anim_*.fbx` en tab Meshes (convención de nombre, no análisis de contenido):** la heurística para separar meshes de animaciones es el prefijo del filename, no inspeccionar el FBX.

- **Razón:** análisis de contenido (chequear si `aiScene::mNumMeshes==0`) sería más robusto pero requiere abrir el archivo cada scan — caro y se ejecuta varias veces durante el ciclo de vida del editor. Convención de nombre es O(1) sobre el path.
- **Trade-off aceptado:** un FBX sin animaciones llamado `anim_static.fbx` aparecería incorrectamente en Animations. El nombre es contractual con la convención del README — devs que ignoren la convención son responsables.

**Decisión clave 7 — `AI_SCENE_FLAGS_INCOMPLETE` ignorado en el standalone loader:** el chequeo original `if (... AI_SCENE_FLAGS_INCOMPLETE != 0 ...) abort()` se removió.

- **Razón:** assimp setea ese flag para FBX "Without Skin" porque no hay geometría — pero las animaciones están bien parseadas. El flag es informativo, no error. El check causó que los 6 clips iniciales devolvieran `[0 tracks, 0.00s]`. Quitar el check fue suficiente — `scene == nullptr` y `mRootNode == nullptr` siguen siendo guards reales.
- **Documentado in-line** en el cpp para que nadie reintroduzca el chequeo "porque el mesh loader lo tiene".

**Decisión clave 8 — Shipping de X/Y Bot demo rigs en el repo + `.gitignore` con excepciones:** los rigs Mixamo X Bot + Y Bot + sus 6 clips (~7.5 MB) se commitearon al repo.

- **Razón:** out-of-the-box working characters. Un dev clonando el repo tiene personajes funcionales sin tener que registrarse en Mixamo + descargar manualmente. El motor "tiene contenido" desde el primer clone.
- **`.gitignore` mantiene regla genérica `**/*.fbx`** (los FBX "With Skin + textures embedded" pueden pesar >100 MB, sobre el límite de GitHub) **pero exceptúa** `!assets/characters/player/*.fbx` y `!assets/characters/npc/*.fbx`. Devs que dropeen sus propios FBX en `hero/`, `enemy/`, etc., siguen gitignored.
- **Licencia:** X Bot / Y Bot son redistribuibles bajo el uso libre de Mixamo. README documenta esto.

**Alternativas descartadas:**

- **Texturas embedded del FBX al cargar:** los X/Y Bot se ven magenta sin texture map. Limitación pre-existente del `MeshLoader` (no extrae `aiScene::mTextures`). Queda en PENDIENTES — ataque cuando emerja necesidad real o cuando F2H50 lo pida visualmente.
- **Persistencia de `externalClips` en `.moodmap`:** deferred. Ningún demo lo usa todavía; F2H50 lo va a empujar cuando el NPC viva en escena persistida.
- **Animation state machines / blending (clip A → clip B con cross-fade):** scope creep para F2H49. F2H50/F2H51 si emerge necesidad real (ej. locomoción idle ↔ walk ↔ run).
- **`useRootMotion` flag por clip:** todos los clips de F2H49 son In Place (locomoción controlada por `CharacterController`). Si emerge cinemática o finishers con root motion, agregar como flag opcional por clip.
- **Bumping schema version de `.moodmap`:** `AnimatorComponent` no se serializa todavía con los campos nuevos. Cuando se sume persistencia (F2H50+), bumpear schema `.moodmap` aditivo.

**Condiciones de revisión:**

- Si el bind pass se vuelve hot path (N personajes × M clips × K frames), considerar skeleton fingerprinting (bone count + hash de nombres) para detectar mesh swap sin pedir al consumidor que lo gestione.
- Si emerge un caso donde el alias debería resolver dinámicamente (ej. múltiples versiones por género/edad: `walk_male`/`walk_female`), considerar un nivel de indirección — pero antes verificar si el caso real lo necesita.
- Si Mixamo decide cambiar la convención `mixamorig:*` o agregar prefijos extra, la búsqueda por `boneName` cae sin warning (todos los tracks quedan `-1`). Agregar un warn de "0 tracks bindeados" después del primer evaluate sería defensivo.

---

## 2026-05-10: F2H48.1 cierre — Serialización DialogComponent + DialogScriptHost (condition_lua reales pre-F2H49)

**Contexto:** tras cerrar F2H48 listé 4 deudas como "diferidas" (inyección hooks Lua reales, persist dialogVars en save, mouse-clic en choices, persist DialogComponent en .moodmap). El dev preguntó: *"Y NO DEBERIAMOS HACER ESTO ANTES DE LO DE LA DEMO DE MIXAMO?"*. Triage honesto en respuesta identificó 2 bloqueantes para una demo F2H49 creíble (serialización + hooks Lua) y 2 YAGNI legítimos (save vars, mouse-clic). El dev confirmó scope completo (B): *"me gustaria que la demo sea completa como la B"*. F2H48.1 es mini-hito (~3h, sin tag mayor — bump patch a `v1.36.1`).

**Decisión clave 1 — Sol::state dedicada del host (NO reusar la per-entity de ScriptSystem):** nueva `DialogScriptHost::g_state` (singleton namespace). Bindings minimal: `dialog` (vars + read-only state) + `hud` (interact_prompt + getters HP/mag/reserve) + `log`. Sin `self`, sin `physics`, sin `engine.exposed`.

- **Razón vs reusar la sol::state de un ScriptComponent específico:** los `condition_lua`/`on_select_lua` son del DIALOG, no de un entity. Si los corro contra el sol::state del NPC, las globales del script del NPC (`hp`, `pathFinding`, etc.) contaminan el sandbox del dialog — y peor: si el NPC NO tiene script, no hay sol::state contra qué evaluar.
- **Razón vs sol::state global por ScriptSystem:** ScriptSystem maneja una sol::state por entity (cada script Lua aislado, decisión F2H8 del plan original). Agregar una "global" rompe esa convención + mezcla responsabilidades.
- **Trade-off aceptado:** los dialog hooks NO pueden acceder a `self.transform` ni `engine.exposed` directamente. Si necesitan eso, el dev usa `dialog.set_var` desde el script del NPC + `dialog.get_var` desde el hook. Patrón clásico de event-bus con state intermedio.

**Decisión clave 2 — NO exponer `dialog.start/advance/continueNext/stop` desde el host:** las funciones de control de flujo del DialogSystem viven solo en la tabla `dialog` de los scripts de entity (LuaBindings.cpp F2H48 Bloque E), NO en el host del F2H48.1.

- **Razón:** si un `on_select_lua` llama `dialog.advance(1)` mientras estamos en medio de `DialogSystem::advance(0)` (el que dispara el hook), entramos en recursión + state machine inconsistente. El host debe ser READ + WRITE de vars, NO control de flujo.
- **Caso futuro:** si un dev pide "saltar al nodo X desde un on_select_lua" (skip directo sin link), agregar `dialog.jump_to(nodeId)` que difiere la transición al final del frame. v1 no lo necesita.

**Decisión clave 3 — Wrap defensivo `"return (" + expr + ")"`:** los `condition_lua` se escriben como expresiones (`dialog.get_var('x') == 'true'`) sin `return` explícito. El host wrappea automáticamente para que el dev no tenga que escribir `"return dialog.get_var(...)"` en cada choice.

- **Razón:** ergonomía. Las expresiones de condition leídas por humanos parecen condiciones (`a == b`), no statements (`return a == b`). Sol2 requiere `return` para capturar el valor.
- **Edge case:** si el dev escribe `"return x"` literal, el wrap produce `"return (return x)"` que es parse error → la choice no aparece (fail-safe loggea). Aceptable — esos casos son raros y el log dirige al dev.

**Decisión clave 4 — Fail-safe a `false` en errores de evaluate:** si el `condition_lua` tiene parse error o runtime error, la choice NO aparece (return false del evaluator). Log a `script` channel para que el dev vea.

- **Razón:** filosofía defensiva. Una choice con condition rota es probablemente "no debería estar visible". Mostrarla y permitir avanzar a un nodo basura es peor que esconderla. El warn en el log es alta señal: el dev ve el error inmediatamente.
- **Alternativa descartada:** mostrar la choice + ejecutar igual el `on_select_lua` rota cuando la elijan → side effects impredecibles. NO.

**Decisión clave 5 — `DialogScriptHost::reset()` NO toca `GameState::dialogVars()`:** la reset del host tira la sol::state (eventuales globals del juego que el dev haya creado) pero las vars del dialog persisten.

- **Razón:** consistencia con la decisión F2H48 #2 (vars sobreviven entre dialogs). Si un `Save/Load` del juego empuja vars al disco (deferred), reset del host NO debería borrarlas.

**Decisión clave 6 — `cachedDialogId` NO se persiste en `.moodmap`:** solo `dialogPath + autoStartOnInteract` van al JSON. El `cachedDialogId` arranca en 0 al load — el DialogInteractSystem llama `loadDialog(dialogPath)` la primera vez que el player entra al trigger.

- **Razón:** los IDs del AssetManager no son estables entre sesiones (depende del orden de loads). Persistir un ID viejo apuntaría a slots equivocados. Recompoer via `loadDialog(path)` es robusto + barato (cache hit tras el primer load).

**Decisión clave 7 — `dialogPath` vacío → NO se serializa el componente:** mismo patrón que `ScriptComponent::path`. Si el dev agrega un `DialogComponent` pero no le asigna path, el JSON queda limpio.

- **Razón:** evitar contaminación del .moodmap con componentes inertes. Un dev que olvida configurar el path no debería ver "dialog: {}" en el archivo.

**Alternativas descartadas:**

- **Persistir `dialogVars` en `.moodsave` como parte de F2H48.1:** YAGNI confirmado por el dev — la demo F2H49 no usa save/load. Sumar cuando emerja "guardia recuerda al cargar partida".
- **Mouse-clic sobre choices del HUD:** YAGNI — teclas 1-9 HL2-style son válidas estilísticamente. Sumar tras feedback de usabilidad post-F2H49.
- **Exponer físicas (`physics.raycast` etc.) en el host:** scope creep — los hooks de dialog NO deberían hacer raycasts. Si emerge caso real, agregar.
- **Cargar el host una vez por NPC en lugar de global:** complica + scope (init+teardown por NPC), sin ganancia (las vars son globales igual).
- **Persistir `cachedDialogId` directo:** rompe entre sesiones (IDs no son estables — depende del orden de loads del AssetManager).

**Condiciones de revisión:**

- Si un dev pide acceso a `physics.raycast` desde un `on_select_lua` (ej. "si el player está a < 2m del NPC, opción extra"), agregar el binding al host con cuidado de no abrir surface innecesario.
- Si emerge necesidad real de `dialog.jump_to(nodeId)` desde un hook (sin link), implementar con deferred transition (queue + flush al final del frame).
- Si los errores de `condition_lua` se vuelven comunes y el log es ruidoso, considerar un "modo estricto" toggleable que muestre las choices con error en rojo (para developer feedback durante demo time).
- Si un dev escribe `condition_lua` con efectos colaterales (`dialog.set_var(...)` dentro), el sandbox lo permite pero es mala práctica. Documentar en la convención del schema (no enforced).

---

## 2026-05-10: F2H48 cierre — Dialog runtime + HUD HL2-style + Lua bindings + DialogComponent

**Contexto:** continuación natural de F2H47 — el editor producía `.mooddialog` pero no había sistema que los interpretara en Play Mode. Bloque 2.3 + 2.4 del `PLAN_SUBFASE_2_5.md` fusionados (runtime y Lua bindings van de la mano). El dev confirmó 3 decisiones pre-implementación que abajo se detallan.

**Decisión clave 1 — DialogSystem como singleton namespace (no clase instanciable):** mismo patrón que `Mood::GameState::*`. El motor es single-threaded y una sola conversación activa a la vez basta para v1 (player no puede hablar con dos NPCs simultáneamente).

- **Razón vs clase con múltiples instancias:** simplifica el acceso desde cualquier punto (HUD widget, Lua bindings, DialogInteractSystem, char controller lock) sin pasarse el puntero. El patrón `GameState` ya está consolidado en el motor.
- **Trade-off aceptado:** si en el futuro el juego necesita "dialog en background con NPC X mientras player camina hacia NPC Y", habrá que refactorizar a clase con N instancias. v1 no lo necesita.

**Decisión clave 2 — `DialogSystem` sin deps de sol2 (callbacks `std::function`):** los hooks `LuaEvaluator/LuaExecutor/NodeEnterHook/ChoiceHook` son `std::function` inyectables. El módulo en `engine/dialog/DialogSystem.h/cpp` NO incluye `<sol/sol.hpp>`. Los tests headless lo prueban end-to-end sin Lua.

- **Razón:** mismo principio que `Graph` (F2H46) y `DialogAsset` (F2H47) — el state model es puro, la integración con Lua vive en `LuaBindings.cpp`. Testeable + se puede usar el sistema desde C++ directo (ej. tutorial in-engine que avanza dialogs sin script Lua).
- **Trade-off aceptado:** los `condition_lua`/`on_select_lua` declarados en los choices del DialogAsset NO se ejecutan en v1 — no hay sol::state inyectado. Los choices se asumen siempre disponibles. Para v2 se necesita una sol::state global del juego (deferred hasta caso real).

**Decisión clave 3 — Variables del dialog persisten en `GameState::dialogVars()`:** map global `unordered_map<string, string>` accesible desde Lua (`dialog.set_var/get_var/has_var/clear_vars`). Sobreviven entre dialogs.

- **Razón vs session-only (clear al stop):** habilita "el NPC recuerda que ya hablaste". Patrón clásico en RPG (Skyrim, Fallout, BG3): la state del mundo persiste entre conversaciones.
- **Persistencia en save:** **NO en v1** — vive solo en memoria. Sumar al `SaveLoad.cpp` cuando emerja necesidad real ("el guardia recuerda al cargar partida"). Sin urgencia.
- **Stringly-typed (value = string):** simplifica binding Lua + JSON serialization futuro sin variant. Si el script necesita int o bool, hace `tonumber()` / `s == "true"` — convención aceptable para v1.

**Decisión clave 4 — Trigger de start = `DialogComponent.autoStartOnInteract` + Lua escape hatch:** un NPC en escena con `DialogComponent` + `TriggerComponent` auto-muestra "[E] Hablar" cuando player entra al trigger; al apretar E, `DialogSystem::start` arranca con el `.mooddialog` apuntado. Como alternativa, `dialog.start("path")` desde Lua arranca cualquier dialog desde donde sea (cinemáticas, custom triggers, scripted events).

- **Razón vs (a) "solo Lua" o (b) "solo auto-trigger":** combinación cubre 80% de casos (NPCs estándar) con cero código + escape hatch para los casos custom. Patrón usado en HL2 (entity NPCs vs scripted_sequence).
- **`autoStartOnInteract=true` por default:** opt-out cuando el dev quiere control total via Lua.
- **`cachedDialogId` en runtime para evitar `loadDialog` por frame:** el primer trigger carga, el resto reusa. Reseteado cuando se cambia el path desde Inspector.

**Decisión clave 5 — HUD widget `dialog_box` HL2-style con choices numeradas 1-9 (no mouse clic):** caja inferior centrada (max 800px o 70% del ancho), NPC text con wrap, "1) opción / 2) opción" listadas en amarillo HL. Tecla 1-9 = `advance(idx)`. Tecla E (cuando no hay choices) = `continueNext`.

- **Razón vs clic con mouse:** simplifica v1 sin handling de hover/click del DrawList (el widget se renderea con `ImDrawList::AddText`, no es interactivo). Patrón HL2/Mass Effect Classic = teclado numérico. Si en el futuro el dev pide mouse clic, agregar `ImGui::IsMouseClicked + AABB test` sobre cada choice — incremento pequeño.
- **Cap visual 9 choices:** suficiente para 99% de los casos (típicamente 2-4). Si emerge un caso con 10+, falla por design — split en sub-dialogs.

**Decisión clave 6 — Char controller lock via flag `GameState::dialogActive()`:** WASD/jump/crouch ignorados mientras dialog activo. Mouse-look queda libre (el player puede mirar al NPC con naturalidad mientras conversa). Editor + Player ambos respetan el flag.

- **Razón vs (a) input completamente desactivado (mouse incluido) o (b) congelar physics entero:** mouse-look libre es ergonómico (jugadores quieren mirar al NPC sin perder agencia visual). Congelar physics entero rompe simulaciones de fondo (NPCs patrullando, partículas, etc.) — innecesario.
- **Reseteo en `GameState::reset()`:** al salir de Play Mode el flag se limpia automáticamente (preservación de invariantes del editor).

**Decisión clave 7 — Workspace Narrativa con viewport 3D (no NarrativeIntro placeholder):** pedido del dev tras tour visual de F2H47. Layout final 3 columnas: Viewport (3D) 30% izq / Dialog Editor 40% centro / col der vertical con Node Inspector arriba + Dialog Browser abajo. NarrativeIntro removido del default (sigue accesible via `Ver`).

- **Razón:** Sub-fase 2.5 va a producir NPCs reales (F2H49). El workspace puro de canvas era abstracto — el dev necesita ver el NPC en escena mientras edita su dialog (posicionar trigger, validar anim, etc.). Patrón estándar de editores narrativos (RenPy con preview, Inkle con scene viz).
- **Trade-off aceptado:** menos espacio para el canvas del Dialog Editor (40% vs 100% previo). Si los grafos crecen, el dev puede expandir el panel con drag — el layout es solo el default.
- **Persistencia por workspace:** cada workspace guarda su propio `iniLayout` en `captureCurrentLayout` al switch + restaura via `LoadIniSettingsFromMemory` al volver. Confirmado al dev: layouts independientes — cambios en Narrativa NO afectan a Layout.

**Decisión clave 8 — `DialogComponent` en popup Add Component (categoría Logic):** descubrible como componente normal del Inspector, junto a Script + NavAgent. Descripción i18n: "Linkea un asset .mooddialog; auto-arranca al interactuar el player si hay un Trigger."

- **Razón:** sin esto, asignar dialog a una entity requeriría Lua o hacks. El popup Add Component es el flujo discoverable estándar.
- **Categoría Logic:** vive con Script + NavAgent — todos cubren behavior/data del entity vs Render/Physics/Audio/World.

**Alternativas descartadas:**

- **DialogSystem como clase con N instancias:** YAGNI v1 (no hay caso de uso).
- **Inyectar `LuaEvaluator/LuaExecutor` desde una sol::state global ya en F2H48:** scope creep — requiere crear infra de sol::state global del juego compartida (no existe hoy). Deferred hasta que un caso real lo pida (probablemente F2H50+ con inventory + quest condition predicates).
- **Persistir `dialogVars` en `.moodsave` desde F2H48:** YAGNI — el demo de F2H49 no lo necesita. Sumar cuando emerja.
- **Mouse clic sobre choices:** complica el v1 sin valor crítico. Teclado numérico es estándar HL2.
- **Workspace con tabs Viewport+Dialog en center:** menos usable que side-by-side. El dev prefiere ambos visibles.
- **Inspector general en Narrativa:** no entra — el workspace se enfoca en canvas + 3D. El dev puede abrir Inspector via `Ver` si lo necesita puntualmente.

**Condiciones de revisión:**

- Si emerge necesidad de dos conversaciones simultáneas (NPC patrullaje + cinemática), refactorizar DialogSystem a clase con N instancias.
- Si los `condition_lua`/`on_select_lua` empiezan a tener uso real en demos/juegos del dev, inyectar `LuaEvaluator/LuaExecutor` con sol::state global dedicada.
- Si el dev valida un caso donde `dialogVars` deben sobrevivir entre Save/Load (guardian recuerda al cargar), agregar serialización al `SaveLoad.cpp`.
- Si emerge un grafo de dialog con 10+ choices en un solo nodo, agregar overflow (scroll en el HUD) o forzar split en sub-dialogs (decisión de pipeline).
- Si el dev pide mouse-clic sobre choices en el HUD (más ergonómico para gamepad? touch?), agregar handler de `IsMouseClicked + AABB test` por choice.
- Si el workspace Narrativa con viewport 3D se siente apretado en monitores chicos (<= 1600px), reconsiderar tabs en center.

---

## 2026-05-10: F2H47 cierre — Dialog Editor (autoría: schema + visual editor + inspector + browser)

**Contexto:** Segundo hito real de Sub-fase 2.5 (Bloque 2 del plan `PLAN_SUBFASE_2_5.md`). Construye el primer editor de contenido sobre la infra del node-graph framework de F2H46. Entrega herramienta de autoría completa end-to-end (schema + asset + 3 paneles del editor + sample demo) pero NO el runtime — el state machine del dialog que corre en Play Mode + HUD widget + Lua bindings + DialogComponent quedan para F2H48. Split editor/runtime deliberado para validar el schema antes de atar el runtime.

**Decisiones técnicas clave:**

- **Schema v1 con un solo tipo de nodo (`dialog_line`).** Razón: simplicidad — los hooks `condition_lua` y `on_select_lua` por choice cubren el 80% de los casos prácticos. Tipos `condition` (nodo de branching puro), `action` (nodo de hook sin texto), `jump` (saltar a otro `.mooddialog`) emergen como necesidad real cuando un caso de juego concreto los pida. Alternativa descartada: incluir los 4 tipos en v1 — schema más rico pero la mayoría no se usaría, y agregar nodos en v2 es non-breaking.

- **Auto-sync invariante: N choices == N output sockets.** Razón: el dev edita choices en el Inspector contextual; el grafo debe reflejar visualmente que un nodo con 3 opciones tiene 3 outputs (uno para cada destino). Si el dev tuviera que agregar/borrar sockets manualmente en paralelo al array de choices, sufriría desync errors. Implementación: `Asset::writeLine()` re-crea sockets para matchear `choices.size()` (o exactamente 1 "continue" si vacío), borrando los sobrantes (cascade de links incidentes via `Graph::removeSocket` que tuve que agregar como extensión a F2H46). Alternativa descartada: dejarlo manual + validator que detecte el desync — propenso a frustrar al dev con errores constantes.

- **Toggle text_key vs text_literal por línea (y por choice).** Razón: prototyping flow real — el dev escribe el texto inline al armar el diálogo y promueve a i18n key después cuando el contenido se estabiliza. Forzar i18n key desde el día 1 friccionaría el prototyping; forzar literal eliminaría la opción de localización. El toggle deja al dev elegir cuándo promover. UI: checkbox "Usar i18n key" en el Inspector — al togglear, el valor previo se mueve al otro campo (no se pierde). Alternativa descartada: dos campos visibles siempre — clutter visual y ambiguo cuál gana en runtime.

- **Cycles permitidos pero reportados como Warning (no Error).** Razón: algunos diseños de diálogo (loops controlados por flags Lua, "el NPC vuelve al menú principal hasta que el jugador elija X") usan ciclos intencionalmente. Bloquearlos como Error frustraría esos casos. Reportar como Warning informa al dev sin impedir guardar. Detección via DFS iterativa con coloring blanco/gris/negro (back edge = ciclo). Alternativa descartada: análisis estático que descarte ciclos "escapables" — feature de v2 cuando los ciclos sean comunes.

- **Save explícito v1 (no auto-save).** Razón: el dev necesita control durante experimentación — auto-save sobrescribe el archivo mientras el dev prueba ideas que quizás revertirá. Auto-save con debounce + undo a-disco es feature de polish para v2.

- **Path absoluto `<cwd>/assets/dialogs/` para el Browser.** Razón: convención del editor existente (Material Editor F2H42 usa `<cwd>/assets/materials/`). El cwd se setea al directorio del proyecto cuando se abre uno; reusar el patrón existente evita inventar abstractions del filesystem específicas para Dialog.

- **Sample demo generado programáticamente en lugar de shipped como archivo.** Razón: hacer commit de un `demo_intro.mooddialog` agregaría un asset al repo con texto pre-cocinado, que después habría que mantener traducido + sincronizado con cambios de schema. Generarlo en `processSpawnDialogDemoRequest()` (DemoSpawners_Basic.cpp) usa el API real del Asset y queda automáticamente up-to-date con cualquier evolución del schema. El archivo se persiste a disco la primera vez para que reabrir el demo no lo regenere (el dev podría haberlo editado y queremos preservar sus cambios).

- **Split editor (F2H47) vs runtime (F2H48).** Razón: checkpoint de validación. Si encerramos editor + runtime en un solo hito mega, cambios al schema durante el desarrollo del runtime requerirían re-trabajar el editor. Con split, F2H47 estabiliza el schema (con tests), F2H48 implementa el runtime sabiendo qué consume. Aceptado por el dev pre-implementación.

- **Window IDs estables (sin `###` ni i18n) para que matchee DockBuilder.** Lección de F2H46 aplicada: el window name pasado a `ImGui::Begin` es lo que `DockBuilderDockWindow` hashea. Si el title se traduce, el hash cambia y el dock no se aplica. Convención: `name()` de cada panel retorna inglés estable; el contenido del panel sí es i18n.

- **Inspector contextual como panel separado (no inline en Editor).** Razón: separación clara de concerns — el Editor maneja el grafo, el Inspector maneja los campos del nodo seleccionado. Beneficio adicional: el Inspector puede docked a la derecha del Editor (consistente con el flujo Inspector/Hierarchy del editor general). Alternativa descartada: campos del nodo dentro de un ImGui::CollapsingHeader en el Editor mismo — agrega clutter al canvas y ocupa espacio que va a ser premium cuando los grafos crezcan.

- **Sandbox queda como Debug-only, NO default del workspace Narrativa post-F2H47.** Razón: el Sandbox era placeholder de F2H46; con F2H47 el workspace tiene un editor real. Mantener el Sandbox visible confundiría al dev sobre cuál es la herramienta principal. Sigue accesible vía Ver > Debug para inspección del framework subyacente — utilidad permanente para futuros devs que extiendan node-graph.

**Alternativas descartadas explícitamente:**

- **Tipos de nodo `condition`/`action`/`jump` en v1**: descartado por scope. Hooks Lua en choices cubren los casos prácticos.
- **Voiceover sync (highlight de palabras con audio timing)**: descartado por scope. customData ya tiene `audio` path; v2 puede agregar timing data si emerge necesidad.
- **Animation wait_for flag**: el customData tiene `animation` pero v1 solo dispara; no espera a que termine. Polish de v2.
- **Theming custom del grafo (paleta narrativa)**: herencia de F2H46 sin urgencia hasta que el grafo se vuelva confuso visualmente.
- **String tables dedicadas para gameplay (vs reusar i18n actual)**: descartado en v1. Las keys de dialog viven en el mismo `assets/i18n/{en,es}.json` que el editor — funciona, no agrega infra.
- **AssetManager::loadDialog**: descartado para F2H47. Se agrega en F2H48 cuando el runtime necesita resolver paths logicos para DialogComponent. F2H47 usa I/O directo de filesystem.

**Condiciones de revisión:**

- Si emerge necesidad real de tipos `condition`/`action`/`jump`: agregar en F2H48 o hito propio antes de que los devs externos los pidan ad-hoc.
- Si auto-sync de choices causa pérdida de links accidental (el dev borra un choice y pierde el link a un nodo importante): considerar agregar warning de confirmación antes del remove en el Inspector.
- Si el Browser con muchos diálogos (>50) se vuelve lento al refresh: paginar o filtrar.
- Si emerge demand por preview en-vivo del diálogo dentro del editor (sin entrar a Play Mode): agregar como feature de F2H48 una vez que el runtime exista.

---

## 2026-05-10: F2H46 cierre — node-graph framework (integración `imgui-node-editor`) + workspace "Narrativa"

**Contexto:** Primer hito real de Sub-fase 2.5 (Bloque 0.1 del plan `PLAN_SUBFASE_2_5.md`). Pre-requisito de los Dialog Editor (F2H47) y Quest Editor (F2H4X) que vienen — comparten arquitectura de grafo, implementar UN framework reutilizable evita duplicación. Estimación inicial ~8h en 8 bloques (A-H); realizado en ~1 sesión con 3 fixes técnicos reactivos al feedback del dev durante validación visual.

**Decisiones técnicas clave:**

- **Lib externa `thedmd/imgui-node-editor` vs framework propio.** Decisión confirmada con dev pre-implementación: usar la lib. Razones: ~10x menos esfuerzo (~1 hito integración vs ~3-5 propio), battle-tested (usado en herramientas Unreal-like), header-only-ish, license MIT. Trade-off: control limitado del look-and-feel (theming custom es feature futura), dependencia externa nueva (aceptable — sigue el patrón de imgui/glm/EnTT/Lua/Jolt/etc en `_deps/`). Alternativa descartada: framework propio sobre ImGui DrawList — control total pero scope explotaría fácil con feature creep (animaciones, snap, multi-select, etc) que la lib ya da gratis.

- **Pinned a commit master post-v0.9.3** en lugar del tag v0.9.3 (Aug 2023). Razón: v0.9.3 usa APIs ImGui ya removidas en docking branch actual (`ImRect::Floor`, `ImGui::GetKeyIndex`). Master tiene los fixes. Cuando aparezca v0.9.4+ tag estable, migrar — por ahora pin a hash 2025-era para reproducibilidad. Documentado en CMakeLists.txt.

- **Patch idempotente al header de la lib via CMake `file(READ/WRITE)` en configure-time.** `imgui_extra_math.inl` define `operator==/!=/operator*(float, ImVec2)` para ImVec2 que colisionan con el ImGui docking branch que ya los provee (`imgui.h:3038-3054`). Patch envuelve el bloque problemático en `#ifndef IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED` con guard `MOOD_NE_OPS_GUARD`. Idempotente — re-config detecta el guard y no reaplica. Alternativa descartada: fork del repo + maintenance overhead, o aplicar patch via `PATCH_COMMAND` en CPM (más frágil con git apply en Windows). El `file(REPLACE)` directo es portable + idempotente + auto-documentado en el log de configure.

- **Separación dura state vs rendering: `engine/nodegraph/Graph.h` puro + `NodeGraphEditor.h` con pImpl.** Razón: Dialog/Quest van a serializar grafos a JSON. La serialización no debe depender de ImGui. Tests unitarios del data model no necesitan ImGui linkeado en `mood_tests`. Mismo patrón que F2H39 GameState ↔ GameOverlay. El pImpl en `NodeGraphEditor` evita que clientes del header incluyan `imgui_node_editor.h` ni ImGui — solo `Graph.h`. Alternativa descartada: una sola clase mezclando state + draw — perdés testabilidad y el header se contamina.

- **IDs propios `u32` con `k_invalid = 0` como sentinel, monotonicos sin reuso post-delete.** Razón: simpler than tombstones / generational indices; `u32` cabe en el `uintptr_t` que usa la lib internamente para sus NodeId/PinId/LinkId (cast directo en helpers). Generadores `m_next*Id` se serializan en JSON para que cargar un grafo + agregar nuevos nodos siga IDs frescos (no reusa IDs de nodos borrados). Alternativa descartada: indices en arrays — frágil ante deletes y reordenamientos.

- **5 reglas de `canConnect` estrictas v1.** (1) ambos sockets existen, (2) kinds correctos (Output → Input, no Output→Output ni Input→Output), (3) typeTag idéntico (sin coerciones / casts entre tipos), (4) no duplicar link existente con mismo from+to, (5) un input acepta solo 1 link entrante (outputs pueden fan-out a N). Razón: los editores específicos (Dialog/Quest) van a definir su propia taxonomía de typeTag — coerciones automáticas en el framework crearían ambigüedad. v2 podría agregar matriz de compatibilidad si emerge necesidad real. Test coverage exhaustivo de las 5 reglas en `test_nodegraph.cpp`.

- **`draw()` no-mutating + retorna `std::vector<EditorEvent>`.** Razón: desacopla el wrapper del sistema de undo/redo. El caller (Sandbox panel, eventualmente Dialog/Quest Editor) traduce eventos a Commands y los pushea al `HistoryStack` para undo. Alternativa descartada: mutar el grafo dentro de draw — el wrapper necesitaría conocer HistoryStack, agregando dependencia transitiva. Eventos suficientes para v1: NodeMoved, NodeDeleted, LinkCreated, LinkDeleted. AddNode no es event (la lib no dispara "create node" implícito — el caller lo dispara explícito via botón/menu y construye directo el `AddNodeCommand`).

- **Sync `graph → lib` unconditional cada frame cuando `!ne::IsActive()`.** Razón: garantizar que un undo de MoveNodeCommand revierta la pos visualmente. La lib mantiene su propio storage de posiciones; si después de `Command::undo()` el graph.position cambia pero la lib sigue mostrando la pos vieja → bug visual. La gate `!IsActive()` evita override durante un drag activo (en ese caso la lib es source of truth temporal hasta drag-end). Alternativa descartada: track explícito de "graph mutated externally" con dirty flags — más invasivo y propenso a perder un evento. Polling con epsilon-compare es trivial.

- **Window titles de paneles nuevos = `name()` directo (English estable, sin `###StableID`).** Razón: convención con el resto del editor (Inspector, Console, Performance — ninguno traduce su title bar). `DockBuilderDockWindow(window_name)` hashea el string completo — debe matchear con lo que `ImGui::Begin` pasa. Si usaramos `"Título###StableID"` traducido, las traducciones romperían el hash → dock no se aplicaría. Solución: title fijo, contenido traducido. Decisión post-fix tras dev reportar "ambos siguen flotando" — el ###suffix initial design no matcheaba con DockBuilder lookup.

- **Workspace "Narrativa" SIN paneles de scene/3D.** Cita del dev (post-validación 1): *"si se trata de narrativa no tiene sentido que muestre paneles que no tienen sentido"*. Workspace contiene SOLO `NodeGraphSandbox` (izq 70%) + `NarrativeIntroPanel` (der 30%). Inspector, Asset Browser, Console quedan accesibles desde Ver pero no son default. Razón: el workspace tiene que ser dedicado a su tarea, no un cajón de sastre.

- **`io.ConfigDebugHighlightIdConflicts = false` global.** Razón: el debug-check nuevo de ImGui 1.92 da false-positives con imgui-node-editor (la lib manipula su propio ID stack internamente). Nuestra suite de 8580 assertions cubre lo que el check detectaría en código nuestro. Desactivar globalmente vs por-panel: el flag se chequea cada widget submission, suprimirlo local fue insuficiente porque la alarma se dispara en otra fase del frame. Trade-off aceptado: perder el check de debug para code real nuestro — los tests + revisiones del agente mitigan.

- **`NarrativeIntroPanel` como placeholder explicativo.** Cita del dev al ver el Sandbox por primera vez: *"como esto conecta con el 3D, como creo yo conversaciones o que puedo crear con esto, no entiendo aun"*. Sin un panel que explique que F2H46 es **infra** y los editores reales vienen en F2H47+, el workspace parece roto / abstractamente inútil. Panel con título en amarillo HL + body explicando + roadmap de F2H47/F2H4X/F2H4Y + footer engine-grade philosophy. Se reemplaza/oculta cuando los editores reales aterricen — temporal pero crítico para handoff a la próxima sesión / dev nuevo.

**Alternativas descartadas explícitamente:**

- **Framework propio sobre ImGui DrawList**: descartado pre-implementación. Justificado por scope (~3-5 hitos extra) sin ganancia clara para v1.
- **v0.9.3 tag estable de imgui-node-editor**: descartado tras encontrar APIs ImGui removidas. Master post-fixes es la opción viable hasta que aparezca v0.9.4+.
- **Coerciones automáticas entre typeTags de sockets**: descartado en v1. Tipos estrictos forzan decisiones explícitas del dev del editor específico (Dialog/Quest). Agregar coerciones más tarde si emerge necesidad.
- **Sync bidireccional lib ↔ graph cada frame**: descartado. Source of truth claro (graph siempre, lib durante drag). Bidireccional crearía race conditions.
- **Theming custom del grafo en F2H46**: descartado por scope. El default de la lib es funcional; theming a la paleta Valve es feature de polish — se agrega cuando los Dialog/Quest Editor lo pidan.
- **Menu contextual para "Add Node" en el canvas**: descartado en v1. Los botones del toolbar (+Source/+Process/+Sink) cubren v1; el contextual menu es feature de ergonomía que aterriza con Dialog Editor cuando los tipos de nodo emerjan.

**Condiciones de revisión:**

- Si la lib master rompe build en update futuro: pinear a commit anterior verificado, o evaluar fork.
- Si el patch al `imgui_extra_math.inl` falla por cambio upstream del header (renombran o cambian estructura del bloque): adaptar el `string(REPLACE)` o eliminar el patch (la versión upstream eventualmente arregla el conflicto en su lado).
- Si emerge necesidad de coerciones entre typeTags (ej. socket "int" conecta a "float"): agregar matriz de compatibilidad en `canConnect`.
- Si la performance con grafos grandes (>500 nodos) se vuelve cuello: profilear, considerar lazy rendering / culling fuera del viewport.
- Si los Dialog/Quest Editor descubren que el evento-based dispatch del wrapper es restrictivo (ej. necesitan interceptar drag mid-action): agregar callbacks adicionales en NodeGraphEditor, sin romper la API de `draw()`.

---

## 2026-05-10: Sub-fase 2.5 — commitment estratégico (scope Pro Tools + filosofía engine-grade)

**Contexto:** Post-F2H45 el dev confirma Sub-fase 2.5 (diálogos / quests / inventario) como next-up. Pre-implementación arranca conversación estratégica: el dev quiere alineamiento del agente con la filosofía del motor antes de tocar código. Esta entrada documenta los compromisos de diseño que aplican a TODA la sub-fase (no a un hito específico) y que cualquier decisión técnica futura en la sub-fase debe respetar. **NO es decisión de implementación** — es marco de diseño no-negociable que precede a la implementación.

**Decisiones estratégicas clave:**

- **Filosofía: motor que crea juegos, no juego concreto.** Cita verbatim del dev: *"este sistema que haremos ahora es el más importante, me refiero a que debe ser la base para que a futuro cualquier dev al crear su juego pueda utilizar este sistema de inventario, quests, y diálogos, para sus juegos, osea debe ser versátil y realmente amigable de usar... aún no estamos creando un juego, estamos creando el motor que va a crear juegos"*. **Implicación dura**: cualquier decisión que solo encaje con un género (RPG / shooter / walking sim / metroidvania) o que asuma una semántica de gameplay (XP / mana / HP como conceptos del motor) es **bug de diseño** y debe rechazarse. El motor solo conoce contenedores genéricos; el dev del juego registra los recursos y semántica que su juego usa.

- **Scope nivel B (Pro Tools), no nivel A (mínimo viable).** Cita verbatim: *"B, aunque nos lleve días"*. Significa: Dialog Editor como node-graph visual interconectado (estilo Unreal Blueprints / `ink`), Quest Editor como flowchart, Item Browser con 3D preview rotable. **NO** listas de nodos con dropdowns, **NO** lista plana de objectives, **NO** Asset Browser plano. **Costo aceptado**: la sub-fase pasa de ~3 hitos (scope A en `PLAN_FASE2.md:285-303`, ahora obsoleto) a ~15-20 hitos. El dev explícitamente acepta el costo en tiempo a cambio de calidad engine-grade.

- **8 principios de diseño no-negociables** (extraídos de la conversación y aplicables a TODOS los hitos de la sub-fase):
  1. **Data-driven, no code-driven**: NPCs / quests / items se crean sin tocar código del motor. Todo es asset visualmente editable (`.mooddialog`, `.moodquest`, `.mooditem`).
  2. **Sin semántica hardcodeada de gameplay**: API genérica `stats.add("xp", 100)` donde `"xp"` es string que el dev registra, no un campo `stats.xp` del motor.
  3. **Hooks Lua sobre primitivas**: `on_quest_complete(id, callback)` — el callback hace lo que ese juego quiera. Patrón ya consolidado con triggers + scripts.
  4. **Default + override en rendering**: widget HUD default funcional + `dialog.set_renderer(lua_callback)` para juegos con HUD propio.
  5. **Composabilidad con sistemas existentes**: quest objectives son predicados genéricos contra estado del motor (`item_count`, `flag_set`, `area_entered`, `counter_at_least`). No tipos específicos como "kill 10 wolves".
  6. **Editor visual real, no JSON a mano**: cada sistema tiene su panel de editor visual. Si el dev abre `.json` crudo para crear contenido, fallamos.
  7. **State vs rendering separados**: patrón F2H39 `GameState` ↔ `GameOverlay` — lógica en módulos puros sin deps gráficas → `mood_tests` los testea sin ImGui. Aplica a Dialog/Quest/Inventory state.
  8. **Engine-agnostic respecto al género**: si la API solo sirve para RPG o solo para shooter, es bug.

- **Orden propuesto de bloques** (pendiente confirmar con dev al arrancar la sub-fase): Bloque 0 (infra compartida: node-graph framework + 3D preview widget) → Bloque 1 (inventario) → Bloque 2 (diálogos) → Bloque 3 (quests) → Bloque 4 (soporte producción: stats genéricos + save v3 + template/sample project + docs developer-facing). **Recomendación inventario primero**: diálogos y quests dependen de "tengo X item" para condicionales — empezar por items evita hardcodear flags `game.has_key` que después hay que migrar. Detalle completo en [`PLAN_SUBFASE_2_5.md`](PLAN_SUBFASE_2_5.md).

- **Bloque 0 (infra compartida) primero, no por dentro de cada sistema.** Razón: Dialog Editor + Quest Editor van a compartir arquitectura de node-graph. Implementar UN framework reutilizable (pan/zoom/snap, sockets, conexiones, save/load) y consumirlo desde los 2 editores. Alternativa descartada: implementar diálogos primero con su propio grafo, refactor cuando llega quests — más rework. Mismo razonamiento para el 3D preview widget (Item Browser ahora + Material Editor pro version después).

- **Decisión técnica abierta para el Bloque 0**: node-graph framework propio sobre ImGui DrawList vs adoptar `imnodes` / `imgui-node-editor`. Pros del propio: control total sobre look-and-feel + integración estética con el resto del motor + no agregar dependencia. Pros de adoptar: ~10x menos esfuerzo + battle-tested. **Pendiente evaluar al arrancar Bloque 0** — documentar pros/cons con código de muestra antes de elegir.

**Alternativas descartadas explícitamente:**

- **Scope A (mínimo viable)**: descartado por el dev. *"B, aunque nos lleve días"*.
- **Implementar los 3 sistemas en paralelo**: descartado por scope. Cada uno es un proyecto considerable; secuencial permite que las decisiones del primero informen al siguiente (ej. cómo se hace el editor visual del inventario informa cómo se hace el de diálogos).
- **Empezar por diálogos**: descartado. Items condicionan diálogos via predicados — si arrancamos por diálogos hardcodearíamos flags que después hay que migrar al sistema de inventario real.
- **Hardcodear "XP", "level", "mana" como campos del Stats system**: descartado. Cada juego tiene su modelo (XP / soul / experiencia / nada). Genérico key-value.
- **Diferir el editor visual para una v2**: descartado. Sin editor visual el sistema NO es engine-grade. Si el dev tiene que tocar JSON crudo para crear NPCs, fallamos.

**Condiciones de revisión:**

- Si en cualquier punto de la sub-fase emerge una decisión técnica que viola alguno de los 8 principios, **detenerse** y pedir confirmación al dev. Los principios son no-negociables salvo intervención explícita.
- Si el scope estimado de ~15-20 hitos resulta muy bajo (ej. el node-graph framework solo toma 4-5 hitos), no recortar features — extender la sub-fase. El dev priorizó calidad sobre velocidad.
- Si emerge un caso donde un género específico (ej. shooter sin diálogos) no necesita un sistema, eso no justifica simplificar el sistema. La opcionalidad ya está garantizada (un juego no usa un sistema que no enchufa) — la generalidad sigue siendo necesaria.
- Cuando se cierre cada hito de la sub-fase, anotar en `DECISIONS.md` las decisiones técnicas específicas, pero referenciar esta entrada como marco general.

---

## 2026-05-10: F2H45 cierre — deudas pre-Sub-fase 2.5 (AddComponentCommand undoable + Lato Player con tildes + Console tooltip i18n)

**Contexto:** tras cerrar F2H44, el dev pidió revisar `PENDIENTES.md` para clasificar deuda real vs feature diferida y arreglar las deudas reales antes de arrancar Sub-fase 2.5: *"no me gustan las deudas futuras, quiero arreglar lo necesario ahora antes de pasar a otra cosa"*. Tres deudas identificadas como reales (consistencia/sweep incompleto): (1) AddComponentCommand sin undo (toda otra acción del Inspector es undoable, romper esa expectativa para Add Component generaba dissonance), (2) font del MoodPlayer atrasada en ProggyClean desde F2H38 + por consecuencia `es.json` sin tildes desde F2H43 (paridad forzada), (3) Console tooltip multilínea con icons FA inline que el subagente de F2H43 dejó sin envolver. Otras ~12 entradas del PENDIENTES.md fueron clasificadas como features diferidas (no son deuda) y NO entraron al hito.

**Decisión clave 1 — AddComponentCommand type-erased en lugar de N templates específicos:** el comando vive como una sola clase `AddComponentCommand` con dos `std::function<void(Entity&)>` (`add` + `remove`) que el helper templated `makeAddComponentCommand<T>(entity, label)` rellena capturando T en las closures. El callsite del popup pasa de `[e]() mutable { e.addComponent<X>(); }` a `[](Entity en, std::string lbl) { return makeAddComponentCommand<X>(en, std::move(lbl)); }`.

- **Razón vs `template<T> class AddComponentCommand`:** una clase templated generaría 11 instanciaciones del comando completo en el binario (con sus virtuales). Type-erase es 1 clase + 11 sitios de captura templated triviales (~3 LOC cada uno). El cost extra de los `std::function` es ~32 bytes por comando + heap alloc de la closure — irrelevante para una operación que el dev hace raramente vs los sliders del Inspector.
- **Razón vs implementar como `EditPropertyCommand<bool>` con setter que add/remove:** EditPropertyCommand está pensado para edits de UN campo de UN componente existente. Crear/destruir el componente entero es semánticamente otro animal — un comando dedicado tiene mejor `name()` ("Agregar componente: Luz") y separa el log del edit del log del add.
- **Sin snapshot del estado pre-add:** si el dev hace add → edita campos del componente → undo, los EditPropertyCommand de los edits quedan más arriba en el stack y se deshacen primero (LIFO). El AddComponentCommand solo necesita add/remove con valores default — los campos editados quedan capturados en sus propios commands. Probado en `test_add_component_command.cpp` con la interacción Add+Edit+Undo.
- **Trade-off aceptado:** redo de un Add (tras undo) recrea con valores default. Si el dev había editado campos en el ciclo original, los EditPropertyCommand del redo los re-aplican en orden — funciona correctamente, pero requiere que la HistoryStack mantenga los edits posteriores en el redo stack (lo hace por design — push() limpia el redo, pero un solo undo+redo no toca el redo).

**Decisión clave 2 — Lato en Player con mismo patrón que F2H38, sin FA merge:** el MoodPlayer ahora carga `LatoLatin-Regular.ttf` 15px en `PlayerApplication_Init.cpp` con range Basic Latin + Latin-1 Supplement + General Punctuation subset. FA (FontAwesome) NO mergeada al atlas.

- **Razón sin FA:** el HUD del Player usa DrawList procedural (F2H39+) — círculos, barras, anillos, hexágonos calculados con math, sin sprites ni iconos. El Pause menu del Player no usa FA tampoco (validado con grep `ICON_FA` en `src/player/`). Cargar FA agregaría ~400KB al atlas para nada. Si en el futuro un menú del Player necesita FA (improbable), agregarlo en su momento.
- **Razón mismo size 15px que el editor:** coherencia visual al transicionar Editor → Play Mode. Pre-F2H45 había salto de tamaño + estilo (ProggyClean 13px bitmap → Lato 15px smooth) que era inconsistente.
- **Sweep tildes habilitado:** con Lato cargado, los `á/é/í/ó/ú/ñ` rendean correctamente. Subagente delegado para sweep mecánico de ~75 palabras corregidas en ~60 valores de `es.json` (fundamentalmente sustantivos `acción/selección/configuración`, palabras del HUD `MUNICIÓN/MENÚ`, `Diseño/Español/Añadir/tamaño`). Términos técnicos en inglés (`Inspector/Hierarchy/Brush/MeshRenderer/workspaces`) intactos por convención del motor.

**Decisión clave 3 — Console tooltip: keys i18n para texto, icons FA en código:** el tooltip de leyenda log levels en `ConsolePanel.cpp` ahora se construye con `std::string` concat en C++:

```cpp
const std::string body =
    I18n::T("editor.panel.console.help.header") + "\n"
    "  " ICON_FA_BUG " " + I18n::T("editor.panel.console.help.level.trace") + "\n"
    ... (5 lines más) ...
    "\n" + I18n::T("editor.panel.console.help.footer");
ImGui::SetTooltip("%s", body.c_str());
```

- **Razón vs poner los icons en el JSON:** los macros `#define ICON_FA_X "\xef\x86\x88"` son secuencias UTF-8 cuyos bytes son válidos en JSON (después de `JSON.parse`), pero serían frágiles a editar y a la vista del traductor son tofu. Mantenerlos en código asegura que el traductor solo edita texto plano (la parte que SÍ entiende su idioma) y que un eventual cambio de los iconos FA no requiere tocar el JSON.
- **Patrón replicable:** este approach (1 key por segmento de texto + concat con bytes literales en código) es la solución estándar para cualquier tooltip / mensaje con icons inline. Quedó documentado en `_comment` de los JSONs como precedente.

**Decisión clave 4 — botón "Recompute mesh" eliminado, no escondido en grupo "Debug avanzado":** a pedido del dev tras tour visual del Inspector. Era debug breadcrumb del Hito 12+ que forzaba `bc.dirty=true` por si alguna mutación del brush no marcaba dirty automáticamente.

- **Razón vs esconderlo:** ninguna mutación del brush deja `dirty=false` por error en práctica (validado por 50+ commits desde F2H12 sin reportes). Esconderlo en un TreeNodeEx("Debug") agrega ruido visual y mantiene código muerto. Eliminar es más limpio.
- **Recuperable si emerge necesidad:** el campo `bc.dirty` sigue público; si en el futuro un developer necesita el escape hatch, puede agregarlo en una rama de debug local con 4 LOC.

**Alternativas descartadas:**

- **Lua scripts traducibles:** identificada como deuda real pero descartada del scope F2H45 — solo aplica a demos actuales (`hud_demo.lua`), no hay scripts gameplay reales. Esperar a que emerjan.
- **Validación full Player con compiledMesh:** no es deuda, es validación pendiente al primer empaquetado real del dev. Sin código que escribir hoy.
- **Implementar AddComponentCommand templated por cada T (sin type-erase):** scope inflado innecesariamente (11 instanciaciones × ~80 LOC cada una = ~900 LOC vs ~80 LOC del header-only type-erased actual).
- **Cargar FA en Player por si acaso:** YAGNI. El HUD procedural no lo necesita; agregar peso al atlas sin uso real es deuda preventiva.
- **Esconder "Recompute mesh" detrás de un toggle "Debug avanzado" en lugar de eliminar:** agrega complejidad de UI por código que nunca se usó. Eliminar > esconder.

**Condiciones de revisión:**

- Si emerge un caso real donde un brush queda con mesh stale sin marcar `dirty=true`, recuperar el botón "Recompute mesh" como single-LOC (`if (ImGui::Button("Recompute")) bc.dirty = true;`) — no requiere reabrir el hito.
- Si el dev percibe overhead notable de los `std::function` en AddComponentCommand al hacer add masivos (improbable — add no es operación de tight loop), refactorizar a templated por T.
- Si en el futuro se agrega un componente que NO sea default-constructible (improbable, todos los actuales lo son), `makeAddComponentCommand<T>` falla en compilación — requeriría sobrecarga adicional con args explícitos.
- Si el subagente cometió errores en el sweep de tildes que solo se ven en uso real (ej. tildes innecesarias en una palabra técnica que el dev espera sin tilde), corregir esa palabra puntual con `Edit` — no requiere hito propio.

---

## 2026-05-10: F2H44 cierre — polish onboarding UX (sin docs)

**Contexto:** tras cerrar F2H43 (i18n), el dev pidió evaluación crítica honesta de la UI desde la perspectiva de un dev de juegos nuevo. La auditoría identificó 5 gaps de descubribilidad/UX que limitaban la primera impresión del editor: (1) workspaces con nombres ambiguos al usuario nuevo, (2) sin "Add Component" en Inspector — el dev solo podía agregar componentes via demos hardcoded del menú Help, (3) demos enterrados en `Ayuda > Demos` (primera vista del editor era dockspace vacío), (4) VisGroups sin onboarding contextual del concepto Hammer/Source, (5) outline AABB de meshes seleccionados invisible (cubo unitario hardcoded que se perdía dentro de meshes grandes como Fox.glb). Bloque originalmente planeado #5 (USER_GUIDE/* + README + GIF) fue descartado por el dev: *"seguiremos agregando cosas que luego vamos a terminar cambiando"* — docs externos rotarían más rápido que la implementación.

**Decisión clave 1 — outline AABB usa `MeshAsset::aabbMin/aabbMax` real (no cubo unitario hardcoded):** pre-F2H44 el outline en perspectiva (introducido en F2H13) tenía un comentario *"compromise historico, no usa el AABB real del MeshAsset"* — usaba `glm::vec3(-0.5f, 0.5f)` para todos los meshes. Con un mesh de ~3m de largo (Fox.glb), el outline era un cubito de 1m³ centrado en el origen del transform: invisible al ojo. Fix: `else if (sel.hasComponent<MeshRendererComponent>())` lee el AABB real via `m_assetManager->getMesh(mr.mesh)`. Para entidades sin mesh ni brush (Light/Audio/Trigger/Camera/ParticleEmitter), AABB chico fijo 0.5m³ alrededor del origen → SIEMPRE hay feedback visual de selección (consistencia con orto que ya hacía esto desde F2H35).

- **Razón vs OBB rotado:** el AABB se sigue calculando world-space tomando 8 corners locales y transformándolos por `t.worldMatrix()`. Eso da un OBB real (rotado), no un AABB axis-aligned al mundo. El nombre "AABB" en el código es del input (local space del mesh), no del output renderizado.
- **Razón AABB chico fijo para point entities:** uniformidad visual. Si un dev clickea una luz puntual, espera ver UNA forma marcada — no un gizmo flotante sin contexto. 0.5m³ es suficiente para verse pero pequeño para no taparlas.

**Decisión clave 2 — Add Component popup sin AddComponentCommand undoable:** el botón "+ Agregar Componente" al final del Inspector abre un popup con search + lista agrupada (Render/Physics/Audio/Logic/World) de los 11 componentes agregables. El click llama directamente `e.addComponent<X>()` sin pasar por la HistoryStack del undo system.

- **Razón vs implementar AddComponentCommand:** consistencia con el patrón actual del motor — los demos del menú `Ayuda > Demos > Agregar luz puntual demo` (etc.) también hacen `e.addComponent` directo sin command. Crear un AddComponentCommand templated por cada uno de los 13 component types + serialización del estado pre-add para el undo es scope significativo (~200 LOC) para un caso que el dev puede revertir manualmente: clickear el icono X del header del componente (que el Inspector renderiza como parte del CollapsingHeader). Anotado como deuda futura por si emerge presión real.
- **Trade-off aceptado:** el dev no puede deshacer un Add Component con Ctrl+Z. Tiene que removerlo manualmente. Aceptable porque la operación es "creativa" (no destructiva) y reversible en 1 click.

**Decisión clave 3 — workspaces con ID ASCII estable separado del label visible:** pre-F2H44 el `Workspace.name` era a la vez el ID persistido en `.moodproj` y el label visible en la pestaña (`"Programar"`/`"Materiales"`/`"Editor de mapas"`). Cuando F2H43 introdujo i18n del editor, intentar traducir esos labels rompía la identidad del workspace (la persistencia en `.moodproj` esperaba el nombre español) — quedó documentado como deuda en F2H43. F2H44 separa: `name` ahora es ID ASCII estable (`"layout"/"scripting"/"materials"/"map_editor"`), label visible viene de `T("workspace.<id>")`.

- **Razón ID ASCII (no más enum):** `.moodproj` se persiste como JSON. Strings ASCII estables son grep-friendly + immune a re-renames de un enum. Si en el futuro se agrega un workspace `"animation"`, no requiere migrar nada del schema — solo agregar la key `workspace.animation` al JSON i18n.
- **Migración robusta:** `migrateWorkspaceName` extendida cubre 3 generaciones: F2H7 (inglés original, ej `"Scripting"→"scripting"`), F2H22 (español task-oriented, ej `"Programar"→"scripting"`), F2H44 (IDs ASCII, passthrough). Proyectos viejos abren sin perder iniLayouts custom del usuario.
- **Decisión paralela: NO eliminé el campo `name` por uno explícito `id`:** habría requerido tocar Workspace.h + ProjectSerializer (schema bump) + re-deserialización de proyectos viejos. El campo `name` sigue funcionando como ID — solo cambio su semántica vía comentario en el header. Refactor mecánico en lugar de cirugía profunda.

**Decisión clave 4 — Material Editor: eliminado modo TwoColumns, layout siempre Vertical:** pre-F2H44 el panel tenía layout adaptativo: `>= 540px` = 2 columnas (controles izq | preview der), `< 540px` = vertical (preview arriba). El modo TwoColumns estaba roto en práctica: `ImGui::Columns` no sincroniza alturas, y al docking en otros lados (workspace `materials` lo ensancha) el preview quedaba flotando en el medio del panel desconectado del label "Preview" — el dev reportó *"si se mueve el panel, ya se rompe"*.

- **Razón eliminar vs arreglar:** arreglar requería sincronizar alturas manualmente (`ImGui::SetCursorPosY` calculado) o migrar a `ImGui::Tables` (refactor mayor). Eliminar es más simple y robusto: ~20 LOC menos, layout vertical funciona en cualquier ancho/dock sin sorpresas.
- **Trade-off aceptado:** en monitores muy anchos no se puede ver preview + controles lado a lado. Marginal: el panel se usa en bursts cortos (ajustar metallic, ver preview, repetir), el scroll vertical en cualquier ancho es aceptable.

**Decisión clave 5 — USER_GUIDE/* + README + GIF descartados:** era el bloque 5 propuesto en mi auditoría inicial. El dev rechazó textualmente: *"seguiremos agregando cosas que luego vamos a terminar cambiando"*. Razón sólida: el motor sigue evolucionando hacia sub-fase 2.5+ (diálogos/quests/inventario). Documentar workflows ahora generaría docs que rotan más rápido que la implementación. Hito propio post-Fase 2 cuando el motor estabilice y los workflows queden congelados.

**Decisión clave 6 — Welcome modal carga "Personaje animado" (Fox.glb) como demo, no Stress Scene:** opciones consideradas: (a) Stress Scene completa, (b) Shadow demo, (c) Personaje animado, (d) Combo Floor + columna + luz + Fox + trigger. Elegido (c).

- **Razón:** visual fuerte sin saturar. Stress Scene (200 cubos + 64 luces + esferas + Fox + CesiumMan + fuego + trigger) intimida y ralentiza editores low-end al primer arranque. Shadow demo es estático (poco impactante). Combo es excesivo para "primera impresión". Personaje animado: 1 entidad, animación visible inmediatamente, demuestra render + animación + assets sin sobrecargar.

**Decisión clave 7 — subagente NO usado en F2H44:** a diferencia de F2H43 (255 keys en 23 archivos = caso textbook de delegación), F2H44 fueron 5 cambios pequeños y heterogéneos que no encajaban en un patrón mecánico. Cada bloque requirió judgment calls puntuales (categorías Add Component, choice del demo, mappings de migración workspaces, layout del Material Editor, gates del Ctrl+wheel). Hacer todo en main context fue lo correcto.

**Alternativas descartadas en F2H44:**
- **AddComponentCommand undoable:** ver decisión 2.
- **Top-level menú "Ejemplos" en MenuBar:** descartado a favor del botón en Welcome modal. El menú top-level requiere otro click + menos descubrible (un dev nuevo no abre "Ejemplos" antes de saber qué es). El botón en Welcome es VISIBLE inmediatamente al primer arranque.
- **Migrar workspaces a `Workspace { id, name }` con campos separados:** ver decisión 3 (paralela). Demasiado refactor para ganancia chica. Mantener un solo campo `name` (semánticamente ID ahora) es más simple.
- **Material Editor con `ImGui::Tables` en lugar de `Columns`:** Tables sincroniza heights mejor pero es más verbose y el problema (preview separado del label) se resuelve sin él. Hito futuro si emerge presión real (ej. necesidad de mostrar preview + controles + node-graph).
- **Tooltip toolbar tools:** mi crítica original incluía esto pero al verificar el código (`Toolbar.cpp` línea 29-31) ya tenía tooltips wired desde F2H36+F2H43. Falsa alarma; no se hizo nada.

**Condiciones de revisión:**
- Si emerge necesidad de undo del Add Component → implementar AddComponentCommand templated genérico + serializar el estado pre-add (default values del componente).
- Si los workspaces crecen a >6 (ej. animation/timeline/profile) → considerar moverlos a `assets/workspaces/*.json` con definiciones declarativas (icono, ID, layout default).
- Si el dev en algún momento habilita docs externos (post-Fase 2) → empezar por GIF demo de 30s (motor en acción, primer arranque a juego corriendo) + docs/USER_GUIDE/GETTING_STARTED.md (5 pasos: open → spawn brush → texturizar → physics → script).

---

## 2026-05-10: F2H43 cierre — sistema de i18n completo (Editor + HUD + Player)

**Contexto:** el plan F2 original tenía F2H5 (i18n del editor, ~500 strings) + F2H34 (i18n de gameplay, diálogos/items/quests) que nunca se hicieron — la deuda se acumuló durante 40 hitos. F2H41 unificó los strings del HUD a inglés *"sienta baseline para futura selección de idioma"* con la promesa explícita de hacer i18n después. F2H43 ataca los dos hitos originales fusionados en uno: infra + barrido completo del editor + barrido del HUD/Player + Player MainMenu.

**Decisión clave 1 — API namespaced `Mood::I18n::T("key")` (no clase singleton):** mismo patrón que `Mood::Log::engine()` y el nuevo `Mood::UserSettings::*`. Funciones libres en namespace, estado privado en `namespace { ... }` del .cpp. La función se llama `T()` (corta a propósito porque se usa 500+ veces). Variante con interpolación `T("key", args...)` usa `fmt::runtime` + `fmt::format` para placeholders `{}` estilo Python/Rust.

- **Razón vs `gettext` lib externa:** una dependencia menos. La app no necesita `_()`, plurales, contextos, ni catalogos compilados `.mo` — un `unordered_map<string, string>` por idioma alcanza para los ~500 strings actuales.
- **Razón vs clase singleton instanciable:** las clases singleton requieren `I18n::instance()->T(...)` que es ruidoso visualmente. La función libre `Mood::I18n::T(...)` lee como llamada a función pura.
- **Razón warn-once por key faltante:** spammar el log con cada frame de render que pase por una key faltante haría el log inutil. `unordered_set<string> s_warnedKeys` registra los warns ya emitidos; reseteable al `setLanguage()`.

**Decisión clave 2 — keys flat con dot notation (no nested JSON):** `"editor.menu.file": "Archivo"` en lugar de `{"editor": {"menu": {"file": "Archivo"}}}`. El JSON se carga a un `unordered_map<string, string>` lineal con la key como literal completo. La dot notation es solo convención de naming para grep + agrupación visual.

- **Razón vs JSON nested:** lookup directo O(1) en map plano sin traversal recursivo. Diff visual más fácil (cada key en su línea, no anidamiento). Permite que `_comment` (metadata) coexista con keys reales sin estructura especial.
- **Trade-off:** más verboso al editar (todas las keys del namespace `editor.menu.file.*` están dispersas alfabéticamente, no agrupadas por sub-objeto). Mitigado: agrupamos manualmente con líneas en blanco entre namespaces.

**Decisión clave 3 — diccionarios SIN tildes en `es.json`:** `MUNICION` en vez de `MUNICIÓN`, `RESISTENCIA` en vez de `RESISTENCIA` (correcto), `PROXIMO` en vez de `PRÓXIMO`. Reemplazos sistemáticos: `á→a / é→e / í→i / ó→o / ú→u / ñ→n / ¿→? / ¡→!`.

- **Razón:** la font del MoodPlayer (ProggyClean default ImGui, charset Basic Latin solo) no cubre Latin-1. Los caracteres con tilde se renderizarían como tofu (`?`) durante gameplay. El editor (Lato F2H38) sí soporta Latin-1, pero mantenemos paridad para tener UNA fuente de verdad consistente.
- **Mitigación de la deuda:** cuando se cambie el Player a Lato (deuda anotada en pendientes desde F2H38), agregar tildes es un find-and-replace mecánico en `es.json` sin tocar código. Hito chico cuando emerja presión visual (un usuario hispanohablante reportando que el HUD se ve raro).

**Decisión clave 4 — persistencia GLOBAL del idioma (`%APPDATA%\MoodEngine\settings.json`), no per-proyecto:** `core/UserSettings.cpp` con un singleton namespaced minimalista. Editor + Player ambos llaman `UserSettings::init() + I18n::init(UserSettings::language())` al arrancar. Path resolver usa `std::getenv("APPDATA")` (Windows) con fallback al cwd.

- **Razón vs per-proyecto en `.moodproj`:** el idioma es preferencia del USUARIO, no del proyecto. Un colaborador hispanohablante abriendo un proyecto creado por un brasileño NO debería ver portugués. La preferencia es de la persona, debe persistir entre proyectos.
- **Razón abre la puerta:** futuro `settings.json` puede crecer con `theme`, `recent_files_limit`, `editor_font_size`, etc. sin tener que reinventar la infra.
- **Trade-off:** el `.moodproj` no recuerda el idioma del autor. Si el dev cierra el proyecto y lo reabre meses después en otra máquina con otro idioma activo, ve la UI en el otro idioma. Aceptable: las traducciones son consistentes (no se "pierde" información), solo cambian las labels visuales.

**Decisión clave 5 — workspace names NO traducidos en F2H43:** `Layout`/`Programar`/`Materiales`/`Editor de mapas` quedan sin envolver en `T()`. Razón: viven en `WorkspaceManager.cpp` con el nombre como ID persistido en `.moodproj` (la selección del workspace activo se guarda como string `"name": "Editor de mapas"`). Traducirlos requiere refactor: separar nombre INTERNO (ID estable, ej `"map_editor"`) del label MOSTRADO (`T("workspace.map_editor")`). Diferido a hito propio chico cuando emerja necesidad.

**Decisión clave 6 — subagente para barrido masivo del editor:** 23 archivos / 255 keys con misma transformación mecánica = caso textbook de delegación según la regla global del repo (>15 archivos misma transformación = subagente). Estrategia: bloque C1 (MenuBar, ~60 keys) hecho manual primero como piloto del patrón → bloque C2 con subagente con la convención ya validada (naming, qué traducir, qué no, IDs internos de ImGui, iconos FA fuera del JSON, etc).

**Alternativas descartadas en F2H43:**
- **`gettext` o `ICU`:** dep externa innecesaria para 500 strings. Plurales, contextos, catalogos `.mo` compilados → overengineering para el scope actual.
- **i18n via Qt `tr()`:** Qt no es nuestro UI framework (usamos ImGui). El `tr()` de Qt está acoplado al meta-object system.
- **Default Inglés:** descartado. El dev trabaja en español, arrancar en su idioma evita un click extra al iniciar el editor.
- **Selector de idioma en menu top-level Settings:** descartado. No tenemos panel Settings y crearlo solo para el idioma sería overkill. `Ver > Idioma` es donde el dev mira primero.
- **Lua scripts traducibles en F2H43:** descartado por scope. Los string literals dentro de `assets/scripts/*.lua` (ej. `hud_demo.lua` con `"Demo: explore the test map"`) requieren un binding Lua adicional `T("...")` + sweep de los .lua. Hito chico futuro cuando aparezca uno con scripts gameplay reales (no demos).
- **Hot-reload del JSON al cambiar el archivo en disco:** descartado. Útil para developers de traducciones pero overhead innecesario para el caso de uso (devs editan JSON + reinician editor para ver cambios).

**Condiciones de revisión:**
- Si aparece presión de un usuario hispanohablante reportando tofu en el HUD del Player → ejecutar el fix Lato Player (anotado pendiente desde F2H38) + agregar tildes a `es.json`.
- Si el catálogo de keys crece a >2000 (ej. con scripts Lua + diálogos + items + quests de Sub-fase 2.5) → considerar partir el JSON por dominio (`editor.json` / `hud.json` / `gameplay.json`) o migrar a un formato compilado (binary keyset map) para arranque más rápido.
- Si emerge necesidad de un 3er idioma (portugués, inglés UK, etc) → solo agregar `pt.json` / `en_uk.json` y entradas en el menú. La infra ya soporta arbitrarios via `enum class Language` extensible.
- Si emerge necesidad de plurales (ej. "1 entity" vs "5 entities" en HUD), evaluar agregar lib externa (gettext) o resolver inline con condicional en el caller (overhead chico).

---

## 2026-05-10: F2H42 cierre — optimización runtime (shadow caching + VSync toggle)

**Contexto:** F2H39 original "optimización runtime" diferido para PC de escritorio (GTX 1660 / Ryzen 5 5600G del baseline F2H2-F2H6) — el dev movió el trabajo a la desktop. Tracy Profiler v0.11.1 ya integrado vía CPM con `MOOD_PROFILE` cmake option (estaba OFF por default en Release; F2H42 lo activa). Stress scene generada por nuevo handler `processSpawnFullStressSceneRequest` que dispara los 8 spawners individuales: 200 cubos + 64 point lights + 9 esferas PBR + shadow demo + Fox + CesiumMan + fuego + trigger Lua = 285 entidades / 17K tris.

**Decisión clave 1 — shadow map caching por hash de escena (no dirty flags):** hash FNV-1a 64 incremental cada frame de (`lightDir` + `sceneCenter` + `sceneRadius` + por entidad con MeshRenderer: `position/rotationEuler/scale/mesh id`). Si coincide con frame anterior y `m_shadowMapValid`, skip `m_shadowPass->record(...)` y reusa la GLTexture existente. Resultado: ShadowPass::record cae de 13.4 ms/frame a **278 μs/frame (-98%)**, cache hit rate **99.996%** en escena estática. Hash overhead: 32 μs por frame.

- **Razón hash vs dirty flags:** el ECS no tiene dirty flags por componente. Implementarlos requeriría hookear todos los call sites que mutan transforms (Inspector, gizmos drag, Lua scripts, `AnimationSystem::update`, `Physics::updateRigidBodies`, undo/redo commands, deserialización de saves). Hash es O(N) con N = entidades MeshRenderer — barato (32 μs en stress de 285 entidades) y captura **cualquier** mutación sin tocar call sites. Si la escena crece a >5000 meshes, re-evaluar hash incremental por chunk + mark-dirty desde mutators críticos.
- **Razón FNV-1a vs xxHash o std::hash:** FNV-1a es 4 líneas de código sin dependencias, suficientemente bueno para detectar cambios (no es crypto). std::hash<glm::vec3> no existe out-of-the-box. xxHash sería más rápido pero requiere lib externa para una optimización marginal.
- **Edge case off→on:** si `shadowEnabled` pasó de false a true entre frames, `m_shadowMapValid=false` aunque el hash coincida. Sin esto, reactivar luz direccional con escena idéntica reusaría una shadow map vacía (nunca se generó).

**Decisión clave 2 — VSync toggle en Performance panel (no en menú global):** método `Window::setVSync(bool)` + getter `vsyncEnabled()` + checkbox "VSync (60fps cap)" en `PerformanceHudPanel`. Patrón request/consume (`consumeVsyncToggleRequest` → EditorApplication aplica via Window). Sync inicial del checkbox con estado real del Window en `EditorApplication_Init.cpp` (cubre edge case del driver que rechace vsync al crear contexto).

- **Razón Performance panel vs menú global:** el dev mide FPS / frame_ms / draws acá. Agregar el toggle en el mismo panel lo hace descubrible cuando ya está midiendo. Patrón consistente con resto del editor (request flags consumidos por EditorApplication, no callbacks).
- **VSync ON por defecto:** mantenemos el default histórico. El toggle es para medir, no para producción. Sin VSync el GPU corre al 100% en escenas pequeñas tirando energía y calor sin razón.

**Decisión clave 3 — skybox reorder probado y revertido:** hipótesis era que dibujar skybox post-PBR con `depth=1 LEQUAL` solo pintaría píxeles donde la geometría no escribió, reduciendo overdraw del fragment shader. Test4 (17913 frames sin VSync): empate dentro del ruido (~744 fps reorder vs ~776 fps original).

- **Por qué no funcionó:** el "1.14 ms del skybox" en test3 NO era trabajo del fragment shader (que es trivial: 1 sampler read del cubemap o 2 atan/asin del equirect). Era **GPU sync** absorbido por el scope (CPU-side stall esperando que la GPU termine trabajo encolado). Mover el skybox al final solo redistribuye el sync entre Skybox/swapBuffers/endFrame; el costo total no baja.
- **Por qué revertir:** sin ganancia medible + rompe el comentario doc del SkyboxRenderer.h ("Llamar PRIMERO en el frame para que la escena escriba encima") + convención estándar de motores (skybox-first es el patrón estable). Mantener el cambio agregaría carga cognitiva sin retorno.

**Decisión clave 4 — cierre temprano del hito a 780 FPS de headroom:** test3 (sin VSync, post-shadow-cache) muestra stress scene a **780 FPS / 1.27 ms-frame**. Sistemas gameplay combined <0.2 ms. Skybox 1.14 ms (real cost). ImGui ~0.4 ms. PBR pases <0.1 ms cada uno. **17x headroom sobre el baseline pre-fix.** No tiene sentido invertir más esfuerzo cuando el motor está sobrado para contenido real. Optimizaciones diferidas (GPU timestamp queries, CSM cascadas, frustum cull shadow pass): hito propio si emerge presión real (escenas >1000 meshes, target VR, mobile port).

**Alternativas descartadas en F2H42:**
- **GPU timestamp queries (`glQueryCounter` con `GL_TIMESTAMP`):** descartado por scope. Mediría el costo GPU real en lugar del CPU stall, pero requiere buffering 2-3 frames + manejo async + lectura diferida. ~2-3 horas de trabajo para diagnóstico que con 17x headroom no se necesita. Hito propio si emerge presión.
- **CSM (Cascaded Shadow Maps):** descartado. La shadow map actual de 2048x2048 + bounding sphere fijo (radius=30m al origen) es suficiente para escenas tipo Hito 20. CSM aporta calidad en escenas grandes / outdoor amplios — no es el caso aún.
- **Frustum culling del shadow pass:** descartado. Con cache 99.996% hit rate, el costo del record es prácticamente cero. Frustum cull sumaría complejidad para optimizar el 0.004% de los frames donde sí se renderiza.
- **Pre-renderizar skybox a cubemap LDR cacheado:** descartado. El cubemap mode YA usa cubemap (no es procedural). El equirect sí podría convertirse a cubemap on-load, pero el costo del shader (2 atan + 1 textureLod) es trivial y la "mejora" de 1ms es ruido a 780 fps.

**Condiciones de revisión:**
- Si la escena crece a >5000 meshes con MeshRenderer, re-evaluar el hash O(N) — podría ser >100 μs y empezar a pesar.
- Si target shifts a VR (90+ fps requeridos x 2 ojos = 180 fps eye) o mobile (30 fps target con thermal throttling), volver a evaluar GPU side: timestamp queries + CSM + Forward+ tile culling más agresivo.
- Si emerge necesidad de profile post-mortem en producción, agregar export Tracy automático al cerrar el editor.

---

## 2026-05-09: F2H41 cierre — 5 widgets HUD diferidos + 3 fixes laterales + i18n unificado

**Contexto:** F2H39 dejó explícitamente diferido un paquete de 5 widgets HUD adicionales (CompassBar / ObjectiveText / KillFeed / Stamina / CRT scanline) — la lista de la "wishlist" del dev (Half-Life base + Doom/Fallout/Metro/CoD). F2H41 ataca ese pendiente directo, manteniendo el contrato del framework establecido en F2H39: agregar widgets toca máximo 4 lugares (función `drawXxx`, registry, HudState, bindings Lua si necesita). Hito mediano (~3h, ~750 LOC neto) que entrega los 5 widgets + 3 fixes reactivos al feedback del dev durante validación + unificación i18n del HUD.

**Decisiones técnicas clave:**

- **Único cambio en arquitectura del framework: `HudContext` gana `glm::vec3 cameraForward`.** Razón: CompassBar necesita yaw del player. Default `vec3(0,0,-1)` para tests que no inicializan cam. Callers (`EditorApplication::drawGameOverlay` y `PlayerApplication_Frame::endFrame`) leen `m_playCamera.forward()` y lo pasan. Alternativa descartada: leer cam global o singleton — pasar explícito vía context es testeable y libre de orden de init.

- **CompassBar yaw via `atan2(forward.x, -forward.z)`, no `FpsCamera.yaw` interno.** Razón: el compass es UI derivada — `forward` es lo que el player ve, derivar el yaw de ahí garantiza consistencia visual con la cámara real. Si emerge inconsistencia (ej. cam vertical pura donde `forward.x≈0 && forward.z≈0`), el widget abortea con guard `if (std::abs(f.x) < 1e-6f && std::abs(f.z) < 1e-6f) return`. Convención: `yaw=0=N=-Z`, `90=E=+X`, `180=S=+Z`, `270=W=-X`. Tickmarks cada 15° con cardinales destacados (1.5x altura + texto). Rango visible ±90° (180° total).

- **CRT scanline default OFF, los demás 12 default ON.** Razón: efecto retro divisivo (Pip-Boy) que no encaja con todo proyecto. `widget_enabled` initializa con `crt_scanline=false`; toggle desde Lua via `setWidget("crt_scanline", true)`. Patrón explícito: el widget se dibuja siempre que esté en el registry y enabled — el opt-out vía init del map preserva el contrato de F2H39 (registry hardcoded, toggle dinámico).

- **StaminaBar bypass si `max_stamina<=0`.** Razón: gameplay sin stamina (proyectos puzzle/walking sim) no debería consumir área de pantalla con una barra inútil. Default `100/100` para que el demo y proyectos típicos arranquen con la barra visible. Alternativa descartada: requerir `setWidget("stamina_bar", false)` explícito — feo por default; el bypass es semántico correcto.

- **KillFeed con `KillEntry { string text, ImU32 color, float ttl }` + lifetime 4s.** Razón: longer lifetime que `pickup_queue` (2.5s) porque el dev quiere ver kills sin perderlos en combate intenso. Cap visual 5 entradas. Color custom permite diferenciar enemy types/headshots/etc — `pushKillColored(text, r, g, b)` desde Lua. Patrón ttl idéntico a F2H39 pickup_queue (decremento con `dt`, popea en `<=0`, fade in/out por edges) — consistencia interna.

- **Helpers de los 4 widgets con state mutable viven en `GameState::*`, no `GameOverlay::*`.** `pushKill / pushKillColored / setObjective / clearObjective / setStamina / setMaxStamina / etc` se definen en GameState por la misma razón que F2H39: LuaBindings linkea contra GameState (puro state, sin ImGui), pero NO contra GameOverlay (depende de ImGui). Mover los helpers a GameState los hace invocables desde Lua sin arrastrar ImGui al modulo de tests `mood_tests.exe`. Patrón establecido en F2H39, F2H41 lo respeta.

- **Lua bindings expandidos en la misma tabla `hud`, no nueva tabla.** 10 funciones nuevas (`setStamina`, `setMaxStamina`, `getStamina`, `getMaxStamina`, `setObjective`, `clearObjective`, `getObjective`, `pushKill`, `pushKillColored`) sobre las 24 ya existentes (Hito 20 + F2H39). Razón: scripts pre-F2H41 siguen funcionando sin cambios; nuevos pueden mezclar libre. Una sola tabla simplifica la API mental. Alternativa descartada: tabla nueva `hud_combat` o `hud_v2` — segmentaría confusamente.

**3 fixes laterales descubiertos en validación visual con dev:**

- **Hover/pick spurious en Hierarchy panel durante Play Mode.** Síntoma observado: cada vez que el dev miraba un elemento en el viewport durante Play, el ítem correspondiente del Hierarchy se marcaba hovered/seleccionado. Causa: `SDL_SetRelativeMouseMode` captura el cursor para FPS look pero ImGui sigue viendo la posición OS warpeada al centro del viewport — y el Hierarchy panel está debajo del centro en muchos layouts, así que ImGui interpreta cada frame que el mouse "está sobre" la primera entry del Hierarchy. Fix: `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)` en `EditorApplication::beginFrame` cuando `m_mode == EditorMode::Play && !GameState::paused()`. Off-screen pos → ImGui no hovera nada. Cuando paused, ImGui vuelve a ver pos real (cursor visible). Aplica solo al Editor — el Player no tiene panels.

- **Caminata "muy lenta con pasitos pegados".** Feedback dev: *"siento que avanzo muy lento, los pasos están muy pegados uno del otro"*. Causa: walkSpeed 4.0 m/s + headbob 5 Hz daba stride visual ~80 cm por paso = denso/agitado. Fix: `k_walkSpeed 4.0→5.5 m/s` (paso normal humano ~5 km/h ≈ 1.4 m/s; FPS convencional 5-6 m/s para feel ágil), `k_crouchSpeed 2.0→3.0`, `k_bobFreq 5.0→3.5 Hz` (paso ~1.6 m a 5.5 m/s = stride realista), `k_bobAmp 0.04→0.05 m`. Aplicado en Editor + Player (paridad — ambos usan mismas constantes en `EditorPlayMode.cpp` y `EditorScene.cpp`). Decisión: estos números son tuning dependiente del feel — quedan ajustables por proyecto en futuro hito (config en `.moodproj`).

- **Spawn no centrado.** Feedback dev: *"en que ubicacion spawnea el usuario? no debería aparecer en el punto 0,0 para que siempre esté en el centro del mapa?"*. Causa: legacy hardcoded `(-4.5, 1.6, 7.5)` heredado del placeholder buildTestMap (esquina del 8x8 grid). Fix: cambio a `(0, 1.6, 0)` en 4 lugares: `EditorApplication.h:m_playCamera` default, `EditorPlayMode.cpp:exitPlayMode` reset, `PlayerApplication.h:m_playCamera` default, `PlayerApplication_SaveLoad.cpp:applyLoadedSave` reset. Convención del motor: spawn al centro del mapa. Para mapas con spawn custom (ej. proyectos con `SpawnPoint` entity), F2H futuro agregará lookup en runtime; por ahora `(0,1.6,0)` es el default sano.

**i18n unificado a inglés** post-feedback dev *"no se si he visto cosas en español e ingles mescladas, no es lo ideal, si a futuro tenemos seleccion de idioma no es lo ideal"*:

- **Decisión: unificar todo el HUD a inglés** (no a español). Razón: convención FPS HUD ya dominante en el codebase pre-fix (HEALTH / AMMO / STAMINA / PAUSED / RESERVE inglés mientras OBJETIVO / CONTINUAR / OPCIONES estaban en español). Inglés es lingua franca de games + matchea estética HL/CoD/Doom inspiración. Strings cambiados: `OBJETIVO:`→`OBJECTIVE:`, `CONTINUAR`→`CONTINUE`, `OPCIONES`→`OPTIONS`, `"Salir al editor"`→`"EXIT TO EDITOR"` (EditorPlayMode), `"Salir al menu"`→`"EXIT TO MENU"` (PlayerApplication_Frame), demo Lua `"Demo: explorar el mapa de pruebas"`→`"Demo: explore the test map"`, `"[E] Levantar item demo"`→`"[E] Pick up demo item"`.
- **Comments del código + log lines siguen en español.** Razón: son dev-facing, no user-facing. La unificación cubre solo strings que aparecen en el HUD/UI del jugador.
- **Sienta baseline para futura selección de idioma** — los string literals serán keys de translation table (`tr("OBJECTIVE")` o similar) sin tener que tocar lógica del HUD. El refactor cuando se implemente i18n será mecánico: wrap de literals en `tr()` + JSON con traducciones por idioma.

**Alternativas descartadas explícitamente:**

- **Vector dinámico de widgets en lugar de array hardcoded:** descartado en F2H41 (igual que F2H39). 13 widgets stable; vector dinámico agregaría runtime registration sin ganancia hasta que aparezca un caso real (ej. plugins / mods que registren widgets propios).
- **HUD diegetic 3D incluido en F2H41:** descartado por scope. Requiere FPS arms (mesh + animator del brazo) que no existen aún. Hito propio mayor futuro.
- **Mini-map / Radar incluidos en F2H41:** descartado por scope. Requiere render-to-texture topdown del mundo cercano — diferente arquitectura del HUD que es pure DrawList. Hito propio mediano futuro.
- **Themes alternativos (Doom saturado / Fallout verde) incluidos en F2H41:** descartado por scope. Requiere theme runtime + bindings Lua. Hito chico propio futuro.
- **Sound feedback (hit marker sound, low HP heartbeat):** descartado por scope. Requiere audio bindings en GameState — fuera del paquete HUD visual.
- **Compass con objective markers proyectados a screen-space:** descartado en v1. Requiere world-space objectives con coordenadas — agregar después si emerge necesidad real (gameplay con waypoints).
- **Implementar i18n completo en F2H41:** descartado. F2H41 unifica strings (baseline) pero translation table queda como hito propio cuando emerja la necesidad real (dev quiere shipping multi-idioma). Sin urgencia ahora.

**Condiciones de revisión:**

- Si emerge feedback de que el HUD necesita más widgets stable (>20), considerar refactor del registry a vector dinámico con plugin pattern.
- Si CompassBar yaw derivation tiene jitter (especialmente cam casi vertical), agregar damping o switch a `FpsCamera.yaw` interno.
- Si CRT scanline a 3 px de espaciado se vuelve cuello en pantallas 1440p+, aumentar spacing a 4-5 px o convertir a shader post-process.
- Si el walk speed 5.5 m/s se siente muy rápido para un proyecto específico, agregar `walk_speed` al `.moodproj` config (ya hay precedente en F2H40 con coyote/jump buffer windows per-proyecto).
- Si emerge proyecto multi-idioma, agregar el sistema i18n con translation table + lookup en HUD strings (ya unificadas).

---

## 2026-05-09: F2H40 cierre — Fix físicas Floor scale-RigidBody desync

**Contexto:** Bug físicas descubierto durante validación de F2H39. El dev no podía pararse en el Floor para ver los widgets dinámicos del HUD porque caía infinito tras enlargar el piso vía Inspector. Pre-F2H40 cuando el `Transform.scale` de una entidad con `RigidBody` cambiaba (Inspector / gizmo / script), el `RigidBody.halfExtents` no se sincronizaba — el visual cambiaba pero la colisión Jolt quedaba al tamaño del body inicial. Mini-hito chico (~45 min, ~80 LOC neto) que cierra el bug + previene futuros desync similares.

**Decisiones técnicas clave:**

- **Auto-sync `halfExtents = Transform.scale * 0.5` solo para Box bodies.** Razón: en Box, el campo `halfExtents` es realmente la mitad del tamaño en 3 ejes — sincronizar con `t.scale * 0.5` es semánticamente intuitivo y matchea cómo el codebase crea bodies (ver `EditorScene.cpp:104` Floor, `:142` Tile, `DemoSpawners` físicas demo). Sphere/Capsule tienen significados distintos para `halfExtents.x` (radio en Sphere; halfHeight en Capsule), no escalan uniformemente desde un `Transform.scale` potencialmente no-uniforme. Si el dev quiere cambiar radius/height de Sphere/Capsule, lo edita en el Inspector field directamente y el caso 2 de abajo lo sincroniza.

- **Re-sync via `setBodyHalfExtents` existente, no destroy+recreate.** `PhysicsWorld::setBodyHalfExtents` (introducido en hitos previos) llama Jolt `BodyInterface::SetShape` que recrea el collider preservando pose + velocity + contacts del body. Importante para Dynamic bodies escalados mid-frame (script Lua o gizmo en Editor Mode con un cube físico). Alternativa descartada: destroy + recreate body — pierde la pose actual + velocidad, lo que rompe Dynamic bodies en movimiento.

- **Cache `lastSyncedHalfExtents` en `RigidBodyComponent`** para detectar cuándo invocar el resync. Inicializado en `vec3(0)` para forzar primer sync inmediato post-materialización. Updated después de cada `setBodyHalfExtents` exitoso. NO se serializa (estado runtime puro). Alternativa descartada: dirty flag `bool needsResync` que el Inspector / gizmo prendan al editar — requiere hookear todos los puntos de mutación del Transform/halfExtents (Inspector partials, gizmo, script bindings); más invasivo que el polling con epsilon compare.

- **Pase de re-sync corre cada frame en `updateRigidBodies`** después del materializar inicial. Overhead negligible (epsilon compare de 3 floats por entity con RigidBody, ~30 ns); a 1000+ entities con RigidBody el costo total queda <30 µs por frame. Si emerge presión de perf en escenas extremas, mover a un dirty-flag system.

- **Aplica también a `PlayerApplication::updatePhysics`** (paridad con Editor). Razón: scripts Lua pueden mutar `Transform.scale` en Play Mode (ej. growing/shrinking effect); saves cargados pueden tener Floor con scale enlargado y el body se recrea con halfExtents derivados de scale del JSON correctamente — pero si el dev guardó un scale modificado en editor pre-F2H40, el sync recién ocurre en F2H40. Patrón idéntico al Editor.

- **Lógica auto-sync ANTES del re-sync condicional**, no en orden inverso. Razón: el caso (a) (Box scale change) muta `rb.halfExtents`; el caso (b) (resync al body) compara `halfExtents` contra `lastSyncedHalfExtents`. Si la lógica corriera en orden inverso, el caso (b) primero observaría el `halfExtents` viejo (pre-mutación) y no detectaría el cambio que el caso (a) iba a aplicar el siguiente frame — un frame de lag visible. Con el orden actual, cualquier cambio se aplica el mismo frame.

**Alternativas descartadas explícitamente:**

- **Sync Transform.position / rotation al body en runtime para Static bodies**: descartado para F2H40 por scope. Ya hay pattern para Dynamic bodies (`step` lee del body al Transform); el caso opuesto (Editor mueve un Floor en Editor Mode → body se actualiza) no estaba en el bug original. Si emerge necesidad real (ej. dev mueve Floor con gizmo y luego entra a Play y el body queda en posición vieja), agregar como hito chico — patrón análogo: comparar `lastSyncedPosition` y llamar `setBodyPosition`.
- **Opt-out flag** `bool autoSyncBoxToScale = true` en RigidBodyComponent: descartado en v1. La convención en todo el codebase actual es `halfExtents = scale*0.5` para Box bodies. Si emerge un caso de uso intencional (ej. Box visual deformado pero colisión cilíndrica más chica), agregar el flag entonces.
- **Refactor del PhysicsSystem a event-driven** (Inspector dispatcha "transform changed" event que el sistema atiende): descartado por scope. Polling con epsilon compare es más simple y suficiente para cualquier escena razonable.

**Condiciones de revisión:**

- Si emerge un demo / entity-creator del codebase con `halfExtents != scale*0.5` intencional (Box body con tamaño distinto al visual), agregar el opt-out flag.
- Si en escenas con 1000+ Box bodies el polling se vuelve cuello del frame (>0.1 ms), refactor a dirty-flag pattern: el Inspector + gizmo + script bindings setean `needsResync=true` al mutar Transform/halfExtents.
- Si emerge necesidad de sync Transform.position/rotation a Static bodies (dev mueve Floor en Editor → body en posición nueva), agregar como hito chico siguiendo el mismo patrón de `lastSyncedPosition`.
- Si Jolt actualiza la API y `BodyInterface::SetShape` deja de existir o cambia comportamiento (preserva pose vs re-spawnea), revisar `PhysicsWorld::setBodyHalfExtents` y este sync flow.

---

## 2026-05-09: F2H39 cierre — HUD framework extensible + paquete inicial estilo HL/Doom/Fallout

**Contexto:** El F2H39 original era "optimización runtime", pospuesto a hito futuro porque el dev está en notebook (Iris Xe) y el baseline de profiling de F2H2-F2H6 se hizo en su desktop (GTX 1660 / Ryzen 5 5600G) — números no comparables. PERFORMANCE.md además explícitamente concluye que sub-fase 2.1 cumplió: *"el motor está listo para contenido real"*. F2H39 pivota al pendiente "HUD del juego procedural/minimalista" anotado post-F2H35. Dev confirma estética **Half-Life como inspiración base** del motor + influencias Doom/Doom Eternal/Fallout/Metro/CoD: *"abarcar todas las posibles cosas de HUD que necesitaremos para futuro, y que se puedan ir agregando más"*. Hito mediano (~3h, ~700 LOC neto) que entrega framework extensible + 8 widgets default + bindings Lua + bug fix lateral.

**Decisiones técnicas clave:**

- **Widget pattern como struct simple, no clase con vtable.** `HudWidget { const char* name, void (*draw)(HudContext&) }` en array hardcoded ordenado back-to-front. Razón: ~10 widgets → vtable overhead innecesario; función pura es testeable + sin estado oculto. Alternativa descartada: `class HudWidget { virtual void draw() = 0; }` con registry dinámico de unique_ptr — mayor complejidad sin ganancia para 10 widgets stable.

- **Helpers de mutación de HudState viven en `GameState`, no `GameOverlay`.** `triggerHitMarker / triggerDamageFlash / pushPickup / clearInteractPrompt` se movieron de `GameOverlay::*` (donde inicialmente los puse) a `GameState::*` durante implementación, tras encontrar **link error en `mood_tests.exe`**: LuaBindings linkea contra GameState (puro state) pero NO contra GameOverlay (depende de ImGui). Pasar los helpers a GameState (sin deps gráficas) los hace invocables desde Lua sin arrastrar ImGui al modulo de tests. Costo: división conceptual entre state mutation (GameState) vs rendering (GameOverlay) — limpia, no convoluta.

- **`PickupNotification` usa `ttl` countdown, no `spawnTime` absoluto.** Refactor durante el fix anterior: la versión inicial usaba `spawnTime = ImGui::GetTime()` en `pushPickup`, pero al moverse a GameState (sin ImGui) hay que sustituir. Patrón ttl: `pushPickup` setea `ttl=2.5f`, el overlay decrementa `n.ttl -= dt` cada frame y popea cuando ttl<=0. Fade in/out se computa de `(k_lifetime - ttl)` para age in y `ttl` directo para age out. Limpio + sin clock dependency.

- **Lua bindings expandidos en la misma tabla `hud`**, no nueva tabla. 18 funciones nuevas (`setMaxHp`, `setMag`, `setMaxMag`, `setReserve`, `setInteractPrompt`/`clearInteractPrompt`/`getInteractPrompt`, `showHitMarker`, `flashDamage(x,y)`, `pushPickup`, `setWidget(name, on)`, `isWidgetEnabled`) preservando las 6 originales del Hito 20 (`setHp`/`setAmmo`/`setPaused` + getters). Razón: scripts pre-F2H39 siguen funcionando sin cambios; nuevos pueden mezclar libre. State-based (no draw calls directos) — el patrón existente escala perfecto al framework expandido. Alternativa descartada: tabla nueva `hud2` o `hud_ext` — segmentaría la API confusamente.

- **Paleta Half-Life hardcoded en `palette` namespace** (no theming runtime). Razón: F2H39 v1 enfoca arquitectura + paquete inicial; theming es scope secundario. Si emerge necesidad real (modo Doom Eternal saturado, modo Fallout monocromo verde), agregar `HudTheme` struct + `setTheme` binding como hito propio. Mientras tanto, el dev edita `palette::*` constexprs si quiere ajustar.

- **`PixelSnapH = false` en Lato + `true` en FA** (heredado de F2H38). HUD usa el mismo atlas, los icons FA salen con snap-to-pixel y el texto Lato smooth. Sin override en GameOverlay.

- **Pause menu rediseño Doom-style con chevrons en hover**: 4 líneas naranjas externas marcando las 4 esquinas del rect del botón. Solo visibles en hover — efecto "geometric Doom" sin saturar el feel. Alternativa descartada: glow shader para el botón hovered — requiere shader pass, scope mayor.

- **SaveLoad: campos nuevos opcionales solo se serializan si difieren del default** (ej. `if (d.hud.max_hp != 100) j["hud"]["max_hp"] = d.hud.max_hp`). Razón: saves chicas para gameplay basico — un save de demo no incluye 7 keys nuevas si los valores son default. Patrón mismo que `coyoteWindowSec` del Hito 40 G y `showEntityLabels` de F2H35.

- **State transient (timers, queue) NO se persiste** en SaveLoad. Razón: no tiene sentido restaurar un hit marker a medio fade tras load. Si emerge necesidad (ej. continuidad cinematográfica de un game over con damage vignette frozen), agregar después.

- **Fix lateral SceneLoader: auto-RigidBody al Floor cargado sin uno.** Descubierto durante validación F2H39 (dev no podía pararse para ver los widgets dinámicos — caía infinito). Causa: proyectos guardados pre-Hito 12 (cuando se introdujo `RigidBodyComponent`) no tenían el body serializado en el Floor, y `EntitySerializer` solo lo agrega si está en el JSON. Fix: en `SceneLoader::applyOneEntity`, si la entidad cargada tiene tag `Floor` o `Tile_X_Y` y NO tiene RigidBody, auto-add un Static Box con `halfExtents = scale * 0.5`. Solo aplica a auto-generadas para no contaminar entities user-creadas que omitan colisión a propósito. Ataca el caso de "loaded project sin RigidBody"; el caso "new project con scale modificado" sigue abierto (ver pendientes).

- **Bug físicas conocido fuera de scope**: cuando el dev cambia `Transform.scale` del Floor en Inspector o vía gizmo, el `RigidBody.halfExtents` no se sincroniza — el visual cambia pero la colisión no. Player puede caer fuera del body. **NO atacado en F2H39** porque está fuera del dominio HUD; documentado como pendiente para hito propio futuro. Fix razonable: en `Inspector_Transform.cpp` o `EditorScene::updateRigidBodies`, detectar el delta `Transform.scale` desde la última creación del body y re-crear o `setBodyShape` con halfExtents proporcional.

**Alternativas descartadas explícitamente:**

- **Hito de optimización runtime sobre la notebook** (Iris Xe): descartado. PERFORMANCE.md baseline está en GTX 1660 — números no son comparables. Posponer al desktop es sensato.
- **HUD shader-based (post-process custom para vignette/scanlines)** en lugar de ImGui DrawList: scope mayor — requiere render pipeline custom para el HUD. F2H39 v1 mantiene DrawList para velocidad de iteración + zero-asset. Si emerge necesidad de fidelidad mayor (CRT scanlines genuinos con chromatic aberration), hito propio futuro.
- **HUD diegetic 3D ahora**: descartado. Requiere FPS arms (mesh + animator del brazo del player) primero — eso es hito propio mayor de gameplay/3D, no HUD overlay.
- **CompassBar / ObjectiveText / KillFeed / etc en F2H39**: descartado por scope. F2H39 entrega arquitectura + paquete inicial; los demás se agregan extendiendo el registry de widgets.
- **Themes alternativos** (Doom saturado / Fallout verde): descartado por scope. Paleta hardcoded HL en v1.

**Condiciones de revisión:**

- Si el dev pide más widgets de la lista diferida (CompassBar, KillFeed, etc.), agregar en hitos chicos siguiendo el patrón de los 8 widgets de F2H39.
- Si emerge necesidad de HUD diegetic 3D (Pip-Boy / muñequera Metro), abrir hito propio: requiere FPS arms primero.
- Si el dev cambia de paleta (ej. Doom Eternal saturado para un nivel específico), agregar `HudTheme` runtime + binding Lua `hud.set_theme(name)`.
- Si los widgets crecen >15 y el array hardcoded se vuelve molesto, refactor a `std::vector<HudWidget>` con `registerWidget()` API. Mientras esté <15, hardcoded es claro.
- Si emerge presión del bug físicas Floor scale-RigidBody desync, abrir hito propio en domain "engine/physics" — el fix natural es en `EditorScene::updateRigidBodies` detectando cambio de Transform.scale.

---

## 2026-05-09: F2H38 cierre — Default font ImGui a Lato

**Contexto:** F2H37 cerró el polish UX + iconos FA con un fix lateral para el em-dash tofu del Welcome modal (síntoma de que ProggyClean — el default font de ImGui — no cubre General Punctuation). El asset `LatoLatin-Regular.ttf` ya estaba en `assets/ui/fonts/` desde antes pero nunca se había cargado. F2H38 promueve Lato a default y consolida los benefits: legibilidad mejorada (especialmente en Console text-heavy), coverage Unicode más amplio, side effect de resolver problemas de tofu para cualquier punctuation natural del español. Mini-hito chico (~30 min, ~25 LOC neto en un solo archivo).

**Decisiones técnicas clave:**

- **Lato Latin Regular a 15px** como tamaño base. Razón: ProggyClean es bitmap monoespaciada de 13px diseñada para terminales (legible en pixel-art, pero pixely y sin kerning para UIs modernas). Lato es sans-serif TTF diseñada para pantalla con kerning correcto. 15px matchea VSCode / JetBrains / convención IDE moderna. Bumpeable a 14 / 16 si validación visual lo pidiera, pero el dev confirmó *"todo se ve perfecto"* al primer arranque.

- **Custom GlyphRanges custom para Lato**: `{0x0020, 0x00FF, 0x2010, 0x2027, 0}`. Cubre Basic Latin + Latin-1 Supplement + subset de General Punctuation (em-dash U+2014, en-dash U+2013, hyphen variants U+2010-U+2015, ellipsis U+2026, comillas curvas U+2018-U+201D). Razón: el dev escribe en español; los caracteres naturales del idioma (acentos, ñ, comillas curvas) deben renderear sin esfuerzo. Pre-F2H38 el em-dash era tofu — F2H38 lo resuelve para todos los strings, no solo el del Welcome modal. Alternativa descartada: `GetGlyphRangesDefault()` (Basic Latin + Latin-1 solo) — dejaría el tofu del em-dash sin resolver.

- **FA merge a 13px explicit**, no `0.0f` implicit. Razón: ImGui 1.92 tira `IM_ASSERT((font->Flags & ImFontFlags_ImplicitRefSize) == 0)` si la primary y la merge no comparten convención de reference size. F2H36 resolvió pasando `0.0f` al merge porque `AddFontDefault` es implicit; F2H38 con Lato a 15.0f explicit obliga a la FA también explicit. 13px = ~85% del texto Lato = icons proporcionales al cuerpo del texto (regla común en design systems: icons al 80-90% del text size para que se vean "del mismo peso visual" sin dominar).

- **`PixelSnapH = false` en Lato**, `true` en FA. Razón: Lato es sans-serif diseñada para anti-aliasing smooth — snap a pixel rompe el rendering a tamaños no enteros y deshabilita kerning sub-pixel. FA monocromática se beneficia del snap (icons crisp en sus tamaños base). Diferencia es visible en hover state y al zoom.

- **NO se revierte el fix em-dash de F2H37** (`—`→`-` en `EditorUI.cpp`). Razón: con Lato cargado, el em-dash ahora rendea correctamente, pero el fix `-` queda en su lugar como approach más robusto: el hyphen-minus U+002D es universal Latin (cualquier font lo cubre), mientras que depender de Lato para U+2014 reintroduciría tofu si alguien cambia la font default en el futuro. Costo del fix mínimo (3 ocurrencias, sin pérdida de información semántica).

- **NO se carga la font del MoodPlayer**. Razón: el Player usa su propio `PlayerApplication_Init.cpp` con su `AddFontDefault()` separado. Cargar Lato allí también requiere replicar la lógica — scope adicional sin valor inmediato (el Player no es text-heavy en runtime, sus paneles son Main Menu + HUD scripts). Si emerge presión de coherencia visual Editor↔Player, fix chico posterior siguiendo el mismo patrón.

- **NO se carga Lato Bold / Italic / SemiBold**. Razón: ImGui no usa font weights nativamente (`ImGui::Text` no tiene un `bold` parameter); el bold se simula con color alfa o `PushFont`. Cargar variantes adicionales agrega ~150 KB cada una al binary sin ganancia clara. Si emerge necesidad real (ej. Inspector quiere bold para headers), agregar variante específica en hito propio.

**Alternativas descartadas explícitamente:**

- **Inter / Roboto / Source Sans Pro / SF Pro / otra sans-serif**: descartado. Lato ya estaba en assets desde antes (heredado, escogido en algún momento previo); cambiar de pack ahora introduce 150-200 KB nuevos sin valor agregado. Lato es decisión "buena suficiente" — todas son sans-serif para UI bien rasterizadas.
- **Cargar Lato con backend FreeType** (mejor rasterizado que stb_truetype default): scope mayor — requiere CPM/build de FreeType, link, y el atlas builder de ImGui debe enchufarse a backend custom. Diferido si emerge presión de calidad de rendering en sizes pequeños.
- **Tamaño 14px o 16px**: descartado tras validación. 15px funciona bien en el monitor del dev (notebook Iris Xe + desktop GTX 1660). Si en pantallas 4K el texto se ve chico, agregar global `io.FontGlobalScale` toggle por DPI — diferido.
- **Font fallback chain (Lato → Noto Color Emoji → CJK)**: scope mayor, no necesario. El editor es interface en español con icons FA — no requiere CJK / emoji ahora.

**Condiciones de revisión:**

- Si el dev pide variantes (Bold para headers de Inspector, Italic para tooltips), agregar hito propio. El patrón es trivial pero requiere decidir mapeo (qué widgets usan qué variante).
- Si emerge necesidad de DPI scaling (4K monitors), agregar `io.FontGlobalScale` toggle desde el menu Ver / settings.
- Si el dev pide cambiar a Inter / otra font, regenerar `IconsFontAwesome6.h` no necesario (FA es independiente). Solo cambiar el TTF + el `AddFontFromFileTTF` path.
- Si MoodPlayer comparte init de ImGui en el futuro (hipotético refactor de "PlayerEditorBase"), Lato se cargaría una sola vez. Mientras tanto, fix chico aparte si emerge.

---

## 2026-05-09: F2H37 cierre — FontAwesome icons en el resto del editor + polish UX general (hito unificado)

**Contexto:** Tras cerrar F2H36 (icons en los 2 toolbars del workspace "Editor de mapas"), el dev pidió expandir al resto del editor: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"*. Al revisar PENDIENTES.md emergió que había además un "Pase de polish UX general continuo" anotado desde F2H21+F2H22 (Inspector drop targets, Hierarchy multi-select feedback, Console level filter, StatusBar layout/colores) — los mismos paneles tocados. Decisión: **fusionar ambos pendientes en F2H37** para evitar doble pasada sobre los mismos archivos. Hito multi-bloque (~5h, ~400 LOC distribuidos en ~15 archivos).

**Decisiones técnicas clave:**

- **Hito unificado deliberadamente**, no dos hitos separados. Razón: ambos pendientes tocan los mismos paneles (Inspector, Hierarchy, AssetBrowser, Console, StatusBar). Hacerlos por separado implicaría abrir InspectorPanel_*.cpp dos veces (una para icons, otra para drop targets), recompilar dos veces, validar visualmente dos veces. Atacar todo junto en un hito reduce churn y evita conflicts de merge entre los dos pasadas. Validado por dev al pedir el scope unificado.

- **Header `IconHelpers.h` separado de `IconsFontAwesome6.h`.** Razón: `IconsFontAwesome6.h` debe quedar como tabla pura de macros UTF-8, sin dependencias de scene/components — eso permite incluirlo desde código que no toca entities (ej. `MenuBar.cpp` solo usa los macros). `IconHelpers.h` agrega los helpers que dispatchean sobre presencia de componentes (`iconForEntity(Entity)`), que sí dependen de `Components.h`/`Entity.h`. Inline en el header — no requiere .cpp porque es un switch puro sobre presencia de componentes.

- **`iconForEntity(Entity)` consolida `entityIconStr` duplicado** que vivía en HierarchyPanel + VisGroupsPanel pre-F2H37. Mismo orden de prioridad (MeshRenderer > Brush > Light > Audio > Script > Trigger > Camera > Particle > sin componente). Razón: el helper local en cada panel se justificaba pre-F2H37 (cada uno mapeaba a `[X]` ASCII), pero al migrar a FA quedaba claro que el mapping es idéntico — si alguien renombra/agrega un component type, hay que actualizarlo en N panels. Helper compartido = un solo punto de cambio. Costo: agregar un nuevo entity type ahora requiere extender el header en lugar de cada panel — bajo, gana coherencia.

- **Polish multi-select del Hierarchy con 3 colores distintos** (naranja=active, amarillo=secundaria, gris=hidden) en lugar de seguir con el ImGui default selected highlight. Razón: pre-F2H37, el dev tenía que abrir el Inspector para saber cuál de las N entities seleccionadas era la primary (la que el Inspector edita por default en single-component flow). Distinguirlas en la lista con color = feedback visual sin click adicional. Color elegido: naranja-amarillo dentro del mismo "warm spectrum" para que no parezcan tipos completamente distintos (ambos son "selected", solo cambia el rol). Pasado test contra el outline naranja de overlay (consistencia cross-panel).

- **Console level filter con 6 SmallButton toggles**, no un dropdown / combo. Razón: un dropdown obliga a 3 clicks (open + click + close) cada vez que cambia el filtro; los 6 toggles independientes permiten ON/OFF de cada nivel con un click puntual sin abrir/cerrar UI. Cada toggle muestra el icon FA del nivel + color del nivel (activo) o tinte tenue (off) — el estado se ve sin tooltip. Trade-off: usa más espacio horizontal en la toolbar que un combo (~6×24=144 px vs ~80 px del combo). Aceptable: la toolbar de Console ya tenía espacio (Limpiar + Auto-scroll + filter input + (?)). Estado en `m_levelEnabled[6]` del header del panel — NO en `.moodproj`. Razón: es un toggle ergonomico de sesión, no una preferencia persistente del proyecto. Default = todos true (mostrar todo).

- **Tabs del AssetBrowserPanel reciben icon, rows NO.** Razón: cada tab es type-pure (todos meshes en Meshes, todos audio en Audio). Agregar el mismo icon en cada row dentro del tab sería ruido visual sin valor agregado. El audit inicial sugería ambos; al implementar emergió la redundancia. Rows mantienen el "displayName + metadata" original limpio.

- **Tab "Texturas" sigue mostrando grid de thumbnails** (no list con icon). Razón: el thumbnail es el feedback visual canonico para texturas — agregar icon `IMAGE` al lado redundaría con el preview. Solo el tab itself recibe icon.

- **InspectorPanel_Brush.cpp `TextDisabled("Brush (CSG)")` → `SeparatorText(ICON " Brush (CSG)")`** durante el pase. Razón: los demás partials del Inspector usan `SeparatorText` (F2H23 convention); el `TextDisabled` original era inconsistente y se notaba en el tour visual. Upgrade gratis al estar tocando el header.

- **InspectorPanel_Particles.cpp mismo upgrade**: `TextDisabled("Particle Emitter")` → `SeparatorText(ICON " Particle Emitter")`. Mismo patrón.

- **InspectorPanel_Internal.h centraliza el include de `IconsFontAwesome6.h`**, no cada partial. Razón: 11 partials hubieran requerido 11 ediciones de include, todas idénticas. Internal.h ya es include compartido por convención (F2H24); agregar un include más mantiene el patrón y reduce churn.

- **Fix lateral em-dash tofu en Welcome modal** (descubierto en Bloque I): el carácter `—` (U+2014, General Punctuation block) no está en ProggyClean (default font de ImGui, cubre ASCII + Latin-1) ni en mi `k_iconRange = {0xE005, 0xF8FF}` (Private Use Area de FA). Pre-F2H36 ya se veía como `?` pero nunca se notó. Fix mínimo: reemplazar `—` por `-` (hyphen-minus U+002D, Basic Latin) en `EditorUI.cpp` (3 ocurrencias). Alternativa descartada: cambiar la default font a Lato (que ya está en `assets/ui/fonts/`) — scope mayor, requiere revisar todo el editor para verificar spacing/alineamiento. Hito propio si emerge necesidad real.

- **Width del botón Play/Stop bumped 64→80 px** para acomodar `ICON_FA_PLAY " Play"` y `ICON_FA_STOP " Stop"`. Sin esto el "Stop" se cortaba.

- **StatusBar mode indicator: `ICON_FA_PLAY` para Play / `ICON_FA_PEN_TO_SQUARE` para Editor.** Razón: `PEN_TO_SQUARE` matchea el icon del menú "Editar" del MenuBar (consistencia cross-panel). Para devs daltonicos, el shape del icon es un refuerzo accesible al color rojo/azul claro existente.

**Alternativas descartadas explícitamente:**

- **Hito separado para FontAwesome (F2H37) y otro para polish UX (F2H38)**: descartado por overlap de paneles. Doble churn sin valor agregado.
- **Cambiar default font a Lato durante F2H37**: descartado, scope mayor. El em-dash tofu es una excepción rara, no un problema sistémico (el resto del UI usa ASCII + Latin-1 que ProggyClean cubre).
- **Header IconHelpers.h con dispatchers para todos los tipos** (entity + asset + log level + workspace): descartado. `iconForEntity` se consolida porque pre-existía duplicación; `iconForAsset` no se usa fuera de AssetBrowserPanel y no hay duplicación que consolidar. `iconForWorkspace` queda local en MenuBar.cpp por la misma razón. YAGNI.
- **Dropdown de level filter en Console**: descartado por UX (3 clicks vs 1 click por toggle).
- **Persistir level filter en `.moodproj`**: descartado. Es ergonomia de sesión, no preferencia del proyecto. Si emerge necesidad real, agregar `Project::consoleLevelEnabled[6]` siguiendo el patrón de `showEntityLabels` (F2H35).

**Condiciones de revisión:**

- Si el dev pide cambiar el default font a Lato (más legible para mucho texto, ej. Console con muchos logs), abrir hito propio. El em-dash tofu se resolvería como side effect.
- Si emerge un component type nuevo (ej. `NavMeshComponent`), extender `iconForEntity` en `IconHelpers.h` con el nuevo case + agregar el macro al header subset.
- Si emerge necesidad de iconos color (multi-tone SVG-style) — FA6 free solid es monocromo. Cambio de pack es scope mayor.
- Si los toggles del level filter se vuelven cuello de botella visual (ej. el dev quiere un "solo errors" macro), agregar shortcut keys (1-6 con modifier) o un macro button "Solo errores" / "Resetear filtros". Diferido.

---

## 2026-05-09: F2H36 cierre — FontAwesome icons en toolbars del editor de mapas

**Contexto:** F2H22 cerró un pase de polish UX que dejó como deuda explícita "iconos image-based del Toolbar" (las labels eran texto castellano corto: `Mover`/`Rotar`/`Escala`/...). El dev expresó interés post-F2H35 con feedback como *"no tenemos iconos para usar para gizmo, en blender es como esto..."*. Mini-hito chico (~30 min, ~50 LOC) que cierra esa deuda agregando FontAwesome 6 free solid al atlas de ImGui y aplicándola a los 17 botones de los 2 toolbars del workspace "Editor de mapas".

**Decisiones técnicas clave:**

- **FontAwesome 6 free solid (`fa-solid-900.ttf`) commiteada al repo en `assets/ui/fonts/`.** Asset estático del repo oficial `FortAwesome/Font-Awesome` rama `6.x`, ~417 KB. Alternativas descartadas: (1) CPM/FetchContent de la zip release de FontAwesome — no hay distribución oficial CMake-friendly, agregaría build step frágil; (2) hand-rolled SVG icons via ImGui DrawList — escala lineal con cantidad de icons, alto costo por icono; (3) Lucide / Material Symbols / IcoMoon — más nuevos pero menos cobertura del legacy FA4/5 codepoints que muchas guías documentan. FA6 es el estándar de facto, estable y autocontenido.

- **Header `IconsFontAwesome6.h` con subset de ~15 macros**, no el header full de ~2000. Macros encoded en UTF-8 con escapes hex (`"\xef\x86\xb2"` etc.) para no depender de la code page del source MSVC (warning C4566). Incrementar el subset al agregar features nuevas con icon es un cambio explícito + visible en code review, no un import implícito que crece sin control. Trade-off: agregar un nuevo icono requiere computar el escape hex UTF-8 a mano (3 bytes para codepoints 0x0800-0xFFFF). Bajo costo, alta legibilidad.

- **Merge con default font (ProggyClean) usando `0.0f` como `SizePixels` en el merge**, no `13.0f`. Razón: ImGui 1.92 introdujo asserts que verifican que el merge use el mismo "reference size" que la dst font (que es implicit cuando se usa `AddFontDefault()`). Pasar 13.0f explicit triggera `IM_ASSERT((font->Flags & ImFontFlags_ImplicitRefSize) == 0)`. Patrón documentado en `imgui-src/docs/FONTS.md` ("Merge font and icons" example).

- **`GlyphMinAdvanceX` dropeado** (que originalmente seteé a 13.0f para mono-spacing del icon column). Razón: ImGui 1.92 tira un segundo assert si combinas glyph advance overrides + `SizePixels = 0.0f` ("Specifying glyph offset/advances requires a reference size to base it on"). Default rendering es suficiente — los iconos quedan al alto natural de la fuente, alineados con el texto del label. La consistencia visual no se nota en uso real con icons mezclados con label castellano.

- **Width del Toolbar bumped 72→92 px** para acomodar `ICON " Cilindro"` sin truncar. `ICON_FA_CIRCLE` + espacio + 8 chars de label en font 13px = ~85 px. Margen de 7 px para padding interno. Valores menores cortaban el label y dejaban el botón con texto mochado.

- **Scope explícitamente acotado a Toolbar + MapEditorTopBar** (los 2 toolbars laterales del workspace "Editor de mapas"). Quedan FUERA y se difieren a F2H37 dedicado: MenuBar, Hierarchy con icon-por-tipo de entity, Inspector con icons en headers de componentes, AssetBrowser con icons por tipo, Console con icons por nivel, StatusBar, paneles VisGroups/Material/Script. Decisión consciente alineada con la convención previa "un hito = un dominio acotado". El dev validó la decisión al pedir el resto: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"* — confirmación de que el approach incremental es correcto, no que el F2H36 quedó incompleto.

- **NO se carga la `LatoLatin-Regular.ttf` que ya existe en `assets/ui/fonts/`.** El dev nunca la activó (ImGui usa default ProggyClean). Cambiar la default font global = scope mayor (revisar todo el editor para verificar que el spacing / alineamiento siguen OK con la nueva métrica) y NO es lo que pidió F2H22 (que pedía iconos, no cambiar la font del editor). Si se decide en el futuro promover Lato como default, hito propio.

**Alternativas descartadas explícitamente:**

- **Embedded TTF en el binary** (via `xxd -i` o `bin2c` para no depender de runtime file): el TTF es 417 KB, se commite una vez al repo, el `mood_runtime_files` target ya copia `assets/` automáticamente. Embedding agregaría build step + complica cargar variantes en el futuro.
- **Mantener el header completo de ~2000 macros** (descargado del repo `juliettef/IconFontCppHeaders`): contamina autocompletado de VS, hace ruido en grep, agrega ~150 KB de overhead de preprocessing por cada TU que lo incluya. El subset acotado se extiende explícitamente cuando hace falta.
- **Push del icon como override del label en `toolButton`** (helper que recibe icon + label separados y los concat internamente): el helper es shared entre Toolbar y MapEditorTopBar pero el call-site sigue siendo el que sabe qué icon usar, así que pasar `ICON_FA_CUBE " Box"` directo es más explícito y permite que el formateo varíe (icon-only / icon-then-label / label-only) sin tocar el helper.

**Condiciones de revisión:**

- Si en F2H37 (extensión al resto del editor) el subset crece a ~50+ icons, considerar adoptar el header `IconsFontAwesome6.h` upstream del repo `juliettef/IconFontCppHeaders` con `#define IGFD_USE_QUICK_PATHS_AND_TYPES` o similar para acotar lo que entra al preprocessor. Mientras estemos < 30 macros, el header propio es más simple.
- Si ImGui actualiza a 1.93+ y cambia el patrón de reference size, revisar este merge. El comentario en `EditorApplication_Init.cpp` apunta al CHANGELOG y FONTS.md como referencia.
- Si el dev pide cambiar a otro icon pack (Lucide, Material Symbols), el cambio principal es regenerar `IconsFontAwesome6.h` (renombrar a `IconsLucide.h` o similar) + reemplazar el TTF + actualizar los call-sites con los macros nuevos. ~30 min de trabajo.
- Si emerge necesidad de iconos color (multi-color SVG-style), FA6 free solid es monocromo — habría que mover a Twemoji / Noto Color Emoji o equivalente, scope mayor.

---

## 2026-05-09: F2H35 cierre — Polish editor: UX viewport + Hammer-style visual

**Contexto:** F2H35 agrupa varios items chicos identificados al validar F2H34 (editor maximizado, toggle wireframe en perspective descartado por dev) + paquete "Hammer-style visual polish" anotado post-F2H33 (tint VisGroup color, color por tipo de entity, labels point entities, pulir face picking). Mini-hito multi-bloque (~1 sesión, ~700 LOC) que cierra deuda de UX visual del editor sin tocar la math/lógica del CSG ni la del scene graph.

**Decisiones técnicas clave:**

- **Editor arranca al tamaño REAL del display, no 1280x720 + maximize async.** `SDL_CreateWindow(SDL_WINDOW_MAXIMIZED)` encola el resize asíncrono — `SDL_GetWindowSize` en el primer frame devuelve los valores que pasamos al CreateWindow (1280x720 stale), no las dimensiones reales maximizadas. El primer rebuild del Dockspace usaba ese WorkSize, los splits con ratio se calculaban a esa resolución y luego ImGui los persistía como offsets ABSOLUTOS al ini. Cuando la ventana ya era 1920x1057 (maximizada real), los offsets stale dejaban panels descuadrados. **Fix:** `SDL_GetDesktopDisplayMode(0, &dm)` antes de CreateWindow para crear directo al tamaño del display + flag `WindowSpec::maximized` para que el WM marque "restaurar" y respete la taskbar. Garantiza dimensiones correctas desde el frame 0.

- **Stamp `k_IniLayoutStamp = 1` per-proyecto en `.moodproj`.** El bumpear `imgui_layout_v2.ini → v3.ini` cubre el ini global de ImGui, pero los `.moodproj` también guardan `iniLayout` por workspace (F2H7). Stamp invalida los iniLayouts persistidos cuando el dockspace builder cambia significativamente. Ausente = legacy 0 (pre-F2H35) → `ProjectSerializer::load` descarta los iniLayouts y deja `iniLayout=""`, forzando rebuild fresh con WorkSize correcto al primer activado del workspace. Sin esto los proyectos viejos seguían descuadrados aunque la ventana arrancara correcta.

- **Tint VisGroup color SOLO en wireframe orto, no en perspective.** Perspective renderea PBR completo (no wireframe excepto outlines de selected); tintar el wireframe del VisGroup tendría sentido solo en orto donde TODO es wireframe. Mantiene consistencia: tipo va al perspective via icon (Bloque D), organización (VisGroup) va al orto via wireframe color (Bloque C). Selection sigue ganando: brush selected = naranja Hammer, override sobre el VisGroup color.

- **Cubitos point entity en orto: tamaño FIJO (r=0.4), no proporcional al snap.** Iteración inicial usaba `snapStep * 0.5` — con snap=64 los cubos eran de 32 unidades de lado, inflaban la vista; con snap=1 desaparecían. Después probé `snapStep * 0.15` clampeado pero seguía siendo demasiado al snap=64. Decisión final: tamaño fijo `r=0.4` que matchea `k_iconPickRadius=0.6` de ScenePick (el área visible coincide con el área pickable, sin sorprender al dev "lo veo pero no lo agarro"). Hammer Source clásico también usa cubitos de tamaño fijo (~8 unidades hammer).

- **Detalles internos elaborados en cubos orto (rayos/X/diagonal/frustum/burst) DESCARTADOS.** Iteración inicial agregó shapes distintivos por tipo dentro del cubo. Feedback dev: *"demasiado grande, como lo hace el hammer editor de valve?"*. Hammer Source clásico = cubito chico + label de texto. La diferenciación fina viene del label (Bloque E), no de la forma. Esto deja el código simpler (1 sola línea `drawAabb` por cubo) y delega la diferenciación al texto que es más legible.

- **Hover preview de face picking en CYAN brillante** `(0.10, 0.95, 1.00)`, NO blanco tenue. Iteración inicial usaba `(0.85, 0.85, 0.95)` — testeado contra textura blanca tilada del proyecto del dev: el blanco desaparecía sobre fondos claros. Cyan saturado contrasta con amarillo (active selected) y naranja (secondary selected) — el dev distingue hover-preview vs ya-seleccionado a primera vista, sobre cualquier textura.

- **Gizmo Rotate constante en pantalla = Translate/Scale.** Pre-F2H35 (F2H30 Bloque D) era `gizmoRingRadius = 0.6 * max(localAabb)` clamped a 0.5 — radio en world-space que se hacía chico al alejar la cam. Translate/Scale ya usaban `k_armLen = 60.0f` píxeles (constante en pantalla). Inconsistencia: alejabas cam, los handles seguían pero el ring se hacía minúsculo. **Fix:** derivar `worldRadius = TARGET_PX / pixelsPerWorld` con `pixelsPerWorld = (h/2) / (camDistance * tan(fovY/2))` — análogo a la proyección perspectiva. Target 70 px (ligeramente mayor que `k_armLen` para que el ring rodee los handles sin solaparse).

- **Toggle "Nombres" default ON, persistido por proyecto.** Hammer original tiene este toggle OFF por default (menú "Map > Show Helpers"). Decisión del dev: *"labels default On"*. Persistencia opcional (`Project::showEntityLabels = true` + `ProjectSerializer::save` solo emite si != default) para no ensuciar `.moodproj` viejos con campos nuevos. Mismo patrón que coyoteWindowSec del Hito 40 G.

- **Pickable extendido a Trigger/Camera/Particle en ScenePick.** Bug pre-existente del F2H17 detectado al implementar Bloque D: `pickEntityFromRay` solo soportaba Light y Audio en la rama sphere-pick — Trigger/Camera/Particle caían al `return` sin pickable. El dev tenía que ir al Hierarchy del workspace Layout para seleccionarlos. Fix extiende la condición a los 5 tipos (mismo `k_iconPickRadius=0.6`). Encajó dentro de F2H35 porque salió a la luz validando el Bloque D.

**Alternativas descartadas explícitamente:**

- **Toggle wireframe/render shading en perspective viewport** (parte original del plan F2H35 Bloque B): descartado por el dev — *"olvdalo, ya tenemos wireframe en el editor de mapas"* (los 3 ortos del workspace "Editor de mapas" ya son wireframe via F2H28). Implementarlo requería tocar el render pipeline + glPolygonMode global + UI overlay con botones — alto costo, bajo valor agregado.
- **Detección automática de WorkSize cambio + rebuild dockspace defensivo**: alternativa al stamp `k_IniLayoutStamp` para invalidar iniLayouts stale. Descartado porque rompería la persistencia del layout custom del dev (cualquier resize manual disparaba reset). Stamp es más predecible.
- **Iconos elaborados por tipo dentro del cubito orto** (rayos sun, X audio, frustum camera, etc.): descartado por feedback dev. Hammer Source clásico = cubito + label. Hammer-Source 2/Hammer++ usa SVG icons via FontAwesome — eso queda como hito futuro propio.

**Condiciones de revisión:**

- Si el render pipeline cambia significativamente y los splits del dockspace requieren bumpearse, incrementar `k_IniLayoutStamp` (mismo patrón del bump del ini global).
- Si emerge necesidad real de iconos image-based en el toolbar (FontAwesome merge), abrir hito propio para no contaminar este — el dev ya lo mencionó como pendiente desde F2H22.
- Si el dev pide hover preview en orto (no solo perspective), agregar — el helper `pickFace` ya está disponible, solo falta el wireup del cursor del orto al render pass.

---

## 2026-05-09: F2H34 cierre — Multi-face material drop (capitaliza refactor F2H33)

**Contexto:** F2H33 introdujo `selectedFaceIndices` (vector de N caras seleccionables via Shift+click), pero el handler de drop en `DemoSpawners_Drop.cpp` quedó pre-F2H33 — solo aplicaba la textura/material al `activeFaceIndex()`. F2H34 capitaliza ese refactor extendiendo el flow para que el drop afecte a las N caras seleccionadas en una sola operación undoable. Es un mini-hito (~1 sesión, ~250 LOC) que cierra una deuda explícita anotada en `PENDIENTES.md` post-F2H33.

**Decisiones técnicas clave:**

- **Extender el command existente, no agregar uno nuevo.** `EditBrushFaceMaterialCommand` cambió `u32 faceIndex` + `u32 oldFaceMatIndex` + `u32 newFaceMatIndex` por `vector<u32>` paralelos, con un constructor 1-cara que wrappea al multi rellenando vectores de tamaño 1. Alternativa descartada: crear `EditBrushFacesMaterialCommand` (plural) — duplicaba 90% del código y forzaba a las llamadoras a elegir entre dos clases con la misma intención. El wrapper preserva los 7 tests del F2H19 sin modificación.

- **Snapshot único de `bc.materials` compartido entre N caras.** Cuando el material es nuevo, un solo `push_back` y todas las caras del set apuntan al mismo slot. Sin esto, cada cara podría duplicar el slot en `apply` inflando `bc.materials` y desincronizando el snapshot post con el real → undo deja state inconsistente. Confirmado por test "aplica el mismo slot a 3 caras".

- **Validación atómica de faceIndices en `apply` (todo o nada).** Si un solo faceIndex está fuera de rango (> `faces.size()`), el command retorna sin mutar nada. Garantiza que un command corrupto nunca deje el brush en estado parcial mid-undo o mid-redo. Cubierto por test específico.

- **Helper `tryAssignMaterialToSelectedFaces` en namespace anónimo del .cpp**, no en EditorApplication header. La lógica solo existe en el flow de drop (texture + material drop comparten 100%). Exponerla en el header propagaría dependencias innecesarias. El helper devuelve `nullptr` cuando no aplica (object mode, brush distinto del active, set vacío) → llamadora cae al flow object-mode sin if extra.

- **Label dinámico singular/plural** según `selectedFaceIndices.size()`: "Asignar textura a cara" vs "Asignar textura a caras". Ediciones del Editar > Deshacer informan correctamente la magnitud de la operación.

- **Deferral consciente de "abrir maximizado" + "toggle wireframe/render" a F2H35.** Durante validación de F2H34 el dev pidió ambos features. Decisión: NO contaminar el commit de cierre F2H34 con scope creep — abrir F2H35 como mini-hito UX viewport propio. Patrón consistente con la convención previa (un hito = un dominio).

**Alternativas descartadas explícitamente:**

- **Push de N commands single-cara** (uno por cara seleccionada): undo/redo serían N pasos, mala UX. El user clickeó UNA vez (drop), el undo debe ser UNA pulsación.
- **Soportar materiales DISTINTOS por cara en una sola op**: poco probable (un drop = un material). Si emerge necesidad futura, extender el command (ya tiene la estructura `vector` lista para variar `newFaceMatIndices` libremente).

**Condiciones de revisión:**

- Si emerge necesidad de aplicar materiales DISTINTOS a caras distintas en una sola op (ej. paint mode con paleta), el command ya soporta `newFaceMatIndices` heterogéneo nativamente — solo falta UI.
- Si el cap de undo/redo del HistoryStack se vuelve apretado con N caras grandes (cada multi-face drop = 1 entry, no N), no hay riesgo previsible. El F2H32 ya tenía operaciones similares con snapshots grandes (BooleanOpCommand) sin problemas.

---

## 2026-05-09: F2H33 cierre — VisGroups + multi-select caras + texture alignment (Hammer cerrado funcional al 100%)

**Contexto:** F2H33 es el último hito antes de cerrar el editor estilo
Hammer en su totalidad funcional (31/44 hitos de Fase 2). Combina 3
features que Hammer 4 tiene y MoodEngine no tenía: VisGroups
(organización), multi-select de caras (selección productiva), y texture
alignment (Align/Fit/Justify del Face Edit Sheet). Tras F2H33, el dev
queda libre de elegir entre sub-fase 2.5 gameplay o seguir con polish UI.

**Decisiones técnicas clave:**

- **Schema bump `.moodmap` v13→v14 aditivo, mismo patrón que F2H26.**
  Array opcional top-level `visgroups: [{id, name, color, hidden}]` +
  campo opcional `visgroupId: u64` por SavedEntity y SavedBrush. El
  loader v14 acepta mapas v13 (array vacío + sin membership) sin
  migración irreversible — al guardar un mapa v13 abierto se persiste
  como v14 sin pérdida. `checkFormatVersion` rechaza v15+ (forward-incompat).

- **VisGroups planos (no jerárquicos), membership 1-a-N.** Hammer 4 es
  así. Una entity pertenece a 0 o 1 grupo (lookup O(1) al renderizar
  sin iterar membership múltiple). Sub-grupos diferidos como deuda
  futura si emerge necesidad real — refactor a `unordered_set<u64>` si
  hace falta. El componente `VisGroupMembershipComponent` opcional con
  `groupId == 0` reservado como sentinel "sin grupo" (preferimos
  ausencia de componente a presencia con 0).

- **Player ignora VisGroups (convención Hammer).** En Hammer 4, los
  VisGroups son herramienta del editor — el `.bsp` final incluye toda
  la geometría sin importar grupos ocultos. Decisión: `applyEntitiesToScene`
  con `useCompiledMesh=true` (path Player) skipea `scene.resetVisGroups()`
  y NO agrega `VisGroupMembershipComponent`. Agregamos parámetro
  `applyVisGroupMembership=true` (default true para callers como
  `DeleteEntityCommand::undo` que sí necesitan preservar membership). Sin
  esto, un mesh oculto en editor también desaparecía al probar en Player
  — bug confuso. Ahora la convención queda explícita: VisGroups son
  productividad del dev, no contenido del juego.

- **Hide gates en 3 lugares: render, picking, Hierarchy grayed.** El
  render (`groupByBatch`, brushPass, skinned pass) skipea entities en
  grupos hidden — no se encolan al frame. ScenePick las skipea — no se
  pueden seleccionar via click viewport. Hierarchy las muestra en gris
  claro `(0.55, 0.55, 0.55)` — el dev necesita verlas para sacarlas del
  grupo, pero queda claro que están "apagadas". Scripts y physics de
  entities ocultas SIGUEN activos (alineado con Hammer — VisGroup no es
  "disable", es "ocultar en viewport"). Si emerge necesidad real,
  agregar flag `disableScripts` separado al VisGroup.

- **Refactor `SelectionSet` breaking-internal: campo → método derivado.**
  Pre-F2H33: `i32 activeFaceIndex` (F2H17) era field público accedido
  como `set.activeFaceIndex`. Para multi-select, refactoreamos a
  `std::vector<i32> selectedFaceIndices` + método `i32 activeFaceIndex()
  const { return selectedFaceIndices.empty() ? -1 : back(); }`.
  Invariante mantenido por los helpers (`setSingleFace`, `toggleFace`,
  `add` que limpia caras al cambiar de active brush): el último del
  vector es siempre la "active" (= primary para single-face ops).
  Migrados ~7 call-sites de field access a método (`set.activeFaceIndex`
  → `set.activeFaceIndex()`); writes a `set.activeFaceIndex = -1`
  cambiados a `set.selectedFaceIndices.clear()`. Tests adaptados +
  6 tests nuevos para los helpers de multi-face. Tradeoff: un breaking
  cambio interno por un API más expresivo. Aceptable porque el dominio
  cambió (single → multi).

- **Active face en amarillo, secundarias en naranja Half-Life.**
  Pre-F2H33 el highlight de cara era naranja uniforme (F2H17). Con
  multi-select N caras todas naranjas no distinguen cuál es la primary
  (la que se mostraba en el header del Face Edit Sheet). Decisión:
  iterar `selectedFaceIndices` y dibujar la última (= active) con
  outline + fill amarillos `(1.00, 0.95, 0.10)`, las demás con el
  naranja Half-Life original. El dev distingue de un vistazo cuál cara
  manda en single-face ops (drop material, ediciones que solo aplican
  a la primary).

- **Face picking robust: pickEntity primero, pickFace después.** Issue
  pre-F2H33 (F2H17 original): `pickFace` solo testeaba contra el brush
  active. Si el dev clickeaba una cara de OTRO brush en Face Mode, el
  faceHit fallaba → caía al `pickEntity` → cambiaba el active → limpiaba
  las caras seleccionadas → segundo click necesario. Con multi-select
  esto rompía el flow de Shift+click cross-brush. Fix: `pickEntity`
  primero (encuentra cualquier brush hovered), `pickFace` contra ese
  brush. Si `sameBrush` + Shift → toggle; sameBrush + sin modifier →
  single; brush distinto → `replaceWithSingle` + `setSingleFace` (no
  toggle: cambiar de brush no acumula caras del brush viejo, sería
  confuso). Resuelve el feedback del dev *"la seleccion es como
  dificil... debo rotar y probar varias caras"*.

- **Reuso de `EditBrushUVCommand` para alignment ops.** El comando ya
  capturaba snapshot completo del brush (todas las caras + sus axisU/V/
  scale/offset/rotation/lockToWorld). Eso significa que aplicar 1 op
  a 1 cara o N caras y pushear el comando funciona sin extensión —
  el snapshot post incluye los cambios de todas las caras tocadas.
  Decisión: NO crear comando nuevo `EditFaceUVCommand`. Tradeoff: el
  command label dice "Editar UV scale" sin distinguir 1 vs N caras
  — aceptable. Los labels específicos del Inspector (`Editar UV scale
  (cara)` / `(N caras)`) los pone el caller via param `label`.

- **"Treat as one face" con axis heterogéneos: warn + aplica.** El
  algoritmo de bounding rect compartido asume que las N caras
  seleccionadas pueden proyectarse al mismo sistema (axisU, axisV) de
  la primary. Si las caras tienen normales muy distintas (ej. arriba +
  costado de un cubo), los axisU/V también difieren y la proyección no
  es geométricamente coherente. Heurística simple: chequear `dot(face.
  uAxis, primary.uAxis)` y vAxis; si < 0.99 (= ángulo > ~8°), log warn
  pero aplicar igual. Hammer 4 hace lo mismo — el dev sabe lo que está
  haciendo cuando activa el checkbox con caras de planos distintos. No
  bloquear la op; solo informar.

- **Refactor CMake colateral: race condition pre-existente.** Pre-F2H33,
  cada exe (MoodEditor + MoodPlayer) tenía sus propios `add_custom_command
  POST_BUILD` con `copy_directory` para shaders/ y assets/ apuntando a
  `$<TARGET_FILE_DIR:exe>/shaders` y `/assets`. Como ambos exes salen
  al mismo `build/Debug/`, `TARGET_FILE_DIR` evalúa al mismo path
  → cuando se compilaban en paralelo (`/m`), ambos POST_BUILD escribían
  a la misma carpeta destino → race condition de `copy_directory` con
  archivos parcialmente escritos. Aparecía intermitente como `MSB3073`
  en MoodPlayer.vcxproj. Fix: nuevo `add_custom_target(mood_runtime_files
  ALL)` con los 3 copies (shaders + assets + SDL2.dll) centralizados;
  `add_dependencies(mood_runtime_files MoodEditor MoodPlayer)` garantiza
  que ambos exes terminan antes del deploy. Compilación de los .cpp de
  ambos exes sigue paralela; solo el deploy es secuencial. Es un fix
  pre-existente que no le tocaba a F2H33 estrictamente, pero apareció
  durante el primer build con el código nuevo y lo cerramos acá.

- **Bug fix `Ctrl++` en teclados 80% sin numérico (layout español).**
  Pre-F2H33 el handler del snap step cycleable de F2H28 aceptaba
  `SDLK_EQUALS` (US/UK: `=` con shift produce `+`) y `SDLK_KP_PLUS`
  (numpad). En layout español la tecla a la derecha de Ñ manda
  `SDLK_PLUS` directo (sin shift). Fix: agregar `SDLK_PLUS` al handler.
  Reportado por el dev al validar Bloque B; aprovechamos el contexto
  para arreglarlo dentro de F2H33.

**Razones:**

- **Paquete unificado (VisGroups + multi-face + alignment) en 1 hito**
  en lugar de 3 separados: comparten dominio (face polish del editor de
  mapas), comparten infra (SelectionSet refactor habilita tanto multi-
  face del Bloque C como el iter del alignment del Bloque D), y cierran
  juntos el "Hammer cerrado funcional 100%" de forma cohesiva. Spliteando
  en 3 hitos hubiera diluido la narrativa.
- **Reuso máximo**: schema bump aditivo (mismo patrón que F2H26),
  comandos undoable (mismo patrón que F2H19 `EditScriptComponentCommand`),
  panel layout (mismo patrón que `HierarchyPanel` con `IPanel::name()`/
  `visible` flag + `applyDefaultVisibilityForWorkspace`), helpers de UV
  (reusan `Csg::collectFaceWorldPolygon` de F2H17), comando UV reusa
  `EditBrushUVCommand` que ya soporta multi-cara nativo.
- **5 commits feat/fix + 1 docs**: granularidad útil para git blame
  (cada bloque cierra una capa). Bloque B es grande pero coherente
  (todo VisGroups end-to-end); bloque C agrupa el refactor SelectionSet
  + handler face picking; bloque D agrupa math + UI; el fix lateral
  del snap es un commit propio porque no es F2H33 strictly.

**Alternativas descartadas:**

- **VisGroups jerárquicos (sub-grupos)**: Hammer 4 es plano. Sub-grupos
  agregaría tree drag UI + propagación de hide/show (¿propagar a hijos?
  ¿nesting de visibility?) + schema más complejo. No agrega valor
  80/20. Diferido.
- **Comando nuevo `EditFaceUVCommand` para alignment**: el snapshot
  completo de `EditBrushUVCommand` ya cubre multi-cara nativamente.
  Comando nuevo sería duplicación.
- **Drag entities desde Hierarchy a VisGroup**: scope mayor (DnD ImGui
  custom). Menú contextual "Asignar selección al grupo" cubre el flow
  básico. Diferido.
- **Multi-face material drop dentro de F2H33**: requiere extender
  `EditBrushFaceMaterialCommand` a multi-cara (vector de snapshots).
  Estaba en el plan pero lo difirimos para no inflar el hito —
  deuda explícita en `PENDIENTES.md`.

**Condiciones de revisión:**

- Si el dev pide sub-grupos VisGroup → refactor mayor (tree storage +
  UI + propagación). Ahí evaluamos si vale el costo.
- Si emerge un escenario donde "Treat as one face" con axis heterogéneos
  produce resultados inaceptables (no solo "raros") → refactor a "rotar
  axisU de las caras secundarias para alinear con la primary antes de
  computar el rect compartido". Por ahora el warn + apply alcanza.
- Si los VisGroups crecen > 100 grupos típicos por proyecto → reemplazar
  el scan lineal de `findVisGroup` por `unordered_map<u64, VisGroup>`.
  Improbable en uso típico de mapping.

---

## 2026-05-09: F2H32 cierre — geometry tools (clip 2-click + carve UI Hammer-style)

**Contexto:** F2H32 cierra 2 de las 3 herramientas core de geometría
de Hammer Editor (Hollow se deja como deferido). Tras F2H31 (selection
+ visual polish), el dev priorizó cerrar el Hammer en su totalidad
antes de pasar a sub-fase 2.5 gameplay.

**Decisiones técnicas clave:**

- **Clip plane derivada de 2 clicks + view-perpendicular del orto.**
  En orto, 3 clicks no pueden definir un plano 3D (todos coplanares
  en el view plane = degenerado). Convención Hammer: la línea entre
  los 2 clicks + extrusión sobre el `forwardAxis` del orto define el
  plano. Implementación: `normal = cross(forwardAxis_orto, lineDir_world)`;
  validación de `||cross|| > kPlaneEpsilon` (línea no paralela al
  forward); `d = -dot(normal, p1)`. La regla "elige el lado positivo
  o negativo" queda determinada por el orden de los clicks, lo cual
  es UX aceptable (si el lado equivocado se conserva, `Ctrl+Z` + `T`
  cycle + Enter de nuevo).

- **Clip = agregar plane a Brush::faces (no clipping geométrico).**
  Convención CSG del motor: las normales de las caras del brush
  apuntan AFUERA, el interior es la intersección de half-spaces
  negativos. Para conservar el lado "Front" (positivo del clipPlane):
  agregar `BrushFace { -clipPlane.normal, -clipPlane.distance }` —
  el interior queda del lado positivo del clipPlane original. Para
  "Back": agregar el plano tal cual. `isBrushValid` filtra resultados
  degenerados (plano fuera del brush, brush queda sin volumen). La
  AABB se recompute via `computeBrushAabb`. **No** se hace polygon
  clipping explícito (Sutherland-Hodgman) — el brush mantiene la
  representación implicit-by-planes.

- **`BooleanOpCommand` extendido con kind=Clip + bSnapshot vacío.**
  Pre-F2H32 el command asumía dos brushes (A + B). Para clip hay 1
  brush (A) y un plano. Decisión: no crear `ClipBrushesCommand`
  separado; en lugar de eso, agregar `Clip` al enum + skipear
  destroy/recreate cuando `bSnapshot.tag.empty()`. Mismo patrón
  reutilizable para carve (donde los carvers no se destruyen, así
  que tampoco hay B que recrear). Trade-off: leve acoplamiento
  semántico (un BooleanOpCommand de "Clip" no tiene B), pero
  evita duplicar el flow snapshot+execute+undo.

- **Carve = subtract iterativo + carvers preservados.** Algoritmo:
  `fragments = [A_world]`; por cada carver B con AABB intersect:
  `fragments = union de subtract(fragment, B)`. Si A queda
  completamente consumido (todos los fragmentos vacíos tras un
  carver), break temprano. Carvers se preservan en el scene
  (estilo Hammer — el dev decide después qué hacer con ellos).
  Push `BooleanOpCommand` con kind=Subtract y bSnapshot vacío
  (los carvers no necesitan recreación en undo).

- **Carve broadphase por AABB.** Sin esto, escenas con muchos
  brushes hacen N² test booleanos por click (cada subtract es ~300
  LOC de geometría). Mitigación: solo brushes cuyo AABB world
  intersecta el AABB del active entran al loop. Ya tenemos
  `intersects(aabb, aabb)` en core/math/AABB.h.

- **Sin keyboard shortcut para carve.** Operación destructiva +
  silenciosa (no hay sesión preview); obligar click explícito en el
  botón "Carve" del toolbar evita accidentes. La tecla `C` queda
  libre. Trade-off: ligeramente más fricción para quien la use mucho;
  aceptable para v1.

- **UX hints visibles via `setStatusMessage`.** Pre-iter el clip y
  el carve solo logueaban warns; el dev no los veía mientras testeaba
  (consola separada). Fix: `setStatusMessage` muestra el hint en el
  status bar inferior — visible siempre que falten pre-condiciones
  (sin selección, sin intersección, plano degenerado, etc.). Misma
  estrategia que el pincel poligonal (F2H30 Bloque C).

- **No hay schema bump.** El clip/carve es spawn dinámico de brushes;
  el undo via `BooleanOpCommand` snapshot. F2H33 traerá el bump
  v13→v14 para VisGroups.

**Razones:**

- **Paquete unificado** (clip + carve en 1 hito) en lugar de 2 hitos:
  comparten dominio (boolean ops sobre brushes con UI Hammer-style),
  comparten el patrón de snapshot+spawn+command, y se validan juntos
  con escenas similares. F2H32 cierra Hammer + carve sin sobrecarga.
- **Reuso máximo**: `Csg::subtract` (F2H12) + `BooleanOpCommand`
  (F2H12) + `brushAabbWorld` (expuesto en F2H31) + `setStatusMessage`
  (F2H30 Bloque C). Solo `clipBrushByPlane` (math chica ~50 LOC)
  + handlers + UI nuevo.
- **3 commits feat (A plan + B+C unificado + D cierre)**: simpler que
  4 porque B y C comparten el patrón de snapshot+command. El commit
  unificado documenta los 2 bloques.

**Alternativas descartadas:**

- **Polygon clipping explícito en clip tool** (Sutherland-Hodgman
  sobre las caras del brush): innecesario — la representación
  implicit-by-planes ya hace el clip "gratis" al agregar la nueva
  cara. Más simple + correcto.
- **`ClipBrushesCommand` separado**: redundante con `BooleanOpCommand`
  + skip de tag vacío. Decisión: extender el existente.
- **Hollow tool en F2H32**: scope incremental sobre carve (sintáctico
  azúcar de "carve A con un brush más chico interior"). Diferido — no
  es típico del flow básico.
- **Multi-brush clip simultáneo (N brushes selectos al confirmar)**:
  ya soportado en el código actual — itera `targets` y cada uno se
  splittea con el mismo plano. Validado mentalmente; no testeado
  exhaustivamente.
- **Clip tool en perspectiva 3D**: gizmo manipulable para mover el
  plano libre sería scope mucho mayor. Hammer no lo tiene.
- **Keyboard shortcut para carve** (ej. `Ctrl+Shift+C`): operación
  destructiva con efecto silencioso (sin sesión preview). El botón
  explícito es la decisión segura. Si emerge pedido del dev, agregar.

**Condiciones de revisión:** si el dev pide hollow tool, multi-brush
carve simultáneo (N A's), clip en perspectiva 3D, o un shortcut para
carve. Si el flow descubre bugs en escenas con muchos brushes
(broadphase O(N) podría ser slow → considerar spatial hash).

---

## 2026-05-08: F2H31 cierre — productivity selection + visual polish (marquee + group transform + snap-to-vertex + frustum + coords cursor)

**Contexto:** F2H31 cierra las brechas de productividad y feedback
visual del editor de mapas estilo Hammer. Tras F2H30 (que cerró el MVP
funcional), el dev evaluó qué falta vs. Hammer real: marquee select,
snap-to-vertex, clip tool, carve UI, VisGroups, texture alignment,
frustum + coords. Decisión: split en 3 hitos (F2H31/32/33) para no
inflar el scope.

**Decisiones técnicas clave:**

- **Tool selector mutually exclusive vs. modifier-based.** Antes el
  block tool fireba siempre con drag en empty space; agregar marquee
  competía por el mismo input. Opciones: (a) tool selector radio
  (Hammer-style); (b) modifier (Shift/Ctrl distingue). Decisión:
  selector radio en la sección "Herramienta" del toolbar lateral —
  alineado con Hammer Editor real, default = Select. Eliminada la
  ambigüedad: cada drag sabe qué hacer según el tool activo.
- **Marquee hit-test "any corner inside" del AABB world.** Para cada
  entidad, proyectar los 8 corners del AABB world al ndc del orto;
  si CUALQUIER corner cae dentro del rectángulo, hit. Más liberal
  que "todos los corners adentro" — alineado con Hammer (cualquier
  overlap selecciona). Trade-off: brushes muy alargados pueden
  seleccionarse con un marquee pequeño que toque solo una esquina.
  Aceptable — es lo que hace Hammer.
- **Group transform reusa infra existente.** El `OrthoDragSession::
  startPositions` desde F2H29 Bloque B ya iteraba `set.selected` al
  populate (no solo el clicked); el comportamiento de "mover N
  entidades juntas" emerge naturalmente al llenar `set.selected` con
  N via marquee. Cero código nuevo de movimiento, solo cambio de
  data. `MultiEditTransformCommand` cubre el push undoable.
- **Snap-to-vertex toggle global (no per-tool).** Un solo flag
  `m_snapToVertexEnabled` afecta pincel + block tool corners + rubber
  band. El dev no quiere "snap-to-vertex solo en pincel" o variantes
  per-tool — uniforme alinea expectativa: si está on, todo lo que
  snappearia al grid prueba primero contra vertices.
- **Snap-to-vertex broadphase por AABB world.** Sin esto, escenas con
  cientos de brushes hacen N² (cada brush enumerar V vertices ×
  cursor cada frame). Mitigación: solo brushes cuyo AABB world
  expandido por threshold contiene el cursor entran al inner loop.
  Si emerge slow en escenas con > 500 brushes, refactor a spatial
  hash. Threshold ndc 0.02 (~8 px screen) generoso.
- **Auto-close del pincel al clickear vertex 1.** Pedido implícito del
  dev tras observar el bug *"cuando cierro todos los puntos y aprieto
  enter, no lo crea"* — porque clickeaba vertex 1 de vuelta generando
  `vertex N == vertex 1` → polígono degenerado → rechazo. Fix: si
  `pointsWorld.size() >= 3` y el click cae dentro de 1mm de
  `pointsWorld[0]`, llamar `closePolygonDraw` directo (skipear el
  push del vertex duplicado). Coexiste con cierre vía Enter
  (Blender-style). Mental model alineado con editores 2D clásicos
  (Photoshop pen tool, Illustrator).
- **Frustum a "look-ahead" 4u en lugar del near-plane real.** El
  near-plane real (~0.1m) es invisible al render (rect colapsa). Un
  rect a distancia 4u en la dirección forward de la cam con dimensión
  proporcional al fovY y aspect del 3D viewport es claramente visible
  y orientable. El dev percibe "qué mira la 3D cam" desde los 3
  ortos. 4 líneas tenues `(0.6, 0.55, 0.2)` desde camPos a las
  esquinas indican el tronco del frustum.
- **Camera basis extraída de la transpose del 3x3 del view matrix.**
  EditorCamera no exposa right/up/fwd directamente. El view matrix
  tiene esas básicas en sus filas (porque view = R^T * T(-eye)). La
  transpose del 3x3 superior-izquierdo da camera-to-world rotation;
  las columnas resultantes son los ejes en world. Sin agregar API
  nueva al EditorCamera. Forward = -row(2) (convención RH OpenGL).
- **Coords cursor solo cuando hovered.** El `m_liveCursor.hovered` ya
  lo reportaba el panel desde F2H30 Bloque C (para el rubber band del
  pincel). Reutilizado: cuando hovered, formatear y mostrar
  `(x, y, z)` debajo del label de la vista. Sin overhead extra
  cuando el cursor está fuera del panel.

**Razones:**

- **Paquete unificado** (5 features en 1 hito) en lugar de 5 hitos:
  todos comparten dominio (productividad + feedback visual del orto)
  y se validan juntos. Marquee + group transform es 1 feature
  conceptual ("seleccionar y mover varios"); snap-to-vertex toca
  pincel + block tool en paralelo; frustum + coords son polish
  visual del orto.
- **3 commits feat (A plan + B+C+D unificado + E cierre)**: simpler
  que 4 commits porque B/C/D entrelazan (snap-to-vertex modifica
  pincel que tiene auto-close fix; frustum coexiste con todo en
  EditorRenderPass). El commit unificado documenta los 3 bloques.
- **Sin schema bump**: F2H31 es state in-memory + UI overlays. F2H33
  hará el bump v13→v14 para VisGroups.

**Alternativas descartadas:**

- **Marquee con modifier (Shift+drag)** en lugar de tool selector:
  rompe el mental model de Hammer donde tools son radio. Descartado.
- **Snap-to-vertex per-tool**: añade complejidad UI (3 toggles) sin
  uso real (el dev quiere un único concepto "estar snappeando al
  vertex").
- **Frustum con near + far + 8 corners + 12 edges**: ruidoso, no
  aporta info útil. Descartado en favor de "rect de mira".
- **Coords cursor en formato custom** (notación científica para
  valores grandes, etc.): premature; `%.1f` cubre el caso común.
  Si emerge necesidad, agregar.
- **Snap-to-vertex extendido a vertex/edge edit (bloque 2.4e)**:
  scope similar pero el caso de uso es distinto (mover un vertex y
  pegarlo a otro vertex). Diferido — no bloquea el flow actual.
- **Marquee Alt-modifier (remove)**: scope incremental, agregar si
  el dev lo pide.

**Condiciones de revisión:** si el dev pide marquee Alt-remove, snap-
to-vertex en vertex edit, frustum más detallado (near + far), o coords
en otra unidad. Si emerge slow en escenas con muchos brushes
(snap-to-vertex N²), refactor a spatial hash.

---

## 2026-05-08: F2H30 cierre — vertex/edge edit + pincel poligonal + W/E/R double-tap modal (3 iteraciones de atajos)

**Contexto:** F2H30 entregó 4 features unificadas como paquete polish UX
del editor de mapas estilo Hammer:
1. Vertex/edge edit con snap absoluto WORLD + rebasing al cierre.
2. Brush poligonal "pincel" + toolbar lateral "Map Tools".
3. Gizmo rotate proporcional al AABB del brush.
4. Atajos modales (esquema final tras 3 iteraciones de feedback).

**Decisiones técnicas clave:**

- **Snap WORLD-space (no LOCAL) en vertex/edge edit.** El grid del
  workspace orto vive en world coords; snappear el delta_local
  inevitablemente desfasaba el resultado para brushes con
  `tf.position != 0`. Implementación: `pivotWorldStart = worldMat *
  pivotLocalStart`; snap del `pivotWorldNew`; reconvertir delta a
  local via `inverse(R) * effDeltaWorld`. **Solo snappear los ejes que
  el dev MOVIO** (`|deltaWorld[i]| > 1e-4`): si snappeás todos,
  brushes con coords no-grid-aligned saltan al grid en el primer
  drag aunque ese eje no se haya tocado.

- **Rebasing del centroide al cierre del drag.** Al modificar planos
  via vertex edit, el centroide del AABB nuevo del brush se aleja
  del `tf.position` original — el gizmo aparece "lejos del brush".
  Fix: en el `dragEnd`, `newCentroidLocal = bc.brush.localAabb.center()`,
  trasladar todos los planos por `-newCentroid`, `tf.position += R *
  newCentroid`. Mismo patrón que F2H12 boolean ops resolvió con
  `snapshotResultWorld`. Ahora el gizmo siempre aparece en el centro
  visual del brush.

- **Dedupe de clicks consecutivos en pincel.** Dos clicks que
  snapeaban a la misma celda del grid generaban un polígono
  degenerado (vértices A → A) y `closePolygonDraw` cancelaba con un
  warn que el dev no veía → percibía "la figura desaparece". Fix:
  skipear el segundo click si está a < 1mm del último, con log
  explícito `[pincel] click duplicado (mismo grid cell) — ignorado`.
  Trade-off: esto solo cubre duplicados *consecutivos*; un polígono
  con vertice 1 = vertice N se sigue rechazando con warn de
  "polígono degenerado". Aceptable.

- **Gates anti-conflicto pincel ↔ vertex/edge ↔ block tool.** Cuando
  el dev activa el pincel:
  - `togglePolygonDrawMode()` setea `m_subMode = Object` +
    `activeFaceIndex = -1` (sin esto, vertex/edge markers seguían
    dibujados encima del pincel + el bloque 2.4e disparaba edits
    accidentales del brush selecto al primer click).
  - El bloque 2.4e (vertex/edge edit) chequea `!m_polyDraw.active`.
  - El bloque 2.4d (block tool LMB) ya tenía el guard.
  Razón: tres consumidores potenciales del LMB en empty space del
  orto. Sin priorización explícita, un click "puro" del pincel
  podía consumirlo el panel como drag (>4 px de jitter) y disparar
  el block tool o el vertex edit en paralelo.

- **Toolbar lateral "Map Tools" como columna derecha.** Pedido
  original del dev: *"hagamos uno superior"*; mutó a *"prefiero
  columna lado derecho"* tras ver que botones horizontales se
  aplastan en 36 px. Ancho 10% del workspace, botones verticales
  32 px alto, highlight del activo + tooltips con shortcut.

- **Gizmo rotate radio proporcional.** `radius = max(0.6 *
  max(localAabb.size()), 0.5)`. Cubre BrushComponent (via
  `bc.brush.localAabb`) y MeshRendererComponent (via
  `MeshAsset::aabbMin/Max`). **No multiplicado por `tf.scale`**:
  simpler, suficiente para el caso común. Si emerge bug con brushes
  rotados + escalados asimétricamente, refactor a OBB world-space.

- **Atajos: 3 iteraciones por feedback explícito del dev.** Plan
  original: G/R/S puros (Blender) + W/E/R Maya conviven. Iter 2: dev
  pidió *"ya no usaremos los shortcuts de maya sino los de blender
  como base"* + *"el de grab eliminalo"* → removidos W/E/R, R = modal
  Rotate, S = modal Scale. Iter 3: dev pidió hibrido double-tap → W
  = Translate gizmo; E single = Scale gizmo / E doble = modal Scale
  uniforme; R single = Rotate gizmo / R doble = modal Rotate libre.
  G y S removidos. Estado final: **sin shortcuts cruzados** — cada
  tecla tiene un único significado. Detección double-tap via state
  `GizmoKeyTapState { lastKey, lastPressTime }` con window 0.4s
  (default Windows double-click).

- **Cuadrado central de uniform-scale gizmo eliminado.** Pedido del
  dev *"asi solo se usa la S"* → mutó a *"asi nos desacemos de la
  S"* (uniform scale via E doble en lugar de S). Como nadie usa el
  cuadrado, removido. Los 3 arrows per-axis siguen.

**Razones:**

- **Paquete unificado** (4 features en 1 hito) en lugar de 4 hitos
  individuales: las 4 comparten dominio (manipulación geométrica
  desde orto + atajos del editor) y se validan juntas. Un hito
  separado por feature sería ceremonia sin valor.
- **Scope cerrado en 4 commits feat (B/C/D + cierre)**: alineado con
  el ritmo de F2H28 / F2H29.
- **Visual feedback del modal** (anillo amarillo + línea
  cursor→centro) por pedido del dev *"que aparezca ese circulo para
  rotar"*. Cosmetic pero importante para entender qué hace el modal.

**Alternativas descartadas:**

- **Snap LOCAL para vertex edit**: dejaba brushes con `tf.position
  != 0` desfasados — el dev lo notó al primer test.
- **Modal G permanece** (translate via cursor): removido por pedido
  explícito del dev.
- **GizmoKeyTapState con timer cosa-loca**: simplificado a `lastKey
  + lastPressTime` + ventana 0.4s. Menos código.
- **Visual de modal con línea punteada del plan original**: cambiado
  a línea sólida + anillo (más visible). Trivial revertir si emerge
  preferencia.
- **Snap-to-vertex** (snap a vertices del scene en lugar de al
  grid): scope mayor + interaction model distinto. Diferido.
- **Marquee select en orto** (rectángulo de selección sobre múltiples
  brushes): scope mayor + se solapa con el block tool en empty
  space. Diferido — no bloquea el modeling flow.

**Condiciones de revisión:** si el dev pide marquee select, snap-to-
vertex, brush poligonal cóncavo, o modal G (translate cursor-driven).
Si emerge preferencia por "rotate sobre view-axis" en lugar de Y
default + axis lock, ajustar el modal Rotate.

---

## 2026-05-08: F2H29 — Block tool + drag-edit en ortos (descope tarde, paquete polish a F2H30)

**Problema:** F2H29 plan original (escrito al cerrar F2H28) prometía 3 bloques de edición en ortos: drag-edit (Bloque B), block tool (Bloque C), vertex/edge edit (Bloque D). Tras implementar y validar B+C, el dev probó el flujo y emergió scope nuevo: gizmo rotate no proporcional al brush (bug pre-existente F2H13), pedido de atajos Blender-style `S/R/G` modal con cursor + línea punteada, pedido de "pincel" poligonal (clicks sobre vertices del grid hasta cerrar mesh — feature distinta del Bloque D vertex/edge edit).

**Decisión:** **descopear el Bloque D + cerrar F2H29 con B+C** + crear F2H30 como paquete polish unificado que junta:
1. Vertex/edge edit (Bloque D diferido).
2. Brush poligonal "pincel" (feature nueva).
3. Gizmo rotate proporcional al AABB del brush (bug pre-existente).
4. Atajos Blender `G` / `R` / `S` modal (feature nueva).

Razones:
- F2H29 ya entrega valor completo y validado por el dev: drag-edit funciona, block tool funciona, gizmo en posición correcta, preview en 4 vistas. El dev puede modelar mapas básicos sin Inspector.
- Bloque D (vertex/edge edit) tiene **alta superficie de bugs** (mover vertice = mutar 3 planos + validar `isBrushValid` post + revertir si rompe) y **bajo valor inmediato** vs. los 3 pedidos polish que el dev hizo explícitamente.
- Los 4 ítems forman un hito coherente de UX-polish del editor de mapas. Splittear más fino agrega ceremonia sin separar dominios reales.
- Decisión explícitamente confirmada por el dev: *"si el ctrl y funciona, y me gusta ese plan siguelo"*.

**Alternativas descartadas:**
- **Forzar Bloque D ahora**: scope creep, rompería el commit de validación incremental con un bloque alto-riesgo. Descartado.
- **Hito separado por cada feature nueva** (F2H30 vertex, F2H31 brush poligonal, F2H32 gizmo, F2H33 atajos): ceremonia sin valor; las 4 features comparten dominio (manipulación de geometría en orto + atajos del editor) y se validan juntas.

---

**Sub-decisión 1 — DragState pulse-style en `OrthoViewportPanel`:**

El panel emite 2 estructuras al caller:
- `DragState { active, justEnded, ndcStart, ndcCur }` — `active=true` mientras LMB-down + delta > 4 px, `justEnded=true` un frame al soltar.
- `ClickSelect { pending, ndc }` — sólo dispara si delta < 4 px.

Mutuamente excluyentes: drag o click, nunca ambos en el mismo frame.

**Razones:** preserva el flow del Bloque F de F2H28 (click-select sin tocar) + agrega drag sin nuevo conflicto. La struct `DragState` separa `active` (continuo durante el drag) de `justEnded` (pulso) — el caller usa `active` para mover en vivo y `justEnded` para pushear el command.

---

**Sub-decisión 2 — Snap al delta, no a posición absoluta (drag-edit):**

```
pos_new = startPos + round((cur_world - start_world) / snap) * snap
```

vs. la alternativa:
```
pos_new = round((startPos + delta) / snap) * snap
```

**Razones:** la 1ra fórmula preserva el offset original del brush respecto al grid (si arrancó desalineado, sigue desalineado pero se mueve en pasos de `snap`). La 2da forzaría al brush a alinearse al grid global en cada move, desplazándolo bruscamente al primer drag. Convención Hammer.

**Block tool usa la opuesta**: snap a las ESQUINAS del rectángulo dibujado (no al delta). Razón: el brush nuevo arranca alineado al grid, lo cual ES lo deseado (no preserva offset previo porque no había brush previo).

---

**Sub-decisión 3 — `pickEntityFromRay` como helper público en ScenePick:**

Bloque B necesitaba pick orto con rayo paralelo. 3 approaches:
1. Duplicar el loop forEach. Descartado: drift garantizado.
2. Refactor con functor que arma el rayo. Descartado: API rara.
3. **Extraer helper público** `pickEntityFromRay(scene, origin, dir, assets)` y hacer que `pickEntity` perspective sea wrapper que arma el rayo con `invVP * (ndc, ±1)` y delega. **Elegido.**

**Razones:** los tests existentes de `pickEntity` siguen verde sin cambios. El helper queda reusable para futuros picking sintéticos (Lua raycast, vertex picking en F2H30, etc).

---

**Sub-decisión 4 — Preview AABB en 4 vistas via debugRenderer:**

Plan original especulaba con preview "AABB cyan via debugRenderer durante drag — visible en perspectiva 3D" y mencionaba que en los 2 ortos extra **NO** aparecería (renderOrthoView no usa debugRenderer). El dev al validar pidió ver el preview en los 3 ortos también: *"creo que deberia haber un boton que active la capacidad de dibujar y que se vea en el wireframe porque seguramente van a crear en el wireframe y luego acomodar en el 3d"*.

**Decisión:** la sesión `OrthoBlockToolSession` guarda `previewMin/Max`. `EditorRenderPass.cpp` re-encola el AABB en `debugRenderer` ANTES de cada `renderOrthoView`. `renderOrthoView` flushea el debugRenderer al final con sus matrices. Cada flush limpia la cola → cada vista renderiza su set queue-eado.

**Razones:** reusa la infra del debug renderer existente (shader + VAO ya cargados). Cero código nuevo de líneas/shaders. Costo: 1 drawAabb call extra por orto + 1 flush extra → trivial.

**Alternativas descartadas:**
- **Overlay 2D en ImGui drawlist en el panel orto fuente**: solo aparece en 1 orto, no resuelve el pedido del dev.
- **Mesh de líneas dedicado**: requiere shader + VBO transient. Overkill para 1 AABB.

---

**Sub-decisión 5 — Color celeste GMod para preview (no cyan):**

Plan original usaba cyan `(0.2, 0.9, 1.0)` para el preview AABB (copy-paste de los drop highlights del editor). Dev al validar: *"porque es cyan? no controlabamos colores de valve y celeste garry'smod?"*.

**Decisión:** preview en celeste GMod `(108, 193, 229)` — mismo RGB que el wireframe regular del orto (`k_wireframeColor` en `SceneRenderer_Ortho.cpp`). Visualmente coherente: lo que el dev ve mientras dibuja es el mismo color que tendrá el brush al materializarse.

---

**Sub-decisión 6 — `spawnBoxBrushAt` rebasea brush a local space (fix de origen):**

Bug detectado en validación: el block tool spawneaba el brush con `tf.position = (0, 1, 0)` (hardcoded en `spawnBrushEntity`) pero la geometría tenía la translation baked-in (porque el block tool pasaba `T(center) * S(dims)` directo a `makeBoxBrush`). Resultado: gizmo en `(0, 1, 0)` y mesh visible lejos.

**Decisión:** `spawnBoxBrushAt` descompone el transform en `center + dims`, construye el brush con sólo `S(dims)` (local space), y override `tf.position = center` post-spawn.

**Razones:** mismo problema y misma fix que F2H12 boolean ops resolvió con `snapshotResultWorld` (rebase planos a local space tras computar centroid). Sin esta fix, drag-edit del brush spawneado se rompe (gizmo no agarrable).

---

**Sub-decisión 7 — Welcome modal fix lateral en `EditorUI` ctor:**

Bug pre-existente reportado por dev al validar F2H29: la pantalla Welcome modal mostraba panels en posiciones stale (residuo del último workspace de la sesión previa via `imgui_layout_v2.ini` auto-loadeado).

**Decisión:** `EditorUI` ctor llama ahora `m_dockspace.requestRebuildForCurrentWorkspace()` después de `applyDefaultVisibilityForWorkspace(initialWs)`. El primer frame ignora el ini stale y construye fresh.

**Razones:** sin proyecto cargado no hay personalización del dev para esta sesión. Cuando carga proyecto, `setWorkspaces` restaura los iniLayout custom del `.moodproj` y este rebuild queda overriden — sin pérdida real.

**Alternativa descartada:** hide del dockspace mientras Welcome está activo. Más invasivo (cambia el flow de render principal); fix actual es 1 línea.

---

**Condiciones de revisión:**
- Si el descope tardío del Bloque D resulta ser un patrón repetido (más hitos splitteados a mitad de implementación), revisar el alcance de los planes iniciales — quizás están siendo demasiado ambiciosos. Por ahora 1 caso aislado; aceptable.
- Si F2H30 (gizmo polish + atajos Blender + brush poligonal + vertex/edge) emerge como hito grande (>1 semana), considerar splittear: F2H30 = vertex/edge + brush poligonal; F2H31 = gizmo polish + atajos Blender.
- El cambio de paleta a celeste GMod (no cyan) deja documentado el principio: usar paleta del workspace (Valve+GMod) para overlays del workspace mismo. Si emerge necesidad de overlays con colores nuevos (ej. preview de boolean op), agregar al `paleta del workspace` consciente.

---

## 2026-05-08: F2H28 — Editor de mapas 4-viewport (split en 2 hitos, fondo negro, snap solo de display)

**Problema:** F2H28 originalmente quería entregar **todo** el editor estilo Hammer en un solo hito: layout 4-viewport + render orto wireframe + grid 2D + click-select + block tool (dibujar rectángulo en orto → crear brush) + drag-edit (mover brushes desde orto con grid snap) + vertex/edge edit. Estimación: ~12 bloques con bugs cruzados (cada feature de edición depende del render + selection + grid; un bug en un layer rompe los otros).

**Decisión:** **Split explícito en 2 hitos**. F2H28 entrega los **fundamentos**: layout + render + grid + click-select + snap visual. F2H29 entrega las **3 features de edición**: block tool, drag-edit, vertex/edge edit. El snap step expuesto por F2H28 (`m_hammerSnapStep` cycleable con Ctrl++/Ctrl+-) se aplica al delta del drag en F2H29.

**Razones:**
- **Bug isolation**: el render multi-viewport + grid + click cross-viewport ya es código nuevo crítico. Sumar drag-edit (que muta el Transform del brush en vivo viendo update en las 4 vistas) duplica la superficie de bugs.
- **MVP visual primero**: el dev puede USAR el workspace inmediatamente para navegar/seleccionar; las features de edición vienen después con esa base ya validada.
- **Patrón ya usado**: F2H25 (cull overlap) + F2H26 (runtime-load) splittearon dos features tightly-coupled del mismo dominio CSG. Mismo approach acá.

**Alternativas descartadas:**
- **Hito grande único de 12+ bloques**: bugs cruzados retrasan todo, no hay punto intermedio para validar visualmente con el dev. Descartado.
- **Diferir block tool a F2H30+**: los 3 (block, drag-edit, vertex) son tightly-coupled — comparten el manipulador 2D del orto + el snap. Splittear más fino agrega ceremonia sin separar dominios reales. Descartado.

---

**Sub-decisión 1 — Label castellano "Editor de mapas" (no "Hammer"):**

Plan original llamaba al workspace "Hammer". Cambio durante implementación: alineamos con la convención F2H22 (workspaces orientados a TAREAS — "Layout", "Programar", "Materiales") y usamos label "Editor de mapas". Internamente seguimos hablando del estilo Hammer/Source como inspiración técnica, pero el dev ve el nombre de la tarea.

**Razones:** consistencia con los otros 3 workspaces; "Hammer" es referencia que solo entiende quien conoce Source SDK; "Editor de mapas" describe qué hace el dev cuando entra ahí.

---

**Sub-decisión 2 — Fondo NEGRO en lugar de gris claro `#C8C8C8`:**

Plan original especificaba paleta Valve+GMod con fondo gris claro `#C8C8C8` (mimic Hammer original). Cambio durante validación visual: dev pidió *"quiero cambiar el fondo gris por negro"* — el wireframe celeste GMod `#6CC1E5` resalta mucho mejor sobre negro que sobre gris.

Ajustes acompañantes: grid menor cambió de `#7A7A7A` (visible sobre gris claro) a `(40,40,40)` (sutil sobre negro, no compite con el wireframe). Grid mayor `#F58220` (naranja Valve) preservado — pop sobre negro mejor que sobre gris.

**Razones:** el plan era guess; la validación visual es la fuente de verdad. Fondo negro es la convención de muchos editores modernos (Unreal Editor wireframe, Houdini network views) y subjetivamente más cómodo para sesiones largas.

---

**Sub-decisión 3 — `OrthoCamera` en `editor/panels/scene/`, NO en `engine/scene/cameras/`:**

Plan original sugería poner `OrthoCamera` en `engine/scene/cameras/Camera.h` (junto al `EditorCamera` orbital y `FpsCamera` del player). Decisión durante implementación: ponerlo en `editor/panels/scene/OrthoCamera.h` junto al `OrthoViewportPanel` que la usa.

**Razones:**
- `OrthoCamera` es **solo del editor** — el `MoodPlayer` no la necesita (no tiene workspace orto).
- Acoplamiento mínimo: el SceneRenderer NO conoce `OrthoCamera` directamente (recibe `panOffset` + `worldHeight` como params plain). Si en el futuro hace falta usarla en `engine/`, mover es trivial.
- Evita layering issues: `engine/` no debe depender de `editor/`. Si `OrthoCamera` viviera en engine y luego algún partial de engine la incluyera, romperíamos la regla.

---

**Sub-decisión 4 — `pickEntityFromRay` como helper público + `pickEntity` delega:**

Bloque F necesitaba picking ortográfico (rayos paralelos). Tres approaches posibles:

1. **Duplicar el loop**: copiar el `forEach<TransformComponent>` con AABB/sphere tests a una función nueva. Descartado: 25 líneas duplicadas, drift garantizado.
2. **Refactor con `unproject lambda`**: pasar a `pickEntity` un functor que arma el rayo. Descartado: API rara, hard de testear.
3. **Extraer helper público `pickEntityFromRay(scene, origin, dir, assets)`** y hacer que `pickEntity` (la versión perspective con view+proj+ndc) sea un wrapper que arma el rayo y delega. **Elegido.**

**Razones:** los tests existentes de `pickEntity` siguen verde sin cambios (mismo loop interno). El nuevo helper queda público y reusable para futuros picking sintéticos (ej. raycast desde script Lua, F2H30+).

---

**Sub-decisión 5 — Snap step solo de display en F2H28, aplicado a drag en F2H29:**

Plan original: `m_hammerSnapStep` cycleable con Ctrl++/Ctrl+- + label "Grid: Nu". Validación durante implementación: el dev preguntó *"luego habrá el snap to grid?"* — confirmando que el snap visual NO actúa sobre movements aún.

**Decisión:** F2H28 expone el valor (UI + uniform del grid shader) pero NO lo aplica al delta del drag (porque drag-edit es F2H29). El handler Ctrl++/Ctrl+- vive en `EditorApplication.cpp::processEvents` con guard de workspace activo.

**Razones:** mantiene F2H28 puramente visual + de selección. La aplicación al drag entra como sub-bloque natural de F2H29 (`pos = round(pos / snap) * snap`). Sin riesgo de regresión: F2H29 solo necesita LEER el valor existente.

---

**Condiciones de revisión:**
- Si el costo del render orto (~3x CPU del frame perspectivo cuando workspace activo es "Editor de mapas") emerge como cuello, optimizar con frustum culling per-viewport o reducir ortos a 2 (top + frontal) configurable.
- Si el dev pide volver al fondo gris (rechazo subjetivo del negro), revertir; los colores quedan como constantes nombradas en `SceneRenderer_Ortho.cpp` para hacer el cambio trivial.
- Si F2H29 (drag-edit) revela que `OrthoCamera` necesita state adicional (ej. clipping plane near/far user-configurable), promoverla a `engine/` cuando ese state aparezca, NO antes.

---

## 2026-05-03: Reorganización arquitectónica de `src/` por dominios (F2H1)

**Problema:** post-v1.0.0 el árbol `src/engine/` era flat con 11K líneas en sub-carpetas planas (`render/`, `scene/`, `physics/`, etc.). Cualquier hito futuro de Fase 2 que aporte 3-5K líneas adicionales (CSG, dialog, quest, material node-graph) iba a contaminar más esa estructura. Antes de empezar a sumar features, reorganizar.

**Decisión:** subdividir `engine/`, `systems/` y `editor/` por sub-dominio explícito. Plan completo en `PLAN_FASE2.md` sección 2. Los movimientos se hicieron bloque por bloque con commits atómicos:

- **Bloque A — `engine/render/`**: split en `rhi/` (interfaces), `backend/opengl/` (impl GL — único lugar que incluye `glad/gl.h`), `pipeline/` (Fog, LightGrid, math), `resources/` (Mesh/Material assets), `scene_renderer/` (coordinador). 5 sub-commits.
- **Bloque B — `engine/scene/`**: split en `core/` (Scene, Entity, Cameras), `components/` (Components.h), `serialization/` (movido desde `engine/serialization/`), `queries/` (ScenePick/ViewportPick). 4 sub-commits.
- **Bloque C — `engine/physics/`**: PhysicsWorld a `world/`, placeholders para `components/` `character/` `queries/`.
- **Bloque D — `engine/animation/audio/scripting/assets/`**: cada uno subdividido. 4 sub-commits.
- **Bloque E — `engine/world/`**: GridMap+Pathfinding a `grid/`, placeholders `csg/` (F2H9+) + `streaming/` (Fase 3).
- **Bloque F — `engine/game/`**: subdivisión en manifest/overlay/state + placeholders dialog/quest/inventory + nueva carpeta `engine/i18n/` (F2H5).
- **Bloque G — `systems/`**: subdividido por dominio: render/physics/animation/ai/particles/audio/light/scripting.
- **Bloque H — `editor/`**: split en `application/` (EditorApplication + 6 partials), `ui/` (Dockspace/MenuBar/StatusBar/EditorUI), `panels/{scene,assets,debug,world}/` por categoría. 3 sub-commits.

**Razones:**
- **Cabe en mente.** Cada subcarpeta es un dominio identificable; nuevas features tienen lugar obvio.
- **Reglas de dependencia aplicables.** Con `backend/opengl/` aislado, el "único lugar con glad/gl.h" se vuelve operacional, no aspiracional.
- **Placeholders `.gitkeep` para hitos futuros.** Cuando F2H9-F2H16 agregue brushes 3D, hay carpeta esperándolos. Idem dialog/quest/inventory en F2H29-F2H31.
- **Zero regression.** Cada sub-bloque cierra con la suite intacta (319/6613). Editor + MoodPlayer compilan limpios y se ven idénticos a v1.0.0.

**Trade-offs:**
- Se pierde: profundidad de paths más larga (ej. `engine/scene/serialization/SceneSerializer.h` vs `engine/serialization/SceneSerializer.h`). Aceptable: el IDE autocompleta y el cambio de path se hizo con sed bulk, no a mano.
- Se gana: superficie de "engine flat" desaparece. Cualquier dev que llega al repo en F2H10+ encuentra `engine/world/csg/` directamente y sabe dónde editar.

**Revisar si:** algún sub-bloque queda con 1 archivo solo durante mucho tiempo (puede colapsarse). Por ahora todos justifican existencia futura (placeholders documentados con `.gitkeep`).

## 2026-05-03: Tracy adoptado como profiler oficial (F2H2)

**Contexto:** F2H2 abre la sub-fase de optimización de Fase 2. Antes de empezar a optimizar (frustum culling F2H3, LOD F2H4, batching futuro) hay que saber **dónde** se va el tiempo de frame. Sin profiler real, las decisiones serían intuición.

**Decisión:** integrar **Tracy v0.11.1** como profiler. Cliente linkeado al ejecutable vía CPM (target `Tracy::TracyClient`), controlado por la opción CMake `MOOD_PROFILE` (default ON). Macros propias `MOOD_PROFILE_FRAME / SCOPE / FUNCTION / PLOT` en `src/core/Profiler.h` que expanden a Tracy cuando ON y a `do{}while(0)` cuando OFF (coste cero en builds finales).

Instrumentación inicial: 10 zonas en `EditorApplication::run` (eventos / UI / física / scripts / animación / nav / partículas / audio / render / present) + 8 sub-zonas dentro de `SceneRenderer::renderScene` (shadow / skybox / light grid / PBR static / PBR skinned / particles / debug / post-process). Plots de FPS y entity count.

**Razones:**
- **Cliente embedded ≠ servidor externo.** El cliente vive linkeado, el servidor es `tracy-profiler.exe` portable que se conecta por TCP. Con server cerrado la overhead es ~1-2% del frame; con server abierto ~5-8%. En Release con `MOOD_PROFILE=OFF` las macros desaparecen del binary.
- **Granularidad real**: Tracy mide por zona con stddev / min / max — no es una vista global tipo "frame ms". Con 3163 frames de captura ya tenemos `mean / max / stddev` por cada zona.
- **`tracy-csvexport.exe`** permite extraer los % de zona a CSV sin la GUI — útil cuando hay mismatch de versión o headless. Esta sesión lo usó: el `tracy-profiler.exe` que el dev bajó tenía protocolo distinto (mismatch), pero `tracy-capture.exe` capturó el `.tracy` y `tracy-csvexport.exe` lo decodificó. La data del baseline final salió de ese pipeline.
- **CMake CPM** lo trae en una línea, sin dependencia adicional al sistema.

**Alternativas consideradas:**
- **Optick**: similar feature set, menos mantenido. Tracy tiene más adopción y mejor visualización.
- **`std::chrono` ad-hoc**: sin overhead pero sin granularidad por zona ni vista temporal — solo da un total. Insuficiente para identificar cuellos.
- **Profilers nativos (Visual Studio Profiler, Superluminal)**: excelentes pero requieren instalación + licencia / setup. Tracy es portable y lib propia, va con el repo.
- **PIX / RenderDoc**: orientados a GPU, no a CPU + frame-level. Complementarios pero distintos.

**Trade-offs:**
- Se pierde: tamaño binario crece ~500KB con Tracy linkeado en Debug. Aceptable en builds de desarrollo, irrelevante en Release con `MOOD_PROFILE=OFF`.
- Se gana: cuellos identificables por zona con stddev. Para F2H2 inmediatamente útil: `PBR::staticPass` se queda con el 70.74% del frame con 836 cubos = decisión clara para F2H3.

**Lección aprendida (mismatch de versión):** el cliente linkeado y el `tracy-profiler.exe` deben ser **exactamente la misma versión**. Bajar el ZIP del tag exacto (v0.11.1 en este caso). El protocolo TCP cambia entre versiones y el server muestra "Protocol mismatch" al conectar. Si pasa, fallback al pipeline `tracy-capture.exe` → `tracy-csvexport.exe`.

**Revisar si:** Tracy deja de ser mantenido (improbable a corto plazo, proyecto activo) o si una migración a Vulkan/D3D12 requiere features de profiling GPU más profundas (entonces evaluar PIX o RenderDoc en paralelo, no como reemplazo).

## 2026-05-03: Frustum culling plano, no jerárquico (F2H3)

**Contexto:** F2H2 midió que `PBR::staticPass` se llevaba el 70.74% del frame con 836 cubos. F2H3 ataca eso descartando entidades fuera del frustum antes del draw call. La pregunta: ¿implementación plana (loop linear sobre todas las entidades) o jerárquica (BVH / octree / quadtree espacial)?

**Decisión:** **plana** para v1, con **API diseñada para que el upgrade interno a estructura jerárquica sea transparente** para los callers. El loop de PBR static itera por entidad como antes, agrega 3 líneas: calcular AABB world, test contra frustum, `continue` si fuera. El header `Frustum.h` expone `aabbVisible` y `worldAabb` en el estilo header-only del motor; los callers no iteran por dentro de una estructura — la API es "test este AABB contra este frustum".

**Razones:**
- **No es cuello todavía.** El test AABB-vs-frustum con truco p-vertex es 1 dot product por plano → 6 dots × ~1 ns = 6 ns por entidad. Para 836 entidades = 5 µs totales. Para 10K entidades = 60 µs. Para 100K = 0.6 ms. Cuando el dato medido diga que el loop linear es cuello, agregamos BVH como hito propio. Hoy no.
- **Costo de implementación.** Un loop de 3 líneas vs un BVH (~300-500 LOC con build, refit en cambios de transform, traversal por frustum, tests). El BVH además agrega risk de bugs en refit incorrecto cuando un transform cambia.
- **API extensible.** Hoy `aabbVisible(aabb, frustum)` se llama por entidad. Mañana, si reemplazamos el loop por `bvh.queryFrustum(frustum) → [aabbs visibles]`, ese cambio queda aislado en `SceneRenderer::renderScene` — el resto del código no se entera.
- **Profile don't guess.** Filosofía Fase 2 (ver `docs/PLAN_FASE2.md`): no optimizar antes de medir. F2H3 nace con datos, no con intuición. El próximo nivel también nacerá así.

**Alternativas consideradas:**
- **BVH dinámico desde día 1**: rechazado por scope. ~5x más código, más superficie de bugs, sin necesidad inmediata.
- **Octree estático**: no encaja con el modelo de mundo dinámico (entidades se mueven con scripts y physics). Refit constante haría perder la ventaja.
- **Spatial hashing (grid uniforme)**: alternativa simple al BVH; postergado para evaluar si emerge como necesidad real.

**Scope explícito de F2H3:**
- Cull solo en `PBR::staticPass`. Shadow + skinned passes excluidos.
  - **Shadow**: cada light tiene su propio frustum (CSM o radius esfera para points). Implementarlo correcto es ~2x el código de v1, y el baseline F2H2 dice que `ShadowPass` ni aparece en top 10 zonas. ROI bajo.
  - **Skinned**: sub-1% del baseline. Cuando emerja en un baseline futuro lo agregamos.
- AABB world-space calculado **on-the-fly cada frame** (sin caché por dirty-flag). Costo medido ~30 ns por entidad, despreciable. Caché entra solo si una medición lo justifica.
- Test conservador: descarta solo cuando los 8 vértices del AABB están del lado negativo de un mismo plano (truco p-vertex). Falsos positivos posibles (AABB que pasa pero no se ve), falsos negativos NO. Es la garantía correcta — preferimos dibujar de más a "que se note un mesh faltando".

**Trade-offs:**
- Se pierde: rendimiento extra que un BVH daría a >100K entidades (cuando ese escenario aparezca).
- Se gana: 3 líneas de código + 12 tests + API estable. Rendimiento medido x13.3 cuando el viewport está vacío de mesh (escena 10K, cámara apuntando lejos).

**Validado en producción** (testtrace2.tracy, 1162 frames, GTX 1660):
- `PBR::staticPass` 70.74% → 15.73% del frame (promedio mezcla escenarios).
- Max de la zona: 2574 ms → 251 ms (10x menos varianza en peor caso).
- `PBR::CulledStatic` plot reporta hasta 835/836 entidades descartadas.
- Costo del culling: <<1% del frame, no entra en top 10 zonas.

**Revisar si:** una medición futura muestra que el loop linear (no el cull en sí) es cuello — escenario probable solo a >100K entidades visibles, lo cual el motor todavía no soporta por otros cuellos pendientes (LOD F2H4, batching futuro).

## 2026-05-03: F2H4 = instancing (swap con LOD original) + decisiones MVP de instancing

**Contexto:** `PLAN_FASE2.md` originalmente colocaba F2H4 = LOD. Cuando llegó el momento de implementarlo después de F2H3, los datos medidos del baseline F2H2 mostraban que el cuello real era **`PBR::staticPass` 70.74% del frame, mean 42.5 ms / 836 entidades = 50 µs por cubo de 12 tris**. Eso es CPU-bound en draw call submission, no GPU-bound en triangle processing. LOD ataca lo segundo; instancing ataca lo primero. Si hubiéramos seguido el plan literal, F2H4 LOD habría dado mejora marginal (los cubos primitivos ya tienen 12 tris, no hay LOD que reducir) y dejado el cuello real intacto.

**Decisión:** **swap F2H4 ↔ F2H5**. Este F2H4 = "Instancing del pase opaco estático", F2H5 = "LOD" (postergado). El plan F2H4 LOD no se descarta — sigue siendo necesario cuando aparezca contenido con meshes de alto poly (Fox.glb, CesiumMan.glb, props complejos del catálogo Kenney). Hoy no hay con qué medirlo útil.

**Validación medida (test3.tracy, 4993 frames):**
- `PBR::instancedPass` mean **0.88 ms** vs `PBR::staticPass` baseline F2H2 mean **42.5 ms** = **~48x más rápido por draw**.
- Escena 10K (836 cubos): antes 4 FPS / 836 draws, ahora **60 FPS / 3 draws** (vsync cap; el rendering ya no es el cuello).
- Escena 100K (8336 cubos): antes congelaba el editor, ahora **10.4 FPS / 3 draws / 82K tris** = editable. F2H3 + F2H4 en cadena hicieron viable un escenario que el motor literalmente no soportaba.
- Costo del helper `groupByBatch` + culling: <<3% del frame con 836 cubos. Despreciable.

**Decisiones MVP del instancing (sub-decisiones, valen revisarse cuando emerjan):**

1. **Re-upload del VBO de instancias cada frame con `glBufferData` orphan-then-fill**: el más simple. Para 836 mat4 = ~50 KB, transferencia en microsegundos. Probable que resista hasta 10K-20K instancias visibles. Cuando emerja (medición futura), upgrade a *persistent mapped buffers* o *triple buffering* sin tocar la API pública (`OpenGLInstanceBuffer::upload`).
2. **Sin caché de matrices model**: las recalculamos cada frame en `groupByBatch`. Para 836 entidades = micro-segundos. Cuando 100K+ entidades estáticas justifiquen caché, agregar dirty-flag en `TransformComponent`. La API de `groupByBatch` no cambia.
3. **Reglas estrictas de batcheable**: 1 submesh + `materials.size() ≤ 1`. Multi-submesh y materiales mixtos caen al path no-instanced del F2H3. Conservador para v1 — extender el grouping a "batch por submesh" cuando contenido real lo justifique.
4. **Shader instanced separado** (`pbr_instanced.vert` + reuso de `pbr.frag`): no usamos `#define INSTANCED` con un solo .vert porque mezcla código con/sin atributos en el mismo archivo dificulta debug de VAO state. Los dos `.vert` son menos LOC totales que el toggle.

**Trade-offs del enfoque MVP:**
- Se pierde: rendimiento extra que persistent mapped + scene caching darían a >100K entidades.
- Se gana: ~700 LOC totales (helper + RHI + shader + cableo + tests). API limpia y extensible. **Cuello del 70% medido en F2H2 → 2.5%** con esta inversión.

**Lección aprendida:** atacar lo que se mide, no lo que el plan original asumió. F2H2 fue exactamente para esto — para que F2H3+F2H4 entren con datos. Si hubiéramos seguido el plan literal, habríamos hecho LOD primero (mejora marginal) y descubierto el cuello real recién después.

**Revisar si:**
- El upload del VBO emerge como cuello (medible en escenas con cambios bruscos de cuántas entidades son visibles por frame).
- Una medición muestra que el helper `groupByBatch` es cuello (reupload cada frame con 100K+ entidades).
- Aparece contenido real con multi-submesh frecuente que justifique extender el grouping a "batch por submesh".

## 2026-05-03: F2H5 = virtualización ImGui Hierarchy + lección sobre predicciones de % de mejora

**Contexto:** F2H4 destapó el cuello `UI::draw` 19% del frame con 8336 entidades. F2H5 según `PLAN_FASE2.md` era LOD; segundo swap consecutivo (F2H4 ya había swapeado con LOD original). El plan F2H5 predijo que virtualizar el panel Hierarchy bajaría `UI::draw` de 19% a <2%, llevando el FPS de 10.4 a 12-15.

**Decisión:** seguir adelante con F2H5 = virtualización Hierarchy. Refactor con `ImGuiListClipper`, helper `collectHierarchyEntries` extraído a `HierarchyCollect.cpp` para testing.

**Resultado medido (CSV + repetición de medición sin Tracy):**
- 100K_full_view: 96 → 90.5 ms / 10.4 → 11.0 FPS = **~6% mejora** (no el 25-40% predicho).
- 100K_no_view: 89.9 → 86.5 ms / 11.1 → 11.5 FPS = **~4% mejora**.

**Por qué falló la predicción:** el plan asumía que `UI::draw` (19% del frame) era **dominantemente** el panel Hierarchy. La realidad: ese 19% se reparte entre Hierarchy + Inspector (con muchos componentes seleccionados) + AssetBrowser + Performance HUD + gizmos + tooltips. Atacar solo el Hierarchy mejora una fracción, no el total.

**Lo que F2H4 destapó realmente** (visible al cerrar F2H5): el cuello con 8336 entidades NO es un panel específico — es **scene iteration distribuida**. Multiples sistemas (`Animation/Script/Nav/Particle/Audio/Trigger`) hacen `scene.forEach<...>` cada frame, sumando costo lineal por entidad. F2H5 no atacaba eso.

**Aprendizaje aceptado para futuros hitos:**
1. **Las predicciones de % de mejora con escenas grandes son frágiles**. Tracy mide zonas con nombre fijo — `UI::draw` engloba todos los panels, no separa por panel. Antes de predecir mejora, instrumentar sub-zonas (`UI::Hierarchy::draw`, `UI::Inspector::draw`, etc.) para tener attribución real.
2. **Atacar lo medido, no lo asumido** (lección F2H4 reaplica). Sin sub-zonas en F2H4 no podíamos saber qué fracción del 19% era cada panel.
3. **Una mejora del 6% sigue siendo positiva** y el código del refactor es correcto: el patrón ListClipper queda como template para `AssetBrowser`, `Inspector` listas, etc. cuando crezcan.

**Decisión de continuar (no revertir):**
El refactor en sí es código limpio y reusable. No introduce regresiones (suite 345/6736). El comportamiento visual es idéntico. **Cerrar F2H5 con la mejora real documentada** > mantenerlo abierto buscando más speedup.

**Aclaración crítica de scope (anotada al cerrar F2H5):**
Todas las mediciones de F2H2-F2H5 son en **build Debug + Tracy ON**. MSVC Debug agrega 5-10x overhead vs Release optimizado. Antes de invertir más hitos en optimización CPU, **medir Release** es el paso obligatorio. Predicción (sin medir aún): 100K_full_view pasaría de 11 FPS / 90 ms (Debug) a 45-60 FPS / 16-20 ms (Release).

**El stress 100K es patológico, no representativo:**
8336 cubos primitivos individuales. Mapas reales tienen muchos más triángulos pero muchas menos entidades:
- HL2 nivel típico: 500K-2M tris en 500-1500 entidades (geometría del nivel = pocos meshes grandes + props sueltos).
- Skyrim escena: 1-5M tris en 200-500 entidades.
- Doom Eternal encuentro: 10-50M tris en 1000-3000 entidades.

F2H3 + F2H4 cubren ambos casos: instancing para props repetidos, frustum cull para mesh grande no visible. **El motor con 100-300 entidades reales en una escena va a estar perfecto** — el stress test 8336 es un test de extremo, no un escenario de producto.

**Revisar si:**
- Una medición de Release muestra que el motor en producto sigue siendo limitado (improbable).
- Otros panels (Inspector, AssetBrowser) emergen como cuellos en escenas reales y justifican aplicar el mismo patrón ListClipper.
- Aparece la necesidad de instrumentar sub-zonas Tracy por panel para tener attribución correcta antes de futuros hitos UI.

## 2026-05-03: F2H6 LOD con cache en disco — cimiento sólido vs parche

**Contexto:** F2H6 retoma el LOD original (era F2H4 plan, postergado dos veces — primero por instancing F2H4, después por virtualización F2H5). El dev preguntó explícitamente "¿esto va a ser parche o cimiento sólido?" antes de implementar. Las decisiones siguen lo que hacen Unity, Unreal y Godot.

**Decisión 1 — Persistencia con cache en disco (NO regeneración pura):**

Lo que hacen los motores serios:
- Unity LOD Group → persiste en `.meta` del asset → load instant.
- Unreal Mesh Editor → persiste en `.uasset` → load instant.
- Godot auto-genera al import → persiste en `.scn` → load instant.

Implementación: `LodCache` lateral al formato `.moodmap` en `assets/.cache/lods/<hash>.moodlod`. FNV-1a 64-bit del logical path como nombre del archivo. Header binario con magic `MLOD` + version + mtime + size del source para invalidación. Borrar el dir `.cache/` no rompe nada — solo fuerza re-generación al próximo arranque.

**Razones:**
- **Tiempo de arranque**: regenerar 50 meshes complejos al cargar un proyecto = 5-30 seg de spinner. Persistir = abre instantáneo después del primer arranque.
- **Determinismo**: lo que ve el dev en el editor === lo que ve el jugador.
- **Workflow futuro**: cuando alguien quiera importar LOD custom de Blender, la API ya está lista (sub-hito futuro).
- **Si meshoptimizer hace algo feo**, el dev puede borrar el cache y regenerar (operativo, no ideal — UI editor es hito futuro).

**Por qué cache lateral, no schema bump:** bumpear `.moodmap` para guardar LODs implicaría migración + back-compat para todos los proyectos existentes. El cache es privado del workspace, no afecta proyectos compartidos. Si emerge necesidad de "guardar LODs en el proyecto" (ej. compartir LODs custom con el equipo), entonces sí schema bump como hito propio.

**Decisión 2 — Per-MeshAsset con default global (NO ranges hardcoded):**

Lo que hacen los motores serios: todos (Unity LOD Group, Unreal Mesh Editor, Godot) permiten configurar los rangos **por mesh**. Razón: un personaje hero usa LOD 0 hasta más lejos; un ladrillo de fondo usa LOD 2 desde cerca. Hardcoded global no escala.

Implementación: campo `lodDistances{30, 80}` en `MeshAsset` (default global). Override per-mesh disponible programáticamente. UI editor para cambiarlo es hito futuro — por ahora el dev edita los defaults o el código.

**Por qué hardcoded global hubiera sido parche:** ata el motor a un escenario "uniforme" que no existe en juegos reales. Cuando aparezca el primer mesh que necesita rangos custom (probable: un personaje hero visible desde lejos en FPS), tendríamos que re-arquitectar.

**Decisión 3 — Skinned meshes saltean LOD generation en v1:**

Reducir vértices en un mesh skinned implica re-mapear los bone weights consistentemente (cada vértice tiene 4 índices + 4 pesos que apuntan al esqueleto). meshoptimizer trabaja sobre vertex positions; los bone weights del vertex eliminado tendrían que redistribuirse a sus vecinos. Implementación correcta es scope grande con risk de bugs visuales (animación que se rompe en LOD 1/2).

Para v1: skinned siempre usa LOD 0. Postergado a hito propio cuando emerja un benchmark con >50 personajes skinned simultáneos. Hoy con CesiumMan + Fox como únicos skinned del catálogo, el cuello no aparece.

**Decisión 4 — Auto-gen al loadMesh, no en el editor manualmente:**

Workflow: el dev arrastra un `.glb` al proyecto, los LODs se generan automáticamente, se cachean en disco, se ven inmediatamente. Sin pasos manuales. Si el resultado de meshoptimizer no le gusta, edita el source y re-arrastra (mtime cambia, cache se invalida).

Alternativa rechazada: UI editor para "Generar LOD" / "Importar LOD custom" / barras visuales de threshold (estilo Unity LOD Group). Es scope grande para v1 — UX nice-to-have pero no esencial. Hito futuro cuando los workflows reales lo justifiquen.

**Trade-offs:**
- Se pierde: control fino sobre cada LOD (no podés "tocar" el resultado de meshoptimizer en el editor todavía). UX de Unity LOD Group con barras y screen-space size visual.
- Se gana: workflow zero-config. El dev importa un mesh y "simplemente funciona". Cache persiste, así que después de la primera vez no hay penalty de tiempo. API queda preparada para los hitos futuros sin re-arquitectar.

**Validación medida**: pendiente con escena de meshes complejos. Los stress patológicos actuales (cubos primitivos) no se benefician del LOD porque ya están en el mínimo (12 tris). Cuando entre contenido real (Fox.glb ~1500 tris, kenney_survival props ~500-2000 tris cada uno), spawnar 50+ instancias a distintas distancias mostrará el speedup esperado: LOD 1 = 750/250-1000 tris, LOD 2 = 225/75-300 tris.

**Lo que NO sería cimiento sólido (rechazado):**
- ❌ Regenerar siempre al load (sin cache): tiempo de arranque crece linealmente, ningún motor profesional hace esto.
- ❌ Schema `.moodmap` bump para guardar LODs: contamina el formato del proyecto, dificulta back-compat.
- ❌ Sin override per-mesh: ata el motor a un escenario uniforme irreal.
- ❌ Hardcoded de "siempre 3 LODs": Unity/Unreal soportan hasta 8 niveles. v1 usa 3 (0/1/2) con espacio para extender (basta agregar `lod3Submeshes` y otro threshold).

**Revisar si:**
- Una medición real con meshes complejos muestra que los ratios 50%/15% no son los óptimos (probable que ajustemos por dominio: characters quizás 60%/30%, props 50%/15%, vegetación 40%/10%).
- El cache crece mucho (>500MB en proyectos grandes) y hace falta política de evicción.
- Aparece la necesidad de un sub-hito UI para "Editor de LOD" (custom LODs importados, override per-mesh visual, sliders de threshold).
- Meshes con multi-submesh + materiales distintos por submesh emergen como caso común y necesitan extensión del flujo (hoy multi-submesh CAE al fallback no-instanced del F2H3, sigue funcionando pero sin LOD).

## 2026-05-04: Multi-mapa intra-proyecto (F2H8) + bug arquitectónico de "tags auto-generados"

**Contexto:** el `.moodproj` soporta `maps[]` desde Hito 6 pero el editor solo opera sobre `defaultMap`. El "Save As" del menú Archivo era un stub explícito ("no implementado, requiere UI multi-mapa"). El dev eligió cerrar la deuda con la opción C (multi-mapa intra-proyecto) en lugar de "Save Project As" (copiar carpeta entera, diferido a hito futuro).

**Decisión 1 — Scope multi-mapa**: F2H8 implementa el CRUD completo de mapas dentro de un proyecto: New, SaveAs, Open, SetDefault, Delete. Sin schema bump (todo lo necesario ya existía). Sin back-compat de proyectos viejos (el dev confirmó que están borrados). UX bajo `Archivo > Mapa` con submenu listando `project.maps[]`.

**Decisión 2 — Helper `MapsManager` PURO**: la lógica de "lista + default + current con invariantes" vive en `editor/project/MapsManager.{h,cpp}`, sin acoplarse a UI ni a disco. Invariantes que mantiene: ≥1 mapa, default + current dentro de la lista, dedup por `generic_string` (paths con separadores distintos = mismo path). Testeado con 13 cases en `tests/test_maps_manager.cpp`.

**Decisión 3 — Snapshot de mapas en EditorUI**: para que `MenuBar` pueda dibujar el submenu sin acoplarse al `Project` directamente, `EditorUI` mantiene un snapshot `(maps[], currentMap, defaultMap)` que `EditorApplication::syncMapsSnapshot()` refresca tras cada operación de mapas. Patrón consistente con `setProjectMapsSnapshot` similar al que usa el `WorkspaceManager` en F2H7.

**Decisión 4 — Bug arquitectónico de "tags auto-generados" descubierto durante el testing**: durante la validación visual de F2H8 emergió que cuando el dev movía el `Floor` (el piso del mapa) y switcheaba entre maps, el editor freezaba. Diagnóstico con datos del `.moodmap` confirmó que el Floor se duplicaba: `rebuildSceneFromMap` creaba un Floor default cada vez, y `applyEntitiesToScene` aplicaba el Floor del JSON sobre eso. Resultado: 2 planos 48×48 superpuestos → fillrate brutal → freeze.

**Fix arquitectónico**: el helper de `applyOneEntity` que reemplaza entidades con mismo tag al cargar (introducido en el fix de Tile_X_Y modificados) se extiende para cubrir TODOS los "tags auto-generados por rebuildSceneFromMap":
- `Floor` (entidad única).
- `Tile_X_Y` (entidad por celda del grid).

Lista hardcoded en `applyOneEntity`. Otros tags (Multi, CajaFisica, etc.) siguen permitiendo duplicados — necesario para `CreateEntityCommand::execute` (redo de comandos batch).

**Lección aprendida**: cualquier entidad creada automáticamente por el motor (no por el user) y que pueda ser modificada por el editor necesita persistencia + reemplazo en el load. El concepto "tag auto-generado" es la primera abstracción que aparece — si emergen más casos (e.g. `Skybox`, `LightProbe`), agregar al patrón.

**Test arquitectónico**: el caso del Floor está cubierto en `test_save_load_full_roundtrip.cpp` con un test que simula el flow exacto del editor (rebuildSceneFromMap → applyEntitiesToScene) y valida que hay un solo Floor con la position del JSON. Si alguien rompe el patrón en el futuro, el test falla con mensaje claro.

**Trade-offs:**
- Se gana: workflow estándar de IDE (varios mapas por proyecto, switching), bug fix del Floor que era prácticamente bloqueante para usar multi-mapa.
- Se pierde: Save Project As (copiar carpeta entera) sigue stub. Diferido — emergencia baja.

**Revisar si:**
- Aparecen escenas con multi-submesh + materiales mixtos modificados (caso similar a Tile pero para entidades arbitrarias).
- El user empieza a usar muchos mapas (>10) y la UI del submenu necesita scrolling/categorías.
- "Save Project As" emerge como necesidad real (workflow de "duplicar proyecto para experimentar").

## 2026-05-04: CI/CD con GitHub Actions (F2H10)

**Contexto:** post-F2H8 el proyecto tiene 396 tests + 8 hitos de Fase 2 cerrados. Sin CI, cualquier regresión accidental podía durar varios commits sin detectarse. Sub-fase 2.2 (CSG / editor de niveles real) viene grande — 2-3 meses estimados — entrar con red de seguridad activa es responsabilidad básica.

**Decisión 1 — Solo Windows MSVC**: el dev tiene como directiva durable que "multiplataforma Linux está fuera de scope" (memoria del proyecto). El plan original de Fase 2 mencionaba "Linux como test de portabilidad", pero eso nunca fue prioridad real. Si emerge necesidad cross-platform, agregar Linux como matrix entry es trivial; mientras tanto, no agrega valor a un proyecto solo del dev.

**Decisión 2 — Sin Dependabot**: el repo es propiedad personal del dev, sin colaboradores externos. Las deps CPM están pinneadas con `GIT_TAG` exactos (Tracy v0.11.1, meshoptimizer v0.21, etc.). Updates de deps van a hacerse de forma deliberada cuando emerja necesidad concreta — no como notificación automática semanal que solo agregaría ruido.

**Decisión 3 — Cache agresivo de CPM deps**: la primera build sin cache puede tardar 15-25 min descargando + compilando 8 deps grandes. Con `actions/cache@v4` y key derivada del hash de `CMakeLists.txt` + `cmake/CPM.cmake`, builds posteriores caen a 2-4 min. La key se invalida automáticamente si una dep cambia de tag. Cache adicional de build artifacts (`CMakeFiles` + `*.obj` + `*.lib`) acelera más cuando solo cambia código del repo.

**Decisión 4 — Build solo en Debug**: CI corre el mismo build que el desarrollo del dev. Release se difiere si emerge necesidad de validación de optimizaciones automáticas (ya hicimos Release manual en F2H5 testing visual y todo OK). Doble build (Debug + Release) duplica el tiempo de CI sin valor proporcional para v1.

**Decisión 5 — Release auto al taguear sin assets pre-compilados**: el workflow `release.yml` con trigger en `v*.*.*` crea un GitHub Release y usa el message del annotated tag como body (con `git tag -l --format='%(contents)'`). Esto da historial visible público sin trabajo manual. **Sin assets pre-compilados** porque hoy nadie consume el motor como binary — el dev clona y compila. "Binary releases" es hito propio cuando el motor sea consumible standalone (probable post-Fase 2 cuando exista MoodPlayer estable).

**Decisión 6 — `concurrency: cancel-in-progress`**: pushes rápidos seguidos (3 commits seguidos en main) no deben acumular 3 builds en cola — solo el último importa. Esto ahorra minutos del plan free de Actions y da feedback más rápido.

**Trade-offs:**
- Se pierde: validación automática en Linux/Mac, releases con binarios, alertas de updates de deps.
- Se gana: red de seguridad real contra regresiones, badge público de status, releases con historial visible y notes consistentes.

**Lección aprendida del primer build CI**: el cache se llena en la primera build (15-25 min). Si en el futuro alguien forkea el repo o agrega un nuevo branch, la primera build ahí también va a ser lenta. Aceptable como cost una sola vez por entorno.

**Revisar si:**
- Emerge un colaborador externo o un fork con PR — Dependabot puede tener sentido entonces.
- Aparece necesidad de validar Release automatizada (regresiones que solo aparecen con optimizaciones MSVC).
- "Binary releases" se vuelve necesidad real (cuando el motor sea producto consumible).
- Multiplataforma emerge como necesidad real (probable solo si el proyecto se vuelve open-source con tracción).

## 2026-05-04: CSG a mano (no manifold/Carve), brush implícito como representación canónica (F2H11)

**Contexto:** primer hito de sub-fase 2.2 (Editor de niveles serio estilo Hammer / TrenchBroom). Necesitamos elegir la representación de los brushes 3D y el algoritmo para convertirlos en mesh renderable. Tres approaches viables:

1. **Brush implícito + plane clipping a mano**: cada brush son N planos (uno por cara), la geometría sale de intersectar tripletes de planos. Algoritmo ~1500 LOC bien documentadas en "Real-Time Collision Detection" (Ericson) cap 5 + TrenchBroom source.
2. **manifold** (Google, MIT, lo usa Blender 4.x): library state-of-art, exact arithmetic, paralelo. Trabaja sobre mesh triangulada, no sobre brush implícito.
3. **Carve / libcsg**: abandonadas (Carve último commit 2014), pesadas, no las recomienda nadie hoy.

**Decisión:** **CSG a mano con brush implícito** (opción 1). Reusar libs existentes (glm para math, meshoptimizer para weld futuro en F2H16, nlohmann/json para persistencia, EnTT para components) pero el algoritmo CSG core es código propio.

**Razones (críticas para el roadmap):**

- **Lock-to-world UVs (F2H14)**: feature que define editores tipo Hammer. La textura no se deforma al mover el brush porque las UVs se calculan desde el plano global (no desde vértices triangulados). Solo posible si la representación canónica es por-plano. Con manifold (mesh triangulada) tendríamos que reconstruir esto a mano por encima — más trabajo, no menos.
- **Edición no destructiva**: arrastrás una cara, los planos se actualizan, la geometría se regenera deterministamente. Con mesh triangulada, mover una cara requiere reconstruir índices vecinos.
- **Operaciones booleanas limpias (F2H12)**: clipping de polígonos contra planos del otro brush ~500 LOC. Los brushes son convexos por construcción (trivial dado que son intersecciones de half-spaces), entonces los algoritmos se simplifican mucho.
- **Mapas livianos**: 100 brushes = 600 planos en JSON, no 100K vértices.
- **Approach industria estándar**: TrenchBroom (Quake/Half-Life mapping pro), Hammer (Source/Source 2), Godot CSGShape3D — todos a mano. Blender usa manifold pero Blender no es editor de niveles tipo brush, trabaja siempre con mesh triangulada y no necesita planos por cara.

**Alternativas descartadas:**
- **manifold**: por la pérdida de lock-to-world. Mantener manifold como escape hatch en hito futuro si surge un problema duro de robustez numérica que el clipping a mano no resuelva.
- **Carve / libcsg**: abandonadas.
- **CGAL**: industry-grade pero LGPL/GPL parcial, build pesadísimo, overkill para el scope.

**Convención del Plane** (durable, compartida con frustum culling):
`dot(normal, p) + distance = 0` define el plano.
`signedDistance(plane, p) = dot(plane.normal, p) + plane.distance`:
- > 0 → p del lado de la normal.
- < 0 → p del lado opuesto.
- ≈ 0 → p sobre el plano.

Para CSG, la `normal` apunta hacia AFUERA del brush. Un punto pertenece al brush ⇔ `signedDistance ≤ kPlaneEpsilon` en TODOS los planos. `kPlaneEpsilon = 1e-4f` da resolución sub-milimétrica sin caer en ruido de float (validado por tests de planos casi paralelos / coincidentes).

**`Plane.h` promovido** desde `engine/render/pipeline/Frustum.h` (donde lo introdujo F2H3) a `core/math/Plane.h`. CSG (subsystem en `engine/world/csg/`) no debe depender de `engine/render/pipeline/`.

**`BrushComponent` con ownership de `unique_ptr<IMesh>`** (excepción al patrón POD non-owning de Components.h): los brushes son geometría editable runtime, no encajan en el modelo asset-loaded-from-disk del AssetManager. Para no contaminar el header común, vive en su propio `engine/scene/components/BrushComponent.h`. `AssetManager::createDynamicMesh(verts, attrs)` expone la `MeshFactory` interna para crear IMesh runtime no persistidas en cache.

**Look "blank gris" para brushes sin material**: `albedoTint=0.7, roughness=0.85, useAlbedoMap=false`. NO caer al material slot 0 (missing.png) — ese es warning visible para meshes con material faltante, no para brushes que conscientemente no tienen material todavía (F2H14 los va a tener per-cara real).

**Schema `.moodmap` v9 → v10**: nuevo array top-level `brushes[]` paralelo a `entities[]`. `BrushComponent` se excluye de `entities[]` para evitar doble-persistencia. Mapas v9 sin `brushes[]` se cargan con lista vacía (back-compat aditiva). En F2H14 vamos a bumpear a v11 cuando el material pase de global por-brush a per-cara con UVs.

**Trade-offs:**
- Se pierde: cero deps nuevas, pero ganamos ~2000 LOC propias que mantener en lugar de ~100 LOC de bindings a manifold. Cualquier bug numérico en plane clipping es nuestro problema.
- Se gana: control total sobre el approach que matchea exactamente el roadmap (lock-to-world, multi-edit per-cara, compilación al guardar). Cero dependencia que pueda morir o cambiar API.

**Lección aprendida**: la elección de representación canónica define qué features son baratos vs caros más adelante. Manifold es excelente para **modelado** (Blender), pero el editor de niveles tipo Hammer es un workflow distinto — los brushes no son meshes editadas vértice por vértice, son volúmenes definidos por restricciones (planos). Elegir la abstracción correcta de entrada evita pelearle al motor en cada hito siguiente.

**Refs canónicas:**
- "Real-Time Collision Detection" (Christer Ericson, 2005) cap 5 — planes y intersection tests.
- TrenchBroom source: https://github.com/TrenchBroom/TrenchBroom — `lib/vm/` para math, `common/src/Model/Brush*.cpp` para el algoritmo.
- Quake / Half-Life Hammer 4 — referencia histórica del workflow de mapping.

**Revisar si:**
- Emerge un caso de robustez numérica que el clipping a mano no resuelva (brushes con caras casi coplanares en posiciones extremas) → considerar manifold como helper interno.
- F2H12 (booleanos) revela que el clipping puro es insuficiente para operaciones complejas y la suite tipo BSP es necesaria → pivot al approach Quake/Hammer clásico (BSP tree).
- Lock-to-world UV (F2H14) resulta innecesario en la práctica del dev → relajar a UVs per-vertex y reconsiderar manifold.

## 2026-05-04: CSG booleanos destructivos via plane clipping (F2H12)

**Contexto:** segundo hito de sub-fase 2.2 (CSG). Decidir cómo modelar las operaciones booleanas Subtract / Union / Intersect entre brushes. Dos approaches dominantes:

1. **Destructivo** (Hammer / TrenchBroom / Quake): la op reemplaza los brushes input por el resultado. Commit definitivo, undoable via comando.
2. **No-destructivo / Modifier stack** (Blender Boolean modifier): los brushes input siguen vivos y editables; la op se "stackea" como modifier que recalcula la geometría cada frame.

**Decisión:** **destructivo**. Las ops `subtract` / `unionOp` / `intersectOp` devuelven `std::vector<Brush>` (cardinalidad 0-N), el editor reemplaza A y B por el resultado, y `BooleanOpCommand` captura snapshots `SavedBrush` para undo/redo.

**Razones:**

- **Workflow Hammer/TrenchBroom**: el dev viene del modelo "Quake-style mapping" (referencia explícita en F2H11). Ese flow es destructivo: arrastrás un brush B sobre A, "Subtract", aparecen los pedazos. Ctrl+Z restaura. Es el comportamiento mental que el user espera.
- **Simplicidad de implementación**: ~600 LOC + 27 tests vs estimado ~1500 LOC + state runtime para modifier stack (recalcular cada frame, manejar dependency graph entre brushes, gestión de cache, hot-reload del modifier al editar input).
- **Suficiente para v1**: ningún juego tipo Quake / Half-Life / CS usa modifier stacks. El user no pidió no-destructividad explícitamente.
- **Undoability via comando estándar**: el patrón `BooleanOpCommand` (con snapshots por tag, no por handle) reusa la infra del HistoryStack existente sin caso especial.

**Alternativas descartadas:**
- **Modifier stack tipo Blender**: ~2x más código, complejidad de ciclos / dependencias entre brushes (qué pasa si A se subtractea de B y B se subtractea de C — invalidación en cadena), no encaja con el workflow de mapping. Diferido a hito propio si emerge necesidad real (ej. el dev dice "necesito poder editar A después de hacer subtract").
- **Boolean ops via libs externas (manifold, Carve)**: ya descartado en F2H11 (decisión durable de "CSG a mano").

**Algoritmo: plane clipping puro a mano.**
- `subtract(A, B)`: half-space carving plano por plano. Mantener un `remainder` que arranca como A; para cada plano `P_i` de B, generar `frag_i = remainder ∪ {flip(P_i)}` y conservar si tiene volumen, después remplazar `remainder ← remainder ∪ {P_i}`. Los `frag_i` juntos forman `A \ B` como descomposición convexa. Edge case: planos coincidentes con caras de A se skipean.
- `intersectOp(A, B)`: brush con `A.faces ∪ B.faces` (dedup por kPlaneEpsilon), validado por `isBrushValid` (≥4 vertices únicos del brush via tripletes filtrados por inside-test).
- `unionOp(A, B)`: composición vía `(A subtract B) ∪ {B}` con casos especiales para contención total y disjunto.

Reusa todos los tipos puros de F2H11 (`Plane`, `Brush`, `BrushFace`, `intersectThreePlanes`, `signedDistance`, `kPlaneEpsilon=1e-4f`). Cero deps nuevas.

**Snapshot pattern para undoability:** `BooleanOpCommand` captura `SavedBrush` por valor (no `Entity` handles). Razón: tras execute/undo los handles cambian (entidades destruidas y recreadas), pero los snapshots permiten reconstruir desde tag/transform/faces. Mismo patrón que `CreateEntityCommand` de Hito 27. La función `recreateBrushEntity` interna duplica parte del código de `SceneLoader::applyEntitiesToScene` para no acoplar el comando al loader.

**Bug fix durable — centroide del resultado:** en la primera validación los brushes resultantes salían con `transform.position=(0,0,0)` mientras la geometría estaba en world space → el gizmo aparecía en el origen del mundo. Fix: `snapshotResultWorld` setea `position = AABB.center()` (centroide en world) y rebasea los planos a local space via `d_local = d_world + dot(n, centroid)`. **Lección durable**: cualquier op CSG futura que produzca brushes resultantes debe seguir esta convención (transform al centroide + planos en local) para que el gizmo y la edición posterior se sientan naturales.

**UX mínima en F2H12 — menú combobox:** `Archivo > Mapa > Boolean > {Subtract, Union, Intersect} > <brush B>` con submenu cascading listando los demás brushes del mapa como B. **NO multi-selección por Shift+click** todavía: requiere refactor del modelo de selección (Hierarchy + Viewport + Inspector + Gizmos) que es F2H15 del plan original. Promovido a hito siguiente porque sin multi-select el flow es awkward.

**Trade-offs:**
- Se pierde: capacidad de "ajustar A después de hacer subtract" sin reverter (modifier stack lo permitiría).
- Se gana: simplicidad, alineación con workflow Hammer, undoability limpia, suficiente para los próximos hitos de la sub-fase 2.2 (cilindros, UVs, face mode).

**Lección aprendida del scope:** el menú con combobox como B es subóptimo pero **funcional**. El user lo identificó al validar y propuso multi-select como mejora. Resistir el scope creep de "hagamos multi-select dentro de F2H12" — multi-select afecta Hierarchy, Viewport, Inspector, Gizmos, render outline; es claramente hito propio. F2H12 cierra con UX awkward y el siguiente hito hace el flow natural de Blender/Hammer.

**Revisar si:**
- El user pide explícitamente no-destructividad (workflow real lo demanda).
- Edge cases de robustez numérica emergen en uso real con brushes asimétricos / muchas caras.
- F2H13 (primitivas extendidas) revela que el algoritmo no escala a brushes con 32+ caras (cilindros) — profile y considerar BSP-style optimization.

## 2026-05-06: SelectionSet como modelo puro + isBrushValid robustecido (F2H13)

**Contexto:** tercer hito de sub-fase 2.2. **Promovido del F2H15 original** porque al validar F2H12 emergió que el flow del menú Boolean con combobox (heredado de selección singular) era awkward. La selección visual primero, ops booleanas después, es el flujo natural Hammer/Blender.

### Decisión 1 — `SelectionSet` como modelo puro testeable

**Problema:** la mayoría del editor (Inspector, Gizmo, comandos del HistoryStack, Viewport overlays) dependía de `EditorUI::selectedEntity()` returnando un `Entity`. Refactorear todo eso a "operar sobre N entidades" sería un sprint propio y no es lo que F2H13 pide — solo necesitamos que **multi-click funcione** y que las ops booleanas escalen.

**Decisión:** `SelectionSet` con dos campos: `vector<Entity> selected` + `Entity active`. La `active` es la entidad "primaria" — la última clickeada, la que el Inspector muestra, la que el Gizmo afecta. Las demás `selected` son contexto adicional para ops batch (Boolean cascade, futuro multi-delete, etc.).

**Back-compat first:** `selectedEntity()` devuelve `m_selectionSet.active`. `setSelectedEntity(e)` hace `replaceWithSingle(set, e)`. Toda la base de código que asumía selección singular sigue funcionando. Esta API "fachada" desacopla los callsites del modelo interno y permite migrar a multi-edit gradual en hitos futuros.

**Header-only con helpers libres:** `editor/selection/SelectionSet.h` define `add`, `remove`, `toggle`, `replaceWithSingle`, `clear`, `contains` como funciones libres. Sin métodos en el struct → testeable como POD, sin acoplamiento a EnTT más allá del `Entity` opaque handle. Invariantes garantizados por los helpers (no por el struct).

**Invariantes durables** (verificados por tests):
- `selected.empty() ⇔ active == Entity{}`.
- `active != Entity{} ⇒ contains(set, active)`.
- `selected` sin duplicados (mismo handle aparece a lo sumo una vez).

**Política para `remove(set, e)` cuando `e` es la `active`:** el nuevo active es el ÚLTIMO elemento del set tras la remoción (o `Entity{}` si quedó vacío). "Último" = la mental model de "active = más recientemente clickeada de las que quedan".

### Decisión 2 — Click semantics estilo Blender

- **Plain click** → `replaceWithSingle(set, e)` (set queda con solo `e`).
- **Shift+click** → `toggle(set, e)` (si está → quitar; si no → agregar y `setActive`).
- **Ctrl+click** → `add(set, e)` (agrega si no estaba; siempre `setActive`).
- **Click en vacío** (sin modifier, no hit) → `clear(set)`.
- **Click en vacío** (con modifier) → no-op (preserva el set actual).

Aplica en Hierarchy panel (`ImGui::GetIO().KeyShift / KeyCtrl`) y Viewport picking (`SDL_GetKeyboardState`).

**Trade-off vs TrenchBroom:** TrenchBroom usa Ctrl+click para toggle (no Shift) en algunas builds. Elegimos Shift por alineación con Blender (más reciente, más usado por dev moderno). Si emerge fricción, agregar opción configurable.

### Decisión 3 — Boolean ops cascade con preserveB

**Subtract**: cascade real por A. Cada `A_i ≠ active` se reemplaza por sus pedazos `subtract(A_i, active)`. La `active` (B = "tool brush") se preserva. Ejemplo Hammer: agarras 5 cubos para hacer 5 huecos en una pared con la herramienta "tool".

**Union / Intersect**: requiere exactamente N=2 brushes. Razón: la operación consume **ambos** brushes (no preserva el "tool"), y cascadear con N>2 es semánticamente ambiguo (¿izquierda-asociativo? ¿qué hacer si una iteración devuelve N>1?). La cardinalidad del resultado puede ser ≥1, todos brushes nuevos. Cascade real de Union/Intersect = hito futuro si emerge necesidad concreta.

**Si N>2 con Union/Intersect**: warning en log + no-op (no aplica nada, no rompe estado).

### Decisión 4 — Outline diferenciado vía debug renderer (no shaders)

`EditorRenderPass.cpp` itera el set y dibuja 12 líneas (corners de AABB) con el debug renderer existente. Ventajas vs uniform de shader PBR:
- **Sin tocar shaders**: cero superficie de regresión.
- **Tunear visualmente sin rebuild de shaders**: cambiar colores y line width es 1 línea de C++.
- **Funciona uniforme** entre MeshRenderer (corners unitarios) y BrushComponent (corners de `bc.brush.localAabb`).

**Color choices durables:**
- `active` = `(1.0, 0.35, 0.0)` naranja saturado Blender.
- `selected` no-active = `(0.95, 0.95, 0.2)` amarillo claro.
- `glLineWidth` 2px → 3px global. Afecta también triggers OBB, drop highlights, navigation paths — todos ganan visibilidad.

**Trade-off**: el outline actual no respeta depth-test "ver detrás del objeto" tipo Blender silhouette. Si emerge, agregar pase con `glDepthFunc(GL_GREATER)` y color más tenue. Suficiente para v1.

### Decisión 5 — `isBrushValid` exige AABB no-degenerada (bug fix durable)

**Bug detectado en validación visual de F2H13**: `subtract(A, B)` con brushes disjuntos generaba múltiples "copias planas" de A en lugar de una sola copia 3D. Causa: el algoritmo de plane clipping iteraba sobre los planos de B y para cada uno donde A satisfacía el flipped half-space (ej. A está en `y ≤ 0.5` cuando flipped(+Y) dice "y ≤ 0.5"), generaba un fragment "remainder ∪ {flipped}". Si la restricción extra reducía el remainder a un cuadrilátero coplanar (sin volumen), `isBrushValid` lo aceptaba porque solo contaba "≥ 4 vertices únicos" — y un cuadrilátero plano tiene 4 vertices.

**Fix:** `isBrushValid` ahora exige adicionalmente que la AABB del brush tenga `size > kPlaneEpsilon` en los 3 ejes. Sin esto, brushes 2D (cuadriláteros, polígonos coplanares) pasan el check pero rompen `buildBrushMesh` (que asume volumen 3D para fan triangulation).

**Lección durable:** "vertices ≥ N" no es check suficiente para brush 3D. Cualquier futuro algoritmo que produzca brushes (más operaciones booleanas, primitivas con vertices casi coplanares, etc.) debe pasar por `isBrushValid` que ya garantiza volumen real.

### Decisión 6 — `uniqueResultTag` con tags reservados intra-batch

**Bug detectado**: cuando una op booleana generaba N brushes resultantes, todos recibían el mismo tag (`Brush_Union_01`) porque `uniqueResultTag` solo verificaba contra entidades **vivas** del scene. Los snapshots aún no creados como entidades no se contaban.

**Fix:** parámetro `const vector<string>& reserved` que el caller mantiene durante la generación de los snapshots. Cada llamada agrega el tag generado a `reserved` y lo pasa a la siguiente. Sin acoplar a estado global ni tabla de tags vivos.

**Patrón aplicable** a cualquier futuro batch que cree N entidades con tags auto-generados (duplicate, paste-multiple, etc.).

### Decisión 7 — Multi-edit del Inspector diferido

Cuando hay multi-selección, el Inspector muestra solo la `active` + un disclaimer "+N entidad(es) adicional(es) seleccionada(s) — solo se edita la activa". Multi-edit (editar property X en N entidades a la vez) requiere refactor de los comandos `EditPropertyCommand` que hoy capturan un `Entity` específico — no es trivial. Diferido a hito futuro si emerge necesidad concreta. El `DeleteEntityCommand` ya soporta multi-target desde Hito 27.

**Trade-offs:**
- Se pierde: editar transform de 5 brushes a la vez ("alinear todos al grid"); editar material de N props a la vez.
- Se gana: foco en lo que F2H13 prometió (selección visual + Boolean cascade) sin meterse en refactor de comandos. Multi-edit es claramente "feature siguiente" — el dev puede pedirlo si lo necesita.

**Revisar si:**
- El dev pide editar transform / material de N entidades a la vez (probable post-F2H17 cuando aparezcan más entidades en mapas grandes).
- Box-select / lasso-select del viewport emergen como necesidad real (probablemente al manejar 50+ brushes).
- El SelectionSet escala mal a 1000+ entidades (improbable; el outline de 12000 líneas para 1000 brushes es ~negligible vs el cost del overlay 2D).

## 2026-05-06: Primitivas como datos puros + sphere=dodecaedro + cylinder=16 segments (F2H14)

**Contexto:** cuarto hito de sub-fase 2.2 (CSG). Era F2H13 en el plan original (primitivas extendidas), renumerado +1 por el adelanto de multi-selección como F2H13. F2H15-F2H17 también renumeran +1.

### Decisión 1 — Primitivas como funciones libres, no clases

**Problema:** decidir cómo representar 5 tipos de primitivas (cilindro, prisma, esfera, pirámide, wedge) en el subsystem CSG. Approach OOP tradicional sería: clase abstracta `Primitive` con métodos `toBrush()`, `getDefaultParams()`, etc. + 5 subclases.

**Decisión:** **funciones libres `make*Brush(matrix, params...)`** que devuelven un `Csg::Brush` standalone. Sin herencia, sin polimorfismo, sin enum runtime "PrimitiveType".

**Razones:**

- **Coherencia con F2H11**: `makeBoxBrush` ya existía como función libre. Las nuevas primitivas siguen exactamente el mismo patrón.
- **Una vez creado, una primitiva es solo "un brush más"**: render, persistencia, ops booleanas, gizmo, picking — todo opera sobre `Csg::Brush` sin saber qué primitiva era originalmente. **No hay estado runtime de "esto es un cilindro"** después de la creación.
- **Cero schema bump del `.moodmap`**: como las primitivas no tienen identidad post-creación, el formato v10 (que persiste `Brush` genéricos como arrays de planos) sirve sin cambios. Nuevas primitivas en hitos futuros tampoco van a requerir schema bumps.
- **Escala trivialmente**: agregar cono / cápsula / torus poliédrico = 1 función nueva. Sin tocar el dispatch de render, persistencia, picking, etc.
- **Tests más limpios**: cada primitiva = 1 archivo de test puro, sin mocks de jerarquía de clases.

**Trade-off:** se pierde la capacidad de "editar parámetros" después del spawn (cambiar segments=16 a 32 en un cilindro existente). En CSG con planos esto requeriría regenerar el brush completo; el approach actual delega a "edición de planos individuales en F2H16 (face mode)" o "delete + spawn nueva con otros parámetros". Aceptable.

**Patrón aplicable** a futuros generadores: primitives de F2H14, primitivas de mapa-entity de F2H17 (lights, triggers visuales), etc.

### Decisión 2 — Sphere como dodecaedro inscripto (12 caras), no UV-sphere

**Decisión:** `makeSphereBrush` devuelve un dodecaedro regular inscripto en esfera de radio 0.5 — 12 caras pentagonales planas.

**Razones:**

- **Convexidad por construcción**: el dodecaedro es convexo, encaja directo con el approach brush implícito. Una UV-sphere (típica de gráficos) NO es convexa por ser una mesh triangulada con N×M tris.
- **Mismo enfoque que TrenchBroom**: la "sphere" de TrenchBroom también es poliédrica (~ icosaedro). Es lo que el dev histórico de Hammer/Quake espera ver.
- **12 caras es suficiente para uso típico**: detail props, columnas redondeadas, etc. Si se necesita más resolución, hito futuro agrega `makeIcosphereBrush(subdivisions=1)` con 80 caras (1 nivel de subdivisión del icosaedro).
- **Boolean ops escalables**: `subtract(box, sphere)` con 12 caras es submilisegundo. Una sphere de 32+ caras (alta resolución) sería ~10x más lenta — diferido hasta que emerja el caso de uso.

**Geometría**: las normales son las 12 direcciones canónicas del icosaedro dual `(0, ±a, ±b)`, `(±a, ±b, 0)`, `(±b, 0, ±a)` con `a = 1/√(1+φ²)`, `b = φ/√(1+φ²)`, `φ` = razón áurea. Distance fija a `-0.5` para inscribir en unit sphere de radio 0.5.

### Decisión 3 — Cylinder default 16 segments

**Decisión:** `makeCylinderBrush` con default `segments=16`. Permite override pero el editor no lo expone (UI fixed defaults en F2H14, params dinámicos = hito futuro).

**Razones:**

- **Matchea TrenchBroom default**.
- **Visualmente "redondo enough"** para mapping FPS / exploration sin caer en facetado obvio.
- **Performance aceptable**: cylinder de 16 segments = 18 caras (16 lat + 2 caps). Subtract contra otro brush de 18 caras es ~324 ops del algoritmo plane clipping = submilisegundo en Debug build.
- **Consistencia entre cylinder y prisma**: prism triangular = cylinder con 3 segments; prism hexagonal = cylinder con 6. Mismo helper `buildPrismaticBrush` interno → cero duplicación.

**Trade-off:** un cilindro de 16 segments tiene un perímetro discreto, no curva real. En distancias cercanas al jugador esto puede ser visible. Si emerge feedback visual, agregar variante `makeCylinderBrush(matrix, 32)` es trivial — el algoritmo ya lo soporta.

### Decisión 4 — Wedge canónico con base cuadrada

**Decisión:** `makeWedgeBrush` produce una rampa con base cuadrada `[-0.5, +0.5]^2` en X-Z, altura máxima en `z=-0.5` (atrás) y altura cero en `z=+0.5` (adelante). 5 planos: `+X`, `-X`, `-Y` (base), `-Z` (atrás), e inclinado `(0, sqrt(2)/2, sqrt(2)/2)`.

**Razones:**

- **Forma canónica de Hammer**: el "wedge" o "ramp" en Quake es exactamente esta forma — un prisma triangular acostado.
- **Útil sin booleanos**: escaleras (1 wedge), rampas (1 wedge), techos inclinados (1 wedge rotado). Las alternativas serían "subtract de un box con un wedge invisible" — más cara computacional y conceptualmente.
- **Plano inclinado con normal `(0, +y, +z)`**: hacia "arriba-adelante", consistente con la convención "rampa que sube hacia atrás".

**Trade-off:** un wedge "no canónico" (ej. base triangular en lugar de cuadrada) requiere `makePrismBrush(matrix, 3)` rotado. Aceptable — el set de primitivas cubre la geometría común de mapping; casos exóticos van por boolean ops.

### Decisión 5 — Pyramid cuadrada (4+1 caras), no triangular ni hexagonal

**Decisión:** `makePyramidBrush` es **siempre** pirámide cuadrada (base 4 vertices, 4 caras laterales convergentes a la cima + 1 cap base). No hay parámetro `sides`.

**Razones:**

- **Mental model "pirámide" = cuadrada (egipcia)**: el dev no espera "pirámide hexagonal". Si emerge necesidad, `makeConeBrush(matrix, sides=4..N)` lo cubre.
- **Las 4 caras laterales tienen geometría exacta computable a mano**: normales `(±lx, ly, 0)` y `(0, ly, ±lx)` con `lx = 1/√1.25`, `ly = 0.5/√1.25`.
- **Distinción semántica con cylinder**: cilindro/prisma tienen N caras laterales paralelas al eje Y. Pirámide tiene N caras laterales **convergentes** a la cima.

**Trade-off:** `makeConeBrush` (variante con cima en lugar de cap top) sería una primitiva nueva — pendiente como hito futuro si emerge necesidad.

### Decisión 6 — Bug fix durable: gizmo rotate/scale para BrushComponent

**Bug detectado en validación visual:** el gizmo (EditorOverlay) solo permitía Translate sobre brushes. Causa: el filtro `selected.hasComponent<MeshRendererComponent>()` decidía si mostrar rotate/scale; brushes sin MeshRenderer caían a translate-only.

**Fix durable:** el filtro ahora chequea **`MeshRendererComponent || BrushComponent`** (renombrado a `hasGeometry` para claridad). Mismo cambio en `InspectorPanel::showRotScale`. **Lección durable:** cualquier nueva forma de "geometría visible" en hitos futuros (ej. `MapEntityComponent` para lights/triggers visuales en F2H17) debe extender este filtro o emergerá el mismo bug.

**Patrón aplicable:** centralizar el chequeo de "tiene geometría" en un helper `bool hasGeometryFor(const Entity&)` cuando aparezca el 3er tipo de geometría (probable F2H17 con map entities).

### Decisión 7 — Educar al user sobre Hammer vs Blender, no implementar Modifier Stack

**Contexto:** durante validación visual de F2H14 emergió frustración del dev al ver Union de 2 prismas con overlap parcial → ~10 piezas convexas, no "una sola forma fusionada". El dev expresó "no es como blender, o yo estoy mal entendiendo algo".

**Decisión:** **mantener el approach destructivo Hammer** (decisión durable de F2H12). NO implementar Blender Modifier Stack en este hito ni adelantar F2H17 (compilación brush → mesh estática). Educar al dev sobre la diferencia fundamental.

**Razones:**

- **CSG con brushes convexos NO PUEDE representar formas cóncavas como un solo objeto**. La unión de 2 convexos overlapping parcial es matemáticamente cóncava → debe descomponerse en N convexos o ser una mesh triangulada (Blender approach).
- **El dev pidió Hammer-style explícitamente** al arrancar sub-fase 2.2 ("hagamos destructivo como hammer editor"). La frustración emergente es educacional, no un cambio de requirement.
- **F2H17 ya está planeado** y resuelve el issue: al guardar el mapa, todos los brushes se compilan a UNA SOLA mesh triangulada con vertex weld + caras internas culled. **Visualmente vas a ver una sola forma** post-F2H17.
- **Adelantar F2H17 ahora retrasa F2H15 (texturizado UV) y F2H16 (face mode)**, que son features más urgentes para el workflow básico de mapping.

**Trade-off:** el user va a seguir viendo pedazos al hacer Union/Intersect hasta F2H17. Aceptable — la mayoría del workflow es Subtract (hacer huecos), donde la descomposición no se nota tanto visualmente porque los pedazos son geométricamente coherentes.

**Lección durable**: cuando el approach matemático del motor diverge del mental model del user, **explicar la divergencia es la solución correcta** (no rebuild el approach). El path natural es "el plan original ya cubre esto en F2H17" — y eso vale más que un fix ad-hoc que rompe la coherencia destructiva.

**Revisar si:**
- Emergen primitivas con casos numéricos patológicos (vertices casi-coplanares en pirámide rotada extremo, sphere con caras casi-paralelas).
- F2H15 (UV editor) revela que las primitivas no-cuadradas tienen problemas específicos de texturizado (esperable: cilindros con 16 segments tendrían UV "stitching" en cada cara).
- El dev pide variantes (cono, cápsula, torus poliédrico, hemisphere) — todas son `make*Brush` nuevas sin tocar el core.

## 2026-05-06: UVs computed-not-stored + lock-to-world por-cara + UI global-en-brush diferida a face mode (F2H15)

**Contexto:** quinto hito de sub-fase 2.2 (CSG). Materializa el feature que justificó el approach brush implícito de F2H11 (vs manifold mesh-based): **lock-to-world UVs**.

### Decisión 1 — UVs computed-not-stored

**Decisión:** las UVs por vertex no se almacenan en `BrushFace`. Cada cara guarda los **parámetros** de UV (`uAxis`, `vAxis`, `uvOffset`, `uvScale`, `uvRotation`, `lockToWorld`); las UVs se computan en `buildBrushMesh` desde esos params para cada vertex.

**Razones:**
- **Cero vertex data extra**: una cara con 100 vertices no almacena 100 UVs duplicadas — solo los 6 params.
- **Edición instantánea**: cambiar `uvScale` no requiere recalcular vertex data — solo invalida el mesh cache. El SceneRenderer rebuildea en el siguiente frame.
- **Persistencia compacta**: el `.moodmap` v11 guarda 6 floats por cara, no N×2 floats por vertex.
- **Lock-to-world barato**: un solo flag por cara cambia el cómputo de proyección; sin lock-to-world, el código nunca evalúa `worldMatrix * pLocal`.

**Trade-off:** UVs requieren rebuild del mesh cuando cambian. Aceptable porque cualquier cambio de UV viene del Inspector (humano = no-realtime).

### Decisión 2 — `lockToWorld` como flag por-cara, no por brush

**Decisión:** `lockToWorld` es campo de `BrushFace`, no de `BrushComponent`.

**Razones:**
- **Granularidad necesaria**: en F2H17 (face mode) el dev va a poder activar lock-to-world solo en algunas caras (ej. piso con textura world-locked + paredes con textura local-locked).
- **Modelo escalable**: el cache `anyFaceLockToWorld` en `BrushComponent` es un summary computado, no la fuente de verdad.
- **UI global por brush en F2H15** sigue siendo trivial — el toggle aplica a todas las caras a la vez. Per-cara emerge naturalmente cuando hay selección de cara (F2H17).

**Trade-off:** un poco más de memoria por cara (1 byte). Negligible.

### Decisión 3 — Tangent basis canónico al construir, override por edición posterior

**Decisión:** las primitivas (`makeBoxBrush`, `makeCylinderBrush`, `makeSphereBrush`, `makePyramidBrush`, `makeWedgeBrush`, `makePrismBrush`) inicializan los UV params con `defaultTangentBasis(normal)` para `uAxis`/`vAxis`. Resto en defaults sensatos (offset 0, scale 1, rotation 0, lockToWorld false).

**Razones:**
- **UVs alineadas out-of-the-box**: el dev spawnea un cilindro y la textura ya se ve correctamente proyectada en cada cara — no requiere edición.
- **Reusable para subtract/union/intersect**: los brushes resultantes de booleans pueden usar `defaultTangentBasis` para sus caras nuevas.
- **Estable**: `defaultTangentBasis` depende solo de la normal, mismo input → mismo output.

**Algoritmo de `defaultTangentBasis`:**
```
helper = (|normal.y| > 0.9) ? (1, 0, 0) : (0, 1, 0)
uAxis = normalize(cross(helper, normal))
vAxis = normalize(cross(normal, uAxis))
```
Switching axis cuando la normal está cerca del eje Y evita cross product con magnitud cero.

**Trade-off:** cerca de la transición (normal con `|y|` ≈ 0.9) los `uAxis`/`vAxis` flippean, lo que puede causar UVs discontinuas entre caras adyacentes. Mitigación: tests con normales patológicas; si emerge en uso real, agregar smoothing o cachear tangent basis al construir el brush.

### Decisión 4 — UV editor en F2H15 = global por brush; per-cara real = F2H17 (Face Mode)

**Decisión:** el UV editor del Inspector aplica los sliders a TODAS las caras del brush a la vez en F2H15. Per-cara real (con selección visual de cara individual estilo Hammer) se difiere a hito propio.

**Contexto:** durante validación visual el dev preguntó "qué pasa con las UV por caras? no faltaba más?". Se le ofrecieron 3 opciones:
- **A**: cerrar F2H15 sin per-cara, F2H17 = face mode con selección visual.
- **B**: workaround sin face mode — dropdown "Cara 0..N" en Inspector + sliders editando solo esa cara por índice (~30 min trabajo).
- **C**: adelantar face mode a F2H16, postergar HistoryStack cleanup.

**Decisión del dev: A** ("lo quiero igual que el hammer, agregalo como hito propio"). Rechazó workarounds parciales — quiere la cosa real cuando llegue.

**Razones:**
- **Selección visual de cara** requiere infraestructura propia: raycast contra polígonos individuales (no contra AABB del brush), sub-modo "Face Mode" del editor (toggle estilo Blender), render outline distinto solo de la cara seleccionada, comandos undoable per-cara, posible multi-selección de caras.
- **Workaround dropdown sería desechable**: cuando llegue F2H17, el flow de "cara seleccionada por índice" se reemplaza completo. El dev prefiere esperar a tener la UX final.
- **F2H15 entrega los cimientos**: estructura `BrushFace` per-cara + cómputo per-cara + persistencia per-cara. Ya está listo para que F2H17 solo agregue el UI visual.

**Trade-off:** durante F2H15-F2H16, los UV params se editan globalmente por brush. Aceptable como UX intermedia.

### Decisión 5 — Schema bump `.moodmap` v10 → v11 con back-compat aditiva + recompute de tangent basis

**Decisión:** v11 agrega 6 campos opcionales a cada `face` del JSON (`uAxis`, `vAxis`, `uvOffset`, `uvScale`, `uvRotation`, `lockToWorld`). Faces v10 sin estos campos cargan con defaults del struct. **El loader detecta si `uAxis/vAxis` vienen como defaults canónicos (+X/+Y) y los recomputa con `defaultTangentBasis`** desde la normal real.

**Razones:**
- **Back-compat aditiva sin migración**: faces v10 que NO tenían UV params cargan con tangent basis correcto (auto desde la normal), no con `+X/+Y` canónico que se vería mal en caras no-axis-aligned.
- **Idempotente**: si un mapa v11 se guardó con `uAxis=+X, vAxis=+Y` deliberadamente (caso raro pero posible), el loader lo recomputa al cargar — pero un re-save preserva los valores recomputados. Pequeña pérdida de info en ese caso edge; mitigación: el dev usa `defaultTangentBasis` consistentemente.

**Trade-off:** ambigüedad si el dev quiere intencionalmente `uAxis=+X, vAxis=+Y` distinto al tangent basis. Caso raro (y siempre resoluble editando los params de otro modo); aceptable.

### Decisión 6 — Bug fix durable: drop de textura/material detecta BrushComponent primero

**Bug detectado:** drop de textura/material sobre un brush en el viewport creaba un **tile-pared en el grid del suelo** en lugar de asignar al brush. Causa: `processViewportTextureDrop` y `processViewportMaterialDrop` no consideraban `BrushComponent` — solo MeshRenderer (material) o tile pick (texture).

**Fix durable:** ambos handlers chequean `BrushComponent` primero. Para texture drop además crea un material wrapper via `assets.createMaterialFromTexture(texId)` y lo asigna a `bc.material`. Solo cae al flow legacy si el cursor no está sobre un brush.

**Lección durable:** cualquier futuro tipo de "geometría visible" (F2H17 face entities, F2H18 compiled meshes) debe extender estos handlers o el bug emerge de nuevo. Mismo patrón que el `hasGeometry` de F2H14 para gizmo rotate/scale.

### Decisión 7 — Bug ABI mismatch en C++: clean rebuild requerido al cambiar size de struct persistido

**Bug detectado:** al extender `SavedBrushFace` con los 6 campos UV (de ~16B a ~72B), una unidad de compilación que crea/copia el struct usaba el layout viejo. `push_back` en el `vector<SavedBrushFace>` agregaba más de 1 elemento por iteración (terminando en 20 faces para una box de 6). Build incremental no recompiló todo el código que tocaba el header.

**Resolución:** `cmake --build ... --clean-first` forzó recompilación completa.

**Lección durable:** cualquier cambio de tamaño en structs persistidos (`SavedBrushFace`, `SavedEntity`, etc.) o públicos en headers ampliamente incluidos requiere clean rebuild para evitar corrupción de memoria. Documentar como step de validación: "tras agregar/cambiar campos a structs en headers públicos, clean rebuild de la suite + lanzar editor y verificar persistencia básica antes de validar la nueva feature".

**Trade-off:** clean rebuild toma 5-10 min vs incremental ~30s. Aceptable como step ocasional cuando se cambian structs.

**Revisar si:**
- El dev empieza a usar lock-to-world masivamente y el rebuild-on-transform-change pega en performance (probable solo con 50+ brushes lock-to-world editados al mismo tiempo).
- F2H17 (face mode) revela que la API "todo per-brush" del UV editor de F2H15 confunde al dev — refactorear UI a "cara seleccionada activa".
- Schema v11 tiene back-compat issues reportados por el dev (proyectos viejos cargan con UVs raras).

## 2026-05-06: HistoryStack Blender-style — wireup, no refactor (F2H16)

**Contexto:** sexto hito de sub-fase 2.2. Hito intermedio de "limpieza de deudas" entre F2H15 (UV editor) y F2H17 (Face Mode estilo Hammer). Insertado en el roadmap por feedback del dev: "importé cilindro, lo escalé, lo elevé, le puse 1ra textura, 2da textura, arrastré textura al suelo creando cubo, Ctrl+Z me devolvió al cilindro antes de elevarlo".

### Decisión 1 — Mantener command pattern, NO refactor a snapshot pattern

**Contexto:** Blender internamente usa snapshot pattern (memcpy del estado relevante tras cada "operator"). MoodEngine desde Hito 27 usa command pattern (`ICommand` con `execute()/undo()`). El dev preguntó "como lo hace blender? me gusta el sistema, podemos copiarlo" — pregunta legítima.

**Decisión:** mantener command pattern. NO refactor a snapshot.

**Razones:**

- **Behaviour observable es idéntico**: lo que el user **ve** en Blender (drag de slider = 1 step, "Last Operator" en UI, Ctrl+Z granular por intención) es 100% reproducible con command pattern. La diferencia es interna.
- **Refactor a snapshot pattern es sprint propio sin valor agregado real**: requeriría serializar todo el state del scene/brushes a memoria/disco tras cada acción, escalable mal con el motor actual.
- **Command pattern de MoodEngine ya es testeable y robusto**: el Hito 27 + 32 ya estableció el patrón con `pushEditIfDone`, snapshots por tag (no handles), invariantes tested en `test_history_stack.cpp`.
- **F6 "tweak last operator" de Blender** (parametrizar el último operator post-hoc) sería scope grande propio. Diferido si emerge.

**Trade-off:** los devs C++ que vienen de Blender pueden esperar snapshot pattern al leer el código. Mitigación: documentar en `Command.h` que el patrón es "Blender-style en behaviour, command-pattern en implementación".

### Decisión 2 — Auditoría exhaustiva como Bloque B

**Contexto:** la deuda no era 1 bug — eran ~8 deudas acumuladas desde Hito 5 hasta F2H15. Implementar sin auditoría llevaría a "fix this fix that" cíclico cuando emerjan más casos.

**Decisión:** delegar Bloque B completo a un subagente con prompt específico ("audit MoodEngine editor for mutations of Scene/GridMap/BrushComponent/MeshRenderer that DON'T push commands"). Output: lista concreta con file:line + categoría + comando propuesto.

**Razones:**

- **Cobertura sistémica**: el subagente buscó por `setTile`, `bc.material =`, `mr.materials`, etc. Pillar todo de una vez evita el bug de "fix uno, descubrir tres más".
- **Documentado en plan**: la lista vive en `PLAN_HITO_F2H16.md` y queda como referencia. Si emerge un futuro caso similar, se compara contra esa lista.
- **Patrón aplicable**: cualquier futuro hito de "deudas técnicas" puede usar el mismo approach (auditoría con subagente + lista concreta + plan + ejecutar).

### Decisión 3 — Captura por tag, no por handle EnTT

**Decisión:** `EditBrushMaterialCommand`, `EditBrushUVCommand`, `EditMeshRendererMaterialCommand` capturan `std::string entityTag` y buscan la entidad por tag en `execute()/undo()`. NO capturan `Entity` handle.

**Razones:**

- **Robustez ante delete/recreate del HistoryStack**: si un comando previo fue `DeleteEntityCommand → undo → recreate`, el handle EnTT cambió. Capturar por tag sobrevive a esto. Mismo patrón que `BooleanOpCommand` de F2H12.
- **Tags son estables**: garantizado por convenciones del editor (Brush_Box_NN, etc.) + UI que evita duplicados de tag al spawnear.
- **Costo: O(N) lookup** por aplicación del comando. Negligible para ~100 entidades típicas.

**Trade-off:** si dos entidades tienen el mismo tag (por bug anterior a F2H8), el comando opera sobre la primera encontrada. Aceptable porque el bug ya fue arreglado y la convención de "tag único" es invariante del editor.

### Decisión 4 — Granularidad por intención del user (drag = 1 command)

**Decisión:** los sliders del UV editor pushean **1 comando al soltar** (`IsItemDeactivatedAfterEdit`), no por cada frame del drag. El checkbox lockToWorld es push instantáneo.

**Razones:**

- **Mental model del user**: el user no piensa "moví el slider 100 px"; piensa "cambié el scale de 1 a 3". Granularidad por intención.
- **Stack legible**: 1 acción = 1 entrada en el undo stack. Sin spam de "Editar UV scale" × 100.
- **Patrón ya existing**: `pushEditIfDone` del Hito 32. F2H16 extiende a UV editor con helper local `captureSnapshotIfActivated()` + `pushCommandIfChanged(label)`.
- **`snapshotsEqual` evita ruido**: si el user clickea un slider pero no lo mueve, snapshot pre = snapshot post → no se pushea comando. Sin entradas vacías en el stack.

### Decisión 5 — StatusBar Blender-style "Último: <name>" sin timeout

**Decisión:** la statusbar muestra "Último: <command name>" persistente, refrescado cada frame leyendo `historyStack->undoName()`. Sin timeout (no se borra después de N segundos).

**Razones:**

- **Info útil constante**: el user siempre quiere saber qué Ctrl+Z va a deshacer, no solo en los 5 segundos post-acción.
- **Implementación trivial**: un getter ya existente (`undoName()` del Hito 27) + un setter en `StatusBar` + sincronización en `EditorUI::draw`. ~10 LOC.
- **No bloquea otros mensajes**: la statusbar tiene FPS, modo, message libre — el "Último: ..." va al final con su propio Separator. Cero conflicto.

**Trade-off:** si la barra se llena visualmente con un command de nombre muy largo, puede empujar otros elementos. Mitigación: nombres concisos en convención (ej. "Editar UV scale" en lugar de "Modificar el parametro UV scale del BrushComponent del brush activo").

### Decisión 6 — `snapshotsEqual` con tolerancia kPlaneEpsilon

**Decisión:** la función `snapshotsEqual(a, b)` compara los UV params componente a componente con tolerancia `kPlaneEpsilon = 1e-4f`.

**Razones:**

- **Float exact equality es frágil**: un drag de slider que termina en el mismo valor numérico puede tener un epsilon de diferencia por float math.
- **Reusa la tolerancia ya estandarizada**: `kPlaneEpsilon` de F2H11 es la tolerancia geométrica del motor. Mismo orden de magnitud para UVs.
- **No spam**: con tolerancia bit-exact, cualquier wiggle del slider creaba commands ruidosos.

### Decisión 7 — Diferir tweak-last-operator (F6 de Blender)

**Decisión:** NO implementar "F6 panel" de Blender (ajustar params del último operator post-hoc).

**Razones:**

- **Scope grande propio**: requiere parametrizar cada comando con sus params editables, panel UI dedicado, integración con el statusbar.
- **No urgente**: el user pidió "como hace Blender" pero no pidió F6 explícitamente. El behaviour de drag = 1 command + Ctrl+Z granular ya cubre 95% del flow Blender.
- **Diferido a hito propio si emerge**: en el plan post-Fase 2.2, posiblemente como UX polish.

**Revisar si:**
- El user pide F6 explícitamente al usar el editor en flow real.
- El statusbar "Último: ..." se llena demasiado visualmente con comandos largos — limitar a N caracteres con ellipsis.
- Emergen NUEVOS handlers que mutan state sin command (probable en F2H17 face mode con material per-cara) — auditar en Bloque B de cada hito posterior.

## 2026-05-06: Face Mode estilo Hammer + multi-material via slots (F2H17)

**Contexto:** F2H15 cerró UV editor con sliders aplicados a TODAS las caras del brush. El dev pidió desde el día uno que la edición fuera per-cara real — "lo quiero igual que el hammer" — y rechazó workarounds parciales (dropdown intermedio, multi-edit aproximado). F2H17 materializa eso como sub-modo del editor con selección visual de cara individual + material distinto por cara.

### Decisión 1 — Sub-modo Face con tecla 3 (Blender convention)

**Decisión:** sub-modo del editor toggle con tecla **3**. Reservar `1` (vertex) y `2` (edge) sin implementar todavía. `Esc` o `3` otra vez vuelve a Object Mode.

**Razones:**
- **El dev ya conoce la convención** (usa Blender). Usar 3 para face es la elección obvia y reduce fricción cognitiva.
- **No reinventar**: imitar Blender en lo que ya funciona. Hammer usa toolbox flotante con botones — feo y obsoleto.
- **Reservar 1/2 ahora** = no romper el muscle memory si vertex/edge mode emergen después (mapping FPS rara vez los necesita, pero está la opción abierta).

**Alternativas descartadas:**
- Tecla `Tab` (Blender real): conflictua con el cycle de focus de ImGui en el editor.
- Botón en la toolbar / menu: el dev pidió teclado-first explícitamente en F2H13.
- Modal popup "elegir modo": rompe el flow rapid-fire del mapping.

**Revisar si:**
- Vertex / Edge mode emergen como necesidad real (improbable para FPS mapping).

### Decisión 2 — Multi-material via slots (no MaterialAssetId per-cara directo)

**Decisión:** `BrushComponent.materials: vector<MaterialAssetId>` (slots indexados por `face.materialIndex`). `BrushFace.materialIndex` ya existía desde F2H11 pero no se usaba — F2H17 lo activa apuntando al vector de slots.

**Razones:**
- **Mismo patrón que `MeshRendererComponent`**: la base de código ya conoce este modelo. `MeshRenderer.materials[]` con `submesh.materialIndex` indexando el array.
- **Dedup natural**: dos caras pueden compartir el mismo MaterialAssetId sin duplicación. Slot 0 = "default del brush"; slots 1+ se agregan al asignar material distinto a una cara específica.
- **Multi-material rendering eficiente**: `buildBrushMesh` agrupa caras por slot y produce 1 submesh por slot. SceneRenderer hace 1 draw call por slot — escala bien si el dev usa pocos materiales distintos por brush (caso común).
- **Schema bump v11→v12 aditivo**: `materialPaths` array nuevo en JSON; v11 con `material` singular se sintetiza como `materials = [material]`. Mapas viejos cargan visualmente idénticos.

**Alternativas descartadas:**
- `MaterialAssetId per-face` directo (1 ID por cara): menos dedup, peor render perf (1 draw call per face en peor caso), refactor más invasivo del schema.
- Mantener material global del brush + override per-cara opcional: dos paths de código diferentes en render, peor de ambos mundos.

**Revisar si:**
- N>16 slots por brush (improbable: típicamente brushes tienen ≤4 materiales distintos). Si pasa, considerar agrupar materiales similares por shader.

### Decisión 3 — `BrushComponent` move-only (`unique_ptr<IMesh>` per slot)

**Decisión:** `BrushComponent.meshCache` cambia de `unique_ptr<IMesh>` (singular) a `vector<unique_ptr<IMesh>>`. `BrushComponent` se vuelve move-only (copy ctor `=delete`).

**Razones:**
- **1 mesh GPU por slot** — alineado con la decisión 2 (multi-material). El SceneRenderer itera el vector y bindea el material correspondiente antes de cada draw.
- **Move-only forzado por `unique_ptr`**: no se puede hacer copy del componente. Migración a `addComponent<BrushComponent>(std::move(bc))` en ~12 callsites — molesto pero refleja el modelo real (un BrushComponent es ownership de meshes runtime, no debería duplicarse).
- **Catch errors at compile time**: el `=delete` explícito hace que cualquier copia silenciosa (por ej. en lambda capture) falle la compilación con error claro.

**Alternativas descartadas:**
- `vector<shared_ptr<IMesh>>`: copy-OK pero overhead refcount + ownership semánticamente ambigua. El BrushComponent ES el dueño, nadie más.
- Mesh interleaved única con offset/count per slot (1 sola GPU buffer con sub-rangos): complica el rebuild parcial cuando cambia 1 cara, sin ganancia de perf real.

**Revisar si:**
- El refactor de move-only causa fricción en algún flow nuevo. Probablemente no — el patrón está bien establecido en EnTT.

### Decisión 4 — Picking de cara con back-face culling (regla `dot > 0`)

**Decisión:** `Csg::pickFace` filtra triángulos con `dot(worldNormal, rayDir) > 0` antes de Möller-Trumbore. Solo las caras de espalda al ray (no apuntan hacia la cámara) son skip — equivalente a back-face culling de render.

**Razones:**
- **Bug real en validación**: sin esto, click en una cara seleccionaba **la opuesta**. Razón: ambas caras paralelas (face front + back) tienen sus polígonos triangulados en el plano del brush; el ray entra por la cara visible y sale por la opuesta — Möller-Trumbore intersecta ambos triángulos y el "más cercano" puede ser la opuesta dependiendo del orden de iteración.
- **Match con render**: las caras visibles son las que apuntan hacia la cámara (back-face culling activo en el shader). Picking en world space con la misma regla mantiene WYSIWYG.
- **Cero falsos negativos**: si una cara está visible para la cámara, su `worldNormal` apunta hacia la cámara → `dot(worldNormal, -rayDir) > 0` → `dot(worldNormal, rayDir) < 0` → NO se filtra. Coincide con la convención de OpenGL.

**Alternativas descartadas:**
- "Más cercano" sin filtrar: bug confirmado por validación.
- `glReadPixels` sobre un buffer de IDs (color picking): añade pase de render dedicado, complica el pipeline para 1 feature.

**Revisar si:**
- El dev rota la cámara dentro del brush (dentro de un cuarto sólido cerrado): las normales apuntarían hacia adentro y el filtro invertiría su efecto. Para mapping eso no pasa (siempre cámara fuera de los brushes), pero documentar.

### Decisión 5 — Highlight visual: outline naranja + fill semi-transparente Half-Life

**Decisión:** outline naranja `(1.0, 0.5, 0.0)` siempre + fill `(1.0, 0.55, 0.10, 0.55)` con alpha blending + `glDepthFunc=GL_LEQUAL` + `glDepthMask=GL_FALSE`. Fill se oculta cuando `Inspector::isEditingBrushUV()=true`.

**Razones:**
- **Cyan tradicional "se vio muy pobre"** en validación con el dev (literal). Naranja Half-Life es saturado, distintivo, asociado a "selección activa" en convención FPS-mapping (Hammer original).
- **Fill semi-transparente, no sólido**: el dev lo necesita ver la textura debajo para poder editarla. Alpha 0.55 deja la textura visible pero comunica "este es el pivote".
- **`LEQUAL` + `depthMask=FALSE`**: el highlight se ve sobre la geometría sin Z-fighting, pero no escribe al depth buffer (no oculta lo que esté detrás).
- **Fill oculto en UV edit**: pedido directo del dev — "alternar entre mostrar toda la cara o no" estorbaba al editar UVs. Outline siempre porque es la confirmación visual de qué cara está activa.

**Alternativas descartadas:**
- Solo outline (sin fill): demasiado sutil cuando el brush es chico o está lejos.
- Fill sólido sin alpha: tapaba la textura, imposible editar UVs.
- Color cycling animado: efectista, distrae.

**Revisar si:**
- El dev cambia de tema visual del editor (claro/oscuro) y el naranja deja de contrastar — tema-aware highlight.

### Decisión 6 — Schema v12 back-compat aditivo + dual-write `material` legacy

**Decisión:** v12 escribe `materials` (array de paths) Y `material` (path del slot 0) en cada brush. Lectores v12 prefieren `materials` array; lectores v11 leen `material` singular y pierden la info de slots adicionales pero ven el brush con el slot 0 visualmente correcto.

**Razones:**
- **Forward compat**: si un mapa v12 cae en una build v11 antigua, no crashea ni se ve raro — la cara con slot 1+ pierde su material y cae al default, pero el brush completo sigue siendo válido.
- **Gradual migration**: el dev no necesita rewriteear sus mapas viejos. v11 abre, v12 escribe, conversión transparente.
- **Costo de bytes mínimo**: el campo legacy duplicado es 1 string corto adicional por brush. Cero impacto en performance de save/load.

**Alternativas descartadas:**
- Solo escribir `materials` (drop legacy): rompe la posibilidad de downgrade durante el desarrollo activo.
- Versión rama (v11.5 con feature flag): complica el reader sin beneficio real.

**Revisar si:**
- El dev confirma que el flujo dev-vs-prod no requiere downgrade (estamos en proyecto solo). En ese caso la próxima versión puede dropear `material` legacy.

### Decisión 7 — Cubo `brick.png` removido del mapa nuevo

**Decisión:** `EditorScene::createDefaultMap` ya no spawnea un cubo con textura `brick.png` central. `arena_16x16` arranca completamente vacío.

**Razones:**
- **Pedido directo del dev**: "el cubo que siempre aparece con textura brick.png del mapa podes eliminarlo, me molesta". Distorsiona el flow de validación visual de cualquier feature CSG (siempre hay que mover/eliminar el cubo primero).
- **Simplicidad**: un mapa vacío es el blank slate correcto para mapping. El dev spawnea lo que quiere desde menu Brush.
- **Cero pérdida funcional**: el cubo era debug-leftover del Hito 4 (cuando solo había tiles). Ya no aplica.

**Revisar si:**
- Algún test o demo asume que el mapa default tiene contenido — verificar (tests pasaron 567/567 sin tocar nada, OK).

## 2026-05-06: Reorg de menús del editor — top-level Mapa + Brush (F2H18)

**Contexto:** el menú `Archivo > Mapa` mezclaba file ops del proyecto (Nuevo, Abrir mapa, Guardar como) con geometría (Añadir Brush ▶, Boolean ▶). Spawn de un brush exigía 4 clicks con jerarquía no obvia. Ítem 1 del backlog `PENDIENTES.md` post-F2H17. F2H18 lo cubre como hito propio antes que F2H19+ acumulen más items en `Archivo > Mapa`.

### Decisión 1 — Promover `Mapa` y `Brush` a top-level

**Decisión:** dos menús nuevos top-level. `Mapa` toma los 5 file ops del mapa actual; `Brush` toma `Añadir ▶` (7 primitivas) + `Boolean ▶` (subtract/union/intersect via `drawBooleanOpMenu`). `Archivo` queda solo con file ops del proyecto + Empaquetar + Nuevo Script + Guardar prefab + Salir.

**Razones:**
- **Flatten**: spawn de brush pasa de 4 clicks a 3. Geometría es la operación dominante en mapping; merece top-level.
- **Separación de dominios**: file ops del proyecto y file ops del mapa son cosas distintas. Mezclarlas en `Archivo` confunde porque "Guardar" guarda el proyecto, no el mapa, pero `Mapa > Guardar como` guarda el mapa.
- **Cero refactor funcional**: los `requestProjectAction(...)` calls quedan idénticos. Solo se reubica el `MenuItem` que dispara cada acción.
- **Habilitación condicional preservada**: ambos top-levels deshabilitados sin proyecto activo (mismo behavior que el submenu anterior).

**Alternativas descartadas:**
- Mantener todo bajo `Archivo > Mapa`: el ítem 1 de PENDIENTES era específicamente esa queja.
- Toolbar lateral con iconos (Hammer-style): scope propio mucho más grande; el dev pidió "mejora a futuro" pero F2H18 era el quick-win de reorg.
- Renombre `Brush → Geometría`: diferido. Hoy todo el mapping es CSG-brush; el día que entren shapes no-brush (mesh import procedural, etc.) se renombra.

**Revisar si:**
- Entran shapes no-brush al editor → renombre a `Geometría`.
- El dev pide atajos rápidos por keyboard (Ctrl+B = Box, etc.) — hito propio.

### Decisión 2 — Demos a submenu `Ayuda > Demos ▶`

**Decisión:** los 13 items "Agregar X demo" + `Stress test poligonos ▶` que vivían top-level en `Ayuda` se agruparon bajo submenu `Ayuda > Demos ▶`. `Ayuda` queda con solo "Acerca de" + el submenu Demos.

**Razones:**
- **Los demos no son ayuda al usuario**: nombre no engaña pero pollutea visualmente. Algo como "Agregar particulas de fuego demo" no es help-content; es validación rápida de features.
- **Tampoco merecen top-level**: son secundarios al flow de mapping serio. Quien usa el editor en producción rara vez los toca.
- **Submenu de Ayuda**: agrupación obvia para "cosas raras del editor que no son del flow principal". Si emergen más, caben acá sin ensanchar la barra.
- **Stress test dentro del mismo Demos**: es un demo más (de poligonos masivos para benchmark) — no necesita su propio nivel.

**Alternativas descartadas:**
- Top-level `Demos`: demasiada visibilidad para algo de uso ocasional.
- Quitar los demos del editor: el dev los usa para validar regresiones rápidas. Mantenerlos accesibles, solo escondidos.
- Panel separado: ortogonal y diferido — los demos hoy son menu items rápidos, no necesitan UI dedicada.

**Revisar si:**
- Los demos crecen a >25 items → considerar agruparlos por categoría dentro de `Demos ▶` (Audio / Gameplay / Render / Stress).

### Decisión 3 — Sin tests nuevos

**Decisión:** F2H18 no agrega tests. Validación es 100% visual con el editor.

**Razones:**
- **El menú es UI puro de ImGui**: cero lógica testeable. Los `requestProjectAction(...)` ya están cubiertos por los tests del dispatcher en `EditorApplication`.
- **Mock de ImGui sería overkill**: testear que `BeginMenu("Mapa")` se llama antes que `EndMenu()` no agrega valor real.
- **Suite confirma cero regresión**: 567/8182 verde con el código nuevo — los tests existentes ya son la red de seguridad de que ningún `requestProjectAction` cambió de signature.

**Revisar si:**
- Aparecen tests de UI snapshot (raros en C++ con ImGui). Improbable.

## 2026-05-07: HistoryStack residual — auditoría con subagente + 2 comandos nuevos (F2H19)

**Contexto:** F2H17 introdujo multi-material rendering con `BrushComponent.materials` vector y drops que distinguen Object Mode (slot 0) vs Face Mode (slot existente o nuevo). El path Face Mode quedó **sin push al HistoryStack**: solo el path Object Mode estaba cubierto por `EditBrushMaterialCommand` (hard-coded a slot 0). Más una deuda pre-F2H17: drop de `.lua` sobre entidad mutaba `ScriptComponent.path` o agregaba el componente sin command. Item 2 del backlog `PENDIENTES.md` post-F2H18.

### Decisión 1 — Auditoría con subagente antes del scope

**Decisión:** Bloque B del hito = subagente (`general-purpose`) recorre `src/editor/` buscando mutaciones sin push. Scope cerrado tras la auditoría, no antes.

**Razones:**
- **Predecir desde el plan inicial subestima**: F2H16 anticipó "2-3 deudas", encontró 8. F2H19 anticipó "regresiones de F2H17", confirmó 2 ALTA + descubrió 1 MEDIA pre-F2H17 que no estaba en el radar.
- **El subagente lee código real, no asume**: verifica que `EditBrushMaterialCommand` está hard-coded a slot 0 (comentario explícito en el .cpp), que el UV editor del Inspector sí pushea correctamente, etc. Sin el subagente uno predice más por miedo o duplica trabajo.
- **Reporte estructurado por archivo:línea + prio**: facilita decidir scope (ALTA / MEDIA → entran; BAJA → confirmar con dev).
- **Misma técnica usada en F2H16 con éxito** — patrón validado.

**Alternativas descartadas:**
- Yo recorrer manualmente: aprox 30+ archivos del editor; el contexto principal se llena de file reads sin valor de razonamiento.
- Predecir desde memoria: los detalles concretos (archivo:línea) se pierden y los pendientes derivan a hipótesis.

**Revisar si:**
- El subagente reporta deudas falsas (sobre-flagging). En este hito 0 falsos positivos — los 5 hallazgos eran todos válidos, solo discrepamos en prio.

### Decisión 2 — `EditBrushFaceMaterialCommand` captura el vector `materials` completo

**Decisión:** el comando snapshotea `oldMaterials` y `newMaterials` (`std::vector<MaterialAssetId>` completos), no solo el slot afectado por el drop.

**Razones:**
- **El drop puede crear slot nuevo via `push_back`**: si `newMat` no estaba en `bc.materials`, el handler hace `materials.push_back(newMat)` y `face.materialIndex = nuevo_index`. Si el undo solo revirtiera `face.materialIndex` sin tocar el vector, quedaría un slot huérfano (material no usado por ninguna cara).
- **Tras varios undo/redo el vector divergiría**: cada execute agrega slot, cada undo no lo quita → `materials.size()` crece sin volverse atrás. Snapshot completo garantiza shape exacto en cualquier dirección.
- **Costo despreciable**: vector de N MaterialAssetId (u32) — N raramente > 4. Copia de 16 bytes en peor caso típico.
- **Robusto a casos edge**: dos drops del mismo material en caras distintas; un drop, undo, drop diferente, undo de nuevo, etc. Snapshot completo cubre todo.

**Alternativas descartadas:**
- Solo snapshot de `(faceIndex, oldFaceMatIndex, newFaceMatIndex)` + flag "did_push_back": agrega complejidad, multiple paths en undo según el flag. La captura completa del vector es estructuralmente más simple.
- Restar/agregar por delta sobre `materials`: requiere reaplicar reglas del handler en undo — duplica lógica.

**Revisar si:**
- N (slots por brush) supera ~32 con frecuencia → snapshot grande, considerar diff. Improbable: typical brushes tienen ≤4 materiales distintos.

### Decisión 3 — `EditScriptComponentCommand` con flag `hadComponent` y edge case de re-creación

**Decisión:** snapshot del comando incluye `bool hadComponent` que distingue 2 sub-casos en `undo()`:
- `!hadComponent` → undo remueve el componente (drop lo agregó).
- `hadComponent` → undo restaura `path` previo. Si alguien removió el componente entre execute y undo, lo recrea con `oldPath`.

**Razones:**
- **Drop de script tiene 2 semánticas distintas**: agregar componente nuevo vs reemplazar path. El undo correspondiente difiere fundamentalmente — sin el flag, el comando tendría que inferirlo y podría equivocarse si el state cambió externamente.
- **Edge case real**: el dev podría remover el ScriptComponent desde el Inspector (botón Remove) entre el drop y el Ctrl+Z. Si undo asume "componente está ahí" crashearía. Recrear con oldPath es la decisión menos sorprendente.
- **No snapshot de exposed props (`overrides`/`exposedProps`)**: el flow normal es "drop reemplaza el script entero"; las overrides del script anterior dejan de aplicar (son por nombre+default del script). Si el undo restaura el path viejo, las overrides se redescubren al re-cargar via mtime check.

**Alternativas descartadas:**
- Comando que infiera el flag desde state: frágil, distintos resultados según orden de operaciones.
- Reusar `EditPropertyCommand<std::string>` con setter custom: no cubre el caso "agregar componente si no existía" — distinto path en setter de get-or-add que en setter de assign.

**Revisar si:**
- El dev pide undo de Inspector edits sobre `overrides` (Hito 24 deuda) → comando dedicado `EditScriptOverrideCommand` con snapshot del map de overrides; ortogonal a este.

### Decisión 4 — Items BAJA fuera de scope (alineado con Blender/Unity)

**Decisión:** acciones del menú `Mapa` (Nuevo / Abrir / Guardar como / Set default / Eliminar) y toggles de modo/selección Face NO pushean al stack. Quedan fuera de F2H19.

**Razones:**
- **Convención de la industria**: Blender, Unity, Godot no hacen undo de "abrir mapa" ni de "cambiar de modo de edición" ni de "cambiar la selección". Son operaciones a nivel proyecto/UI, no edición del modelo.
- **Semantica del HistoryStack**: representa edits del scene (state persistido). Cargar otro mapa reemplaza el scene entero — el stack debería **resetearse** al cambiar de mapa, no acumular un "undo open map".
- **Seleccion**: Ctrl+Z después de hacer click en una cara debería deshacer la EDICIÓN previa, no la selección. Imitar Blender aquí.

**Alternativas descartadas:**
- Stack separado para selección/modo: complica la UX (qué Ctrl+Z deshace qué) sin valor real.

**Revisar si:**
- El dev pide explícitamente undo de "abrir mapa" o de selección. Improbable — el feedback ya validó que pasaron los 3 fixes core.





## 2026-05-07: F2H20 — compilación brush → mesh + export OBJ on-demand, sin persistencia ni runtime-load

**Contexto:** El plan original `PLAN_FASE2.md` entrada F2H14 ("Compilación brush → mesh optimizada") sugería: "al guardar el mapa, todos los brushes se compilan a una mesh estática unificada con caras internas eliminadas, vertices soldados, e índices generados. Esta mesh es lo que se renderiza en runtime. Brushes individuales solo existen en el editor." Tras el rerouting (Face Mode primero en F2H17, reorg menús F2H18, cleanup HistoryStack F2H19), F2H20 toma el ítem. Hay tres niveles de scope posibles:

1. **MVP**: helper puro `compileMap` + export OBJ + UI menu, sin tocar el formato `.moodmap` ni el runtime del MoodPlayer.
2. **Persistencia**: agregar `compiledMesh` al `.moodmap` JSON con schema bump (back-compat aditiva). Editor sigue usando brushes; runtime puede usar la mesh compilada si el campo está disponible.
3. **Two-paths**: editor carga brushes (edición), MoodPlayer carga **solo** la mesh compilada (sin código CSG en el runtime). Cumple el plan original al 100%.

**Decisión:** **Nivel 1 (MVP)** para F2H20. La compilación es una **vista derivada** del scene actual, accesible desde 2 menu items en `Mapa`:
- "Compilar mapa (stats)" → `pfd::message` con stats (brushes / faces totales / culled / triángulos / vertices pre-weld / unique / submeshes).
- "Exportar OBJ..." → `pfd::save_file` + `compileMap` + `writeObj` → `.obj` + `.mtl` lateral.

**Por qué MVP:**
- **Cumple el caso de uso real principal**: el dev quiere ver el resultado de cull/weld + exportar a Blender / MeshLab para iteración. Eso lo cubre el MVP completo.
- **Persistencia infla el `.moodmap`** sin beneficio inmediato (vertex data es ~50 bytes/vertex en JSON; un mapa típico de 200 brushes con 5K vertices = 250 KB extra). Si emerge necesidad de loading time en `MoodPlayer`, abrir un hito futuro con schema bump claro.
- **Two-paths duplica complejidad**: editor + player con lógicas distintas para leer el mismo formato; refactor del `SceneLoader`. Beneficio (dependency cero del runtime + load time mejorado) es real pero no urgente — los brushes se compilan rápido al cargar (`buildBrushMesh` per-brush; ~2-5 ms para mapas típicos en debug).
- **No two-paths en F2H20 mantiene la suite verde sin tocar code paths críticos**: el MoodPlayer no cambia, no hay risk de regresión.

**Cómo se implementa el MVP:**
- `engine/world/csg/CompileMap.{h,cpp}` (puro, testeable sin GL): tipos `BrushSource` / `CompiledVertex` / `CompiledSubmesh` / `CompiledMap` / `CompileStats`. Funciones `collectFaces` (por cada cara: polígono local via duplicación del helper privado de `BrushMesh.cpp`, ordenado CCW, transformado a world), `markInternalFaces` (pareja exhaustiva i<j buscando antiparalelos + `polygonsMatch` ignorando orden de vertices), `compileMap` (paso 1 descubre paths en orden, paso 2 triangula con builders paralelos + spatial hash celda eps en world). El weld matchea **position + UV + normal** — vertices coincidentes en posición pero con UV/normal distintas se mantienen separados (split estilo OBJ flat shading). El `BrushSource.materialPaths` lleva un path lógico por slot (resolución `MaterialAssetId → string` via `AssetManager::materialPathOf`); agrupación final por path lógico, no MaterialAssetId, para reproducibilidad entre sesiones.
- **Cull pareja-exacta, no overlap parcial**: dos caras se cullean solo si los polígonos coinciden vértice-a-vértice ±eps (orden libre — uno con CCW y el otro con CW visto desde la primera normal) + `dot(n_i, n_j) < -0.9999`. Cubre el caso típico (cubos pegados); overlap parcial (caras que comparten solo PARTE) requiere clipping general — diferido si emerge necesidad.
- **UV preservation respeta `lockToWorld`** igual que `buildBrushMesh`: si false, transforma vertex world → local con `inverse(worldMatrix)` antes del proyectado sobre `uAxis`/`vAxis`. Si true, la posición world es input al UV calc directamente.
- `engine/world/csg/MapExportObj.{h,cpp}`: `writeObj(compiled, path)` produce `.obj` (mtllib + o + v/vn/vt globales + bloques `usemtl` por submesh + caras `f a/a/a` con índices 1-based + offset acumulado entre submeshes) y `.mtl` lateral (1 newmtl por path distinto, material vacío `""` → `_default` con `Kd 0.8 0.8 0.8`). `sanitizeMtlName` reemplaza no-alfanumérico (excepto `_.-/`) por `_` (formato MTL no tolera espacios en `newmtl`).

**Suite resultante:** **602/8323** (+20 cases / +104 asserts vs F2H19). Tests cubren empty / 1 box (12 tris / 24 verts) / 2 separados misma material / materiales distintos (orden estable) / 2 pegados con cull (20 tris) / brush degenerado / `markInternalFaces` aislado en 3 escenarios / weld global / contenido del `.obj` con `mtllib`/`o`/`v`/`vn`/`vt`/`f`/`usemtl` correctos / indices 1-based con offset entre submeshes (`f 25/...` en el segundo).

**Validación visual del dev:** spawn 2 brushes → "Compilar mapa" mostró stats coherentes en dialog → "Exportar OBJ..." escribió a `Desktop/test/test.obj` (2 submeshes, 24 tris) → editor cerró limpiamente.

**Alternativas descartadas:**
- **Persistir la compilación en `.moodmap`** (nivel 2): infla el archivo sin beneficio inmediato. Schema bump v12 → v13 con back-compat aditiva era doable; la decisión fue no agregar ese peso hasta que emerja un caso de uso real.
- **Cargar la mesh compilada en `MoodPlayer`** (nivel 3): refactor del `SceneLoader` con dos branches (editor vs player). Riesgo de regresión + dos paths a mantener. Diferido.
- **Cull por overlap parcial via clipping general**: complejo, requiere intersección polígono-polígono. El caso simple cubre el 80% (cubos pegados); el resto puede esperar.
- **Schema versionado del OBJ con `MoodEngine F2H20` en el header**: ya está el comentario `# MoodEngine F2H20 — compiled map`. Si el dev necesita validación más estricta del export, agregar checksum o version explícita en hito futuro.

**Revisar si:**
- El dev reporta que la mesh exportada no corresponde visualmente (probable bug de UV o normal transform).
- El dev pide cargar la mesh compilada en runtime (abrir hito propio nivel 2 o 3).
- El cull de overlap parcial empieza a ser pedido en validación (mapas grandes con muchos brushes pegados parcialmente).

## 2026-05-07: F2H21 — Material Editor con preview esférico, scope MVP vs node-graph del plan F2H17 original

**Contexto:** El plan F2 original entrada F2H17 agrupaba "Material editor con node-graph (visual)" — node-graph + nodos básicos (TextureSample, ColorConstant, ScalarConstant, Multiply, Add, Mix, Output) + preview esférico + persistencia, todo en un solo hito. F2H18 original era el follow-up "Shader graph runtime compilation" (genera GLSL en runtime + cache por hash). Tras evaluar el costo:

- Node graph visual implementado a mano: ~500-700 LOC de UI ImGui.
- Compilación runtime del graph a GLSL + cache por hash: ~300-500 LOC.
- Refactor de `SceneRenderer` para shaders custom por material: cada graph distinto = shader distinto, rompe el batching de F2H4 + risk de regresión.

Total ~ 1-2 semanas de hito grande.

**Decisión:** **F2H21 entrega solo el componente de mayor valor inmediato — preview esférico off-screen — sin pagar el costo del node graph.** El dev ve sus cambios de tint / sliders / drop de texturas en una esfera dedicada en el mismo panel sin tener que asignar el material a una entidad del viewport. El node graph queda anotado en PENDIENTES.md como hito futuro si emerge necesidad real (probable F2H24+).

**Por qué MVP:**
- El preview esférico es ~80% del valor visual del plan original con ~20% del scope.
- Sin refactor del SceneRenderer: F2H21 solo agrega un componente lateral, riesgo de regresión casi cero.
- Reusa el shader PBR del repo en lugar de un shader custom: trade-off es setear los uniforms del Forward+ aunque no haya point lights (SSBOs vacíos con count=0).

**Implementación:**

- `engine/render/preview/MaterialPreviewRenderer.{h,cpp}` (~330 LOC):
  - FBO LDR 256x256 + depth.
  - Reusa `pbr.vert` + `pbr.frag` + `assets.primitiveSphereId()`.
  - Cámara fija frontal + rotación lenta automática del modelo sobre Y a ~22 deg/s — el plan original era cámara fija pero el dev al validar pidió rotación. Tiempo del clock monotónico, sin estado interno acumulado.
  - 1 directional light fija desde 3/4 + IBL inyectado del SceneRenderer (no duplica disk-load). Si IBL no disponible, cae a `uAmbient = 0.20` (subido del 0.05 original tras feedback "mitad oscura demasiado negra").
  - SSBOs vacíos para Forward+ bindings 2/3/4: el shader requiere los binds aunque uTilesX=1/uTilesY=1 deshabiliten el iter.
  - Sin shadow + bind dummy 2D al slot uShadowMap (algunos drivers validan sampler2DShadow aún en branches no tomados).

- `AssetManager::saveMaterial(MaterialAssetId id)` (~70 LOC):
  - Serializa al path lógico con el mismo schema que `loadMaterial` lee.
  - Solo escribe campos de textura cuando slot != 0 (preserva contrato del loader).
  - Rechaza ids fuera de rango y sentinels (`__default_material`, `__tex#<id>`, `__runtime#<id>`), VFS sin resolve, errores de I/O.

- `MaterialEditorPanel` reescrito con polish post-validación:
  - **Layout adaptativo**: >=540px → 2 columnas (controles izq | preview der); <540px → vertical (preview ARRIBA, controles abajo).
  - **Selección inicial del primer no-sentinel**: slot 0 magenta sigue accesible vía dropdown pero no es default.
  - **Texture slots con descubribilidad** (3 fixes que emergieron al validar):
    - Botón "X" reservando 28px a la derecha (antes -FLT_MIN cortaba el X fuera del panel).
    - Slots vacíos con label "(vacio - drop textura aquí)" (antes mostraban path "textures/missing.png" del fallback y confundían).
    - Tooltips + header "(?)" explicativo.
  - **Botón Guardar** con feedback verde/rojo durante 120 frames.
  - **Logs de tracking discretos** (`IsItemActivated` pre, `IsItemDeactivatedAfterEdit` log delta) — sin spam por frame. Mismo patrón F2H16 Inspector.

- `EditorApplication`: `m_materialPreview` creado post-`m_sceneRenderer` con IBL inyectado, destruido **antes** del SceneRenderer en el dtor (refs no-owning a textures que el scene manage).

**Suite resultante:** **607/8341** (+5 cases / +18 asserts en `test_material_serializer.cpp`). **Bug fix Windows-specific**: el test que leía el JSON resultante mantenía un `ifstream` con handle abierto al hacer `std::filesystem::remove` → crash silencioso (exit 9, doctest summary cortado); arreglado leyendo en scope cerrado y usando overload con `std::error_code`.

**Validación visual end-to-end del dev**: confirmó funcionamiento — esfera rotando, dropdown actualiza la esfera, sliders refrescan en vivo, drop de textura desde AssetBrowser activa `useAlbedoMap=true` automáticamente, click Guardar persiste el `.material` (probado con `acero_pulido.material` roughness 0.20 → 0.16). Reformat JSON por `nlohmann json::dump(2)` con keys alfabéticas y floats con precisión máxima — tradeoff aceptado.

**Alternativas descartadas:**
- **Node graph completo en F2H21** (scope original del plan): 1-2 semanas. Diferido.
- **Shader simplificado para preview**: duplica código del PBR sin ganar mucho.
- **Orbit cam con mouse**: nice-to-have anotado en PENDIENTES.md.
- **Schema bump del `.material`**: conservar el actual; cuando emerja node graph, ahí sí campo `graph` opcional.

**Revisar si:**
- El dev pide preview en el Inspector también (mini-preview inline cuando hay material seleccionado).
- Algún material concreto (water shader, vegetation) necesita node graph — abrir hito propio.
- El polish UX general del editor (mencionado por el dev tras F2H21: *"a futuro deberemos mejorar toda la UI para hacerla más fácil de entender"*) emerge como prioritario antes del 4-viewport — re-priorizar.

## 2026-05-07: F2H22 — UX rework (workspaces orientados a tareas + visibility default + Toolbar + AssetBrowser tabs), adelantado vs sub-fase 2.7

**Contexto:** El plan F2 original tenía la sub-fase 2.7 (UI/UX final del editor, F2H41-F2H44, ~6+ meses) para el polish ergonómico. Tras feedback explícito del dev al cerrar F2H21 (*"a futuro deberemos mejorar toda la UI para hacerla más fácil de entender"*) reafirmado al arrancar (*"mientras quede bien ordenado"*), F2H22 adelanta parte de ese scope. El 4-viewport Hammer-style layout (era F2H22 candidato charlado tras F2H20) se mueve a F2H23 — el workflow va primero, las features visuales después.

**Decisión:** **F2H22 ataca 4 fricciones específicas** identificadas en uso real, no un rewrite UX completo. Scope acotado, ~1 día de trabajo.

1. **Renames de workspaces a tareas**: `Layout → Modelar`, `Scripting → Programar`, `Profile → Optimizar`, `Materials → Materiales`. Los nombres viejos no comunicaban "esto es para hacer X".
2. **Visibility default por workspace**: cada workspace solo muestra los panels relevantes a su tarea. Antes todos los panels estaban dockeados en todos los workspaces (solo cambiaba dónde) — ruido visual.
3. **Toolbar lateral nueva**: panel `Tools` con 6 botones para gizmo modes + brushes + face mode toggle. Tools críticos descubribles al ojo, no escondidos en menús.
4. **AssetBrowser refactoreado a tabs**: 6 tabs (Texturas/Meshes/Prefabs/Materiales/Scripts/Audio) reemplazan los CollapsingHeaders apilados. La lista de meshes ya no infla el panel.

**Por qué no un rewrite completo:**
- **Lo que funciona, no se toca**: el sistema F2H7 de workspaces con `WorkspaceManager` + `Dockspace` + `iniLayout` per-workspace es sólido. F2H22 es **aditivo** (rename de strings + nuevo método de visibility + nuevo panel + refactor del onImGuiRender del AssetBrowser) — sin tocar la arquitectura.
- **El feedback del dev es iterativo**: durante la validación visual surgieron 3 fricciones más (toolbar flotante al cargar proyecto, lista de meshes exagerada, iconos abstrusos). Resueltas en bloque F polish, sin abrir hitos nuevos.

**Cómo se implementa:**

- `WorkspaceManager` con helper `migrateWorkspaceName(oldName)` aplicado en `setWorkspaces` — los `.moodproj` viejos cargan con nombres nuevos preservando `iniLayout` intacto. Sin pérdida de configuración.
- `Dockspace::buildLayoutForWorkspace` con dispatcher que acepta nombres nuevos como primary + viejos como alias defensivo (defensa en profundidad si la migración no se aplicó).
- `EditorUI::applyDefaultVisibilityForWorkspace(name)` setea `panel->visible` por workspace. Llamado desde 3 sitios:
  1. Ctor de EditorUI (arranque inicial).
  2. `applyPendingWorkspaceSwitch` cuando el workspace de destino tiene `iniLayout` vacío (primer activado en este proyecto).
  3. MenuBar "Restablecer layout" (re-aplica visibility + dock layout default).
- `editor/ui/Toolbar.{h,cpp}` (~110 LOC) panel ImGui nuevo (categoría Scene). Botones de 72×36 con texto en castellano (no letras T/R/S — el dev confirmó que no quedaban claras). Cada botón emite request al EditorUI; EditorApplication consume cada frame en `run()`. Sin acoplamiento directo al EditorApplication. La franja del Dockspace en Modelar reservó 8% del ancho para anclar el Tools a la izquierda del Hierarchy.
- `AssetBrowserPanel::onImGuiRender` reescrito a `BeginTabBar` + 6 `BeginTabItem`. Cada tab: counter de items + `BeginChild` con scroll interno. Drag&drop preservado (mismos payloads).

**Polish post-validación (3 iteraciones)**:
1. Toolbar flotante al primer arranque + ventanas flotantes vacías en el centro: causa = `imgui.ini` global persistente del último uso. Fix: en `tryOpenProjectPath`, siempre `setActiveByIndex(0)` + `applyDefaultVisibilityForWorkspace("Modelar")` + `LoadIniSettingsFromMemory("")` cuando workspace 0 sin iniLayout custom + `requestRebuildForCurrentWorkspace()`. Antes el editor recordaba el último estado; ahora siempre arranca limpio en Modelar al cargar proyecto.
2. Iconos del Toolbar abstrusos: dev pidió iconos visuales tipo Blender. Compromiso: labels en castellano (`Mover`/`Rotar`/`Escala`/`Box`/`Cilindro`/`Cara`) + tooltips. FontAwesome / IcoMoon image-based anotado como pendiente futuro (requiere mergear font binaria al init de ImGui — ~5-10 LOC + binario).
3. Lista de meshes exagerada: 84 entries (Kenney Survival Pack 82 + 4 sueltos) sobrecargaba el tab Meshes. Fix: eliminar `assets/meshes/kenney_survival/` del repo. Quedan 5 demos básicos (CesiumMan/Fox/cube_mtl/pyramid). *"Más adelante los usuarios podrán descargar sus meshes"*.

**Suite resultante:** **610/8357** (+3 cases / +16 asserts en `test_workspace_manager.cpp`). Tests cubren: migración de los 4 nombres viejos completos, mezcla nombres nuevos+viejos (solo migra los viejos), preservación de nombres custom no reconocidos.

**Validación visual end-to-end del dev:** confirmó funcionamiento — tabs de workspace en castellano, cada workspace con sus panels relevantes, Toolbar lateral en Modelar con labels claros, AssetBrowser con 6 tabs scrolleables, editor arranca siempre limpio en Modelar. Pidió cerrar el hito.

**Alternativas descartadas:**
- **Rewrite UX completo del editor** (estilo Unreal/Unity): scope masivo. Sub-fase 2.7 original lo cubre cuando el motor esté maduro. F2H22 ataca solo las fricciones identificadas en uso real.
- **Rename de strings sin back-compat**: rompe los `.moodproj` existentes con nombres viejos. Migración aplicada en `setWorkspaces`.
- **Iconos image-based en F2H22**: scope extra (font binaria + integración + diseño). Diferido — labels en castellano cubren "saber qué hace cada botón".
- **Eliminar `kenney_survival` solo del AssetBrowser** (filter UI): hack — los archivos seguirían en el repo inflándolo. Mejor borrar de raíz, dejar 5 demos, y que los usuarios traigan sus meshes (cuando F2H8+ habilite drag-drop import o similar).

**Revisar si:**
- El dev pide deshabilitar la migración (workspaces con nombres custom que coinciden con los viejos). Improbable — los 4 nombres viejos son hardcoded del editor F2H7.
- El dev pide que el editor recuerde el último workspace al cerrar/abrir (en lugar de siempre Modelar). Compromiso entre "predecible" y "respetar el flow del dev". Si emerge, agregar toggle en preferencias del proyecto.
- El polish UX general continuo identifica más fricciones (Inspector / Hierarchy / Console / StatusBar). Hito propio cuando emerja presión.

## 2026-05-07: F2H23 — pase polish UX con 5 iteraciones de feedback en vivo + multi-edit Transform agrupado

**Contexto:** Tras cerrar F2H22 (rework UX con renames de workspaces + Toolbar + AssetBrowser tabs), el dev pidió continuar el polish sobre 4 panels que F2H22 no había tocado: Inspector / Hierarchy / Console / StatusBar. F2H23 lo cubre con el patrón heredado de F2H16/F2H19 — auditoría con subagente → lista priorizada → fixes acotados.

**Decisión clave:** **scope cerrado tras Bloque B** (auditoría) + **5 iteraciones de polish post-validación** durante Bloque F. Cada iteración emergió 2-3 bugs UX nuevos que el subagente no podía predecir sin uso real (tabs cruzados, panels que se mezclan al cambiar workspace, multi-edit que no funciona desde viewport, etc.). Sin abrir hitos nuevos por cada iteración — el plan F2H23 deliberadamente dejó scope acotado para permitir 5 iteraciones sin explotar.

**Auditoría inicial (Bloque B):** 32 ítems totales (13 ALTA / 13 MEDIA / 6 BAJA). Scope cerrado en 17 (13 ALTA + 4 MEDIA críticos):

- Inspector: helpMarker `(?)` con tooltip, SeparatorText × 9 componentes, drop targets con highlight verde durante drag activo (`isDragActiveOfType`), tooltips Transform.
- Hierarchy: iconos ASCII por tipo `[M]/[B]/[L]/[A]/[S]/[T]/[C]/[P]`, hint shortcuts arriba con tooltip, warning >5000 entidades.
- Console: popup confirmación Limpiar, leyenda completa de niveles con `(?)`, input filtro ancho dinámico, counter de líneas filtradas.
- StatusBar: FPS coloreado por rango, modo Play/Editor con color, "Ultimo:" → "Ultimo comando:".

**5 iteraciones de polish (las realmente importantes):**

1. **Iter 1 — multi-edit Transform en Inspector + tabs cruzados**:
   - Inspector con multi-edit: delta del active aplicado a todas las del SelectionSet usando `IsItemActivated`/`IsItemDeactivatedAfterEdit`.
   - DragFloat3 con `FramePadding(6,6)` para clickeabilidad.
   - Cada workspace dockea solo sus panels (Script Editor ya no aparece como tab cruzado en Layout).

2. **Iter 2 — workspace Optimizar fuera + nombres + Floor**:
   - Optimizar eliminado (era para benchmark Fase 1, no flujo cotidiano del dev). 3 workspaces ahora: Layout/Programar/Materiales.
   - "Modelar" → "Layout" (revert F2H22, pedido explícito del dev).
   - Hierarchy panel name() → "Escena" (más descriptivo + castellano consistente).
   - Floor default 16×16/tile=3 → 8×8/tile=1.5 (Floor 12×12m). Antes 48×48m hacía que un brush 1m se viera diminuto.
   - WorkspaceManager filtra workspaces obsoletos al cargar `.moodproj` viejos + completa con defaults si lista incompleta.

3. **Iter 3 — bugs operacionales**:
   - SmallButton "R" en lugar de "Recargar" grande en AssetBrowser.
   - EditorCamera radio default 30 → 12m (consistente con Floor más chico).
   - **Workspace switch SIEMPRE aplica visibility default** (no solo primera vez). Antes los panels "se mezclaban" al cambiar entre workspaces porque el iniLayout custom pisaba la visibility. Tradeoff aceptado: predecible > customización persistente. La customización del dev en un workspace persiste durante la sesión actual, pero al volver desde otro workspace vuelve al default.
   - Hierarchy invierte modifiers a **Maya-style**: Shift=ADD, Ctrl=TOGGLE (antes era Blender-style Shift=toggle/Ctrl=add). Pedido del dev: *"shift seleccionar ambos y de ahí mover"*.

4. **Iter 4 — layout default columna derecha**:
   - Layout default reescrito: columna derecha unificada (Escena arriba + Inspector abajo 50/50), Viewport ocupa centro completo, Toolbar franja izquierda. Antes Escena izq + Inspector der consumían 2 columnas.
   - `finalizeGizmoDrag` aplica delta del gizmo a todas las del SelectionSet AL SOLTAR — pero las "demás" saltaban visualmente al final, no se movían en vivo.

5. **Iter 5 — los 3 bugs reales del flow viewport (la crítica)**:
   - **(a) Drag visual no en vivo**: el dev veía solo el active moverse durante el drag y las demás saltaban al soltar. FIX: snapshot `otherStarts` (entidades extra del SelectionSet) al iniciar drag con sus startValues del Field correspondiente. Cada frame del drag aplica el delta del active a TODAS las demás en vivo. Aplica a Translate / Rotate / Scale (per-axis y uniform). Helpers `readTransformField` / `writeTransformField` (con clamp para Scale).
   - **(b) Ctrl+Z no agrupado**: el HistoryStack solo recibía el active. FIX: nuevo `MultiEditTransformCommand` (`editor/commands/MultiEditTransformCommand.{h,cpp}`, ~95 LOC) que encapsula `vector<{Entity, before, after}>` compartiendo el mismo Field. `execute()`/`undo()` iteran y aplican. `isNoOp` si todas las entries son nearlyEqual. Resilience con `valid()` chequeo por entry. `finalizeGizmoDrag` construye el `MultiEditTransformCommand` cuando `otherStarts` no está vacío; sino fallback al `EditTransformCommand` single. Push del command revierte primero todos los transforms al `startValue` para que `execute()` re-aplique sin doble-aplicación.
   - **(c) Shift+click en viewport no acumulaba**: el path de pickEntity en `EditorApplication::run()` ya tenía lógica F2H13 con `keyShift` y `keyCtrl` — pero usaba el orden viejo (Blender-style). FIX: invertir a Shift=ADD / Ctrl=TOGGLE para consistencia con HierarchyPanel. Logs explícitos en cada path: "[viewport] Shift+click ADD '...' (selected=N)".
   - Validación visual end-to-end: log muestra "[viewport] Shift+click ADD 'Brush_Cyl_01' (selected=2)" + "[gizmo multi-edit] push MultiEditTransformCommand: 3 entidades, field=0/1/2" probando los 3 modos. **Dev confirmó: "funciona"**.

**Suite resultante:** **610/8359** verde sin regresiones. Tests del WorkspaceManager actualizados al schema 3-default + filtro de workspace obsoleto. UI pura (Inspector / Hierarchy / Console / StatusBar) no testeable sin GL — validación visual del dev al cierre de cada iteración.

**Limpieza assets:** pack `kenney_survival` (82 meshes externos) eliminado del repo. Quedan 5 demos básicos (CesiumMan/Fox/cube_mtl/pyramid). Pedido del dev: *"5 nomás, más adelante los usuarios podrán descargar sus meshes"*.

**Alternativas descartadas:**
- **Refactor profundo de los panels** (rework completo): scope masivo. F2H23 atacó solo fricciones identificadas en uso real con auditoría dirigida.
- **MultiEdit con commits visibles solo al final** (iter 4 approach): el dev confirmó que NO funciona cuando las demás "saltan" al soltar. La versión en vivo (iter 5) es necesaria.
- **CompoundCommand genérico** para agrupar N commands cualesquiera: scope mayor que `MultiEditTransformCommand` específico para Transform. Diferido a hito futuro si emerge necesidad de agrupar otros tipos de commands (ej. crear+mover+pintar como un solo undo).
- **FontAwesome para iconos image-based**: deuda explícita F2H22 que F2H23 NO ataca. Hito chico futuro.

**Revisar si:**
- El dev encuentra más fricciones tras uso real prolongado (iter 6+): atender en hito propio si pasan las 3 iteraciones del polish.
- El dev pide que la customización de visibility por workspace persista entre switches: agregar toggle "modo predecible / modo customizable" en preferencias.
- Los archivos grandes empiezan a frenar el desarrollo: F2H24 resuelve esto (split por dominio).
- El gizmo de rotación con multi-selección revela bugs sutiles (rotación se aplica per-entity sin pivot común — esperado pero documentar si emerge confusión).

## 2026-05-08: F2H24 — split de archivos críticos >800 LOC en partials por dominio (refactor estructural sin cambios funcionales ni de API pública)

**Contexto:** F2H23 cerró con 5 iteraciones de polish UX donde el dev confirmó *"funciona"* pero también pidió explícitamente *"creo que hay archivos demasiado grandes que te cuesta arreglar, así que mejor debemos organizar, que ningún archivo tenga demasiadas líneas para que sea fácil de mantener"*. El cap soft 500 / hard 800 LOC por `.cpp/.h` ya estaba documentado en CLAUDE.md y en la memoria del proyecto, pero acumulamos 5 archivos CRÍTICOS >800 LOC durante Fase 1 y Fase 2: InspectorPanel.cpp 1338, EditorProjectActions.cpp 1272, DemoSpawners.cpp 1188, PlayerApplication.cpp 1160, EditorApplication.cpp 826. Total: 5784 LOC repartidos en archivos que el dev encontraba difíciles de mantener.

**Decisión:** F2H24 — refactor puramente estructural. Split de los 5 CRÍTICOS en archivos parciales con sufijo descriptivo (`Foo_<Dominio>.cpp`) implementando métodos privados de la **misma clase** declarada en `Foo.h`. Helpers compartidos en header interno `Foo_Internal.h` con namespace `Mood::detail`. API pública intacta. Cero cambios funcionales — el editor y el player arrancan idénticos al usuario final. Los 4 archivos ALTO (700-780 LOC) quedan en `PENDIENTES.md` para hito futuro si emerge presión.

**Razones:**
- **Cumplir cap del proyecto** (soft 500 / hard 800 LOC). F2H24 reduce los 5 CRÍTICOS para que ningún partial supere 500 LOC excepto `_Frame` y `_Run` que rondan 435-484 (loops monolíticos sin sub-secciones obvias).
- **Patrón ya validado en el repo**: `EditorApplication.cpp` ya tenía 6 partials desde Hito 16 (EditorProjectActions / DemoSpawners / EditorOverlay / EditorPlayMode / EditorRenderPass / EditorScene). F2H24 extiende el mismo patrón a InspectorPanel + DemoSpawners + PlayerApplication + EditorApplication.
- **Validación incremental**: build + suite verde después de cada Bloque B.X (5 sub-bloques, 5 commits intermedios). Permite detectar regresiones por TU sin debugar todo el split al final. La suite 610/8359 quedó idéntica antes y después de cada bloque (refactor sin cambios funcionales por construcción).
- **API pública intacta = cero riesgo de regresión externa**: solo `InspectorPanel.h` recibió 13 métodos PRIVADOS nuevos (`renderTagSection(Entity)`, etc.) para que el dispatch del `onImGuiRender` quede legible. El user-facing del editor + player + tests no ve diferencia alguna.
- **Headers internos `Foo_Internal.h` para helpers compartidos**: alternativa al namespace anónimo (que solo es visible dentro de un único `.cpp`). Patrón limpio aplicado a InspectorPanel (`pushEditIfDone` template + `helpMarker` + `isDragActiveOfType` con `inline`) y DemoSpawners (`WorldYBounds` + `rotatedAabbWorldY` con `inline`).

**Distribución LOC por archivo crítico:**

- **InspectorPanel.cpp 1338 → 11 archivos**: núcleo 77 (dispatch por `hasComponent<>`) + Internal.h + 10 partials por componente (Misc 82 = Tag+Camera+Trigger; Audio 103; Physics 106; Animation 108; Script 130; Transform 141; Light 160 = Light+Environment; MeshRenderer 177; Particles 192; Brush 208).
- **EditorProjectActions.cpp 1272 → 7 archivos**: núcleo 106 (confirmDiscardChanges + addToRecentProjects + load/saveEditorState — helpers compartidos) + _FileIO 329 (project lifecycle: new/open/save/saveAs/close/newScript) + _Package 101 (handlePackageProject único) + _Map 257 (multi-mapa: saveMapAs/newMap/openMap/setDefault/delete + sanitizeMapName + syncMapsSnapshot) + _Brush 117 (spawnBrushEntity helper + 7 handleAdd*Brush) + _Boolean 327 (snapshot helpers + buildWorldBrush + handleBooleanOp F2H12) + _Compile 108 (formatCompileStats + handleCompileMap + handleExportObj F2H20).
- **DemoSpawners.cpp 1188 → 5 archivos + Internal.h**: núcleo 41 (pushCreatedEntities) + Internal.h (WorldYBounds + rotatedAabbWorldY) + _Basic 199 (8 demos chicos: Rotator/HUD/PhysicsBox/Environment/PointLight/AudioSource/FireParticles/Trigger) + _Stress 376 (6 demos pesados: Enemy/Shadow/PbrSpheres/LightStress/AnimatedChar/StressTris) + _Prefab 214 (SavePrefab + ViewportPrefabDrop) + _Drop 399 (4 viewport drops con Face Mode awareness: Texture/Mesh/Material/Script).
- **PlayerApplication.cpp 1160 → 4 archivos**: núcleo 111 (mapWorldOrigin + buildTestMap + rebuildSceneFromMap) + _Init 224 (PlayerApplication ctor + tryLoadGameManifest + dtor) + _Frame 484 (processEvents + beginFrame + endFrame + updateCamera char controller + updateRigidBodies + run loop) + _SaveLoad 435 (drawMainMenu + applyLoadedSave + captureCurrentState + quickSave F5 + saveAs F6).
- **EditorApplication.cpp 826 → 3 archivos**: núcleo 173 (updateWindowTitle + markDirty + processEvents + beginFrame + endFrame + mapWorldOrigin + viewportAspect) + _Init 275 (glDebugCallback + ctor con SDL/GL/ImGui/sistemas/inyeccion + dtor) + _Run 435 (loop principal con dispatchers de UI requests + click-to-select Maya-style + system updates physics/scripts/triggers/animation/nav/particles/audio + render).

**Errores resueltos durante el trabajo (no requieren mención del dev pero quedan registrados):**
- **`glm/gtx/compatibility.hpp` agregado por error a `InspectorPanel_Brush.cpp`**: extensión experimental de glm que requiere `GLM_ENABLE_EXPERIMENTAL` antes del include. Eliminado — no era necesario para el código del partial.
- **`APIENTRY` undefined en `EditorApplication_Init.cpp` cuando se compilaba como TU separada**: la macro APIENTRY se define solo si `<windows.h>` se incluyó transitivamente, lo cual variaba según el orden de includes del partial. Fix definitivo: usar `GLAD_API_PTR` en lugar de `APIENTRY` para la firma del callback `glDebugCallback` — `GLAD_API_PTR` siempre lo define `glad/gl.h` (alias condicional de APIENTRY o vacío según platform). Patrón general: en partials, evitar macros que dependen de inclusión transitiva.
- **`FrameStats` undefined en `EditorApplication_Run.cpp`**: forward-declared en `SceneRenderer.h` (donde solo se usa como tipo de retorno opaco), definición completa en `IRenderer.h`. El partial necesitaba `IRenderer.h` además de `SceneRenderer.h` para que el `FrameStats stats = m_sceneRenderer->frameStats()` compilara.

**Alternativas descartadas:**
- **Refactor profundo con extracción de clases helper**: scope masivo + cambia API. F2H24 ataca solo el problema de tamaño con cambios mínimos.
- **Funciones libres en namespace anónimo dentro del partial** (en lugar de métodos privados de la clase): no pueden acceder a miembros privados (`m_ui`, `m_assets`, etc.). Solución vía `friend` declarations sería peor que agregar métodos privados al header.
- **Bloque C: split de los 4 archivos ALTO (700-780 LOC)** (`SceneRenderer`, `MeshLoader`, `EditorOverlay`, `AssetManager`): skipped por presupuesto. Bajo el hard cap 800 — menos urgentes. Movidos a `PENDIENTES.md` como deuda chica.
- **CompoundCommand genérico** para agrupar refactors en commits atómicos: F2H24 ya tiene granularidad por Bloque B.X (5 commits intermedios), suficiente para revertir si emerge regresión.

**Revisar si:**
- El dev encuentra que algún partial sigue sintiéndose grande (>500 LOC y costoso de editar): partir más fino. Candidatos probables: `_Frame` (484) y `_Run` (435).
- Los 4 archivos ALTO movidos a PENDIENTES.md (`SceneRenderer.cpp` 776, `MeshLoader.cpp` 767, `EditorOverlay.cpp` 745, `AssetManager.cpp` 743) crecen pasada la marca 800: abrir hito chico para split de los que estén sobre el cap.
- Aparece un caso donde la inclusión transitiva de macros varía entre TUs y rompe builds: estandarizar headers internos `Foo_Internal.h` para isolar dependencias macro-sensibles.
- Surge necesidad de un patrón de helpers compartidos por partials además de los 5 ya hechos: codificar el patrón `Foo_Internal.h` con namespace `Mood::detail` como convención del proyecto.

## 2026-05-08: Cull de overlap parcial via BSP polygon clipping (F2H25)

**Contexto:** F2H20 implementó la compilación brush → mesh estática con cull de **pareja exacta** (dos caras coincidentes con normales antiparalelas). Faltaba cull de **overlap parcial** — cuando una cara está parcialmente dentro de otro brush. Sin esto, mapas con brushes solapados arrastran tris invisibles dentro del volumen vecino.

### Decisión 1 — BSP-style polygon clipping (Sutherland-Hodgman extendido)

**Decisión:** algoritmo iterativo. Para cada cara F y cada brush B != A: clipear F contra los planos de B uno por uno. En cada plano, split en `above` (lado positivo = afuera del brush respecto a ese plano = output) + `below` (sigue siendo testeado contra los siguientes planos). Lo que sobrevive en `inside` tras todos los planos = adentro de B → descartar.

**Razones:**
- Polígonos convexos por construcción (`worldPolygonCcw` viene de `BrushMesh.cpp`). Sutherland-Hodgman funciona tal cual sin descomponer.
- Una cara contra un brush convexo = cara intersectada con la unión de half-spaces externos. El BSP iterativo materializa esa unión correctamente.
- Reusa infra existente: `Plane`, `signedDistance`, `kPlaneEpsilon`. Cero deps nuevos.

**Alternativas descartadas:**
- Clipping general polígono-polígono (Weiler-Atherton, Vatti): overkill para polígonos siempre convexos.
- CSG completo con Carve/manifold: refactor masivo del modelo.

### Decisión 2 — 3 pre-tests críticos antes del BSP loop

**Decisión:** pre-tests al inicio de `cullPolygonAgainstBrush`:
1. "Cara entera afuera de B" — output = poly entero sin partir.
2. "Cara entera adentro de B" — output vacío.
3. "Polígono coplanar a un plano de B" — emit en `below` (NO en `above`), saltea ese plano.

**Razones:**
- Sin pre-test 1, el BSP partía la cara en N trozos contiguos cuya unión era la cara entera (N-1 splits falsos + N draw calls innecesarios).
- Pre-test 2: descartar caras totalmente internas sin pasar por el loop O(P).
- Sin pre-test 3, cara coplanar con la pared exterior de B salía prematuramente como output cuando la sub-region central estaba dentro de B respecto a los OTROS planos.

### Decisión 3 — Stats: caras eliminadas enteras + fragmentos partidos (no "tris ahorrados")

**Decisión:** `culledOverlapTriangles` cuenta SOLO los tris de caras descartadas enteras. `splitFragments` cuenta caras partidas en >1 fragmento.

**Razones:**
- El BSP clipping puede AUMENTAR el tri count: una cara de 2 tris partida en 4 fragmentos puede generar 8 tris (cada fragmento independiente requiere su propia triangulación).
- El beneficio del cull es ÁREA visible (overdraw), no tri count global. Mapas grandes con mucho overlap se benefician en términos de overdraw.
- Reportar "tris ahorrados" sería engañoso: en muchos casos sería negativo.

### Decisión 4 — UI layout version stamp en `imgui.ini`

**Decisión:** cambiar `io.IniFilename = "imgui.ini"` → `"imgui_layout_v2.ini"`. Bumpear el sufijo cuando agreguemos paneles nuevos al dockspace.

**Razones:**
- Pedido directo del dev: "por defecto la UI sea fija, luego el usuario acomodará a su gusto".
- Tras F2H22 que agregó panel `Tools`, los `imgui.ini` viejos lo dejaban flotante al primer arranque.
- Trade-off aceptado: customizaciones en archivos viejos no se migran al bumpear. Frecuencia: 1-2 bumps por hito de UI. Aceptable.
- Sin file I/O custom: 1 línea de código.

**Alternativas descartadas:**
- ImGui Settings Handler propio que detecte versión: ~50 LOC de boilerplate por mejora marginal.

## 2026-05-08: Runtime-load de mesh compilada en MoodPlayer (F2H26)

**Contexto:** Plan original F2H14 hablaba de "brushes solo en el editor" — los brushes son herramienta de autoría; la entrega final al runtime es la mesh estática unificada. F2H20 entregó la compilación on-demand pero NO la persistía en `.moodmap` ni la consumía el Player. F2H26 cierra ese loop.

### Decisión 1 — Schema bump aditivo v12→v13, NO destructivo

**Decisión:** `compiledMesh` opcional al top-level del JSON. Mapas v12 cargan como v13 con `compiledMesh = nullopt`. Los brushes siguen persistidos siempre (para que el editor pueda re-editar).

**Razones:**
- Back-compat sin migración: cualquier `.moodmap` existente sigue cargando.
- Editor sigue editando brushes: el `compiledMesh` es OUTPUT del editor + INPUT del Player. El Editor lo escribe pero NO lo usa para render.
- Roundtrip preserva todo: brushes individuales + compiledMesh coexisten. Al guardar tras editar, el editor regenera el compiledMesh desde los brushes nuevos.

**Alternativas descartadas:**
- Reemplazar `brushes` por `compiledMesh`: rompe edición.
- Persistir compiledMesh en archivo separado: más complejidad sin beneficio claro para mapas chicos.

### Decisión 2 — Layout PBR de 11 floats interleaved (mismo que `brushSubmeshToInterleaved`)

**Decisión:** `SavedCompiledSubmesh.vertices` es `vector<f32>` con 11 floats por vertex (pos+color+uv+normal), indices ya expandidos sin EBO.

**Razones:**
- Mismo formato que `brushSubmeshToInterleaved` produce: el editor reusa el helper. Cero código nuevo de serialización.
- Compatible con shader PBR estándar: `kPbrAttrs` se reusa para BrushComponent y CompiledMeshComponent. Un solo render path.
- Color=1 fijo: el `albedoTint` del material domina; mantener color real no aporta.

**Alternativas descartadas:**
- 8 floats sin color: obligaría VAO distinto. No vale el ahorro.
- EBO con indices separados: requeriría expandir al cargar. Optamos por expandir al SAVE (una vez) para load-time mínimo en Player.

### Decisión 3 — `CompiledMeshComponent` move-only + 1 entity por mapa

**Decisión:** Componente nuevo con `vector<unique_ptr<IMesh>>` + `vector<MaterialAssetId>` paralelos. Move-only. Player crea **una sola entity** "WorldCompiledMesh".

**Razones:**
- Mismo patrón que F2H17 BrushComponent multi-material. Vector paralelos, 1 draw call por submesh.
- 1 entity por mapa: la mesh compilada es UN objeto unificado.
- Move-only por unique_ptr: evita copias accidentales.

**Alternativas descartadas:**
- Una entity por submesh: genera N entities cuando 1 alcanza. Sin beneficio.
- Reusar `MeshRendererComponent`: no aplica (MeshRenderer apunta a MeshAssetId persistido; compiled mesh es runtime-creada).

### Decisión 4 — Flag `useCompiledMesh` en SceneLoader, NO un loader separado

**Decisión:** `applyEntitiesToScene(saved, scene, assets, bool useCompiledMesh = false)`. Editor pasa default `false`; Player pasa `true`.

**Razones:**
- Una sola función, dos modos: evita duplicar código de carga de tiles/entidades/lights.
- Default `false` es seguro: callsites existentes (Editor + tests) no cambian.
- Fallback automático: Player con flag `true` pero sin compiledMesh (mapa v12 legacy) cae a procesar brushes. Transición v12→v13 transparente.

**Alternativas descartadas:**
- Dos funciones distintas: duplica código.
- Flag implícito desde `mode` global de la app: oculta la decisión, dificulta testing.

## 2026-05-08: F2H27 (F6 panel) descartado durante implementación

**Contexto:** F2H27 estaba planificado como "F6 panel estilo Blender — ajustar params del último operator post-hoc" (diferido desde F2H16). Durante implementación, recortado a versión "F6-light" con sliders Position/Rotation/Scale del último brush spawneado. El dev al verlo: *"no entiendo porque aparece esta información acá si directamente para eso tengo el inspector, es redundante"*.

**Decisión:** descartar F2H27 entero. Código removido, sin tag.

**Razones:**
- F6-light era redundante con Inspector: ya muestra Transform editable de la entidad seleccionada.
- F6 real de Blender requiere parametrizar comandos: ajustar `size`/`segments`/`materialDefault` al spawn — params que NO están en Inspector y requieren metadata por comando + UI dedicada + re-ejecución del comando con nuevos params. Scope de hito grande propio.
- Reconocer el error rápido: el dev marcó la redundancia en la primera validación. Descartar antes de invertir más en algo sin valor agregado.

**Alternativas consideradas:**
- Mantener F6-light para evitar abrir Inspector cuando el brush no está seleccionado: marginal — Ctrl+click selecciona y abre Inspector con 1 click extra.
- Implementar F6 real ahora: scope grande, no priorizado.

**Revisar si:**
- El dev pide explícitamente "ajustar params del operador post-spawn" (ej. cambiar `segments` de un cilindro tras spawnearlo): abrir hito propio con parametrización formal de comandos.

---

> **Nota:** entre F2H28 y F2H74 las decisiones se registraron en los docs por
> hito (`docs/hitos/F2H<N>.md`) en vez de acá. F2H75 retoma el log para las
> decisiones de mayor alcance arquitectónico.

## 2026-05-21: F2H75 — render de mesh dinámico para cloth (renderer dedicado, no MeshRendererComponent)

**Contexto:** la tela (cloth/soft body) necesita un mesh cuyos vértices se reescriben cada frame desde la simulación de Jolt. Hasta F2H74 TODA la geometría del engine era estática (`GL_STATIC_DRAW`) o skinneada por matrices de hueso — no existía un path para geometría procedural actualizada por frame.

**Decisión:** renderer dedicado (`OpenGLClothRenderer`, espejo del `OpenGLParticleRenderer`) con VBO `GL_DYNAMIC_DRAW` re-subido por frame (orphan + `glBufferSubData`) + shader propio `cloth.{vert,frag}` lit simple (1 direccional + ambiente), doble cara vía `gl_FrontFacing`. NO se reusó `MeshRendererComponent` + el draw loop opaco.

**Razones:**
- El path de mesh estándar asume asset cacheado en `AssetManager` + `uModel` desde el Transform + geometría estática. La tela tiene vértices en world-space que cambian cada frame y no es un asset compartido.
- Un renderer dedicado no perturba el pipeline de assets/materiales/PBR (blast radius chico) y reusa un patrón ya probado (partículas).
- Costo aceptado: lit simple en vez de PBR completo (lights+shadows+IBL). Para una bandera/cortina alcanza; upgradeable si emerge demanda.

**Alternativas descartadas:**
- `MeshRendererComponent` + IMesh dinámico: reusaría el lit PBR completo, pero requería un IMesh dinámico fuera del cache de assets + un flag double-sided en el draw loop + transformar los vértices world→local por frame (inverse del Transform). Más acoplamiento al pipeline central por un beneficio visual marginal en v1.
- Dibujar la tela como wireframe con el debug renderer: descartado por el dev (quería tela sólida iluminada).

**Otras decisiones del hito** (detalle en `docs/hitos/F2H75.md`): grilla procedural NxM en vez de import de mesh; `stiffness [0,1] → compliance` de Jolt; viento tratado como aceleración sobre la velocidad de las partículas libres; `previewRest` analítico para el editor sin física.

**Revisar si:**
- Emerge demanda de telas con texturas/PBR/sombras (ropa de personajes, etc.): evaluar migrar al path de mesh estándar con un IMesh dinámico + flag double-sided.
- Aparecen muchas telas simultáneas: el renderer re-sube cada una a un VBO compartido secuencialmente; considerar un VBO por tela o instancing.

## 2026-05-21: F2H75 fix lateral — Slider constraint con límites iguales (Jolt v5.2.0 assert)

**Contexto:** al correr la suite completa tras el Bloque B de F2H75, el test `test_physics_constraints.cpp` (Slider, F2H71) crasheaba con un assert de Jolt: `SliderConstraint.cpp:159 mLimitsMin != mLimitsMax || mFrequency > 0` ("Better use a fixed constraint"). El test crea un slider "bloqueado" con travel `[0,0]`. Pre-existente y ajeno al cloth.

**Decisión:** `createSliderConstraint` expande `mLimitsMax` por un epsilon (`1e-4 m`) cuando `min == max`, dejando el slider efectivamente fijo pero válido para Jolt.

**Razones:**
- Un slider con travel cero (el dev lo "bloquea" desde el editor) es UX válida; no debe crashear la app en Debug.
- El fix en el wrapper protege tanto el test como cualquier uso runtime (el dev podría setear min==max en el Inspector).
- El epsilon es imperceptible (0.1 mm de travel).

**Alternativas descartadas:**
- Arreglar solo el test (usar min≠max): dejaría el crash latente para el dev en runtime.
- Bloquear min==max en la UI: trata el síntoma, no la causa; el wrapper es el punto correcto.

**Revisar si:**
- El dev quiere un "lock real" rígido: el Fixed constraint (F2H71) es la vía correcta, no un slider de travel ~0.

## 2026-05-21: F2H76 — temas del editor como presets en código (no editor de paletas)

**Contexto:** primer hito de la Sub-fase 2.7 (UI/UX). El dev quería poder cambiar el look del editor. Opciones: (a) un set de temas predefinidos, (b) un editor de colores donde el usuario arma su propia paleta.

**Decisión:** 4 temas built-in (`EditorThemes`: dark/light/midnight/sepia), cada uno una función que rellena el `ImGuiStyle`. Registro `{id, i18nKey}` para poblar el combo. El redondeo de esquinas (`applyRounding`) se setea **aparte de los colores**, al final de `apply()`, común a todos los temas.

**Razones:**
- 4 presets cubren el 90% de la necesidad (oscuro/claro + 2 con personalidad) con esfuerzo acotado; un editor de paletas es un hito propio.
- El registro `id → función` deja la puerta abierta a temas por JSON a futuro sin refactor.
- El redondeo separado de los colores evita duplicarlo en cada preset y sobrevive al theme-switch (las `StyleColorsX` solo tocan `style.Colors`, no geometría).
- Idioma centralizado en Preferencias (removido de Ver → Idioma): tenerlo en dos lados era redundante (señalado en la auditoría UX de la sesión).

**Alternativas descartadas:**
- Editor de paletas custom por usuario: futuro; el diseño lo admite pero excede el scope de "abrir la casa de los ajustes".
- Persistencia con botón OK/Cancel: el combo aplica + guarda live; `settings.json` es chico, un OK agregaría fricción sin valor.

**Revisar si:**
- Emerge demanda de paletas 100% custom editables por el usuario: agregar un tema "custom" cuyo `apply` lea colores del `settings.json`.
- El runtime (MoodPlayer) necesita temas: hoy este hito tema-iza solo el editor.

## 2026-05-21: F2H77 — hito acotado por auditoría + "sin guardar" reusa el dirty flag existente

**Contexto:** tras F2H76 el dev pidió aplicar "esa forma" (el redondeo) a más paneles + recomendaciones UX. Una auditoría con agente explorador listó varias "quick wins". Al verificarlas en código, la mayoría **ya estaban implementadas** (tooltips con atajos en toolbars, pestaña Luces, Inspector con estado vacío + secciones colapsables).

**Decisión:** acotar F2H77 a lo genuinamente faltante: (1) espaciado/padding unificado en el tema (hermano del redondeo), (2) un badge "sin guardar" en la status bar que **reusa el `m_projectDirty` existente** en vez de derivar dirty del undo stack.

**Razones:**
- Verificar el audit antes de implementar evita "trabajo falso" sobre features ya presentes (el reporte del explorador sobre-estimó los gaps).
- `m_projectDirty` ya es un flag robusto (prendido en todos los `markDirty()`, apagado en save, ya pone el `*` en el título del SO). Surfacearlo en la status bar es cero riesgo. Derivar dirty del `HistoryStack` (que no tiene save-point) sería frágil y no capturaría ediciones fuera del stack.
- Sync en `updateWindowTitle()` (ya invocado en cada transición de dirty) evita polling por frame y mantiene un único punto de verdad.

**Alternativas descartadas:**
- Trackear `undoCountAtLastSave` en el HistoryStack: más código, más frágil, redundante con `m_projectDirty`.
- Implementar toda la lista del audit: la mayoría era trabajo ya hecho.

**Revisar si:**
- El dev quiere granularidad por-asset (script/shader/item con su propio "sin guardar"): hoy el badge refleja el dirty del proyecto/mapa, no de cada editor de asset.

## 2026-05-22: F2H80 — miniaturas 3D cacheadas (render-once) + primitivas reusando el path de brushes

**Contexto:** el "+ Crear Entidad" y el Asset Browser listaban meshes como texto. Se quería un grid de cards con preview 3D (estilo SFM / Unreal). El `MaterialPreviewRenderer` ya renderizaba una esfera con un material, pero no un mesh arbitrario.

**Decisión:** `MeshThumbnailRenderer` nuevo (no extender el de materiales): renderiza el mesh real a un FBO por mesh y **cachea la textura** (render-once, lazy). Las primitivas se previsualizan construyendo su `Csg::Brush` → `buildBrushMesh` → `createDynamicMesh` (reusa el path de brushes ya existente). Las luces NO se renderizan en 3D (no tienen modelo) → ícono. El mini-player de animaciones se difiere a F2H81.

**Razones:**
- **Render-once cacheado, no animado**: una grilla de N miniaturas rotando es cara (N draws/frame) y distrae; una textura fija a ángulo 3/4 cubre el reconocimiento visual. El preview animado del Material Editor se justifica porque es 1 sola esfera con foco.
- **Clase nueva vs. extender MaterialPreviewRenderer**: la lógica de cache + camera-fit + multi-submesh es distinta; no tocar el preview de materiales (que anda). El setup PBR común se compartió con helpers privados.
- **Primitivas reusando brushes**: `buildBrushMesh`/`brushSubmeshToInterleaved`/`createDynamicMesh` ya existían (la render layer ya dependía de Csg vía SceneRenderer); no se reinventó geometría.
- **Luces = ícono**: una luz no tiene geometría; un thumbnail 3D no aporta (estándar Unity/Unreal).
- **Animaciones a F2H81**: previsualizar un clip requiere montar un personaje con esqueleto + posarlo = mini-reproductor, feature de otra naturaleza que una miniatura estática.

**Alternativas descartadas:**
- Registrar las primitivas como MeshAssets sintéticos en el AssetManager: acoplaría el AssetManager a la generación CSG por un beneficio solo de preview.
- Animar los thumbnails: caro y distractor en una grilla.

**Revisar si:**
- Hay cientos de meshes: el cache es 1 FBO 128² por mesh; si la memoria importa, bajar a 96² o evictar LRU.
- Se re-importa un mesh (su id cambia): hay `invalidate(id)` / `clear()` para refrescar.

## 2026-05-22: F2H81 — preview de animaciones hover-to-play + Inspector plegable + break de auditoría acotado

**Decisión (preview de animaciones):** cada clip es una card sobre el NPC de Mixamo; la card con el mouse encima se reproduce **en vivo**, el resto muestra una **miniatura estática cacheada**. No auto-reproducir todas ni un mini-player único.

**Razones:** una grilla de N personajes animándose = N draws skinned/frame (caro) + distrae; un mini-player único es menos directo (clic → mirar otro lado). Hover-to-play da reconocimiento inmediato con costo de 1 render vivo. El huevo-y-gallina del hover (necesito la textura antes de dibujar el botón) se resuelve con el clip hovered del frame previo (1 frame de lag, imperceptible).

**Decisión (Inspector):** cada componente pasa de `SeparatorText` (siempre abierto) a `CollapsingHeader` plegable vía `beginComponentSection<T>` (default abierto). Tag queda fijo (es el nombre). "Quitar componente" por clic derecho usa `makeRemoveComponentCommand<T>` que **snapshotea por move** y restaura por construcción.

**Razones:** apilar 16 secciones siempre-abiertas mareaba (pedido literal del dev). El snapshot por move (no copy) es obligatorio: `BrushComponent` es move-only (copy borrado por miembros con ownership GPU); mover el componente fuera del registro antes de destruirlo + restaurar por `addComponent<T>(std::move(...))` evita `operator=`. Las stats read-only del MeshRenderer van a un foldout "Technical details" colapsado (estilo Unity) — no se borran, se esconden.

**Decisión (break de auditoría):** acotado a **"Components.h + DRY, diferir los 3 de render"**. Se partieron los archivos >800 multi-función / data (`AssetBrowserPanel.cpp`, `EntitySerializer.cpp`, `Components.h`); se **difirieron** los 3 que son una sola función gigante / god-class en el hot path de render (`SceneRenderer_Render.cpp`, `EditorRenderPass_Overlay.cpp`, `EditorApplication.h`).

**Razones:** partir un archivo multi-función o un header de structs es mecánico y los errores son loud (no compila). Partir una función gigante de render = extraer helpers identificando estado capturado + orden GL; un error ahí es **sutil y visual**, no lo agarra el test suite. Meterlo a las apuradas en un break de auditoría va contra "mejores prácticas". `Components.h` quedó como agregador de 3 headers por categoría → cero churn en los call-sites.

**Alternativas descartadas:**
- Forzar los 6 en un pase: riesgo de regresión visual sin validación automatizada.
- Dejar Components.h sin partir: es el header más incluido; el split por categoría detrás del agregador mejora navegación + compile times sin romper a nadie.

**Revisar si:**
- Se hace el hito de refactor de render diferido: ver [BACKLOG.md § 4](BACKLOG.md).
- Un componente nuevo no es ni copy ni move constructible: `makeRemoveComponentCommand<T>` no compilaría para ese T (caso teórico — todos los componentes actuales son al menos move-constructible).



