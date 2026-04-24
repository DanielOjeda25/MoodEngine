#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/render/ITexture.h"
#include "engine/render/MeshAsset.h"
#include "engine/render/opengl/OpenGLDebugRenderer.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLMesh.h"
#include "engine/render/opengl/OpenGLRenderer.h"
#include "engine/render/opengl/OpenGLShader.h"
#include "engine/render/opengl/OpenGLTexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ViewportPick.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/audio/AudioDevice.h"
#include "systems/AudioSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/ScriptSystem.h"

#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL
// (evita redefiniciones con <SDL_opengl.h> o los loaders internos).
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Mood {

namespace {

constexpr const char* k_GlslVersion = "#version 450 core";

// Callback que recibe el driver para cada warning/error GL. Filtra por
// severidad y rutea al logger `render` con el nivel equivalente. Solo se
// activa si el contexto fue creado con el debug bit (builds Debug).
void APIENTRY glDebugCallback(GLenum /*source*/, GLenum type, GLuint id,
                               GLenum severity, GLsizei /*length*/,
                               const GLchar* message, const void* /*userParam*/) {
    // Filtrar notifications verbosos del driver (buffer info, etc.).
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    // Silenciar warnings conocidos que sabemos benignos y que spamearian la
    // Console cada frame. Si el driver cambia y empiezan a aparecer otros
    // mensajes relevantes, el callback los sigue capturando.
    if (std::strstr(message, "API_ID_LINE_WIDTH") != nullptr) {
        // glLineWidth(>1.0) esta deprecated en Core Profile pero todos los
        // drivers actuales lo honran. Lo usamos en el debug draw.
        return;
    }

    const char* typeStr = "other";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "error";       break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "deprecated";  break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "undefined";   break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "perf";        break;
        default: break;
    }

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        Mood::Log::render()->error("[GL {} #{}] {}", typeStr, id, message);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Mood::Log::render()->warn("[GL {} #{}] {}", typeStr, id, message);
    } else {
        Mood::Log::render()->debug("[GL {} #{}] {}", typeStr, id, message);
    }
}

// Dimensiones del AABB del jugador (0.6 x 1.8 x 0.6 m). Centrado en la
// posicion de la camara FPS. Escala SI realista: una persona promedio.
// El half-extent 0.3 m es muy superior al near clipping plane (0.1 m) asi
// que no hay riesgo de que el frustum atraviese los muros al pegarse.
constexpr glm::vec3 k_playerHalfExtents{0.3f, 0.9f, 0.3f};

} // namespace

EditorApplication::EditorApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodEngine iniciando...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodEngine Editor - v0.4.0-dev (Hito 4)";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    // --- GLAD ---
    if (!gladLoaderLoadGL()) {
        throw std::runtime_error("gladLoaderLoadGL fallo: no se pudo cargar OpenGL");
    }
    Log::render()->info("OpenGL iniciado: {}",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::render()->info("GPU: {} {}",
        reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    // Debug context callback: solo si el driver expone la extension y el
    // contexto fue creado con el flag (builds Debug). En Release el callback
    // no esta enganchado y no cuesta nada.
    {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // stacktrace util al paso
            glDebugMessageCallback(glDebugCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                                  0, nullptr, GL_TRUE);
            Log::render()->info("GL debug output enganchado");
        }
    }

    // --- Dear ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    // --- RHI + recursos del viewport offscreen ---
    m_renderer = std::make_unique<OpenGLRenderer>();
    m_renderer->init();

    m_viewportFb = std::make_unique<OpenGLFramebuffer>(1280u, 720u);

    m_defaultShader = std::make_unique<OpenGLShader>(
        "shaders/default.vert", "shaders/default.frag");

    // Factoria real: crea OpenGLTexture desde el path de filesystem. Los tests
    // pasan otra factoria que devuelve mocks sin tocar GL. El cubo fallback
    // ya no vive aca: el AssetManager lo genera via MeshFactory en el slot 0.
    m_assetManager = std::make_unique<AssetManager>(
        "assets",
        [](const std::string& fsPath) -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(fsPath);
        },
        /*audioFactory*/ AssetManager::AudioClipFactory{},
        /*meshFactory*/
        [](const std::vector<f32>& verts,
           const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
            return std::make_unique<OpenGLMesh>(verts, attrs);
        });
    m_ui.assetBrowser().setAssetManager(m_assetManager.get());

    m_debugRenderer = std::make_unique<OpenGLDebugRenderer>();

    m_ui.viewport().setFramebuffer(m_viewportFb.get());

    m_scene = std::make_unique<Scene>();
    m_scriptSystem = std::make_unique<ScriptSystem>();

    // Audio (Hito 9). El device tolera fallo de init (modo mute) — el motor
    // sigue funcionando aunque no haya hardware de audio.
    m_audioDevice = std::make_unique<AudioDevice>();
    m_audioSystem = std::make_unique<AudioSystem>(*m_audioDevice, *m_assetManager);

    // Inyectar dependencias en los paneles de ECS.
    m_ui.hierarchy().setScene(m_scene.get());
    m_ui.hierarchy().setEditorUi(&m_ui);
    m_ui.inspector().setEditorUi(&m_ui);
    m_ui.inspector().setAssetManager(m_assetManager.get());

    buildInitialTestMap();
    rebuildSceneFromMap();
    updateWindowTitle();

    MOOD_LOG_INFO("Editor listo");

    // Restaurar estado de la sesion anterior (ultimo proyecto, flags).
    // Silencioso si no hay archivo o si el ultimo proyecto desaparecio.
    loadEditorState();
}

void EditorApplication::buildInitialTestMap() {
    // 8x8 con bordes sólidos (grid.png) y columna central (brick.png) para
    // validar texturas por tile. Mismo estado que arrancaba el editor desde
    // el Hito 5; se reusa al cerrar un proyecto para volver a un baseline
    // conocido.
    m_map = GridMap(8u, 8u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");
    m_wallTextureId = grid;

    for (u32 i = 0; i < m_map.width(); ++i) {
        m_map.setTile(i, 0u,                   TileType::SolidWall, grid);
        m_map.setTile(i, m_map.height() - 1u,  TileType::SolidWall, grid);
    }
    for (u32 j = 0; j < m_map.height(); ++j) {
        m_map.setTile(0u,                  j, TileType::SolidWall, grid);
        m_map.setTile(m_map.width() - 1u,  j, TileType::SolidWall, grid);
    }
    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("Mapa de prueba cargado: prueba_8x8 ({} tiles solidos)",
                       m_map.solidCount());
}

void EditorApplication::rebuildSceneFromMap() {
    // Vision actual: Scene es derivada del GridMap. Cada tile solido =
    // 1 entidad con Transform + MeshRenderer. Limpiamos en-place el
    // registry (en vez de recrear el Scene completo) para no invalidar
    // los punteros que tienen los paneles Hierarchy/Inspector.
    if (!m_scene) m_scene = std::make_unique<Scene>();
    m_scene->registry().clear();
    m_ui.setSelectedEntity(Entity{}); // la seleccion apuntaba a un handle ya
                                       // destruido; invalidarla.
    // Los sol::state mapeados por entt::entity del ScriptSystem quedan
    // huerfanos: hay que tirarlos antes de recrear entidades con los
    // mismos handles numericos.
    if (m_scriptSystem) m_scriptSystem->clear();
    // AudioSystem::clear() -> AudioDevice::stopAll(): los sonidos de los
    // AudioSourceComponent de las entidades que estamos por destruir.
    if (m_audioSystem) m_audioSystem->clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

    for (u32 y = 0; y < m_map.height(); ++y) {
        for (u32 x = 0; x < m_map.width(); ++x) {
            if (!m_map.isSolid(x, y)) continue;

            const std::string name = "Tile_" + std::to_string(x) + "_" + std::to_string(y);
            Entity e = m_scene->createEntity(name);

            auto& t = e.getComponent<TransformComponent>();
            t.position = glm::vec3(
                origin.x + (static_cast<f32>(x) + 0.5f) * tileSize,
                origin.y + 0.5f * tileSize,
                origin.z + (static_cast<f32>(y) + 0.5f) * tileSize);
            t.scale = glm::vec3(tileSize);

            // Mesh id 0 = cubo primitivo generado por el AssetManager.
            // La textura del tile va como unico material (1 submesh = 1 textura).
            e.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(),
                m_map.tileTextureAt(x, y));
        }
    }
}

void EditorApplication::updateTileEntity(u32 tileX, u32 tileY, TextureAssetId texture) {
    if (!m_scene) return;
    const std::string name = "Tile_" + std::to_string(tileX) + "_" + std::to_string(tileY);

    // Busqueda linear sobre ~30 entidades. Suficiente hasta que los mapas
    // pasen de 16x16 o aparezca un Scene query indexado.
    Entity found{};
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(found) && tag.name == name) found = e;
    });

    if (static_cast<bool>(found)) {
        if (found.hasComponent<MeshRendererComponent>()) {
            auto& mr = found.getComponent<MeshRendererComponent>();
            if (mr.materials.empty()) mr.materials.push_back(texture);
            else mr.materials[0] = texture;
        } else {
            found.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(), texture);
        }
        return;
    }

    // La tile no tenia entidad (era Empty). Creamos una con los mismos
    // defaults que rebuildSceneFromMap.
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    Entity e = m_scene->createEntity(name);
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(
        origin.x + (static_cast<f32>(tileX) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize,
        origin.z + (static_cast<f32>(tileY) + 0.5f) * tileSize);
    t.scale = glm::vec3(tileSize);
    e.addComponent<MeshRendererComponent>(
        m_assetManager->missingMeshId(), texture);
}

void EditorApplication::updateWindowTitle() {
    std::string title = "MoodEngine Editor - v0.6.0-dev (Hito 6)";
    if (m_project.has_value()) {
        title = "MoodEngine Editor - " + m_project->name;
        if (m_projectDirty) title += " *";
    }
    SDL_SetWindowTitle(m_window->sdlHandle(), title.c_str());
    m_ui.setHasProject(m_project.has_value());
}

void EditorApplication::markDirty() {
    if (!m_project.has_value()) return;
    if (!m_projectDirty) {
        m_projectDirty = true;
        updateWindowTitle();
    }
}

bool EditorApplication::confirmDiscardChanges() {
    if (!m_projectDirty) return true;

    const auto choice = pfd::message(
        "MoodEngine — Cambios sin guardar",
        "Hay cambios sin guardar en el proyecto actual.\n\n"
        "Si - guardar antes de continuar\n"
        "No - descartar los cambios\n"
        "Cancelar - volver al editor",
        pfd::choice::yes_no_cancel,
        pfd::icon::question).result();

    switch (choice) {
        case pfd::button::yes:
            handleSave();
            // Si el save fallo (p.ej. disco lleno), m_projectDirty sigue
            // en true y abortamos la accion.
            return !m_projectDirty;
        case pfd::button::no:
            return true;
        case pfd::button::cancel:
        default:
            return false;
    }
}

void EditorApplication::handleNewProject() {
    // confirmDiscardChanges: sigue aplicando si el usuario ejecuta "Nuevo"
    // sobre un proyecto modificado sin guardar.
    if (!confirmDiscardChanges()) return;

    const auto cwd = std::filesystem::current_path().generic_string();
    const std::string saveTo = pfd::save_file(
        "Nuevo proyecto — nombre y ubicacion del .moodproj",
        cwd,
        {"Proyectos MoodEngine (*.moodproj)", "*.moodproj"},
        pfd::opt::none).result();
    if (saveTo.empty()) {
        Log::editor()->info("Nuevo proyecto: cancelado");
        return;
    }

    std::filesystem::path moodprojPath(saveTo);
    if (moodprojPath.extension() != ".moodproj") {
        moodprojPath += ".moodproj";
    }

    const std::filesystem::path root = moodprojPath.parent_path();
    const std::string name = moodprojPath.stem().generic_string();
    if (name.empty() || root.empty()) {
        pfd::message("MoodEngine", "Nombre o carpeta del proyecto invalidos.",
                     pfd::choice::ok, pfd::icon::warning);
        return;
    }

    auto created = ProjectSerializer::createNewProject(root, name);
    if (!created.has_value()) {
        pfd::message("MoodEngine", "No se pudo crear el proyecto (ver console/log).",
                     pfd::choice::ok, pfd::icon::error);
        return;
    }

    // Nuevo proyecto -> contenido inicial = mapa de prueba (template).
    buildInitialTestMap();
    rebuildSceneFromMap();

    const auto mapPath = created->root / created->defaultMap;
    std::filesystem::create_directories(mapPath.parent_path());
    SceneSerializer::save(m_map, created->name, m_scene.get(), *m_assetManager, mapPath);

    m_project = std::move(created);
    m_currentMapPath = m_project->defaultMap;
    m_projectDirty = false;
    addToRecentProjects(m_project->root / (m_project->name + ".moodproj"));
    updateWindowTitle();
    m_ui.setStatusMessage("Proyecto creado: " + m_project->name);
    Log::editor()->info("Proyecto creado: {} ({})",
        m_project->name,
        (m_project->root / (m_project->name + ".moodproj")).generic_string());
}

void EditorApplication::handleOpenProject() {
    if (!confirmDiscardChanges()) return;

    const auto cwd = std::filesystem::current_path().generic_string();
    const auto selection = pfd::open_file(
        "Abrir proyecto", cwd,
        {"Proyectos MoodEngine (*.moodproj)", "*.moodproj"},
        pfd::opt::none).result();
    if (selection.empty()) {
        Log::editor()->info("Abrir proyecto: cancelado");
        return;
    }

    if (!tryOpenProjectPath(std::filesystem::path(selection.front()))) {
        pfd::message("MoodEngine", "No se pudo abrir el proyecto (ver console/log).",
                     pfd::choice::ok, pfd::icon::error);
    }
}

bool EditorApplication::tryOpenProjectPath(const std::filesystem::path& moodproj) {
    if (!std::filesystem::exists(moodproj)) {
        Log::editor()->warn("Proyecto no existe en disco: {}", moodproj.generic_string());
        return false;
    }
    auto loaded = ProjectSerializer::load(moodproj);
    if (!loaded.has_value() || loaded->maps.empty()) {
        Log::editor()->warn("Proyecto invalido o sin mapas: {}", moodproj.generic_string());
        return false;
    }

    const auto mapPath = loaded->root / loaded->defaultMap;
    auto savedMap = SceneSerializer::load(mapPath, *m_assetManager);
    if (!savedMap.has_value()) {
        Log::editor()->warn("No se pudo cargar mapa default: {}", mapPath.generic_string());
        return false;
    }

    m_map = std::move(savedMap->map);
    rebuildSceneFromMap();
    // Entidades no-tile persistidas (Hito 10 Bloque 6): se aplican DESPUES
    // del rebuildSceneFromMap que crea las Tile_X_Y. Si no hay entidades
    // (archivo v1 o mapa sin meshes dropeados), el loop no hace nada.
    if (m_scene) {
        for (const auto& se : savedMap->entities) {
            Entity e = m_scene->createEntity(se.tag);
            auto& t = e.getComponent<TransformComponent>();
            t.position      = se.position;
            t.rotationEuler = se.rotationEuler;
            t.scale         = se.scale;
            if (se.meshRenderer.has_value()) {
                const auto& mr = *se.meshRenderer;
                const MeshAssetId meshId = m_assetManager->loadMesh(mr.meshPath);
                std::vector<TextureAssetId> mats;
                mats.reserve(mr.materials.size());
                for (const auto& texPath : mr.materials) {
                    mats.push_back(texPath.empty()
                        ? m_assetManager->missingTextureId()
                        : m_assetManager->loadTexture(texPath));
                }
                e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
            }
        }
    }
    m_project = std::move(loaded);
    m_currentMapPath = m_project->defaultMap;
    m_projectDirty = false;
    addToRecentProjects(std::filesystem::absolute(moodproj));
    updateWindowTitle();
    m_ui.setStatusMessage("Proyecto abierto: " + m_project->name);
    Log::editor()->info("Proyecto abierto: {}", m_project->name);
    return true;
}

void EditorApplication::addToRecentProjects(const std::filesystem::path& moodproj) {
    const auto canonical = std::filesystem::absolute(moodproj);
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [&canonical](const std::filesystem::path& p) {
                return std::filesystem::absolute(p) == canonical;
            }),
        m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), canonical);
    if (m_recentProjects.size() > 8) m_recentProjects.resize(8);
    m_ui.setRecentProjects(m_recentProjects);
}

void EditorApplication::handleSave() {
    // Modelo Unity/Godot: el editor siempre tiene un proyecto activo (el
    // modal Welcome bloquea la entrada al editor sin proyecto). Ctrl+S
    // siempre guarda sobre el proyecto actual.
    if (!m_project.has_value()) {
        Log::editor()->warn("Guardar: no hay proyecto activo (estado invalido)");
        return;
    }

    const auto mapPath = m_project->root / m_currentMapPath;
    std::filesystem::create_directories(mapPath.parent_path());
    try {
        SceneSerializer::save(m_map, m_currentMapPath.stem().generic_string(),
                              m_scene.get(), *m_assetManager, mapPath);
        ProjectSerializer::save(*m_project);
        m_projectDirty = false;
        updateWindowTitle();
        m_ui.setStatusMessage("Guardado: " + m_project->name);
        Log::editor()->info("Proyecto guardado ({}): {}",
            m_project->name, mapPath.generic_string());
    } catch (const std::exception& e) {
        Log::editor()->warn("Guardar fallo: {}", e.what());
        pfd::message("MoodEngine", std::string("Error al guardar: ") + e.what(),
                     pfd::choice::ok, pfd::icon::error);
    }
}

void EditorApplication::handleSaveAs() {
    // "Guardar como" completo queda diferido: requiere UI multi-mapa por
    // proyecto (el schema .moodproj lo soporta pero el editor todavia
    // opera sobre el defaultMap). Ver pendientes del Hito 6.
    if (m_project.has_value()) {
        pfd::message("MoodEngine — Guardar como",
                     "Guardar como todavia no esta implementado.\n"
                     "Usa Ctrl+S para guardar sobre el proyecto actual,\n"
                     "o Archivo > Cerrar proyecto y crear uno nuevo.",
                     pfd::choice::ok, pfd::icon::info);
    }
}

void EditorApplication::handleCloseProject() {
    if (!m_project.has_value()) return;
    if (!confirmDiscardChanges()) return;

    Log::editor()->info("Cerrando proyecto: {}", m_project->name);
    m_project.reset();
    m_currentMapPath.clear();
    m_projectDirty = false;
    // El mapa de prueba queda como "fondo" mientras el modal Welcome esta
    // abierto (el usuario lo ve borroso detras del popup). Ayuda visualmente
    // a distinguir "editor sin proyecto" vs "proyecto vacio".
    buildInitialTestMap();
    rebuildSceneFromMap();
    updateWindowTitle();
}

void EditorApplication::loadEditorState() {
    const auto path = std::filesystem::path(".mood") / "editor_state.json";
    std::ifstream in(path);
    if (!in.is_open()) return;

    nlohmann::json j;
    try { in >> j; } catch (...) { return; }

    // Preferencias globales.
    m_debugDraw = j.value("debugDraw", false);

    // Lista de proyectos recientes. El mas reciente esta al principio.
    m_recentProjects.clear();
    if (j.contains("recentProjects") && j["recentProjects"].is_array()) {
        for (const auto& p : j["recentProjects"]) {
            m_recentProjects.emplace_back(p.get<std::string>());
        }
    }
    m_ui.setRecentProjects(m_recentProjects);

    // Auto-restore del ultimo proyecto (convencion Unity/Godot). Si falla
    // — archivo borrado, JSON corrupto, etc. — caemos silenciosamente en
    // el modal Welcome para que el usuario elija.
    if (!m_recentProjects.empty()) {
        if (tryOpenProjectPath(m_recentProjects.front())) {
            return;
        }
        Log::editor()->info(
            "No se pudo auto-abrir el ultimo proyecto ({}); mostrando Welcome.",
            m_recentProjects.front().generic_string());
    }
}

void EditorApplication::saveEditorState() const {
    std::error_code ec;
    std::filesystem::create_directories(".mood", ec);
    if (ec) return;

    nlohmann::json j;
    j["debugDraw"] = m_debugDraw;
    j["recentProjects"] = nlohmann::json::array();
    for (const auto& p : m_recentProjects) {
        j["recentProjects"].push_back(p.generic_string());
    }

    std::ofstream out(std::filesystem::path(".mood") / "editor_state.json");
    if (!out.is_open()) return;
    out << j.dump(2);
}

EditorApplication::~EditorApplication() {
    // Persistir preferencias antes de tirar cualquier recurso. Falla silencio-
    // samente si no puede escribir — no queremos que un shutdown explote por
    // un archivo de estado corrupto.
    saveEditorState();

    // Los recursos GL dependen del contexto; liberar ANTES de destruir ImGui
    // y el contexto (el destructor del Window destruye el contexto al final).
    m_scriptSystem.reset(); // sol::state destructors; no dependen de GL
    // AudioSystem antes que AudioDevice: el system tiene &device pero no
    // toca el device en su destructor; stopAll lo llamamos antes via clear.
    m_audioSystem.reset();
    m_audioDevice.reset();
    m_debugRenderer.reset();
    m_assetManager.reset(); // dueño de las texturas y meshes (incluido el cubo fallback)
    m_defaultShader.reset();
    m_viewportFb.reset();
    m_renderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    gladLoaderUnloadGL();

    m_window.reset();
    SDL_Quit();

    MOOD_LOG_INFO("MoodEngine cerrado limpiamente");
    Log::shutdown();
}

void EditorApplication::processEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) {
            m_running = false;
        } else if (ev.type == SDL_WINDOWEVENT &&
                   ev.window.event == SDL_WINDOWEVENT_CLOSE &&
                   ev.window.windowID == SDL_GetWindowID(m_window->sdlHandle())) {
            m_running = false;
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_ESCAPE &&
                   m_mode == EditorMode::Play) {
            exitPlayMode();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_F1 &&
                   ev.key.repeat == 0) {
            m_debugDraw = !m_debugDraw;
            Log::editor()->info("Debug draw {}", m_debugDraw ? "activado" : "desactivado");
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_s &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Ctrl+S: atajo de Guardar. Emitimos la misma solicitud que el menu
            // para reusar el dispatcher unico.
            m_ui.requestProjectAction(ProjectAction::Save);
        }
    }
}

void EditorApplication::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void EditorApplication::endFrame() {
    ImGui::Render();

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    m_window->swapBuffers();
}

glm::vec3 EditorApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

void EditorApplication::enterPlayMode() {
    m_mode = EditorMode::Play;
    m_ui.setMode(EditorMode::Play);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    // Descartar cualquier delta acumulado para evitar un salto inicial de la vista.
    SDL_GetRelativeMouseState(nullptr, nullptr);
    Log::editor()->info("Play Mode activo (WASD + mouse. Esc para salir)");
}

void EditorApplication::exitPlayMode() {
    m_mode = EditorMode::Editor;
    m_ui.setMode(EditorMode::Editor);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    Log::editor()->info("Editor Mode activo");
}

void EditorApplication::updateCameras(f32 dt) {
    if (m_mode == EditorMode::Editor) {
        const float dx = m_ui.viewport().cameraRotateDx();
        const float dy = m_ui.viewport().cameraRotateDy();
        const float panDx = m_ui.viewport().cameraPanDx();
        const float panDy = m_ui.viewport().cameraPanDy();
        const float wheel = m_ui.viewport().cameraWheel();
        if (dx != 0.0f || dy != 0.0f) m_editorCamera.applyMouseDrag(dx, dy);
        if (panDx != 0.0f || panDy != 0.0f) m_editorCamera.applyPan(panDx, panDy);
        if (wheel != 0.0f) m_editorCamera.applyWheel(wheel);
    } else {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 dir(0.0f);
        if (keys[SDL_SCANCODE_W]) dir.z += 1.0f;
        if (keys[SDL_SCANCODE_S]) dir.z -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;
        if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
        if (keys[SDL_SCANCODE_SPACE]) dir.y += 1.0f;
        if (keys[SDL_SCANCODE_LSHIFT]) dir.y -= 1.0f;

        // Movimiento: delta deseado -> colisiones -> delta aplicado.
        const glm::vec3 desired = m_playCamera.computeMoveDelta(dir, dt);
        if (desired.x != 0.0f || desired.y != 0.0f || desired.z != 0.0f) {
            const glm::vec3 camPos = m_playCamera.position();
            AABB playerBox{camPos - k_playerHalfExtents, camPos + k_playerHalfExtents};
            const glm::vec3 actual = moveAndSlide(m_map, mapWorldOrigin(), playerBox, desired);
            m_playCamera.translate(actual);
        }

        int mx = 0;
        int my = 0;
        SDL_GetRelativeMouseState(&mx, &my);
        if (mx != 0 || my != 0) {
            m_playCamera.applyMouseMove(static_cast<float>(mx), static_cast<float>(my));
        }
    }
}

void EditorApplication::renderSceneToViewport(f32 dt) {
    const u32 desiredW = m_ui.viewport().desiredWidth();
    const u32 desiredH = m_ui.viewport().desiredHeight();
    if (desiredW > 0 && desiredH > 0) {
        m_viewportFb->resize(desiredW, desiredH);
    }

    m_viewportFb->bind();

    ClearValues clear{};
    clear.color = {0.07f, 0.12f, 0.22f, 1.0f};
    m_renderer->beginFrame(clear);

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;

    glm::mat4 view;
    glm::mat4 projection;
    if (m_mode == EditorMode::Play) {
        view = m_playCamera.viewMatrix();
        projection = m_playCamera.projectionMatrix(aspect);
    } else {
        view = m_editorCamera.viewMatrix();
        projection = m_editorCamera.projectionMatrix(aspect);
    }

    // El mapa se dibuja centrado en el origen del mundo (mapWorldOrigin()).
    // Mismo offset que consume PhysicsSystem: single source of truth.
    const glm::vec3 origin = mapWorldOrigin();
    (void)dt; // Ya no rotamos el modelo; el mapa es estatico.

    // Render scene-driven unificado (Hito 10 Bloque 4). Antes habia dos
    // pases: GridRenderer loopeando el GridMap + pase scene-driven filtrando
    // `Tile_`. Ahora `rebuildSceneFromMap` crea las entidades-tile ya con
    // MeshRendererComponent, y este unico loop las dibuja junto con modelos
    // importados / rotador demo / etc.
    // GridMap sigue existiendo como fuente authoritative de la topologia
    // (physics + save/load + rebuild); solo el render dejo de dependir de el.
    //
    // Itera por submeshes para soportar MeshAssets con multiples submeshes
    // (imports assimp). 1 draw call por submesh, textura segun materialIndex.
    if (m_scene) {
        m_defaultShader->bind();
        m_defaultShader->setMat4("uView", view);
        m_defaultShader->setMat4("uProjection", projection);
        m_defaultShader->setInt("uTexture", 0);

        m_scene->forEach<TransformComponent, MeshRendererComponent>(
            [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
                MeshAsset* asset = m_assetManager->getMesh(mr.mesh);
                if (asset == nullptr) return;
                m_defaultShader->setMat4("uModel", t.worldMatrix());
                for (usize i = 0; i < asset->submeshes.size(); ++i) {
                    const auto& sub = asset->submeshes[i];
                    if (sub.mesh == nullptr) continue;
                    // Fallback slot 0 (missing.png) si el array es mas corto
                    // que el numero de submeshes.
                    const TextureAssetId tex = mr.materialOrMissing(sub.materialIndex);
                    m_assetManager->getTexture(tex)->bind(0);
                    m_renderer->drawMesh(*sub.mesh, *m_defaultShader);
                }
            });
    }

    // Tile picking: solo en Editor Mode, cuando el cursor esta sobre la
    // imagen del viewport. Resultado se usa abajo para el hover highlight
    // y mas adelante para drag & drop desde el Asset Browser.
    TilePickResult hovered{};
    if (m_mode == EditorMode::Editor && m_ui.viewport().imageHovered()) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                            m_ui.viewport().mouseNdcY());
        hovered = pickTile(m_map, origin, view, projection, ndc);
    }
    m_hoveredTile = hovered;

    // Debug draw: AABBs de tiles solidos + AABB del jugador en Play Mode
    // (toggle con F1). Hover highlight del tile bajo el cursor (siempre
    // visible en Editor Mode). Todo se acumula y flushea al final.
    bool hasDebugGeometry = false;
    if (m_debugDraw) {
        const glm::vec3 tileColor(1.0f, 0.85f, 0.15f);
        for (u32 ty = 0; ty < m_map.height(); ++ty) {
            for (u32 tx = 0; tx < m_map.width(); ++tx) {
                if (!m_map.isSolid(tx, ty)) continue;
                const AABB local = m_map.aabbOfTile(tx, ty);
                const AABB world{origin + local.min, origin + local.max};
                m_debugRenderer->drawAabb(world, tileColor);
            }
        }
        if (m_mode == EditorMode::Play) {
            const glm::vec3 playerColor(0.2f, 1.0f, 0.4f);
            const glm::vec3 pos = m_playCamera.position();
            m_debugRenderer->drawAabb(
                AABB{pos - k_playerHalfExtents, pos + k_playerHalfExtents},
                playerColor);
        }
        hasDebugGeometry = true;
    }
    if (hovered.hit) {
        const glm::vec3 hoverColor(0.2f, 0.9f, 1.0f); // cyan
        const AABB local = m_map.aabbOfTile(hovered.tileX, hovered.tileY);
        const AABB world{origin + local.min, origin + local.max};
        m_debugRenderer->drawAabb(world, hoverColor);
        hasDebugGeometry = true;
    }
    if (hasDebugGeometry) {
        m_debugRenderer->flush(view, projection);
    }

    m_renderer->endFrame();
    m_viewportFb->unbind();
}

int EditorApplication::run() {
    m_deltaTimer.reset();

    while (m_running) {
        // Hot-reload de texturas solicitado el frame anterior (boton
        // Recargar del Asset Browser). Se aplica aca, entre frames, para
        // que ningun draw en curso use un GLuint recien destruido.
        if (m_ui.assetBrowser().consumeReloadRequest()) {
            const auto n = m_assetManager->reloadChanged();
            if (n > 0) {
                Log::editor()->info("Hot-reload: {} texturas actualizadas", n);
            }
        }

        const f64 dtD = m_deltaTimer.elapsedSeconds();
        const f32 dt = static_cast<f32>(dtD);
        m_deltaTimer.reset();

        const f32 fps = m_fpsCounter.tick(dtD);
        m_ui.setFps(fps);

        processEvents();
        beginFrame();

        // 1) Construir el widget tree de ImGui. ViewportPanel captura input
        //    de camara aqui (solo efectivo en Editor Mode).
        bool requestQuit = false;
        m_ui.draw(requestQuit);

        // 2) Atender toggles de modo solicitados desde la UI (boton Play/Stop).
        if (m_ui.consumeTogglePlayRequest()) {
            if (m_mode == EditorMode::Editor) {
                enterPlayMode();
            } else {
                exitPlayMode();
            }
        }

        // 2.25) Acciones de proyecto (Archivo > Nuevo / Abrir / Guardar / ...).
        switch (m_ui.consumeProjectAction()) {
            case ProjectAction::NewProject:   handleNewProject();   break;
            case ProjectAction::OpenProject:  handleOpenProject();  break;
            case ProjectAction::Save:         handleSave();         break;
            case ProjectAction::SaveAs:       handleSaveAs();       break;
            case ProjectAction::CloseProject: handleCloseProject(); break;
            case ProjectAction::None:         break;
        }

        // Modal Welcome: click en un reciente emite path directo en lugar
        // de pasar por el dialogo pfd.
        if (auto recentPath = m_ui.consumeOpenProjectPath(); recentPath.has_value()) {
            if (!tryOpenProjectPath(*recentPath)) {
                pfd::message("MoodEngine",
                    "No se pudo abrir el proyecto reciente '" +
                    recentPath->generic_string() + "'. Quedara resaltado en la "
                    "lista hasta que se cierre el editor.",
                    pfd::choice::ok, pfd::icon::warning);
            }
        }

        // Demo Hito 8: "Ayuda > Agregar rotador demo". Crea una entidad
        // flotante sobre el centro del mapa con ScriptComponent apuntando a
        // assets/scripts/rotator.lua. Util para validar el ScriptSystem sin
        // tocar el mapa ni el serializer.
        if (m_ui.consumeSpawnRotatorRequest() && m_scene) {
            Entity r = m_scene->createEntity("Rotador");
            auto& t = r.getComponent<TransformComponent>();
            t.position = glm::vec3(0.0f, 4.0f, 0.0f);
            t.scale = glm::vec3(1.0f);
            // Usa el cubo fallback del AssetManager (slot 0) como mesh del rotador.
            r.addComponent<MeshRendererComponent>(m_assetManager->missingMeshId(), m_wallTextureId);
            r.addComponent<ScriptComponent>(std::string{"assets/scripts/rotator.lua"});
            Log::editor()->info("Spawned rotador demo en (0, 4, 0)");
        }

        // Demo Hito 9: spawnea una entidad con AudioSourceComponent. Carga
        // el beep.wav via AssetManager (cacheado despues del primer spawn)
        // y lo ubica en una esquina del mapa (en world coords escala SI:
        // 1 unidad = 1 m, tileSize=3m, esquina ~ (-10, 1.5, -10)).
        if (m_ui.consumeSpawnAudioSourceRequest() && m_scene && m_assetManager) {
            const AudioAssetId beepId = m_assetManager->loadAudio("audio/beep.wav");
            Entity src = m_scene->createEntity("AudioSource demo");
            auto& t = src.getComponent<TransformComponent>();
            t.position = glm::vec3(-10.0f, 1.5f, -10.0f);
            // Sin MeshRenderer: es una entidad invisible (marcador audio).
            // El debug draw del Hito 4 solo dibuja tile colliders, no esto.
            AudioSourceComponent asrc{beepId};
            asrc.loop = true;
            asrc.is3D = true;
            asrc.playOnStart = true;
            asrc.volume = 0.8f;
            src.addComponent<AudioSourceComponent>(asrc);
            Log::editor()->info("Spawned audio source demo en (-10, 1.5, -10)");
        }

        // 2.5) Drop de textura desde AssetBrowser: usar el tile picking para
        //      convertir las NDC del drop en coords de tile y aplicar.
        const ViewportPanel::TextureDrop drop = m_ui.viewport().consumeTextureDrop();
        if (drop.pending && m_mode == EditorMode::Editor) {
            const float aspect = (m_viewportFb->height() > 0)
                ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
                : 1.0f;
            const glm::mat4 view = m_editorCamera.viewMatrix();
            const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
            const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                                glm::vec2(drop.ndcX, drop.ndcY));
            if (hit.hit) {
                const auto texId = static_cast<TextureAssetId>(drop.textureId);
                // Mantener el GridMap sincronizado (physics + serializer).
                m_map.setTile(hit.tileX, hit.tileY, TileType::SolidWall, texId);
                // Edit localizado en la Scene (preserva handles + seleccion).
                // Reemplaza el rebuildSceneFromMap completo que se hacia antes
                // del Hito 10 Bloque 4.
                updateTileEntity(hit.tileX, hit.tileY, texId);
                Log::editor()->info("Drop textura id={} -> tile ({}, {})",
                                     drop.textureId, hit.tileX, hit.tileY);
                markDirty();
            }
        }

        // 2.6) Drop de mesh desde AssetBrowser (Hito 10 Bloque 5): spawnea
        //      una entidad con MeshRendererComponent en el centro del tile
        //      bajo el cursor. Si el pick falla (cursor fuera del mapa), el
        //      drop se descarta silenciosamente.
        const ViewportPanel::MeshDrop mdrop = m_ui.viewport().consumeMeshDrop();
        if (mdrop.pending && m_mode == EditorMode::Editor && m_scene) {
            const float aspect = (m_viewportFb->height() > 0)
                ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
                : 1.0f;
            const glm::mat4 view = m_editorCamera.viewMatrix();
            const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
            const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                                glm::vec2(mdrop.ndcX, mdrop.ndcY));
            if (hit.hit) {
                const auto meshId = static_cast<MeshAssetId>(mdrop.meshId);
                const glm::vec3 origin = mapWorldOrigin();
                const f32 tileSize = m_map.tileSize();
                // Nombre incremental por si spawneamos varios del mismo mesh.
                const std::string meshName = m_assetManager->meshPathOf(meshId);
                Entity e = m_scene->createEntity("Mesh_" + meshName);
                auto& t = e.getComponent<TransformComponent>();
                t.position = glm::vec3(
                    origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
                    origin.y + 0.5f * tileSize, // sobre el piso, altura = 0.5 tile
                    origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);
                t.scale = glm::vec3(1.0f);
                // Material por defecto: slot 0 (missing.png). El usuario puede
                // dropear texturas encima despues. Un TextureAssetId por
                // submesh; los submeshes ausentes caen a slot 0 via
                // `materialOrMissing`.
                e.addComponent<MeshRendererComponent>(meshId, TextureAssetId{0});
                Log::editor()->info("Drop mesh '{}' -> tile ({}, {})",
                                     meshName, hit.tileX, hit.tileY);
                markDirty();
            }
        }

        // 3) Input -> camaras.
        updateCameras(dt);

        // 3.5) Scripts Lua: correr onUpdate(self, dt) en cada entidad con
        //      ScriptComponent. Antes del render para que los cambios en
        //      Transform se vean este mismo frame.
        if (m_scene && m_scriptSystem) {
            m_scriptSystem->update(*m_scene, dt);
        }

        // 3.6) Audio: arranca playOnStart, sincroniza posicion 3D, y setea
        //      el listener a la camara activa. Despues de scripts para que
        //      cualquier cambio de Transform del frame ya este aplicado.
        if (m_scene && m_audioSystem) {
            m_audioSystem->update(*m_scene, dt);

            // Listener = camara activa segun modo. World-up fijo (0,1,0).
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            if (m_mode == EditorMode::Play) {
                m_audioSystem->setListener(
                    m_playCamera.position(),
                    m_playCamera.forward(),
                    worldUp);
            } else {
                // Editor Mode: listener en la posicion de la orbit cam
                // mirando al origen (target por defecto del EditorCamera).
                const glm::vec3 camPos = m_editorCamera.position();
                const glm::vec3 fwd = glm::length(camPos) > 1e-5f
                    ? glm::normalize(-camPos)
                    : glm::vec3(0.0f, 0.0f, -1.0f);
                m_audioSystem->setListener(camPos, fwd, worldUp);
            }
        }

        // 4) Render 3D al framebuffer offscreen (el panel mostrara la textura).
        renderSceneToViewport(dt);

        // 5) Draw final de ImGui.
        endFrame();

        if (requestQuit) {
            m_running = false;
        }
    }

    return 0;
}

} // namespace Mood
