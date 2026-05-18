# PLAN HITO F2H63 — Materiales translúcidos / alpha blending

> **Estado**: STUB pendiente arranque (2026-05-17).
> **Tag previsto**: `v1.50.0-fase2-hito63`.
> **Origen**: pedido del dev al cierre F2H62 — *"para mas adelante, porque es obvio que deberemos tener transparentes, un motor grafico debe tenerlo"*. F2H62 cerró Sub-fase 2.3 (Renderer) del plan original 100% (PBR + SSAO + bloom + CSM + SSR + shader graph). El plan original Fase 2 no tenía un hito dedicado a transparencia (asumió opaque-only para v1); este hito abre el roadmap post-2.3 con el feature más solicitado: materiales translúcidos.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué es transparencia / alpha blending

Materiales que **no son completamente opacos** — la luz los atraviesa parcialmente y se ve lo que hay detrás. Casos del juego:

- **Vidrios** (ventanas, vasos, vitrinas).
- **Agua** (lagos, charcos translúcidos).
- **Hologramas con transparencia real** (no solo glow — ver a través).
- **Decals translúcidos** (manchas de sangre semi-transparentes sobre paredes).
- **Partículas con fade** (humo, niebla, llamas).
- **UI 3D in-world** (textos, indicadores flotando en el mundo).

### Por qué no entra en F2H62

El shader graph runtime de F2H62 emite RGB al `FragColor` y asume blending opaque. Transparencia es un cambio de pipeline, no de shader:

1. **El shader necesita emitir alpha** además de RGB.
2. **El render pipeline necesita un pase separado** para transparentes (después del pase opaco).
3. **Los transparentes necesitan estar ordenados de atrás para adelante** (sino se ven mal porque el depth test los descarta).
4. **El batching tiene que separarlos del path opaco** (no se pueden batchear mezclados).

### Cómo se integraría en MoodEngine

- **MaterialAsset extendido** con un `enum BlendMode { Opaque, Translucent, Additive, Multiply }` (default `Opaque` para back-compat). Convención Unity URP / Unreal Material `Blend Mode` / Godot StandardMaterial3D.
- **Inspector → Material → Blend Mode dropdown** + slider `opacity` cuando blend != Opaque.
- **RenderBatching** separa entidades por blend mode: opacos al path existente (instanced + nonBatchable), translúcidos a un bucket nuevo.
- **SceneRenderer** pase nuevo `PBR::translucentPass` después del opaco + skybox + brushes pero ANTES de SSAO/SSR/bloom/post-process. Sort back-to-front por distancia cámara→centro AABB world.
- **Shader pbr.frag** emite `FragColor = vec4(lighting, opacity)` cuando el material tiene blend != Opaque. El `pbr_graph_template.frag` también — el OutputPBR del shader graph gana un 6to input `opacity` (float, default 1.0).
- **GL state**: `glEnable(GL_BLEND)` + `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` para Translucent, `glBlendFunc(GL_SRC_ALPHA, GL_ONE)` para Additive, `glDepthMask(GL_FALSE)` (escriben depth ≠ no escriben — convención).

### Convención industria mínima v1

- **Opaque** (default): blending disabled, depth-write enabled. Path actual.
- **Translucent**: SRC_ALPHA / ONE_MINUS_SRC_ALPHA, depth-test enabled, depth-write disabled. Sort back-to-front. El más común.
- **Additive**: SRC_ALPHA / ONE, depth-test enabled, depth-write disabled. Para emissive (glow, fuego, magia). Sin sort estricto (commutative).
- **Multiply** (opcional v1): DST_COLOR / ZERO. Para sombras pintadas / tinturas. Diferible si emerge presión de scope.

---

## 🧱 Bloques de ejecución (DRAFT — refinar al arrancar)

### Bloque A — Schema MaterialAsset extendido

- `enum class BlendMode { Opaque, Translucent, Additive }` en `MaterialAsset.h`.
- Campos `BlendMode blendMode = Opaque`, `float opacity = 1.0f` en `MaterialAsset`.
- Serialización JSON aditiva en `AssetManager_Material.cpp` (`blend_mode`: "opaque"/"translucent"/"additive"; `opacity`: float). Default Opaque para retro-compat.
- Test: roundtrip JSON con cada blend mode + opacity.

### Bloque B — Shader emite alpha + uniforms

- `pbr.frag` gana uniform `uOpacity` (default 1.0). Última línea: `FragColor = vec4(lighting, uOpacity);`. Para Opaque el uniform queda en 1.0 y el alpha no afecta nada (mismo rendering visual).
- `pbr_graph_template.frag` análogo — agregar marker `__SHADERGRAPH_OPACITY__` y nuevo input al OutputPBR. Generator actualizado.
- Skinned y instanced igual.
- `SceneRenderer` setea `uOpacity` en `applyShaderUniforms` per-material.

### Bloque C — Path de render translucent en SceneRenderer

- Nuevo bucket `std::vector<Entity> translucents` en `BatchingResult`.
- En `groupByBatch`, si `material->blendMode != Opaque`, va a `translucents` (no entra a batches ni a nonBatchable).
- Nuevo método `SceneRenderer::renderTranslucentPass(scene, view, proj, cameraPos)`:
  - Ordenar `translucents` por distancia cámara→centro AABB (descending = farthest first).
  - `glEnable(GL_BLEND)` + `glDepthMask(GL_FALSE)` + glBlendFunc según mode.
  - Loop: bind shader, set uniforms, draw mesh. Reusa `drawMeshRenderer` lambda (que ya maneja shader graph cache).
  - Restore GL state al final.
- Llamada en `endFrame` después de PBR static + skinned + brushes, antes de SSAO/SSR/bloom.

### Bloque D — UI: Inspector dropdown blend mode + opacity slider

- En `InspectorPanel_MeshRenderer.cpp`, sección "Material" → nuevo CollapsingHeader "Blending".
- `ImGui::Combo` Blend Mode (3 opciones — Opaque / Translucent / Additive).
- `ImGui::SliderFloat` opacity (0.0 - 1.0), disabled si Opaque.
- Undo via `pushEditIfDone` (mismo patrón que albedoTint).
- i18n keys nuevas en es + en.

### Bloque E — Samples shipados

- 1-2 samples en `assets/shaders/graphs/` o materiales pre-armados:
  - **Vidrio translúcido**: PBR estándar con `blendMode=Translucent`, `opacity=0.3`, roughness baja.
  - **Hologram con alpha**: extender `sample_hologram.moodshader` para usar el alpha output del shader graph (cuando F2H62 gane el 6to input `opacity` en OutputPBR).
- Sample shader graph nuevo `sample_glass.moodshader` con Fresnel modulando opacity (más opaco en bordes, más translúcido al frente — look "vidrio real").

### Bloque F — Tests

- Roundtrip JSON MaterialAsset con cada blend mode + opacity.
- `groupByBatch` separa correctamente entidades por blend mode.
- Sort back-to-front: array de entidades a varias distancias → orden de render correcto.
- `pbr_graph_template.frag` con el nuevo marker `__SHADERGRAPH_OPACITY__` sigue pasando los tests de "no marcadores residuales".

### Bloque G — Validación visual + cierre

Dev abre el editor, crea un cubo con material Translucent opacity=0.3 sobre el suelo, ve la grilla a través del cubo. Pone otro cubo opaco detrás del translúcido → se ve a través. Mueve la cámara → los translúcidos se ordenan dinámicamente sin pop-in.

### Bloque H — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.50.0-fase2-hito63`. ~30 min.

---

## Total estimado

**8 bloques A-H, ~10-14h**. Hito mediano. La complejidad real está en (a) el pipeline change (Bloque C) y (b) hacer que el shader graph soporte alpha (extender el OutputPBR a 6 inputs — Bloque B + extension del generator).

---

## NO entra en F2H63

- **OIT (Order-Independent Transparency)** — algoritmos como Weighted Blended OIT (McGuire 2013) o Per-Pixel Linked Lists evitan el sort manual pero requieren MRT + compositing pass. Diferible a futuro si emerge demanda de muchos translúcidos solapados con orden no-resoluble (cabello, follaje denso).
- **Refracción** (vidrios curvos que distorsionan lo que está detrás) — requiere sample de la backbuffer en posiciones desplazadas + cálculo de IOR. Diferible.
- **Premultiplied alpha workflow** — la convención straight alpha es más simple y suficiente para v1.
- **Transparencia en shadow casting** (sombras "vidrio") — el shadow pass se mantiene opaco. Translúcidos no proyectan sombra en v1.
- **Multiply blend mode** — diferible si no emerge demanda; Opaque + Translucent + Additive cubre 95% de casos.

---

## Referencias

- Real-Time Rendering 4th ed., cap 5.5 (Transparency, Translucency).
- Unity URP `Surface Type` dropdown documentation.
- Unreal Material `Blend Mode` documentation.
- McGuire & Bavoil "Weighted Blended Order-Independent Transparency" (2013) — para OIT futura.

---

## Riesgos identificados

- **Sort instabilities con cámara dinámica** — entidades muy cerca entre sí en distance camera→center pueden alternar orden frame-a-frame causando flicker. Mitigación: tiebreaker por entity ID (estable).
- **Performance con muchos translúcidos** — cada uno es un draw call separado (sin batching). Para escenas con 50+ glasses puede impactar. Mitigación: sort + cull agresivos; OIT como upgrade futuro si emerge presión.
- **Depth-write disabled + post-process** — SSAO/SSR leen el depth attachment. Si el translúcido no escribe depth, el SSAO/SSR de pixeles detrás del translúcido podría no ver el oclusor translúcido. Decisión: aceptable v1 (convención estándar). Translúcidos no participan en SSAO/SSR (limitación documentada).
- **Compatibilidad con shader graph** — el OutputPBR gana un 6to input `opacity`. Assets `.moodshader` viejos (5 inputs) deben cargar OK como `opacity=1.0` default. Aditivo schema.
- **Compatibilidad con instancing** — el path A.1 (instanced) puede soportar translucent si los materials son el mismo en todo el batch + el sort se hace por batch center. Diferible: por ahora translucents siempre caen a nonBatchable (mismo trato que shader graphs en F2H62).
