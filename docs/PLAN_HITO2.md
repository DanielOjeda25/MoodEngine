# Plan — Hito 2: Primer triángulo con OpenGL

> **Leer primero:** `ESTADO_ACTUAL.md` (estado global), sección 10 y 11 de `MOODENGINE_CONTEXTO_TECNICO.md` (contexto del hito dentro del roadmap y filosofía de desarrollo).
>
> **Formato de este documento:** cada tarea es un checkbox. Al completar una tarea, marcar `[x]` y dejar el documento como registro histórico. Si aparece una decisión técnica no prevista, anotarla en la sección "Decisiones durante implementación" al final, y también en `docs/DECISIONS.md`.

---

## Objetivo

Dibujar un triángulo con colores por vértice dentro del panel Viewport del editor, usando OpenGL 4.5 Core cargado via GLAD, a través de una interfaz RHI abstracta. El triángulo debe renderizarse a un framebuffer offscreen cuya textura se muestra en el Viewport vía `ImGui::Image`.

Este es el hito donde el motor deja de ser "shell vacío" y empieza a dibujar geometría real con un pipeline de renderizado propio.

---

## Criterios de aceptación

### Automáticos (verificables por logs / build)

- [ ] `cmake --preset windows-msvc-debug` sigue configurando sin errores desde un repo limpio.
- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Al arrancar `MoodEditor.exe`, el log `logs/engine.log` incluye líneas nuevas del canal `render`:
  - `OpenGL iniciado: <versión>`
  - `GPU: <vendor> <renderer>`
- [ ] El proceso cierra con exit code 0 sin leaks reportados por el validator de OpenGL (si se habilita debug context).

### Visuales (verificables por el dev)

- [ ] Dentro del panel Viewport se ve un triángulo con tres colores interpolados (rojo, verde, azul clásicos).
- [ ] El fondo del Viewport (detrás del triángulo) es un color sólido distinto al gris del editor (ej. azul oscuro o negro).
- [ ] Al redimensionar el panel Viewport (arrastrando bordes o acoplando), el framebuffer se redimensiona y el triángulo sigue visible sin aspect ratio roto.
- [ ] El resto del editor (menús, Inspector, Asset Browser, status bar) sigue funcionando igual que en Hito 1.
- [ ] Cerrar con X o Archivo > Salir sigue siendo limpio.

---

## Tareas

### Bloque 1 — Integrar GLAD

- [ ] Generar GLAD en https://glad.dav1d.de/ con:
  - Language: C/C++
  - Specification: OpenGL
  - API gl: Version 4.5
  - Profile: Core
  - Extensions: (ninguna al inicio; se agregan cuando aparezca la necesidad)
  - Options: "Generate a loader" activado
- [ ] Colocar los archivos descargados en:
  - `external/glad/include/glad/glad.h`
  - `external/glad/include/KHR/khrplatform.h`
  - `external/glad/src/glad.c`
- [ ] Eliminar el `external/glad/README.md` placeholder o reemplazarlo con una nota breve de cómo regenerar si hay que cambiar la versión.
- [ ] Agregar target estático `glad` al `CMakeLists.txt` raíz con `add_library(glad STATIC external/glad/src/glad.c)` y `target_include_directories(glad PUBLIC external/glad/include)`.
- [ ] Enlazar `glad` a `MoodEditor` con `target_link_libraries(MoodEditor PRIVATE glad)`.
- [ ] En `src/editor/EditorApplication.cpp`:
  - Reemplazar `#include <SDL_opengl.h>` por `#include <glad/glad.h>`.
  - Después de `SDL_GL_CreateContext`, llamar `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)` y abortar con log de error si devuelve 0.
  - Loguear `glGetString(GL_VERSION)`, `GL_VENDOR`, `GL_RENDERER` en el canal `render`.
- [ ] Recompilar y verificar que el editor sigue abriendo igual que en Hito 1 y que aparecen las nuevas líneas de log.

### Bloque 2 — RHI abstracto (capa 4 del motor)

Definir las interfaces en `src/engine/render/` sin implementación concreta todavía. La idea es que el código del editor hable contra estas interfaces, no contra OpenGL directo.

- [ ] `src/engine/render/IRenderer.h` — interfaz con métodos:
  - `beginFrame(clearColor)`
  - `endFrame()`
  - `setViewport(x, y, width, height)`
  - `drawMesh(const IMesh&, const IShader&)`
- [ ] `src/engine/render/IShader.h` — interfaz con:
  - `bind()`, `unbind()`
  - `setUniform<T>(name, value)` (al menos para `mat4`, `vec3`, `float`, `int`)
- [ ] `src/engine/render/IMesh.h` — interfaz con:
  - `bind()`, `unbind()`, `indexCount()` o `vertexCount()`
- [ ] `src/engine/render/IFramebuffer.h` — interfaz con:
  - `bind()`, `unbind()`, `resize(w, h)`, `colorAttachmentHandle()` (para pasar a ImGui)

### Bloque 3 — Backend OpenGL (capa 3)

Implementaciones concretas bajo `src/engine/render/opengl/`.

- [ ] `OpenGLRenderer.{h,cpp}` — implementa `IRenderer`, usa `glClear`, `glViewport`, `glDrawElements` / `glDrawArrays`.
- [ ] `OpenGLShader.{h,cpp}` — compila vertex + fragment, linkea, guarda el `GLuint program`. Carga desde archivo de texto (no hot-reload aún).
- [ ] `OpenGLMesh.{h,cpp}` — encapsula VAO + VBO (+ EBO si hace falta). Constructor recibe arrays de vértices/índices.
- [ ] `OpenGLFramebuffer.{h,cpp}` — FBO con textura de color + renderbuffer de depth. Soporte para `resize()`.
- [ ] RAII estricto en todos: el destructor llama `glDelete*` correspondiente.

### Bloque 4 — Shaders built-in

- [ ] `shaders/default.vert` — recibe `aPosition` (vec3) y `aColor` (vec3), pasa color al fragment por `varying`/`out`.
- [ ] `shaders/default.frag` — recibe el color interpolado y lo emite a `FragColor`.
- [ ] Ambos targeted a GLSL 450 core.
- [ ] `CMakeLists.txt`: copiar `shaders/` al directorio del ejecutable post-build (junto a los DLLs) para que el exe los encuentre en runtime.

### Bloque 5 — Integración con el Viewport

- [ ] En `EditorApplication` u `EditorUI`, crear un `OpenGLFramebuffer` de tamaño inicial (ej. 1280x720) al arrancar.
- [ ] Crear la malla del triángulo: 3 vértices con posición + color en NDC (`-0.5,-0.5,0`, `0.5,-0.5,0`, `0,0.5,0`), colores rojo/verde/azul.
- [ ] Antes de que ImGui empiece su frame, bindear el framebuffer, renderizar el triángulo, desbindear.
- [ ] En `ViewportPanel::onImGuiRender`:
  - Reemplazar el texto placeholder por `ImGui::Image((ImTextureID)(intptr_t)framebuffer.colorAttachmentHandle(), panelSize)`.
  - Detectar si el tamaño del panel cambió y llamar `framebuffer.resize(newSize)`.
  - Cuidado con la inversión vertical: OpenGL tiene el origen abajo-izquierda, ImGui arriba-izquierda → pasar `uv0 = (0,1)`, `uv1 = (1,0)` a `ImGui::Image`.

### Bloque 6 — Ajustar solapamiento del dockspace (pendiente de Hito 1)

- [ ] Usar `ImGui::DockBuilderDockWindow` para fijar layout inicial: Viewport al centro, Inspector a la derecha, Asset Browser abajo, sin solapamientos.
- [ ] Aplicar solo la primera vez que se ejecuta el editor o cuando `imgui.ini` no existe.

### Bloque 7 — Cierre

- [ ] Recompilar desde cero y verificar todos los criterios de aceptación (automáticos + visuales).
- [ ] Actualizar `docs/HITOS.md` marcando Hito 2 como completado.
- [ ] Actualizar `docs/DECISIONS.md` con cualquier decisión técnica de esta iteración.
- [ ] Actualizar `docs/ESTADO_ACTUAL.md` con el nuevo estado (Hito 2 cerrado, Hito 3 como próximo).
- [ ] Commits atómicos en español siguiendo la convención del proyecto.
- [ ] Merge a `main`, tag `v0.2.0-hito2`, push a origin.
- [ ] Crear `docs/PLAN_HITO3.md` con el desglose del próximo hito.

---

## Qué NO hacer en el Hito 2

Para mantenerse fiel al principio de incrementalismo (sección 9 del doc técnico):

- **No** cargar modelos de archivos (`.obj`, `.gltf`). Eso es Hito 10.
- **No** implementar cámara 3D navegable. El triángulo se dibuja en NDC fijo. Cámara llega en Hito 3.
- **No** agregar texturas al triángulo. stb_image entra en Hito 5.
- **No** implementar iluminación. Eso es Hito 11.
- **No** generalizar prematuramente la RHI a soportar Vulkan o DirectX. Sólo el contrato abstracto; la implementación es OpenGL.
- **No** armar un sistema de hot-reload de shaders. Llega después.
- **No** tocar EnTT, Lua, Jolt. Cada uno tiene su hito.

---

## Decisiones durante implementación

> Llenar esta sección a medida que aparezcan. Cada entrada también va a `docs/DECISIONS.md`.

_(vacío al inicio del hito)_

---

## Pendientes que quedan para Hito 3 o posterior

> Llenar al cerrar el hito con cosas que surgieron pero no correspondía resolver acá.

_(vacío al inicio del hito)_
