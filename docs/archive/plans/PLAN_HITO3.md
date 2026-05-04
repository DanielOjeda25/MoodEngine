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

- [x] `cmake --preset windows-msvc-debug` sigue configurando sin errores.
- [x] `cmake --build build/debug --config Debug` compila sin warnings nuevos.
- [x] El log incluye nueva línea del canal `render`: `Textura cargada: <ruta> (WxH, N canales)`.
- [x] Se ejecutan los tests de `tests/test_math.cpp` con doctest y pasan todos.
- [x] Cierre limpio sin leaks de GL.

### Visuales

- [x] Dentro del panel Viewport se ve un cubo 3D con una textura aplicada (ej. grid o crate), rotando lentamente alrededor del eje Y.
- [x] En Editor Mode (arranque por defecto): click-drag con botón derecho del ratón orbita la cámara alrededor del cubo; rueda del ratón hace zoom in/out.
- [x] Status bar muestra `Editor Mode` o `Play Mode` según el estado actual.
- [x] Apretar el botón "Play" (nuevo, en la menu bar o status bar) cambia a Play Mode: ratón capturado, WASD mueve la cámara, mouse look rota la vista en primera persona.
- [x] Apretar `Esc` en Play Mode vuelve a Editor Mode y libera el ratón.
- [x] El cubo NO se estira al redimensionar el Viewport: el aspect ratio se recalcula.
- [x] El resto del editor (menús, paneles, layout) sigue funcionando.

---

## Tareas

### Bloque 1 — stb_image

- [x] Bajar `stb_image.h` y `stb_image_write.h` desde https://github.com/nothings/stb/tree/master/deprecated — usar los headers single-file (típicamente `stb_image.h` y `stb_image_write.h` en la raíz del repo stb).
- [x] Colocarlos en `external/stb/` (reemplazando el placeholder README por uno real con link y versión).
- [x] Agregar target interface `stb` al `CMakeLists.txt`: `add_library(stb INTERFACE)` + `target_include_directories(stb INTERFACE external/stb)`.
- [x] Linkear `stb` a `MoodEditor`.
- [x] Crear un archivo `.cpp` pequeño (p.ej. `src/engine/assets/stb_impl.cpp`) que haga `#define STB_IMAGE_IMPLEMENTATION` + `#include <stb_image.h>` (y equivalente para `stb_image_write` si se usa en el hito).

### Bloque 2 — ITexture y OpenGLTexture

- [x] `src/engine/render/ITexture.h`: interfaz con `bind(slot=0)`, `unbind()`, `width()`, `height()`, `handle()` (devuelve `TextureHandle`).
- [x] `src/engine/render/opengl/OpenGLTexture.{h,cpp}`:
  - Constructor desde path: usa `stbi_load`, crea `GL_TEXTURE_2D`, setea filtros (linear por defecto, mipmap cuando llegue), wrapping `GL_REPEAT`.
  - Detecta formato según canales (RGB, RGBA, grayscale).
  - Libera con `glDeleteTextures` en destructor.
- [x] Agregar un asset de ejemplo: `assets/textures/grid.png` o `crate.png` (de dominio público, NO descargar de sitios con licencia restrictiva; alternativa: generar un grid procedural con `stb_image_write` al arrancar la primera vez si no existe).

### Bloque 3 — Matrices MVP

- [x] Extender `IShader` con `setMat4` ya existente (OK).
- [x] Extender `shaders/default.vert` para recibir `uniform mat4 uModel`, `uniform mat4 uView`, `uniform mat4 uProjection` y aplicar `gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0)`.
- [x] Extender `shaders/default.frag` para opcionalmente samplear una textura (uniform `sampler2D uTexture`). Si se quiere mantener el color por vértice también, combinar: `FragColor = texture(uTexture, vUv) * vec4(vColor, 1.0)`.
- [x] Actualizar `shaders/default.vert` para recibir `aUv` (vec2) y pasarlo a `vUv`.

### Bloque 4 — Cubo con UVs

- [x] Actualizar `OpenGLMesh` para aceptar un layout con `aPos (vec3) + aColor (vec3) + aUv (vec2)` (stride = 8 floats).
- [x] Crear el cubo en `EditorApplication` (o mejor en un helper `MeshBuilder::cube()`): 36 vértices (6 caras × 2 triángulos × 3 vértices), posiciones ±0.5, UVs por cara.
- [x] Opcional: migrar a EBO (24 vértices + 36 índices) para reducir duplicados. Puede quedar para Hito 5+.
- [x] Activar `glEnable(GL_DEPTH_TEST)` en `OpenGLRenderer::init()` y agregar `glEnable(GL_DEPTH_TEST)` permanente al framebuffer del viewport.
- [x] Rotar el modelo en `renderSceneToViewport` con `glm::rotate` sobre `uModel` con `dt` acumulado.

### Bloque 5 — Cámara

- [x] `src/engine/scene/Camera.{h,cpp}` (o `src/systems/CameraController.*`): clase abstracta con `viewMatrix()` y `projectionMatrix(aspect)`.
- [x] `EditorCamera`: orbital. Parámetros: yaw, pitch, distancia al target (origen). Mouse right-drag rota; wheel zoom.
- [x] `FpsCamera`: position + yaw + pitch. WASD translada en el plano XZ, Espacio/Shift para subir/bajar. Mouse look rota.
- [x] Evaluar input con SDL: `SDL_GetKeyboardState` para teclado; `SDL_GetRelativeMouseState` cuando `SDL_SetRelativeMouseMode(true)` está activo (Play Mode).

### Bloque 6 — Estado de modo Editor/Play

- [x] Enum `EditorMode { Editor, Play }` en `EditorApplication`.
- [x] Botón "Play" en la menu bar (o en la status bar a la derecha). Al presionarlo → modo Play, `SDL_SetRelativeMouseMode(true)`.
- [x] Handler de `SDL_KEYDOWN` con `SDL_SCANCODE_ESCAPE` → si modo Play, vuelve a Editor, `SDL_SetRelativeMouseMode(false)`.
- [x] Status bar muestra el modo actual dinámicamente.
- [x] En Editor Mode, input de cámara sólo responde dentro del panel Viewport (verificar `ImGui::IsItemHovered()` o `ImGui::IsWindowHovered()`).

### Bloque 7 — Tests con doctest

- [x] CPMAddPackage para doctest.
- [x] `tests/CMakeLists.txt` que agregue un target `mood_tests` enlazado a `doctest` + `glm` + las unidades a testear.
- [x] Primer test en `tests/test_math.cpp`: verificar identidad de `glm::mat4`, rotaciones, lookAt, perspective. Son tests triviales para confirmar que doctest + GLM funcionan juntos.
- [x] Enable CTest: `enable_testing()` + `add_test(NAME mood_tests COMMAND mood_tests)`.

### Bloque 8 — Tracy (opcional, sólo los stubs)

- [x] Decidir si entra en Hito 3 o se deja para Hito 5-6. Si entra:
  - CPMAddPackage para Tracy.
  - Agregar macros `MOOD_PROFILE_ZONE` / `MOOD_PROFILE_FRAME` en un header nuevo `src/core/Profile.h`.
  - Cuando la feature flag `MOOD_PROFILING` está off (default en release), expandir a nada.
- [x] Si no entra: anotar en "Pendientes para Hito 4+".

### Bloque 9 — Cierre

- [x] Recompilar desde cero y verificar todos los criterios.
- [x] Actualizar `docs/HITOS.md` marcando Hito 3.
- [x] Actualizar `docs/DECISIONS.md`.
- [x] Actualizar `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Merge a main, tag `v0.3.0-hito3`, push.
- [x] Crear `docs/PLAN_HITO4.md`.

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

> Entradas detalladas en `docs/DECISIONS.md` (fecha 2026-04-22 bajo Hito 3).

- stb_image bajado con `curl` y commiteado; target `stb` INTERFACE, símbolos en `src/engine/assets/stb_impl.cpp` único.
- Textura de prueba generada con `tools/gen_grid_texture.py` (Python + Pillow); el PNG + el script se commitean.
- `ITexture` extiende el RHI con el mismo patrón que el resto; backend OpenGL con `stbi_set_flip_vertically_on_load(true)` + mipmaps.
- Cubo hardcoded en `PrimitiveMeshes::createCubeMesh`, 36 vertices interleaved, sin EBO por ahora.
- Orden del loop reordenado: offscreen render va DESPUÉS de `ui.draw()` para que el panel pueda capturar input y la cámara responda en el mismo frame.
- `EditorMode { Editor, Play }` en header propio; `EditorUI` mantiene mirror + flag `togglePlayRequested`; botón Play/Stop en la menu bar con colores.
- `EditorCamera` y `FpsCamera` como clases concretas sin jerarquía virtual; el callsite elige según modo.
- Tests con doctest para unidades puras; el target `mood_tests` compila las fuentes de las cámaras directamente (no hay lib compartida aún).
- Tracy diferido a Hito 5-6 (no entra en Hito 3, no apareció necesidad real todavía).

---

## Pendientes que quedan para Hito 4 o posterior

- ~~**FPS counter en la status bar no siempre visible.**~~ Resuelto 2026-04-23: `StatusBar::draw` migrado a `ImGui::BeginViewportSideBar(ImGuiDir_Down, ...)` (requiere `<imgui_internal.h>`) y reordenado antes de `Dockspace::begin` en `EditorUI::draw`. ImGui reserva la franja inferior del viewport; el dockspace recibe un `WorkSize` ya recortado y el Asset Browser deja de solaparse.
- **Face culling (`glEnable(GL_CULL_FACE)`):** con el cubo actual y su ordenamiento de triángulos, se podría habilitar back-face culling para ahorrar fragmentos. Evaluar en Hito 4 cuando aparezcan más mallas opacas.
- **Estructura compartida de código testeable.** Por ahora `tests/CMakeLists.txt` compila directamente las fuentes de las cámaras. Si crece la superficie testeada, extraer `mood_core` como static lib compartida entre `MoodEditor` y `mood_tests`.
- **Hot-reload de shaders.** Útil cuando los shaders crezcan (Hito 11 iluminación, Hito 17 PBR).
- **Debug context de OpenGL (`GL_ARB_debug_output`).** Captura warnings/errores directo a spdlog; activar con flag de build cuando haya un bug difícil.
- **Tracy profiler.** Postergado a Hito 5-6 (originalmente planeado ahí); anotado.
