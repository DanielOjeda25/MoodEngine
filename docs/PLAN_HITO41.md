# Plan — Hito 41: Save/Load extendido (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 41 cerrado).

---

## Estado

**Hito 41 cerrado** (`v0.41.0-hito41`, suite **315/6591**). Save/Load (Hito 38 A) extendido para persistir el state runtime que V1 dejó afuera: snapshots de Dynamic bodies + globals Lua filtradas. Schema bumpea a v2 (back-compat verificado: archivos v1 cargan OK, los nuevos campos quedan vacíos).

## Bloques cerrados

- [x] **A — Snapshots de Dynamic bodies en `.moodsave`**: nueva struct `BodySnapshot { entityTag, position, rotationQuat, linearVelocity, angularVelocity }` agregada al `SaveData`. Save itera entidades con `RigidBodyComponent::Type::Dynamic + bodyId != 0 + tag != ""` y captura pose + vel via nuevos getters de `PhysicsWorld`. Load matchea por tag (estable entre sesiones). Schema v2 con array `bodies` opcional.
- [x] **B — Lua globals filtradas en `.moodsave`**: nueva struct `ScriptGlobalsSnapshot { scriptPath, globals: map<name, ExposedValue> }`. Filtro estricto: solo number/bool/string/vec3 (tabla 1-indexed con 3 floats). Funciones, tables genéricas, userdata se omiten silenciosos. `ScriptSystem` gana `captureGlobals(entity)` y `restoreGlobals(entity, map)`. Si el script aún no se cargó al `restoreGlobals`, los pendings se aplican en el próximo `update()` post-carga. Reservados los nombres internos (`engine`, `log`, `hud`, `physics`, `self`, `onUpdate`, `on_trigger_*`, libs Lua) para no spammear.
- [x] **C — PhysicsWorld vel getters/setters**: `bodyLinearVelocity`, `bodyAngularVelocity` (const, vía `GetBodyInterfaceNoLock`), `setBodyLinearVelocity`, `setBodyAngularVelocity`. Plus `bodyPositionRot(bodyId, outQuat) → vec3` y `setBodyPositionRot(bodyId, pos, quat)` para snapshot completo de pose.
- [x] **D — Tests + cierre**:
  - 3 tests nuevos en `test_save_load.cpp`: round-trip de bodies + round-trip de script globals + back-compat de v1 file con campos v2 vacíos.
  - Suite total **315/6591** (antes Hito 40 cerrado: 312/6564).
  - Tag `v0.41.0-hito41`.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-02):
- **TAG como identifier de body** en saves (bodyId raw es volátil entre sesiones, tag es estable).
- **Lua globals filtradas estrictamente** por tipos del `ExposedValue` variant; reserved names list para evitar capturar bindings del runtime.
- **Schema bump v1 → v2** con back-compat: loader acepta v1 (campos nuevos quedan vacíos).

---

## Riesgos y mitigaciones aplicadas

- **Tags duplicados**: si dos entidades comparten tag, el snapshot se escribe pero al load solo se aplica al primero encontrado. Documentado como limitación V1.
- **Script aún no cargado al restoreGlobals**: `m_pendingGlobals` stash del state, aplicado en próximo `update()`.
- **Vec3 detection en globals Lua**: heurística `tabla[1..3] == f32 + size==3`. Si un script usa tabla {1,2,3,extra=true} se omite (no encaja).

## Verificación visual del dev (criterios cumplidos)

- F5 con caja física empujada → save guarda pose + linear/angular velocity. Load (desde el menu) restaura → caja vuelve a la posición exacta del save con la misma velocidad.
- Script con `local hp = 75` declarado como global → save lo captura. Load → script ve `hp = 75` al onUpdate siguiente.
- Cargar un `.moodsave` v1 (sin bodies/globals) en MoodPlayer v2 → carga OK, sin warnings.
- Suite 315/6591 sin regresiones.
