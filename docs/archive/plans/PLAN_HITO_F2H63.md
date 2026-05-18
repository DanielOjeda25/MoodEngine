# PLAN HITO F2H63 — Transparencia realista base + refracción

> **Estado**: REFINADO post-AUDIT-3 (2026-05-17). Listo para arrancar.
> **Tag previsto**: `v1.50.0-fase2-hito63`.
> **Origen**: pedido del dev al cierre F2H62 *"un motor grafico debe tener transparentes"*. Tras discusión de scope, el dev pidió **transparencia realista, no simulaciones**. Decisión: dividir en dos hitos para no convertir F2H63 en un mega-hito de 30h. F2H63 entrega la base visual realista (alpha + refracción + Fresnel); F2H64 entrega correctness (OIT + sombras translúcidas).

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué entrega F2H63

- **Materiales translúcidos** con blend modes Opaque / Translucent / Additive.
- **Vidrio que distorsiona lo que tiene detrás** — no es una capa de color, es refracción real con IOR (índice de refracción) ajustable por material. Ves la grilla del suelo deformada al mirar a través.
- **Fresnel automático**: los materiales translúcidos son más opacos en los bordes (grazing angle) y más transparentes de frente. Convención que da el look "vidrio real" sin que el dev tenga que pelearse con el shader.
- **Inspector → Material → Blending** con dropdown (Opaque/Translucent/Additive), slider Opacity y slider IOR (1.0 = sin refracción, 1.5 = vidrio, 1.33 = agua).
- **Shader Graph** soporta alpha — el `OutputPBR` gana un 6º input `Opacity`. Los samples viejos siguen cargando (opacity=1.0 default).

### Qué NO entra (F2H64)

- **OIT** (Order-Independent Transparency) — F2H63 usa sort back-to-front por distancia cámara→centro. Funciona perfecto para escenas con pocos translúcidos no-solapados. Si tenés muchos vidrios intersectados o follaje denso, puede haber flicker.
- **Sombras translúcidas** — los vidrios v1 proyectan sombra opaca (o ninguna, según un toggle). Las sombras tintadas vienen en F2H64.
- **Refracción full Snell** — usamos screen-space distortion (convención Unity URP / Unreal "Distortion"). No es Snell físico pero el resultado visual es indistinguible para el ojo en geometría plana / convexa simple.

---

## 🧱 Bloques de ejecución

### Bloque A — Schema MaterialAsset extendido (~1h)

- `enum class BlendMode { Opaque, Translucent, Additive }` en `MaterialAsset.h`.
- Campos nuevos:
  - `BlendMode blendMode = BlendMode::Opaque`
  - `float opacity = 1.0f` (0-1, alpha base del material)
  - `float ior = 1.0f` (1.0 = sin refracción, 1.5 = vidrio glass, 1.33 = agua)
  - `float refractionStrength = 0.0f` (0-1, multiplicador del offset screen-space; permite "refracción suave" sin tocar IOR)
- Serialización aditiva en `AssetManager_Material.cpp` — keys `blend_mode` ("opaque"/"translucent"/"additive"), `opacity`, `ior`, `refraction_strength`. Defaults preservan back-compat: materiales viejos cargan con `Opaque` + sin refracción.
- Test: roundtrip JSON con cada blend mode + opacity + ior.

### Bloque B — Shader emite alpha + refracción screen-space (~3h)

- `pbr.frag` (+ skinned + instanced) gana uniforms `uOpacity`, `uIor`, `uRefractionStrength`, `uBackbuffer` (sampler2D), `uScreenSize` (vec2).
- Fresnel-driven opacity (default integrado, no opt-in):
  ```glsl
  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
  float effectiveOpacity = mix(uOpacity, 1.0, fresnel * 0.5); // bordes más opacos
  ```
- Refracción screen-space:
  ```glsl
  vec2 ndcCoord = gl_FragCoord.xy / uScreenSize;
  vec2 refractOffset = (mat3(uView) * N).xy * (uIor - 1.0) * uRefractionStrength * 0.05;
  vec3 refracted = texture(uBackbuffer, ndcCoord + refractOffset).rgb;
  vec3 finalColor = mix(refracted, lighting, effectiveOpacity);
  FragColor = vec4(finalColor, effectiveOpacity);
  ```
- Para `Opaque` el camino corto: `FragColor = vec4(lighting, 1.0)` sin sampling de backbuffer (zero overhead).
- `pbr_graph_template.frag` análogo — markers `__SHADERGRAPH_OPACITY__` + `__SHADERGRAPH_REFRACTION_PROLOGUE__` (el sample del backbuffer queda fuera del scope del usuario del graph; el template lo emite siempre que blend != Opaque).
- `SceneRenderer` setea uniforms per-material en `applyShaderUniforms`.

### Bloque C — Backbuffer copy para refracción (~2h)

- Tras el pase opaco + skybox + brushes, antes del pase translucent:
  - **Blit del color attachment actual** a una textura `m_backbufferCopy` (mismo tamaño que viewport, formato RGBA16F).
  - `glBlitFramebuffer` desde el FBO main al FBO de backup. Mantiene HDR para no perder rango.
- La textura se bindea como `uBackbuffer` slot fijo en todos los shaders durante el translucent pass.
- Lazy alloc + resize en `onResize`.

### Bloque D — Translucent pass en SceneRenderer (~3h)

- Nuevo bucket `std::vector<Entity> translucents` en `BatchingResult`.
- `groupByBatch` ramifica: si `material->blendMode != Opaque`, va a `translucents` (no entra a `instanced` ni a `nonBatchable`).
- Nuevo método `SceneRenderer::renderTranslucentPass(scene, view, proj, cameraPos)`:
  1. Ordenar `translucents` por `distance(cameraPos, entityWorldCenter)` descending.
  2. Tiebreaker por entity ID estable (evita flicker frame-a-frame para entidades co-distantes).
  3. Blit backbuffer (delega a Bloque C).
  4. `glEnable(GL_BLEND)`, `glDepthMask(GL_FALSE)`, depth-test ON.
  5. Per-entity:
     - `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` si Translucent, `(GL_SRC_ALPHA, GL_ONE)` si Additive.
     - Bind shader, set uniforms (incluye uBackbuffer si Translucent), draw.
  6. Restore GL state.
- Llamada en `endFrame` después de pase opaco + skybox + brushes, **antes de SSAO/SSR/bloom** (correcto — los translucents leen el color de los opacos, no afectan el screen-space pass).

### Bloque E — UI Inspector Blend Mode + Opacity + IOR (~1.5h)

- En `InspectorPanel_MeshRenderer.cpp` sección "Material" → nuevo `CollapsingHeader` "Blending".
- `ImGui::Combo` BlendMode (Opaque / Translucent / Additive).
- `ImGui::SliderFloat` Opacity (0.0 - 1.0), disabled si Opaque.
- `ImGui::SliderFloat` IOR (1.0 - 2.5), disabled si Opaque o Additive (refracción no aplica a additive). Presets como tooltip: "Agua=1.33, Vidrio=1.50, Diamante=2.42".
- `ImGui::SliderFloat` "Distorsión" (refractionStrength, 0.0-1.0), disabled si Opaque.
- Undo via `pushEditIfDone` (mismo patrón que `albedoTint`).
- i18n keys nuevas (es + en).

### Bloque F — Shader Graph OutputPBR 6º input Opacity (~2h)

- Extender el `OutputPBR` node en `ShaderGraphAsset.h` de 5 → 6 inputs (Albedo, Metallic, Roughness, Normal, Emissive, **Opacity**).
- Marker `__SHADERGRAPH_OPACITY__` en `pbr_graph_template.frag`. Generator wiring.
- Back-compat: assets `.moodshader` viejos cargan con `opacity` desconectado → emite `1.0` por default en el GLSL generado.
- Si `Opacity` está conectado y el material no es `Translucent`, warning en log: *"OutputPBR.Opacity conectado pero material es Opaque — el alpha será ignorado. Cambiá el BlendMode a Translucent"*.
- Test regression: `test_shader_graph.cpp` valida que template real no deje residuales con el nuevo marker.

### Bloque G — Samples shipados (~1.5h)

- `assets/shaders/graphs/sample_glass.moodshader` nuevo: PBR estándar + Fresnel node modulando Opacity en el OutputPBR. Material asociado con `BlendMode=Translucent, IOR=1.5, refractionStrength=0.6`.
- `assets/shaders/graphs/sample_water.moodshader` actualizado: extender con Opacity conectado al output, material `Translucent, IOR=1.33, refractionStrength=0.3`. Animación sutil con Time + Noise sobre la normal.
- `sample_hologram.moodshader` actualizado: usar el 6º input Opacity con Fresnel inverso (más transparente en bordes, look "scan").

### Bloque H — Tests (~1h)

- `test_asset_manager.cpp`: roundtrip JSON MaterialAsset con cada BlendMode + opacity + IOR + refractionStrength.
- `test_render_batching.cpp` (nuevo o extender): `groupByBatch` separa entidades por blendMode correctamente.
- Sort back-to-front: array de 4 entidades a distintas distancias → orden de render esperado, tiebreaker por ID.
- `test_shader_graph.cpp`: shader graph viejo (5 inputs) carga con opacity default=1.0. Generator emite marker correctamente.

### Bloque I — Validación visual + cierre (~1h)

- Dev abre editor:
  1. Crea cubo, en Inspector → Material → BlendMode=Translucent, Opacity=0.5, IOR=1.5.
  2. Pone el cubo sobre el suelo grillado. **Espera ver la grilla DISTORSIONADA a través del cubo**.
  3. Activa rotación de cámara → distorsión sigue dinámicamente.
  4. Pone otro cubo opaco detrás → se ve a través del translúcido.
  5. Carga `sample_glass.moodshader` y aplica a una esfera → look vidrio real (más opaco en bordes, transparente al frente).
- Updates docs: HITOS one-liner + ESTADO_ACTUAL (sección 0.1) + DECISIONS (decisión de scope) + crear `docs/hitos/F2H63.md` + archivar este plan.
- Commit + tag `v1.50.0-fase2-hito63` + push.

---

## Total estimado

**9 bloques A-I, ~15-17h reales**. Hito mediano. Lo más riesgoso:
- Backbuffer copy + sampling correcto (Bloque B+C) — primera vez que el motor lee del backbuffer mid-frame.
- Shader graph extensión a 6 inputs (Bloque F) — back-compat con assets viejos.

---

## NO entra en F2H63 (queda para F2H64)

- **OIT Weighted Blended** (McGuire 2013) — solución a parpadeo en escenas con muchos translúcidos solapados.
- **Sombras translúcidas** — vidrios proyectan sombra tintada según color + opacity. Requiere extender shadow atlas a 3 canales.
- **Refracción full Snell** — trazar el rayo a través del material según grosor estimado. Por ahora screen-space hack es suficiente.
- **Premultiplied alpha workflow** — la convención straight alpha funciona para v1 sin issues conocidos.
- **Translúcidos en SSAO/SSR** — leen el depth attachment del pase opaco. Aceptable v1 (convención estándar Unity URP / Unreal).
- **Multiply blend mode** — sin demanda específica.

---

## Riesgos identificados

- **Sort instabilities con cámara dinámica** — entidades co-distantes pueden alternar orden. Mitigación: tiebreaker por entity ID estable.
- **Performance con muchos translúcidos** — cada uno es draw call separado. Mitigación: aceptable v1 (típicamente <50 vidrios por escena); OIT en F2H64 si emerge presión.
- **Refracción screen-space tiene artifacts en bordes de pantalla** — si la geometría está cerca del borde, la coord desplazada puede caer fuera del backbuffer copy y devolver negro. Mitigación: clamp + fade en bordes.
- **HDR backbuffer copy** — formato RGBA16F mismo que main FBO. Verificar al implementar.
- **Shader graph back-compat** — 5 vs 6 inputs. Aditivo schema con default; tests cubren.

---

## Referencias

- Real-Time Rendering 4th ed., cap 5.5 (Transparency, Translucency, Refraction).
- Unity URP Forward Renderer / Surface Type / Refraction.
- Unreal Material `Blend Mode` + `Refraction` documentation.
- McGuire & Bavoil "Weighted Blended Order-Independent Transparency" (2013) — para F2H64.
- "Real-Time Rendering of Glass" — Wyman 2005 (refracción).
