# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**Hito 1 cerrado, mergeado a `main` y publicado en origin.**
Tag: `v0.1.0-hito1` en commit `292533b`.
Verificado automático (logging, startup, shutdown) + verificado por el dev a ojo (UI, menús, paneles, status bar, docking).

**Próximo paso:** Hito 2 — primer triángulo con OpenGL. Plan detallado en `docs/PLAN_HITO2.md`.

### Lo que ya está hecho (Hito 1)

- Estructura completa de carpetas del repo (sección 6 del doc técnico).
- Raíz: `.gitignore`, `.gitattributes`, `.clang-format`, `LICENSE`, `README.md`, `CMakeLists.txt`, `CMakePresets.json`.
- `cmake/CPM.cmake` (bootstrap autodescargable 0.40.2) y `cmake/CompilerWarnings.cmake`.
- `docs/`: ARCHITECTURE, DECISIONS, HITOS, CODING_STYLE, CONTRIBUTING, SETUP_WINDOWS, PLAN_HITO2, y este mismo documento.
- `src/core/`: Types.h, Log (.h/.cpp sobre spdlog), Assert.h, Time (.h/.cpp con Timer + FpsCounter).
- `src/platform/Window.(h|cpp)`: wrapper RAII sobre `SDL_Window` con contexto OpenGL 4.5 Core.
- `src/editor/`: EditorApplication, EditorUI, MenuBar, StatusBar, Dockspace + 3 paneles (Viewport, Inspector, AssetBrowser) heredando de `IPanel`.
- `src/main.cpp`: entry point con `SDL_MAIN_HANDLED` y try/catch.
- Stubs (`.gitkeep`) en `src/engine/`, `src/systems/`, `src/game/`, `tests/`, `shaders/`, `assets/`, `tools/`.

### Pendiente menor detectado en Hito 1 (no bloqueante)

- Solapamiento visual del layout inicial del dockspace. Se ajusta en Hito 2 cuando el Viewport tenga contenido real (framebuffer con el triángulo) y podamos fijar un `DockBuilder` con posiciones explícitas.

### Dependencias que baja CPM al configurar

- SDL2 `release-2.30.2`
- GLM `1.0.1`
- spdlog `1.14.1`
- ImGui rama `docking` (compilado como target interno `imgui` con backends SDL2 + OpenGL3)

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

### Tarea inmediata: implementar el Hito 2

El Hito 1 está cerrado (tag `v0.1.0-hito1` en origin). El foco ahora es el **Hito 2 — primer triángulo con OpenGL**.

El plan desglosado por tareas está en `docs/PLAN_HITO2.md`. Leé ese documento y respetá el orden propuesto (GLAD primero, después RHI, después framebuffer, después wiring al ViewportPanel).

**Punto de arranque concreto:** `external/glad/` — hoy contiene sólo un README. Hay que generar los archivos de GLAD para OpenGL 4.5 Core desde https://glad.dav1d.de/ y ubicarlos en `external/glad/include/glad/glad.h`, `external/glad/src/glad.c`, `external/glad/include/KHR/khrplatform.h`. Después se agrega el target estático `glad` al `CMakeLists.txt` raíz.

**Archivo clave a modificar primero:** `src/editor/EditorApplication.cpp`. Cuando GLAD esté disponible:
- Reemplazar el include `<SDL_opengl.h>` por `<glad/glad.h>`.
- Tras `SDL_GL_CreateContext`, llamar `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)` y abortar si devuelve 0.
- Loguear `glGetString(GL_VERSION)`, `GL_VENDOR`, `GL_RENDERER` para verificación.

### Flujo recomendado en esta sesión

1. Leer `docs/PLAN_HITO2.md` (tareas numeradas).
2. Trabajar tarea por tarea, marcando como completadas en el plan al terminar cada una.
3. Después de cada bloque grande (GLAD + contexto, RHI, framebuffer, integración), compilar y confirmar que sigue funcionando.
4. Actualizar `docs/DECISIONS.md` si aparece alguna decisión no prevista.
5. Al final: commits atómicos en español, merge a main, tag `v0.2.0-hito2`, actualizar este documento y `docs/HITOS.md`.

---

## 5. Gotchas conocidos — cosas que ya se aprendieron por las malas

1. **VS 2026 Community se instaló sin el workload de C++.** El `Developer Command Prompt for VS 2026` abre pero `cl` y `cmake` no existen. No depender de VS 2026 hasta que el dev agregue el workload o lo desinstale. Usar siempre VS 2022.
2. **El `Developer Command Prompt` normal de Windows** (sin MSVC) no tiene `cl` en PATH. Si un comando falla con "cl no se reconoce", revisar que el prompt diga "Visual Studio 2022" en el banner.
3. **SDL2 en debug se llama `SDL2d.dll`**, no `SDL2.dll`. El `add_custom_command` POST_BUILD del CMakeLists copia la variante correcta según `$<TARGET_FILE:SDL2::SDL2>`.
4. **GLAD no está incluido en Hito 1.** Para que ImGui-OpenGL3 funcione sin loader, `EditorApplication.cpp` incluye `<SDL_opengl.h>` y llama `glClearColor`/`glClear`/`glViewport` directamente (esas funciones son GL 1.1 y están en `opengl32.lib` sin loader). Al entrar GLAD en Hito 2, **quitar** ese include para evitar colisión de símbolos.
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
