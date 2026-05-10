# PLAN HITO F2H40 — Fix físicas Floor scale-RigidBody desync

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.30.0-fase2-hito40`.
> **Origen**: bug físicas descubierto durante validación de F2H39.
> El dev no podía pararse en el suelo para ver los widgets dinámicos
> del HUD — caía infinito en Play Mode tras enlargar el Floor.

## Bug

Cuando el dev cambia `Transform.scale` de una entidad con `RigidBody`
(Floor, brushes, walls, dropped meshes, etc.) vía Inspector o gizmo,
el `RigidBody.halfExtents` (que define el shape de Jolt) NO se
sincroniza. El visual cambia pero la colisión queda al tamaño que
tenía cuando se creó el body — el player puede caer fuera o
atravesar paredes que ya no existen visiblemente.

**Reproducción**:
1. Crear proyecto nuevo. Floor por default tiene Transform.scale =
   `(mapW * tileSize, 0.1, mapH * tileSize)` y RigidBody.halfExtents
   = `(mapW * 0.5, 0.05, mapH * 0.5)`. Matchean.
2. Seleccionar Floor, escalar scale.x 2x via Inspector. Visual se
   ensancha, halfExtents queda en `mapW * 0.5` original.
3. Entrar a Play. Player spawnea encima del visual nuevo pero fuera
   del body original → cae infinito.

## Fix

**Auto-sync `halfExtents` desde `Transform.scale` para Box bodies**
en `EditorApplication::updateRigidBodies`. Sphere/Capsule mantienen
halfExtents independiente (el campo significa cosas distintas: radio,
altura).

**Mecánica**:

1. Agregar `glm::vec3 lastSyncedHalfExtents{0.5f}` a
   `RigidBodyComponent` (cache para detectar cambios).
2. En `updateRigidBodies`, después de materializar el body inicial
   (cuando `bodyId == 0`), agregar pase de re-sync sobre bodies
   existentes:
   - Si `shape == Box` AND `halfExtents != t.scale * 0.5` (epsilon):
     auto-update `halfExtents = t.scale * 0.5`.
   - Si `halfExtents != lastSyncedHalfExtents` (cualquier shape):
     `m_physicsWorld->setBodyHalfExtents(bodyId, shape, halfExtents)`
     + `lastSyncedHalfExtents = halfExtents`.

**Por qué `setBodyHalfExtents` y no destroy+recreate**: ya existe en
`PhysicsWorld.cpp:463`, llama `BodyInterface::SetShape` que preserva
pose + velocity + contacts. Importante para Dynamic bodies que se
escalan mid-frame (ej. via script o gizmo en Editor Mode).

**Por qué solo Box**: en Box, el campo `halfExtents` es realmente la
mitad del tamaño en 3 ejes — sincronizar con `t.scale * 0.5` es
intuitivo. En Sphere `halfExtents.x` es radio (no escalable
uniformemente desde un Transform.scale potencialmente no-uniforme).
En Capsule `halfExtents.x = halfHeight, halfExtents.y = radius` —
mismo problema. Si el dev quiere cambiar radio/altura de Sphere/
Capsule, lo edita en el Inspector field directamente y el caso 2 de
arriba lo sincroniza.

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 5 min | Se archiva en cierre |
| B | Implementación: agregar `lastSyncedHalfExtents` + lógica auto-sync en `updateRigidBodies` (EditorScene + PlayerApplication paths) | ~30 min | Edits en `Components.h`, `EditorScene.cpp`, `PlayerApplication_Frame.cpp` |
| C | Build + validación visual (proyecto nuevo + Floor scale 2x + Play → debe quedar parado) | ~10 min | El bug que descubrió el dev en F2H39 debe resolverse |
| D | Cierre: docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits + tag + push | ~20 min | Patrón estándar |

**Total estimado**: ~1h. Mini-hito chico.

## Riesgos y tradeoffs

- **Riesgo: regresión en escenas con Dynamic Box bodies que
  intencionalmente tienen halfExtents != scale*0.5**. Mitigación:
  buscar en el codebase casos así. La convención hasta hoy parece
  ser `halfExtents = scale * 0.5` (ver `EditorScene.cpp:104` Floor,
  `EditorScene.cpp:142` Tile, `DemoSpawners` físicas demo). Si
  emerge regresión, agregar opt-out flag `bool autoSyncBoxToScale =
  true` en RigidBodyComponent.
- **Tradeoff: overhead extra en updateRigidBodies** (compara
  halfExtents cada frame). Negligible (epsilon compare de 3 floats),
  pero a 1000+ entidades con RigidBody se nota. Mitigación: skip si
  `bodyId == 0` (ya materializado check) — solo entra al re-sync si
  existe.
- **Tradeoff: el caso F2H39 lateral fix (proyectos pre-Hito 12 sin
  RigidBody serializado)** y este caso (scale-halfExtents desync)
  son síntomas de la misma raíz: el body se construye una vez al
  materializar y no responde a mutaciones del Transform. Este fix
  resuelve el segundo; el primero ya fue resuelto en F2H39 con
  auto-add. Juntos cubren los dos vectores.

## NO entra en F2H40

- Sync de Transform.position / rotation al body (separado — ya hay
  pattern para Dynamic bodies vía PhysicsWorld pero no para Static
  bodies que el dev mueve en Editor).
- Sync de Sphere/Capsule halfExtents al Transform (no aplica
  semánticamente).
- Refactor mayor del PhysicsSystem para event-driven body updates
  (cuando emerja necesidad).

## Validación

- Crear proyecto nuevo. Default Floor tiene scale (24, 0.1, 24)
  matchea halfExtents (12, 0.05, 12).
- En Inspector, cambiar Transform.scale.x a 60 (2.5x). Visual se
  ensancha. Verificar log o Inspector que halfExtents se actualizó
  a (30, 0.05, 12).
- Entrar a Play. Player spawn a default (-4.5, 1.6, 7.5) — bien
  dentro del Floor enlargado. Debe pararse.
- Bonus: cambiar Floor.scale.y a 5 (Floor mucho más alto). Player
  debe caminar arriba sin atravesar.

## Cambio importante (anotar en cierre si aplica)

Si la validación destapa que algun demo o entity-creator del
codebase tenía `halfExtents != scale*0.5` intencionalmente, anotar
en DECISIONS.md la decisión + considerar el opt-out flag.
