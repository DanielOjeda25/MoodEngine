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

- [ ] Compila sin warnings nuevos.
- [ ] Tests: helpers numéricos del fog (factor de fog según distancia + densidad) + parser del cubemap (6 caras desde paths lógicos).
- [ ] Cierre limpio.

### Visuales

- [ ] Detrás de la escena se ve un skybox real (default: gradient day-sky generado a mano si no se carga uno).
- [ ] Mover la cámara (orbital o FPS) muestra el skybox infinito sin parallax.
- [ ] Fog activable desde un panel — al subir densidad, los muros distantes se difuminan en el color del fog.
- [ ] Toggle "tonemap on/off" muestra la diferencia entre RGBA8 directo y RGBA16F → tonemap → gamma.
- [ ] Cambiar exposición en runtime se ve sin relanzar.

---

## Bloque 0 — Pendientes arrastrados del Hito 14

- [ ] Drop de prefab reemplaza entidad existente (queda diferido nuevamente si no aparece caso de uso).
- [ ] Thumbnails 3D del AssetBrowser (también arrastrado de Hito 10/14). Si entra en este hito sería un side-effect del FB de preview, evaluable después del Bloque 3.

## Bloque 1 — Cubemap + skybox

- [ ] `engine/render/ICubemapTexture.h` o extensión de `ITexture` para 6 caras. Decisión documentada acá.
- [ ] Backend OpenGL: `glTexImage2D` x 6 con `GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z`. Filtros LINEAR + clamp_to_edge.
- [ ] `assets/skyboxes/<name>/{px,nx,py,ny,pz,nz}.png` (6 PNG). Convención de naming inspirada en KTX/Khronos.
- [ ] Sample default `assets/skyboxes/sky_day/` generado con Python (gradient azul claro → naranja en `+Y`, oscuro abajo). Tools/script que regenere si hace falta.
- [ ] `shaders/skybox.{vert,frag}`: vertex elimina el componente W (cube proyectado al far plane), fragment muestra `texture(cubemap, dir)`. `gl_Position.z = gl_Position.w` para que el depth quede en 1.
- [ ] Render order: `clear` → `skybox` (depth test LEQUAL) → escena scene-driven. El skybox queda detrás de TODO porque su depth=1.

## Bloque 2 — Fog en `lit`

- [ ] Uniforms nuevos en `lit.frag`: `uFogColor: vec3`, `uFogDensity: float`, `uFogMode: int` (0=off, 1=lineal, 2=exp, 3=exp2).
- [ ] Cálculo: distancia desde el frag al ojo (`length(uCameraPos - worldPos)`) → factor 0..1 → `mix(litColor, uFogColor, factor)`.
- [ ] Lineal: `(d - near) / (far - near)`. Exp: `1 - exp(-density*d)`. Exp2: `1 - exp(-(density*d)^2)`.
- [ ] Tests numéricos: `applyFog(litColor, fogColor, density, distance, mode)` en una función `engine/render/Fog.h` testeable sin GL.

## Bloque 3 — Render target HDR + post-process

- [ ] `IFramebuffer` gana un parámetro `format` (default `RGBA8`, nuevo `RGBA16F`). El backend OpenGL lo materializa con `glTexImage2D(GL_RGBA16F, ...)`.
- [ ] `EditorApplication` ahora tiene DOS framebuffers para el viewport: `m_sceneFb` (HDR, donde se pinta la escena + skybox + fog) y `m_finalFb` (LDR, lo que ImGui muestra). El segundo se mantiene RGBA8 porque ImGui asume sRGB.
- [ ] `shaders/post_process.{vert,frag}`: fullscreen triangle (single triangle truc), samplea `uHdrTex` y aplica `exp(uExposure) * color`, luego `Reinhard` (`x / (1+x)`) o `ACES` por uniforms, luego `pow(color, 1/2.2)`.
- [ ] Pase post-process: bind `m_finalFb` → fullscreen quad con shader `post_process` → ImGui muestra `m_finalFb`. `m_sceneFb` queda invisible al usuario.

## Bloque 4 — UI: panel "Render Settings" o EnvironmentComponent

- [ ] Decisión: ¿panel global del editor o componente en una entidad? Opción más limpia para serializar = `EnvironmentComponent` en una entidad "Environment" creada por `rebuildSceneFromMap`.
- [ ] Campos: `skyboxPath`, `fogColor`, `fogDensity`, `fogMode (combo)`, `exposure (-5..+5)`, `tonemapMode (combo: None/Reinhard/ACES)`.
- [ ] Inspector renderiza la sección con los widgets. Persistencia: `EntitySerializer` aprende a leer/escribir `EnvironmentComponent`.
- [ ] Schema bump si aplica: si los defaults no cambian comportamiento de archivos viejos, `k_MoodmapFormatVersion` se queda en 5. Si el field requiere semantica nueva, bump a 6.

## Bloque 5 — Tests + cierre

- [ ] Tests: `tests/test_fog.cpp` con casos numéricos para los 3 modos a distintas distancias.
- [ ] Tests: round-trip `EnvironmentComponent` en `.moodmap`.
- [ ] Sample skybox (`sky_day/`) commiteado.
- [ ] Smoke test visual: arranca con sky default, cambiar fog density en Inspector cubre/descubre la sala. Tonemap toggle preserva contraste.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español. Tag `v0.15.0-hito15` + push.
- [ ] Crear `docs/PLAN_HITO16.md` (Shadow mapping).

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

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 16 o posterior

_(llenar al cerrar el hito)_
