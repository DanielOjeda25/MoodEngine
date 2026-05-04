# Plan — Hito 15: Skybox, fog y post-procesado

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 15) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Que el viewport se vea como una escena 3D moderna y no un viewport plano:
- **Skybox** dibujado como cubemap detrás de la escena (depth=1, 6 caras de textura).
- **Fog** exponencial / lineal aplicado en el shader `lit`, con color y densidad editables.
- **Render target HDR** (RGBA16F) en lugar de RGBA8 — habilita exposición y tonemapping reales.
- **Pase de post-procesado**: framebuffer HDR → fullscreen quad con shader que aplica exposición + tonemapping (Reinhard primero; ACES si entra fácil) + gamma correction → framebuffer LDR (RGBA8) que mostramos en el viewport.

Todo editable en runtime desde un panel "Render Settings" o desde un componente nuevo `EnvironmentComponent` en una entidad sentinel de la escena. Decisión de diseño parte del Bloque 1.

No-goals del hito: bloom (entra en Hito 18 si la pipeline lo justifica), SSAO, depth of field, motion blur, volumetric fog, IBL del skybox para iluminación (entra con PBR Hito 17).

---

## Criterios de aceptación

### Automáticos

- [x] Compila sin warnings nuevos.
- [x] Tests: helpers numéricos del fog (factor de fog según distancia + densidad) + parser del cubemap (6 caras desde paths lógicos).
- [x] Cierre limpio.

### Visuales

- [x] Detrás de la escena se ve un skybox real (default: gradient day-sky generado a mano si no se carga uno).
- [x] Mover la cámara (orbital o FPS) muestra el skybox infinito sin parallax.
- [x] Fog activable desde un panel — al subir densidad, los muros distantes se difuminan en el color del fog.
- [x] Toggle "tonemap on/off" muestra la diferencia entre RGBA8 directo y RGBA16F → tonemap → gamma.
- [x] Cambiar exposición en runtime se ve sin relanzar.

---

## Bloque 0 — Pendientes arrastrados del Hito 14

- [ ] Drop de prefab reemplaza entidad existente (sigue diferido; no apareció caso de uso concreto).
- [ ] Thumbnails 3D del AssetBrowser (también arrastrado de Hito 10/14). No entró en este hito.

## Bloque 1 — Cubemap + skybox

- [x] `engine/render/opengl/OpenGLCubemapTexture.{h,cpp}`: wrapper de `GL_TEXTURE_CUBE_MAP` con 6 caras. Filtros LINEAR + clamp_to_edge en S/T/R para evitar seams. NO setea `stbi_set_flip_vertically_on_load(true)` (las caras del cubemap esperan top-left, distinto de las texturas 2D).
- [x] **Decisión**: no abstraer ICubemapTexture todavía — un solo backend, un solo consumidor (`SkyboxRenderer`). La interfaz se extrae cuando aparezca un segundo (IBL en Hito 17).
- [x] `assets/skyboxes/sky_day/{px,nx,py,ny,pz,nz}.png` generados por `tools/gen_sky_cubemap.py` (Pillow). Gradient cenit→horizonte, accent sunset sutil en `+Z`.
- [x] `shaders/skybox.{vert,frag}`: vertex anula la translación de view (`mat4(mat3(view))`) y fuerza `gl_Position.z = w` (depth=1). Fragment samplea el cubemap con la dirección del vertex.
- [x] `systems/SkyboxRenderer.{h,cpp}`: owns cubemap + shader + cubo unitario de 36 verts. `draw(view, projection)` hace switch temporal a `GL_LEQUAL` + desactiva culling, dibuja, restaura state.
- [x] `EditorApplication`: construye el SkyboxRenderer con try/catch — si la carga falla, queda nullptr y el motor sigue con clear color visible.

## Bloque 2 — Fog en `lit`

- [x] `engine/render/Fog.h` (header-only): `FogMode {Off, Linear, Exp, Exp2}` + `FogParams` + `computeFogFactor` + `applyFog`. Misma matemática que el shader, testeable sin GL.
- [x] `shaders/lit.frag` extendido con uniforms `uFogMode/uFogColor/uFogDensity/uFogStart/uFogEnd` + función `computeFogFactor(distance)` que replica el helper. Aplicado post-iluminación con `mix(lighting, uFogColor, factor)`.
- [x] `EditorApplication` setea los uniforms cada frame. Valores vienen del frame state (`m_fog`) que el `applyEnvironmentFromScene` regenera desde el `EnvironmentComponent` o defaults.
- [x] `tests/test_fog.cpp`: 6 casos cubriendo Off, Linear (con limites + degenerado), Exp (asintótico), Exp2 vs Exp comparados, `applyFog` end-to-end.

## Bloque 3 — Render target HDR + post-process

- [x] `OpenGLFramebuffer` gana `enum class Format {LDR, HDR}`. HDR usa `GL_RGBA16F`; LDR sigue siendo `GL_RGBA8`. Default LDR para no romper callsites.
- [x] `OpenGLFramebuffer::glColorTextureId()` getter (no en IFramebuffer) para que el PostProcessPass samplee sin filtrar el GLuint via la interfaz abstracta.
- [x] `EditorApplication` ahora tiene DOS framebuffers para el viewport: `m_sceneFb` (HDR, donde se dibuja sky + escena + lit + fog + debug) y `m_viewportFb` (LDR, lo que ImGui muestra).
- [x] `shaders/post_process.{vert,frag}`: fullscreen triangle generado por `gl_VertexID` (sin VBO). Aplica `2^exposure` + tonemap (None / Reinhard / ACES Narkowicz) + gamma 1/2.2.
- [x] `systems/PostProcessPass.{h,cpp}`: owns shader + VAO trivial. `apply(src, dst, exposure, tonemap)` cambia depth/cull state durante el draw.

## Bloque 4 — UI: EnvironmentComponent

- [x] **Decisión**: NO un panel global "Render Settings" — el `EnvironmentComponent` se agrega a una entidad cualquiera (convención: un objeto vacío llamado "Environment"). Si hay varias entidades con el componente, gana la primera; las demás se ignoran. Si no hay ninguna, se usan los defaults.
- [x] `EnvironmentComponent` en `Components.h` con: skyboxPath (placeholder hasta catálogo de cubemaps), fogMode/Color/Density/LinearStart/LinearEnd, exposure, tonemapMode.
- [x] `EditorApplication::applyEnvironmentFromScene` helper: reset a defaults + apply del primer Environment encontrado. Llamado por `renderSceneToViewport` cada frame Y por `tryOpenProjectPath` justo después de cargar las entidades — así la primera frame post-load ya muestra los valores guardados sin un flash a defaults.
- [x] Inspector sección Environment con Combo (modo fog), ColorEdit (color fog), DragFloat (density / start / end / exposure), Combo (tonemap).
- [x] Item de menú `Ayuda > Agregar Environment`. Si ya hay uno, selecciona el existente.
- [x] Serialización: `SavedEnvironment` opcional en `SavedEntity`. EntitySerializer lo lee/escribe con strings para fogMode/tonemapMode.
- [x] Schema bump: `k_MoodmapFormatVersion` 5 → 6 con backward-compat. `tryOpenProjectPath` reaplica el componente.
- [x] **Bug fix detectado en smoke test**: el bloque 4 inicial NO reseteaba m_fog antes del scan, así que abrir un proyecto sin Environment heredaba los valores del proyecto previo. Fix: reset a defaults en cada frame antes del scan.

## Bloque 5 — Tests + cierre

- [x] `tests/test_fog.cpp` (Bloque 2, 6 casos).
- [x] `tests/test_scene_serializer.cpp` extendido: round-trip de `EnvironmentComponent` con todos los campos.
- [x] Suite total **120 / 530** (antes 113 / 493).
- [x] Smoke test visual: skybox visible, fog viviendo el viewport, exposure / tonemap reactivos, Environment se persiste y reaplica.
- [x] **Polish post-bloque (no en plan original, fix UX)**:
  - Memoria nueva `feedback_no_autoopen_project.md`: el editor SIEMPRE arranca con el Welcome modal. `loadEditorState` ya no auto-abre el último proyecto; solo puebla la lista de recientes para alimentar el modal.
  - Welcome modal con UX para limpiar la lista: botón "Limpiar inexistentes" + botón "X" por entrada + indicador rojo `(no existe)`. `EditorUI::eraseRecent` y `pruneMissingRecents` con flag dirty que el EditorApplication consume y persiste.
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español. Tag `v0.15.0-hito15` + push.
- [x] Crear `docs/PLAN_HITO16.md` (Shadow mapping).

---

## Qué NO hacer en el Hito 15

- **No** bloom ni SSAO ni DOF — Hito 18+.
- **No** IBL del skybox para iluminación de la escena — Hito 17 (PBR).
- **No** sky procedural (Hosek-Wilkie / Preetham). Cubemap estático alcanza.
- **No** volumetric fog. Fog uniforme via factor por fragment.
- **No** múltiples capas de skybox / parallax sky. Una sola cubemap.
- **No** anti-aliasing del post-process pass (FXAA/TAA). El render directo a 1280x720 con MSAA no tiene problema visible aún.

---

## Decisiones durante implementación

Detalle en `DECISIONS.md` bajo fecha 2026-04-26 (Hito 15).

- **No abstraer `ICubemapTexture`** todavía. Un backend, un consumidor; la interfaz se extrae cuando entre IBL (Hito 17).
- **HDR via segundo framebuffer + post-process pass**, no un solo FB sRGB con extensiones modernas. Mantiene la pipeline explícita y portable.
- **`EnvironmentComponent` en una entidad cualquiera** en vez de un panel global o un singleton. Reusa la persistencia/inspección del ECS sin código nuevo.
- **Reset a defaults antes del scan cada frame**: bug encontrado en smoke test cuando se abrió un proyecto sin Environment después de uno con fog verde — los valores del previo se "colaban".
- **El editor NO auto-abre el último proyecto**. Decisión explícita del dev en Hito 6 que se había violado con el auto-restore. Welcome modal aparece SIEMPRE al arrancar.
- **Limpieza manual de recientes** desde el modal (botón "Limpiar inexistentes" + "X" por entrada). Necesario porque sin auto-open la lista de recientes es la única vía rápida y los proyectos borrados ensucian la lista.

---

## Pendientes que quedan para Hito 16 o posterior

- **Catálogo de skyboxes** (cubemap por proyecto). Hoy se usa el cubemap fijo cargado al iniciar (`sky_day`). El campo `skyboxPath` del `EnvironmentComponent` se persiste pero no se aplica todavía. **Trigger:** cuando aparezcan ≥2 skyboxes en el proyecto.
- **Bloom** post-process. **Trigger:** cuando los highlights HDR realmente se vean apagados sin él (escenas más complejas + PBR).
- **Anti-aliasing** (MSAA del scene FB o FXAA en post-process). **Trigger:** quejas visibles de jaggies en escenas con muchas líneas finas.
- **Skybox procedural** (atmospheric scattering). **Trigger:** demo "ciclo dia/noche".
- **`ICubemapTexture` abstracta** + factory en AssetManager (catálogo). **Trigger:** entrada de IBL en Hito 17 que también consume cubemaps.
- **Drop de prefab reemplaza entidad existente** y **thumbnails 3D** siguen como pendientes de hitos previos.
