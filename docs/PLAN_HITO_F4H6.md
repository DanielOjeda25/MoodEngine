# PLAN F4H6 — Game feel pass del combate

**Estado:** PLAN.
**Predecesor:** F4H5 cerrado (tag `v3.5.0-fase4-hito5`).
**Origen:** PLAN_FASE4.md §5 — era F4H5 textual (game feel pass), renumerado a F4H6 tras el bump de F4H4 (HUD/pickups). Cierra Sub-fase 4.1 "¿se siente bien disparar?".

---

## Norte

Hoy el combate FUNCIONA (HP, armor, ammo, swap, hitscan, projectile, splash, pickups) pero NO SE SIENTE. El jugador dispara y el feedback visual es casi nulo: ni muzzle flash, ni hit marker, ni screen shake, ni tracer. F4H6 cierra ese gap con efectos **procedurales** (sin assets nuevos — pure code + particles) intensidad **HL/COD sutil** (no Doom exagerado — feedback visible pero contenido).

**Norte arcade:** que disparar se sienta como un evento físico — tu pantalla pulsa, tu crosshair confirma, tu proyectil deja rastro, tu daño te empuja la cámara.

---

## Decisiones (cerradas con AskUserQuestion)

- **D1 — Scope: TODOS los efectos.** Muzzle flash + hit marker + screen shake + crosshair dinámico + pain reaction + tracer del proyectil. Cierra Sub-fase 4.1 de una.
- **D2 — Intensidad: HL/COD sutil.** Efectos visibles pero contenidos. Sin exagerar. Si emerge falta de "punch" tras playtests, se sube via UserSettings backlog (`MOOD_GAMEFEEL_INTENSITY` global, F4H6.1).

**Decisiones del agente (defaults convencionales):**

- **D3 — Camera shake: noise procedural con decay exponencial.** Amplitud + duración por evento; ruido senoidal con frecuencia alta para feel "shake" (no oscilación periódica). Cooldown anti-spam para disparos consecutivos.
- **D4 — Crosshair dinámico: gap base 8px + spread del arma activa.** Gap = `8 + min(16, spreadDeg * 1.5)`. Velocity-based opening opcional F4H6.1 si emerge demanda.
- **D5 — Pain reaction: pitch wobble + roll random del FpsCamera.** Pitch +2° smoothed + roll random ±1° sobre 0.25s, return a 0 en 0.3s. NO mover yaw (mantiene apuntar).
- **D6 — Tracer del proyectil: ParticleEmitter persistente attached al proyectil.** Color por categoría (rocket=humo gris semi-transparente, plasma=cyan glow additive, granada=chispas cortas). Lifetime ~0.3s de las partículas — estela corta detrás del proyectil.

---

## Sub-tareas

### Sub-tarea 1 — CameraShake helper + apply path

- `GameState::triggerCameraShake(amplitude, duration)` — gemelo de `triggerDamageFlash`. Setea `hud.shake_t / shake_amp / shake_max_t`.
- `HudState` extendido: `shake_t / shake_amp / shake_max_t` + `pain_pitch_t / pain_pitch_amp` (Sub-5).
- `EditorApplication::tickSystems` Play mode: decrementa timers, computa offset 2D (`sin(time * freq_x)`, `sin(time * freq_y)`) escalado por `amp * (timer / maxTimer)`.
- Apply: agregar offset al `m_playCamera` position antes del frame render (variable miembro `glm::vec3 m_cameraShakeOffset`). El SceneRenderer lee `m_playCamera.position()` para la view matrix → el shake aplica directo.
- Tests: trigger setea timer + amp, decay lineal a 0, intensidad escalada.

### Sub-tarea 2 — Muzzle flash particle burst

- `Weapon::fire`: tras spawn projectile / hit raycast, spawnea entity efímera `__muzzle_flash` con `ParticleEmitterComponent` (color amarillo-blanco, lifetimeMax=0.05s, 8 partículas, velocityRange chico forward del shooter).
- Reusar `ParticleBurstComponent { ttl=0.1s }` para auto-cleanup (gemelo del `__weapon_impact_burst` F4H2).
- Trigger `triggerCameraShake(0.02, 0.08)` (chico) en cada disparo del player.

### Sub-tarea 3 — Hit marker trigger

- En `Weapon::fire`: si algún pellet pegó a entity con `HealthComponent` y shooter es tag `"player"` → `GameState::triggerHitMarker()` (helper ya existe F2H39).
- En `Projectile::tickSystem::explode`: si splash damage afectó algún entity con Health → `triggerHitMarker()` (también si el proyectil pegó direct hit a Health entity).
- El widget `drawHitMarker` ya pinta el crosshair cyan flash 0.3s; solo faltan los triggers.

### Sub-tarea 4 — Crosshair dinámico

- `drawCrosshair` widget en `GameOverlay.cpp`: lee `hud.crosshair_spread_deg` (nuevo campo HudState) + computa `gap = 8 + min(16, spread * 1.5)`. Las 4 líneas se separan proporcional al gap.
- `EditorApplication::tickSystems`: cada frame, sync `hud.crosshair_spread_deg` desde `spec.spreadDeg` del arma activa del player (~0.5 pistola / 6 shotgun / 0 rocket).

### Sub-tarea 5 — Pain reaction (camera pitch + roll wobble)

- Al detectar damage al player en el bridge (mismo polling que F4H4 `prevHitFlashTimer`), trigger `GameState::triggerPainReaction()`:
  - `hud.pain_pitch_t = 0.25f` + `hud.pain_pitch_amp = 2.0f deg` + `hud.pain_roll_offset = (random ±1.0 deg)`.
- Apply: cada frame en Play decae timer, agregar offset al pitch/roll del `m_playCamera` ANTES de la view matrix.
- Cooldown anti-spam: si el timer ya activo, no re-trigger (evita oscilación si te dañan en cada frame).

### Sub-tarea 6 — Tracer del proyectil

- `Projectile::spawn` (en `Weapon::fire` projectile branch): agregar `ParticleEmitterComponent` al entity del proyectil con preset por categoría:
  - Rocket: emitRate 60/s, color start `(0.5, 0.5, 0.5, 0.8)`, end `(0.3, 0.3, 0.3, 0.0)`, size 0.15 → 0.05, lifetimeMax 0.4s, additive=false.
  - Plasma: emitRate 80/s, color start `(0.3, 0.7, 1.0, 1.0)`, end `(0.1, 0.4, 1.0, 0.0)`, size 0.1 → 0.02, lifetimeMax 0.3s, additive=true.
  - Granada: emitRate 30/s, color start `(1.0, 0.8, 0.2, 0.9)`, end `(0.5, 0.2, 0.0, 0.0)`, size 0.05 → 0.01, lifetimeMax 0.2s, gravityFactor 0.3, additive=true.
- `WeaponSpec.projectile` extendido con string `tracerPreset` (`"rocket" | "plasma" | "grenade" | ""` = sin tracer). Engine-generic: cada juego define sus presets en `Projectile::tracerPresetFromName(name)` helper.

### Sub-tarea 7 — Tests + docs + commit + tag

- Tests nuevos esperables ~15-20: CameraShake decay + trigger + state lifecycle / triggerCameraShake / triggerPainReaction / Crosshair gap computation / Projectile tracer preset spawn.
- ESTADO/HITOS/DECISIONS update + Cierre del plan.
- Commit con sección "Chequear:" (entrar a Play → disparar → ver muzzle flash + crosshair shake; pegar al dummy → hit marker cyan; recibir damage Lua → vignette + pitch wobble; lanzar rocket → estela de humo gris; explosion → screen shake fuerte).
- Tag `v3.6.0-fase4-hito6`.

---

## Métricas de éxito

- Disparar shotgun → ves muzzle flash + crosshair se abre + screen shake chico.
- Apuntar al dummy + click → hit marker cyan + dummy daña.
- Lanzar rocket → estela gris siguiendo el proyectil + screen shake al explotar.
- `health.damage("player", 25)` → vignette + pitch wobble.
- Suite verde, 0 regresión.

---

## Backlog (NO entra a F4H6)

- **F4H6.1** — UserSettings sliders (intensity 0-100% per efecto). Si playtests piden tuning.
- **F4H6.2** — Crosshair velocity-based opening (CS-style: el crosshair se abre al moverte rápido).
- **F4H6.3** — Killcam / death cam (cuando F4H10 ragdoll del enemy esté listo).
- Hit marker direccional (arc en lugar de cyan flash) — convención Apex.
- Damage indicator direccional (arc en pantalla apuntando al atacante).
- Particle muzzle flash con color específico por arma (hoy todos amarillo-blanco).

---

## Riesgos

- **Camera shake mareo**: HL/COD sutil debería evitarlo, pero validar con playtest. Si molesta, agregar toggle en UserSettings (F4H6.1).
- **Pain reaction interferencia con apuntar**: el pitch wobble podría desviar el aim. Mitigación: amplitud chica (2°) + duración corta (0.25s) + NO mover yaw.
- **Tracer rendimiento**: 30-80 partículas/segundo por proyectil. Con 10 proyectiles simultáneos = 600+ partículas. Validar FPS en stress test.

---

## Cierre — 2026-05-31 (tag `v3.6.0-fase4-hito6`)

**Suite full 1418/12240 verde** (+14 cases / +40 asserts vs F4H5: 1404 → 1418). 0 regresión.

### Entregables

1. **`HudState` extendido** (`src/engine/game/state/GameState.h`) con 8 fields nuevos: `shake_amp/t/max_t`, `pain_pitch_amp/t/max_t`, `pain_roll_offset`, `crosshair_spread_deg`.
2. **`GameState::triggerCameraShake(amp, dur)`** + **`triggerPainReaction()`** con anti-spam (timer > 50ms / > 100ms respectivamente) en `src/engine/game/state/GameState.cpp`. Determinismo xorshift32 para axis shake + roll pain.
3. **`FpsCamera::setShakeOffset(vec3)` + `setPainOffset(pitch, roll)`** + miembros `m_shakePosOffset/m_painPitchOffset/m_painRollOffset` (`src/engine/scene/core/FpsCamera.{h,cpp}`). `forward()` aplica pain pitch; `viewMatrix()` aplica shake position + rotated up para pain roll.
4. **Muzzle flash** en `Weapon::fire` (`src/engine/gameplay/weapon/WeaponSystem.cpp`) — `ParticleBurst` naranja-amarillo post-sound (hitscan + projectile spawn branches).
5. **Tracer trail proyectil** en `Weapon::fire` projectile branch — `ParticleEmitter` adjunto al projectile entity con preset heuristic por `spec.displayName` (rocket→gris humo / plasma→cian aditivo / grenade→naranja gravity sparks).
6. **`Projectile::tickSystem` retorno void → `TickStats{explosionCount, damageTargetsHit, lastExplosionCenter}`** (`src/engine/gameplay/projectile/ProjectileSystem.{h,cpp}`). `applySplashDamage` void → `int` (count entities dañadas).
7. **Bridge `EditorApplication_Run::tickSystems`** polling:
   - Muzzle flash → trigger ya en `Weapon::fire`.
   - Hit marker → `if (fireResult.targetsHit > 0) triggerHitMarker()`.
   - Camera shake al disparar: `triggerCameraShake(0.015, 0.08)`.
   - Pain reaction → polling `hitFlashTimer` transition del player → `triggerPainReaction()` + `triggerCameraShake(0.05, 0.2)`.
   - Projectile stats → `triggerCameraShake(0.2 * falloff, 0.4)` con distancia falloff hasta 10m + hit marker si hit + `crosshair_spread_deg` sync desde recoil.
   - Camera offsets apply al final del frame: `cam.setShakeOffset(...)`, `cam.setPainOffset(...)`, reset a 0 si timers expiran.
8. **`GameOverlay::drawCrosshair`** modificado (`src/engine/game/overlay/GameOverlay.cpp`) para gap dinámico `gap = 3 + min(16, crosshair_spread_deg * 1.5)`.
9. **Asset cleanup**: `assets/meshes/CesiumMan.glb` eliminado (Fox.glb preservado: `test_scene_loader.cpp:56` lo referencia).
10. **14 tests nuevos verdes** en `tests/test_game_feel_f4h6.cpp` (40 asserts).

### Decisiones cerradas

- **D1** — Bundle 6 feedback layers en hito atómico (vs split por efecto).
- **D2** — Intensidad HL/COD sutil 80% inferior rango industria (vs Doom Eternal screen-wide / Quake hyper-arcade).
- **D3** — Procedural-only sin assets art (defer visual pass a Sub-fase 4.3 unificada per strategic deferral usuario).
- **D4** — Polling pattern bridge preserva decoupling engine→game (R4 F4H4).
- **D5** — Eje shake xorshift32 deterministic (vs `rand()` thread-safe + reproducible).
- **D6** — Tracer preset heuristic por `displayName` (vs field schema opt-in — defer a F4H6.1).

### Ajustes reactivos

- **R1** — `triggerCameraShake` anti-spam timer > 50ms (granadas multi-bounce stutter).
- **R2** — `Projectile::tickSystem` retorno void → `TickStats` para bridge polling.
- **R3** — Tracer NO se agregó al schema `.moodweapon` (heuristic cubre F4H5 demo).

### Strategic deferral del usuario (verbatim)

*"antes de lo visual falta algo mas en el sistema? cuando tengamos toda la logica implementada ahi podemos ver lo visual, ademas no quiero usar el cesium man, ese eliminalo, tenemos ya un npc de mixamo con algunas animaciones y podemos bajar mas"*

**Implicación arquitectónica**: Sub-fase 4.2 (F4H7-F4H12 enemies) cierra con cubos placeholder. Visual pass unificado en Sub-fase 4.3 después.

### Próximo hito

**F4H7** — Sub-fase 4.2 arranca: enemigo básico (EnemyComponent + state machine simple {Idle/Chasing/Attacking/Dead} + spawn via Crear Entidad). Logic-only.

