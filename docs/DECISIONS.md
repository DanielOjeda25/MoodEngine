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
