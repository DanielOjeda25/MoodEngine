# Plan — Hito 34: Cerrar deudas chicas (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 34 cerrado).

---

## Estado

**Hito 34 cerrado** (`v0.34.0-hito34`, suite **286/5555**). Scope acordado con el dev: tras cerrar deudas del editor en Hito 32, ahora las deudas chicas acumuladas en hitos previos. 5 bloques de bajo coste, todos visibles al usar el motor.

## Bloques cerrados

- [x] **Bloque A — Friction per-body en `RigidBodyComponent`** (deuda Hito 31): nuevo campo `friction = 0.5f` (default heredado del hardcode anterior). `PhysicsWorld::createBody` recibe `friction` como param opcional. Persistencia en `.moodmap` opcional — solo se escribe si difiere del default 0.5 (back-compat con archivos viejos). Inspector con slider undoable via `pushEditIfDone<f32>` (rango [0, 2]).
- [x] **Bloque B — Raycast con filtro `ignoredBodyId`** (deuda Hito 33): `PhysicsWorld::raycast(..., ignoredBodyId=0)` con default 0 = sin filtro. Implementación con `JPH::BodyFilter` subclase local (`IgnoreOneBodyFilter`). Lua expone el 4to argumento como `sol::optional<u32>` — los scripts existentes (3 args) siguen funcionando. Cero impacto en callers existentes.
- [x] **Bloque C — Coyote time + jump buffer** (deuda Hito 30): coyote window 100ms (saltar después de dejar el suelo), jump buffer 150ms (input ANTES de aterrizar igual gatilla). Detección de flanco `up→down` de SPACE (no hold). Lógica idéntica en `EditorPlayMode` y `PlayerApplication`. Cooldown de 0.2s del Hito 30 se mantiene como anti-double-jump.
- [x] **Bloque D — Headbob amp escalado con velocity** (deuda Hito 31): nuevo `m_horizSpeed01` per-frame normalizado contra `k_walkSpeed`. La amplitud final = `k_bobAmp * m_horizSpeed01` — caminando full-speed = bob completo, crouched (~0.5) = bob sutil, quieto = sin bob. Paridad editor/player.
- [x] **Bloque E — Wire-up undo de widgets selectos del Inspector** (deuda Hito 32): cableado `pushEditIfDone<T>` en 7 widgets más editados (LightComponent.color/intensity/radius, RigidBodyComponent.halfExtents/mass, ParticleEmitterComponent.emitRate/gravityFactor). Plus el friction del Bloque A = 8 widgets totales. Los 30+ restantes siguen como pendiente.
- [x] **Bloque F — Tests + cierre**: 5 tests nuevos:
  - 2 en `test_scene_serializer.cpp`: friction custom round-trip (0.05 hielo) + friction default no-persiste-en-JSON.
  - 3 en `test_raycast.cpp`: ignoredBodyId saltea al body cercano (pega al de atrás), ignoredBodyId del único body devuelve miss, ignoredBodyId == 0 (default) no filtra nada.
  - Suite total **286/5555** (antes Hito 33 cerrado: 281/5535).
  - Tag `v0.34.0-hito34`.

## Lo que NO se cubrió

- **Tests headless de coyote/jump buffer y headbob velocity scale** (Bloques C, D): son features de feel/UI con dependencia de input + frame timing. Verificación visual del dev. Los headless mockearían SDL_GetKeyboardState + multiples frames — overhead sin valor proporcional.
- **30+ widgets restantes del Inspector** (heredado de Hito 32). Patrón uniforme — sigue como pendiente menor.
- **Setter runtime de friction**: editar friction en Play Mode no se aplica al body ya materializado. El re-Play sí lo aplica. Documentado en el header de `RigidBodyComponent`.
- **Filtro de raycast por layer/tag**: solo `ignoredBodyId`. Sin filtros tipo "solo enemies" o "ignore static". Suficiente para "ignore self".
- **Triggers detectan dynamic bodies / NPCs**, **emisión por shape**, **modelos `.obj` con `.mtl`**: pendientes medianos que quedan para hito gameplay/asset futuro.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-01):
- Friction default 0.5 + persistencia opcional (sin bump de schema mayor).
- Coyote 100ms / jump buffer 150ms hardcoded (no per-proyecto en v1).
- Headbob amp con scaling lineal de velocity (no ease-in/out).
- Raycast con `BodyFilter` subclase local en lugar de `IgnoreSingleBodyFilter` de Jolt (portabilidad).

---

## Verificación visual del dev (criterios cumplidos)

- Caja física empujada desde el char contra superficies de fricción 0.05 (hielo) sigue resbalando lejos; misma caja sobre superficie 0.5 (default) se detiene en pocos metros.
- Raycast desde un body que se ignora a sí mismo encuentra el wall behind sin autodetectarse.
- Saltar 50ms después de dejar un borde (correr off platform) sigue gatillando el salto (coyote OK).
- Apretar SPACE 100ms antes de aterrizar y soltar antes del touchdown igual hace que el char salte al tocar el suelo (buffer OK).
- Headbob al correr full-speed se siente igual que antes; al caminar en crouch (~50% speed) la cámara baja la amplitud notablemente; quieto = sin bob.
- Editar light color/intensity/radius o rigid body mass/halfExtents desde el Inspector → Ctrl+Z revierte el drag completo.
