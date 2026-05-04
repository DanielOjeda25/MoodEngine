# Plan — Hito 37: Cerrar 3 medianas pendientes (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 37 cerrado).

---

## Estado

**Hito 37 cerrado** (`v0.37.0-hito37`, suite **300/5927**). Sexto hito seguido cerrando deudas (32 → 33 → 34 → 35 → 36 → 37). Las 3 medianas pendientes barridas: PackageBuilder smart-pack (Hito 26), triggers que detectan dynamic bodies (Hito 33), emisión de partículas por shape (Hito 29). Próximo hito puede arrancar feature visible (save/load, main menu).

## Bloques cerrados

- [x] **Bloque A — PackageBuilder smart-pack** (deuda Hito 26): nuevo `cfg.smartPack` (default `true`). El builder ahora escanea cada `.moodmap` del proyecto + `.material` referenciados, recolecta paths lógicos (mesh, texturas, materiales, scripts, particle textures, prefabs, skybox) y copia solo esos al package. Whitelist defensiva: `textures/missing.png`, `audio/missing.wav`, `skyboxes/` recursivo entero (los shaders ya iban aparte). El `assets/` entero se vuelve fallback opcional con `cfg.smartPack = false`. Nueva función pública `collectReferencedAssetPaths(project, engineAssetsDir)` reutilizable para tests.
- [x] **Bloque B — Triggers detectan dynamic bodies** (deuda Hito 33): `TriggerSystem::update` ahora hace dos AABB-tests:
  1. Player char (Hito 33+36): dispatch `on_trigger_enter` / `on_trigger_exit` / `on_trigger_stay`.
  2. Cada entidad con `RigidBodyComponent` Dynamic + Kinematic (Static se ignora — no se mueve, no aporta valor): dispatch nuevos callbacks `on_trigger_body_enter(bodyId)` / `on_trigger_body_exit(bodyId)` / `on_trigger_body_stay(bodyId)` que reciben el `entt::entity` raw del body como número Lua.
  - Estado per-trigger guardado en `TriggerComponent::bodiesInside` (set, no serializado). Bodies destruidos entre frames se limpian automáticamente porque no aparecen en el set del frame actual → flank false→true del set previo dispara exit.
  - Nueva sobrecarga `ScriptSystem::dispatchEvent(entity, eventName, u32 arg)` para pasar args primitivos a Lua sin acoplar TriggerSystem a sol2.
- [x] **Bloque C — Emisión de partículas por shape** (deuda Hito 29): nuevo `ParticleEmitterComponent::EmissionShape { Point, Box, Sphere, Disc }` + `emissionShapeSize` (radio para Sphere/Disc, halfExtents iguales para Box). Default Point = comportamiento del Hito 29 (cero impacto en mapas viejos). Sphere/Disc usan rejection sampling para uniformidad. Persistencia en `.moodmap` opcional (solo se escribe si shape != Point). Inspector con combo + DragFloat condicional + undo del slider.
- [x] **Bloque D — Tests + cierre**:
  - 2 tests en `test_package_builder.cpp`: walk de `.moodmap` recolecta paths esperados + expansión de `.material` agrega texturas internas.
  - 3 tests en `test_trigger_system.cpp`: dynamic body entrando dispatcha `on_trigger_body_enter` + exit al salir + Static body es ignorado.
  - 4 tests en `test_particle_system.cpp`: Sphere spawnea dentro del radio + Disc mantiene Y constante en plano XZ + emission shape custom round-trip + emission shape default Point NO se persiste en JSON.
  - Suite total **300/5927** (antes Hito 36 cerrado: 291/5574).
  - Tag `v0.37.0-hito37`.

## Lo que NO se cubrió

- Save/Load gameplay state, main menu, settings persistidos: candidatos PLAN_HITO38.
- Polish menores acumulados (DragFloatRange2 widgets undoables, combos estructurales, etc.).

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-01):
- PackageBuilder smart-pack con whitelist defensiva (skyboxes/ + missing.*) sin bumpear schema.
- Triggers detectan bodies dispatchando con `entt::entity` raw como `u32` (sin acoplar TriggerSystem a sol2).
- Particle emission shapes con rejection sampling (Sphere/Disc) en vez de polar coords.

---

## Verificación visual del dev (criterios cumplidos)

- "Empaquetar proyecto" produce un paquete que contiene SOLO los assets referenciados por los .moodmap del proyecto + missing.* + skyboxes/ + shaders/. El zip pesa fracciones de lo que pesaba con el copy entero.
- Trigger demo + caja física empujada al AABB → script imprime `on_trigger_body_enter` con el id del body. Al sacar la caja → `on_trigger_body_exit`.
- Particle emitter con shape `Sphere`/radius 2.0 spawnea humo distribuido en una esfera de 4m de diámetro (no más todas saliendo de un punto).
- Save/cerrar/reabrir preserva shape + size del emisor.
- Suite 300/5927 sin regresiones.
