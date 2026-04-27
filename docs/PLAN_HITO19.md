# Plan — Hito 19: Animación esquelética

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 19) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Cargar y reproducir animaciones esqueléticas desde `.gltf`/`.glb` (assimp ya está integrado desde el Hito 10). El shader vertex skineable, una clase `Skeleton` + `AnimationClip`, y un Animator básico que avance tiempo + interpole keyframes.

- **Skinning lineal** (LBS) — cuatro huesos por vértice, blend lineal de matrices. Sin dual quaternion, sin spherical blend.
- **Interpolación lineal** entre keyframes. Sin curvas Bezier ni reduction de keys.
- **Sin blend de animaciones** (un único clip activo a la vez por entidad). Blend nodes / state machines quedan para Hito 19.5+.
- **Sin retargeting** (un esqueleto por mesh).

No-goals: ragdoll, animaciones procedurales (IK), motion matching, blendshapes/morphs.

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: interpolación de matrices (transform de hueso en `t=0`, `t=1`, `t=mid`), suma de pesos por vértice = 1.
- [ ] Cierre limpio.

### Visuales

- [ ] Demo "Ayuda > Agregar personaje animado" carga un `.glb` con esqueleto + animación walk/idle de la web (CC0). En Play Mode (o auto-play) la animación corre en loop.
- [ ] Cambiar el clip activo desde el Inspector (combo box).
- [ ] El personaje recibe sombra (Hito 16) y luces PBR (Hito 17 + 18) como cualquier otro mesh.

---

## Bloque 0 — Pendientes arrastrados

- [ ] **AABB del MeshAsset** (Hito 16 + 17): los meshes con esqueleto necesitan AABB dinámico (cambia por frame con la pose). En este hito alcanza con el AABB del bind pose conservativo + un margen.

## Bloque 1 — Skeleton + AnimationClip (datos)

- [ ] `engine/animation/Skeleton.h`: `vector<Bone>` con índice de padre, `inverseBindMatrix`, `localBindTransform`. Métodos `boneIndex(name)`, `globalTransform(boneIdx, pose)`.
- [ ] `engine/animation/AnimationClip.h`: lista de `BoneTrack` (uno por hueso), cada uno con keyframes de `position`, `rotation` (quat), `scale`. Métodos `evaluate(time, pose)`.
- [ ] `MeshAsset` extendido con campos opcionales `std::optional<Skeleton>`, `std::vector<AnimationClip>`. Los meshes sin esqueleto (cubo, esfera) los dejan vacíos.

## Bloque 2 — Carga via assimp

- [ ] Extender `MeshLoader.cpp`: si `aiScene->mNumMeshes > 0` Y el primer mesh tiene `mNumBones > 0`, parsear el esqueleto (mapeo nombre → índice, parent links, inverse bind matrix). Sin caché compartido todavía: cada mesh trae su propio esqueleto.
- [ ] Parsear `aiAnimation` → `AnimationClip` con keyframes lineales. Convertir `aiNodeAnim::mPositionKeys/mRotationKeys/mScalingKeys`.
- [ ] Vertex layout extendido: `vec4 boneIds + vec4 boneWeights` por vértice. Default 0/0 cuando el mesh no tiene esqueleto.

## Bloque 3 — Shader skinneable

- [ ] `shaders/pbr_skinned.vert`: nuevo path con `uniform mat4 uBoneMatrices[N]` (N = ~128 typical, máximo ~256 según limits).
  - `vec4 skinnedPos = boneMatrices[boneIds.x] * pos * weights.x + ...` (4 huesos, blend LBS).
  - Las normales se transforman por la inversa-transpuesta del skinned matrix (aprox: blend de las inversas-transpuestas).
- [ ] `pbr.frag` queda intacto (recibe `vWorldPos` y `vWorldNormal` ya transformados).
- [ ] Switch del shader en EditorRenderPass: si la entidad tiene `SkeletonComponent` con esqueleto, usar el shader `pbr_skinned`; si no, el `pbr` normal.

## Bloque 4 — `AnimatorComponent` + sistema

- [ ] `engine/scene/Components.h`: `AnimatorComponent { string clipName; float time; bool playing; bool loop; }`.
- [ ] `engine/scene/Components.h`: `SkeletonComponent { vector<glm::mat4> currentPose; }` — el array de matrices que el shader sube al uniform. Recalculado por frame.
- [ ] `systems/AnimationSystem.h`: para cada entidad con `SkeletonComponent` + `AnimatorComponent`, avanza `time` por dt y evalúa el clip activo a la pose. Loop / clamp según flag.
- [ ] Actualizar `EditorApplication::run()` para llamar `AnimationSystem::update(scene, dt)` antes del render.

## Bloque 5 — Demo + tests

- [ ] Bakear o descargar un `.glb` simple con animación (~50 frames idle/walk). Asset CC0 (Mixamo, GitHub gltf-Sample-Models). Drop en `assets/meshes/`.
- [ ] "Ayuda > Agregar personaje animado" spawnea la entidad con MeshRenderer + Skeleton + Animator. Auto-play del clip default.
- [ ] `tests/test_animation.cpp`: interpolación de keyframes (`evaluate(t=0)` == primer key, `evaluate(t=1)` == último, suma de pesos vertex válida).

## Bloque 6 — Cierre

- [ ] Smoke test visual completo (personaje animado con sombra + IBL + Forward+).
- [ ] Actualizar `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- [ ] Tag `v0.19.0-hito19` + push.
- [ ] Crear `PLAN_HITO20.md` (UI del juego con RmlUi).

---

## Pendientes futuros que pueden aparecer en este hito

- **Persistir el `.glb` con esqueleto en `.moodmap`**: hoy `MeshRendererComponent` solo persiste el path del mesh; el esqueleto vive en el MeshAsset cargado por el AssetManager. Si el mesh cambia el esqueleto cambia. Aceptable.
- **Quaternion blending**: si los keyframes son escasos y el LERP de quats da twisting, migrar a SLERP. Trigger: animación visible con artifacts.
- **Motion matching / blend trees**: Hito dedicado, no entra acá.
- **Vertex animation textures** (alternativa a skinning para crowds): si aparece la necesidad de cientos de personajes animados, evaluar.
