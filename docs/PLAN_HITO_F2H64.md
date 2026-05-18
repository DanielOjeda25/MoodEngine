# PLAN HITO F2H64 — OIT + sombras translúcidas

> **Estado**: REFINADO (2026-05-18). Arrancando.
> **Tag previsto**: `v1.51.0-fase2-hito64`.
> **Origen**: split scope de F2H63 (2026-05-17). F2H63 entregó base visual (alpha + refracción + Fresnel); F2H64 entrega correctness técnica.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué entrega F2H64

- **Sin parpadeo en escenas con muchos translúcidos solapados.** 20 botellas de vidrio una encima de otra: ninguna salta de orden frame-a-frame.
- **Sombras tintadas automáticas.** Una botella verde tira sombra verdosa. Una roja, rojiza. Default ON para Translucent.
- **API del Inspector queda igual** (BlendMode + Opacity + IOR + Distorsión). Solo se agrega checkbox "Proyectar sombra tintada".

### Decisiones de scope validadas con el dev (2026-05-18)

- **(a) Sombras tintadas opt-in con default ON para Translucent.** Default OFF para Additive (no tiene sentido). Escape hatch si emerge problema de perf.
- **(b) OIT reemplaza el sort manual de F2H63.** Una sola forma de hacer las cosas.
- **(c) OIT compatible con shader graph desde día 1.** Sin esto los samples (water/glass/hologram) flickearían al apilar.

### Cómo se integra

- **OIT Weighted Blended (McGuire & Bavoil 2013)**: 2 render targets nuevos (`accumColor` RGBA16F, `revealage` R16F) en un FB separado `m_oitAccumFb`. Translucents escriben acumulados ponderados por peso heurístico depth-based. Pase composite final mezcla con el opaco.
- **Shadow atlas RGB extendido**: shadow map array gana color attachment RGBA8 además de depth. Translucents escriben `albedo × (1-opacity)` al canal. Sampling del shadow tinta la luz directional.

---

## 🧱 Bloques refinados

### Bloque A — OIT framebuffer separado (~2h)

- Crear `m_oitAccumFb` como `OpenGLFramebuffer` separado (convención del proyecto: ya hay `m_ssaoOutFb`, `m_bloomFb`, `m_backbufferCopyFb`).
- 2 attachments: `accumColor` RGBA16F (loc 0) + `revealage` R16F (loc 1).
- Comparte el depth attachment del scene FBO (lectura para depth-test, sin escribir).
- Lazy alloc + resize en `onResize`.
- Decisión: NO extender `OpenGLFramebuffer` con flag nuevo — FB independiente más simple.

### Bloque B — Shader OIT (uniform branching) (~3h)

- `pbr.frag` + `pbr_skinned.frag` + `pbr_instanced.frag` ganan uniform `int uOitPass` (default 0 = forward normal, 1 = OIT pass).
- En el branch translucent al final del shader:
  ```glsl
  if (uOitPass == 1) {
      float depth = gl_FragCoord.z;
      float weight = clamp(pow(min(1.0, opacity * 10.0) + 0.01, 3.0)
                           * 1e8 * pow(1.0 - depth * 0.9, 3.0), 1e-2, 3e3);
      outAccum    = vec4(lighting * opacity, opacity) * weight;
      outReveal   = opacity;
      return;
  }
  // ... resto del path Translucent F2H63 (refracción + mix) sin cambios
  ```
- `out vec4 outAccum` (loc 0), `out float outReveal` (loc 1) reemplazan `FragColor` cuando `uOitPass==1`.
- `pbr_graph_template.frag` análogo — el generator emite ambos paths controlados por `uOitPass`.

### Bloque C — Composite pass (~2h)

- Nuevo shader `oit_composite.frag` (fullscreen quad):
  ```glsl
  vec4 accum = texture(uAccum, vUv);
  float reveal = texture(uRevealage, vUv).r;
  if (reveal >= 1.0) discard;  // sin translucents en este pixel
  FragColor = vec4(accum.rgb / max(accum.a, 1e-5), 1.0 - reveal);
  ```
- `glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA)` para componer sobre el color buffer opaco.
- Llamada después del OIT pass, antes de SSAO/SSR/bloom.

### Bloque D — Reemplazar PBR::translucentPass por oitPass (~1.5h)

- `SceneRenderer_Render.cpp:791-864` reemplazado por `PBR::oitPass`:
  1. Blit backbuffer copy (F2H63, sigue).
  2. Bind `m_oitAccumFb`. `glDrawBuffers(2, ...)` con accum + revealage.
  3. Clear: accum=(0,0,0,0), revealage=(1,1,1,1).
  4. GL state: `glEnable(GL_BLEND)`, `glBlendFunci(0, GL_ONE, GL_ONE)`, `glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR)`, depth-test ON, depth-write OFF.
  5. Iterar `translucents` SIN sort (orden de creación es ok — el algoritmo es order-independent).
  6. Per-entity: set `uOitPass=1`, bind backbuffer copy, draw.
  7. Restore GL state.
  8. Bind scene FBO. Composite pass (Bloque C).
- Sort manual + tiebreaker eliminados.

### Bloque E — Shadow atlas RGB tinted (~3h)

- `OpenGLShadowMapArray` gana color texture `GL_TEXTURE_2D_ARRAY` RGBA8 paralela a la depth texture, mismas dimensiones + cascade count.
- Shadow FBO bindea ambos attachments por capa via `glFramebufferTextureLayer`.
- `shadow_depth.frag` gana `out vec4 outShadowColor` (loc 0):
  - Opaque / Additive / Translucent sin `castTranslucentShadow`: `outShadowColor = vec4(1.0)` (sin tinte).
  - Translucent con `castTranslucentShadow=true`: `outShadowColor = vec4(albedoTint * (1.0 - opacity), 1.0)`.
- `pbr.frag` sampling: para cada cascade lee depth + color → `dirLight *= shadowColor.rgb`.
- Translucents escriben con `glBlendFunc(GL_ZERO, GL_SRC_COLOR)` para acumular multiplicativamente (tinte de A × tinte de B = sombra final).

### Bloque F — UI toggle + MaterialAsset extension (~1h)

- `MaterialAsset.castTranslucentShadow = true` (default ON).
- Persistencia aditiva en `AssetManager_Material.cpp`: key `cast_translucent_shadow`, solo se escribe si `blendMode == Translucent` Y valor es distinto del default.
- `InspectorPanel_MeshRenderer.cpp` Blending header: checkbox "Proyectar sombra tintada", disabled si Opaque/Additive.
- Undo via `pushEditIfDone<bool>` (o cast a u32 si el variant no soporta bool).
- 2 keys i18n nuevas (es + en).

### Bloque G — Samples actualizados + tests (~2h)

- `sample_glass.moodshader`: validar que la sombra tinte cyan tenue.
- `sample_water.moodshader`: validar que tinte azul tenue.
- Tests:
  - `test_render_batching.cpp`: ELIMINAR "sort back-to-front por distancia^2" (línea 293). Los otros 4 F2H63 tests siguen.
  - Nuevo test: `oitPass orden-independent` — 3 translucents A,B,C en distintos órdenes producen el mismo output (mock del orden via vector swap).
  - Nuevo test: `MaterialAsset roundtrip JSON con castTranslucentShadow` (true / false / default).
  - Nuevo test: opaque y additive NO escriben tinted shadow (solo Translucent con flag).

### Bloque H — Validación visual + cierre (~1h)

- Tour dev:
  1. Cuarto con 6 botellas de varios colores Translucent apiladas → ninguna parpadea al mover cámara.
  2. Sample_glass aplicado a cubo sobre suelo blanco con luz directional → sombra cyan tenue.
  3. Toggle "Proyectar sombra tintada" OFF → sombra vuelve a ser opaca/oscura.
- Updates docs: HITOS one-liner + ESTADO_ACTUAL 0.1 + DECISIONS + crear `docs/hitos/F2H64.md` + archivar este plan.
- Commit + tag `v1.51.0-fase2-hito64` + push.

---

## Total estimado

**8 bloques A-H, ~15-17h reales.** Hito mediano.

---

## NO entra en F2H64 (queda para futuro hito si emerge demanda)

- **Refracción full Snell** (rayo a través del material según grosor estimado).
- **Sombras de Additive translúcido** (no tiene sentido físico).
- **Premultiplied alpha workflow** (straight alpha funciona).
- **Translucents contribuyen a SSAO/SSR/AO** (siguen leyendo depth attachment del pase opaco).
- **Multi-bounce refraction** (vidrio detrás de vidrio refracta correctamente al primer vidrio).

---

## Riesgos identificados

- **OIT weight tuning** — heurística McGuire requiere validación con escenas reales. Mitigación: empezar con valores del paper, ajustar si emerge artefacto.
- **Shadow atlas RGB cost** — cada translucent con `castTranslucentShadow` duplica el FB attach. Mitigación: default OFF para Opaque/Additive; opt-out por material si la escena lo necesita.
- **CSM compatibility** — F2H60 usa array textures para cascades. El color attachment también va como array. Verificar al implementar Bloque E.
- **Shader graph templates** — el template ahora emite branch OIT. Tests de regression validan que generator no rompe shaders existentes.
- **Backbuffer copy reuso** — refracción F2H63 sigue funcionando dentro del OIT pass (cada translucent puede leerlo). Verificar visualmente con sample_glass apilado.
