# PLAN F3H31 — Sky Atmosphere Hosek-Wilkie + GPU IBL bake runtime

**Estado:** ✅ **CERRADO** (2026-05-29, tag `v2.31.0-fase3-hito31`).
**Predecesor:** F3H30 (Texture pack HL1-style procedural).
**Sub-fase:** 3.4 reabierta (estaba cerrada 11/11; cierra ahora 12/12 con F3H31).

## Origen

Pedido del dev: *"quiero algo útil y profesional como el del Unreal"* + *"me gusta lo del bake en real time, en runtime, mientras sea procedural"*. Stub de Sub-fase 3.4 original era "HDRI dinámico + ciclo día/noche" — F3H30 pivoteó a texturas, ahora retomamos HDRI dinámico con scope más ambicioso: sky procedural Hosek-Wilkie 2012 + GPU IBL bake runtime.

Ruta elegida: **B** (Hosek-Wilkie analítico, no Hillaire 2020) — 95% del look "profesional con ciclo día/noche" con menos complejidad técnica. Upgrade a Hillaire es portable más adelante si emerge necesidad de multi-scattering correcto.

---

## Decisiones

**D1 — Hosek-Wilkie 2012 (analítico) vs Hillaire 2020 (LUT-based).** Elegido HW por simplicidad de port + 95% del look. Hillaire requiere 4 LUTs precomputadas (transmittance, multiscattering, sky-view, aerial perspective) — overkill para use case actual.

**D2 — GPU IBL bake runtime (no offline).** Pedido literal del dev. Trigger: cuando `EnvironmentComponent` con `skyboxSource=Procedural` cambia params (timeOfDay/turbidity/groundAlbedo), marca `skyDirty=true`. En `tickFrame`, si dirty + procedural → re-render sky cubemap + re-bake IBL (irradiance + prefilter). Auto, no manual.

**D3 — Stack: diharaw/sky-models (MIT) + escrito desde cero el IBL bake.** El IBL bake de Khronos glTF-IBL-Sampler (Apache-2.0) es monolítico y diseñado para Vulkan MRT — para nuestro stack OpenGL 4.3 con patrón face-by-face que ya existe, escribimos 2 shaders simples (irradiance.frag + prefilter.frag) desde el paper de Epic UE4 "Real Shading" (concepto público, no copia de código NC). LearnOpenGL DESCARTADO por licencia CC BY-NC.

**D4 — `skyboxSource` enum extiende EnvironmentComponent, NO reemplaza.** Back-compat: maps pre-F3H31 con `skyboxPath` cargan como `skyboxSource=HDRI`. JSON `skybox_source` opcional con default `"hdri"`.

**D5 — Sun direction calculada del time-of-day, opt-in sync con LightComponent.** Arco N-S simple: `azimuth = π` (sur), `elevation = sin((timeOfDay - 6) * π / 12) * π/2`. Si hay un `LightComponent` directional con flag nuevo `bindToSky=true`, su `direction` se override del time-of-day. Sin lat/long astronómica real (over-engineering).

**D6 — Cubemap procedural en GPU: 256×6 RGBA16F.** Match con resolución de los IBL prefilter mip0. Costo GPU: ~1.5MB por cubemap. Re-render ~3-5ms.

**D7 — Re-bake cost budget: ~150ms total.** Irradiance 32×32×6 + Prefilter 5 mips (128/64/32/16/8) × 6 faces × 1024 samples GGX = ~100-150ms en GPU mid-range. Re-bake corre OUT OF FRAME (no every-frame), solo en transitions de slider time-of-day.

---

## Implementación (compacta)

**1. Dataset Hosek-Wilkie copiado**: `src/engine/render/sky/hosek_data_rgb.inl` (de diharaw, MIT, 65KB, 270 coefs × 3 canales × 2 albedos × 10 turbidities como `static const double[]`).

**2. CPU-side helper** `src/engine/render/sky/HosekWilkie.h/.cpp` (~120 LOC):
- `struct HosekCoefficients { glm::vec3 A,B,C,D,E,F,G,H,I,Z; }`
- `compute_coefficients(sunDir, turbidity, groundAlbedo, normalizedSunY=1.15f)` → interpola del dataset → struct lista para upload uniforms.

**3. Sky shader** `assets/shaders/procedural_sky.vert/.frag` (~80 LOC GLSL):
- Vertex: fullscreen quad, `vDir = normalize((invVP * clip).xyz)`.
- Fragment: Hosek-Wilkie analítico con uniforms A..I, Z + sun direction.

**4. `ProceduralSkyRenderer`** `src/engine/render/sky/ProceduralSkyRenderer.h/.cpp` (~200 LOC):
- Mantiene cubemap 256×6 RGBA16F + FBO con 6 face attachments.
- `render(sunDir, turbidity, groundAlbedo)` → upload uniforms + render full quad por face con view matrix por face (6 lookAt).
- Returns el cubemap GL handle.

**5. IBL bake shaders** `assets/shaders/ibl_irradiance.frag` + `ibl_prefilter.frag` (~80 LOC c/u):
- Irradiance: cosine-weighted hemisphere sampling, 4096 samples uniformes.
- Prefilter: GGX importance sampling, 1024 samples, `uniform float uRoughness`.

**6. `IBLBaker`** `src/engine/render/sky/IBLBaker.h/.cpp` (~200 LOC):
- `bake(envCubemap)` → produce `{irradiance, prefilter}` cubemaps.
- Irradiance: 32×32×6 RGBA16F, 1 mip.
- Prefilter: 128×128×6 RGBA16F, 5 mips (roughness 0/0.25/0.5/0.75/1.0).

**7. Extender `EnvironmentComponent`** (`src/engine/scene/components/Components_Render.h`):
- `enum SkyboxSource { HDRI = 0, Procedural = 1 }`.
- `SkyboxSource skyboxSource = HDRI;` (back-compat default).
- `float timeOfDay = 12.0f;` (0-24h).
- `float turbidity = 2.5f;` (1-10, clear sky default).
- `glm::vec3 groundAlbedo{0.3f, 0.3f, 0.3f};` (gris medio).
- `bool skyDirty = true;` (transient, no serializa).

**8. Serialización** (`SceneSerializer.cpp` + `EntitySerializer.cpp` + `SceneLoader.cpp`):
- Persistir `skybox_source` ("hdri" | "procedural"), `time_of_day`, `turbidity`, `ground_albedo`.
- Maps pre-F3H31 sin estos campos → defaults safe.

**9. Integración SceneRenderer** (`SceneRenderer.cpp::applyEnvironmentFromScene`):
- Si `skyboxSource=Procedural` y `skyDirty`:
  - Call `m_proceduralSky->render(...)` → produce cubemap dinámico.
  - Call `m_iblBaker->bake(cubemap)` → produce irradiance + prefilter.
  - Reemplazar `m_iblIrradiance` y `m_iblPrefilter` con los nuevos.
  - Reemplazar `m_skyboxRenderer` con uno que use el cubemap dinámico.
- Si `skyboxSource=HDRI`: flow existente intacto.

**10. Sync con LightComponent**:
- Add `bool bindToSky = false;` a `LightComponent` (directional only).
- En `SceneRenderer::applyEnvironmentFromScene`, si hay un directional con `bindToSky=true`:
  - Compute sun direction del `timeOfDay`.
  - Override `light.direction = -sunDirToWorld`.

**11. UI Inspector Environment** (`InspectorPanel_Environment.cpp`):
- Dropdown "Skybox source": HDRI / Procedural.
- Si Procedural:
  - SliderFloat "Time of day" (0.0-24.0) con preview "06:30 / Mediodía / Atardecer / Noche".
  - SliderFloat "Turbidity" (1.0-10.0).
  - ColorEdit3 "Ground albedo".
  - Reset buttons ↺ en cada.
- Si HDRI: dropdown presets + file picker (intacto).
- Edits marcan `skyDirty = true`.

**12. Tests** (`tests/test_hosek_wilkie.cpp` nuevo):
- `compute_coefficients` para (sun zenith, T=2, albedo=0.3) devuelve valores no-NaN.
- `compute_coefficients` para sun sub-horizon (Y<0) devuelve cielo nocturno (Z atenuado).
- Sun direction de timeOfDay roundtrip (12h → Y>0, 0h/24h → Y<0).
- EnvironmentComponent serialization roundtrip incluyendo nuevos campos.

**13. `docs/THIRD_PARTY_LICENSES.md` NEW**: atribuciones diharaw MIT + Khronos Apache-2.0 + Hosek/Wilkie 2012 dataset.

---

## Backlog post-F3H31

- **F3H32**: Volumetric clouds raymarched (CaptainProton42 o portar Schneider HZD). ~15-18h.
- **F3H33**: Weather presets (clear/overcast/storm/sunset) + auto-cycle día/noche en gameplay runtime. ~6-10h.
- **Hillaire 2020 upgrade**: si emerge demanda de aerial perspective o atardeceres ultra-saturados, portar `JolifantoBambla/webgpu-sky-atmosphere`. Backlog.
- **Sun disc visible**: actualmente el sol es solo punto de iluminación, no disco renderizado. Agregable con `step(cos(M_PI/360), dot(dir, sun_dir))` en el sky shader.
- **Stars de noche**: cuando sol Y<-0.1, fade-in de starfield. Hito propio si emerge demanda.
- **Latitud/longitud astronómicas**: hoy sun rota arco N-S simple. Real sun position con lat/long es backlog (use case marginal para juego de pasillos HL1-style).

---

## Lo que NO toca F3H31

- Volumetric clouds (F3H32).
- Weather presets (F3H33).
- Auto-cycle día/noche runtime gameplay.
- Hillaire 2020.
- Aerial perspective scattering.
- Eclipses, lunas, multiple suns.

---

## Cierre — checklist

- [x] D1-D7 documentadas
- [x] Dataset Hosek-Wilkie + CPU helper
- [x] Sky shader Hosek-Wilkie + ProceduralSkyRenderer
- [x] IBL bake GPU (irradiance + prefilter)
- [x] EnvironmentComponent extendido + serialization
- [x] Integración SceneRenderer (re-bake trigger)
- [x] LightComponent bindToSky + sync
- [x] UI Inspector Environment (split header Cielo/Niebla + mini-resets por slider)
- [x] Tests Hosek-Wilkie + sun direction (6 cases pass)
- [x] THIRD_PARTY_LICENSES.md
- [x] Build verde + validación visual confirmada por el dev
- [x] Docs cierre + commits + tag F3H31 + push

---

## Ajustes reactivos post-validación visual

**R1 — Outline reflejado por SSR/Bloom/AO.** El dev reportó (con luz puntual + SSR/Bloom/AO activos) que el outline amarillo del cubo seleccionado aparecía "fantasma" reflejado en el piso (SSR), con halo (Bloom) y oscurecido (SSAO). Causa: el `debugRenderer->flush()` (que dibuja outlines/AABBs/gizmos) se ejecutaba al inicio de `endFrame()` sobre el `m_sceneFb` HDR — los pases SSR/Bloom/SSAO leían ese FB con los overlays dentro. Fix: mover el flush al FINAL de `endFrame()` (post-tonemap) bindeando `m_viewportFb` LDR + blit del depth de `m_sceneFb` (para que el z-test del debug shader funcione vs la geometría). `OpenGLFramebuffer` gana `GLuint glHandle()` getter para `glBlitFramebuffer` con bindings READ/DRAW separados.

**R2 — Dropdown "Origen del cielo" mostraba "????".** Bug de lifetime en C++ — `I18n::T(...).c_str()` devuelve puntero al `std::string` temporal; el array initializer `const char* sourceLabels[] = { I18n::T(...).c_str(), ... }` quedaba con punteros a memoria liberada apenas cerrado el bloque. Idéntico al bug documentado en líneas ~779-790 del mismo archivo (`InspectorPanel_Environment.cpp`) para el preview del LUT preset de Color Grading. Fix: capturar `lblHdri` / `lblProc` como `std::string` locales con lifetime que cubre el `ImGui::Combo`.

**R3 — Header "Origen del cielo" vivía bajo "Niebla".** Tras fix R2, el dev pidió separar visualmente — convención Unity Lighting / Unreal Sky & Atmosphere: sky es su propia sección top-level del Environment, no sub-header de fog. Refactor `drawEnvSkyAndFog` → `drawEnvSky` + `drawEnvFog` con headers independientes. Reset global por sección preservado, ahora resetea sky-only (skyboxSource + skyboxPath + 3 params procedural) o fog-only.

**R4 — Mini-reset (↺) por slider individual.** Pedido del dev: *"falta los botones de reset al final de cada slider"* — patrón ya establecido en F3H30 UV reset (`InspectorPanel_Brush.cpp`). Helper lambda `skyResetFloat` / `skyResetVec3` inline en `drawEnvSky`: SameLine + `SmallButton(ICON_FA_ROTATE_LEFT)` que pushea `pushAtomicEdit<T>` con el default de `kEnvDefaults`. Cada mini-reset es undoable como cualquier edit manual; coexiste con el reset global de la sección (que sigue restaurando todos los fields a la vez).
