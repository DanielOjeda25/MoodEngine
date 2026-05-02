# Plan — Hito 40: Cerrar pendientes chicos/medios post-v1.0.0 (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 40 cerrado).

---

## Estado

**Hito 40 cerrado** (`v0.40.0-hito40`, suite **312/6564**). Tras `v1.0.0` (cierre Fase 1), el dev pidió cerrar TODO lo pendiente documentado antes de la recapitulación. 11 de 14 bloques cerrados; 3 pendientes con justificación documentada.

## Bloques cerrados

- [x] **A — Cone particle shape con axis custom**: `emissionConeAxis` vec3 default +Y. Sample en +Y local + rotación Rodrigues hacia el axis target. Persistencia opcional (no escribe si == default).
- [x] **B — OBB trigger preview en debug draw** 🎯: nuevo loop en `EditorRenderPass` que dibuja las 12 aristas del cubo del trigger usando `drawLine`, respetando rotation del Transform.
- [x] **C — Layers custom**: cubierto vía `TagComponent` + `layerMask` raycast (Hito 39 C). Documentado en `PhysicsWorld.h` como decisión permanente — extender el enum no aporta hasta que emerja necesidad de cambiar la matriz Jolt.
- [x] **D — Setter runtime mass/halfExtents**: `setBodyHalfExtents` via `BodyInterface::SetShape` con `inUpdateMassProperties=true` (recrea collider, preserva pose, recalcula inertia). `setBodyMass` quedó como **stub documentado** — la API directa de Jolt para cambiar mass de body activo crashea con SEH; el caller debe usar `setBodyHalfExtents` para cambios de mass. Pendiente real para Fase 2 (con versión de Jolt actualizada).
- [x] **E — DragFloatRange2 undoables**: agregado `std::pair<f32, f32>` al variant del `InspectorEditTracker`. Cableados los DragFloatRange2 de `lifetime` y `size` del ParticleEmitter con undo.
- [x] **F — Combos estructurales con undo**: cableados con `EditPropertyCommand<u32>` (atómico, sin tracker drag-end): RigidBody type, RigidBody shape, Light type. Animator clipName con `EditPropertyCommand<std::string>` (setter resetea time = 0 al cambiar).
- [x] **G — Coyote/jump-buffer per-proyecto** 🎯: agregados `Project::coyoteWindowSec` y `jumpBufferWindowSec` (defaults 0.10 y 0.15). Persistencia opcional en `.moodproj`. `EditorPlayMode` y `PlayerApplication` los leen del project cargado.
- [x] **H — Sort partículas bucket-Z** 🎯: cambio de `std::sort` por `std::stable_sort` con quantize a buckets de 0.10m en view-space Z. Elimina el flicker visible cuando dos partículas tienen Z casi igual entre frames.
- [x] **I — `localSpace` con rotation/scale del entity** 🎯: el render multiplica las positions por `tf.worldMatrix()` completa (no solo translation). Particulas en un emisor rotado siguen la rotación.
- [x] **J — Compactación cross-frame Inspector**: cleanup por documentación. Auditado: ningún widget actual escapa el patrón `IsItem*` que ya garantiza un comando por gesto. Aceptado como NO requerido.
- [x] **K — Tests headless de `setBodyFriction`/`setBodyMass`/`setBodyHalfExtents`**: 3 tests nuevos en `test_raycast.cpp` (no-op para invalid + acepta valor sobre Dynamic + halfExtents API contract).
- [ ] **L — Tests headless de coyote/jump-buffer**: NO HECHO. Requiere refactor de `EditorPlayMode` y `PlayerApplication` a pure functions (la lógica está acoplada a `SDL_GetKeyboardState` per-frame). Aceptado como pendiente Fase 2.
- [ ] **M — Tests headless de headbob velocity**: NO HECHO. Misma razón. Aceptado como pendiente Fase 2.
- [x] **N — Cierre + tag**: docs + tag `v0.40.0-hito40`.

## Lo que NO se cubrió (Hitos 41 + 42)

- Save/Load extendido con snapshots de Dynamic bodies (Jolt body pose + lin/ang vel) + Lua globals arbitrarias.
- Editor de materiales visual (panel dedicado con preview esférico).

## Lo que NO se cubrió (Fase 2 si emerge necesidad)

- `setBodyMass` real (no-stub) — requiere versión de Jolt que exponga API limpia.
- Tests headless de coyote/jump-buffer y headbob — requiere refactor a pure functions.
- Layers custom además de Static/Moving — requiere extender matriz Jolt.

---

## Verificación visual del dev (criterios cumplidos)

- 🎯 **B**: F1 toggle del debug. Trigger rotado 45° muestra OBB rotado en debug (no AABB axis-aligned).
- 🎯 **G**: editar `coyoteWindowSec` en el `.moodproj` a 0.30 → coyote más generoso al saltar de bordes. Reload del proyecto necesario.
- 🎯 **H**: dos emisores de humo overlapping no flickerea entre frames (estable).
- 🎯 **I**: emisor con `localSpace=true` rotado 45° en Y → las partículas spawneadas en `+X local` aparecen en una diagonal world.
- Test suite **312/6564** sin regresiones.
- Editor + MoodPlayer compilan limpios.
