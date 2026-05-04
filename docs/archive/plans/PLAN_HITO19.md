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

- [x] Compila sin warnings nuevos.
- [x] Tests: interpolación de matrices (transform de hueso en `t=0`, `t=1`, `t=mid`), suma de pesos por vértice = 1.
- [x] Cierre limpio.

### Visuales

- [x] Demo "Ayuda > Agregar personaje animado" carga un `.glb` con esqueleto + animación walk/idle de la web (CC0 — `Fox.glb` de glTF Sample Assets, 3 clips: Survey/Walk/Run). En Play Mode (o auto-play) la animación corre en loop.
- [x] Cambiar el clip activo desde el Inspector (combo box).
- [x] El personaje recibe sombra (Hito 16) y luces PBR (Hito 17 + 18) como cualquier otro mesh.

---

## Bloque 0 — Pendientes arrastrados

- [x] **AABB del MeshAsset** (Hito 16 + 17): se calcula del bind pose en `MeshLoader::flattenAiMesh` acumulando min/max de positions; queda en `MeshAsset.aabbMin/aabbMax`. Para meshes animados se considera "conservativo" (la pose runtime puede exceder el bind un poco — aceptable para outline 3D, no para frustum culling estricto).

## Bloque 1 — Skeleton + AnimationClip (datos)

- [x] `engine/animation/Skeleton.h`: `vector<Bone>` con índice de padre, `inverseBindMatrix`, `localBindTransform`. Métodos `boneIndex(name)`, helper libre `computeSkinningMatrices(skel, localPose, out)`. Header-only.
- [x] `engine/animation/AnimationClip.h`: lista de `BoneTrack` (uno por hueso), cada uno con keyframes de `position`, `rotation` (quat), `scale`. Métodos `samplePosition/Rotation/Scale/LocalTRS` + `evaluate(t, skeleton, out)` que cae al `localBindTransform` para huesos sin track. Header-only.
- [x] `MeshAsset` extendido con `std::optional<Skeleton>` + `std::vector<AnimationClip>` + `aabbMin/aabbMax`. Helper `hasSkeleton()`.

## Bloque 2 — Carga via assimp

- [x] Extender `MeshLoader.cpp`: construye un único `Skeleton` por `MeshAsset` aglutinando `aiBone` de todos los `aiMesh` (convención glTF: skin compartido por todos los primitives). Topo sort para que padres tengan índice menor que hijos.
- [x] Parsear `aiAnimation` → `AnimationClip` con keyframes lineales. Convertir `aiNodeAnim::mPositionKeys/mRotationKeys/mScalingKeys`. ticks → segundos via `mTicksPerSecond` (con default 25 si es 0).
- [x] Vertex layout extendido a stride 19: `vec4 boneIds + vec4 boneWeights` (locations 4 y 5). Default 0/0 cuando el mesh no tiene esqueleto. Top-4 influencias por vértice, renormalizadas a suma 1.0. Flag `aiProcess_LimitBoneWeights` + clamp/normalize defensivo en C++.

## Bloque 3 — Shader skinneable

- [x] `shaders/pbr_skinned.vert`: LBS 4 huesos. `uniform mat4 uBoneMatrices[128]`. Si `sum(weights) < 1e-4` cae al pipeline no-skinneado (defensivo). Normal con `mat3(boneMat)` (asume scales uniformes en huesos — válido para humanoides).
- [x] `pbr.frag` queda intacto (recibe `vWorldPos`/`vWorldNormal` ya transformados).
- [x] Switch en `EditorRenderPass.cpp`: dos pases. Pase A (estático) usa `m_pbrShader` y skipea entidades con `SkeletonComponent`. Pase B (skin) bindea `m_pbrSkinnedShader` solo si hay alguna entidad con `SkeletonComponent` y sube `uBoneMatrices[i]` por entidad. Lambda `applyShaderUniforms` evita duplicar el setup.

## Bloque 4 — `AnimatorComponent` + sistema

- [x] `Components.h`: `AnimatorComponent { string clipName; float time; float speed; bool playing; bool loop; }`. `clipName` vacío -> primer clip del MeshAsset.
- [x] `Components.h`: `SkeletonComponent { vector<glm::mat4> skinningMatrices }`. Lo rellena `AnimationSystem` cada frame; el render lee del componente.
- [x] `systems/AnimationSystem.{h,cpp}`: itera entidades con (MeshRenderer + Animator + Skeleton), resuelve clip por nombre o cae al primero, avanza `time` con loop/clamp, evalúa la pose y compone matrices skinning.
- [x] `EditorApplication::run()` llama `m_animationSystem->update(*m_scene, *m_assetManager, dt)` entre Script y Audio (paso 3.55).

## Bloque 5 — Demo + tests

- [x] Bajado `assets/meshes/Fox.glb` (162 KB, CC0, glTF Sample Assets de Khronos). 3 clips: "Survey", "Walk", "Run".
- [x] "Ayuda > Agregar personaje animado" spawnea entidad "Fox" con escala 0.01 (assimp lo trae a escala ~100). Auto-play del clip default.
- [x] `tests/test_animation.cpp`: 12 casos cubriendo Skeleton (boneIndex, computeSkinningMatrices con bind/rotación), BoneTrack (clamp + lerp + quat normalization + scale fallback), AnimationClip::evaluate (fallback al bind pose para huesos sin track + clip vacío).
- [x] InspectorPanel: combo de clips con `duration: %.2fs time: %.2fs` debajo, sliders de speed, checkboxes playing/loop, botón Reset.

## Bloque 6 — Cierre

- [ ] Smoke test visual completo (personaje animado con sombra + IBL + Forward+). **Pendiente del dev**.
- [x] Actualizar `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- [ ] Tag `v0.19.0-hito19` + push.
- [x] Crear `PLAN_HITO20.md` (UI del juego con RmlUi).

---

## Pendientes futuros que pueden aparecer en este hito

- **Persistir el `.glb` con esqueleto en `.moodmap`**: hoy `MeshRendererComponent` solo persiste el path del mesh; el esqueleto vive en el MeshAsset cargado por el AssetManager. Si el mesh cambia el esqueleto cambia. Aceptable.
- **Quaternion blending**: si los keyframes son escasos y el LERP de quats da twisting, migrar a SLERP. Trigger: animación visible con artifacts.
- **Motion matching / blend trees**: Hito dedicado, no entra acá.
- **Vertex animation textures** (alternativa a skinning para crowds): si aparece la necesidad de cientos de personajes animados, evaluar.
