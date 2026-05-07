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

# Tracy (F2H2): profiler de baja overhead. ON por default en Debug, OFF en
# Release final. El cliente vive linkeado al ejecutable; el server es
# tracy-profiler.exe externo y se conecta por TCP. Cuando MOOD_PROFILE=OFF las
# macros de core/Profiler.h son no-op total (zero overhead).
option(MOOD_PROFILE "Habilitar instrumentacion Tracy" ON)
if(MOOD_PROFILE)
    CPMAddPackage(
        NAME tracy
        GITHUB_REPOSITORY wolfpld/tracy
        GIT_TAG v0.11.1
        OPTIONS
            "TRACY_ENABLE ON"
            "TRACY_ON_DEMAND ON"
    )
endif()

# meshoptimizer (F2H6): generacion de LODs por simplificacion de mesh
# y otras optimizaciones de geometria. Usado en `AssetManager::loadMesh`
# tras cargar el LOD 0 para auto-generar LOD 1 (50% tris) y LOD 2 (15%).
# Skinned meshes lo saltean en v1 (re-mapping de bone weights es scope
# grande con risk visual).
CPMAddPackage(
    NAME meshoptimizer
    GITHUB_REPOSITORY zeux/meshoptimizer
    GIT_TAG v0.21
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

# Hito 20: la UI del juego (HUD, menu de pausa) se implementa con
# Dear ImGui via el overlay del ViewportPanel — los mismos primitives
# que ya usamos para gizmos, iconos de luces y outline OBB. El intento
# de integrar RmlUi se descarto: el responsive con vw/vh no se
# comportaba como esperabamos y la complejidad superaba el beneficio
# para el alcance actual (HUD simple + menu de 3 botones). Si en el
# futuro necesitamos una UI compleja con CSS / HTML, RmlUi puede
# reincorporarse — el branch tiene la integracion completa hasta el
# Hito 20 Bloque 4 WIP (commit d9bbc2a).

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
# Libreria estatica del motor: compartida por MoodEditor + MoodPlayer
# (Hito 21 Bloque 3). Contiene todo `core/`, `platform/`, `engine/`,
# `systems/`. Lo unico que NO esta aca es `src/editor/` (paneles +
# herramientas de edicion) y `src/player/` (loop standalone). Ambos
# ejecutables linkean contra esta lib y agregan solo sus propios .cpp.
# ---------------------------------------------------------------------------
add_library(mood_engine_lib STATIC
    src/core/Log.cpp
    src/core/LogRingSink.cpp
    src/core/Time.cpp

    src/platform/Window.cpp
    src/platform/VFS.cpp

    src/engine/assets/manager/AssetManager.cpp
    src/engine/assets/loaders/MeshLoader.cpp
    src/engine/assets/primitives/PrimitiveMeshes.cpp
    src/engine/assets/loaders/stb_impl.cpp

    src/engine/audio/clips/AudioClip.cpp
    src/engine/audio/device/AudioDevice.cpp
    src/engine/audio/device/miniaudio_impl.cpp

    src/engine/game/manifest/GameManifest.cpp
    src/engine/game/overlay/GameOverlay.cpp
    src/engine/game/state/GameState.cpp

    src/engine/packaging/PackageBuilder.cpp

    src/engine/physics/world/PhysicsWorld.cpp

    src/engine/saving/SaveLoad.cpp

    src/engine/scene/serialization/EntitySerializer.cpp
    src/engine/scene/serialization/SceneLoader.cpp
    src/engine/scene/serialization/SceneSerializer.cpp
    src/engine/scene/serialization/ProjectSerializer.cpp
    src/engine/scene/serialization/PrefabSerializer.cpp

    src/engine/scene/core/EditorCamera.cpp
    src/engine/scene/core/FpsCamera.cpp
    src/engine/scene/core/Scene.cpp
    src/engine/scene/queries/ViewportPick.cpp
    src/engine/scene/queries/ScenePick.cpp

    src/engine/world/grid/GridMap.cpp
    src/engine/world/grid/Pathfinding.cpp
    src/engine/world/csg/Brush.cpp
    src/engine/world/csg/BrushMesh.cpp
    src/engine/world/csg/BrushOps.cpp
    src/engine/world/csg/CompileMap.cpp
    src/engine/world/csg/Primitives.cpp

    src/systems/animation/AnimationSystem.cpp
    src/systems/audio/AudioSystem.cpp
    src/systems/light/LightSystem.cpp
    src/systems/ai/NavSystem.cpp
    src/systems/particles/ParticleSystem.cpp
    src/systems/physics/PhysicsSystem.cpp
    src/systems/render/PostProcessPass.cpp
    src/systems/scripting/ScriptSystem.cpp
    src/systems/render/ShadowPass.cpp
    src/systems/physics/TriggerSystem.cpp
    src/systems/render/SkyboxRenderer.cpp

    src/engine/scripting/exposed/ExposedProperty.cpp
    src/engine/scripting/bindings/LuaBindings.cpp

    src/engine/render/backend/opengl/OpenGLMesh.cpp
    src/engine/render/backend/opengl/OpenGLShader.cpp
    src/engine/render/backend/opengl/OpenGLFramebuffer.cpp
    src/engine/render/backend/opengl/OpenGLRenderer.cpp
    src/engine/render/backend/opengl/OpenGLTexture.cpp
    src/engine/render/backend/opengl/OpenGLCubemapTexture.cpp
    src/engine/render/backend/opengl/OpenGLShadowMap.cpp
    src/engine/render/backend/opengl/OpenGLSSBO.cpp
    src/engine/render/backend/opengl/OpenGLDebugRenderer.cpp
    src/engine/render/backend/opengl/OpenGLInstanceBuffer.cpp
    src/engine/render/backend/opengl/OpenGLParticleRenderer.cpp

    src/engine/render/pipeline/LightGrid.cpp
    src/engine/render/scene_renderer/RenderBatching.cpp
    src/engine/render/scene_renderer/SceneRenderer.cpp

    src/engine/assets/cache/LodCache.cpp

    src/engine/scene/serialization/TilePersistence.cpp

    src/editor/workspace/WorkspaceManager.cpp
    src/editor/project/MapsManager.cpp
)

target_include_directories(mood_engine_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# PUBLIC: los consumidores (MoodEditor / MoodPlayer / mood_tests) heredan
# todas estas dependencias sin tener que repetirlas. SDL2/glm/spdlog
# aparecen en headers publicos del motor, asi que tienen que ser PUBLIC.
target_link_libraries(mood_engine_lib PUBLIC
    SDL2::SDL2
    glm::glm
    spdlog::spdlog
    imgui
    glad
    stb
    nlohmann_json::nlohmann_json
    EnTT::EnTT
    lua_static
    sol2::sol2
    miniaudio
    assimp::assimp
    Jolt
    meshoptimizer
    OpenGL::GL
)

# Tracy (F2H2): el target Tracy::TracyClient ya define TRACY_ENABLE PUBLIC,
# asi que cualquier consumidor que incluya core/Profiler.h y linke a
# mood_engine_lib activa las macros. Cuando MOOD_PROFILE=OFF, no se linkea
# Tracy y las macros caen al fallback no-op.
if(MOOD_PROFILE)
    target_link_libraries(mood_engine_lib PUBLIC Tracy::TracyClient)
endif()

mood_set_compiler_warnings(mood_engine_lib)

# ---------------------------------------------------------------------------
# Ejecutable principal: MoodEditor
# ---------------------------------------------------------------------------
add_executable(MoodEditor
    src/main.cpp

    src/editor/application/DemoSpawners.cpp
    src/editor/application/EditorApplication.cpp
    src/editor/application/EditorOverlay.cpp
    src/editor/application/EditorPlayMode.cpp
    src/editor/application/EditorProjectActions.cpp
    src/editor/application/EditorRenderPass.cpp
    src/editor/application/EditorScene.cpp
    src/editor/ui/EditorUI.cpp
    src/editor/ui/MenuBar.cpp
    src/editor/ui/StatusBar.cpp
    src/editor/ui/Dockspace.cpp
    src/editor/commands/HistoryStack.cpp
    src/editor/commands/EditTransformCommand.cpp
    src/editor/commands/DeleteEntityCommand.cpp
    src/editor/commands/CreateEntityCommand.cpp
    src/editor/commands/BooleanOpCommand.cpp
    src/editor/commands/EditBrushMaterialCommand.cpp
    src/editor/commands/EditBrushFaceMaterialCommand.cpp
    src/editor/commands/EditBrushUVCommand.cpp
    src/editor/commands/SetTileCommand.cpp
    src/editor/commands/EditMeshRendererMaterialCommand.cpp
    src/editor/commands/EditScriptComponentCommand.cpp
    src/editor/panels/scene/ViewportPanel.cpp
    src/editor/panels/scene/InspectorPanel.cpp
    src/editor/panels/assets/AssetBrowserPanel.cpp
    src/editor/panels/debug/ConsolePanel.cpp
    src/editor/panels/scene/HierarchyPanel.cpp
    src/editor/panels/scene/HierarchyCollect.cpp
    src/editor/panels/debug/LuaApiPanel.cpp
    src/editor/panels/debug/PerformanceHudPanel.cpp
    src/editor/panels/assets/ScriptEditorPanel.cpp
    src/editor/panels/assets/MaterialEditorPanel.cpp
)

target_link_libraries(MoodEditor PRIVATE
    mood_engine_lib
    pfd # solo el editor usa portable-file-dialogs
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
# Ejecutable secundario: MoodPlayer (Hito 21)
#
# Runtime standalone del juego. Comparte casi todo el codigo del motor
# con MoodEditor; lo unico que difiere es `src/player/` + ausencia de
# paneles ImGui. En Hito 21 Bloque 1 es un scaffolding (solo abre una
# ventana y cierra con Esc); los Bloques 2-4 le agregan render de
# escena, scripts, audio y carga de `game.json`.
# ---------------------------------------------------------------------------
add_executable(MoodPlayer
    src/player/main.cpp
    src/player/PlayerApplication.cpp
)

target_link_libraries(MoodPlayer PRIVATE
    mood_engine_lib
    pfd  # Hito 38 B: file dialogs para Save/Load Game
)

mood_set_compiler_warnings(MoodPlayer)

if(WIN32)
    add_custom_command(TARGET MoodPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:MoodPlayer>
        COMMENT "Copiando SDL2.dll junto a MoodPlayer"
    )
endif()

# Mismo deploy de shaders + assets que MoodEditor — para que el player
# arranque desde build/debug/Debug/MoodPlayer.exe sin tener que copiar
# nada a mano. Bloque 5 (PackageBuilder) usara estos mismos paths.
add_custom_command(TARGET MoodPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:MoodPlayer>/shaders
    COMMENT "Copiando shaders junto a MoodPlayer"
)
add_custom_command(TARGET MoodPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:MoodPlayer>/assets
    COMMENT "Copiando assets junto a MoodPlayer"
)

set_target_properties(MoodPlayer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Tests (doctest + CTest)
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
