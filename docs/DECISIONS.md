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
