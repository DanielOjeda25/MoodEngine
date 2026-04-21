# Log de decisiones técnicas

Registro cronológico de decisiones arquitectónicas no triviales. Formato por entrada: contexto, decisión, razones, alternativas descartadas, condiciones de revisión.

---

## 2026-04-21: OpenGL 4.5 Core en lugar de Vulkan

**Contexto:** elección de API gráfica para el motor.
**Decisión:** OpenGL 4.5 Core Profile, loader GLAD.
**Razones:** menor curva de aprendizaje, suficiente para calidad AAA 2010-2015, multiplataforma (Windows + Linux), la GTX 1660 del desarrollador la soporta perfectamente.
**Alternativas consideradas:** Vulkan (descartado por complejidad inicial), DirectX 12 (solo Windows, objetivo es multiplataforma).
**Revisar si:** el rendimiento pincha en escenas grandes, o se necesitan features modernas (mesh shaders, ray tracing hardware).

## 2026-04-21: CPM.cmake para gestionar dependencias

**Contexto:** elección de gestor de dependencias C++.
**Decisión:** CPM.cmake, capa fina sobre FetchContent. Bootstrap en `cmake/CPM.cmake` descarga la versión pinneada en la primera configuración.
**Razones:** cero instalación manual, todo versionado en el repo, funciona en Windows sin ecosistema adicional.
**Alternativas consideradas:** vcpkg (añade dependencia al sistema), Conan (configuración más pesada), submódulos git (más fricción al actualizar).
**Revisar si:** los tiempos de configuración inicial se vuelven insoportables o una dependencia no publica tags usables.

## 2026-04-21: ImGui rama `docking`, no `master`

**Contexto:** elección de rama de Dear ImGui.
**Decisión:** rama `docking`.
**Razones:** paneles acoplables y ventanas multi-viewport son esenciales para un editor de motor tipo Unity/Unreal.
**Alternativas consideradas:** `master` (no tiene docking). Los inconvenientes de `docking` (API levemente experimental) son aceptables.
**Revisar si:** `docking` se fusionase en `master` oficialmente.

## 2026-04-21: `/WX` desactivado al inicio

**Contexto:** política de warnings en MSVC.
**Decisión:** `/W4 /permissive-` pero sin `/WX` (warnings no son errores).
**Razones:** el proyecto está en fase temprana y queremos iterar sin bloquear cada build por un warning menor de una dependencia.
**Alternativas consideradas:** activar `/WX` desde el Hito 1 — descartado por fricción innecesaria.
**Revisar si:** el código llega a Hito 5 estable; entonces activar `/WX` para nuestro target (no para dependencias de terceros).

## 2026-04-21: Contexto OpenGL creado desde Hito 1

**Contexto:** cuándo crear el contexto OpenGL.
**Decisión:** crear contexto OpenGL 4.5 Core ya en Hito 1 aunque no se dibuje nada con él; ImGui OpenGL3 backend lo usa para renderizar su UI.
**Razones:** evita reconfigurar en Hito 2, ImGui necesita un contexto GL para funcionar con `imgui_impl_opengl3`.
**Alternativas consideradas:** usar `imgui_impl_sdlrenderer2` en Hito 1 y migrar en Hito 2 — descartado por ser trabajo tirado.

## 2026-04-21: GLAD v2 generado con `glad2` en Python, archivos commiteados

**Contexto:** cómo obtener el loader de OpenGL.
**Decisión:** generar GLAD v2 con `pip install glad2` + `python -m glad --api gl:core=4.5 --out-path external/glad c --loader` y commitear los archivos generados (`external/glad/include/glad/gl.h`, `include/KHR/khrplatform.h`, `src/gl.c`). Se compila como target estático `glad` en el CMakeLists raíz.
**Razones:** regeneración reproducible (cualquiera con Python puede repetirla), los archivos quedan versionados alineado con la sección 4.5 del doc técnico, el build NO requiere Python.
**Alternativas consideradas:** web generator en glad.dav1d.de (más manual, sin trazabilidad de versión); subproject de glad2 via CPM (obliga a tener Python en cada build); glad v1 (en mantenimiento pasivo).
**Revisar si:** glad2 rompe compatibilidad binaria o se necesita otra versión de GL.

## 2026-04-21: GLAD y el loader interno de ImGui pueden coexistir sin conflictos

**Contexto:** ImGui trae su propio loader (`imgui_impl_opengl3_loader.h`, fork de GL3W); GLAD también define punteros de función GL.
**Decisión:** dejar ambos en sus TUs separadas. GLAD se linkea a `MoodEditor` y se incluye con `#include <glad/gl.h>` en `EditorApplication.cpp`. El backend de ImGui usa su loader interno. Al compilar no hay símbolos duplicados porque glad2 prefija con `glad_` y las macros `#define glClear glad_glClear` sólo afectan la TU que incluye `glad/gl.h`.
**Razones:** funciona con la configuración más simple posible; evita parchar imgui_impl_opengl3.cpp con force-include.
**Alternativas consideradas:** definir `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` y force-include de GLAD en el .cpp de ImGui — más configuración CMake sin beneficio real hoy.
**Revisar si:** al actualizar ImGui aparecen conflictos de linker o macros inesperadas.

## 2026-04-21: RHI minimalista como interfaces abstractas

**Contexto:** diseño de la capa de render.
**Decisión:** interfaces puras en `src/engine/render/` (`IRenderer`, `IShader`, `IMesh`, `IFramebuffer` + `RendererTypes.h` con `ClearValues`, `VertexAttribute`, `TextureHandle`). Implementación concreta OpenGL en `src/engine/render/opengl/`. El resto del motor y del editor sólo incluyen las interfaces.
**Razones:** cumple la "capa 4 abstracta" de la arquitectura (sección 5 del doc técnico); permite migrar a Vulkan/DirectX sin tocar el motor; facilita tests unitarios con backends mock más adelante. El diseño es deliberadamente mínimo: sólo lo que Hito 2 necesita; se extiende hito por hito.
**Alternativas consideradas:** exponer OpenGL directo y abstraer sólo cuando llegue otro backend — viola la separación de capas y genera deuda.
**Revisar si:** la API se vuelve pesada de mantener al incorporar texturas (Hito 5) o iluminación (Hito 11).

## 2026-04-21: `TextureHandle` como `void*` en lugar de tipo concreto

**Contexto:** cómo pasar la textura del color attachment de un framebuffer a `ImGui::Image` desde código agnóstico de backend.
**Decisión:** `using TextureHandle = void*;` en `RendererTypes.h`. OpenGL la materializa con `reinterpret_cast<void*>(static_cast<uintptr_t>(texId))`, alineado con `ImTextureID` de ImGui.
**Razones:** no filtra `GLuint` en la interfaz pública; Vulkan podrá devolver un `VkDescriptorSet` por el mismo canal.
**Alternativas consideradas:** `uint64_t` — funcionaría igual pero requiere cast en el callsite.

## 2026-04-21: Render offscreen a framebuffer, mostrar vía `ImGui::Image`

**Contexto:** dónde dibuja la escena 3D.
**Decisión:** un `OpenGLFramebuffer` con color attachment (textura RGBA8) + depth renderbuffer. La escena se dibuja ahí en cada frame, antes de la UI. `ViewportPanel` muestra la textura con `ImGui::Image` invirtiendo V (OpenGL origen abajo-izquierda vs. ImGui arriba-izquierda).
**Razones:** permite paneles de editor con render independiente (viewport principal + previews futuras); no mezcla UI y escena en el mismo framebuffer.
**Alternativas consideradas:** renderizar directo al default framebuffer y superponer ImGui — simple pero ata el viewport al tamaño de la ventana y rompe paneles flotantes.
**Revisar si:** el formato RGBA8 no alcanza (Hito 15: HDR → cambiar a RGBA16F).

## 2026-04-21: Resize del framebuffer con lag de un frame

**Contexto:** `ViewportPanel::onImGuiRender` descubre el tamaño del panel DURANTE la renderización de la UI, pero el render offscreen ocurre ANTES de la UI.
**Decisión:** el panel guarda `desiredWidth/Height` cada frame; el loop lee esos valores en el frame siguiente y llama `resize()` antes del render offscreen.
**Razones:** una estructura simple, sin invertir el orden de render + UI, sin doble pasada. Lag de un frame es imperceptible a 60+ FPS.
**Alternativas consideradas:** renderizar la escena dentro de `ViewportPanel::onImGuiRender` — acopla el panel al renderer y obliga a pasarle renderer/mesh/shader.

## 2026-04-21: DockBuilder para layout por defecto, respetando `imgui.ini`

**Contexto:** al primer arranque (o si no hay `imgui.ini`), los paneles aparecían superpuestos en el dockspace.
**Decisión:** `Dockspace::begin` invoca `buildDefaultLayoutIfEmpty` que usa `ImGui::DockBuilder*` para fijar Viewport (centro), Inspector (derecha 22%) y Asset Browser (abajo 28%). Sólo se aplica si el dockspace no tiene nodos hijos (`node->IsSplitNode() == false`); si el usuario ya acomodó paneles, se respeta el layout persistido.
**Razones:** primera experiencia profesional sin manipular la UI; preserva cambios del usuario.
**Alternativas consideradas:** forzar el layout siempre — pisa los ajustes del dev.
