# Plan — Hito 36: Cerrar lo que arrastra (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 36 cerrado).

---

## Estado

**Hito 36 cerrado** (`v0.36.0-hito36`, suite **291/5574**). Quinto hito seguido cerrando deudas (32 → 33 → 34 → 35 → 36). Los últimos pendientes barribles del scope previo: undo del drop de textura (Hito 35 A), `u32` en variant del tracker + maxParticles undoable (Hito 32→35), y `on_trigger_stay` callback (Hito 33). Próximo hito puede arrancar feature pesado con piso limpio.

## Bloques cerrados

- [x] **Bloque A — Undo del drop de textura sobre material slot** (deuda Hito 35 A): el handler en `InspectorPanel` ahora empuja un `EditPropertyCommand<u32>` (MaterialAssetId == u32) al HistoryStack en lugar de asignar directo. Setter captura `slotIndex` por valor y muta `mr.materials[slotIndex]`. Reusamos el comando templado existente — sin clase nueva. Fallback a asignación directa si no hay history disponible.
- [x] **Bloque B — `u32` en variant del `InspectorEditTracker` + undo de `maxParticles`** (deuda Hito 32 → 35): agregado `u32` al `std::variant<...>` del tracker. Cableado `pushEditIfDone<u32>` en el slider DragInt de `ParticleEmitterComponent::maxParticles`. El setter del comando reaplica el cleanup completo de la pool runtime (alive/positions/velocities/ages/lifetimes/aliveCount) — sin esto, undo dejaría índices stale ≥ la nueva capacidad.
- [x] **Bloque C — `on_trigger_stay` callback per-frame** (deuda Hito 33): `TriggerSystem::update` ahora dispatcha `on_trigger_stay` cada frame que `playerInside == true` y NO hubo flank (frame del enter dispatcha enter, los siguientes mientras adentro dispatchan stay). Scripts que no definan `on_trigger_stay` siguen funcionando — `dispatchEvent` ya hace miss silencioso.
- [x] **Bloque D — Tests + cierre**:
  - 2 tests nuevos en `test_edit_property_command.cpp`: `EditPropertyCommand<u32>` con setter que indexa slot (Bloque A) + setter complejo que limpia pool runtime (Bloque B).
  - 2 tests nuevos en `test_trigger_system.cpp`: dispatch de stay cada frame que el player sigue dentro (Bloque C) + NO dispatcha stay cuando está fuera.
  - Suite total **291/5574** (antes Hito 35 cerrado: 287/5561).
  - Tag `v0.36.0-hito36`.

## Lo que NO se cubrió

Todo el scope chico que quedaba arrastrando se cerró. Lo que queda son items **medios o sin trigger concreto**, candidatos a aparecer en hitos futuros si el dev los necesita:

- **Triggers detectan dynamic bodies / NPCs** (Hito 33): coste medio, fuera de scope chico.
- **Trigger AABB rotation (OBB)** (Hito 33): coste medio.
- **Setter runtime de friction** (Hito 34 A): el cambio del slider hoy se aplica al re-Play.
- **Coyote/jump buffer per-proyecto** (Hito 34 C): hardcoded por ahora.
- **Filtro raycast layer/tag genérico** (Hito 34 B): solo `ignoredBodyId` cubre "ignore self".
- **DragFloatRange2 widgets undoables** (Hito 32→35): lifetime/size del ParticleEmitter — requeriría `std::pair<f32,f32>` en variant.
- **Combos estructurales del Inspector** (type/shape/clipName): cambios de schema, no caben en `pushEditIfDone<T>`.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-01):
- Reusar `EditPropertyCommand<T>` con T=u32 para el undo del drop de textura — no nuevo comando.
- `on_trigger_stay` se dispatcha desde el frame siguiente al enter (frame del enter solo dispatcha enter).

---

## Verificación visual del dev (criterios cumplidos)

- Drop de textura sobre material slot → Ctrl+Z restaura el material previo. Otras entidades que compartían el material original siguen intocadas.
- Editar `maxParticles` en el slider → Ctrl+Z restaura el valor anterior y resetea la pool runtime (no hay índices stale).
- Trigger demo con script que define `on_trigger_stay` loguea cada frame que el player sigue dentro; al salir, deja de loguear.
- Suite 291/5574 sin regresiones.
