# Plan — Hito 3: Cubo texturizado con cámara

> **Leer primero:** `ESTADO_ACTUAL.md` (estado global), `DECISIONS.md` (decisiones acumuladas), sección 10 y 11 de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar una tarea, marcar `[x]`. Decisiones no previstas van acá y en `DECISIONS.md`.

---

## Objetivo

El motor deja de dibujar un triángulo plano en NDC y pasa a renderizar un **cubo 3D texturizado rotando**, visto a través de una cámara. Hay dos modos:

- **Editor Mode**: cámara de editor orbital (rotación alrededor del origen con click-drag del ratón, zoom con la rueda).
- **Play Mode**: cámara FPS controlable con WASD + ratón (mouse look). Al entrar en Play, el ratón se captura; al salir (Esc), vuelve al editor.

Este hito consolida el pipeline 3D completo: matrices MVP, texturas con stb_image, cámara, primer sistema de input, y primer modo de juego.

---

## Criterios de aceptación

### Automáticos

- [ ] `cmake --preset windows-msvc-debug` sigue configurando sin errores.
- [ ] `cmake --build build/debug --config Debug` compila sin warnings nuevos.
- [ ] El log incluye nueva línea del canal `render`: `Textura cargada: <ruta> (WxH, N canales)`.
- [ ] Se ejecutan los tests de `tests/test_math.cpp` con doctest y pasan todos.
- [ ] Cierre limpio sin leaks de GL.

### Visuales

- [ ] Dentro del panel Viewport se ve un cubo 3D con una textura aplicada (ej. grid o crate), rotando lentamente alrededor del eje Y.
- [ ] En Editor Mode (arranque por defecto): click-drag con botón derecho del ratón orbita la cámara alrededor del cubo; rueda del ratón hace zoom in/out.
- [ ] Status bar muestra `Editor Mode` o `Play Mode` según el estado actual.
- [ ] Apretar el botón "Play" (nuevo, en la menu bar o status bar) cambia a Play Mode: ratón capturado, WASD mueve la cámara, mouse look rota la vista en primera persona.
- [ ] Apretar `Esc` en Play Mode vuelve a Editor Mode y libera el ratón.
- [ ] El cubo NO se estira al redimensionar el Viewport: el aspect ratio se recalcula.
- [ ] El resto del editor (menús, paneles, layout) sigue funcionando.

---

## Tareas

### Bloque 1 — stb_image

- [ ] Bajar `stb_image.h` y `stb_image_write.h` desde https://github.com/nothings/stb/tree/master/deprecated — usar los headers single-file (típicamente `stb_image.h` y `stb_image_write.h` en la raíz del repo stb).
- [ ] Colocarlos en `external/stb/` (reemplazando el placeholder README por uno real con link y versión).
- [ ] Agregar target interface `stb` al `CMakeLists.txt`: `add_library(stb INTERFACE)` + `target_include_directories(stb INTERFACE external/stb)`.
- [ ] Linkear `stb` a `MoodEditor`.
- [ ] Crear un archivo `.cpp` pequeño (p.ej. `src/engine/assets/stb_impl.cpp`) que haga `#define STB_IMAGE_IMPLEMENTATION` + `#include <stb_image.h>` (y equivalente para `stb_image_write` si se usa en el hito).

### Bloque 2 — ITexture y OpenGLTexture

- [ ] `src/engine/render/ITexture.h`: interfaz con `bind(slot=0)`, `unbind()`, `width()`, `height()`, `handle()` (devuelve `TextureHandle`).
- [ ] `src/engine/render/opengl/OpenGLTexture.{h,cpp}`:
  - Constructor desde path: usa `stbi_load`, crea `GL_TEXTURE_2D`, setea filtros (linear por defecto, mipmap cuando llegue), wrapping `GL_REPEAT`.
  - Detecta formato según canales (RGB, RGBA, grayscale).
  - Libera con `glDeleteTextures` en destructor.
- [ ] Agregar un asset de ejemplo: `assets/textures/grid.png` o `crate.png` (de dominio público, NO descargar de sitios con licencia restrictiva; alternativa: generar un grid procedural con `stb_image_write` al arrancar la primera vez si no existe).

### Bloque 3 — Matrices MVP

- [ ] Extender `IShader` con `setMat4` ya existente (OK).
- [ ] Extender `shaders/default.vert` para recibir `uniform mat4 uModel`, `uniform mat4 uView`, `uniform mat4 uProjection` y aplicar `gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0)`.
- [ ] Extender `shaders/default.frag` para opcionalmente samplear una textura (uniform `sampler2D uTexture`). Si se quiere mantener el color por vértice también, combinar: `FragColor = texture(uTexture, vUv) * vec4(vColor, 1.0)`.
- [ ] Actualizar `shaders/default.vert` para recibir `aUv` (vec2) y pasarlo a `vUv`.

### Bloque 4 — Cubo con UVs

- [ ] Actualizar `OpenGLMesh` para aceptar un layout con `aPos (vec3) + aColor (vec3) + aUv (vec2)` (stride = 8 floats).
- [ ] Crear el cubo en `EditorApplication` (o mejor en un helper `MeshBuilder::cube()`): 36 vértices (6 caras × 2 triángulos × 3 vértices), posiciones ±0.5, UVs por cara.
- [ ] Opcional: migrar a EBO (24 vértices + 36 índices) para reducir duplicados. Puede quedar para Hito 5+.
- [ ] Activar `glEnable(GL_DEPTH_TEST)` en `OpenGLRenderer::init()` y agregar `glEnable(GL_DEPTH_TEST)` permanente al framebuffer del viewport.
- [ ] Rotar el modelo en `renderSceneToViewport` con `glm::rotate` sobre `uModel` con `dt` acumulado.

### Bloque 5 — Cámara

- [ ] `src/engine/scene/Camera.{h,cpp}` (o `src/systems/CameraController.*`): clase abstracta con `viewMatrix()` y `projectionMatrix(aspect)`.
- [ ] `EditorCamera`: orbital. Parámetros: yaw, pitch, distancia al target (origen). Mouse right-drag rota; wheel zoom.
- [ ] `FpsCamera`: position + yaw + pitch. WASD translada en el plano XZ, Espacio/Shift para subir/bajar. Mouse look rota.
- [ ] Evaluar input con SDL: `SDL_GetKeyboardState` para teclado; `SDL_GetRelativeMouseState` cuando `SDL_SetRelativeMouseMode(true)` está activo (Play Mode).

### Bloque 6 — Estado de modo Editor/Play

- [ ] Enum `EditorMode { Editor, Play }` en `EditorApplication`.
- [ ] Botón "Play" en la menu bar (o en la status bar a la derecha). Al presionarlo → modo Play, `SDL_SetRelativeMouseMode(true)`.
- [ ] Handler de `SDL_KEYDOWN` con `SDL_SCANCODE_ESCAPE` → si modo Play, vuelve a Editor, `SDL_SetRelativeMouseMode(false)`.
- [ ] Status bar muestra el modo actual dinámicamente.
- [ ] En Editor Mode, input de cámara sólo responde dentro del panel Viewport (verificar `ImGui::IsItemHovered()` o `ImGui::IsWindowHovered()`).

### Bloque 7 — Tests con doctest

- [ ] CPMAddPackage para doctest.
- [ ] `tests/CMakeLists.txt` que agregue un target `mood_tests` enlazado a `doctest` + `glm` + las unidades a testear.
- [ ] Primer test en `tests/test_math.cpp`: verificar identidad de `glm::mat4`, rotaciones, lookAt, perspective. Son tests triviales para confirmar que doctest + GLM funcionan juntos.
- [ ] Enable CTest: `enable_testing()` + `add_test(NAME mood_tests COMMAND mood_tests)`.

### Bloque 8 — Tracy (opcional, sólo los stubs)

- [ ] Decidir si entra en Hito 3 o se deja para Hito 5-6. Si entra:
  - CPMAddPackage para Tracy.
  - Agregar macros `MOOD_PROFILE_ZONE` / `MOOD_PROFILE_FRAME` en un header nuevo `src/core/Profile.h`.
  - Cuando la feature flag `MOOD_PROFILING` está off (default en release), expandir a nada.
- [ ] Si no entra: anotar en "Pendientes para Hito 4+".

### Bloque 9 — Cierre

- [ ] Recompilar desde cero y verificar todos los criterios.
- [ ] Actualizar `docs/HITOS.md` marcando Hito 3.
- [ ] Actualizar `docs/DECISIONS.md`.
- [ ] Actualizar `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Merge a main, tag `v0.3.0-hito3`, push.
- [ ] Crear `docs/PLAN_HITO4.md`.

---

## Qué NO hacer en el Hito 3

- **No** cargar modelos 3D de archivos externos (`.obj`, `.gltf`). Eso es Hito 10.
- **No** implementar mapas de mundo con tiles/grid. Eso es Hito 4.
- **No** agregar AssetManager todavía; la textura se carga directo desde path en el constructor por ahora. AssetManager entra en Hito 5.
- **No** implementar iluminación ni sombras. Hitos 11+.
- **No** implementar scripting, audio, EnTT, Jolt, networking.
- **No** meter múltiples luces o materials complejos.

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 4 o posterior

_(llenar al cerrar el hito)_
