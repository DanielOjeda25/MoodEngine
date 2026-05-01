# Plan — Hito 35: Cerrar deudas viejas (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 35 cerrado).

---

## Estado

**Hito 35 cerrado** (`v0.35.0-hito35`, suite **287/5561**). Tras el barrido de deudas chicas en Hito 34, el dev pidió seguir cerrando lo más viejo antes de empezar features pesados. 4 bloques de bajo coste sobre las deudas más antiguas no atacadas (Hito 26 + 32). Bonus: descubrimos un bug latente del Hito 26 al validar `.obj`+`.mtl`.

## Bloques cerrados

- [x] **Bloque A — Drop textura sobre material slot del Inspector** (deuda Hito 26): nuevo botón "Drop textura para reemplazar material" debajo de cada material slot del MeshRenderer. Acepta payload `MOOD_TEXTURE_ASSET` (existente del AssetBrowser). Al recibir el drop genera un material instance único (anti-contagio del Hito 25) via `AssetManager::createMaterialFromTexture(texId)` y lo asigna al slot correspondiente. Sin undo del cambio estructural por ahora — documentado como pendiente menor.
- [x] **Bloque B — Wire-up undo de los widgets restantes del Inspector** (deuda Hito 32 / 34): cableado `pushEditIfDone<T>` en 23 widgets nuevos:
  - **Camera** (3): fovDeg, nearPlane, farPlane.
  - **Environment** (6): fogColor, fogLinearStart, fogLinearEnd, fogDensity, exposure, iblIntensity.
  - **AudioSource** (4): volume, loop, playOnStart, is3D.
  - **Animator** (3): speed, playing, loop.
  - **ParticleEmitter** (7): emitting, additive, localSpace, velocityMin, velocityMax, colorStart, colorEnd.
  - Total acumulado Inspector con undo: 9 (Hito 32) + 8 (Hito 34) + 23 (Hito 35) = **40 widgets**. Quedan ~10 estructurales (combos type/shape, paths, ranges DragFloatRange2, DragInt) como pendiente menor con criterio "incluir solo edits escalares/vectoriales puros".
- [x] **Bloque C — Validar `.obj`+`.mtl`** (deuda Hito 26): creados `assets/meshes/cube_mtl.obj` + `cube_mtl.mtl` con `map_Kd ../textures/brick.png`. Test headless en `test_mesh_asset.cpp` que carga el cubo via AssetManager y verifica que al menos un material extrae albedo no-missing.
  - **Bug latente fixeado**: el path `(meshLogicalPath.parent_path() / mtlPath)` producía `meshes/../textures/brick.png` que el VFS rechazaba como leak. Fix: `lexically_normal()` normaliza a `textures/brick.png` antes de pasar a `loadTexture`. Aplica a CUALQUIER mesh con texturas en paths que escapen su carpeta — issue genérico del Hito 26 que solo se notaba al usar este patrón.
- [x] **Bloque D — Docs + cierre**: PLAN_HITO35 cerrado, PLAN_HITO36 abierto (TBD), HITOS.md / ESTADO_ACTUAL.md / DECISIONS.md actualizados, tag `v0.35.0-hito35`.

## Lo que NO se cubrió

- **Undo del drop de textura sobre material slot**: cambio estructural (asigna nuevo MaterialAssetId). Requeriría comando dedicado tipo `ReplaceMaterialCommand`. Aceptado para v1.
- **Triggers detectan dynamic bodies / NPCs** (deuda Hito 33): coste medio, fuera de scope chico.
- **Emisión de partículas por shape** (deuda Hito 29): coste medio, fuera de scope.
- **~10 widgets estructurales del Inspector** restantes (combos type/shape, ranges, DragInt): heredado, criterio: solo escalares/vectores puros van con `pushEditIfDone<T>`.
- **Save/Load gameplay state, main menu, PackageBuilder smart-pack**: candidatos PLAN_HITO36.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-01):
- Drop textura sobre material slot crea instance único (anti-contagio del Hito 25), sin undo por ahora.
- MeshLoader normaliza paths de texturas externas con `lexically_normal()` antes del VFS.

---

## Verificación visual del dev (criterios cumplidos)

- Drag de una textura del AssetBrowser sobre el botón del slot del Inspector reemplaza el material por uno nuevo con esa textura como albedo. La entidad muestra el cambio en el frame siguiente. Otras entidades que compartían el material original NO se ven afectadas.
- Editar fov/near/far de Camera, fogColor/fogDensity/exposure/iblIntensity de Environment, volume/loop/playOnStart/is3D de AudioSource, speed/playing/loop de Animator, todos los toggles + velocidades + colores de ParticleEmitter → Ctrl+Z revierte cada uno.
- Drop de un `.obj` con `.mtl` que referencia una textura del repo → la entidad spawneada muestra la textura correcta (antes caía a missing por el bug del path con `..`).
- Suite 287/5561 sin regresiones.
