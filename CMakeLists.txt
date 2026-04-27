cmake_minimum_required(VERSION 3.24)

project(MoodEngine
    VERSION 0.1.0
    DESCRIPTION "Motor grafico 3D propio con editor visual integrado"
    LANGUAGES CXX C
)

# Estandar C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Directorios de salida unificados
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Modulos CMake del proyecto
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CPM)
include(CompilerWarnings)

# OpenGL (lo necesita el backend de ImGui)
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# Target interno: GLAD (loader de OpenGL 4.5 Core, generado con glad2)
# ---------------------------------------------------------------------------
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)
target_link_libraries(glad PUBLIC OpenGL::GL)

# ---------------------------------------------------------------------------
# Target interno: stb (headers single-file de Sean Barrett)
# ---------------------------------------------------------------------------
# INTERFACE: solo expone include path. Los simbolos viven en stb_impl.cpp.
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE external/stb)

# ---------------------------------------------------------------------------
# Target interno: miniaudio (single-header de David Reid, Hito 9)
# ---------------------------------------------------------------------------
# Mismo patron que stb: INTERFACE + una sola TU materializa los simbolos.
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE external/miniaudio)
# En Linux habra que linkear dl/pthread/m; en Windows el driver WASAPI viene
# con el SDK. Por ahora solo soportamos Windows en CI, agregar condicional
# cuando se active el Hito 25 (Linux).

# ---------------------------------------------------------------------------
# Dependencias externas gestionadas por CPM
# ---------------------------------------------------------------------------

CPMAddPackage(
    NAME SDL2
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-2.30.2
    OPTIONS
        "SDL_STATIC OFF"
        "SDL_SHARED ON"
        "SDL_TEST OFF"
        "SDL2_DISABLE_INSTALL ON"
)

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.14.1
    OPTIONS
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_INSTALL OFF"
)

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME doctest
    GITHUB_REPOSITORY doctest/doctest
    VERSION 2.4.11
    OPTIONS
        "DOCTEST_WITH_TESTS OFF"
        "DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF"
        "DOCTEST_NO_INSTALL ON"
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
    OPTIONS
        "JSON_BuildTests OFF"
        "JSON_Install OFF"
)

# Portable File Dialogs (single-header, cross-platform). Download-only para
# crear un target INTERFACE propio; el repo no define uno utilizable via CMake.
CPMAddPackage(
    NAME portable_file_dialogs
    GITHUB_REPOSITORY samhocevar/portable-file-dialogs
    GIT_TAG 0.1.0
    DOWNLOAD_ONLY YES
)
add_library(pfd INTERFACE)
target_include_directories(pfd INTERFACE ${portable_file_dialogs_SOURCE_DIR})

# EnTT (Hito 7): almacenamiento de entidades + consultas tipo ECS.
# Se envuelve en una fachada propia (`Mood::Scene` + `Mood::Entity`) para no
# filtrar la API de EnTT al resto del motor. Ver DECISIONS.md.
CPMAddPackage(
    NAME EnTT
    GITHUB_REPOSITORY skypjack/entt
    VERSION 3.13.2
)

# Lua 5.4 (Hito 8): backend de scripting. Lua upstream no trae CMake; usamos
# el wrapper walterschell/Lua que empaqueta lua-5.4.x con un CMakeLists.
# Target: `lua_static`.
# El wrapper walterschell/Lua registra `lua-testsuite` en CTest si
# `LUA_ENABLE_TESTING` esta ON (default). Como desactivamos el binario
# `lua.exe` arriba, el test falla por "Could not find executable lua".
# Forzamos el cache con tipo BOOL antes de CPM porque los `option(...)`
# internos del wrapper ignoran variables UNINITIALIZED.
set(LUA_ENABLE_TESTING OFF CACHE BOOL "Build and run tests (Lua wrapper)" FORCE)

CPMAddPackage(
    NAME Lua
    GITHUB_REPOSITORY walterschell/Lua
    GIT_TAG v5.4.5
    OPTIONS
        "LUA_BUILD_BINARY OFF"
        "LUA_BUILD_COMPILER OFF"
)

# sol2 (Hito 8): binding header-only C++17 <-> Lua. Se envuelve dentro de
# src/engine/scripting/ para no exponer sol::* al resto del motor.
CPMAddPackage(
    NAME sol2
    GITHUB_REPOSITORY ThePhD/sol2
    GIT_TAG v3.3.0
)

# Jolt Physics (Hito 12): biblioteca de fisica rigid body. Target `Jolt`.
# Desactivamos sample/test/viewer/performance-test targets — solo queremos
# la libreria. `OVERRIDE_CXX_FLAGS OFF` evita que Jolt pise nuestros
# warnings. `INTERPROCEDURAL_OPTIMIZATION OFF` acelera la primera compile
# en Debug (LTO no aporta ahi). Single-precision (DOUBLE_PRECISION OFF).
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build
    OPTIONS
        "TARGET_UNIT_TESTS OFF"
        "TARGET_HELLO_WORLD OFF"
        "TARGET_PERFORMANCE_TEST OFF"
        "TARGET_VIEWER OFF"
        "TARGET_SAMPLES OFF"
        "OVERRIDE_CXX_FLAGS OFF"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
        "CROSS_PLATFORM_DETERMINISTIC OFF"
        "DOUBLE_PRECISION OFF"
        "GENERATE_DEBUG_SYMBOLS ON"
        # MoodEditor linka contra /MDd (dynamic debug runtime); Jolt default
        # es /MTd y eso produce LNK2038 al mezclar. Forzamos dynamic.
        "USE_STATIC_MSVC_RUNTIME_LIBRARY OFF"
)

# assimp (Hito 10): importacion de modelos 3D. Desactivamos todos los
# formatos por default y habilitamos solo OBJ + glTF + FBX (los del plan
# del hito). Sin exportadores, sin herramientas CLI, sin tests, sin shared
# lib (la API estable en Windows requiere copiar DLLs extra si fuera
# shared). `ASSIMP_WARNINGS_AS_ERRORS` se fuerza OFF porque el codigo de
# assimp tiene warnings sobre MSVC /W4 que no son nuestros.
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
CPMAddPackage(
    NAME assimp
    GITHUB_REPOSITORY assimp/assimp
    GIT_TAG v5.4.3
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "ASSIMP_BUILD_TESTS OFF"
        "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
        "ASSIMP_INSTALL OFF"
        "ASSIMP_BUILD_SAMPLES OFF"
        "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF"
        "ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF"
        "ASSIMP_BUILD_OBJ_IMPORTER ON"
        "ASSIMP_BUILD_GLTF_IMPORTER ON"
        "ASSIMP_BUILD_FBX_IMPORTER ON"
        "ASSIMP_INJECT_DEBUG_POSTFIX OFF"
)

# ---------------------------------------------------------------------------
# Target interno: ImGui compilado como libreria estatica con backends SDL2 + OpenGL3
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC
    SDL2::SDL2
    OpenGL::GL
)

# ---------------------------------------------------------------------------
# Ejecutable principal: MoodEditor
# ---------------------------------------------------------------------------
add_executable(MoodEditor
    src/main.cpp

    src/core/Log.cpp
    src/core/LogRingSink.cpp
    src/core/Time.cpp

    src/platform/Window.cpp
    src/platform/VFS.cpp

    src/engine/assets/AssetManager.cpp
    src/engine/assets/MeshLoader.cpp
    src/engine/assets/PrimitiveMeshes.cpp
    src/engine/assets/stb_impl.cpp

    src/engine/audio/AudioClip.cpp
    src/engine/audio/AudioDevice.cpp
    src/engine/audio/miniaudio_impl.cpp

    src/engine/physics/PhysicsWorld.cpp

    src/engine/serialization/EntitySerializer.cpp
    src/engine/serialization/SceneSerializer.cpp
    src/engine/serialization/ProjectSerializer.cpp
    src/engine/serialization/PrefabSerializer.cpp

    src/engine/scene/EditorCamera.cpp
    src/engine/scene/FpsCamera.cpp
    src/engine/scene/Scene.cpp
    src/engine/scene/ViewportPick.cpp
    src/engine/scene/ScenePick.cpp

    src/engine/world/GridMap.cpp

    src/systems/AudioSystem.cpp
    src/systems/LightSystem.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/PostProcessPass.cpp
    src/systems/ScriptSystem.cpp
    src/systems/ShadowPass.cpp
    src/systems/SkyboxRenderer.cpp

    src/engine/scripting/LuaBindings.cpp

    src/engine/render/opengl/OpenGLMesh.cpp
    src/engine/render/opengl/OpenGLShader.cpp
    src/engine/render/opengl/OpenGLFramebuffer.cpp
    src/engine/render/opengl/OpenGLRenderer.cpp
    src/engine/render/opengl/OpenGLTexture.cpp
    src/engine/render/opengl/OpenGLCubemapTexture.cpp
    src/engine/render/opengl/OpenGLShadowMap.cpp
    src/engine/render/opengl/OpenGLSSBO.cpp
    src/engine/render/opengl/OpenGLDebugRenderer.cpp

    src/engine/render/LightGrid.cpp

    src/editor/DemoSpawners.cpp
    src/editor/EditorApplication.cpp
    src/editor/EditorProjectActions.cpp
    src/editor/EditorRenderPass.cpp
    src/editor/EditorUI.cpp
    src/editor/MenuBar.cpp
    src/editor/StatusBar.cpp
    src/editor/Dockspace.cpp
    src/editor/panels/ViewportPanel.cpp
    src/editor/panels/InspectorPanel.cpp
    src/editor/panels/AssetBrowserPanel.cpp
    src/editor/panels/ConsolePanel.cpp
    src/editor/panels/HierarchyPanel.cpp
)

target_include_directories(MoodEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(MoodEditor PRIVATE
    SDL2::SDL2
    glm::glm
    spdlog::spdlog
    imgui
    glad
    stb
    nlohmann_json::nlohmann_json
    pfd
    EnTT::EnTT
    lua_static
    sol2::sol2
    miniaudio
    assimp::assimp
    Jolt
    OpenGL::GL
)

mood_set_compiler_warnings(MoodEditor)

# En Windows: copiar SDL2.dll junto al ejecutable para poder arrancar
if(WIN32)
    add_custom_command(TARGET MoodEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:MoodEditor>
        COMMENT "Copiando SDL2.dll junto al ejecutable"
    )
endif()

# Copiar los shaders built-in junto al ejecutable para que se encuentren con
# rutas relativas. Se usa copy_directory para que funcione tambien al agregar
# shaders nuevos sin regenerar el CMake.
add_custom_command(TARGET MoodEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:MoodEditor>/shaders
    COMMENT "Copiando shaders junto al ejecutable"
)

# Idem para los assets de ejemplo (texturas, etc.) cuando existan.
add_custom_command(TARGET MoodEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:MoodEditor>/assets
    COMMENT "Copiando assets junto al ejecutable"
)

# Working directory por defecto para Visual Studio
set_target_properties(MoodEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Target por defecto al abrir la solucion en Visual Studio
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MoodEditor)

# ---------------------------------------------------------------------------
# Tests (doctest + CTest)
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
