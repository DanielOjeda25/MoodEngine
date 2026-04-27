# Plan — Hito 18: Deferred / Forward+

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 18) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Escalar el pipeline de iluminación más allá del forward shading actual. El Hito 17 dejó un PBR forward que evalúa todas las luces en cada frag — funciona con la directional + ≤8 point lights, pero se cae con escenas de decenas de luces. El Hito 18 introduce uno (o ambos) de:

- **Forward+** (tile-based forward): cull las luces por tile en una compute pass + shader que loopea solo las que afectan el tile. Mantiene el forward (compatible con MSAA, transparencias, sin G-buffer extra de memoria).
- **Deferred shading**: G-buffer (albedo/normal/material/depth) + lighting pass que evalúa las luces por light-volume. Mejor para escenas con muchas luces, peor para transparencias y MSAA.

No-goals: clustered shading (Forward++ por slabs de profundidad), ray-traced shadows, GI baked.

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: light-tile assignment numérico (Forward+) o reconstrucción de world-space desde depth (Deferred).
- [ ] Cierre limpio.

### Visuales

- [ ] Demo "stress test luces": grid 8×8 de point lights con colores aleatorios sobre un plano texturizado. 30+ luces visibles a la vez sin caer del FPS objetivo.
- [ ] La sombra del Hito 16 sigue funcionando sobre el nuevo pipeline.
- [ ] El IBL del Hito 17 sigue funcionando sobre el nuevo pipeline.

---

## Bloque 0 — Decisión técnica + pendientes arrastrados

- [ ] **Spike:** elegir Forward+ vs Deferred para esta iteración. Documentar en DECISIONS.md.
  - Forward+ favorece compatibilidad con post-process actual (HDR, fog, transparencias futuras), MSAA.
  - Deferred favorece muchas luces densas, normal mapping integrado en G-buffer, rebuilds más rápidos al cambiar lighting.
  - Recomendación inicial: **Forward+** porque no rompe el path actual y reusa el shader PBR; Deferred queda para Hito 18.5 si Forward+ no escala.
- [ ] Pendientes arrastrados del Hito 17: AABB del MeshAsset (necesario para tighter light culling), normal mapping (más visible con muchas luces).

## Bloque 1 — Light grid (CPU side)

- [ ] `engine/render/LightGrid.h`: estructura tile-based 16×16 px. Por tile, un offset + count en una flat array global de light indices.
- [ ] `LightSystem::buildLightGrid(cameraView, cameraProj, framebufferSize)`: por luz, computar la AABB en screen-space + asignar a los tiles que toca.
- [ ] Test: validar que una luz centrada en una posición conocida cae en los tiles esperados.

## Bloque 2 — Light grid (GPU upload)

- [ ] SSBO o UBO con: `vec4[]` data de luces + `uvec2[]` (offset, count) por tile + `uint[]` light indices.
- [ ] `pbr.frag` extendido: `gl_FragCoord.xy / 16` → tile id → loop solo las luces del tile.
- [ ] El path "directional sin grid" del Hito 17 se mantiene para la sun (sigue en uniform).

## Bloque 3 — Demo + shadow integration

- [ ] "Ayuda > Stress test luces" spawnea 64 point lights (8×8 grid sobre el plano del demo de sombras) con colores procedurales.
- [ ] Verificar que el shadow map de la directional sigue funcionando (no debería romperse: el shadow pass es independiente del lighting pass).

## Bloque 4 — Tests + perf

- [ ] `tests/test_light_grid.cpp`: light → tile assignment para casos conocidos (luz en centro, luz en esquina, luz fuera del frustum).
- [ ] Profile: medir FPS antes/después con 64 luces. Idealmente ≥60 FPS en hardware del dev (Iris Xe).

## Bloque 5 — Cierre

- [ ] Smoke test visual completo.
- [ ] Actualizar `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- [ ] Tag `v0.18.0-hito18` + push.
- [ ] Crear `PLAN_HITO19.md` (Animación esquelética).

---

## Pendientes futuros que pueden aparecer en este hito

- **Compute shaders** para light culling. Si el target es solo Iris Xe + GTX 1660, GL 4.5 + compute shaders funciona. Si llega un hardware peor, fallback a CPU-side.
- **MSAA** dentro del scene FB HDR. Hoy no tenemos AA — Forward+ lo permite trivialmente, Deferred lo complica.
- **Cluster por profundidad (Z-bins)**. Hito 18.5 si Forward+ no alcanza para 200+ luces.
