# Plan — Hito 30: Player Character Controller con Jolt (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 30 cerrado).

---

## Objetivo

Reemplazar el `moveAndSlide` AABB del Hito 4 (que usa `PlayerApplication` y `EditorApplication` en Play Mode) con `JPH::CharacterVirtual` de Jolt. Es el último gran agujero entre el motor actual y "puede ser un FPS de verdad" — sin esto el jugador no salta, no sube escaleras, no slide en pendientes, y la fisica del player corre desconectada de los `RigidBody` del Hito 12.

API esperable post-hito:
- WASD + mouse mueve via `JPH::CharacterVirtual` con capsule de 0.4m radio × 1.8m alto.
- **Space** = salto (con cooldown anti-hold).
- **LCtrl** = crouch (capsule reduce a 1.0m alto).
- Step-up automático para escalones < 0.4m.
- Slope sliding: superficies > 45° resbalan.
- Capsule colisiona con `RigidBody::Static` (tiles del mapa) y empuja `Dynamic` (cajas físicas).

---

## Criterios de aceptación

### Automáticos

- [x] Compila sin warnings nuevos.
- [x] Tests headless de la API de `PhysicsWorld::createCharacter` / `setCharacterMovement` / `characterPosition` / `isCharacterOnGround` (al menos 4 casos).
- [x] Suite total ≥ 261.

### Visuales

- [x] Editor + Player: WASD mueve sin atravesar paredes (mismo comportamiento que Hito 4 en plano).
- [x] Espacio salta y cae por gravedad.
- [x] LCtrl encoge la capsule visualmente (cámara baja).
- [x] Spawnear caja física + caminar contra ella → la caja se mueve (player la empuja).
- [x] Spawnear demo de sombras (escalón 0.1m) → player sube sin saltar.

---

## Bloque 1 — API en `PhysicsWorld` para `JPH::CharacterVirtual`

- [x] `PhysicsWorld::createCharacter(initialPos, capsuleHalfHeight, capsuleRadius)` → devuelve `u32` (handle estable). Internamente construye `JPH::CharacterVirtualSettings` + `CharacterVirtual`.
- [x] `setCharacterMovement(id, desiredVelocity)` — el caller calcula la velocidad horizontal+vertical deseada (post-input + gravedad acumulada).
- [x] `characterPosition(id)` / `setCharacterPosition(id, pos)` — lectura/teleport.
- [x] `isCharacterOnGround(id)` → bool. Lo usa el caller para resetear vertical velocity al aterrizar y permitir un nuevo salto.
- [x] `destroyCharacter(id)` — cleanup, idempotente.
- [x] El `step(dt)` existente sigue stepeando la sim de bodies; los characters se actualizan dentro del mismo `step` via `CharacterVirtual::ExtendedUpdate`.
- [x] Tests headless: crear character + step → no crash. Setear movement (1, 0, 0) por 1s → posición avanza ~1m. Crear character sobre static body → isOnGround=true tras 1 step.

## Bloque 2 — Cablear al `PlayerApplication` y `EditorApplication`

- [x] `PlayerApplication`: en el ctor, crear character en la posición de spawn de `m_playCamera`. En `updatePlayerInput`: calcular `desired` (input WASD * speed + verticalVel), llamar `setCharacterMovement`, leer `characterPosition` + setear `m_playCamera.setPosition`.
- [x] Acumular gravedad (`verticalVel += -9.81 * dt`) cada frame; al `isCharacterOnGround()` resetear a 0.
- [x] `EditorApplication::updateCameraInput` (Play branch): mismo wireado para que el editor en Play Mode también use el character.
- [x] Eliminar el `moveAndSlide` + `k_playerHalfExtents` del player (queda solo para tests si hace falta).

## Bloque 3 — Salto + crouch

- [x] Salto: tecla Space + on-ground → `verticalVel = 5.5 m/s` (≈1.5m de altura). Cooldown 0.2s anti-hold para evitar saltos repetidos al mantener apretado.
- [x] Crouch: LCtrl mantiene → recrear el character con halfHeight 0.5m (en lugar de 0.9m). Soltar restaura, salvo que haya techo encima (raycast hacia arriba; si hit, queda crouched).
- [x] Camera height sigue al character (eye = position + halfHeight - 0.1m).

## Bloque 4 — Tests + cierre

- [x] `tests/test_character_controller.cpp`: 4–6 casos (create/destroy, mover sobre piso → posición avanza, gravedad sin piso → cae, isOnGround flags, salto en piso vs en aire).
- [x] Smoke manual: WASD, Space, LCtrl en el demo de sombras + caja física empujable.
- [x] Commits atómicos: `feat(physics)`, `feat(player)`, `feat(editor)`, `test(physics)`.
- [x] Tag `v0.30.0-hito30`.
- [x] Crear `docs/PLAN_HITO31.md`.
- [x] Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.

---

## Decisiones a tomar

- [x] **Capsule shape vs Cilindro+Esfera**: `JPH::CharacterVirtual` soporta cualquier `JPH::Shape`. Default = capsule. **Decidido: capsule** (idiomático, suave en step-up).
- [x] **`CharacterVirtual` vs `Character`**: la primera ("virtual") es kinematic-style con resolución manual de slide; la segunda usa rigid body real. V1 = `CharacterVirtual` por estabilidad y control fino del slide.
- [x] **Mantener AABB `moveAndSlide` para tests**: `tests/test_collision.cpp` lo usa. Lo dejamos disponible (no lo deletear) hasta que el dev confirme que no se necesita.

---

## Riesgos identificados

- **Curva de Jolt**: `CharacterVirtual` requiere settings cuidadosos (max slope, step-up vector, contact callback). Mitigación: empezar con valores típicos de Jolt sample y ajustar.
- **Drift entre cámara y character**: si la cámara guarda posición propia y el character otra, pueden desincronizarse. Mitigación: `m_playCamera.setPosition(characterPos + eyeOffset)` cada frame, sin guardar duplicados.
- **Crouch con techo bajo**: Jolt no resuelve este caso solo. Hace falta un overlap test antes de uncrouch. Mitigación: documentar y testear en el demo de sombras.
