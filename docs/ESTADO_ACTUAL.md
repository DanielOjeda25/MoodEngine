# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**Hito 1 implementado y compilando sin errores en MSVC.** Falta solo verificación visual por parte del desarrollador y, si pasa, commit + tag `v0.1.0-hito1`.

### Lo que ya está hecho

- Estructura completa de carpetas del repo (sección 6 del doc técnico).
- Raíz: `.gitignore`, `.gitattributes`, `.clang-format`, `LICENSE`, `README.md`, `CMakeLists.txt`, `CMakePresets.json`.
- `cmake/CPM.cmake` (bootstrap autodescargable 0.40.2) y `cmake/CompilerWarnings.cmake`.
- `docs/`: ARCHITECTURE, DECISIONS, HITOS, CODING_STYLE, CONTRIBUTING, SETUP_WINDOWS, y este mismo documento.
- `src/core/`: Types.h, Log (.h/.cpp sobre spdlog), Assert.h, Time (.h/.cpp con Timer + FpsCounter).
- `src/platform/Window.(h|cpp)`: wrapper RAII sobre `SDL_Window` con contexto OpenGL 4.5 Core.
- `src/editor/`: EditorApplication, EditorUI, MenuBar, StatusBar, Dockspace + 3 paneles (Viewport, Inspector, AssetBrowser) heredando de `IPanel`.
- `src/main.cpp`: entry point con `SDL_MAIN_HANDLED` y try/catch.
- Stubs (`.gitkeep`) en `src/engine/`, `src/systems/`, `src/game/`, `tests/`, `shaders/`, `assets/`, `tools/`.

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

### Tarea inmediata: verificar el Hito 1 con el desarrollador

El desarrollador debe ejecutar `MoodEditor.exe` y comprobar los **criterios de aceptación** de la sección 11.2 del doc técnico:

- [ ] Ventana 1280x720, título `"MoodEngine Editor - v0.1.0 (Hito 1)"`.
- [ ] Menú Archivo / Editar / Ver / Ayuda.
  - Archivo > Nuevo / Abrir / Guardar → popup "No implementado".
  - Archivo > Salir → cierra la app.
  - Ayuda > Acerca de → popup con nombre, versión, link al repo.
  - Ver → toggles de visibilidad para los 3 paneles.
- [ ] Tres paneles acoplables visibles: Viewport, Inspector, Asset Browser.
- [ ] Los paneles son arrastrables, acoplables y flotantes.
- [ ] Status bar con FPS (actualizándose), `"Editor Mode"`, mensaje `"Listo"`.
- [ ] X de ventana cierra limpio.
- [ ] Archivo `logs/engine.log` creado con las entradas de arranque (`MoodEngine iniciando...`, `Ventana creada (1280x720)`, `Editor listo`) y la de cierre (`MoodEngine cerrado limpiamente`).

Si algo falla visualmente (panel mal posicionado, FPS en 0, popup no aparece, etc.), pedí al desarrollador un screenshot o descripción precisa. Arreglá en el código, recompilá y vuelva a probar.

### Una vez que el Hito 1 pase todos los criterios

1. Proponer al desarrollador una serie de **commits atómicos en español** siguiendo la convención de la sección 9.8 del doc técnico. Sugerencia de división:
   - `chore(build): configurar CMake con CPM y CMakePresets`
   - `chore(repo): agregar .gitignore, .gitattributes, .clang-format, LICENSE`
   - `docs: documentacion inicial (architecture, decisions, hitos, style)`
   - `feat(core): logging con spdlog, tipos, aserciones y utilidades de tiempo`
   - `feat(platform): wrapper SDL2 con contexto OpenGL 4.5 core`
   - `feat(editor): shell con dockspace, menu bar, status bar y tres paneles`
   - `feat(main): entry point del editor`
2. **Pedir confirmación explícita antes de hacer los commits** (el desarrollador no siempre los autoriza aunque estén listos).
3. Cuando todo esté pusheado, crear el tag: `git tag v0.1.0-hito1 && git push --tags`.
4. Actualizar `docs/HITOS.md` marcando Hito 1 como completado.

### Siguiente hito: empezar Hito 2 — primer triángulo con OpenGL

Objetivo del Hito 2 (ver sección 10 del doc técnico):

1. Generar GLAD para OpenGL 4.5 Core desde https://glad.dav1d.de/ y colocarlo en `external/glad/include/glad/glad.h`, `external/glad/src/glad.c`, `external/glad/include/KHR/khrplatform.h`.
2. Añadir target estático `glad` al `CMakeLists.txt` raíz.
3. Enlazar `glad` en `MoodEditor` **antes** que `OpenGL::GL` y retirar `<SDL_opengl.h>` de `EditorApplication.cpp`.
4. Definir la interfaz RHI abstracta en `src/engine/render/`: `IRenderer`, `IShader`, `IMesh`.
5. Implementarlos en `src/engine/render/opengl/`.
6. Crear shaders built-in `shaders/default.vert` y `shaders/default.frag` (triángulo de colores vértice por vértice, sin texturas).
7. Dibujar el triángulo a un **framebuffer offscreen** y mostrarlo como textura dentro del panel Viewport con `ImGui::Image`.
8. Añadir Tracy + doctest a CPM (planeados para Hito 5-6 pero se pueden preparar).

Archivo de punto de arranque: `src/editor/EditorApplication.cpp`, función `EditorApplication::EditorApplication()` — después de crear el contexto, inicializar GLAD con `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)` y loguear `glGetString(GL_VERSION)` para verificar.

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
2. Leer `MOODENGINE_CONTEXTO_TECNICO.md` si es la primera vez.
3. `git status` + `git log --oneline -10` para ver si hay cambios nuevos desde la última handoff.
4. Preguntar al desarrollador: "¿verificaste el Hito 1 o pasó algo nuevo desde el último documento?"
5. Actuar según la respuesta.
