# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**Hito 3 cerrado, mergeado a `main` y publicado en origin.**
Tag: `v0.3.0-hito3`.
Verificado automático (`Textura cargada: assets/textures/grid.png (256x256, 4 canales)`, `OpenGLRenderer listo (depth test activo)`, tests doctest 10/10 pasando, shutdown exit 0) + verificado por el dev a ojo (cubo rotando texturizado, WASD en Play Mode, toggle Play/Stop + Esc para salir).

**Próximo paso:** Hito 4 — mundo grid + colisiones AABB. Plan detallado en `docs/PLAN_HITO4.md`.

### Lo que ya está hecho

**Hito 1** — Shell del editor completo (tag `v0.1.0-hito1`):
- Estructura completa de carpetas del repo (sección 6 del doc técnico).
- Build: CMake 3.24+ con CPM.cmake, CMakePresets, MSVC.
- `src/core/`: logging (spdlog), Types, Assert, Time (Timer + FpsCounter).
- `src/platform/Window`: wrapper RAII sobre SDL2 con contexto OpenGL 4.5 Core.
- `src/editor/`: EditorApplication, EditorUI, MenuBar, StatusBar, Dockspace + 3 paneles.

**Hito 2** — Primer triángulo con OpenGL (tag `v0.2.0-hito2`):
- `external/glad/`: GLAD v2 para OpenGL 4.5 Core generado con `glad2` Python, files committed, target estático `glad`.
- `src/engine/render/`: RHI abstracta (`IRenderer`, `IShader`, `IMesh`, `IFramebuffer`, `RendererTypes.h`).
- `src/engine/render/opengl/`: backend OpenGL (OpenGLRenderer, OpenGLShader con cache de uniforms, OpenGLMesh, OpenGLFramebuffer con color RGBA8 + depth RB).
- `shaders/default.{vert,frag}`: GLSL 4.5 Core. Copiado post-build junto al exe.
- `EditorApplication` monta renderer + framebuffer + shader + mesh; renderiza offscreen cada frame antes de la UI.
- `ViewportPanel` muestra el color attachment con `ImGui::Image` y UV invertido vertical.
- `Dockspace.cpp` arma layout por defecto con `DockBuilder` cuando `imgui.ini` no tiene nodos split.

**Hito 3** — Cubo texturizado con cámara (tag `v0.3.0-hito3`):
- `external/stb/`: `stb_image.h` (2.30) + `stb_image_write.h` (1.16) commiteados; target INTERFACE `stb`.
- `src/engine/assets/stb_impl.cpp`: única TU con `STB_IMAGE_IMPLEMENTATION`.
- `src/engine/assets/PrimitiveMeshes.{h,cpp}`: helper `createCubeMesh()` (36 vértices, pos+color+uv).
- `src/engine/render/ITexture.h` + `OpenGLTexture.{h,cpp}`: carga con stb_image, genera mipmaps, flip vertical al cargar.
- `src/engine/scene/EditorCamera.{h,cpp}`: orbital (yaw/pitch/radio + target). Input: right-drag + wheel.
- `src/engine/scene/FpsCamera.{h,cpp}`: libre (position + yaw/pitch). Input: WASD + mouse relativo.
- `src/editor/EditorMode.h`: enum Editor/Play + toggle request via `EditorUI`.
- `MenuBar` tiene ahora un botón Play/Stop verde/rojo empujado a la derecha.
- `StatusBar::draw(EditorMode)` muestra "Editor Mode" o "Play Mode" dinámicamente.
- Shaders extendidos: `uModel/uView/uProjection` + `sampler2D uTexture`, atributos pos/color/uv.
- Depth test activo en `OpenGLRenderer::init()`.
- `assets/textures/grid.png` generada con `tools/gen_grid_texture.py` (Python + Pillow, 256x256 RGBA).
- `tests/` con doctest: 10 casos, 37 asserciones (matemática GLM + cámaras).

### Dependencias que baja CPM al configurar

- SDL2 `release-2.30.2`
- GLM `1.0.1`
- spdlog `1.14.1`
- ImGui rama `docking` (compilado como target interno `imgui` con backends SDL2 + OpenGL3)
- doctest `2.4.11` (sólo para el target `mood_tests`)

### Herramientas externas necesarias (solo para regenerar, no para build)

- `glad2` (Python) — para regenerar GLAD si cambia la versión de GL. Ver `external/glad/README.md`.
- `Pillow` (Python) — para regenerar `assets/textures/grid.png` desde `tools/gen_grid_texture.py`. No hace falta si el PNG ya está commiteado.

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real instalado en la máquina del dev

- **Visual Studio 2022 Community** → tiene MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Este es el que usamos.**
- **Visual Studio 2026 Community** → instalado **sin** workload de C++. No tiene compilador. Ignorar hasta que el dev lo complete o desinstale.

La ruta clave es:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

### Cargar entorno MSVC desde PowerShell (el comando que funciona)

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde un **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente sin esto.

### Versiones verificadas

- `cl` → `Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35226 for x64`
- `cmake --version` → `3.31.6-msvc6`
- `git --version` → `2.49.0.windows.1`

### Generador CMake correcto

`Visual Studio 17 2022` (ya está en `CMakePresets.json`, no tocar).

---

## 3. Comandos que ya corrieron exitosamente

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug
```

Resultado: `build/debug/Debug/MoodEditor.exe` (3.9 MB) + `SDL2d.dll` (4.3 MB) copiada automáticamente al mismo directorio.

Para ejecutar:

```
.\build\debug\Debug\MoodEditor.exe
```

---

## 4. Qué tiene que hacer el próximo agente

### Tarea inmediata: implementar el Hito 4

El Hito 3 está cerrado (tag `v0.3.0-hito3` en origin). El foco ahora es el **Hito 4 — mundo grid + colisiones AABB**.

El plan desglosado por tareas está en `docs/PLAN_HITO4.md`.

**Punto de arranque concreto:** crear `src/engine/world/GridMap.{h,cpp}` con una grilla 2D de tiles (enum TileType). El renderer itera la grilla y dibuja un cubo texturizado por celda ocupada. Después, `src/core/math/AABB.{h,cpp}` + `src/systems/PhysicsSystem.cpp` con raycasts tile-a-tile para colisión jugador-vs-paredes.

### Flujo recomendado en esta sesión

1. Leer `docs/PLAN_HITO4.md`.
2. Trabajar tarea por tarea, marcando como completadas en el plan al terminar cada una.
3. Después de cada bloque grande (grid data, grid render, AABB math, collision, debug draw), compilar y confirmar.
4. Actualizar `docs/DECISIONS.md` si aparece alguna decisión no prevista.
5. Al final: commits atómicos en español, merge a main, tag `v0.4.0-hito4`, actualizar este documento y `docs/HITOS.md`, crear `docs/PLAN_HITO5.md`.

---

## 5. Gotchas conocidos — cosas que ya se aprendieron por las malas

1. **VS 2026 Community se instaló sin el workload de C++.** El `Developer Command Prompt for VS 2026` abre pero `cl` y `cmake` no existen. No depender de VS 2026 hasta que el dev agregue el workload o lo desinstale. Usar siempre VS 2022.
2. **El `Developer Command Prompt` normal de Windows** (sin MSVC) no tiene `cl` en PATH. Si un comando falla con "cl no se reconoce", revisar que el prompt diga "Visual Studio 2022" en el banner.
3. **SDL2 en debug se llama `SDL2d.dll`**, no `SDL2.dll`. El `add_custom_command` POST_BUILD del CMakeLists copia la variante correcta según `$<TARGET_FILE:SDL2::SDL2>`.
4. ~~**GLAD no está incluido en Hito 1.**~~ Resuelto en Hito 2: `EditorApplication.cpp` usa `<glad/gl.h>` y llama `gladLoaderLoadGL()` tras crear el contexto. GLAD y el loader interno de ImGui conviven sin conflictos porque glad2 prefija sus símbolos con `glad_`.
8. **GLAD v2 usa naming distinto de v1.** Header: `<glad/gl.h>` (no `<glad/glad.h>`). Source: `gl.c` (no `glad.c`). Loader: `gladLoaderLoadGL()` / `gladLoaderUnloadGL()`.
9. **Dos máquinas de desarrollo.** Notebook con Intel Iris Xe (GL 4.5 OK, menos performance) y desktop con AMD Ryzen 5 5600G + NVIDIA GTX 1660. En el desktop, forzar NVIDIA para `MoodEditor.exe` desde el Panel de Control NVIDIA (sino Windows puede elegir la iGPU AMD). `imgui.ini` y `build/` NO viajan por git: cada máquina genera lo suyo.
10. **Shaders se buscan con path relativo `shaders/default.vert`.** Funciona desde la raíz del repo (VS_DEBUGGER_WORKING_DIRECTORY) y desde el directorio del exe (post-build `copy_directory shaders/`). Si agregás shaders nuevos, la copia es automática.
5. **El primer `cmake --preset` tarda ~5 minutos** porque baja y compila subdeps de SDL2 + ImGui + spdlog + GLM. Ese tiempo solo ocurre la primera vez; después el build incremental es segundos.
6. **spdlog busca pthread en Windows** y sale un warning de que no lo encuentra. Es esperado y benigno en Windows.
7. **GLM tira warnings de `cmake_minimum_required` deprecated** porque su CMakeLists usa sintaxis vieja. Ignorable; no es nuestro código.

---

## 6. Recordatorios de filosofía (sección 9 del doc técnico)

- Ship something: no romper el build entre commits.
- No implementar hitos futuros "por adelantar trabajo".
- No añadir dependencias que no estén en la sección 4 del doc sin consultar.
- Comentarios en español.
- Convención de commits: `tipo(scope): mensaje` en español.
- Preguntar al dev antes de asumir ante ambigüedades.

---

## 7. Archivos clave que tocar cuando

| Para... | Tocar |
|---|---|
| Añadir una dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags de compilador | `cmake/CompilerWarnings.cmake` |
| Añadir un panel al editor | `src/editor/panels/*` + `EditorUI.(h|cpp)` |
| Registrar una decisión arquitectónica | `docs/DECISIONS.md` |
| Marcar progreso de hito | `docs/HITOS.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Al arrancar una sesión nueva

1. Leer este archivo entero.
2. Leer `docs/PLAN_HITO<N>.md` del hito en curso para ver qué tareas quedan.
3. Leer `MOODENGINE_CONTEXTO_TECNICO.md` si es la primera vez.
4. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5` para ver tags y cambios recientes.
5. Preguntar al desarrollador: "¿seguimos con el Hito en curso o pasó algo nuevo?"
6. Actuar según la respuesta.
