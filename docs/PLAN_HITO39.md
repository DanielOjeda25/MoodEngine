# Plan — Hito 39: Polish final + cierre Fase 1 (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 39 cerrado).

---

## Estado

**Hito 39 cerrado** (`v0.39.0-hito39` + `v1.0.0`, suite **308/6554**). **Fase 1 del proyecto terminada.** El motor recibe el tag final `v1.0.0` — listo para que el dev haga la recapitulación + arranque la Fase 2.

## Bloques cerrados

- [x] **Bloque A — Cone particle shape**: agregado `EmissionShape::Cone` al enum. Sample: cubic-root height (uniform sobre el volumen) + disc rejection sampling con radio decreciente linealmente con la altura. Axis fijo +Y; altura == base radio == `emissionShapeSize`. Persistencia + Inspector combo + serialización integrados con el patrón del Hito 37 C.
- [x] **Bloque B — OBB trigger** (rotation del Transform): nueva función `obbContainsWorldPoint` reemplaza el AABB axis-aligned previo. Construye matriz solo con position + rotation (ignora scale para mantener `halfExtents` como metros directos), invierte y testea el actor en espacio local. Si `rotationEuler == (0,0,0)`, resultado idéntico al AABB anterior (back-compat verificado por tests existentes).
- [x] **Bloque C — Filtro raycast por layer mask**: `PhysicsWorld::raycast(..., layerMask = 0xFFFFFFFFu)` con `LayerMaskFilter` subclase de `JPH::ObjectLayerFilter`. Bit 0 = Static, bit 1 = Moving. Lua expone como 5to argumento opcional. `IgnoreOneBodyFilter` ahora chequea `m_ignoredRaw == 0` antes de comparar (combina cleanly con layerMask sin ignored body).
- [x] **Bloque D — Setter runtime de friction**: `PhysicsWorld::setBodyFriction(bodyId, friction)` via `BodyInterface::SetFriction` + `ActivateBody`. Slider del Inspector + cambio en vivo durante Play (no requiere re-Play).
- [x] **Bloque E — Tests + docs Fase 1 + tag `v1.0.0`**:
  - 1 test nuevo en `test_particle_system.cpp` (Cone confina partículas dentro del volumen).
  - 1 test nuevo en `test_trigger_system.cpp` (OBB respeta rotation, punto que cae fuera del AABB axis-aligned cae dentro del OBB rotado 45°).
  - 1 test nuevo en `test_raycast.cpp` (layerMask filtra Static vs Moving vs nada).
  - Suite total **308/6554** (antes Hito 38 cerrado: 305/5947).
  - Tags `v0.39.0-hito39` (hito) + `v1.0.0` (cierre Fase 1).

---

## Fase 1 terminada

Tras 39 hitos:
- **Fundamentos** (1-5): editor shell, OpenGL pipeline, mundo grid, asset browser.
- **Edición y contenido** (6-10): serialización, ECS, Lua, audio, modelos 3D.
- **Motor adulto** (11-15): luces, Jolt physics, gizmos, prefabs, skybox/fog.
- **Visual moderno** (16-20): shadows, PBR, Forward+, animación, HUD.
- **Herramienta seria** (21-29): empaquetado, scripts workflow, pathfinding, exposed properties, hot-reload, asset pipeline, undo/redo, particles.
- **Gameplay scriptable** (30-37): char controller, polish del feel, Inspector undo (40+ widgets), raycasts, triggers, save/load.
- **Cierre Fase 1** (38-39): main menu del MoodPlayer, polish final.

El motor jugable está listo para producir demos FPS reales con progresión.

---

## Lo que NO se cubrió (Fase 2 o backlog post-v1.0.0)

- Save/Load extendido con snapshots de Dynamic bodies + Lua globals arbitrarias.
- Cone shape con direction custom (axis del cone). V1 = axis +Y.
- Layers custom definidos en el editor. V1 = solo Static/Moving binarios.
- DragFloatRange2 widgets undoables + combos estructurales del Inspector.
- Multiplataforma Linux (descartado del plan por el dev).
- Editor de materiales visual aparte (cubierto parcial vía Inspector).
- RmlUi (descartado de facto, ImGui cubre todo el editor).

La Fase 2 será definida por el dev tras la recapitulación.
