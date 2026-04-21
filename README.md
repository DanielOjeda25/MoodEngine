# MoodEngine

Motor gráfico 3D propio con editor visual integrado, escrito en C++17. Inspirado en la arquitectura de Source Engine y la filosofía de desarrollo de id Software.

**Estado actual:** Hito 1 — Shell del editor.

## Requisitos

- **Windows 10/11**
- **Visual Studio 2022** con herramientas de C++ (MSVC v143+)
- **CMake 3.24 o superior** (incluido con Visual Studio)
- **Git**
- Conexión a internet en la primera configuración (CPM.cmake descarga dependencias)

## Dependencias

Todas las dependencias se descargan automáticamente al configurar CMake. No hay que instalar nada manualmente.

- SDL2 (ventana, input, timers)
- Dear ImGui rama `docking` (UI del editor)
- spdlog (logging)
- GLM (matemática 3D)

## Cómo clonar, compilar y ejecutar

```bash
# 1. Clonar
git clone https://github.com/DanielOjeda25/MoodEngine.git
cd MoodEngine

# 2. Configurar (descarga dependencias la primera vez)
cmake --preset windows-msvc-debug

# 3. Compilar
cmake --build build/debug --config Debug

# 4. Ejecutar
./build/debug/Debug/MoodEditor.exe
```

Para Release:

```bash
cmake --preset windows-msvc-release
cmake --build build/release --config Release
./build/release/Release/MoodEditor.exe
```

## Estructura del proyecto

- `src/` — código fuente del motor y editor
- `cmake/` — helpers de CMake y bootstrap de CPM
- `external/` — código externo no gestionado por CPM (glad, stb)
- `docs/` — documentación del proyecto
- `tests/` — tests unitarios (entra en Hito 3-4)
- `shaders/` — shaders built-in del motor (entra en Hito 2-3)
- `assets/` — assets de ejemplo del editor

Ver `docs/ARCHITECTURE.md` para detalle arquitectónico y `docs/HITOS.md` para el roadmap.

## Licencia

MIT. Ver archivo `LICENSE`.
