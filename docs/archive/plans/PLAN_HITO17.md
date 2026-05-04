# Plan — Hito 17: PBR (Physically Based Rendering)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 17) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Reemplazar el shading Blinn-Phong por **PBR metallic-roughness** + **IBL** básico desde el cubemap del skybox. Pasar de "luce decente con luz directa" a "luce decente bajo cualquier iluminación, incluso sin luces directas".

- **Modelo de material:** metallic + roughness + albedo + (opcional) normal map. Sin clearcoat, sin sheen, sin transmission por ahora.
- **BRDF:** Cook-Torrance (GGX para distribución + Smith para geometry + Fresnel-Schlick).
- **IBL básico:** convolucionar el cubemap del skybox a una *irradiance map* (difusa) + *prefiltered env map* (especular en mips). Usar un BRDF LUT 2D pre-bakeado.
- **MaterialComponent persistido:** swap del par `MeshRenderer.materials[]` (texturas) por `MeshRenderer.materials[]` apuntando a `MaterialAsset` (id + slots de texturas + parámetros escalares).
- **`.moodmap` v7:** con materiales como entidades de primer nivel.

No-goals: refraction, SSS, anisotropic, parallax. Tampoco editor-de-grafo de materiales (ese es Hito 23). El BRDF LUT se hornea offline con Python (no runtime).

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: cálculo de Fresnel-Schlick con valores conocidos (F0 dieléctrico vs metálico). Round-trip de `MaterialAsset` en `.moodmap`. BRDF LUT cargado correctamente (channels + dimensiones).
- [ ] Cierre limpio.

### Visuales

- [ ] Una esfera metálica (metallic=1, roughness=0) refleja el skybox claramente, sin necesidad de luces directas.
- [ ] La misma esfera con roughness creciente (0 → 1) muestra reflejo cada vez más difuso (degradación visible).
- [ ] Una esfera dieléctrica (metallic=0) con roughness 0.2 muestra Fresnel: el borde grazing es más reflectivo que el centro.
- [ ] Demo "Showcase PBR" (3×3 esferas, eje X = metallic, eje Y = roughness) accesible desde "Ayuda > Agregar showcase PBR".
- [ ] El piso del mapa con material metálico baja refleja el skybox (junto con la columna proyectando sombra desde Hito 16, ahora con reflejo).

---

## Bloque 0 — Pendientes arrastrados del Hito 16

- [ ] **Cascaded Shadow Maps (CSM)** — diferido si no entra natural acá. La nueva escena PBR puede no necesitarlo si el bounding sphere fijo sigue siendo OK.
- [ ] **AABB del MeshAsset** para outlines precisos (pendiente arrastrado al outline OBB del Hito 16). Va a salir natural si parseamos AABBs desde assimp en este hito.

## Bloque 1 — `MaterialAsset` + serialización

- [ ] `engine/render/MaterialAsset.h`: struct con paths a 4 texturas (albedo, metallic-roughness packed, normal, AO opcional) + 4 escalares fallback (`albedoTint`, `metallicMult`, `roughnessMult`, `aoMult`). El "MR packed" es la convención glTF (R=AO, G=roughness, B=metallic).
- [ ] `AssetManager`: catálogo de `MaterialAsset` con `MaterialAssetId`, fallback a un default material (albedo blanco, metallic=0, roughness=0.5).
- [ ] `MeshRendererComponent.materials` (hoy `vector<TextureAssetId>`) → `vector<MaterialAssetId>`. Migración: `.moodmap` viejos siguen leyéndose con un upgrader que envuelve cada textura en un material auto-generado.
- [ ] `EntitySerializer`: persistir `material_path` por submesh.
- [ ] Bump `k_MoodmapFormatVersion` 6 → 7.
- [ ] Tests: round-trip de `MaterialAsset` + carga de `.moodmap` v6 (back-compat).

## Bloque 2 — Shaders PBR

- [ ] `shaders/pbr.vert` (igual al `lit.vert` actual, agregando si hace falta `aTangent` para normal mapping diferido).
- [ ] `shaders/pbr.frag`: Cook-Torrance con GGX + Smith + Schlick. Funciones helper auditables (`distributionGGX`, `geometrySmith`, `fresnelSchlick`).
- [ ] Sample del normal map en world-space (TBN matrix; tangents importados de assimp).
- [ ] Integrar el shadow factor del Hito 16 sobre el término directo (no el IBL).
- [ ] Logs de compilación + uniforms cacheados.

## Bloque 3 — IBL: irradiance + prefiltered + BRDF LUT

- [ ] `tools/bake_ibl.py`: dado el cubemap del skybox, generar `irradiance.cubemap` (32×32 por cara, 16-bit) y `prefilter.cubemap` (mip chain 128 → 4, 5 niveles, roughness 0..1).
  - Método: Monte Carlo por píxel + GGX importance sampling. Lento pero offline.
- [ ] `tools/bake_brdf_lut.py`: 2D LUT 256×256 RG16F con `(NdotV, roughness) -> (scale, bias)` para el split-sum de Epic. Tabla universal (no depende del skybox).
- [ ] Loader de cubemaps con mip chain en `OpenGLCubemapTexture` (extender el del Hito 15).
- [ ] Loader de la BRDF LUT como `OpenGLTexture` 2D float.
- [ ] `EnvironmentComponent.irradiancePath` + `prefilterPath` + `brdfLutPath`. Default: mismo skybox actual + LUT general.

## Bloque 4 — Inspector + UX

- [ ] `MaterialEditor` mini-panel: cuando el Inspector ve un `MeshRenderer`, dibujar por submesh el slot de material (drag&drop desde Asset Browser de un `.material`) + sliders de los multiplicadores (`metallic`, `roughness`, `albedoTint`).
- [ ] `AssetBrowser` sección Materiales: lista de `.material` JSON files en `assets/materials/`. Drag al viewport (sobre una entidad) asigna el material al primer submesh hovered.
- [ ] Hotkey: `M` mientras el mouse está sobre el viewport — abre menú flotante "Cambiar material".

## Bloque 5 — Demo + tests

- [ ] Demo "Ayuda > Agregar showcase PBR": grid 3×3 de esferas con materiales pre-bakeados (metallic=0/0.5/1 × roughness=0/0.5/1).
- [ ] `assets/materials/`: ~5 materiales sample (`metal_brass.material`, `plastic_red.material`, `wood_oak.material`, etc.) en JSON.
- [ ] `tests/test_pbr_brdf.cpp`: Fresnel-Schlick para F0=(0.04, 0.04, 0.04) (dieléctrico) en θ=0 y θ=π/2; F0=(0.95, 0.93, 0.88) (oro) en θ=0; etc.
- [ ] `tests/test_material_serializer.cpp`: round-trip + back-compat de `.moodmap` v6.

## Bloque 6 — Cierre

- [ ] Smoke test visual completo (showcase + escena del Hito 16 con materiales reasignados a metálico/dieléctrico).
- [ ] Actualizar `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- [ ] Tag `v0.17.0-hito17` + push.
- [ ] Crear `PLAN_HITO18.md` (Deferred / Forward+).

---

## Pendientes futuros que pueden aparecer en este hito

- **Tangents importados** — si los `.obj`/`.gltf` actuales no traen tangentes, calcular en runtime con MikkTSpace o desactivar normal map en esos meshes.
- **HDR cubemaps reales** — hoy el skybox es un PNG LDR; con PBR la falta de rango dinámico se nota en los reflejos especulares. Mitigación inicial: un escalar `skyboxIntensity` en `EnvironmentComponent`. Migración real a `.hdr` es un sub-bloque opcional.
- **Cubemap loader desde `.hdr` (Radiance RGBE)** — stb_image lo soporta. Disparar si los reflejos LDR se ven planos.
