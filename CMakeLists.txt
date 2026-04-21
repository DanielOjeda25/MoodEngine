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
    src/core/Time.cpp

    src/platform/Window.cpp

    src/engine/render/opengl/OpenGLMesh.cpp
    src/engine/render/opengl/OpenGLShader.cpp
    src/engine/render/opengl/OpenGLFramebuffer.cpp
    src/engine/render/opengl/OpenGLRenderer.cpp

    src/editor/EditorApplication.cpp
    src/editor/EditorUI.cpp
    src/editor/MenuBar.cpp
    src/editor/StatusBar.cpp
    src/editor/Dockspace.cpp
    src/editor/panels/ViewportPanel.cpp
    src/editor/panels/InspectorPanel.cpp
    src/editor/panels/AssetBrowserPanel.cpp
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

# Working directory por defecto para Visual Studio
set_target_properties(MoodEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Target por defecto al abrir la solucion en Visual Studio
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MoodEditor)
