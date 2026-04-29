# Plan — Hito 29: Particle system

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 28 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 28 cerrado** (`v0.28.0-hito28`, suite **246/5431**). Tras 4 hitos consecutivos de polish editor, el roadmap vuelve a features de gameplay. Particle system estaba como Hito 24 en el roadmap macro original (`HITOS.md` sección "Estado") — lo recuperamos ahora.

---

## Objetivo

Sistema de partículas CPU básico utilizable desde el editor y persistido en `.moodmap`. Cubre los efectos visuales más comunes: fuego, humo, sparks, niebla local, polvo. Nada exótico (sim físico, GPU compute) — eso es Hito futuro.

API esperable desde el editor:
- `Ayuda > Agregar partículas demo` → spawnea un emisor de fuego en el origen.
- Inspector con `ParticleEmitterComponent`: rate (particles/sec), lifetime (s), velocity range, size range, color over time, texture (sampler2D, billboard hacia la cámara).
- Persistencia en `.moodmap` (schema v9).

API esperable desde Lua: **no en V1**. Si el dev quiere spawnear via script, trigger en un Hito posterior.

---

## Bloques

### Bloque 1 — `ParticleSystem` headless (CPU, struct of arrays)

- [ ] `engine/render/ParticleEmitter.h` con `ParticleEmitterComponent` (rate, lifetime, velocity min/max, size min/max, color start/end, gravityFactor, textureId, maxParticles).
- [ ] `engine/render/ParticleSystem.{h,cpp}`: por entidad con emisor, struct of arrays internas (positions, velocities, ages, sizes, colors, alive). `update(scene, dt)`:
  - Por cada emisor: spawnea `rate * dt` partículas (acumulador para tasas fraccionarias).
  - Por cada partícula viva: `age += dt`, `pos += vel * dt`, `vel.y += gravity * dt` si aplica, evalúa color/size en función de `age/lifetime`. Si `age >= lifetime`, marca como muerta.
  - El struct de arrays se agrupa en una pool por emisor (tamaño max = `maxParticles`).
- [ ] Tests headless: emisor con rate=10/s spawneará 5 en 0.5s; partícula con velocity (0,1,0) y gravity=-9.81 sigue trayectoria; lifetime=1s mata a la partícula al `update(1.001)`.

### Bloque 2 — Render con billboards + alpha blending

- [ ] `shaders/particle.{vert,frag}`: vertex shader genera quad alineado a la cámara (4 vértices por partícula instanciadas via `gl_VertexID` y atributo per-instance del struct of arrays). Fragment con `texture()` * color + alpha discard si <0.01.
- [ ] `OpenGLParticleRenderer` (alguna ubicación bajo `engine/render/opengl/`) que sube los SoA a un VBO dinámico (orphan buffer + glBufferSubData) y dibuja con `glDrawArraysInstanced`.
- [ ] Integración en `SceneRenderer::renderSceneToViewport`: pase de partículas DESPUÉS de la geometría opaca (PBR + skinneada) y ANTES del post-process. Depth test ON pero **depth write OFF** (clásico para semitransparentes). Blending `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` (o aditivo opcional via flag del componente).
- [ ] Smoke visual: spawnear un emisor en el editor y ver partículas billboard renderizadas con la textura.

### Bloque 3 — UI Inspector + spawner demo

- [ ] `InspectorPanel`: nueva sección "Particle Emitter" con DragFloat para rate, lifetime, velocity range, size range, ColorEdit4 para colorStart/colorEnd, drag de textura desde Asset Browser al slot.
- [ ] `Ayuda > Agregar partículas de fuego demo` (`processSpawnFireParticlesRequest`): spawnea entidad "Fuego" en (0, 0.5, 0) con un emisor preset (rate=60, lifetime=1.5, vel +Y 1±0.5, color naranja→amarillo, gravityFactor=-0.1). Cableado a `pushCreatedEntities` para que sea undoable (Hito 28).
- [ ] Asset nuevo: `assets/textures/particle_fire.png` (chispa redonda blanca en alpha — generable con `tools/gen_particle_textures.py` o manual).

### Bloque 4 — Persistencia en `.moodmap`

- [ ] `SavedParticleEmitter` en `SceneSerializer.h` con todos los campos del componente.
- [ ] `EntitySerializer.cpp`: serializa/parsea bloque `particle_emitter`.
- [ ] `SceneLoader.cpp::applyOneEntity`: aplica el componente.
- [ ] Filtro de `SceneSerializer::save`: incluir entidades con `ParticleEmitterComponent` (ya tiene Mesh/Light/RigidBody/Environment/Script — sumar este).
- [ ] `k_MoodmapFormatVersion` 8 → 9. Archivos v8 sin `particle_emitter` cargan igual.

### Bloque 5 — Tests + cierre

- [ ] `tests/test_particle_system.cpp`: tests del Bloque 1 (rate, lifetime, gravity), round-trip en `.moodmap`.
- [ ] Smoke visual: spawnear el demo, mover la cámara, verificar billboards alineados.
- [ ] Verificar que persiste/carga (guardar proyecto, cerrar, reabrir, las partículas siguen emitiendo).
- [ ] Commits atómicos: `feat(render)`, `feat(editor)`, `feat(serialization)`, `test(render)`.
- [ ] Tag `v0.29.0-hito29`.
- [ ] Crear `docs/PLAN_HITO30.md`.
- [ ] Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.

---

## Decisiones a tomar

- [ ] **CPU vs GPU**: V1 es CPU (struct of arrays + upload por frame). GPU compute queda para un Hito posterior si la performance no alcanza.
- [ ] **Pool por emisor vs pool global**: V1 es per-emisor con `maxParticles` configurable. Cap del runtime: ~1024 partículas por emisor parece sensato.
- [ ] **Sorting**: V1 NO sortea partículas por depth (orden de spawn). Para alpha blend correcto haría falta sort back-to-front. Trigger: si el dev nota artifacts visuales en escenas con varios emisores superpuestos.
- [ ] **Color over time**: V1 = lerp lineal entre `colorStart` y `colorEnd` por `age/lifetime`. Si más adelante se quiere curva (ramp), agregar un componente de gradient como en Unity.

---

## Riesgos identificados

- **Performance del upload por frame**: orphan buffer + glBufferSubData es estándar pero puede saturar con > 10k partículas/frame. Mitigación: capear `maxParticles` por emisor + warning en log.
- **Alpha blending con depth write off**: típico clásico. Si el dev mete partículas opacas (alpha=1) por error, depth write off las hace mezclar feo. Mitigación: docs en el componente explicando que las partículas asumen semi-transparencia.
- **Forward+ del Hito 18 ignora partículas**: las point lights afectan geometría opaca via tiles; partículas no deberían sufrir lighting (van con su color custom). Mitigación: shader de partículas no usa light grid — auto-resuelto.
