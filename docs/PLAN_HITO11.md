# Plan — Hito 11: Iluminación Phong / Blinn-Phong

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 4 (shaders) y sección 10 (Hito 11) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Que las escenas dejen de ser "unlit" (textura directa sin sombreado) y pasen a iluminarse con Phong/Blinn-Phong. El `LightComponent` que es placeholder desde el Hito 7 pasa a ser real.

- Atributo de normal en vertices + normales calculadas por `aiProcess_GenNormals` (ya activado en Hito 10).
- Shader `lit.{vert,frag}` reemplaza/complementa `default.{vert,frag}`. Modelo: Blinn-Phong (ambient + diffuse + specular).
- Soporte de luces: 1 `DirectionalLight` + hasta N `PointLight` (N acotado, p.ej. 8). `SpotLight` si entra sin desviarse.
- `LightSystem` recolecta los `LightComponent` de la scene cada frame y los sube como uniforms.
- Normal map opcional (2do slot de textura por submesh); si no hay normal map, se usa la normal del vertice.
- Demo: `Ayuda > Agregar luz puntual demo` spawnea una entidad con `LightComponent(Point, color blanco, intensidad 1.0)` a una altura visible. Moverla via Inspector (position/color/intensity) actualiza la escena en vivo.

No-goals del hito: PBR (Hito 17), shadow mapping (Hito 16), deferred/forward+ (Hito 18), HDR (Hito 15), IBL.

---

## Criterios de aceptación

### Automáticos

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Shader `lit.{vert,frag}` copiado post-build junto al exe.
- [ ] Tests: helpers de iluminación (transformación de normales con inverse transpose; atenuación de point light en una distancia dada; acumulación Phong con 1 directional + 1 point).
- [ ] Cierre limpio, exit 0.

### Visuales

- [ ] La sala 8x8 muestra sombreado: las caras frente a la luz son más brillantes que las opuestas.
- [ ] La pirámide importada del Hito 10 muestra highlights especulares cuando una point light está cerca.
- [ ] Mover la luz en tiempo real (Inspector) actualiza la escena frame a frame.
- [ ] Cambiar color / intensidad de la luz en el Inspector se refleja en el viewport sin relanzar el editor.

---

## Bloque 0 — Pendientes arrastrados del Hito 10

- [ ] **Preview 3D de meshes en el AssetBrowser** — pendiente del usuario. Puede entrar si al final del hito queda tiempo; si no, queda diferido nuevamente.
- [ ] **Hot-reload de shaders**: útil para iterar Phong sin relanzar cada vez. Condición de disparo listada en Hito 2 pendientes. Este hito es el natural para activarlo.

## Bloque 1 — Normales en el pipeline

- [ ] `VertexAttribute` layout extendido: pos(3) + color(3) + uv(2) + normal(3) = stride 11 floats. Location 3 para normal.
- [ ] `createCubeMesh()` (primitivo del AssetManager slot 0) genera normales correctas por cara (no interpoladas — caras duras, pared/piso clásico).
- [ ] `MeshLoader::loadMeshWithAssimp` guarda las normales que `aiProcess_GenNormals` calcula (hoy las descarta). Si el import falla sin normales, las genera en CPU como fallback.

## Bloque 2 — Shader `lit.{vert,frag}`

- [ ] `shaders/lit.vert`: transforma pos al world + normal por `mat3(transpose(inverse(uModel)))`. Pasa `worldPos`, `worldNormal`, `uv` al fragment.
- [ ] `shaders/lit.frag`: modelo Blinn-Phong con `ambient + diffuse + specular`. Lee `uCameraPos`. Itera sobre un array `uPointLights[MAX]` + `uDirectionalLight`. Muestreo de `uTexture` como `albedo`.
- [ ] `uniform int uActivePointLights` para evitar iterar slots vacíos.
- [ ] Constantes: `MAX_POINT_LIGHTS = 8`. Si el contador pasa de 8, warn al loguear (Hito 18+ mitigará con deferred).
- [ ] Compilar via `OpenGLShader`; reemplaza `default.{vert,frag}` para todas las entidades con MeshRenderer. `default.{vert,frag}` se conserva para debug lines + UI (sin iluminación).

## Bloque 3 — LightSystem

- [ ] `src/systems/LightSystem.{h,cpp}`: recolecta LightComponents de la scene cada frame.
- [ ] API: `buildFrameData(Scene&, glm::vec3 cameraPos) -> LightFrameData` donde `LightFrameData` tiene el directional light elegido (primer `LightComponent::Type::Directional` que encuentre) + array de point lights (primeros MAX encontrados).
- [ ] Upload de uniforms al shader `lit`: el caller (EditorApplication) pasa la struct al shader antes de dibujar. Convención: `bindUniforms(IShader&)` en LightSystem.
- [ ] `LightComponent` gana: `radius` (distancia máxima de influencia, atenuación cuadrática por defecto), `direction` (para directional; sobreescribe a derivar de TransformComponent). `intensity` sigue en [0, +inf); 1.0 es "luna llena".

## Bloque 4 — Inspector + demo

- [ ] `InspectorPanel`: sección `Light` ampliada (antes placeholder) con sliders reales — intensity, radius, color (ColorEdit3 HDR). Cambios en vivo.
- [ ] Agregar `TransformComponent` opcional a la sección (si la entidad no tiene — rare).
- [ ] Item de menú `Ayuda > Agregar luz puntual demo` spawnea una entidad con `TagComponent("Luz demo") + TransformComponent(0, 4, 0) + LightComponent(Point, white, 1.0, radius=10)`. Log: `Spawned luz puntual en (0, 4, 0)`.
- [ ] Demo: al spawnear, la sala se nota iluminada "desde arriba".

## Bloque 5 — Serialización (extension menor)

- [ ] `SavedEntity` gana `SavedLight` opcional (`{type, color, intensity, direction?, radius?}`). `SceneSerializer` lo serializa para entidades no-tile con `LightComponent`. Schema: igual patrón que `mesh_renderer`.
- [ ] `k_MoodmapFormatVersion` 2 -> 3. Archivos v2 siguen leyendo (light field opcional).
- [ ] Test round-trip: entidad con LightComponent + sin otros componentes debe persistir type/color/intensity/radius.

## Bloque 6 — Hot-reload de shaders (pendiente Hito 2)

- [ ] `OpenGLShader` gana `reload()` — reabre los dos `.vert/.frag`, compila, swap atomic si OK. Log al canal `render`.
- [ ] Botón en algún panel (ConsolePanel o un nuevo Debug) que dispara reload. Alternativa: watcher de mtime análogo al de ScriptSystem.
- [ ] Útil en este hito porque el fragment shader tiene matemática no trivial.

## Bloque 7 — Tests

- [ ] `tests/test_lighting.cpp`: tests puros de helpers numéricos — transformación de normal con inverse transpose, atenuación, acumulación.
- [ ] Extender round-trip de entidades para LightComponent.

## Bloque 8 — Cierre

- [ ] Recompilar, tests verdes, demo visible (luz demo ilumina la sala, el dev puede mover la luz en Inspector).
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.11.0-hito11` + push.
- [ ] Crear `docs/PLAN_HITO12.md` (física con Jolt).

---

## Qué NO hacer en el Hito 11

- **No** PBR. Blinn-Phong es suficiente para AAA 2010-2015 (el target estético del motor).
- **No** shadow mapping. Sombras vienen en Hito 16.
- **No** deferred / forward+. Forward simple con MAX_POINT_LIGHTS=8 alcanza.
- **No** IBL / skybox reflection. Hito 15 (skybox+fog).
- **No** HDR. Render a RGBA8 como hoy. HDR es Hito 15.
- **No** normal mapping avanzado (parallax, displacement). Normal map simple entra si hay tiempo en Bloque 4; si no, Hito 17.
- **No** area lights / tube lights / rect lights.
- **No** volumetric lighting.

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 12 o posterior

_(llenar al cerrar el hito)_
