# Plan — Hito 16: Shadow mapping

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 16) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Sombras dinámicas de la directional light:
- **Shadow map** del directional light en un framebuffer depth-only (`GL_DEPTH_COMPONENT`).
- **Pase de depth** que renderiza la escena desde la perspectiva de la luz, antes del pase de color.
- **Sample en `lit.frag`** del shadow map para decidir si cada fragment está en sombra.
- **PCF** (Percentage-Closer Filtering) 3×3 para bordes suaves.
- Bias configurable para evitar shadow acne.

No-goals: cascadas (CSM) — quedan como pendiente si entran fácil pero el plan estricto los difiere a un hito dedicado. Tampoco shadow maps para point/spot lights (ese sería Hito 16.5 o futuro). Nada de soft shadows variables (PCSS, VSM).

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: helpers de proyección de la luz (matrices `lightView` + `lightProj` correctas para distintos `direction` y radio del frustum).
- [ ] Cierre limpio.

### Visuales

- [ ] Una directional light con shadow casting habilitado proyecta sombras de los muros del mapa contra el piso (creado por una entidad demo plana o por un tile en `-Y`).
- [ ] El rotador demo proyecta sombra al moverse.
- [ ] PCF se nota: los bordes son suaves, no aliased.
- [ ] Toggle de "casts shadows" en `LightComponent` se ve en vivo (al apagarlo, las sombras desaparecen sin relanzar).

---

## Bloque 0 — Pendientes arrastrados del Hito 15

- [ ] Catálogo de skyboxes (skyboxPath del EnvironmentComponent ya persiste pero no se aplica) — diferido si no entra natural acá.
- [ ] Anti-aliasing (FXAA / MSAA): cuando los jaggies del shadow map y la geometría se sumen, puede que sea natural agregarlo aquí.

## Bloque 1 — Depth-only framebuffer + shader

- [ ] `OpenGLFramebuffer` gana `Format::Depth` (o un nuevo `OpenGLDepthFramebuffer`): textura `GL_DEPTH_COMPONENT24` + sin color attachment + `glDrawBuffer(GL_NONE) / glReadBuffer(GL_NONE)`.
- [ ] Tamaño default 2048×2048 — suficiente para una sala 8×8.
- [ ] `shaders/shadow_depth.{vert,frag}`: vertex aplica `lightProj * lightView * model * pos`. Fragment vacío (depth se escribe automático).

## Bloque 2 — Pase de depth desde la luz

- [ ] `systems/ShadowPass.{h,cpp}`: owns el FB depth + shader. `record(Scene&, lightDir, sceneCenter, sceneRadius)`:
  - Calcular `lightView` con `glm::lookAt(sceneCenter - lightDir * sceneRadius, sceneCenter, up)`.
  - Calcular `lightProj` con `glm::ortho(-r, r, -r, r, 0, 2*r)` donde `r` cubre la escena (sceneRadius con margen).
  - Renderizar la escena con el shadow_depth shader, sin texturas ni iluminación.
- [ ] Front-face culling durante el shadow pass para reducir peter-panning (Decisión documentada).
- [ ] Llamado por `EditorApplication::renderSceneToViewport` ANTES del bind del scene FB.

## Bloque 3 — Sample en `lit.frag`

- [ ] `lit.vert` calcula `vLightSpacePos = lightProj * lightView * worldPos`.
- [ ] `lit.frag` recibe `uShadowMap: sampler2DShadow` (o `sampler2D` con compare manual) + `uShadowBias` + `uShadowEnabled`.
- [ ] Función `shadowFactor(lightSpacePos)` con PCF 3×3 que devuelve 1.0 (no en sombra) o ~0.0 (en sombra), suavizado.
- [ ] La directional light queda multiplicada por `shadowFactor` en su contribución al lighting.

## Bloque 4 — `LightComponent` extendido

- [ ] Campo nuevo `bool castShadows = false` (default off). Solo aplicable a Directional en este hito.
- [ ] Inspector lo expone como checkbox.
- [ ] Si una directional light tiene `castShadows`, `EditorApplication` ejecuta el ShadowPass + setea el sampler en `lit`.
- [ ] Si no hay ninguna o si `castShadows=false`, se setea `uShadowEnabled=0` y el lit shader saltea el sample.

## Bloque 5 — Demo + tests

- [ ] Item de menú `Ayuda > Agregar luz direccional con sombras demo` (default direction `(-0.3, -1, -0.4)` normalizado, intensity 1.5, castShadows=true). O extender el demo existente.
- [ ] `tests/test_shadow_proj.cpp`: dado un lightDir + sceneCenter + sceneRadius, verificar que `lightView * sceneCenter` queda en `(0, 0, -sceneRadius)` aprox (la cámara de la luz mira al centro), y que `lightProj * lightView` mapea los extremos del bounding sphere al rango [-1, 1].
- [ ] Round-trip de `castShadows` en `.moodmap` — schema bump 6 → 7 si hace falta (pero como es campo opcional al parsear con default false, NO requiere bump — queda en 6).

## Bloque 6 — Cierre

- [ ] Recompilar, tests verdes, demo visible (rotador + muros con sombras suaves).
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español. Tag `v0.16.0-hito16` + push.
- [ ] Crear `docs/PLAN_HITO17.md` (PBR).

---

## Qué NO hacer en el Hito 16

- **No** cascade shadow maps (CSM). Si una sola cascade no alcanza para escenas grandes, se agrega en hito dedicado.
- **No** shadow maps de point lights (cubemap shadow). Default es directional only.
- **No** spot lights ni sus shadows.
- **No** PCSS / VSM / soft shadows variables. PCF 3×3 fijo alcanza.
- **No** shadow bias automático. Slider en Inspector + default razonable.
- **No** ESM / EVSM (exponential / variance shadow maps). Hito de optimización futuro.

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 17 o posterior

_(llenar al cerrar el hito)_
