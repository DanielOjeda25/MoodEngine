# MoodEngine — Documento Técnico de Contexto

> **Propósito de este documento:** dar al agente de código (Claude Code en Antigravity) todo el contexto necesario para implementar el proyecto MoodEngine siguiendo las decisiones arquitectónicas ya tomadas. Lee el documento completo antes de escribir código. Si algo no está claro, pregunta al usuario antes de asumir.
>
> **Estado del proyecto:** Hito 0 completado (repositorio creado). Próximo paso: implementar Hito 1.

---

## ÍNDICE

1. Contexto general del proyecto
2. Contexto del desarrollador y entorno
3. Visión a largo plazo
4. Stack técnico definitivo (decisiones fijas)
5. Arquitectura general del motor
6. Estructura de carpetas del repositorio
7. Convenciones de código y estilo
8. Sistema de build y gestión de dependencias
9. Filosofía de desarrollo
10. Roadmap completo de hitos (~25 hitos)
11. Especificación detallada del Hito 1
12. Qué NO hacer
13. Metodología de trabajo con el agente
14. Glosario técnico
15. Anexos: recursos y referencias

---

## 1. Contexto general del proyecto

**Nombre:** MoodEngine

**Repositorio:** https://github.com/DanielOjeda25/MoodEngine

**Descripción corta:** Motor gráfico 3D propio con editor visual integrado, escrito en C++ moderno, inspirado en la arquitectura de Source Engine y la filosofía de desarrollo de John Carmack / id Software. Objetivo principal: crear juegos FPS, con puertas abiertas a RPG en el futuro.

**Licencia objetivo:** abierta (MIT o similar) cuando esté listo para compartir públicamente. Durante desarrollo inicial, el repositorio puede permanecer privado.

**Estado actual:** repositorio inicializado en GitHub con un `README.md` mínimo. Ninguna implementación existente. El siguiente paso es el Hito 1 descrito en la sección 11.

---

## 2. Contexto del desarrollador y entorno

**Desarrollador principal:** Daniel Ojeda (usuario `DanielOjeda25` en GitHub, alias `MussolTex`).

**Nivel de experiencia:** programador con motivación alta pero sin experiencia previa en desarrollo de motores gráficos. Aprendiendo C++ a la par que construye el motor. Busca entender todos los fundamentos a fondo, no solo copiar código.

**Tiempo de dedicación:** tanto tiempo como sea necesario, sin presión de plazos. El objetivo es calidad y aprendizaje, no velocidad.

**Hardware de desarrollo:**

- CPU: AMD Ryzen 5 5600G (6 núcleos / 12 hilos, arquitectura Zen 3, soporta AVX2 y SSE4.2)
- RAM: 16 GB DDR4
- GPU: NVIDIA GeForce GTX 1660 5 GB (arquitectura Turing sin RT Cores ni Tensor Cores)
- Almacenamiento: asumir SSD

**Sistema operativo de desarrollo:** Windows

**Editor/IDE:** Antigravity (fork de VS Code con agente de código integrado)

**Herramientas ya instaladas:**

- Compilador MSVC (viene con Visual Studio / Antigravity)
- CMake
- Git

**Herramientas que no están instaladas pero pueden instalarse:** Clang/LLVM (no se usa por ahora, MSVC es el compilador principal).

---

## 3. Visión a largo plazo

### Género objetivo primario

**First-Person Shooter** (FPS) similar en ambición a los juegos hechos con Source Engine (Half-Life 2, Portal, Counter-Strike: Source). Esto implica:

- Cámara en primera persona
- Movimiento WASD + ratón estándar de FPS
- Soporte para armas, proyectiles, enemigos
- Mapas con geometría 3D arbitraria, no solo tiles
- Física razonable (no hyperrealista, suficiente para gameplay)
- Iluminación dinámica eventualmente
- Audio posicional 3D

### Género objetivo secundario (no descartado)

**Role-Playing Game** (RPG) tipo Zelda/Skyrim más adelante. Esto implica, cuando llegue el momento:

- Cámara en tercera persona como alternativa
- Sistemas de inventario
- Diálogos
- NPCs con comportamiento complejo
- Misiones / quest system

El motor se diseña para no cerrarse a ningún género, pero las primeras iteraciones priorizan FPS por simplicidad.

### Alcance técnico objetivo

El motor no busca competir con Unreal 5 o Unity. Busca llegar a una calidad visual comparable a **juegos AAA de 2010-2015** (por ejemplo: Half-Life 2, Portal 2, Battlefield 3, BioShock). Esto es alcanzable con OpenGL 4.5 y el hardware del desarrollador.

### Multiplataforma

**Objetivo a largo plazo:** Windows + Linux como mínimo. Mac y móviles no prioritarios.

**Fase inicial:** Windows exclusivamente. El código se escribe preparado para portabilidad (sin APIs Windows-only), pero solo se compila y testea en Windows durante los primeros ~10 hitos.

### Distribución y colaboración

**Objetivo a largo plazo:** abierto, compartible como herramienta para que terceros hagan sus juegos con él. Esto eleva el estándar de:

- Documentación (toda en Markdown dentro del repo).
- Estabilidad de API pública.
- Disciplina de versionado semántico.
- Mensajes de error útiles y comportamiento predecible.

### Lo que NO se desarrolla

Estas características están explícitamente fuera de alcance y no deben ser consideradas en ninguna decisión arquitectónica:

- Networking / multijugador.
- Internacionalización (i18n) de la UI del editor.
- Cursores personalizados del sistema.
- Soporte multi-monitor inteligente.
- Ray tracing en hardware (la GTX 1660 no lo soporta eficientemente).
- Publicación a consolas.

---

## 4. Stack técnico definitivo

Las siguientes decisiones son **fijas** y no deben desviarse sin acuerdo explícito del desarrollador. Cada elección tiene su justificación.

### 4.1 Lenguaje y estándar

**C++17** como estándar base.

Justificación: balance entre features modernas (structured bindings, `std::optional`, `std::variant`, `if constexpr`, `std::filesystem`) y compatibilidad amplia. C++20 se evaluará en el futuro si algún feature específico lo justifica.

Prohibido:

- `using namespace std;` en headers públicos.
- Macros para lógica de negocio (solo para logging, debug o meta).
- Herencia virtual innecesaria en hot paths.

### 4.2 Sistema de build

**CMake 3.24+** como generador de proyectos.

Dependencias gestionadas con **CPM.cmake**, una delgada capa sobre `FetchContent` que simplifica la declaración de paquetes externos. Archivo único `CPM.cmake` incluido en el repo bajo `cmake/`.

Todas las dependencias se bajan automáticamente al ejecutar `cmake -B build`. **Ninguna instalación manual de librerías.** Sin vcpkg, sin Conan, sin apt/choco.

### 4.3 Compilador

**MSVC** (Microsoft Visual C++) como compilador primario en Windows, versión la que venga con Visual Studio 2022 o posterior.

Flags recomendados:

- `/W4` (warning level alto)
- `/WX` no (no tratar warnings como errores al inicio del proyecto; activar más adelante cuando el código esté estable)
- `/permissive-` (conformidad estricta al estándar)
- `/Zi` para PDB en Debug
- `/O2` en Release

### 4.4 Ventana, input, timers

**SDL2** (Simple DirectMedia Layer), versión `release-2.30.x` o la última estable de la rama SDL2 (no SDL3 por ahora, aunque migrar después será sencillo).

Se utiliza para:

- Creación de ventana
- Manejo de eventos (teclado, ratón, joystick)
- Creación del contexto OpenGL
- Abstracción de sistema operativo
- (Más adelante) audio vía SDL_mixer o se sustituye por miniaudio

### 4.5 API gráfica

**OpenGL 4.5 Core Profile**.

Loader de punteros de funciones: **GLAD** (generado para OpenGL 4.5 Core). Se incluye en el repositorio como archivos generados (`glad.c`, `glad.h`) bajo `external/glad/`.

Justificación: más aprendible que Vulkan, suficiente para calidad AAA 2010-2015, multiplataforma (Windows + Linux). El día que se necesite migrar a Vulkan, la capa de abstracción lo permite sin reescribir el motor.

### 4.6 Matemática 3D

**GLM** (OpenGL Mathematics).

Header-only, sintaxis idéntica a GLSL, eficiente. Uso obligatorio para todo tipo vectorial, matricial y cuaterniones. Ningún tipo matemático propio.

### 4.7 UI del editor

**Dear ImGui**, rama `docking` (no `master`).

Backend: `imgui_impl_sdl2.cpp` + `imgui_impl_opengl3.cpp`.

Justificación: estándar de la industria para editores de motor. Rama docking permite paneles acoplables tipo Unity/Unreal.

**ImGuizmo**: complemento para gizmos de transformación 3D. Se introduce en Hito 7-8, no antes.

### 4.8 Carga y escritura de imágenes

**stb_image** (lectura de PNG, JPG, BMP, TGA, etc.) y **stb_image_write** (escritura de PNG, JPG, BMP).

Ambos single-header, del proyecto stb de Sean Barrett. Se incluyen directamente en `external/stb/`.

### 4.9 Importación de modelos 3D

**assimp** (Open Asset Import Library). Entra en el Hito 10 aproximadamente.

Soporta 40+ formatos (OBJ, FBX, glTF, DAE, STL, etc.). Permite al usuario final importar modelos de Blender, Maya, 3ds Max, Sketchfab sin restringirse a un solo formato.

### 4.10 Serialización

**Fase 1 (Hitos 1-6):** JSON usando **nlohmann/json** (single-header).

**Fase 2 (Hitos 10+):** complemento con **cereal** para facilitar serialización automática de tipos custom y eventual migración a formato binario sin reescribir lógica.

Todos los formatos del motor comienzan siendo JSON legible. Binario se considera una optimización posterior cuando los archivos excedan cierto tamaño.

### 4.11 Logging

**spdlog**.

Desde el Hito 1. Niveles: `trace`, `debug`, `info`, `warn`, `error`, `critical`. Canales (sinks) configurables: consola + archivo rotatorio en `logs/`.

Categorías/loggers separados por sistema: `engine`, `render`, `audio`, `physics`, `script`, `editor`, `game`.

### 4.12 Formateo de strings

**fmt** (viene incluido con spdlog, no hay que declararlo aparte).

Uso preferido sobre `iostream` para cualquier formateo.

### 4.13 Enums a strings

**magic_enum** (single-header).

Para serialización legible, logging, debugging, y UI del editor. Uso obligatorio para cualquier enum que cruce la frontera hacia texto.

### 4.14 Entidades y componentes

**EnTT**, envuelto en una fachada propia.

La API pública del motor ofrece clases `Entity` y `Scene` con métodos tipo `entity.addComponent<Transform>(args...)` y `entity.getComponent<Transform>()`. Por debajo, estas fachadas usan EnTT para el almacenamiento y las consultas.

Justificación: EnTT da rendimiento y robustez industriales. La fachada esconde la verbosidad de templates y mensajes de error complejos, ofreciendo una API al desarrollador similar a Unity.

Entra en el Hito 7.

### 4.15 Física

**Fase 1 (Hitos 4-9):** colisiones AABB propias, aproximadamente 50-100 líneas de código. Solo para colisión jugador-vs-paredes. Sin dinámicas, sin respuesta física real más allá de "stop".

**Fase 2 (Hitos 10+):** integración de **Jolt Physics** para física completa (rigid bodies, ragdolls, constraints, raycasts, characters).

Justificación: Jolt es el estándar moderno (usado en Horizon Forbidden West y Godot 4). Introducirlo temprano abruma; introducirlo cuando hay mecánicas que lo justifican es natural.

### 4.16 Scripting

**Lua 5.4** con **sol2** como binding C++.

Entra en Hito 6. Se usa exclusivamente para:

- Comportamiento de entidades no-triviales (enemigos, triggers, puertas).
- Lógica de nivel específica.
- Configuración en tiempo de ejecución.
- (Más adelante) UI del juego.

**No se usa Lua para:**

- Sistemas del motor (render, física, audio): eso es C++.
- Código de performance crítica.

Hot-reload de scripts: sí, soportado desde su introducción. El motor detecta cambios en archivos `.lua` y recarga el script sin reiniciar.

### 4.17 Audio

**miniaudio** (single-header).

Entra en Hito 8-9. Soporta reproducción, mezcla, audio posicional 3D con trabajo adicional.

### 4.18 Profiling

**Tracy Profiler**.

Entra en Hito 5-6. Integración mínima en el motor (un puñado de macros). Tracy es una aplicación externa a la que el motor se conecta vía red local para visualizar timings.

### 4.19 Testing

**doctest** (single-header).

Entra en Hito 3-4. Solo para código puro: matemática, parsers, algoritmos. No se testea render, UI ni lógica de gameplay.

### 4.20 Debug GPU

**RenderDoc** (aplicación externa, gratuita). Se instala cuando aparezca necesidad (Hito 3+).

Permite capturar un frame y analizarlo draw call por draw call. Esencial para depurar problemas visuales. No requiere integración en el código.

### 4.21 Formato de código

**clang-format** con archivo de configuración `.clang-format` en la raíz del repositorio.

Estilo base: LLVM o Google, adaptado al gusto del desarrollador. Se formatea automáticamente en commits (pre-commit hook opcional).

### 4.22 UI del juego (distinto de UI del editor)

**RmlUi**: entra en Hito 15+ cuando se necesiten menús de juego "bonitos" (HTML/CSS-like). Por ahora, cualquier UI dentro del juego (HUD, menús) puede hacerse con ImGui temporalmente.

### 4.23 Resumen tabular

| Componente          | Librería               | Hito de entrada      |
| ------------------- | ---------------------- | -------------------- |
| Lenguaje            | C++17                  | 1                    |
| Build               | CMake + CPM.cmake      | 1                    |
| Compilador          | MSVC (Windows)         | 1                    |
| Ventana/input       | SDL2                   | 1                    |
| API gráfica         | OpenGL 4.5 Core + GLAD | 2-3                  |
| Matemática          | GLM                    | 1                    |
| UI editor           | Dear ImGui (docking)   | 1                    |
| Gizmos 3D           | ImGuizmo               | 7-8                  |
| Imágenes entrada    | stb_image              | 5                    |
| Imágenes salida     | stb_image_write        | según necesidad      |
| Modelos 3D          | assimp                 | 10                   |
| Serialización       | nlohmann/json + cereal | 6                    |
| Logging             | spdlog                 | 1                    |
| Enums ↔ strings     | magic_enum             | 2                    |
| Entidades           | EnTT (fachada propia)  | 7                    |
| Física              | AABB propio → Jolt     | 4 propio, 10-12 Jolt |
| Scripting           | Lua 5.4 + sol2         | 6                    |
| Audio               | miniaudio              | 8-9                  |
| Profiler            | Tracy                  | 5-6                  |
| Tests               | doctest                | 3-4                  |
| Debug GPU (externo) | RenderDoc              | 3                    |
| Formato código      | clang-format           | 1                    |
| UI in-game          | RmlUi                  | 15+                  |

---

## 5. Arquitectura general del motor

### 5.1 Principios arquitectónicos

**Separación estricta entre editor y runtime.** El editor es una capa sobre el runtime. El runtime (el motor en sí) debe poder ejecutarse sin la capa de editor para futuros builds standalone de juegos.

**Separación estricta entre motor y juego.** El motor expone una API. El juego (eventualmente en Lua + algunos scripts C++) consume esa API. El motor no conoce detalles del juego.

**Una sola aplicación, dos modos.** El ejecutable `MoodEditor.exe` corre en **Editor Mode** o **Play Mode**. Transición con botón "Play" y tecla Escape.

**Abstracción de API gráfica (Render Hardware Interface / RHI).** Todo el código fuera de `src/engine/render/opengl/` debe ser agnóstico de OpenGL. Habla con clases abstractas `IRenderer`, `ITexture`, `IShader`, `IMesh`. Esto permite migrar a Vulkan/DirectX en el futuro sin reescribir el motor entero.

**Sistema de entidades-componentes** como modelo de escena. Todo lo que existe en un mapa es una entidad con componentes.

**Sistema de eventos / mensajes** para desacoplamiento entre sistemas. Por ejemplo, el sistema de audio se suscribe a eventos `EntitySpawned` emitidos por la escena.

### 5.2 Capas del motor (de bajo nivel a alto nivel)

```
┌─────────────────────────────────────────────┐
│  Capa 8: Juego (Lua scripts + assets)       │  ← Contenido del usuario
├─────────────────────────────────────────────┤
│  Capa 7: Editor (ImGui UI, herramientas)    │
├─────────────────────────────────────────────┤
│  Capa 6: Sistemas de juego                  │
│  (physics, audio, input, ai, etc.)          │
├─────────────────────────────────────────────┤
│  Capa 5: Escena y entidades (EnTT facade)   │
├─────────────────────────────────────────────┤
│  Capa 4: Renderer (RHI abstracto)           │
├─────────────────────────────────────────────┤
│  Capa 3: Backend OpenGL (GLAD, shaders)     │
├─────────────────────────────────────────────┤
│  Capa 2: Plataforma (SDL2, filesystem)      │
├─────────────────────────────────────────────┤
│  Capa 1: Núcleo (logging, math, memoria)    │
└─────────────────────────────────────────────┘
```

**Regla de dependencias:** una capa solo puede depender de capas iguales o inferiores, nunca superiores. Esto mantiene el motor modular y testeable.

### 5.3 Loop principal (pseudocódigo)

```cpp
while (app.running) {
    // 1. Input y eventos
    processSDLEvents();
    dispatchEventsToImGui();          // Solo si estamos en Editor Mode
    dispatchEventsToGameInput();      // Siempre

    // 2. Update
    double dt = computeDeltaTime();
    if (mode == Mode::Play) {
        scene.update(dt);             // Lógica de juego
        physics.step(dt);
        audio.update(dt);
    } else {
        editor.update(dt);            // Solo animaciones de UI, hot-reload, etc.
    }

    // 3. Render
    beginFrame();
    renderer.renderScene(scene, camera);
    if (mode == Mode::Editor) {
        editor.drawUI();              // ImGui sobre el framebuffer
    }
    endFrame();
}
```

### 5.4 Modelo de entidades y componentes

Una **Entity** es solo un identificador numérico (un `uint32_t` por detrás, gestionado por EnTT).

Un **Component** es una estructura plana con datos (sin métodos de lógica). Ejemplos:

- `TransformComponent { vec3 position; quat rotation; vec3 scale; }`
- `MeshRendererComponent { MeshHandle mesh; MaterialHandle material; }`
- `CameraComponent { float fov; float near; float far; }`
- `LightComponent { vec3 color; float intensity; LightType type; }`
- `ScriptComponent { std::string scriptPath; sol::state scriptState; }`
- `RigidBodyComponent { ... }` (entra con Jolt)
- `AudioSourceComponent { AudioClipHandle clip; ... }`

Un **System** es un bloque de lógica que opera sobre entidades con cierta combinación de componentes. Ejemplos:

- `RenderSystem`: itera entidades con `Transform` + `MeshRenderer` y las dibuja.
- `ScriptSystem`: itera entidades con `Script` y ejecuta sus funciones Lua de update.
- `PhysicsSystem`: sincroniza `Transform` ↔ `RigidBody`.
- `AudioSystem`: actualiza fuentes de audio 3D según posición.

**Fachada de Entity:**

```cpp
class Entity {
public:
    template<typename T, typename... Args>
    T& addComponent(Args&&... args);

    template<typename T>
    T& getComponent();

    template<typename T>
    bool hasComponent() const;

    template<typename T>
    void removeComponent();

    // Por dentro, un entt::entity y referencia al registro.
private:
    entt::entity m_handle;
    entt::registry* m_registry;
};
```

Esto esconde EnTT del código del usuario del motor.

### 5.5 Sistema de assets

**AssetManager centralizado** con carga diferida y caching.

Cada tipo de asset tiene un `Handle<T>` opaco. Cargar una textura devuelve un `Handle<Texture>`, no un puntero directo. Esto permite:

- Carga asíncrona futura.
- Hot-reload transparente (el handle sigue siendo válido).
- Conteo de referencias automático.
- Serialización estable (los handles referencian por path relativo al proyecto).

Tipos de assets planificados: `Texture`, `Mesh`, `Material`, `Shader`, `AudioClip`, `Prefab`, `Script`, `Scene`.

### 5.6 Virtual File System

Todos los archivos del juego se acceden vía paths virtuales relativos al proyecto:

```
/textures/wall.png
/scripts/enemy.lua
/scenes/level1.moodmap
```

El VFS resuelve estos paths al filesystem real. Inicialmente los mapea directo a `<project>/assets/`. En el futuro permite empacar todo en `.pak` files para distribución.

Entra en el Hito 5.

### 5.7 Extensiones de archivo del motor

- `.moodproj`: archivo de proyecto (metadatos, configuración, lista de escenas)
- `.moodmap`: archivo de mapa/escena serializado
- `.moodprefab`: plantilla reutilizable de entidad

Estructura interna: JSON durante fases iniciales, migración posterior a formato binario tipo chunks cuando el tamaño lo justifique.

### 5.8 Estructura de un proyecto MoodEngine

```
MiJuego/
├── MiJuego.moodproj
├── assets/
│   ├── textures/
│   ├── models/
│   ├── sounds/
│   ├── scripts/
│   └── materials/
├── scenes/
│   ├── main_menu.moodmap
│   └── level1.moodmap
├── prefabs/
│   └── enemy.moodprefab
└── config/
    └── settings.json
```

El editor sabe cómo crear esta estructura al ejecutar **Archivo > Nuevo Proyecto**.

---

## 6. Estructura de carpetas del repositorio

Esta es la estructura **que el Hito 1 debe dejar establecida**, aunque muchas carpetas queden vacías o con stubs.

```
MoodEngine/
├── .clang-format                    # Configuración de formato C++
├── .gitignore
├── .gitattributes                   # line endings normalizados
├── CMakeLists.txt                   # Punto de entrada de CMake
├── CMakePresets.json                # Configuraciones predefinidas (Debug, Release)
├── LICENSE                          # MIT (placeholder hasta decisión final)
├── README.md                        # Cómo clonar, compilar, ejecutar
│
├── cmake/                           # Helpers CMake
│   ├── CPM.cmake                    # Dependencias
│   ├── CompilerWarnings.cmake       # Flags de warnings por compilador
│   └── FindX.cmake                  # (si hace falta)
│
├── docs/
│   ├── ARCHITECTURE.md              # Visión general de la arquitectura
│   ├── DECISIONS.md                 # Log cronológico de decisiones técnicas
│   ├── HITOS.md                     # Roadmap actualizado
│   ├── CODING_STYLE.md              # Convenciones de código
│   ├── CONTRIBUTING.md              # (vacío al inicio, llenar cuando sea público)
│   └── assets/                      # Imágenes para los docs
│
├── external/                        # Código externo que NO va por FetchContent
│   ├── glad/                        # Loader de OpenGL generado
│   │   ├── include/glad/glad.h
│   │   └── src/glad.c
│   └── stb/                         # stb_image, stb_image_write
│       ├── stb_image.h
│       └── stb_image_write.h
│
├── src/                             # Todo el código del motor y editor
│   ├── main.cpp                     # Punto de entrada mínimo
│   │
│   ├── core/                        # Capa 1: núcleo
│   │   ├── Log.h / .cpp             # Wrapper sobre spdlog
│   │   ├── Assert.h                 # Macros de aserción custom
│   │   ├── Types.h                  # Aliases numéricos (u32, f32, etc.)
│   │   ├── Time.h / .cpp            # Timers, delta time
│   │   ├── Memory.h                 # Helpers de memoria (futuro)
│   │   └── math/                    # Wrappers mínimos sobre GLM
│   │       ├── Math.h
│   │       ├── Transform.h
│   │       └── AABB.h               # (Hito 4)
│   │
│   ├── platform/                    # Capa 2: plataforma
│   │   ├── Window.h / .cpp          # Wrapper SDL2 window
│   │   ├── Input.h / .cpp           # Keyboard, mouse, gamepad
│   │   ├── FileSystem.h / .cpp      # Lecturas/escrituras de archivos
│   │   └── VFS.h / .cpp             # Virtual File System (Hito 5)
│   │
│   ├── engine/                      # Capas 3-5: motor
│   │   ├── render/
│   │   │   ├── IRenderer.h          # RHI abstracto
│   │   │   ├── ITexture.h
│   │   │   ├── IShader.h
│   │   │   ├── IMesh.h
│   │   │   └── opengl/              # Implementación OpenGL
│   │   │       ├── OpenGLRenderer.h / .cpp
│   │   │       ├── OpenGLTexture.h / .cpp
│   │   │       ├── OpenGLShader.h / .cpp
│   │   │       └── OpenGLMesh.h / .cpp
│   │   ├── scene/
│   │   │   ├── Scene.h / .cpp
│   │   │   ├── Entity.h / .cpp
│   │   │   └── Components.h         # Todos los components en un solo header
│   │   ├── assets/
│   │   │   ├── AssetManager.h / .cpp
│   │   │   ├── Texture.h / .cpp
│   │   │   ├── Mesh.h / .cpp
│   │   │   ├── Material.h / .cpp
│   │   │   └── Handle.h
│   │   └── serialization/
│   │       ├── SceneSerializer.h / .cpp     # Hito 6
│   │       ├── ProjectSerializer.h / .cpp   # Hito 6
│   │       └── JsonHelpers.h
│   │
│   ├── systems/                     # Capa 6: sistemas
│   │   ├── RenderSystem.h / .cpp
│   │   ├── ScriptSystem.h / .cpp    # Hito 6
│   │   ├── PhysicsSystem.h / .cpp   # Hito 4 (AABB propio), luego Jolt
│   │   ├── AudioSystem.h / .cpp     # Hito 8-9
│   │   └── InputSystem.h / .cpp
│   │
│   ├── editor/                      # Capa 7: editor
│   │   ├── EditorApplication.h / .cpp   # Shell principal del editor
│   │   ├── EditorUI.h / .cpp            # Coordinador de paneles
│   │   ├── MenuBar.h / .cpp
│   │   ├── StatusBar.h / .cpp
│   │   ├── Dockspace.h / .cpp
│   │   └── panels/
│   │       ├── IPanel.h             # Interfaz base de panel
│   │       ├── ViewportPanel.h / .cpp
│   │       ├── InspectorPanel.h / .cpp
│   │       ├── AssetBrowserPanel.h / .cpp
│   │       ├── HierarchyPanel.h / .cpp    # (Hito 7)
│   │       ├── ConsolePanel.h / .cpp      # (Hito 5)
│   │       └── SettingsPanel.h / .cpp
│   │
│   └── game/                        # Capa 8: juego (vacío al inicio)
│       └── .gitkeep
│
├── tests/                           # Hito 3-4
│   ├── CMakeLists.txt
│   ├── test_math.cpp
│   └── test_serialization.cpp
│
├── assets/                          # Assets de ejemplo del editor
│   └── textures/
│       └── missing.png              # Textura fallback rosa-negro (Hito 5)
│
├── shaders/                         # Shaders built-in del motor
│   ├── default.vert                 # (Hito 3)
│   └── default.frag                 # (Hito 3)
│
└── tools/                           # Herramientas externas auxiliares (futuro)
    └── .gitkeep
```

**Regla:** el Hito 1 crea toda esta estructura, aunque las carpetas de hitos futuros solo contengan un `.gitkeep`.

---

## 7. Convenciones de código y estilo

### 7.1 Nombres

- **Tipos** (`class`, `struct`, `enum`, `using`): `PascalCase`. Ejemplo: `Renderer`, `TransformComponent`, `LightType`.
- **Funciones libres y métodos**: `camelCase`. Ejemplo: `createWindow`, `renderScene`.
- **Variables locales y parámetros**: `camelCase`. Ejemplo: `int triangleCount`.
- **Miembros privados** de clase: `m_camelCase`. Ejemplo: `SDL_Window* m_window;`
- **Miembros públicos de structs de datos**: `camelCase` sin prefijo.
- **Constantes de tiempo de compilación**: `k_PascalCase`. Ejemplo: `constexpr int k_MaxEntities = 10000;`
- **Macros**: `UPPER_SNAKE_CASE` y solo cuando no hay alternativa (`MOOD_ASSERT`, `MOOD_LOG_INFO`).
- **Namespaces**: `snake_case` o `PascalCase` (preferido: PascalCase para consistencia). El motor entero vive bajo el namespace `Mood`.

### 7.2 Archivos

- Un tipo principal por archivo, salvo cuando varios tipos pequeños están fuertemente acoplados (ej: `Components.h` contiene todos los components).
- Nombre de archivo igual al tipo principal: `Renderer.h` / `Renderer.cpp`.
- Headers con `#pragma once`, no include guards tradicionales.
- Extensión `.h` para headers, `.cpp` para implementaciones.

### 7.3 Includes

Orden dentro de un archivo:

1. Header del propio módulo (si es un `.cpp`).
2. Headers del proyecto (`"core/..."`, `"engine/..."`).
3. Headers de librerías externas (`<SDL.h>`, `<glad/glad.h>`, `<glm/...>`).
4. Headers estándar (`<vector>`, `<string>`).

Usar comillas `"..."` para includes del proyecto, `<...>` para externos.

### 7.4 Comentarios

- En **español**, para acompañar el aprendizaje del desarrollador.
- Solo donde aportan valor: **intención**, no **descripción**.
- Malo: `// incrementa i`. Bueno: `// Saltamos el primer vértice porque es el pivote del fan.`
- Doxygen ligero en APIs públicas del motor: `/// @brief ...`.

### 7.5 Formateo

Archivo `.clang-format` en la raíz, basado en estilo **LLVM** con estos ajustes:

```
IndentWidth: 4
ColumnLimit: 120
PointerAlignment: Left
AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: Empty
BreakBeforeBraces: Attach
```

Todo archivo nuevo se formatea con `clang-format -i <archivo>` antes de commit. Opcional: pre-commit hook automatizado.

### 7.6 Otros principios de código

- Preferir `const` siempre que se pueda.
- Preferir referencias `&` a punteros cuando no hay semántica de "puede ser nulo".
- `std::unique_ptr` para ownership único, `std::shared_ptr` solo cuando sea realmente compartido.
- Forward declarations en headers cuando se puede, para reducir tiempos de compilación.
- Nada de `new`/`delete` manual fuera de casos muy específicos (sistemas de memoria custom futuros).
- RAII siempre: destructores cierran recursos.

---

## 8. Sistema de build y gestión de dependencias

### 8.1 Estructura del CMakeLists.txt raíz

El CMakeLists raíz debe:

1. Especificar versión mínima de CMake (3.24+).
2. Definir el proyecto `MoodEngine` con versión (0.1.0 al inicio).
3. Configurar C++17 y flags de compilador.
4. Incluir `cmake/CPM.cmake`.
5. Declarar todas las dependencias externas con `CPMAddPackage`.
6. Compilar las librerías internas (GLAD, backends de ImGui) como targets estáticos.
7. Definir el ejecutable `MoodEditor`.
8. Enlazar todo.

### 8.2 Dependencias con CPM

Ejemplo de estructura (el agente de código puede adaptar versiones exactas a las últimas estables):

```cmake
# SDL2
CPMAddPackage(
    NAME SDL2
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-2.30.2
    OPTIONS "SDL_STATIC OFF" "SDL_SHARED ON"
)

# GLM (header-only)
CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
)

# spdlog
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.14.1
)

# Dear ImGui (docking branch)
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
    DOWNLOAD_ONLY YES    # No tiene CMakeLists propio utilizable
)
# ImGui se compila como librería interna en su propio target.
```

### 8.3 Librerías internas a compilar

Estas no vienen con CMakeLists o queremos control total:

**`imgui` (target estático):**

- Archivos fuente: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imgui_demo.cpp`, `backends/imgui_impl_sdl2.cpp`, `backends/imgui_impl_opengl3.cpp`.
- Includes públicos: el directorio de ImGui + `backends/`.

**`glad` (target estático):**

- Archivos fuente: `external/glad/src/glad.c`.
- Includes públicos: `external/glad/include/`.

**`stb` (target interface / header-only):**

- Solo includes: `external/stb/`.

### 8.4 CMakePresets.json

Define dos presets mínimos:

```json
{
  "version": 4,
  "configurePresets": [
    {
      "name": "windows-msvc-debug",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "windows-msvc-release",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ]
}
```

### 8.5 Comandos estándar

Documentar en el README:

```bash
# Configurar
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug

# Ejecutar
./build/debug/Debug/MoodEditor.exe
```

---

## 9. Filosofía de desarrollo

Estos principios deben respetarse en todas las decisiones del agente de código:

### 9.1 Ship something

Prioridad número uno: que el proyecto compile y corra después de cada sesión. Nunca dejar el repositorio en estado roto. Un hito feo y funcional > un diseño elegante sin compilar.

### 9.2 Incrementalismo disciplinado

Cada hito agrega valor visible. Cada commit es auto-contenido y compila. Las features grandes se descomponen en sub-hitos.

### 9.3 Introducir complejidad solo cuando se necesita

No pre-optimizar, no pre-abstraer. Las librerías pesadas (EnTT, Jolt, Lua) entran solo cuando su problema concreto aparece. Ver roadmap para timing.

### 9.4 Código leíble antes que código elegante

El desarrollador está aprendiendo. Preferir:

- Nombres verbosos y claros.
- Comentarios de intención en funciones no triviales.
- Patrones obvios antes que patrones ingeniosos.
- Flat antes que nested.

### 9.5 Mediciones antes que optimizaciones

No optimizar especulativamente. Usar Tracy y RenderDoc para identificar cuellos de botella antes de cambiar código. "Profile, don't guess" (Carmack).

### 9.6 RAII estricto

Todo recurso se gestiona por destructor. Ningún `SDL_Quit` o `glDelete*` colgado. Shutdown limpio garantizado por diseño.

### 9.7 Documentar decisiones

Toda decisión técnica no trivial se anota en `docs/DECISIONS.md` con fecha, contexto y razonamiento. Esto evita re-litigar decisiones y ayuda a entender el "por qué" meses después.

Formato:

```markdown
## 2026-04-21: OpenGL 4.5 Core en lugar de Vulkan

**Contexto:** elección de API gráfica.
**Decisión:** OpenGL 4.5 Core Profile.
**Razones:** menor curva de aprendizaje, suficiente para calidad AAA 2010-2015,
la GTX 1660 del desarrollador la soporta perfectamente.
**Alternativas consideradas:** Vulkan (descartado por complejidad inicial),
DirectX 12 (solo Windows, objetivo es multiplataforma).
**Revisar si:** el rendimiento pincha en escenas grandes, o si se necesitan
features modernas (mesh shaders, ray tracing hardware).
```

### 9.8 Git hygiene

- Commits atómicos con mensajes descriptivos.
- Formato sugerido: `tipo(scope): mensaje`. Ejemplos: `feat(editor): añadir panel de inspector`, `fix(render): evitar crash al cerrar ventana`, `docs: actualizar README con instrucciones de build`.
- Branches para features grandes (`feature/hito-4-edicion-mapas`), merge a `main` cuando compilen y funcionen.
- Tags para hitos completados: `v0.1.0-hito1`, `v0.2.0-hito2`, etc.

---

## 10. Roadmap completo de hitos

Listado de alto nivel de los ~25 hitos planificados. Solo el Hito 1 tiene especificación detallada en este documento. Los demás se elaborarán cuando se aproximen.

### Fase 1: Fundamentos (Hitos 1-5)

**Hito 1 — Shell del editor.**
Ventana SDL2 + Dear ImGui (docking) + logging con spdlog + estructura completa de carpetas + menús funcionales + paneles acoplables vacíos + barra de estado + CMake con CPM.

**Hito 2 — Primer triángulo con OpenGL.**
Contexto OpenGL 4.5 Core inicializado vía GLAD. RHI abstracto con implementación OpenGL. Un triángulo con shader propio dibujado en el viewport del editor (render a textura que ImGui muestra en el panel Viewport).

**Hito 3 — Cubo texturizado con cámara.**
Cargar una textura con stb_image. Cubo rotando con matrices de modelo/vista/proyección. Cámara FPS controlable con WASD + ratón en Play Mode (Editor Mode muestra cámara de editor orbital). Introducción a Tracy y doctest.

**Hito 4 — Mundo grid + colisiones AABB.**
Modo "Wolfenstein simulado": mapa definido como grid 2D de tiles, renderizado como cubos 3D. Colisiones AABB propias para que el jugador no atraviese paredes. Debug rendering básico (líneas, cajas).

**Hito 5 — Asset Browser + gestión de texturas.**
AssetManager funcional. Panel Asset Browser que lee una carpeta del proyecto y muestra miniaturas. Asignar texturas a tiles del mapa desde el editor. VFS inicial. Textura de fallback rosa-negro. Consola in-game básica.

### Fase 2: Edición y contenido (Hitos 6-10)

**Hito 6 — Serialización de proyectos y mapas.**
Archivo > Nuevo Proyecto crea estructura de carpetas. Archivo > Abrir / Guardar con JSON (nlohmann). Formato `.moodproj` + `.moodmap`. Persistencia de estado del editor entre sesiones.

**Hito 7 — Entidades, componentes, jerarquía.**
Integración de EnTT con fachada propia (`Entity`, `Scene`). Panel Hierarchy (árbol de entidades). Panel Inspector funcional (ver/editar componentes de la entidad seleccionada). Componentes básicos: Transform, MeshRenderer, Camera, Light.

**Hito 8 — Scripting con Lua.**
Integración Lua 5.4 + sol2. ScriptComponent. Hot-reload de scripts. API mínima expuesta a Lua: transform, input, log, spawn/destroy entity. Primer enemigo scripteado.

**Hito 9 — Audio básico.**
Integración miniaudio. AudioClip asset. AudioSourceComponent. Reproducción de música y efectos. Audio posicional 3D básico.

**Hito 10 — Importación de modelos 3D.**
Integración assimp. Carga de modelos `.obj`, `.gltf`. MeshAsset con submallas. Materiales básicos (textura + color). Drag & drop de modelos al viewport.

### Fase 3: Motor adulto (Hitos 11-15)

**Hito 11 — Iluminación: Phong / Blinn-Phong.**
Luces direccional, puntual y spot. Shader de iluminación estándar. Normal mapping. Materiales PBR todavía no, solo Blinn-Phong.

**Hito 12 — Física con Jolt.**
Retirar colisiones AABB propias. Integrar Jolt Physics. RigidBodyComponent (static, dynamic, kinematic). Character Controller para el jugador. Raycasts.

**Hito 13 — Gizmos y selección.**
ImGuizmo integrado. Selección de entidades por click en el viewport (picking). Gizmos de mover/rotar/escalar. Snapping al grid.

**Hito 14 — Prefabs.**
Sistema de prefabs: guardar entidad + hijos como `.moodprefab`, instanciar múltiples copias. Sobreescritura de propiedades por instancia.

**Hito 15 — Skybox, fog, post-procesado básico.**
Skybox con cubemap. Fog exponencial. Post-procesado: gamma correction, exposición, tone mapping. Framebuffer con HDR.

### Fase 4: Visual moderno (Hitos 16-20)

**Hito 16 — Shadow mapping.**
Shadow maps para la luz direccional. PCF para sombras suaves. Cascadas (CSM) si es viable.

**Hito 17 — PBR (Physically Based Rendering).**
Migrar a shaders PBR metallic/roughness. Texturas: albedo, normal, metallic, roughness, ao. IBL básica con cubemap.

**Hito 18 — Renderer avanzado: deferred o forward+.**
Decidir arquitectura de renderer para soportar muchas luces eficientemente. Migrar desde forward simple.

**Hito 19 — Animación esquelética.**
Huesos, skeletal meshes, animaciones desde glTF. Blend de animaciones. Animator básico.

**Hito 20 — UI del juego con RmlUi.**
Menú principal, HUD, pantalla de pausa escriptables con HTML/CSS-like. Separado de la UI del editor.

### Fase 5: Herramienta seria (Hitos 21-25+)

**Hito 21 — Empaquetado de juegos standalone.**
Build de "juego final" sin editor. Assets empaquetados en `.pak`. Exportar proyecto a carpeta distribuible.

**Hito 22 — Undo/Redo completo en editor.**
Command pattern para todas las operaciones del editor. Historial visible.

**Hito 23 — Editor de materiales visual.**
Panel para crear/editar materiales. Preview en vivo.

**Hito 24 — Particle system.**
Sistema de partículas GPU-driven. Emisores configurables desde el editor.

**Hito 25 — Multiplataforma: Linux.**
Compilar y correr en Linux. Ajustes necesarios. Clang + GCC soportados.

**Hitos 26+ — TBD.**
Según lo aprendido y lo que el motor necesite: navegación con AI path, occlusion culling, LOD, terrain system, vegetation, stream del mundo, editor de niveles avanzado, etc.

---

## 11. Especificación detallada del Hito 1

**Esta sección es lo que el agente de código debe implementar ahora. No implementar hitos futuros, pero dejar estructura preparada.**

### 11.1 Objetivo del Hito 1

Al ejecutar `MoodEditor.exe`, el usuario debe ver una aplicación funcional con apariencia de herramienta profesional: ventana con menús, paneles acoplables, barra de estado, y sistema de logging operativo.

### 11.2 Criterios de aceptación

- [ ] `cmake --preset windows-msvc-debug` se ejecuta sin errores desde una carpeta limpia (clonado fresco del repo).
- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings relevantes.
- [ ] El ejecutable abre una ventana de 1280x720 con título `MoodEngine Editor — v0.1.0 (Hito 1)`.
- [ ] La ventana tiene una barra de menú superior con los items: **Archivo**, **Editar**, **Ver**, **Ayuda**.
  - Archivo: Nuevo Proyecto, Abrir Proyecto, Guardar (los tres muestran popup "No implementado" o log), Salir (cierra la app).
  - Editar: Deshacer, Rehacer (placeholders).
  - Ver: ítems para mostrar/ocultar cada panel.
  - Ayuda: "Acerca de" que muestra un popup con nombre del proyecto, versión, link al repo.
- [ ] Existe un `DockSpace` ocupando toda la ventana bajo la menu bar.
- [ ] Hay tres paneles acoplables inicialmente:
  - **Viewport** (centro): panel grande con fondo gris oscuro. En el centro dibuja texto "Viewport (Hito 2: primer triángulo)".
  - **Inspector** (derecha): texto "No hay entidad seleccionada".
  - **Asset Browser** (abajo): texto "Sin assets cargados".
- [ ] Los paneles son movibles, acoplables a distintos bordes, y flotantes si se arrastran fuera.
- [ ] Hay una barra de estado en la parte inferior que muestra:
  - FPS actual (promedio de los últimos 60 frames).
  - Modo actual: "Editor Mode" (Play Mode no implementado aún, solo mostrar el texto).
  - Un mensaje de estado libre (ej: "Listo").
- [ ] El sistema de logging spdlog está inicializado:
  - Logger principal `engine` escribiendo a consola con colores + archivo `logs/engine.log`.
  - Al arrancar, log al menos tres entradas INFO: "MoodEngine iniciando...", "Ventana creada (1280x720)", "Editor listo".
  - Al cerrar, log "MoodEngine cerrado limpiamente".
- [ ] El cierre es limpio:
  - Cerrar con la X de la ventana funciona.
  - Archivo > Salir funciona.
  - No hay fugas de SDL ni de ImGui (destructores correctos, shutdown ordenado).
- [ ] El `.gitignore` ignora `build/`, `.vs/`, `logs/*.log`, archivos temporales de IDEs.
- [ ] El `README.md` contiene instrucciones claras: cómo clonar, requisitos, cómo configurar CMake, cómo compilar, cómo ejecutar.
- [ ] Los archivos `docs/ARCHITECTURE.md`, `docs/DECISIONS.md`, `docs/HITOS.md` existen con contenido inicial (pueden ser stubs referenciando este documento).

### 11.3 Archivos a crear en el Hito 1

**Raíz:**

- `CMakeLists.txt`
- `CMakePresets.json`
- `.clang-format`
- `.gitignore`
- `.gitattributes`
- `README.md`
- `LICENSE` (MIT placeholder)

**cmake/:**

- `CPM.cmake` (descargado del repo oficial)
- `CompilerWarnings.cmake`

**docs/:**

- `ARCHITECTURE.md`
- `DECISIONS.md`
- `HITOS.md`
- `CODING_STYLE.md`
- `CONTRIBUTING.md` (placeholder)

**external/glad/** (solo estructura, puede generarse en Hito 2):

- Nota: GLAD es necesario desde Hito 2. En Hito 1 puede dejarse la carpeta vacía con un README indicando que se generará al entrar Hito 2.

**external/stb/** (solo estructura):

- Placeholder por ahora.

**src/main.cpp:** punto de entrada mínimo. Instancia `Mood::EditorApplication`, llama a `run()`, devuelve código de salida.

**src/core/:**

- `Log.h` / `Log.cpp`: wrapper sobre spdlog. Funciones `Mood::Log::init()`, `Mood::Log::shutdown()`, loggers nombrados (`engine`, `editor`, `render`).
- `Assert.h`: macro `MOOD_ASSERT(cond, msg)`.
- `Types.h`: aliases `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64`.
- `Time.h` / `Time.cpp`: clase `Timer` con `elapsed()`, cálculo de delta time, FPS counter.

**src/platform/:**

- `Window.h` / `Window.cpp`: wrapper sobre `SDL_Window`. Crea ventana con flags apropiados para OpenGL (aunque OpenGL se usa desde Hito 2, configurar el contexto desde ya).

**src/editor/:**

- `EditorApplication.h` / `EditorApplication.cpp`: clase principal. Miembros: `Window m_window`, `SDL_GLContext m_glContext`, `bool m_running`, `EditorUI m_ui`. Métodos: constructor (inicializa SDL, ventana, ImGui, spdlog), destructor (shutdown inverso), `run()` (loop principal).
- `EditorUI.h` / `EditorUI.cpp`: dibuja todos los elementos ImGui por frame. Coordina menus, dockspace, paneles, status bar.
- `MenuBar.h` / `MenuBar.cpp`: dibuja la menu bar.
- `StatusBar.h` / `StatusBar.cpp`: dibuja la barra de estado.
- `Dockspace.h` / `Dockspace.cpp`: configura el dockspace central.
- `panels/IPanel.h`: interfaz base con método virtual `onImGuiRender()` y campo `bool visible`.
- `panels/ViewportPanel.h` / `.cpp`: panel viewport.
- `panels/InspectorPanel.h` / `.cpp`: panel inspector.
- `panels/AssetBrowserPanel.h` / `.cpp`: panel asset browser.

**src/engine/, src/systems/, src/game/:** carpetas con `.gitkeep` o headers stub comentados con TODO para hitos futuros.

### 11.4 Detalles técnicos específicos del Hito 1

**Creación del contexto OpenGL desde Hito 1:** aunque no se dibuje nada con OpenGL todavía, la ventana SDL se crea con flags `SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE`. Se configura el contexto para OpenGL 4.5 Core (`SDL_GL_CONTEXT_MAJOR_VERSION = 4`, `SDL_GL_CONTEXT_MINOR_VERSION = 5`, `SDL_GL_CONTEXT_PROFILE_CORE`). El glClear en cada frame con un color gris oscuro (ej: `0.1, 0.1, 0.1, 1.0`) sirve como fondo sobre el cual ImGui renderiza. Esto evita reconfigurar todo en Hito 2.

**Backend ImGui OpenGL3:** usar `imgui_impl_opengl3.cpp` aunque en Hito 1 solo renderice UI. Es el backend correcto para todo el ciclo de vida del proyecto.

**Dockspace full-window:** crear una ventana "host" de ImGui con flags:

```
ImGuiWindowFlags_NoDocking | NoTitleBar | NoCollapse |
NoResize | NoMove | NoBringToFrontOnFocus | NoNavFocus |
NoBackground | MenuBar
```

y dentro llamar a `ImGui::DockSpace`. La menu bar va dentro de esta ventana host.

**FPS counter:** promedio móvil de los últimos 60 frames. Actualizar el texto de la status bar con `fmt::format` vía spdlog o directamente.

**Habilitar docking:** `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable`. Opcional: `ImGuiConfigFlags_ViewportsEnable` para que los paneles puedan salir de la ventana principal (útil con multi-monitor, aunque no es prioridad del proyecto).

**Main de SDL2 en Windows:** definir `SDL_MAIN_HANDLED` antes de `#include <SDL.h>` en `main.cpp` para evitar que SDL se apropie de `main`.

### 11.5 Qué NO hacer en el Hito 1

- No inicializar OpenGL ni cargar punteros con GLAD todavía (se deja para Hito 2). Solo crear el contexto GL básico con SDL.
- No dibujar triángulos, shaders, ni geometría.
- No implementar lógica de guardado/carga real (los menús muestran popup "No implementado").
- No crear sistema de entidades, componentes, ni nada de EnTT.
- No integrar Lua ni Jolt.
- No crear Play Mode funcional (solo el texto en la status bar).

### 11.6 Entregables del Hito 1

1. Todos los archivos listados, creados y consistentes.
2. El proyecto compila en Windows con MSVC en modo Debug y Release.
3. La aplicación se ejecuta y cumple todos los criterios de aceptación.
4. Un commit (o varios) con mensajes descriptivos siguiendo convención.
5. Tag git `v0.1.0-hito1` al completar.
6. Actualización de `docs/HITOS.md` marcando Hito 1 como completado y listando siguientes pasos.
7. Actualización de `docs/DECISIONS.md` con cualquier decisión técnica relevante tomada durante implementación.

### 11.7 Resumen ejecutivo del Hito 1 para el agente

Creá un proyecto C++17 con CMake 3.24+ en Windows. Configurá SDL2, Dear ImGui (docking) y spdlog vía CPM.cmake. Inicializá una ventana OpenGL 4.5 de 1280x720. Montá Dear ImGui con backends SDL2 + OpenGL3. Dibujá una UI con menu bar, dockspace central, tres paneles vacíos (Viewport, Inspector, Asset Browser), y una barra de estado con FPS. Cerrá limpio al recibir QUIT. Dejá la estructura de carpetas completa del proyecto armada para que hitos futuros encajen sin refactor. Comentá el código en español. Mensajes de commit en español. No implementes ningún hito futuro. Cuando termine, tag `v0.1.0-hito1` y reportá al usuario qué se hizo y qué sigue.

---

## 12. Qué NO hacer (aplicable a todos los hitos)

Reglas que no se violan sin acuerdo explícito del desarrollador:

- **No instalar dependencias manualmente.** Todo vía CPM.cmake (salvo glad y stb que van en `external/`).
- **No usar vcpkg, Conan, apt, chocolatey, winget** para dependencias del proyecto.
- **No usar** motores o engines existentes (Unity, Unreal, Godot, Raylib, bgfx, SFML como wrapper).
- **No usar** Vulkan, DirectX, Metal como API gráfica.
- **No usar** ImGui master (debe ser la rama `docking`).
- **No usar** OpenGL legacy / compatibility profile, solo Core Profile.
- **No usar** `glBegin/glEnd`, matrices fijas (`glMatrixMode`), funciones deprecadas.
- **No usar** `std::system` o `popen` para cosas que el programa puede hacer directo.
- **No escribir** APIs específicas de Windows (Win32, COM, Direct3D) fuera de lo que SDL2 ya abstrae.
- **No añadir** dependencias no listadas en este documento sin justificación y documentación en DECISIONS.md.
- **No mezclar** capas de la arquitectura (ej: no hacer llamadas OpenGL desde capas superiores al backend).
- **No implementar** features de hitos futuros para "adelantar trabajo".
- **No dejar** warnings de compilación sin justificación.
- **No dejar** `TODO` sin archivar en un issue de GitHub o en `docs/HITOS.md`.
- **No commitear** archivos generados (binarios, `build/`, `.vs/`, cachés).
- **No commitear** assets binarios grandes al repo (usar LFS si llega el caso; por ahora, assets de ejemplo deben ser mínimos).
- **No cambiar** nombres, extensiones, ni decisiones técnicas fijas sin confirmación del desarrollador.

---

## 13. Metodología de trabajo con el agente

### 13.1 Flujo recomendado

1. El desarrollador (con ayuda del asistente de planificación) define el objetivo del hito.
2. El asistente genera o actualiza el documento de contexto técnico con especificaciones detalladas del hito.
3. El desarrollador le pasa el documento al agente de código en Antigravity.
4. El agente implementa el hito siguiendo el documento.
5. El desarrollador verifica compilación, ejecución y criterios de aceptación.
6. Si algo falla o queda dudoso, el desarrollador vuelve al asistente con un resumen de lo que se hizo, lo que rompió, y lo que no quedó claro.
7. El asistente ajusta el plan y genera el documento del siguiente hito.
8. Repetir.

### 13.2 Qué hace el agente de código

- Implementar según especificación.
- Hacer preguntas al desarrollador cuando algo del documento sea ambiguo, antes de asumir.
- Compilar y verificar localmente antes de dar por terminado un hito.
- Actualizar `docs/DECISIONS.md` y `docs/HITOS.md` al terminar.
- Reportar al final: qué se hizo, qué decisiones se tomaron no contempladas en el documento, qué issues quedaron abiertos, cuál es el siguiente paso sugerido.

### 13.3 Qué NO hace el agente de código

- No toma decisiones arquitectónicas mayores sin consultar.
- No añade dependencias no aprobadas.
- No implementa hitos futuros "por ahorrar tiempo".
- No renombra archivos, tipos o convenciones establecidas.
- No modifica este documento de contexto.

### 13.4 Reporting al final de cada hito

Al terminar un hito, el agente genera un mensaje con:

```
## Hito N completado

### Lo que se implementó
- ...
- ...

### Decisiones tomadas no contempladas en el documento
- Decisión X: justificación.

### Issues abiertos / TODOs
- ...

### Siguiente paso sugerido
- Empezar Hito N+1 con objetivo Y. Punto de arranque concreto: archivo Z, función W.

### Instrucciones para verificar
- git pull
- cmake --preset windows-msvc-debug
- cmake --build build/debug --config Debug
- Ejecutar build/debug/Debug/MoodEditor.exe
- Verificar criterios: ...
```

---

## 14. Glosario técnico

**AABB:** Axis-Aligned Bounding Box. Caja de colisión alineada a los ejes del mundo, la más simple y eficiente.

**BVH:** Bounding Volume Hierarchy. Estructura acelerada para ray tracing y colisiones.

**BSP:** Binary Space Partitioning. Estructura usada en Doom/Quake para particionar el mundo.

**Core Profile (OpenGL):** modo moderno de OpenGL que elimina las funciones deprecadas de versiones anteriores. Obligatorio para aprender OpenGL correctamente hoy.

**DDA:** Digital Differential Analyzer. Algoritmo clásico para raycasting 2D en grids.

**ECS:** Entity Component System. Patrón de arquitectura que separa datos (componentes) de lógica (sistemas).

**FPS:** 1) Frames Per Second. 2) First-Person Shooter.

**GLM:** OpenGL Mathematics. Librería header-only de matemática para gráficos.

**Hot-reload:** recargar un recurso (código, script, textura) sin reiniciar el programa.

**IHV:** Independent Hardware Vendor. NVIDIA, AMD, Intel.

**ImGui:** Immediate Mode GUI. Paradigma de UI donde el estado no se guarda entre frames.

**MSVC:** Microsoft Visual C++. Compilador de Microsoft.

**PBR:** Physically Based Rendering. Modelo de materiales basado en propiedades físicas reales.

**RAII:** Resource Acquisition Is Initialization. Patrón C++ donde recursos se liberan en destructores.

**RHI:** Render Hardware Interface. Capa de abstracción sobre APIs gráficas concretas.

**VAO/VBO/EBO:** Vertex Array Object, Vertex Buffer Object, Element Buffer Object. Objetos OpenGL para almacenar datos de malla en GPU.

**VFS:** Virtual File System. Capa de abstracción sobre el sistema de archivos real.

---

## 15. Anexos: recursos y referencias

### 15.1 Recursos de aprendizaje

**C++ moderno:**

- _Effective Modern C++_ — Scott Meyers.
- [learncpp.com](https://www.learncpp.com/) (referencia gratuita completa).

**OpenGL:**

- [learnopengl.com](https://learnopengl.com/) — el recurso definitivo, gratuito, con versión en español.
- _OpenGL Superbible_ — para profundidad adicional.

**Arquitectura de motores:**

- _Game Engine Architecture_ — Jason Gregory (3ra edición). El libro de referencia.
- _Game Programming Patterns_ — Robert Nystrom (gratis online en gameprogrammingpatterns.com).

**Inspiración histórica:**

- _Game Engine Black Book: Wolfenstein 3D_ — Fabien Sanglard.
- _Game Engine Black Book: Doom_ — Fabien Sanglard.
- _Masters of Doom_ — David Kushner (biografía de Carmack y Romero).

**Matemática:**

- _Mathematics for 3D Game Programming and Computer Graphics_ — Eric Lengyel.
- _3D Math Primer for Graphics and Game Development_ — Dunn & Parberry (más accesible).

**Lua:**

- _Programming in Lua_ — Roberto Ierusalimschy (creador del lenguaje). Primera edición gratuita online.

**ImGui:**

- Repositorio oficial: https://github.com/ocornut/imgui
- Wiki con ejemplos: https://github.com/ocornut/imgui/wiki

**Física (cuando toque):**

- Documentación de Jolt: https://github.com/jrouwe/JoltPhysics
- _Real-Time Collision Detection_ — Christer Ericson.

### 15.2 Herramientas recomendadas

- **RenderDoc** — https://renderdoc.org — debugger gráfico.
- **Tracy** — https://github.com/wolfpld/tracy — profiler.
- **Nsight Graphics** — debugger NVIDIA, complementario a RenderDoc.
- **CompilerExplorer** (godbolt.org) — para inspeccionar assembly generado.
- **CPPReference** — https://en.cppreference.com — referencia C++ oficial.

### 15.3 Referencias del repositorio

- Repo: https://github.com/DanielOjeda25/MoodEngine
- Issues: https://github.com/DanielOjeda25/MoodEngine/issues (crear cuando haga falta)
- Wiki: https://github.com/DanielOjeda25/MoodEngine/wiki (futuro)

---

## NOTA FINAL PARA EL AGENTE DE CÓDIGO

Este documento es la fuente de verdad del proyecto MoodEngine. Cualquier duda sobre decisiones arquitectónicas debe resolverse consultando este documento primero. Si una pregunta no está respondida aquí, pregunta al desarrollador antes de asumir.

Tu primera tarea es implementar el **Hito 1** especificado en la sección 11. Sigue la estructura de carpetas de la sección 6, las convenciones de código de la sección 7, y el sistema de build de la sección 8. Respeta la filosofía de desarrollo de la sección 9 y las reglas de "qué no hacer" de las secciones 11.5 y 12.

Al terminar, genera el reporte descrito en la sección 13.4 y taguea `v0.1.0-hito1`.

**Buena suerte. Construyamos MoodEngine.**
