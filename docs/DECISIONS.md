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

## 2026-04-22: stb_image commiteado, target `stb` INTERFACE + `stb_impl.cpp`

**Contexto:** carga de texturas para el Hito 3 (adelantada respecto del roadmap que la planeaba en Hito 5).
**Decisión:** bajar `stb_image.h` (2.30) y `stb_image_write.h` (1.16) del repo `nothings/stb` y commitearlos en `external/stb/`. Target CMake `stb` es INTERFACE (sólo expone include path). Los símbolos se generan en una única TU `src/engine/assets/stb_impl.cpp` con `STB_IMAGE_IMPLEMENTATION` + `STB_IMAGE_WRITE_IMPLEMENTATION`. `README.md` documenta cómo actualizar.
**Razones:** reproducible sin dependencias externas, single-header aprovechado como viene del autor.
**Alternativas consideradas:** CPM con repo de stb — funciona pero es innecesario para dos headers.
**Revisar si:** algún header de stb tiene API-breaking update que queramos pinnear a un commit.

## 2026-04-22: Textura de prueba generada con Python/Pillow, PNG commiteado

**Contexto:** hace falta una textura real para sampler2D; descargar de internet abre tema de licencias.
**Decisión:** script `tools/gen_grid_texture.py` (Python + Pillow) genera `assets/textures/grid.png` (256x256 RGBA). Ambos se commitean.
**Razones:** reproducible, sin licencias externas, con orientación visual (acento naranja cada 4 celdas). El script sirve como documentación viva del asset.
**Alternativas consideradas:** generar la textura en runtime con `stb_image_write` en el primer arranque — desplaza trabajo a código de producto; innecesario.
**Revisar si:** el asset crece (p.ej. sprite sheets, atlas) y la generación por script se vuelve ruidosa.

## 2026-04-22: RHI amplia con `ITexture` + backend `OpenGLTexture`

**Contexto:** bloque complementario al renderer del Hito 2: subir texturas a GPU y bindearlas por unit.
**Decisión:** interfaz `ITexture` con `bind(slot)`, `handle()` (TextureHandle opaco). Implementación OpenGL usa stb_image con `stbi_set_flip_vertically_on_load(true)` para que V=0 esté abajo (coincide con OpenGL). Genera mipmaps con `glGenerateMipmap`, filtro LINEAR_MIPMAP_LINEAR / LINEAR, wrap REPEAT.
**Razones:** misma filosofía que el resto del RHI: interfaz mínima en agnóstica, detalles GL en el backend.
**Alternativas consideradas:** incluir una clase Material que agrupe textura + shader — prematuro; entra cuando haya varios materials.

## 2026-04-22: Cubo hardcoded en `PrimitiveMeshes::createCubeMesh`

**Contexto:** necesitamos geometría 3D concreta; assimp está planeado para Hito 10.
**Decisión:** helper `createCubeMesh()` que devuelve `MeshData` con 36 vértices interleaved (pos+color+uv). Sin EBO por ahora; se introduce cuando la duplicación empiece a doler.
**Razones:** mantenerse minimalista; 36 vértices para un cubo son 8*36=288 floats, trivial en RAM.
**Alternativas consideradas:** EBO + 24 vértices — mejor data pero más código para el primer cubo del motor.

## 2026-04-22: Render offscreen ahora tras `ui.draw()`, no antes

**Contexto:** el panel Viewport necesita capturar input del mouse para la cámara orbital. Si el render offscreen se hiciera antes de construir el widget tree de ImGui, el input captado entraría en el frame siguiente.
**Decisión:** nuevo orden del loop — `beginFrame -> ui.draw -> consumeToggle -> updateCameras -> renderSceneToViewport -> endFrame`. El render offscreen queda entre el widget tree y la renderización final de ImGui; la textura del FB se upload antes de que la GPU consuma el draw list que la referencia (`ImGui::Image` sólo guarda el handle).
**Razones:** input de cámara responde en el mismo frame; el lag de un frame del resize del FB sigue siendo aceptable.
**Alternativas consideradas:** capturar input en `processEvents` antes de ImGui — forza replicar la lógica de hover/drag sobre un panel ImGui específico.

## 2026-04-22: Modos Editor/Play con toggle desde menu bar y Esc para salir

**Contexto:** dos cámaras (orbital vs FPS) necesitan estados distintos del editor.
**Decisión:** enum `EditorMode { Editor, Play }` en `src/editor/EditorMode.h`. `EditorApplication` dueño del estado; `EditorUI` tiene un mirror + flag `m_togglePlayRequested`. Botón Play/Stop (verde/rojo) empujado a la derecha de la menu bar. Esc en Play vuelve a Editor. Al entrar Play: `SDL_SetRelativeMouseMode(SDL_TRUE)` + descarta delta residual.
**Razones:** UX reconocible (botón Play prominente tipo Unity), patrón request-response desacopla UI de estado.
**Alternativas consideradas:** manejo directo del estado en la UI — acopla MenuBar al mouse capture de SDL.

## 2026-04-22: `EditorCamera` orbital + `FpsCamera` libres, sin clase base

**Contexto:** dos cámaras conceptualmente distintas.
**Decisión:** clases concretas separadas (`src/engine/scene/EditorCamera.{h,cpp}` y `FpsCamera.{h,cpp}`), sin jerarquía virtual. Ambas exponen `viewMatrix()` + `projectionMatrix(aspect)`; el callsite elige cuál usar según modo.
**Razones:** duck typing sin vtable ni allocación dinámica; para el tamaño actual, una base virtual es overhead sin beneficio.
**Alternativas consideradas:** `class Camera` base abstracta — útil cuando haya 3+ cámaras o un CameraController polimórfico; hoy no.
**Revisar si:** aparecen cámaras cinematic (spline, target-locked) o un sistema ECS necesita tratarlas uniformemente.

## 2026-04-23: `GridMap` texturas via arrays paralelos en vez de `struct Tile`

**Contexto:** en Hito 5 Bloque 5 hay que agregar textura por tile. Dos diseños: (a) cambiar `std::vector<u8> m_tiles` a `std::vector<Tile>` con `struct Tile { TileType type; TextureAssetId texture; }`, o (b) mantener `m_tiles` intacto y agregar un `std::vector<TextureAssetId> m_tileTextures` paralelo indexado igual.
**Decisión:** (b) arrays paralelos.
**Razones:** diff más chico, tests existentes no se tocan (API `tileAt` sigue devolviendo `TileType`), mantiene compatibilidad con el código que sólo necesita saber si un tile es sólido. La separación es práctica ahora: el `PhysicsSystem` sólo lee `isSolid`, el renderer lee `isSolid + tileTextureAt`.
**Alternativas consideradas:** `struct Tile` — más limpio conceptualmente pero fuerza cambios en todos los callers y tests sin beneficio hoy. Revisar si se agregan 3+ campos por tile (variantes, flags, rotación): ahí sí promover a struct.

## 2026-04-23: `LogRingSink` inherits `base_sink<null_mutex>` + lock propio

**Contexto:** el `ConsolePanel` necesita leer los últimos logs del motor, en vivo, desde un thread (el del editor). spdlog `base_sink<Mutex>` ya lockea `mutex_` en `log()` antes de llamar `sink_it_`, pero ese mutex es `protected` y está pensado para sync interno del sink, no para uso externo.
**Decisión:** heredar de `base_sink<spdlog::details::null_mutex>` (cero sync provisto por spdlog) y usar un `std::mutex` propio que protege tanto `sink_it_` como `snapshot()`.
**Razones:** un único mutex para todos los accesos al ring buffer; evita double-lock si usara `base_sink<std::mutex>` + otro mutex para snapshot. El costo de sync se paga una vez por log entry.
**Alternativas consideradas:** (1) `base_sink<std::mutex>` + `snapshot()` que re-adquiere `mutex_` — funciona pero acopla a un detalle protected de spdlog. (2) Atomic snapshot via CAS — overkill para 512 entradas de log.
**Revisar si:** el log sale de muchos threads distintos y el contention del lock se vuelve visible en el profiler.

## 2026-04-23: AssetManager con `TextureAssetId` (u32) en vez de reusar `TextureHandle`

**Contexto:** el RHI ya define `using TextureHandle = void*` (para pasar a `ImGui::Image`). `AssetManager` también necesita un identificador de textura, pero los requisitos son distintos (numérico, estable entre sesiones, indexable en `std::vector`).
**Decisión:** introducir `using TextureAssetId = u32` como tipo propio del `AssetManager`. 0 se reserva para la textura "missing"; cualquier `getTexture(id)` con id fuera de rango cae al fallback. Los callers pueden convertir entre ambos con `getTexture(id)->handle()` cuando necesitan el `TextureHandle` opaco.
**Razones:** mantiene responsabilidades separadas (identidad del asset vs handle GPU-specific). Permite storage indexable en `std::vector`. No rompe nada del RHI.
**Alternativas consideradas:** usar `void*` como AssetId — permite puntero directo pero pierde la propiedad de "id estable entre hot-reloads".

## 2026-04-23: Convencion de escala del motor - 1 unidad = 1 metro SI

**Contexto:** durante el Hito 4 las dimensiones eran artificiales (`tileSize=1.0`, player `0.4x0.9x0.4`, walls 1m cubicos). Un personaje de 0.9m "cabia" debajo de una pared de 1m, inconsistente con cualquier asset realista. El dev detecto la inconsistencia al cerrar el Hito 4.
**Decision:** fijar **1 unidad = 1 metro SI** como convencion del motor, valida para todo el codigo + assets importados (modelos 3D del Hito 10, fisica de Jolt en Hito 12, etc.). Aplicada en Hito 5 Bloque 0:
- `GridMap::tileSize` default `1.0f` -> `3.0f` (pared 3m cubica).
- `EditorApplication::k_playerHalfExtents` `(0.2, 0.45, 0.2)` -> `(0.3, 0.9, 0.3)` (player 0.6 x 1.8 x 0.6 m).
- `FpsCamera` default position `(0, 1, 3)` -> `(0, 1.6, 0)` (altura de ojos humana), speed `3.0` -> primero `4.0` y tras feedback del dev ajustado a `3.0` m/s (paso humano rapido sostenido).
- `EditorCamera` default `radius` `4.0` -> `30.0` (mapa 8x8=24m en cuadro).
- `EditorApplication` refleja los nuevos defaults: player spawn en tile (2,6) = world `(-4.5, 1.6, 7.5)`, orbit radius 30.
**Razones:** alinea con GLTF/assimp/Jolt que asumen metros, evita escalados de conversion por-asset en Hito 10+, los numeros quedan mas faciles de razonar ("la sala son 24m", "la pared 3m", "el jugador 1.8m").
**Alternativas consideradas:** dejar la escala arbitraria y documentar "1 unidad = X m" por proyecto -> rechazado, es el tipo de decision que ensucia todo si no se fija temprano.
**Revisar si:** aparecen assets con otra convencion (Unreal usa cm por default) - si es necesario, poner un factor de escala en el importer, no en el motor.

## 2026-04-23: `GridMap` sin world origin, offset inyectado por el consumer

**Contexto:** el mapa se renderiza centrado en el origen del mundo (para que las cámaras con target=0 lo vean sin ajustes). `aabbOfTile` tendría que saber del offset para que `PhysicsSystem` compare en las mismas coords que el renderer.
**Decisión:** `GridMap` queda en map-local coords (tile (0,0) en XZ=(0,0)). El offset lo computa `EditorApplication::mapWorldOrigin()` (derivado de width/height/tileSize) y se pasa explícito al renderer y a `moveAndSlide`. Single source of truth en el callsite.
**Razones:** mantiene la data del mapa pura (testeable sin transformaciones), evita agregar estado mutable a `GridMap` por un solo consumidor.
**Alternativas consideradas:** guardar `worldOrigin` dentro de `GridMap` — postergado hasta que aparezca un segundo consumidor con otro transform (ej. minimapa, edición de un subrango).

## 2026-04-23: `OpenGLDebugRenderer` concreto, sin interfaz `IDebugRenderer`

**Contexto:** debug draw de AABBs para validar physics (Hito 4 Bloque 5).
**Decisión:** crear `OpenGLDebugRenderer` como clase concreta en `src/engine/render/opengl/` (no extraer interfaz aún). `drawLine/drawAabb` acumulan vértices `{pos, color}` en un `std::vector` CPU, y `flush(view, projection)` los sube a un VBO dinámico (growth 2×) y los dibuja con `GL_LINES` bajo el shader `shaders/debug_line.{vert,frag}`.
**Razones:** hay un solo backend de render (OpenGL). El plan explícitamente permite "`IDebugRenderer.h` o ampliar `IRenderer`"; la tercera opción (concreto sin abstracción) cumple el criterio de "evitar abstracciones prematuras" de CLAUDE.md. Si Vulkan llega (no previsto), extraer.
**Alternativas consideradas:** `IDebugRenderer.h` + `OpenGLDebugRenderer : public IDebugRenderer` — añade dos archivos por nada hoy.

## 2026-04-23: `FpsCamera` separa `computeMoveDelta` de `translate`

**Contexto:** `PhysicsSystem::moveAndSlide` necesita ver el delta que la cámara quiere aplicar **antes** de aplicarlo, para poder clampearlo contra colisiones.
**Decisión:** agregar `glm::vec3 computeMoveDelta(dir, dt) const` (devuelve el delta sin mutar) y `void translate(glm::vec3 delta)` (aplica el delta ya resuelto). El método `move(dir, dt)` original se mantiene como conveniencia (delega en ambos).
**Razones:** el callsite queda `desired -> moveAndSlide -> actual -> translate`. Nada que depende del viejo `move()` se rompe (tests + código simple siguen funcionando).
**Alternativas consideradas:** que `move()` devuelva el delta aplicado y permita pasar un predicado de colisión — acopla la cámara al sistema de colisiones, mala dirección.

## 2026-04-23: Player AABB 0.4×0.9×0.4 en vez del 0.3×0.9×0.3 del plan

**Contexto:** con `half-extent=0.15` (0.3 ancho) la cámara queda a 0.15 unidades del muro al colisionar. Con el near clipping plane a 0.1 y pitch > 0, el frustum se metía visualmente dentro del muro.
**Decisión:** subir half-extent a 0.2 (0.4 ancho). El margen a la pared queda en 0.2, el doble del near plane.
**Razones:** fix estándar (FPS comerciales hacen lo mismo, "body" más ancho que lo estrictamente necesario). No requiere bajar el near plane (que penaliza precisión del depth).
**Revisar si:** se adopta la convención de escala realista (Hito 5+) — el half-extent pasará a 0.3 (player ~0.6m wide) que ya es mucho mayor al near plane y este problema se disuelve.

## 2026-04-23: `drawMesh` sin `shader.unbind()` al terminar

**Contexto:** al agregar el loop que dibuja 29 cubos del grid (un `setMat4("uModel", ...)` entre draws), todos los cubos aparecían en la posición del primero.
**Decisión:** `OpenGLRenderer::drawMesh` deja el shader y la mesh bindeados al terminar. El que llama decide cuándo cambiar estado.
**Razones:** `glUniform*` actúa sobre el program bound. Con `unbind()` al final de cada draw, el `setMat4` de la próxima iteración se ejecutaba con program 0 bound y fallaba silencioso (GL_INVALID_OPERATION). Es el patrón "bind once, draw many" estándar; unbind explícito es lo atípico.
**Alternativas consideradas:** rebindear el shader dentro del loop antes de cada `setMat4` — oculta el bug, no lo elimina.

## 2026-04-23: Status bar con `ImGui::BeginViewportSideBar` antes del dockspace

**Contexto:** post Hito 3, con el Asset Browser acoplado al 28% inferior del dockspace, la status bar inferior quedaba tapada. El dockspace host ocupa todo `viewport->WorkSize` y los paneles docked se dibujan encima de cualquier ventana que posicionemos manualmente al borde inferior.
**Decisión:** migrar `StatusBar::draw` a `ImGui::BeginViewportSideBar(name, nullptr, ImGuiDir_Down, height, flags)` — requiere `<imgui_internal.h>` en la rama `docking` actual — y llamarla ANTES de `Dockspace::begin` en `EditorUI::draw`. ImGui registra la reserva en `BuildWorkOffsetMax` del viewport; el dockspace recibe un `WorkSize` recortado y no pisa la franja de la status bar.
**Razones:** solución canónica de ImGui para barras laterales/inferiores; un frame de lag en el ajuste pero sin parpadeo visible; evita z-order manual entre ventanas no-docked y dockspace.
**Alternativas consideradas:** `BringWindowToFront` manual o flag `NoBringToFrontOnFocus` en la status bar — funcionan inconsistentemente según el `imgui.ini` persistido.
**Revisar si:** activamos multi-viewport de ImGui o cambiamos el host del dockspace.

## 2026-04-22: Tests con doctest, sólo unidades puras

**Contexto:** inicio de la red de tests. doctest adelantado desde Hito 3-4 (roadmap decía 3-4).
**Decisión:** target `mood_tests` compila las unidades puras (hoy `EditorCamera`, `FpsCamera` + `test_math.cpp`). No se testea render ni UI. `enable_testing()` + `add_subdirectory(tests)` en el CMakeLists raíz. Ejecución con `ctest` o invocando el exe directo.
**Razones:** cubrir la matemática sin montar mocks de GL; sirve como sanity check del build de cámaras.
**Alternativas consideradas:** extraer una static lib `mood_core` compartida con MoodEditor y mood_tests — prematuro con dos archivos; se hace cuando la superficie crezca.


## 2026-04-23: Tile picking via raycast al plano y=0

**Contexto:** drag & drop de texturas sobre el viewport necesita saber qué tile cae bajo el cursor. Hay muros (cubos) que ocluyen el piso, pero queremos que el click funcione incluso sobre tiles vacíos.
**Decisión:** `ViewportPick::pickTile` hace unproject con la inversa de `proj * view` de dos puntos NDC (z=-1 near, z=+1 far), saca el rayo `near → far`, lo intersecta con el plano y=0. Si el hit cae dentro del rectángulo del mapa, devuelve `(tileX, tileY)`. Ignora la geometría de los muros — al clickear sobre la cima de un muro, el rayo pincha el piso del mismo tile.
**Razones:** O(1), sin BVH ni raycast contra geometría, suficiente para editores de grid; permite pintar tiles vacíos y sólidos por igual.
**Alternativas consideradas:** raycast contra cada AABB del mapa — O(W*H) por frame, innecesario para este caso.
**Revisar si:** aparecen niveles no-planos (rampas, puentes) donde "piso" deja de ser una plano único.

## 2026-04-23: TextureFactory inyectable en `AssetManager`

**Contexto:** el Hito 5 dejó pendiente que `AssetManager` no era testeable — instanciaba `OpenGLTexture` directo en `loadTexture`, lo cual requiere contexto GL.
**Decisión:** añadir `using TextureFactory = std::function<std::unique_ptr<ITexture>(const std::string&)>` al constructor. `EditorApplication` pasa una lambda que crea `OpenGLTexture`; los tests pasan una que devuelve un `MockTexture` (no hace I/O, no toca GL).
**Razones:** 7 casos nuevos (+24 asserciones) para caching, fallback, VFS sandbox y rangos inválidos, sin GL. Mantiene la API pública sin cambios para los callsites existentes (una lambda más al construir).
**Alternativas consideradas:** método virtual protegido + subclase de tests — más ceremonia y menos flexible.

## 2026-04-23: Middle-drag como pan estilo Blender en `EditorCamera`

**Contexto:** el dev pidió paneo al faltar la forma de encuadrar el mapa al lado (solo había rotar y zoom).
**Decisión:** `applyPan(dxPixels, dyPixels)` mueve el `m_target` perpendicular al view direction. Sensibilidad `0.0015 * radius` para que se sienta constante al cambiar de zoom. Middle-drag se captura en `ViewportPanel` con el mismo patrón que right-drag. Dirección: mouse a la derecha mueve el target a la izquierda ("agarra" el mundo con el cursor, igual que Blender/Maya/3ds Max).
**Alternativas consideradas:** Shift+right-drag — no es convención estándar en editores 3D.

## 2026-04-23: JSON con `nlohmann/json` 3.11.3 + adapters ADL

**Contexto:** serialización del Hito 6.
**Decisión:** `nlohmann/json` 3.11.3 vía CPM. Adaptadores de tipos del motor en `src/engine/serialization/JsonHelpers.h` (header-only). Estrategia: `adl_serializer<T>` para `glm::vec2/3/4` (compacto como array `[x,y,z]`) y `AABB` (explícito `{min, max}`); `NLOHMANN_JSON_SERIALIZE_ENUM` para `TileType` → strings `"empty"/"solid_wall"` (estables a renumeración del enum). La macro tiene que estar en el mismo namespace que el enum para que ADL la encuentre.
**Razones:** `nlohmann/json` es el default de facto en C++ moderno. Header-only, excelente API. El schema del motor es pequeño y plano; no necesita un framework pesado.
**Alternativas consideradas:** `cereal` (planeado para Hito 10+ según el doc técnico cuando lleguen objetos más complejos), `tinygltf`-style ad-hoc — menos ergonómico.

## 2026-04-23: Versionado de formato por entero monotónico

**Contexto:** `.moodmap` y `.moodproj` son los primeros formatos persistidos del motor.
**Decisión:** cada formato tiene una constante `k_MoodmapFormatVersion = 1` / `k_MoodprojFormatVersion = 1`. Helper `checkFormatVersion(j, supported, fileKind)` en `JsonHelpers.h`: rechaza versiones mayores con `runtime_error`, acepta iguales o menores (los serializers deciden si pueden migrar). Bump cuando cambia la semántica — agregar un campo nuevo opcional con default al leer NO requiere bump.
**Razones:** simple, explícito, permite mensajes de error útiles ("versión 2 no soportada; máxima: 1"). Migraciones pueden sumarse hito por hito sin cambiar la estrategia.
**Alternativas consideradas:** semver — overkill para archivos internos del motor.

## 2026-04-23: Texturas en `.moodmap` se guardan por path lógico, no por id

**Contexto:** los `TextureAssetId` son índices en la tabla del `AssetManager` actual — volátiles entre sesiones o proyectos distintos.
**Decisión:** al serializar un tile, se escribe `"texture": "textures/grid.png"` (path lógico del VFS). Al cargar, se llama `AssetManager::loadTexture(path)` y se usa el id que devuelva. Requirió:
1. `AssetManager::pathOf(id) -> string`: lookup inverso via `std::vector<std::string> m_texturePaths` paralelo a `m_textures`.
2. Cachear `"textures/missing.png"` al id 0 en el constructor, para que round-trip save→load del fallback preserve el id.
**Razones:** formato estable a cambios en el orden de carga de assets; permite compartir proyectos con otro dev que tenga distintas texturas ya cargadas.
**Alternativas consideradas:** UUIDs por textura — requeriría un registro persistente aparte, sobreingeniería para el tamaño actual.

## 2026-04-23: File dialogs nativos con `portable-file-dialogs`

**Contexto:** el menú Archivo necesita abrir diálogos para seleccionar carpeta / `.moodproj`.
**Decisión:** `portable-file-dialogs` 0.1.0 vía CPM (DOWNLOAD_ONLY + target INTERFACE propio; el repo no trae `CMakeLists.txt` utilizable). Header-only, cross-platform, usa APIs nativas del OS (Shell/GTK/Cocoa).
**Razones:** mínimo esfuerzo, look nativo, mejor UX que reimplementar un file browser en ImGui.
**Alternativas consideradas:** `tinyfiledialogs` (más viejo, C puro), browser en ImGui (tiempo).

## 2026-04-23: Request/consume para acciones del menú (ProjectAction)

**Contexto:** el menú Archivo tiene cinco items con lógica no trivial (diálogos, I/O, cambios de estado).
**Decisión:** `EditorUI` expone `requestProjectAction(ProjectAction)` y `consumeProjectAction()`. `MenuBar` sólo emite requests; `EditorApplication` consume después de `ui.draw()` y dispatcha a handlers concretos. Ctrl+S se captura en `processEvents` y emite el mismo request — dispatcher único.
**Razones:** mismo patrón que `togglePlayRequest` del Hito 3 y `dockspace.requestResetToDefault` del Bloque 0. Desacopla UI de side effects; facilita testear la UI sin ejecutar diálogos.
**Alternativas consideradas:** callbacks desde MenuBar — acopla MenuBar a las implementaciones concretas de los handlers.

## 2026-04-23: `Project.root` no se serializa

**Contexto:** el `.moodproj` declara paths a mapas y textures.
**Decisión:** `Project.root` se infiere del `parent_path` del archivo al cargar, NO se escribe en el JSON. Todos los demás paths (maps[], defaultMap) son relativos a root.
**Razones:** el proyecto se puede mover a otra carpeta y seguir funcionando; no hay que reescribir el `.moodproj` al renombrar el folder.
**Alternativas consideradas:** guardar root como path absoluto — rompería al mover la carpeta.

## 2026-04-23: Flow de proyectos estilo Unity/Godot con modal Welcome

**Contexto:** post-cierre de Hito 6, el flow de save/load acumulaba bugs
reactivos porque el editor mezclaba dos modelos: "Blender sin proyecto"
(estado vacio con mapa de prueba) y "Unity con proyecto" (carpeta con
.moodproj + .moodmap). Cada parche para un caso rompia otro.
**Decisión:** adoptar el modelo Unity/Godot/Unreal sin mezcla:
- El editor **siempre tiene un proyecto activo**. No existe el estado
  "sin proyecto y con mapa de prueba editable".
- Si no hay proyecto (primera vez o tras Cerrar), aparece un **modal
  Welcome bloqueante** con [Nuevo] / [Abrir] / [Recientes].
- Al arrancar se intenta auto-abrir el ultimo proyecto (convencion
  Unity). Si falla o no hay recientes, modal Welcome.
- Ctrl+S siempre guarda sobre el proyecto actual. Nuevo/Abrir siempre
  tienen la misma semantica (archivo en disco, crea o abre).
- El mapa de prueba pasa a ser **template** para Nuevo Proyecto,
  no un estado editable del editor.
**Razones:** predictibilidad. Cada accion tiene un unico resultado
posible; desaparecen los patches "si no hay proyecto hago X, si hay
hago Y". El modelo matchea las expectativas del dev (viene de Unity/
Godot) y no requiere reinventar la UX.
**Alternativas consideradas:**
- Modelo Blender (sin concepto de proyecto, solo .moodmap). Mas
  simple pero renuncia a poder empaquetar varios mapas + textures
  bajo un nombre comun, objetivo del roadmap.
- Hybrido VS-Code-like (puede abrir sin workspace). Genera la
  ambiguedad actual.
**Revisar si:** a largo plazo aparece la necesidad de un "scratch
mode" para experimentar sin compromiso (editor juega al papel de
Blender por un rato). Hasta entonces, proyectos everywhere.

## 2026-04-23: ECS con EnTT + fachada Scene/Entity

**Contexto:** Hito 7 introduce el modelo de entidades-componentes-sistemas para
que el editor tenga un modelo de escena sobre el cual Hierarchy/Inspector/
RenderSystem puedan trabajar de forma uniforme.
**Decisión:** `skypjack/entt` 3.13.2 como backend, oculto detrás de una
fachada propia: `Mood::Scene` envuelve `entt::registry` y `Mood::Entity` es
un wrapper liviano (16 bytes: `entt::entity` + `Scene*`). El resto del
motor no incluye `<entt/entt.hpp>` directamente — todo pasa por `Entity.h`
/ `Scene.h`. Los componentes en `Components.h` son POD sin lógica; los
sistemas (hoy solo RenderSystem conceptual) iteran con `Scene::forEach`.
**Razones:** EnTT es performance-first, battle-tested (Dungeons & Dragons
Online, Minecraft Bedrock); la fachada esconde templates complejos,
permite cambiar el backend si aparece otro ECS (Flecs, etc.) sin tocar el
código cliente. Alineado con la sección 4.14 del doc técnico.
**Alternativas consideradas:** exponer `entt::registry` directo (mejor
performance, peor API ergonomics); escribir un ECS propio (reinvento la
rueda); Flecs (menos adoptado en C++).
**Revisar si:** necesitamos queries complejas (compound views con exclusion)
que se vuelvan verbosas de envolver.

## 2026-04-23: Scene derivada de GridMap en Hito 7 (no authoritative todavía)

**Contexto:** en el Hito 7 queríamos introducir entidades pero sin
reescribir el render (GridRenderer sobre el grid) ni el serializador
(.moodmap es un grid).
**Decisión:** `Scene` en Hito 7 es una VISTA derivada del `GridMap`.
`rebuildSceneFromMap` se llama cada vez que el mapa cambia (buildInitial,
openProject, drop, closeProject). El editor ve entidades en el Hierarchy y
puede editarlas en el Inspector, pero las ediciones son EPHEMERAL — no se
persisten al `.moodmap` porque el formato no las soporta todavía. El flip
a Scene-authoritative viene cuando llegue geometría no-grid (Hito 10 con
assimp: meshes importados desde archivo necesitan ser entidades).
**Razones:** entrega valor incremental sin romper lo que funciona. El
Hierarchy/Inspector dan feedback visual útil; cuando Scene tome el mando,
las ediciones pasarán a persistir sin cambios en la UI.
**Alternativas consideradas:** Scene authoritative desde el día 1 —
requiere redefinir `.moodmap` con entidades en vez de tiles; mucho cambio
para cero beneficio inmediato.
**Revisar si:** Hito 10+ (assimp mesh loading) o si el costo del
`rebuildSceneFromMap` se nota (~30 entidades hoy, ~1000+ con mapas más
grandes).

## 2026-04-23: `Scene` se reinicia vía `registry.clear()`, no recreando el unique_ptr

**Contexto:** primera versión de `rebuildSceneFromMap` recreaba el
`std::unique_ptr<Scene>`. Los paneles Hierarchy/Inspector guardan
`Scene*` que se volvía dangling pointer tras cada rebuild, produciendo
crashes al acceder a entidades destruidas.
**Decisión:** rebuild ahora hace `m_scene->registry().clear()` + repoblar.
El `Scene*` de los paneles sigue siendo válido (apunta al mismo objeto).
La selección (también un `Entity` con handle ya inválido) se invalida
explícitamente con `m_ui.setSelectedEntity(Entity{})`.
**Razones:** evita un dangling-pointer bug sutil sin complicar la API.
**Alternativas consideradas:** cambiar la API a `Scene&` por referencia
(aún más seguro pero requiere pasar la scene por todos los constructores).

## 2026-04-24: Lua 5.4 via walterschell/Lua wrapper + sol2 v3.3.0

**Contexto:** Hito 8 integra scripting en Lua. Lua upstream (lua.org) no
trae `CMakeLists.txt` nativo; CPM prefiere repos con CMake.
**Decisión:** `walterschell/Lua` (tag `v5.4.5`) como wrapper CMake sobre
Lua 5.4.5. Expone target `lua_static`. Opciones: `LUA_BUILD_BINARY OFF`,
`LUA_BUILD_COMPILER OFF` (no necesitamos el REPL ni `luac`),
`LUA_ENABLE_TESTING OFF` (si no, el wrapper registra `lua-testsuite` en
CTest que falla por falta de `lua.exe`). El flag se setea como `CACHE BOOL`
antes del `CPMAddPackage` porque los `option(...)` internos ignoran
variables UNINITIALIZED.
sol2 v3.3.0 (header-only) como binding C++17↔Lua; wrapped detrás de
`src/engine/scripting/LuaBindings.{h,cpp}` para no filtrar `sol::*` al
resto del motor.
**Razones:** wrapper estable, Lua 5.4 con stdlib completa, sol2 es el
default de facto para C++17.
**Alternativas consideradas:**
- Bajar las fuentes de Lua commiteadas (como stb): funciona pero
  reinventar el CMake a mano cuando ya hay wrapper es fricción.
- LuaJIT: mucho más rápido pero más restrictivo en plataformas y API; el
  doc técnico pide Lua 5.4 pelado.
- luabridge o kaguya: menos adoptados que sol2 hoy.
**Revisar si:** el wrapper se desactualiza o sol2 v4 aporta cambios
relevantes.

## 2026-04-24: Un `sol::state` por entidad (islas aisladas)

**Contexto:** cada entidad con `ScriptComponent` necesita su propio
contexto Lua: las globals de un script no deberían cruzar a otras
entidades.
**Decisión:** `ScriptSystem` guarda
`std::unordered_map<entt::entity, std::unique_ptr<sol::state>> m_states`.
El `sol::state` NO vive en el `ScriptComponent` porque `sol::state` no es
copiable y hacerlo movible complica el storage de EnTT (componentes se
mueven al compactar). `unique_ptr` asegura que la dirección del state no
cambie al crecer el mapa. `clear()` vacía el mapa cuando el registry se
limpia (llamado desde `rebuildSceneFromMap`).
**Razones:** aislamiento simple; los usertypes registrados apuntan al
`Entity` específico via closures, no se confunden entre scripts.
**Alternativas consideradas:** un `sol::state` compartido con "sandbox
tables" por entidad (`_ENV`). Más eficiente en RAM pero complica la API y
el reset: cerrar una entidad no libera su tabla trivialmente.
**Revisar si:** aparecen cientos de entidades con script y el costo de
un state per-entity (cada `sol::state` arrastra toda la stdlib de Lua
cargada) empieza a pesar en RAM.

## 2026-04-24: API C++→Lua con scope mínimo en Hito 8

**Contexto:** el plan original de Hito 8 Bloque 3 listaba `Input`,
`scene:createEntity`, etc. Durante la implementación el dev pidió
"seguir estricto, sin desviarnos" (ver `feedback_plan_discipline` en
memoria). El demo concreto de Bloque 6 (`rotator.lua`) solo necesita
`self.transform.rotationEuler.y`.
**Decisión:** exponer únicamente lo que `rotator.lua` necesita. Bindings
disponibles desde Lua en `LuaBindings.cpp`:
- `Entity.tag` (lectura), `Entity.transform` (ref al `TransformComponent&`).
- `TransformComponent.position` / `.rotationEuler` / `.scale` (glm::vec3
  con `.x/.y/.z`).
- Tabla global `log` con `info(str)` y `warn(str)` → canal `script`.
- Libs Lua habilitadas: `base`, `math`, `string`. NO `io`/`os`/`package`
  (sandbox razonable; ningún script del motor necesita tocar el FS o
  cargar módulos dinámicamente por ahora).
`Input`, `scene:createEntity`, mutaciones de otros componentes, quedan
diferidos con trigger explícito en `PLAN_HITO8.md`.
**Razones:** evitar "scope creep" exponiendo API que nadie usa. Cada
función expuesta tiene costo: superficie de tests, compatibilidad futura,
cambios de comportamiento si la cambiamos. Prefiero agregar cuando haya
un demo que la pida.
**Alternativas consideradas:** exponer todo de entrada — viola
feedback del dev y el principio de "no diseñar para hipotéticos" de
CLAUDE.md.

## 2026-04-24: Hot-reload de scripts con throttle global + state reuse

**Contexto:** detectar cambios en `.lua` sin stat al FS cada frame.
**Decisión:** `ScriptSystem` acumula `dt` en `m_hotReloadAccumulator`.
Cuando supera `k_hotReloadInterval = 0.5 s`, chequea mtime de cada
`ScriptComponent::path` contra `m_mtimes`. Si difiere, reejecuta
`safe_script_file` sobre el **mismo** `sol::state` (preserva globals, solo
redefine `onUpdate` y compañía). Log: `Recargado 'path/to/script.lua'`.
El "Recargar" del Inspector, en contraste, pone `loaded=false` — eso
fuerza un `sol::state` **nuevo** la próxima iteración (reset duro, útil
cuando el dev cambia el path al script).
**Razones:** diferenciar los dos casos:
- Hot-reload automático: el usuario editó el archivo, quiere ver el
  cambio sin perder el estado en-flight (posición del cubo, etc.).
- "Recargar" manual: reset limpio (descarta globals acumulados).
Throttle global porque el stat al filesystem es más barato por entrada que
por frame con 60+ frames/s, pero sigue siendo innecesario a esa frecuencia.
**Alternativas consideradas:** `std::filesystem` con `inotify`/`ReadDirectoryChangesW` — más eficiente
pero mucho más código plataforma-específico para ahorrar un stat cada
500 ms. Dejar para cuando haya ≥100 scripts.

## 2026-04-24: Pase de render scene-driven (demo fix) separado de GridRenderer

**Contexto:** el `GridRenderer` itera `GridMap` directamente, no el
`Scene`. La entidad "Rotador" spawneada por el menú tenía
`MeshRendererComponent` pero nadie la dibujaba. Es un gap del plan, no un
bug del script: el header de `EditorApplication.h` ya decía "La migracion
a Scene-driven render viene en hitos posteriores cuando haya geometria
no-grid (assimp en Hito 10)".
**Decisión:** agregar un pase inline en `renderSceneToViewport` DESPUÉS
del `GridRenderer::draw`, iterando `Scene::forEach<Transform, MeshRenderer>`
y dibujando cada entidad que NO tenga tag con prefijo `"Tile_"` (las
entidades-tile de `rebuildSceneFromMap` sí lo tienen). El filtro evita
overdraw: las tiles ya las dibujó el GridRenderer.
**Razones:** cambio mínimo para que el Bloque 6 del Hito 8 funcione
visualmente. Es explícitamente transicional — cuando Hito 10 reemplace
el `GridRenderer` por un `RenderSystem` scene-driven, este pase se funde
con él y el filtro por prefijo desaparece.
**Alternativas consideradas:**
- Agregar un `TileBackedComponent{}` marker component para el filtro en
  vez del prefijo del tag: más limpio, pero es nueva API por un caso
  puntual que va a morir en Hito 10. Prefijo de tag reusa data existente.
- Reescribir el render a scene-driven ahora: fuera de scope de Hito 8 y
  explícitamente diferido al 10.
**Revisar si:** aparece otro caso de "entidades con MeshRenderer no-tile"
antes del Hito 10; ahí vale la pena el marker component.

## 2026-04-24: miniaudio v0.11.21 vendored single-header (Hito 9)

**Contexto:** elección de backend de audio para el motor.
**Decisión:** miniaudio v0.11.21 (mackron/miniaudio) vendored como single-header en `external/miniaudio/`. Target CMake INTERFACE; implementación materializada en una única TU `src/engine/audio/miniaudio_impl.cpp` con `MINIAUDIO_IMPLEMENTATION` + `MA_NO_ENCODING` + `MA_NO_GENERATION`. Mismo patrón que stb.
**Razones:** cero instalación/ecosistema (es C, no C++ estándar), soporta WAV/OGG/MP3/FLAC de stock, high-level API `ma_engine` incluye resource manager (cacheo de PCM) y sonidos posicionales 3D, dominio público / MIT-0. La alternativa OpenAL Soft requiere CMake build + config por plataforma. FMOD/Wwise = licencia. SDL_mixer es funcional pero atado a SDL específicamente y menos moderno.
**Alternativas consideradas:** OpenAL Soft (más complejo de integrar, target CMake pesado), SDL_mixer (API menos rica, no positional modern), FMOD (licencia comercial).
**Revisar si:** necesitamos features que miniaudio no expone (reverb fullband, ambisonics, HRTF externo). En ese caso, swap backend; la interfaz `AudioDevice` es agnóstica.

## 2026-04-24: `AudioDevice` null-safe (modo mute al fallar init)

**Contexto:** CI sin audio hardware, máquinas del dev con driver roto, y tests unitarios que corren en máquinas sin sonido.
**Decisión:** si `ma_engine_init(nullptr, &engine)` falla, `AudioDevice` queda en estado inválido. `isValid()` retorna false; `play/stop/setListener/stopAll` son no-ops silenciosos. El resto del motor no se entera.
**Razones:** no queremos que un driver raro rompa el editor completo ni que los tests requieran hardware. Los games con opción `--nosound` implementan lo mismo históricamente.
**Alternativas consideradas:** throw en init y que `EditorApplication` atrape (más ruidoso, mismo resultado). Poll del sistema antes de intentar init (no evita drivers corruptos).
**Revisar si:** aparecen casos donde necesitamos saber si el device falló para mostrar UI ("audio no disponible"). Hoy alcanza con el warn del log.

## 2026-04-24: `AudioClip` solo metadata — PCM en resource manager de miniaudio

**Contexto:** diseño del segundo tipo de asset del motor. Primer instinto: `AudioClip` owner del buffer de PCM decodificado.
**Decisión:** `AudioClip` guarda path + metadata (duración, sr, canales). El PCM lo maneja el resource manager interno de `ma_engine` (via `ma_sound_init_from_file` que cachea). El `AssetManager` solo lleva la lista de clips (identidad + path); `AudioDevice::play` crea `ma_sound` instances fresh cada vez, compartiendo el PCM del cache.
**Razones:** miniaudio ya hace resource management; duplicarlo sería redundante. AudioClip se mantiene liviano y serializable (solo string path). Las reproducciones concurrentes del mismo clip (múltiples enemigos golpeando, ambient loops) salen automáticas.
**Alternativas consideradas:** cargar PCM completo en `AudioClip` con `ma_decoder_decode_buffer` + `ma_audio_buffer`: más control pero duplica trabajo del resource manager. Útil solo si el motor cargara miniaudio solo como decodificador (sin device).
**Revisar si:** necesitamos custom DSP por clip (filters, resampling), o streaming de archivos grandes donde el cache no-streaming mata la RAM.

## 2026-04-24: `AudioAssetId` distinto de `TextureAssetId`

**Contexto:** `AssetManager` ya tiene `TextureAssetId = u32` con `0 = missing`. Al agregar audio es tentador reusar el tipo.
**Decisión:** `AudioAssetId = u32` como alias separado. Namespaces numéricos distintos (texturas y audio crecen independiente), `missingAudioId() == 0` también pero no colisiona con `missingTextureId() == 0` porque son accesores diferentes.
**Razones:** typesafety: un bug que pase un id de textura a `getAudio` es silencioso si son el mismo tipo. Alias separados hacen el error obvio en el code review. Overhead: cero (mismo tamaño de alias).
**Alternativas consideradas:** `AssetId<Texture>` / `AssetId<Audio>` strongly-typed. Mejor teoría, pero cada call-site requiere instanciación explícita y el cast a u32 para logs/serialización se vuelve ritual. El alias separado da el 80% del beneficio con el 20% del costo.
**Revisar si:** aparece un tercer tipo de asset (meshes en Hito 10). En ese caso, `using MeshAssetId = u32` sigue el mismo patrón; si empiezan a mezclarse, considerar el template strongly-typed.

## 2026-04-24: `AudioSourceComponent` no se serializa en `.moodmap`

**Contexto:** el `.moodmap` actual serializa tiles + texturas asignadas, no entidades ECS completas. `ScriptComponent` tampoco se serializa (decisión Hito 8).
**Decisión:** `AudioSourceComponent` tampoco se persiste. Los audio sources se spawnean programáticamente (ej. el demo de `Ayuda > Agregar audio source demo`) o (futuro) desde scripts Lua.
**Razones:** consistente con `ScriptComponent`. El modelo Scene-driven authoritative se discute en Hito 10+ cuando aparezcan meshes de archivo. Serializar un subconjunto de componentes ahora fuerza un diseño a medias.
**Alternativas consideradas:** agregar una sección `[audio_sources]` al `.moodmap` ahora. Genera doble fuente de verdad (`SceneSerializer` persiste tiles + esta nueva sección).
**Revisar si:** cuando entre Scene authoritative en Hito 10+, AudioSourceComponent + ScriptComponent + MeshRendererComponent se persisten juntos en el mismo `[entities]`.

## 2026-04-24: assimp 5.4.3 como static lib, solo importadores OBJ + glTF + FBX

**Contexto:** Hito 10 introduce la importacion de modelos 3D. `assimp`
soporta 40+ formatos por default pero la mayoria no interesan al motor
(BVH animaciones, WAV audio, etc.) y suman ~10 MB al binario.
**Decision:** `CPMAddPackage(assimp v5.4.3)` con `BUILD_SHARED_LIBS OFF`
(estatica, sin DLL extra en Windows), `ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT
OFF` + `ASSIMP_BUILD_OBJ_IMPORTER/GLTF_IMPORTER/FBX_IMPORTER ON`. Todos
los exportadores off (no exportamos desde el motor), tests + samples +
CLI tools off. `ASSIMP_WARNINGS_AS_ERRORS OFF` forzado como `CACHE BOOL`
antes del CPMAddPackage porque el codigo de assimp tiene warnings en
MSVC /W4 que no son nuestros.
**Razones:** triple cobertura de pipeline (OBJ = mas simple/legacy,
glTF = standard moderno, FBX = formato industria). ~30 MB menos de
binario respecto del build completo. Estatica evita el carrusel de
DLLs que ya tenemos con SDL2.
**Alternativas consideradas:**
- `tinyobjloader` + `cgltf` + nada para FBX: menos friccion de build,
  pero tres codebases distintas para mantener y no hay ruta clara a FBX
  sin reimplementarlo.
- assimp dinamica: ~5 MB menos en memoria pero obliga a copiar DLL
  post-build y complica packaging.
**Revisar si:** el tiempo de configure se vuelve insoportable (assimp es
el package mas pesado de CPM hoy) o si algun formato nuevo (USDZ) se
vuelve importante y no esta en v5.4.3.

## 2026-04-24: MeshLoader expande indices a vertices planos (sin EBO)

**Contexto:** assimp entrega cada mesh como `(vertices[], indices[])`.
El pipeline actual de render usa `OpenGLMesh` con `glDrawArrays` (sin
EBO). Dos opciones: migrar `IMesh` a soportar EBO o expandir a vertices
planos.
**Decision:** expandir. `loadMeshWithAssimp` itera las caras y escribe
los atributos del vertice referenciado (pos+color+uv) al buffer plano;
cada triangulo ocupa 3*8 floats sin comparticion.
**Razones:** matchea lo que `OpenGLRenderer::drawMesh` ya sabe dibujar;
cero cambios en el RHI. Para meshes pequenos (pyramid demo=18 verts,
Suzanne=~500 verts) el overhead de RAM es despreciable con expansion
~1.8x tipica. Migrar a EBO cuando aparezca un mesh real grande se hace
en un solo punto (nuevo `OpenGLIndexedMesh` + el loader deja de
expandir).
**Alternativas consideradas:**
- Agregar EBO a `OpenGLMesh` ahora: mas codigo que refactorizar, zero
  beneficio para los meshes del hito. YAGNI aplicado.
- Abstraer mesh indexed vs non-indexed por interfaz: prematuro con un
  solo backend.
**Revisar si:** primer mesh importado con >50k vertices o el perfil de
RAM empieza a doler en escenas con cientos de meshes.

## 2026-04-24: `NullMesh` stub como default de `MeshFactory` en AssetManager

**Contexto:** el ctor del `AssetManager` crea el mesh de fallback
(slot 0 = cubo primitivo) llamando a la `MeshFactory`. Si la factory no
se pasa, el ctor deberia fallar — pero 10+ tests que no tocan GL se
romperian en cascada (construyen el manager solo para testear caching
/ VFS).
**Decision:** si `MeshFactory` es null, el `AssetManager` la defaultea a
una que produce un `NullMesh` (IMesh stub interno, `vertexCount()` = lo
que sale de los attrs, `bind/unbind` no-ops). Produccion siempre pasa
una factory real que crea `OpenGLMesh` (ver `EditorApplication`).
**Razones:** misma filosofia que el `AudioClipFactory` defaultea a
construir `AudioClip` directo. Mantiene los tests existentes sin cambios
y no obliga a cada test a fabricar un stub manualmente.
**Alternativas consideradas:** exigir `MeshFactory` y migrar todos los
tests. Viable pero mucho churn para cero beneficio.
**Revisar si:** el `NullMesh` empieza a usarse en contextos no-test
(bug) — ahi el comportamiento silencioso es peligroso y habria que
volver al enforcement estricto.

## 2026-04-24: Render scene-driven unificado; GridRenderer eliminado

**Contexto:** desde Hito 8 Bloque 6 habia dos pases de render: el
`GridRenderer` (loop por tiles del `GridMap`) y un pase Scene-driven que
dibujaba entidades con `MeshRenderer` filtrando `Tile_*` (workaround para
que las tiles no se dibujaran dos veces). Con el Hito 10 las tiles ya
son entidades (`rebuildSceneFromMap` las crea) y tienen un `MeshRenderer`
apuntando al cubo fallback; el `GridRenderer` es redundante.
**Decision:** un solo loop scene-driven. `GridRenderer` eliminado
(archivos borrados). `renderSceneToViewport` itera
`Scene::forEach<Transform, MeshRenderer>` sin filtros. `GridMap` sigue
siendo la fuente authoritative de la topologia del mapa (physics +
serializer + `rebuildSceneFromMap`); solo el render dejo de depender de
el.
**Razones:** menos codigo (la logica de "donde dibujar tiles" vive en
un solo lugar — `rebuildSceneFromMap`). Habilita mezclar tiles con
modelos importados sin casos especiales. Prepara el camino a Hito 14+
donde la Scene sera authoritative para persistencia tambien.
**Alternativas consideradas:**
- Mantener `GridRenderer` por perfo (un solo draw call por tile en loop
  tight vs un submesh loop por entidad): zero diferencia medible con 30
  tiles, y perderiamos la unificacion.
- Usar un marker component `TileBackedComponent` para el filtro: agrega
  componente nuevo por caso puntual que va a morir cuando el render no
  distinga tiles de modelos.
**Revisar si:** escenas de cientos de tiles + muchos meshes empiezan a
pinchar — ahi vale la pena un render sort/batching (Hito 18+).

## 2026-04-24: `updateTileEntity` para edits localizados (preserva seleccion)

**Contexto:** el drop de textura sobre un tile antes llamaba
`m_map.setTile + rebuildSceneFromMap`. `rebuildSceneFromMap` hace
`registry.clear()` que invalida la seleccion del Hierarchy (el `Entity`
del panel apunta a un handle muerto). UX raro: parece que se "resetea"
la seleccion sin motivo aparente.
**Decision:** nuevo helper `EditorApplication::updateTileEntity(x, y, tex)`.
Busca la entidad con tag `Tile_X_Y` por `forEach` lineal (max ~30
entidades en mapas 8x8), edita `MeshRenderer.materials[0]` in-place, o
crea una nueva entidad tile si la celda era Empty. El `GridMap.setTile`
se sigue llamando en paralelo para mantener physics/serializer
sincronizados.
**Razones:** preserva la seleccion y evita un registry rebuild completo.
`rebuildSceneFromMap` queda reservado para cambios globales (open/close
proyecto, buildInitialTestMap).
**Alternativas consideradas:** un mapa `(x,y) -> entt::entity` para O(1)
lookup. Overkill para 30 entidades; se agrega cuando los mapas crezcan.

## 2026-04-24: `.moodmap` schema v2 con seccion `entities` opcional

**Contexto:** Hito 10 persiste por primera vez entidades del Scene (los
meshes dropeados). El schema v1 solo conocia tiles.
**Decision:** bump a `k_MoodmapFormatVersion = 2`. Nueva seccion
`entities` = array de `{tag, transform:{position, rotationEuler, scale},
mesh_renderer:{mesh_path, materials[]}}`. Filtro de serializacion: solo
entidades cuyo tag NO empieza con `"Tile_"` (esas se reconstruyen del
grid en `rebuildSceneFromMap`) Y tienen `MeshRendererComponent`. Paths
logicos en texturas/meshes para que los ids volatiles no se filtren al
JSON. Al leer, `entities` es opcional (archivos v1 quedan con lista
vacia), bump justificado porque el writer nuevo produce archivos que
un reader v1 no entenderia.
**Razones:** primer paso hacia Scene authoritative sin romper compat con
proyectos v1 ya creados. Solo MeshRenderer en este primer pase:
`ScriptComponent` y `AudioSourceComponent` tienen estado runtime
(`sol::state`, `SoundHandle`) que no tiene sentido serializar.
**Alternativas consideradas:**
- Persistir TODOS los componentes ahora: scope creep del Hito 10 +
  estado runtime contamina el JSON.
- Schema separado `.moodscene` para entidades: duplica el problema del
  roundtrip entre archivos. Mejor un solo archivo por mapa por ahora.
**Revisar si:** aparece un schema evolution mayor (Hito 14 prefabs) que
justifique partir `.moodmap` en `.moodmap` + `.moodscene`.

## 2026-04-24: `.gitignore` excepcion para Wavefront OBJ

**Contexto:** el `.gitignore` tenia `*.obj` para objetos compilados de
MSVC. Los Wavefront OBJ (modelos 3D) tambien usan `.obj` y quedaban
ignorados: `assets/meshes/pyramid.obj` no se committeaba.
**Decision:** agregar `!assets/meshes/*.obj` como excepcion al patron.
**Razones:** mantiene el patron general para evitar commitear artefactos
de build sin migrar toda la convencion a un subfolder aparte.
**Alternativas consideradas:** mover los OBJ de assets a otro path
(`assets/models/`). Renombrar por una colision de extensions es lo
opuesto al principio de menor sorpresa.

## 2026-04-24: Vertex layout pos+color+uv+normal (stride 11) en lugar de capas separadas

**Contexto:** Hito 11 necesita normales en cada vértice. Opciones: (a) extender el layout existente in-place a stride 11; (b) tener dos layouts paralelos (legacy stride 8 + nuevo stride 11) y elegir por shader; (c) un VBO interleaved + un VBO de normales separado.
**Decisión:** (a) — extender el layout existente. `createCubeMesh` y `MeshLoader` siempre emiten stride 11. El shader `default.{vert,frag}` queda como está y simplemente ignora la `location = 3` que recibe (OpenGL no falla por atributos no usados en el VAO).
**Razones:** menos branching en el código de carga, un solo path. La sobrecarga de 3 floats por vértice es trivial frente a los volúmenes actuales (~10k vértices). Mantener dos layouts agrega complejidad por un beneficio mínimo.
**Alternativas consideradas:** (b) elegante pero genera deuda de migrar después; (c) más caché-friendly para draws que sólo necesitan posición (shadows), pero hoy no hace falta y multiplica VBOs/VAOs.
**Revisar si:** aparece un draw path que sólo lea posición (shadow mapping en Hito 16). Ahí evaluar pull a un buffer por atributo o vertex stream layouts (gl 4.5 attrib divisor / separate buffer bindings).

## 2026-04-24: Inverse-transpose por vertex en GLSL en lugar de `uNormalMatrix`

**Contexto:** transformar normales con escalas no uniformes requiere `mat3(transpose(inverse(uModel)))`. Lo común en engines es pre-calcularlo en CPU y pasar como uniform.
**Decisión:** en `lit.vert`, hacerlo por vertex.
**Razones:** las escenas actuales tienen ~30 entidades; el costo de la inversa por vertex es despreciable y el código queda más simple (un uniform menos que sincronizar en CPU). Cuando los volúmenes crezcan o aparezca skinning (Hito 19), pasar al uniform es directo.
**Alternativas consideradas:** uniform pre-calculado en CPU — lo correcto a largo plazo pero overengineering para hoy.
**Revisar si:** el profiler muestra que el vertex shader se vuelve cuello de botella (post-Hito 18 con muchos draws).

## 2026-04-24: Atenuación cuadrática smooth con cutoff por radius (no la fórmula 1/(a+b·d+c·d²))

**Contexto:** modelo de atenuación para `PointLight`.
**Decisión:** `att = (1 - clamp(dist/radius, 0, 1))^2`. Cae a 0 exactamente en `radius`, suave en el medio.
**Razones:** UX para diseño de niveles: el modder dice "esta luz alcanza 12 metros" y el resultado coincide. La fórmula clásica con (constant + linear + quadratic) requiere afinar 3 parámetros para cortar en una distancia dada.
**Alternativas consideradas:** atenuación 1/d² física-correcta + cutoff suave — coherente con PBR pero exagerado para Blinn-Phong AAA-2010 (el target estético del motor). Dejar abierto para Hito 17.
**Revisar si:** los assets requieren matching con DCC tools (Blender, Maya) que asumen atenuación inversa-cuadrada física.

## 2026-04-24: Single sun + N point lights (sin SpotLight ni multi-Directional)

**Contexto:** API de luces para Hito 11.
**Decisión:** el shader `lit.frag` soporta exactamente UNA `DirectionalLight` (uniform `uDirectional`) + hasta 8 `PointLight`. `LightSystem::buildFrameData` toma la primera Directional que encuentra y la usa; las demás se ignoran.
**Razones:** alcanza para "hay un sol" + luces locales. Un mapa real rara vez quiere dos directionales (¿dos soles?). SpotLight + multiple directionals son features de polish que entran cuando un escenario lo justifique.
**Alternativas consideradas:** array de hasta 4 directionals desde el día uno — más uniforms a manejar, branching en el shader, sin caso de uso concreto.
**Revisar si:** Hito 14+ trae un escenario "noche con focos" o un nivel con luna + sol cinemático.

## 2026-04-24: Bindings por nombre en `LightSystem::bindUniforms` (no UBO)

**Contexto:** subir N PointLights al shader.
**Decisión:** loop CPU que llama `setVec3("uPointLights[0].position", ...)` etc. Nombres construidos con `snprintf`.
**Razones:** la cache de `glGetUniformLocation` en `OpenGLShader` lo hace barato (una sola consulta GL por uniform en todo el lifetime). Sin UBO, sin std140 padding, sin layout pinning. Para 8 luces × 4 campos = 32 uniforms; trivial.
**Alternativas consideradas:** UBO (Uniform Buffer Object) con std140 layout — más rápido y más cercano a Vulkan/DX12, pero overengineered para 8 lights. Migrar cuando el limit suba (Hito 18 deferred / clustered).
**Revisar si:** se sube `MAX_POINT_LIGHTS` arriba de 64 (cluster forward o deferred), o si aparece SSBO necesario para tile-based shading.

## 2026-04-24: Jolt Physics v5.2.0 + runtime dinamica forzada (Hito 12)

**Contexto:** eleccion de motor de fisica para el Hito 12.
**Decision:** Jolt Physics v5.2.0 via CPM (`jrouwe/JoltPhysics`). `USE_STATIC_MSVC_RUNTIME_LIBRARY OFF` para alinear con `/MDd` del resto del proyecto (Jolt default `/MTd` produce LNK2038). Targets auxiliares (samples/viewer/tests/hello world) apagados — solo la lib.
**Razones:** Jolt es usado por Horizon Zero Dawn 2, Doom Eternal y otros AAA. Multiplataforma, determinista opcional (no lo activamos), zlib/MIT dual license. Performance excelente para rigid bodies sin ragdoll (nuestro target). Alternativas: PhysX (license NVIDIA compleja), Bullet (antiguo, API menos limpia), ReactPhysics (menos maduro).
**Alternativas consideradas:** Bullet — descartado por ser tecnologia de hace 15 anos aunque sigue siendo sólido. PhysX — license + complejidad del SDK. React Physics — insuficiente en rendimiento para AAA.
**Revisar si:** necesitamos doble precision (cambiar `DOUBLE_PRECISION ON`), vehicle simulation, o soft-body (Jolt los soporta pero requieren otros constraints).

## 2026-04-24: `PhysicsWorld` como facade sin exponer tipos Jolt

**Contexto:** diseno de la API para consumidores de Jolt en el motor.
**Decision:** `PhysicsWorld.h` usa forward decls de `JPH::PhysicsSystem`, `JPH::BodyID` etc. Nada de `<Jolt/...>` en headers. `BodyID` se expone como `u32` (via `GetIndexAndSequenceNumber()`). Cuerpo de la API: `createBody`, `destroyBody`, `bodyPosition/setBodyPosition`, `addForce/addImpulse`, `step`.
**Razones:** evita arrastrar la jerarquia de headers de Jolt a todo el motor. Si aparece un segundo backend de fisica (improbable pero posible), la facade no cambia. Los consumidores (EditorApplication) no tienen que entender `JPH::RVec3` vs `glm::vec3`.
**Alternativas consideradas:** exponer tipos Jolt directo (menos codigo, mas eficiente) — descartado porque filtraria 50+ includes al resto del motor.
**Revisar si:** necesitamos features avanzadas de Jolt que no encajan en la facade (constraints custom, ragdoll, mesh colliders complejos).

## 2026-04-24: Map tiles como RigidBodyComponent(Static) uniforme

**Contexto:** como crear los static bodies del mapa en Jolt.
**Decision:** en `rebuildSceneFromMap`, cada tile solido spawna con `RigidBodyComponent(Static, Box, halfExtents=tileSize/2)` ademas de Transform + MeshRenderer. `updateRigidBodies` materializa el body en el proximo frame igual que cualquier otra entidad.
**Razones:** un solo code path para todos los bodies del motor. Si despues queremos editar la colision de un tile (ej. "esta pared es empujable"), basta con cambiar su `type` a Dynamic — todo lo demas ya funciona. Alternativa de "API separada para bodies del mapa" duplica complejidad.
**Alternativas consideradas:** `PhysicsWorld` expone `createStaticBoxFromGrid(map)` — menos componentes pero menos flexible. La uniformidad del componente gana.
**Revisar si:** el mapa crece a mapas con miles de tiles — Jolt puede manejarlo pero el body-per-tile es ~O(n) en memoria. Optimizacion posible: consolidar corridas de tiles en un solo MeshShape (Hito 13+ con mesh colliders).

## 2026-04-24: Pose de rigid body NO persistida en .moodmap

**Contexto:** que campos del `RigidBodyComponent` se guardan.
**Decision:** `SavedRigidBody` solo persiste `type`, `shape`, `halfExtents`, `mass`. La posicion/rotacion actual del body **no**. Al cargar, el body se crea en la posicion del `TransformComponent` de la entidad (que si se persiste).
**Razones:** Jolt es authoritative en runtime — la `Transform.position` que escribe `updateRigidBodies` despues de `step()` ya refleja la pose del body. Persistir ambos duplicaria la fuente de verdad y si divergen, cual gana? Tambien: los usuarios esperan que "guardar" capture el estado inicial (disenado), no el runtime (la caja despues de rodar).
**Alternativas consideradas:** guardar tambien pose actual + flag "esta es la inicial o post-simulacion" — overengineering.
**Revisar si:** aparece un caso de "quiero guardar el estado simulado" (savegames gameplay) — eso es un formato distinto del `.moodmap` (que es un asset/level file).

## 2026-04-24: CharacterController diferido post-Hito 12

**Contexto:** el plan pedia que el jugador de Play Mode use `JPH::CharacterVirtual` en vez del `moveAndSlide` AABB del Hito 4.
**Decision:** diferido. El jugador sigue con `moveAndSlide`. Los rigid bodies dinamicos del demo (caja) funcionan sobre Jolt correctamente; lo que falta es migrar la fisica del jugador.
**Razones:** `CharacterVirtual` requiere handling correcto de slope, step-up, ground detection, jump state machine, y callbacks de colision. Implementarlo "bien" es varios dias; implementarlo mal produce un jugador que se traba en esquinas o clip-throughs. El hito ya cumple el core objetivo (Jolt integrado, rigid bodies demos). Preferimos diferirlo a cuando tengamos gizmos (Hito 13) para poder mover/ajustar al player interactivamente y depurar el controller, en vez de hacerlo a ciegas.
**Alternativas consideradas:** implementar un `Character` (no `CharacterVirtual`) rapido con solo gravedad + colision horizontal — funciona pero la calidad es inferior a lo que ya hace `moveAndSlide`. Net negative de UX si apuramos.
**Revisar si:** se termina el Hito 13 (gizmos) o aparece gameplay concreto que necesite jump/slope (plataformas, puzzles).

## 2026-04-24: Gizmos en screen-space con `ImDrawList`, no geometria GL (Hito 13)

**Contexto:** como renderizar los gizmos 3D (flechas de translate, anillos de rotate, cajas de scale).
**Decision:** dibujar los gizmos como polylines 2D via `ImDrawList` en un callback que `ViewportPanel` invoca DESPUES de `ImGui::Image` del framebuffer. `EditorApplication` proyecta los world-points del gizmo a screen-space con las `m_lastView/m_lastProjection` del frame anterior, y el `ImDrawList` los pinta con clip-rect al area de la imagen.
**Razones:** cero shaders nuevos, cero VBOs, cero depth buffer para overlays — los gizmos son andamios, no parte de la escena. Reutilizamos el stack de ImGui que ya es dependencia del editor. El "1 frame de lag" por usar matrices del frame anterior es imperceptible a la practica (gizmos no se mueven sin input del usuario, que es lo que dispara el re-render).
**Alternativas consideradas:** mesh + shader dedicado (torus + arrow mesh) con GL passes — mas lineas de codigo, mas estado que mantener, y el picking seguiria siendo math 2D. Integrar ImGuizmo (lib externa) — descartado: queriamos escribir la math propia para entenderla + no depender de un tercero para algo que es tan central al editor.
**Revisar si:** aparece un caso de gizmo que requiera oclusion real con la escena (ej. transparencias 3D), o multi-viewport con gizmos en cada uno.

## 2026-04-24: Picking por AABB unitario escalado, no por malla real (Hito 13)

**Contexto:** `ScenePick::pickEntity` necesita intersectar el rayo del mouse contra cada entidad.
**Decision:** toda entidad con `MeshRendererComponent` se testea contra un AABB `[-0.5, 0.5]^3` local, transformado por `Transform.position` + `Transform.scale`. Entidades sin mesh (Light, AudioSource) se testean contra una esfera world-space de radio 0.6m centrada en `Transform.position`.
**Razones:** el AABB unitario coincide con el primitivo `cube.obj` (default de la demo y tile de mapa). Para modelos importados, es una conservative hitbox: el click selecciona "cerca" del modelo, con un poco de falso-positivo alrededor — aceptable para un editor. No requerimos construir BVH por mesh ni iterar vertices. La esfera de 0.6m para iconos es ~2x el tamano visual del icono en pantalla, minimiza "no le pego a la luz".
**Alternativas consideradas:** integrar `PhysicsWorld::rayCast` de Jolt + tabla BodyID → Entity — mas preciso pero solo funciona con entidades que tienen `RigidBodyComponent`, y las luces/audio no lo tienen. Tambien arrastra el costo de Jolt a un feature puramente de editor. `MeshFromAsset` con bounds reales por modelo — alineado con futuro mesh collider pero overkill para el loop "spawneo un cubo y lo selecciono".
**Revisar si:** empieza a molestar el false-positive con modelos no-cuboides (ej. dos esferas cercanas donde el AABB se superpone). Trigger: Hito 11+ con BVH + `MeshFromAsset` disponible.

## 2026-04-24: Click vs drag con threshold de 4px (Hito 13)

**Contexto:** el mismo mouse-button (left) se usa para click-to-select y para drag-de-gizmo. Hay que decidir cual es cual.
**Decision:** `ViewportPanel` registra la posicion del mouseDown sobre la imagen; al mouseUp, si `|delta|^2 < 16` (4px) y el mouseDown fue sobre el viewport, dispara `ClickSelect{ndcX, ndcY}`. Si se movio mas, fue un drag (camara o gizmo) y el click-select se descarta. Ademas, si el gizmo consumio el click (flag `m_gizmoConsumedClick` en `EditorApplication`), el click-select se suprime aunque el mouse no haya drifteado.
**Razones:** 4px es el umbral estandar en editores (Unity, Godot lo usan). Diferenciar por intencion, no por tiempo — evita la UX de "mantenelo quieto 100ms" que es fragil. El flag doble (gizmo + threshold) cubre dos escenarios: "click puro que no tocaba el gizmo" (threshold) y "mouse down sobre el gizmo sin mover" (flag).
**Alternativas consideradas:** threshold de tiempo (si el boton se solto en <150ms cuenta como click) — menos intuitivo, depende del framerate. Diferenciar el button (left = select, middle = gizmo) — rompe convencion de editores 3D.
**Revisar si:** algun usuario reporta que sus clicks no se registran (monitor sensible al jitter) — entonces subir a 6-8px.

## 2026-04-24: Uniform-scale handle = cuadrado blanco central (Hito 13)

**Contexto:** el scale gizmo original tenia 3 ejes (X/Y/Z) pero no habia forma de escalar uniforme sin escribir los 3 componentes a mano o usar el Inspector.
**Decision:** en modo Scale, ademas de los 3 handles de eje (index 0/1/2) hay un cuarto handle (index 3) dibujado como cuadrado blanco en el origen del gizmo. Drag sobre el: factor `1 + deltaPx / k_armLen` (k_armLen=60px), aplicado a los 3 componentes de `Transform.scale`. Clamp inferior a 0.01 para evitar scale=0 que colapsa la mesh.
**Razones:** pedido explicito del dev. Convencion comun (Unity, Blender tienen un handle central para uniform). Un solo `delta_px` se convierte a factor multiplicativo — mas predecible que intentar leer los 3 ejes por separado y promediar. Clamp >= 0.01 evita artifacts de rendering (normal matrix degenerada cuando scale llega a 0).
**Alternativas consideradas:** toggle global "lockear uniform scale" en el Inspector — menos descubrible. Shift+drag sobre cualquier eje → uniform — poco intuitivo sin UI hint.
**Revisar si:** aparece el requerimiento de un "bounding box drag" estilo GIMP (arrastrar corner del AABB para escalar 2 ejes a la vez).

## 2026-04-24: Inspector esconde scale/rotation sin MeshRenderer (Hito 13)

**Contexto:** `InspectorPanel` mostraba 3 DragFloat3 (position/rotation/scale) para cualquier entidad con `TransformComponent`. Problema: una `PointLight` mostraba campos de escala/rotacion que no afectan nada (las luces puntuales no tienen orientacion y scale no escala el radio). Los usuarios los tocaban y no pasaba nada.
**Decision:** si la entidad **no** tiene `MeshRendererComponent`, el Inspector solo muestra `position`. `rotationEuler` y `scale` quedan ocultos. Si se agrega MeshRenderer mas tarde, aparecen.
**Razones:** feedback directo del dev — "no tiene sentido escalar una luz". La UI es un documento del modelo mental: mostrar solo lo que aplica. Si en el futuro una luz "direccional" requiere rotation, esa visibilidad la gatilla `LightComponent::Type::Directional`, no el Transform crudo.
**Alternativas consideradas:** disable (grey out) en vez de ocultar — todavia distrae. Dejarlo como estaba — UX floja.
**Revisar si:** se agrega un componente que legitimamente usa rotation/scale del Transform sin MeshRenderer (ej. AudioListener con cono direccional) — extender el condicional para incluirlo.

## 2026-04-24: Overlays de edicion ocultos en Play Mode (Hito 13)

**Contexto:** los iconos 2D de Light/AudioSource + los gizmos 3D son andamios del editor. Al entrar a Play Mode se seguian dibujando.
**Decision:** early return del callback de overlay si `m_mode == EditorMode::Play`. Los iconos + gizmos solo existen en Editor Mode.
**Razones:** Play Mode existe para ver el juego como lo vera el jugador final. Los iconos rompen la inmersion (un circulo amarillo flotando = "esto es debug"). Los gizmos sobre el player en Play tampoco tienen sentido — no estas editando, estas jugando.
**Alternativas consideradas:** toggle manual ("show editor overlays in play") — overhead innecesario, nadie va a querer eso por default.
**Revisar si:** se agrega un "play-in-editor with debug overlays" flow (comun en juegos AAA para QA). Trigger: Hito 22+ con tooling de QA.

## 2026-04-26: `EntitySerializer` compartido entre `.moodmap` y `.moodprefab`

**Contexto:** Hito 14 introduce `.moodprefab` como nuevo formato. Tanto
ese formato como el `.moodmap` (Hito 6, extendido en 10/11/12) tienen
que serializar entidades con sus componentes (Tag, Transform,
MeshRenderer, Light, RigidBody). En el `.moodmap` la lógica vivia en
funciones libres `serializeEntity` / `parseEntity` dentro del namespace
anonimo de `SceneSerializer.cpp`. Duplicar esa logica en
`PrefabSerializer.cpp` deja la puerta abierta a que un componente nuevo
se agregue a un solo lado y se pierdan datos en round-trips.
**Decision:** extraer las dos funciones a un header publico —
`engine/serialization/EntitySerializer.{h,cpp}` — con firmas
`serializeEntityToJson(Entity, AssetManager) -> json` y
`parseEntityFromJson(json) -> SavedEntity`. `SceneSerializer.cpp`
mantiene wrappers locales que delegan al helper para minimizar el diff
con el codigo legacy.
**Razones:** un solo punto donde mapear Component <-> JSON. Cualquier
componente nuevo (en este hito: `PrefabLinkComponent`) se agrega una
vez y aparece automatico en ambos formatos. Cero duplicacion.
**Alternativas consideradas:** dejar el helper en SceneSerializer y que
PrefabSerializer lo llame con `friend` o exponiendo la funcion via
header privado: friccion sin beneficio, viola la convencion de
"namespace anonimo = privado al .cpp".
**Revisar si:** aparece un nuevo formato (Hito 21 packaging) que
necesite un mapeo distinto — ahi el helper tendria que aceptar un
`SerializeOptions` para seleccionar que componentes incluir.

## 2026-04-26: Prefabs como assets globales del repo

**Contexto:** Hito 14 plan original sugeria guardar `.moodprefab` en
`<proyecto>/assets/prefabs/` (per-proyecto, estilo Unity). En el primer
smoke test el usuario guardo un prefab pero al cambiar de proyecto el
AssetBrowser no lo listaba — bug obvio: el browser escanea
`<cwd>/assets/prefabs/`, no la carpeta del proyecto activo.
**Decision:** los prefabs son **assets globales del repo**, vivem en
`<cwd>/assets/prefabs/`. Misma convencion que texturas / audio / meshes
(todos en `<cwd>/assets/<tipo>/`). El proyecto solo persiste mapas
(`.moodmap`) + manifiesto (`.moodproj`); los assets se comparten.
**Razones:**
- Consistencia: el editor tenia un modelo "assets globales" desde
  Hito 5; los prefabs lo violaban sin razon.
- Reuso: un prefab "torch" sirve para todos los proyectos sin duplicar.
- UX: aparecen automaticos en el AssetBrowser sin importar el proyecto.
**Alternativas consideradas:**
- Per-proyecto puro: requiere refactor del AssetBrowser para que sepa
  del proyecto activo; rompe el modelo. Util si los proyectos quieren
  versiones distintas del mismo prefab — caso real escaso a esta etapa.
- Hibrido (prefabs en `<proyecto>/...` + un fallback al global): mas
  flexibilidad, mas complejidad.
**Revisar si:** un usuario quiere "este prefab solo para este proyecto"
con sentido real (cuando los proyectos sean grandes y los assets se
ensucien entre si).

## 2026-04-26: `PrefabLinkComponent` como string-only marker

**Contexto:** una entidad instanciada de un prefab necesita "recordar"
de donde vino para futuras features (revert/apply/scan).
**Decision:** componente nuevo con un solo campo `std::string path`.
Sin metadata extra (versioning del prefab, dirty tracking, override
list, timestamp). Solo el path logico relativo a `assets/`.
**Razones:** Hito 14 NO implementa propagacion bidireccional, asi que
no hay nada que trackear. Un string es suficiente para el uso actual y
es trivialmente serializable. Si en un hito futuro aparece la necesidad
real de "lista de overrides modificados", se agrega un struct sin
romper compat (campo nuevo opcional al parsear).
**Alternativas consideradas:**
- Struct con `path + lastSyncedTimestamp + List<string> overrides`:
  prematuro. Hace mas pesado el round-trip y aporta cero hoy.
- Storage externo (un mapa entt::entity -> link en el editor): se
  pierde al cerrar el editor; no se serializa.
**Revisar si:** la propagacion bidireccional entra y necesita trackear
overrides explicitos.

## 2026-04-26: Sin nested prefabs en Hito 14 (postergado)

**Contexto:** un prefab podria contener una entidad que es a su vez una
instancia de otro prefab. Unity lo soporta (con muchas restricciones
historicas).
**Decision:** no en este hito. El schema reserva `children: []` pero
todos los `.moodprefab` guardados desde el editor producen array vacio.
Razon tecnica: `TransformComponent` del Hito 7 no expone `parent`/
`children`, asi que la instanciacion no puede reconstruir jerarquia.
**Razones:** entrega valor incremental sin bloquear el resto del hito.
Los prefabs flat ya son el 90% de los casos de uso (item, prop, light
preset).
**Alternativas consideradas:** agregar `ParentComponent` + `ChildrenComponent`
ahora — scope creep significativo (afecta Hierarchy, gizmos,
serializer, picking). Mejor en hito dedicado.
**Revisar si:** aparece un caso real de "torch_with_flame" donde el
flame es otra entidad ECS.

## 2026-04-26: Drop de prefab siempre instancia, no reemplaza

**Contexto:** plan original incluia "drop sobre una entidad existente
(via Hierarchy) reemplaza su mesh".
**Decision:** diferido. Hoy el drop SIEMPRE crea una entidad nueva en
el tile bajo el cursor. Reemplazar quedo postergado.
**Razones:** ambiguedad semantica que no se puede resolver sin un
caso de uso real:
- ¿Reemplazar todos los componentes o solo MeshRenderer?
- ¿Conservar el Transform local?
- ¿Renombrar el tag al del prefab o conservar el original?
- ¿Que pasa si la entidad existente tenia componentes que el prefab no?
Sin un workflow concreto que pida "swap mesh by prefab", elegir mal
introduce un comportamiento confuso dificil de revertir.
**Alternativas consideradas:** implementar la version mas conservadora
(solo agrega/reemplaza componentes que el prefab tiene, mantiene el
Transform). Plausible pero a riesgo de pisar trabajo del usuario.
**Revisar si:** aparece un workflow real ("tengo 50 props, quiero
swappear todos al modelo nuevo del prefab X").

## 2026-04-26: `pfd::save_file` requiere path nativo + filename saneado

**Contexto:** smoke test del Hito 14 — al hacer "Archivo > Guardar como
prefab..." el dialogo nativo de Windows no aparecia (devolvia "" sin
mostrarse) y el log marcaba `Guardar prefab: cancelado`. Comparando
con `handleNewProject` que SI funciona: ese pasa solo el directorio;
el handler de prefab pasaba `<dir>/<filename>`.
**Decision:** dos fixes combinados:
1. **Sanitizar el filename**: tags pueden tener caracteres invalidos
   para Windows shell (`<>:"/\|?*`). Caso real: `Mesh_meshes/pyramid.obj_1`
   tras un drop de mesh — `/` rompe el filename. Helper `sanitize`
   reemplaza por `_`.
2. **Path nativo, no generic**: `generic_string()` produce forward
   slashes; pfd::save_file en Windows usa IFileSaveDialog que para
   paths FILE+DIR espera backslashes. `string()` mantiene los
   separadores nativos del filesystem.
`handleNewProject` no tenia el bug porque pasa solo el cwd (directorio,
sin filename). Cuando hay archivo en el path, los separadores importan.
**Razones:** el fix permite seguir pre-rellenando el nombre del prefab
en el dialogo (UX mejor que pedirle al usuario que escriba "Tile_6_7"
manualmente). Si pasaramos solo el directorio, el dialogo aparece pero
sin nombre default.
**Alternativas consideradas:** pasar solo el directorio (como
handleNewProject) y dejar que el usuario escriba el nombre. Funciona
pero peor UX — y la causa raiz del bug igual hay que entenderla porque
otros call-sites futuros podrian repetirla.
**Revisar si:** se migra a un file dialog propio (ImGui-based) en
Linux — ahi pfd hace cosas distintas y los separadores serian
homogeneos.

## 2026-04-26: Skybox via cubemap + truc pos.z=w (sin abstraccion ICubemapTexture)

**Contexto:** Hito 15 introduce el primer skybox del motor. La opcion
"correcta a futuro" es abstraer `ICubemapTexture` analogo a `ITexture`,
pero hoy hay un solo backend (OpenGL) y un solo consumidor (`SkyboxRenderer`).
**Decision:** mantener la cubemap dentro de `engine/render/opengl/` sin
interfaz abstracta. `OpenGLCubemapTexture` carga 6 PNGs con stb_image,
crea `GL_TEXTURE_CUBE_MAP` con LINEAR + CLAMP_TO_EDGE en S/T/R.
`SkyboxRenderer` owns el cubemap + shader + cubo unitario; `draw()` usa
el truc `gl_Position.z = gl_Position.w` para que tras /w el depth quede
en 1 (far plane), con depth_func temporalmente cambiado a LEQUAL para
que el clear con depth=1 no descarte el sky. La translacion de la view
matrix se anula con `mat4(mat3(view))` para que el sky no se desplace
al mover la camara.
**Razones:** entrega valor (skybox visible) sin generar abstraccion
prematura. Cuando IBL (Hito 17) entre, ahi se extrae `ICubemapTexture`
con dos clientes reales.
**Alternativas consideradas:** abstraer ya — codigo nuevo sin cliente
adicional, viola "no introducir abstraccion antes que el segundo
consumidor".

## 2026-04-26: Fog distance-based con replica matematica en C++/GLSL

**Contexto:** el fog del Hito 15 corre en el shader pero queremos
testearlo sin GL.
**Decision:** `engine/render/Fog.h` (header-only) con `FogMode` (Off /
Linear / Exp / Exp2), `FogParams` y funciones puras `computeFogFactor`
+ `applyFog`. El fragment shader `lit.frag` replica la misma logica
con un `if (uFogMode == ...)` por modo. El bind de uniforms en
`EditorApplication` lee de `m_fog` (FogParams).
**Razones:** los modos del fog son matematica simple pero faciles de
pifiar (limites del lineal, signos del exp); tener tests sin GL captura
regresiones rapido. La duplicacion C++/GLSL es minima (~30 lineas) y
explicita.
**Alternativas consideradas:** generar GLSL desde el header con un
script — ingenieria por gusto.

## 2026-04-26: Pipeline HDR via segundo framebuffer + post-process pass

**Contexto:** sin HDR los highlights de las luces se queman a blanco;
no hay forma de aplicar exposicion ni tonemapping no-trivial.
**Decision:** dos framebuffers para el viewport del editor:
- `m_sceneFb` (RGBA16F, HDR): aqui se dibuja sky + escena + lit + fog
  + debug. Permite valores > 1 sin clip.
- `m_viewportFb` (RGBA8, LDR): lo que ImGui muestra dentro del panel.
Un `PostProcessPass` aplica `2^exposure * color` + tonemap (None /
Reinhard / ACES Narkowicz fast) + gamma 1/2.2 al pasar de uno a otro.
Fullscreen triangle generado por `gl_VertexID` (sin VBO; un VAO trivial
porque Core Profile lo exige).
**Razones:** habilita exposicion + tonemapping sin parchar todo el
pipeline. Mantener LDR final compatible con ImGui (que asume sRGB).
ACES como default es perceptualmente mejor que clamp directo en
escenas con highlights.
**Alternativas consideradas:** un solo FB sRGB con `glEnable(GL_FRAMEBUFFER_SRGB)`
y dejar el tonemap fuera. Mas dependiente del driver y dificulta el
control fino. Tambien evaluado: bloom integrado en este hito —
diferido a 18+.
**Revisar si:** llega bloom (Hito 18) — el `m_sceneFb` HDR es la
fuente natural para downsamples.

## 2026-04-26: `EnvironmentComponent` en una entidad cualquiera (no panel global)

**Contexto:** los parametros de skybox/fog/exposure/tonemap necesitan
una UI editable y persistible.
**Decision:** se expone como un componente ECS, agregable a cualquier
entidad (convencion: una entidad vacia "Environment"). El render scan
toma el primer `EnvironmentComponent` encontrado y lo aplica al frame.
Si no hay ninguno, defaults seguros (fog Off, exposure 0, tonemap ACES).
Se serializa como un `SavedEnvironment` opcional en cada `SavedEntity`.
**Razones:** reusa la persistencia, el inspector, el scan y la
serialization del ECS sin codigo nuevo. Un panel "Render Settings"
global hubiera sido un singleton + UI separada + persistencia separada.
**Alternativas consideradas:** singleton en `EditorApplication` + panel
dedicado. Mas trabajo, peor encaje con el flow ECS-driven del editor.
**Revisar si:** hay 5+ campos de "render settings" que no encajan en
un componente serializable (luminance, fog volumetrico, etc.) — ahi un
panel dedicado tiene mas sentido.

## 2026-04-26: Reset a defaults antes de aplicar Environment cada frame

**Contexto:** bug detectado en smoke test del Hito 15: tras abrir un
proyecto con Environment fog verde y luego abrir otro proyecto sin
Environment, los uniforms del fog/exposure/tonemap quedaban "colgados"
con los valores del previo. La causa: el scan solo seteaba si encontraba
un componente, sin reset previo.
**Decision:** `applyEnvironmentFromScene` resetea `m_fog =
FogParams{}` + `m_exposure = 0` + `m_tonemap = ACES` ANTES del scan;
si encuentra un Environment, sobreescribe. Llamado por
`renderSceneToViewport` cada frame Y por `tryOpenProjectPath` justo
despues de cargar las entidades — asi la primera frame post-load
no muestra un flash a defaults.
**Razones:** una sola fuente de verdad por frame. El reset elimina
"fugas" entre proyectos sin requerir invalidacion explicita en
handlers de Cerrar / Abrir / Nuevo.
**Alternativas consideradas:** resetear los uniforms en cada handler
de proyecto (Cerrar / Abrir / Nuevo / etc.). Mas codigo, mas chances
de olvidar uno.

## 2026-04-26: El editor SIEMPRE arranca con el Welcome modal

**Contexto:** desde Hito 6 estaba implementado un auto-restore del
ultimo proyecto reciente (convencion Unity/Godot). En el smoke test
del Hito 15 el dev recordo que esto va contra una decision suya
explicita ("en cada relanzamiento debe estar limpio, nada cacheado").
El auto-restore:
- ocultaba el Welcome modal (raras veces se ve la lista de recientes),
- arrastraba estado del proyecto previo silenciosamente al arrancar,
- el dev creyo durante el smoke test que era estado "colgado" de la
  sesion anterior.
**Decision:** quitar el auto-open. `loadEditorState` SOLO puebla
`m_recentProjects` para alimentar el modal Welcome (y `m_debugDraw`
para preferencias globales). El modal aparece SIEMPRE al arrancar.
Memoria persistente `feedback_no_autoopen_project.md` registra la
decision para que ningun agente futuro la re-introduzca como
"convencion Unity".
**Razones:** la convencion Unity no es ley; lo importante es que el
flow encaje con como el dev quiere usar el editor. Si en el futuro
quiere "auto-open opcional", se mete detras de un toggle de
preferencias (off por default).
**Alternativas consideradas:** dejar el auto-open con un toggle
visible para apagarlo. Cinco veces hubo confusion durante este
hito; mejor cambiar el default y opt-in si alguien lo pide.

## 2026-04-26: Welcome modal con gestion de recientes (Limpiar inexistentes + X)

**Contexto:** sin auto-open, los proyectos recientes son la unica via
rapida para reabrir. La lista se ensucia cuando el dev mueve / borra /
renombra proyectos por afuera del editor.
**Decision:** dos affordances en el modal:
- Boton "Limpiar inexistentes" arriba de la lista — barre todos los
  recientes cuyo archivo ya no existe en disco.
- Boton "X" al final de cada fila — quita esa entrada manualmente.
Las filas inexistentes se renderizan en rojo con tag `(no existe)` y
su click no las abre, las marca para borrar.
`EditorUI::eraseRecent` y `pruneMissingRecents` setean un flag dirty;
`EditorApplication` lo consume post-frame y persiste a
`editor_state.json`.
**Razones:** UX directa, sin confirmaciones extra (la accion es
trivialmente reversible — cualquier "Abrir Proyecto..." vuelve a
agregar el path al tope).
**Alternativas consideradas:** dialogo de confirmacion al borrar
("Estas seguro?"). Para una operacion reversible es ruido.

## 2026-04-26: Shadow mapping con sampler2DShadow + PCF 3x3 hardware (Hito 16)

**Contexto:** primera implementacion de sombras dinamicas. Decisiones
que se tomaron para no resolver el problema de "sombras realistas" pero
si tener una base que funcione y se pueda iterar.
**Decision:** sampler hardware-PCF con `GL_COMPARE_REF_TO_TEXTURE` +
`GL_LEQUAL`, 1 cascada con bounding sphere fijo del mapa,
`GL_DEPTH_COMPONENT24` 2048x2048, front-face culling durante el shadow
pass para reducir peter-panning, border depth=1 para que muestras
fuera del frustum cuenten como "iluminadas". El loop del shadow pass
filtra solo entidades con `MeshRendererComponent`.
**Razones:**
- `sampler2DShadow` da PCF gratis (4 taps por `texture()`) — con un
  PCF 3x3 del shader son 36 muestras efectivas por pixel.
- 1 cascada con bounding sphere alcanza para los mapas 8x8 actuales;
  CSM se posterga hasta que la resolucion se note pixelada.
- Front-face culling se eligio sobre slope-scale bias porque es mas
  simple y no requiere tunear el bias por escena. Limitacion conocida:
  no funciona en geometria abierta (planos sueltos), pero ningun
  mesh actual la tiene.
- Border depth=1 evita el clasico bug de sombras "infinitas" cuando
  la geometria se extiende fuera del frustum de la luz.
**Alternativas consideradas:** VSM/PCSS (muy temprano para tunear),
deferred shadows (atado al pipeline forward actual), sombras de point
lights con cubemap (postergado a hitos posteriores).

## 2026-04-26: `castShadows` opcional en .moodmap sin bump de version

**Contexto:** agregar `castShadows: bool` a `LightComponent` requeria
decidir si bumpeabamos el formato (v6 -> v7) o lo tratabamos como
campo opcional con default false.
**Decision:** campo opcional. Solo se serializa si `castShadows=true`,
y al cargar el default es false. `k_MoodmapFormatVersion` se queda en
6.
**Razones:** archivos viejos (sin `cast_shadows`) cargan como antes
sin warnings de version. El bump solo se hace cuando el formato
introduce un campo no-opcional o cambia semantica.
**Alternativas consideradas:** bump de version con migracion implicita.
Mas ruidoso, sin beneficio funcional aca.

## 2026-04-26: `ShadowMath.h` separado de `ShadowPass` para testabilidad

**Contexto:** las matrices del shadow pass (lightView, lightProj,
producto lightSpace) son matematica pura GLM pero estaban acopladas
al `ShadowPass.cpp` que tira `glad/gl.h` y abre el FBO.
**Decision:** extraer a `engine/render/ShadowMath.h` un helper
header-only con `computeShadowMatrices(lightDir, sceneCenter,
sceneRadius) -> ShadowMatrices`. `ShadowPass` lo consume.
**Razones:**
- Permite testear las matrices con doctest sin GL ni mock (5 casos en
  `test_shadow_proj.cpp`: centro NDC, sphere fits, fallback de
  zero-dir, normalizacion idempotente).
- En hitos posteriores el helper se va a reusar para visualizar el
  frustum de la luz como debug overlay.
**Alternativas consideradas:** dejar todo dentro de `ShadowPass.cpp`
y testear via integration test con un FBO real. Costo en tiempo y
complejidad mucho mayor para el mismo coverage.

## 2026-04-26: Ambient global bajado a 0.08 al activar shadow mapping

**Contexto:** con `k_defaultAmbient=0.18` (Hito 11) las caras "en
sombra" del shading direccional ya quedaban a ~0.18 del color, asi
que cualquier sombra adicional del shadow map se notaba apenas. El
dev reporto "no se ven sombras" antes de identificar la causa.
**Decision:** `k_defaultAmbient = 0.08f` en `LightSystem.cpp`.
**Razones:** con shadow mapping las sombras tienen que oscurecer
visiblemente las caras opuestas a la luz; un ambient bajo hace
contraste. Aun queda iluminacion residual para no perder legibilidad
de texturas en sombra completa.
**Trade-off:** escenas heredadas que dependian del ambient alto se
van a ver mas oscuras. Mitigacion: cuando aparezca PBR + IBL (Hito
17) el "ambient" deja de ser un escalar global y pasa a ser un
sample del cubemap irradiance, asi que esto se vuelve obsoleto.

## 2026-04-26: Highlight cyan del tile solo durante drag de asset

**Contexto:** en Hitos 5-10 el cubo cyan del tile bajo el cursor se
mostraba siempre que el cursor estuviera sobre la imagen del
viewport. UX ruidosa: al mover la camara o seleccionar entidades, el
cubo seguia al mouse y desviaba la atencion.
**Decision:** condicionar el cubo a `ViewportPanel::assetDragActive()`,
que es true solo cuando hay un payload ImGui de tipo
`MOOD_TEXTURE_ASSET`, `MOOD_MESH_ASSET` o `MOOD_PREFAB_ASSET` activo
en el frame.
**Razones:** el cubo originalmente fue affordance del drag&drop;
mostrarlo sin drag perdio su semantica. La seleccion ya tiene su
propio outline OBB (decision siguiente).
**Alternativas consideradas:** togglearlo con una hotkey (F5).
Costo: el dev tiene que recordarlo. Acoplarlo al drag es invisible.

## 2026-04-26: Iconos de luces estilo Blender (Point: anillo + dots, Sun: rayos)

**Contexto:** los iconos de Hito 13 eran un circulo amarillo plano con
una linea vertical para distinguir Directional. Funcional pero
indiscernible a primera vista.
**Decision:** redibujar en el overlay 2D con primitivas de ImGui:
- Point: anillo grueso + 8 dots en circulo + core central (estilo
  "atomo" de Blender).
- Sun: core lleno + 8 rayos cortos a 22.5 deg offset + linea de
  direccion proyectada desde la posicion siguiendo `lc.direction`.
**Razones:** alineamiento con la convencion del usuario (viene de
Blender). La linea de direccion del Sun es informativa: muestra
adonde apunta sin tener que abrir el Inspector.
**Alternativas consideradas:** texturas de iconos (PNG sprites). Mas
flexible pero require asset pipeline + escalado por DPI; primitivas
2D son resolution-independent.

## 2026-04-26: Tecla `.` "frame selected" estilo Blender

**Contexto:** ergonomia comun de DCCs (Blender, Maya, etc.). Sin
esto, alejarse de una entidad para hacer otra cosa requiere navegar
manualmente con orbit + pan + zoom para volver.
**Decision:** `EditorCamera::focusOn(worldPos, objectRadius)` que
re-centra `m_target` y ajusta `m_radius` segun half-FOV +
`objectRadius * 1.6` para dejar margen visual. Mantiene yaw/pitch
para no desorientar al usuario. Hotkey `.` (ImGuiKey_Period) en el
overlay del viewport.
**Razones:**
- 1.6x del minimo geometrico evita que el objeto ocupe toda la
  pantalla.
- Mantener yaw/pitch matchea expectativa de Blender ("centrar la
  vista actual en X", no "resetear la vista").
- `objectRadius = length(scale) * 0.6` aproxima el bounding sphere
  de un cubo unitario (sqrt(3)/2 ~= 0.866) con margen.
**Alternativas consideradas:** AABB exacto del MeshAsset. Postergado
hasta que el helper exista (hoy MeshAsset no expone bounds).

## 2026-04-26: Outline OBB naranja para entidad seleccionada con mesh

**Contexto:** la seleccion en hitos previos solo se mostraba con un
halo cyan en la posicion proyectada de la entidad — fino para
luces/audios sin mesh, pero invisible cuando hay un mesh grande
detras del halo. El dev pidio explicitamente "un reborde para verlo
mejor en el mundo".
**Decision:** dibujar las 12 aristas del cubo unitario [-0.5, 0.5]^3
transformadas por `Transform::worldMatrix()`, en color naranja
Blender (`(1.0, 0.55, 0.05)`), via el `OpenGLDebugRenderer` ya
existente. Solo entidades con `MeshRendererComponent` (las
Light/Audio mantienen el halo 2D porque no tienen mesh).
**Razones:**
- OBB sigue rotacion + escala (no AABB axis-aligned, que se veria
  desalineado con la geometria al rotar).
- Reusa el `OpenGLDebugRenderer` (lineas GL_LINES) sin agregar nuevo
  pipeline.
- Cubo unitario asume que el mesh ocupa ese rango en local space —
  cierto para primitivos y para `pyramid.obj`. Limitacion documentada.
**Trade-offs:**
- Sufre depth test contra la escena: si la entidad esta detras de un
  muro, parte del outline no se ve. Trade-off aceptado por
  legibilidad espacial; siempre-encima se sentiria "flotante".
**Alternativas consideradas:** stencil-based outline (silueta exacta
del mesh). Mas elegante pero require pase extra + soporte de stencil
en el FBO HDR; postergado hasta que aparezca demanda real.

## 2026-04-27: PBR metallic-roughness con IBL split-sum (Hito 17)

**Contexto:** primera implementacion de PBR. Reemplaza al Blinn-Phong
del Hito 11. Decisiones que cierran el espacio de diseno entre la
formula del shader y la pipeline de assets.

**Decisiones tomadas:**

- **Cook-Torrance estandar (GGX + Smith + Schlick).** No Disney
  Principled, no Frostbite, no clearcoat/sheen. Es la formula que
  todos los engines AAA usan como baseline; cualquier extension
  futura (anisotropia, transmission) se agrega sin romper el path
  base.

- **Workflow metallic-roughness sobre specular-glossiness.** Mas
  intuitivo (menos artist-confusion: "metal o no?" es booleano,
  roughness es una escala lineal). Es el estandar de glTF 2.0 y
  Substance — alineamos con el ecosistema.

- **Convencion glTF para MR packed:** R=AO, G=roughness, B=metallic
  en una sola textura. Ahorra samples + memoria sin perder fidelidad
  perceptual. Si aparece un input con encoding distinto, el loader
  agrega un campo `mr_encoding` al `.material`.

- **`MaterialAsset` como JSON `.material` separado del `.moodmap`,
  no inline.** Reusable entre escenas, auditable diff-friendly,
  compatible con futuro editor de grafo (Hito 23). Costo: un
  archivo extra por material. Aceptable (los materials se reusan).

- **Wrappers `__tex#<id>` para back-compat de `.moodmap` v6.** Cada
  textura referenciada en formato viejo se envuelve en un material
  auto-generado al cargar. Persistir el wrapper como `texture_path`
  (no como path sintetico) hace que el archivo siga siendo legible
  por v7+ sin requerir un `.material` explicito en disco.

- **IBL split-sum de Karis (Real Shading in UE4, 2013).** Standard
  de la industria desde 2013, separable en dos terminos
  precomputables: prefilter cubemap por roughness + BRDF LUT
  universal. Permite IBL especular convincente con mips en lugar
  de raytracing.

- **Pre-bake offline en Python.** Scripts `tools/bake_ibl.py` y
  `tools/bake_brdf_lut.py` con numpy. Lento (~94s), corre solo si
  cambia el cubemap fuente. Alternativa real-time en C++ con FBO
  + 6 caras × N mips queda como pendiente futuro (mas complejo de
  implementar, beneficio marginal mientras el dev no cambie de
  skybox seguido).

- **Cubemaps LDR PNG, no HDR.** Limitacion conocida: los
  highlights especulares no tienen rango dinamico. Esta decision
  es pragmatica (Pillow no escribe `.hdr` Radiance facil) y se
  revisa cuando aparezca un skybox que lo necesite.

**Razones del split-sum vs alternativas:**
- Real-time IBL via raytracing (RTX): hardware-only.
- IBL via texture3D del environment: caro en memoria.
- Importance sampling per-frag en shader: caro en samples.
Split-sum es el sweet spot.

## 2026-04-27: Cubemaps de color con `GL_SRGB8_ALPHA8` (Hito 17 fix doble-gamma)

**Contexto:** al activar IBL la escena se veia muy clara, casi
saturada. Causa: los cubemaps PNG (skybox + IBL) son sRGB-encoded en
disco; cargados como `GL_RGBA8` GL los lee como linear, suma colores
inflados al ambient, y el post-process aplica `pow(1/2.2)` al final
duplicando el gamma encode.

**Decision:** los cubemaps de color (skybox + irradiance + prefilter)
se cargan con internal format `GL_SRGB8_ALPHA8`. GL hace el decode
automatico al samplear (`pow(2.2)`), el shader trabaja en linear, y
el post-process hace UN solo gamma encode al final. Resultado:
colores fisicamente correctos.

**Excepcion:** la BRDF LUT 2D NO es sRGB (es una tabla numerica de
scale/bias del split-sum). Se carga como `GL_RGBA8` linear.

**Razones:** es el bug clasico de doble-gamma. La unica forma
correcta de manejar texturas-color es declararlas sRGB en GPU; el
hardware ya tiene fast-path para el decode.

**Trade-off:** el cubemap se ve un ~50% mas oscuro que antes en
el viewport, pero ese era el bug — ahora la escena se ve correcta
relativa al directional light. Sin esto el PBR diffuse IBL salia
inflado y los metales reflejaban un cielo blanqueado.

## 2026-04-27: `MeshRenderer.materials` migra a `vector<MaterialAssetId>` con bump v6 -> v7

**Contexto:** hasta el Hito 16 el slot por submesh era una
`TextureAssetId` (solo albedo). El PBR necesita mas: albedo +
metallic + roughness + AO + normal. Tres opciones:
1. Embed los floats en el componente (struct anidada).
2. Mantener vector<TextureAssetId> + agregar arrays paralelos
   para metallic/roughness/etc.
3. Indireccion a un MaterialAsset por id.

**Decision:** opcion 3. `vector<MaterialAssetId>`. El
`MaterialAsset` vive en el AssetManager (single source of truth),
multiples entidades pueden compartir el mismo material, y el
serializer persiste un solo path por slot.

**Razones:**
- Compartir materiales entre entidades es clave (un material "oro
  pulido" puede aplicarse a 50 props sin duplicar datos).
- Mutar el material desde el Inspector se propaga
  automaticamente a todas las instancias (deseado en este flow).
- El serializer queda mas simple: 1 string por slot, no 5 fields.
- Editor de grafo de materiales (Hito 23) opera sobre
  `MaterialAsset`, no sobre componentes.

**Trade-off:** dos MeshRenderers no pueden tener overrides
distintos del MISMO material sin clonar el material primero. No
es un caso comun en la pipeline actual; cuando aparezca,
agregamos un patron tipo "material instance" (Unreal-style) sobre
el catalogo existente.

**Bump de version:** v6 → v7 con upgrader que detecta paths
terminados en `.material` (v7) vs paths de imagen (v6) y los
trata correctamente. Archivos viejos cargan sin warning.

## 2026-04-27: "Nuevo Proyecto" crea su propia carpeta (Unity/Godot)

**Contexto:** el handler `handleNewProject` usaba el `.moodproj`
como pivote y creaba `maps/` + `textures/` en su parent dir.
Cuando el dev elegia `Escritorio/test.moodproj`, el Escritorio
quedaba con carpetas sueltas — feo y rompe el patron mental "un
proyecto vive en su carpeta".

**Decision:** si el directorio padre del path elegido NO se llama
igual que el proyecto, crear `<parent>/<name>/` y poner el
`.moodproj` + subcarpetas adentro. Si YA esta dentro de una
carpeta con el mismo nombre (ej. el dev ya creo la carpeta y
guarda `<dir>/foo/foo.moodproj`), no se agrega un nivel extra
(DWIM: respeta la intencion explicita).

**Razones:** convencion Unity/Godot/UE. Cualquier dev que viene
de esos engines lo espera asi. El no-anidar-de-mas evita ambiguedad.

**Alternativas consideradas:**
- Dialog separado para "carpeta destino" + "nombre del proyecto":
  mas formal pero rompe el flujo de un solo `pfd::save_file`.
- Forzar siempre subcarpeta: incomodo para devs que ya armaron
  estructura por su cuenta.

## 2026-04-27: Sphere primitive en `AssetManager` con slot reservado

**Contexto:** el showcase de PBR del Hito 17 necesita esferas
porque el shading no se ve igual en cubos. Hasta el Hito 16
el motor solo tenia `createCubeMesh`.

**Decision:** agregar `createSphereMesh(segments=32)` en
`PrimitiveMeshes` + un `MeshAssetId primitiveSphereId()` en
`AssetManager` reservado en el ctor (slot logico
`__primitive_sphere`, similar al `__missing_cube` del slot 0).

**Razones:**
- Sin esto, el showcase requiere un `.obj` esfera importado
  (assimp), agregando complejidad de pipeline a un demo.
- Generar la mesh en runtime es trivial (UV sphere ~6 lineas).
- Cualquier futuro showcase / debug visual la va a usar.

**Alternativas consideradas:**
- Importar `assets/meshes/sphere.obj` desde algun lado (Blender
  default sphere). Costo: un archivo binario en el repo +
  dependencia de assimp para algo que se genera analiticamente.

## 2026-04-27: Forward+ tile-based culling sobre Deferred (Hito 18)

**Contexto:** subir el cap de point lights de 8 (uniform array) a
256+ requiere cambiar la arquitectura. Las dos opciones estandar son
Forward+ y Deferred shading.

**Decision:** Forward+ con tile size 16x16 px, light grid CPU-side,
SSBOs para luces + tiles + indices.

**Razones:**
- Compatibilidad con el HDR + post-process actual del Hito 15: el
  scene FB sigue siendo un solo color attachment RGBA16F. Deferred
  requiere G-buffer multi-target (albedo + normal + material +
  depth) — refactor grande del FB y el shader pbr.
- MSAA "trivial" en Forward+. En Deferred es complicado porque
  los samples del G-buffer requieren resolve + lighting per-sample.
- Reusa el shader pbr.frag intacto: solo cambia el loop de point
  lights, no el modelo de iluminacion.
- El target de luces simultaneas del proyecto (~30-100) entra
  comodamente en Forward+ sin necesidad del power-of-2 culling de
  Deferred.

**Trade-off aceptado:** Deferred escala mejor a 1000+ luces densas
(tipico en escenas urbanas nocturnas con farolas en cada esquina).
No es el caso del proyecto. Si aparece, Hito 18.5 hace el switch.

**Alternativas consideradas:**
- Clustered shading (Forward++): hito 18.5 si los Z-bins se
  vuelven necesarios.
- Texture-based light grid (en vez de SSBO): mas barato en GPUs
  viejas pero con limites de samplers; SSBO es lo moderno y el
  motor pide GL 4.5.

## 2026-04-27: Light grid CPU-side via 8-corner AABB del bounding sphere

**Contexto:** el LightGrid necesita decidir, por cada point light,
a que tiles afecta. Las opciones son:
1. AABB del bounding sphere world-space proyectado a screen-space.
2. Sphere proyectada como elipse (matematica de Mara/Lengyel).
3. Frustum-vs-sphere por tile (test exacto pero N_tiles * N_luces
   testeos).

**Decision:** opcion 1. Para cada luz, tomamos las 8 esquinas del
AABB world-space (`center +/- radius` en cada eje), las proyectamos
a NDC, sacamos min/max XY, clampeamos al viewport, y marcamos los
tiles del rectangulo resultante.

**Razones:**
- Simple de implementar y de testear (helper puro
  `projectSphereToTileRange`).
- Conservativo: nunca mete una luz en menos tiles de los que
  realmente la afectan. False positives (asignar la luz a tiles
  donde no llega) son aceptables — el shader hace `if (dist >
  radius) continue` y la luz se descarta a nivel fragment.
- El caso "sphere cruza el plano near de la camara" se maneja
  marcando todo el viewport (alguna parte de la sphere puede
  iluminar cualquier pixel, no se puede acotar barato).

**Trade-off aceptado:** la elipse exacta daria menos false positives
~30-50% segun benchmarks publicos. Para el target de 64-256 luces
del motor el costo extra del AABB no se mide; cuando aparezca,
Hito 18.5 mejora el algoritmo.

## 2026-04-27: SSBO std430 layout con padding C++ explicito

**Contexto:** las point lights migran de uniform array a SSBO. El
layout std430 de GLSL tiene reglas de alineamiento (vec3 alineado a
16, vec4 a 16, struct a max(miembros)). Si el C++ no replica el
layout exacto, los miembros se desalinean y el shader lee basura.

**Decision:** struct C++ `PointLightStd430` con padding floats
explicitos para totalizar 48 bytes (3 vec4 alignados):
```
vec3 position; float pad0;       // 16B
vec3 color;    float intensity;  // 16B
float radius;  float pad1, pad2, pad3;  // 16B
```
`static_assert(sizeof(PointLightStd430) == 48)` garantiza el match
en compile-time. El shader replica el mismo layout con padding
floats.

**Razones:**
- std430 NO usa el padding implicito de std140 (que aliña vec3 a
  vec4 silenciosamente). Sin padding explicito en C++, el segundo
  PointLight del array empieza 4 bytes antes de lo que GLSL espera.
- `static_assert` lo detecta antes de runtime.
- La duplicacion del padding en C++ y GLSL es minima (12 bytes de
  pads); los costos en complejidad son menores al beneficio de un
  layout deterministico.

**Alternativas consideradas:**
- Pack manual a `std::vector<float>` y cargar sin struct intermedio.
  Mas error-prone (offsets numericos en el codigo) y peor
  legibilidad.
- `layout(std140)` que tiene reglas mas estrictas y el C++ default
  con vec3 + 1 float lo respetaria sin pads explicitos. Costo:
  std140 padea TODO al multiplo de vec4, los arrays de scalars
  pasan a 16B/elemento — gran gasto de memoria en `uLightIndices[]`.

## 2026-04-27: `iblIntensity` en EnvironmentComponent como slider

**Contexto:** durante el smoke test del Hito 18 el dev reporto que
la escena se veia "muy brillante" sin point lights. La causa
probable: el cubemap del skybox aporta mucho diffuse IBL que
ahoga el contraste de las luces directas. La solucion no es bajar
el cubemap (es el aspecto deseado del cielo) sino dejar que el
material o environment local diga "yo no quiero tanto IBL aqui".

**Decision:** agregar `EnvironmentComponent.iblIntensity` (default
1.0, range [0, 2]) como multiplicador del termino IBL en el shader.
Slider en el Inspector + persistido en `.moodmap` solo si != 1.0
(no bumpea ni ensucia archivos viejos).

**Razones:**
- Da control sin romper nada: el dev decide si quiere mas o menos
  ambient IBL por escena.
- El range [0, 2] permite tanto bajarlo (escenas indoor con luces
  artificiales dominantes) como subirlo (open-world brillante).
- Persistido como campo opcional: archivos viejos siguen cargando
  con default 1.0 sin warnings.

**Alternativas consideradas:**
- Bajar el IBL globalmente a 0.5 por default. Decision de UX
  arbitraria que rompe la apariencia "exterior" para todos.
- Replicar el slider en cada `MaterialAsset` (override por
  material). Util pero overkill — el ambient es propiedad del
  environment, no del material.
