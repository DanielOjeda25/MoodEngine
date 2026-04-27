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
#include "engine/scene/ScenePick.h"
#include "engine/scene/ViewportPick.h"
#include "engine/serialization/PrefabSerializer.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/audio/AudioDevice.h"
#include "engine/physics/PhysicsWorld.h"
#include "systems/AudioSystem.h"
#include "systems/LightSystem.h"
#include "systems/PostProcessPass.h"
#include "systems/ShadowPass.h"
#include "systems/SkyboxRenderer.h"
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

    // Hito 15 Bloque 3: dos framebuffers para el viewport.
    //   - `m_sceneFb`: HDR (RGBA16F). Donde se pinta sky + escena + lit + fog.
    //     Soporta valores > 1.0 que el tonemap luego comprime.
    //   - `m_viewportFb`: LDR (RGBA8). Resultado del post-process pass; lo que
    //     ImGui muestra dentro del ViewportPanel (asume sRGB / RGBA8).
    m_sceneFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    m_viewportFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::LDR);

    m_defaultShader = std::make_unique<OpenGLShader>(
        "shaders/default.vert", "shaders/default.frag");
    // Hito 11: shader iluminado para todas las entidades con MeshRenderer.
    // El default queda vivo solo para emergencias (UI/debug si aparece).
    m_litShader = std::make_unique<OpenGLShader>(
        "shaders/lit.vert", "shaders/lit.frag");

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

    // Skybox (Hito 15 Bloque 1): cubemap default `assets/skyboxes/sky_day/`.
    // Tolera fallo de carga (en CI sin assets, o si el path cambia): el
    // motor sigue corriendo con clear color visible.
    try {
        m_skyboxRenderer =
            std::make_unique<SkyboxRenderer>("assets/skyboxes/sky_day");
    } catch (const std::exception& e) {
        Log::render()->warn("SkyboxRenderer no disponible: {}. Sky fallback al clear color.",
                             e.what());
        m_skyboxRenderer.reset();
    }

    m_ui.viewport().setFramebuffer(m_viewportFb.get());

    // Post-process pass (Hito 15 Bloque 3). Toma `m_sceneFb` (HDR) y
    // produce `m_viewportFb` (LDR) tras exposicion + tonemap + gamma.
    m_postProcess = std::make_unique<PostProcessPass>();

    // Shadow pass (Hito 16). Owns un depth FB + shader shadow_depth.
    // El render pass lo usa antes del lit cuando hay una directional con
    // `castShadows=true`.
    try {
        m_shadowPass = std::make_unique<ShadowPass>(2048);
    } catch (const std::exception& e) {
        Log::render()->warn("ShadowPass no disponible: {}. Sombras off.",
                             e.what());
        m_shadowPass.reset();
    }

    m_scene = std::make_unique<Scene>();
    m_scriptSystem = std::make_unique<ScriptSystem>();
    m_lightSystem  = std::make_unique<LightSystem>();
    // Hito 12: PhysicsWorld (Jolt). Init es pesado la primera vez del proceso
    // (Factory + RegisterTypes) pero instancias subsecuentes son baratas.
    m_physicsWorld = std::make_unique<PhysicsWorld>();

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

    // Hito 13 Bloque 2.5 + 3: overlay 2D sobre el viewport.
    // - Iconos para entidades sin mesh visible (Light/Audio).
    // - Halo de seleccion.
    // - Translate gizmo: 3 flechas (X rojo, Y verde, Z azul) arrastrables.
    m_ui.viewport().setOverlayDraw(
        [this](ImDrawList* dl, float x0, float y0, float w, float h) {
            if (!m_scene) return;
            // Play Mode: sin overlays de editor (iconos, gizmos, halo).
            // El jugador no deberia ver marcadores de seleccion en el juego.
            if (m_mode == EditorMode::Play) return;
            const glm::mat4 vp = m_lastProjection * m_lastView;

            // Helper: worldPos -> pixel-space.
            auto project = [&](const glm::vec3& p, float& sx, float& sy) -> bool {
                const glm::vec4 clip = vp * glm::vec4(p, 1.0f);
                if (clip.w <= 1e-4f) return false;
                const float ndcX = clip.x / clip.w;
                const float ndcY = clip.y / clip.w;
                const float ndcZ = clip.z / clip.w;
                if (ndcZ < -1.0f || ndcZ > 1.0f) return false;
                sx = x0 + (ndcX * 0.5f + 0.5f) * w;
                sy = y0 + (1.0f - (ndcY * 0.5f + 0.5f)) * h;
                return true;
            };
            // Helper: mouse screen -> camera ray (origen + direccion).
            auto cameraRay = [&](ImVec2 mp, glm::vec3& rorigin, glm::vec3& rdir) -> bool {
                const float ndcX = ((mp.x - x0) / w) * 2.0f - 1.0f;
                const float ndcY = 1.0f - ((mp.y - y0) / h) * 2.0f;
                const glm::mat4 invVP = glm::inverse(vp);
                const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                const glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY, +1.0f, 1.0f);
                if (nearH.w == 0.0f || farH.w == 0.0f) return false;
                rorigin = glm::vec3(nearH) / nearH.w;
                const glm::vec3 farW = glm::vec3(farH) / farH.w;
                const glm::vec3 d = farW - rorigin;
                const float len = glm::length(d);
                if (len < 1e-6f) return false;
                rdir = d / len;
                return true;
            };
            // Parametro del punto de lineA (pA + dA*t) mas cercano al rayo
            // lineB (pB + dB*t). dA y dB deben ser unitarios.
            auto closestParam = [](const glm::vec3& pA, const glm::vec3& dA,
                                    const glm::vec3& pB, const glm::vec3& dB) -> f32 {
                const glm::vec3 n = pA - pB;
                const f32 b  = glm::dot(dA, dB);
                const f32 d1 = glm::dot(dA, n);
                const f32 d2 = glm::dot(dB, n);
                const f32 denom = 1.0f - b * b;
                if (std::abs(denom) < 1e-5f) return 0.0f; // paralelas
                return (b * d2 - d1) / denom;
            };

            const ImU32 colLight = IM_COL32(255, 225, 100, 220);
            const ImU32 colAudio = IM_COL32(100, 220, 230, 220);
            const ImU32 colBorder = IM_COL32(20, 20, 20, 255);
            const ImU32 colSel = IM_COL32(30, 180, 255, 255);

            Entity selected = m_ui.selectedEntity();
            const auto selectedHandle = selected ? selected.handle() : entt::entity{entt::null};

            // --- 1) Iconos + halo por entidad ---
            m_scene->forEach<TransformComponent>(
                [&](Entity e, TransformComponent& t) {
                    const bool hasMesh = e.hasComponent<MeshRendererComponent>();
                    const bool hasLight = e.hasComponent<LightComponent>();
                    const bool hasAudio = e.hasComponent<AudioSourceComponent>();
                    const bool needsIcon = (hasLight || hasAudio) && !hasMesh;
                    const bool isSelected = (selectedHandle != entt::entity{entt::null})
                                             && (e.handle() == selectedHandle);
                    if (!needsIcon && !isSelected) return;

                    float sx, sy;
                    if (!project(t.position, sx, sy)) return;

                    if (needsIcon) {
                        const ImVec2 c(sx, sy);
                        if (hasLight) {
                            const auto& lc = e.getComponent<LightComponent>();
                            // Estilo inspirado en Blender: el icono distingue
                            // a simple vista entre Point (anillo con puntos)
                            // y Sun (centro con rayos). Los outlines negros
                            // mantienen legibilidad contra cielos claros.
                            constexpr f32 kTwoPi = 6.28318530718f;
                            if (lc.type == LightComponent::Type::Point) {
                                const f32 rOuter = 11.0f;
                                const f32 rDots  = 7.0f;
                                const f32 rCore  = 2.5f;
                                dl->AddCircle(c, rOuter, colLight, 24, 2.0f);
                                dl->AddCircle(c, rOuter, colBorder, 24, 0.8f);
                                for (int i = 0; i < 8; ++i) {
                                    const f32 a = (f32)i * kTwoPi / 8.0f;
                                    const ImVec2 p(c.x + std::cos(a) * rDots,
                                                   c.y + std::sin(a) * rDots);
                                    dl->AddCircleFilled(p, 1.6f, colLight, 8);
                                }
                                dl->AddCircleFilled(c, rCore, colLight, 12);
                            } else { // Directional / Sun
                                const f32 rCore     = 3.5f;
                                const f32 rayInner  = 6.5f;
                                const f32 rayOuter  = 11.0f;
                                dl->AddCircleFilled(c, rCore, colLight, 16);
                                dl->AddCircle(c, rCore, colBorder, 16, 0.8f);
                                for (int i = 0; i < 8; ++i) {
                                    const f32 a = (f32)i * kTwoPi / 8.0f
                                                + kTwoPi / 16.0f; // offset 22.5
                                    const ImVec2 p1(c.x + std::cos(a) * rayInner,
                                                    c.y + std::sin(a) * rayInner);
                                    const ImVec2 p2(c.x + std::cos(a) * rayOuter,
                                                    c.y + std::sin(a) * rayOuter);
                                    dl->AddLine(p1, p2, colBorder, 3.0f);
                                    dl->AddLine(p1, p2, colLight,  1.8f);
                                }
                                // Linea de direccion: corta proyeccion del
                                // vector lc.direction desde la posicion de
                                // la entidad. Indica visualmente hacia donde
                                // apunta el sol sin necesidad de gizmo.
                                glm::vec3 dir = lc.direction;
                                if (glm::length(dir) > 1e-4f) {
                                    dir = glm::normalize(dir);
                                    const glm::vec3 endW = t.position + dir * 2.0f;
                                    f32 ex, ey;
                                    if (project(endW, ex, ey)) {
                                        dl->AddLine(c, ImVec2(ex, ey), colBorder, 3.0f);
                                        dl->AddLine(c, ImVec2(ex, ey), colLight,  1.5f);
                                    }
                                }
                            }
                        } else { // Audio
                            const f32 r = 10.0f;
                            dl->AddCircleFilled(c, r, colAudio, 24);
                            dl->AddCircle(c, r, colBorder, 24, 1.5f);
                        }
                    }

                    if (isSelected) {
                        const float r = needsIcon ? 14.0f : 12.0f;
                        dl->AddCircle(ImVec2(sx, sy), r, colSel, 24, 2.0f);
                    }
                });

            // --- 2) Gizmo (translate / rotate / scale) para la seleccion ---

            // Hotkeys W/E/R estilo Unity + `.` estilo Blender ("frame
            // selected"). Ignorar si ImGui esta capturando texto (p.ej. un
            // InputText del Inspector esta focuseado).
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_gizmoMode = GizmoMode::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_gizmoMode = GizmoMode::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_gizmoMode = GizmoMode::Scale;
                if (ImGui::IsKeyPressed(ImGuiKey_Period, false) &&
                    selected && selected.hasComponent<TransformComponent>()) {
                    const auto& tf = selected.getComponent<TransformComponent>();
                    // Bounding sphere approx: para entidades con mesh,
                    // usar la diagonal del scale como radio (cubrir cubos
                    // y meshes razonables). Para Light/Audio sin mesh,
                    // un radio chico para que el zoom sea cercano sin
                    // pegarse al icono.
                    const f32 r = selected.hasComponent<MeshRendererComponent>()
                        ? glm::length(tf.scale) * 0.6f
                        : 1.0f;
                    m_editorCamera.focusOn(tf.position, std::max(r, 0.3f));
                }
            }

            if (!selected || !selected.hasComponent<TransformComponent>()) return;

            auto& tform = selected.getComponent<TransformComponent>();
            const bool hasMesh = selected.hasComponent<MeshRendererComponent>();

            // Rotate/Scale sobre entidades sin mesh caen a Translate: el
            // Inspector oculta rotation/scale para esas (Light/Audio) y
            // queremos consistencia UX.
            GizmoMode effectiveMode = m_gizmoMode;
            if (!hasMesh && effectiveMode != GizmoMode::Translate) {
                effectiveMode = GizmoMode::Translate;
            }

            const glm::vec3 worldOrigin = tform.position;
            f32 osx, osy;
            if (!project(worldOrigin, osx, osy)) return;

            const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            const ImU32 axisCol[3] = {
                IM_COL32(230, 50, 50, 230),   // X rojo
                IM_COL32(80, 230, 80, 230),   // Y verde
                IM_COL32(70, 120, 255, 230),  // Z azul
            };
            const ImU32 hoverCol = IM_COL32(255, 220, 30, 255);
            const f32 k_armLen = 60.0f;
            const f32 k_ringRad = 55.0f;

            const ImVec2 mousePos = ImGui::GetMousePos();
            const bool mouseDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const bool insideViewport = mousePos.x >= x0 && mousePos.x <= x0 + w
                                     && mousePos.y >= y0 && mousePos.y <= y0 + h;

            // ====== MODO TRANSLATE / SCALE (ambos usan handles por eje) ======
            if (effectiveMode == GizmoMode::Translate ||
                effectiveMode == GizmoMode::Scale) {

                f32 screenDX[3] = {0, 0, 0};
                f32 screenDY[3] = {0, 0, 0};
                f32 endSX[3] = {osx, osx, osx};
                f32 endSY[3] = {osy, osy, osy};
                bool axisVisible[3] = {false, false, false};
                for (int i = 0; i < 3; ++i) {
                    f32 esx, esy;
                    if (!project(worldOrigin + axes[i] * 1.0f, esx, esy)) continue;
                    const f32 dx = esx - osx;
                    const f32 dy = esy - osy;
                    const f32 lenPx = std::sqrt(dx * dx + dy * dy);
                    if (lenPx < 1e-3f) continue;
                    screenDX[i] = dx / lenPx;
                    screenDY[i] = dy / lenPx;
                    endSX[i] = osx + screenDX[i] * k_armLen;
                    endSY[i] = osy + screenDY[i] * k_armLen;
                    axisVisible[i] = true;
                }

                int hoverAxis = -1;
                if (!m_gizmo.active) {
                    // En modo Scale, el handle central (axis=3) escala
                    // uniformemente. Hit-test primero (gana al per-axis si
                    // el mouse esta encima del centro).
                    if (effectiveMode == GizmoMode::Scale) {
                        const f32 dx = mousePos.x - osx;
                        const f32 dy = mousePos.y - osy;
                        if (std::sqrt(dx * dx + dy * dy) < 8.0f) {
                            hoverAxis = 3;
                        }
                    }
                    if (hoverAxis < 0) {
                        f32 bestDist = 7.0f;
                        for (int i = 0; i < 3; ++i) {
                            if (!axisVisible[i]) continue;
                            const f32 dx = endSX[i] - osx;
                            const f32 dy = endSY[i] - osy;
                            const f32 len2 = dx * dx + dy * dy;
                            if (len2 < 1.0f) continue;
                            const f32 px = mousePos.x - osx;
                            const f32 py = mousePos.y - osy;
                            const f32 t = std::max(0.0f, std::min(1.0f, (px * dx + py * dy) / len2));
                            const f32 qx = osx + t * dx - mousePos.x;
                            const f32 qy = osy + t * dy - mousePos.y;
                            const f32 d = std::sqrt(qx * qx + qy * qy);
                            if (d < bestDist) { bestDist = d; hoverAxis = i; }
                        }
                    }
                }

                if (!m_gizmo.active && mouseClicked && hoverAxis >= 0 && insideViewport) {
                    if (hoverAxis == 3) {
                        // Uniform scale: param = distancia 2D del mouse al centro.
                        m_gizmo.active = true;
                        m_gizmo.axis = 3;
                        m_gizmo.startValue = tform.scale;
                        const f32 dx = mousePos.x - osx;
                        const f32 dy = mousePos.y - osy;
                        m_gizmo.startParam = std::sqrt(dx * dx + dy * dy);
                        m_gizmoConsumedClick = true;
                    } else {
                        glm::vec3 rayOrigin, rayDir;
                        if (cameraRay(mousePos, rayOrigin, rayDir)) {
                            m_gizmo.active = true;
                            m_gizmo.axis = hoverAxis;
                            m_gizmo.startValue = (effectiveMode == GizmoMode::Translate)
                                ? tform.position : tform.scale;
                            m_gizmo.startParam = closestParam(worldOrigin, axes[hoverAxis],
                                                               rayOrigin, rayDir);
                            m_gizmoConsumedClick = true;
                        }
                    }
                }

                if (m_gizmo.active && mouseDown && m_gizmo.axis >= 0) {
                    if (m_gizmo.axis == 3) {
                        // Uniform scale: factor = 1 + (delta_pixels / k_armLen).
                        // Mover el cursor k_armLen px afuera del centro duplica
                        // la escala; mover hacia el centro la divide.
                        const f32 dx = mousePos.x - osx;
                        const f32 dy = mousePos.y - osy;
                        const f32 nowParam = std::sqrt(dx * dx + dy * dy);
                        const f32 deltaPx = nowParam - m_gizmo.startParam;
                        const f32 factor = std::max(0.01f, 1.0f + deltaPx / k_armLen);
                        tform.scale = m_gizmo.startValue * factor;
                        if (m_project) markDirty();
                    } else {
                        glm::vec3 rayOrigin, rayDir;
                        if (cameraRay(mousePos, rayOrigin, rayDir)) {
                            const glm::vec3 axisPoint = (effectiveMode == GizmoMode::Translate)
                                ? m_gizmo.startValue : worldOrigin;
                            const f32 nowParam = closestParam(
                                axisPoint, axes[m_gizmo.axis], rayOrigin, rayDir);
                            const f32 delta = nowParam - m_gizmo.startParam;
                            if (effectiveMode == GizmoMode::Translate) {
                                tform.position = m_gizmo.startValue + axes[m_gizmo.axis] * delta;
                            } else {
                                glm::vec3 ns = m_gizmo.startValue;
                                ns[m_gizmo.axis] = std::max(0.01f, ns[m_gizmo.axis] + delta);
                                tform.scale = ns;
                            }
                            if (m_project) markDirty();
                        }
                    }
                }

                if (m_gizmo.active && !mouseDown) m_gizmo.active = false;

                // Draw.
                // Handle central de uniform scale (cuadrado en el origen).
                if (effectiveMode == GizmoMode::Scale) {
                    const bool isActive = m_gizmo.active && m_gizmo.axis == 3;
                    const bool isHover  = !m_gizmo.active && hoverAxis == 3;
                    const ImU32 ccol = (isActive || isHover) ? hoverCol
                                       : IM_COL32(220, 220, 220, 230);
                    const f32 cs = 5.0f;
                    dl->AddRectFilled(ImVec2(osx - cs, osy - cs),
                                       ImVec2(osx + cs, osy + cs), ccol);
                    dl->AddRect(ImVec2(osx - cs, osy - cs),
                                 ImVec2(osx + cs, osy + cs),
                                 IM_COL32(20, 20, 20, 255));
                }
                for (int i = 0; i < 3; ++i) {
                    if (!axisVisible[i]) continue;
                    ImU32 col = axisCol[i];
                    const bool isActive = m_gizmo.active && m_gizmo.axis == i;
                    const bool isHover  = !m_gizmo.active && hoverAxis == i;
                    if (isActive || isHover) col = hoverCol;
                    dl->AddLine(ImVec2(osx, osy), ImVec2(endSX[i], endSY[i]), col, 2.5f);

                    const f32 perpX = -screenDY[i];
                    const f32 perpY =  screenDX[i];
                    if (effectiveMode == GizmoMode::Translate) {
                        // Flecha triangular al final.
                        const f32 ah = 10.0f;
                        const f32 aw = 5.0f;
                        const f32 bx = endSX[i] - screenDX[i] * ah;
                        const f32 by = endSY[i] - screenDY[i] * ah;
                        dl->AddTriangleFilled(
                            ImVec2(endSX[i], endSY[i]),
                            ImVec2(bx + perpX * aw, by + perpY * aw),
                            ImVec2(bx - perpX * aw, by - perpY * aw),
                            col);
                    } else {
                        // Scale: cuadrado en el extremo.
                        const f32 s = 5.0f;
                        dl->AddQuadFilled(
                            ImVec2(endSX[i] + perpX * s,   endSY[i] + perpY * s),
                            ImVec2(endSX[i] + screenDX[i] * s*2 + perpX * s,
                                   endSY[i] + screenDY[i] * s*2 + perpY * s),
                            ImVec2(endSX[i] + screenDX[i] * s*2 - perpX * s,
                                   endSY[i] + screenDY[i] * s*2 - perpY * s),
                            ImVec2(endSX[i] - perpX * s,   endSY[i] - perpY * s),
                            col);
                    }
                }
            }
            // ====== MODO ROTATE (3 anillos eje-alineados) ======
            else {
                // Samplear cada anillo como N puntos del circulo en el plano
                // perpendicular al eje. Proyectar cada punto a screen-space.
                // Hit-test: distancia minima 2D entre mouse y cualquier sample.
                constexpr int k_ringSamples = 48;
                f32 ringSX[3][k_ringSamples];
                f32 ringSY[3][k_ringSamples];
                bool ringVisible[3] = {true, true, true};

                for (int i = 0; i < 3; ++i) {
                    // Base vectors en el plano perpendicular al eje i.
                    glm::vec3 u, v;
                    if (i == 0)      { u = {0, 1, 0}; v = {0, 0, 1}; }
                    else if (i == 1) { u = {1, 0, 0}; v = {0, 0, 1}; }
                    else             { u = {1, 0, 0}; v = {0, 1, 0}; }

                    bool anyHit = false;
                    for (int s = 0; s < k_ringSamples; ++s) {
                        const f32 a = 2.0f * 3.1415926f * static_cast<f32>(s)
                                       / static_cast<f32>(k_ringSamples);
                        // Radio 1m world-space. La conversion a screen via
                        // project() da un radio variable segun zoom, buscado
                        // (Blender hace lo mismo: scale con el zoom).
                        const glm::vec3 p = worldOrigin + (u * std::cos(a) + v * std::sin(a));
                        f32 sx, sy;
                        if (!project(p, sx, sy)) {
                            ringSX[i][s] = osx; ringSY[i][s] = osy; continue;
                        }
                        ringSX[i][s] = sx; ringSY[i][s] = sy;
                        anyHit = true;
                    }
                    ringVisible[i] = anyHit;
                }

                int hoverAxis = -1;
                if (!m_gizmo.active) {
                    f32 bestDist = 8.0f;
                    for (int i = 0; i < 3; ++i) {
                        if (!ringVisible[i]) continue;
                        for (int s = 0; s < k_ringSamples; ++s) {
                            const f32 dx = ringSX[i][s] - mousePos.x;
                            const f32 dy = ringSY[i][s] - mousePos.y;
                            const f32 d = std::sqrt(dx * dx + dy * dy);
                            if (d < bestDist) { bestDist = d; hoverAxis = i; }
                        }
                    }
                }

                if (!m_gizmo.active && mouseClicked && hoverAxis >= 0 && insideViewport) {
                    const f32 startAng = std::atan2(mousePos.y - osy, mousePos.x - osx);
                    m_gizmo.active = true;
                    m_gizmo.axis = hoverAxis;
                    m_gizmo.startValue = tform.rotationEuler;
                    m_gizmo.startParam = startAng;
                    m_gizmoConsumedClick = true;
                }

                if (m_gizmo.active && mouseDown && m_gizmo.axis >= 0) {
                    const f32 nowAng = std::atan2(mousePos.y - osy, mousePos.x - osx);
                    f32 deltaRad = nowAng - m_gizmo.startParam;
                    // Unwrap: mantener la acumulacion suave al cruzar el pi / -pi.
                    while (deltaRad >  3.1415926f) deltaRad -= 2.0f * 3.1415926f;
                    while (deltaRad < -3.1415926f) deltaRad += 2.0f * 3.1415926f;
                    const f32 deltaDeg = deltaRad * (180.0f / 3.1415926f);
                    glm::vec3 nr = m_gizmo.startValue;
                    // Signo: si el eje apunta "hacia la camara", la rotacion
                    // en sentido horario del mouse equivale a sentido antihor
                    // alrededor del eje (regla de la mano derecha vista desde +axis).
                    // Aprox: invertir si axis . camera_forward > 0 — el eje
                    // apunta hacia donde mira la camara.
                    const glm::vec3 camPos = glm::vec3(glm::inverse(m_lastView)[3]);
                    const glm::vec3 camToOrigin = worldOrigin - camPos;
                    const f32 sgn = (glm::dot(axes[m_gizmo.axis], camToOrigin) > 0.0f) ? 1.0f : -1.0f;
                    nr[m_gizmo.axis] = m_gizmo.startValue[m_gizmo.axis] + sgn * deltaDeg;
                    tform.rotationEuler = nr;
                    if (m_project) markDirty();
                }

                if (m_gizmo.active && !mouseDown) m_gizmo.active = false;

                // Draw rings as polylines.
                for (int i = 0; i < 3; ++i) {
                    if (!ringVisible[i]) continue;
                    ImU32 col = axisCol[i];
                    const bool isActive = m_gizmo.active && m_gizmo.axis == i;
                    const bool isHover  = !m_gizmo.active && hoverAxis == i;
                    if (isActive || isHover) col = hoverCol;
                    for (int s = 0; s < k_ringSamples; ++s) {
                        const int n = (s + 1) % k_ringSamples;
                        dl->AddLine(ImVec2(ringSX[i][s], ringSY[i][s]),
                                     ImVec2(ringSX[i][n], ringSY[i][n]),
                                     col, 2.0f);
                    }
                }
                (void)k_ringRad; // silenciar warning si no se usa
            }
        });

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

// `applyEnvironmentFromScene` movido a `EditorRenderPass.cpp` (Hito 16).

void EditorApplication::rebuildSceneFromMap() {
    // Vision actual: Scene es derivada del GridMap. Cada tile solido =
    // 1 entidad con Transform + MeshRenderer. Limpiamos en-place el
    // registry (en vez de recrear el Scene completo) para no invalidar
    // los punteros que tienen los paneles Hierarchy/Inspector.
    if (!m_scene) m_scene = std::make_unique<Scene>();
    // Hito 12: antes del clear, destruir los bodies de Jolt asociados a
    // RigidBodyComponent — tras registry.clear() perdemos los componentes
    // y los bodyIds quedarian huerfanos en PhysicsWorld.
    if (m_physicsWorld) {
        m_scene->forEach<RigidBodyComponent>([&](Entity, RigidBodyComponent& rb) {
            if (rb.bodyId != 0) {
                m_physicsWorld->destroyBody(rb.bodyId);
                rb.bodyId = 0;
            }
        });
    }
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

            // Hito 12: cada tile solido tambien es un static body en Jolt
            // (Box de tileSize/2 en los 3 ejes). updateRigidBodies materializa
            // el body en el proximo frame.
            e.addComponent<RigidBodyComponent>(
                RigidBodyComponent::Type::Static,
                RigidBodyComponent::Shape::Box,
                glm::vec3(tileSize * 0.5f),
                0.0f);
        }
    }
}

void EditorApplication::updateRigidBodies(f32 dt) {
    if (!m_scene || !m_physicsWorld) return;

    // 1) Materializar bodies nuevos (bodyId==0) con los valores iniciales
    //    de la entidad.
    m_scene->forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
            if (rb.bodyId != 0) return;
            CollisionShape shape = CollisionShape::Box;
            switch (rb.shape) {
                case RigidBodyComponent::Shape::Box:     shape = CollisionShape::Box; break;
                case RigidBodyComponent::Shape::Sphere:  shape = CollisionShape::Sphere; break;
                case RigidBodyComponent::Shape::Capsule: shape = CollisionShape::Capsule; break;
            }
            BodyType type = BodyType::Static;
            switch (rb.type) {
                case RigidBodyComponent::Type::Static:    type = BodyType::Static; break;
                case RigidBodyComponent::Type::Kinematic: type = BodyType::Kinematic; break;
                case RigidBodyComponent::Type::Dynamic:   type = BodyType::Dynamic; break;
            }
            rb.bodyId = m_physicsWorld->createBody(t.position, shape,
                                                    rb.halfExtents, type, rb.mass);
        });

    // 2) Stepear la simulacion SOLO en Play Mode. En Editor Mode los bodies
    //    existen (para debug draw futuro) pero no se mueven.
    if (m_mode == EditorMode::Play) {
        m_physicsWorld->step(dt);
        // 3) Sync body -> Transform para bodies dinamicos. Los Static no se
        //    mueven, se salta el read para ahorrar calls.
        m_scene->forEach<TransformComponent, RigidBodyComponent>(
            [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
                if (rb.type == RigidBodyComponent::Type::Static) return;
                if (rb.bodyId == 0) return;
                t.position = m_physicsWorld->bodyPosition(rb.bodyId);
            });
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

// Project handlers (confirmDiscardChanges, handleNewProject, handleOpenProject,
// tryOpenProjectPath, addToRecentProjects, handleSave, handleSaveAs,
// handleCloseProject, loadEditorState, saveEditorState) movidos a
// `EditorProjectActions.cpp` (Hito 16 refactor).


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
    m_lightSystem.reset();
    m_physicsWorld.reset();
    m_skyboxRenderer.reset();
    m_shadowPass.reset();
    m_postProcess.reset();
    m_debugRenderer.reset();
    m_assetManager.reset(); // dueño de las texturas y meshes (incluido el cubo fallback)
    m_litShader.reset();
    m_defaultShader.reset();
    m_viewportFb.reset();
    m_sceneFb.reset();
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

// `renderSceneToViewport` movido a `EditorRenderPass.cpp` (Hito 16).

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

        // Hito 15 polish: el modal Welcome puede editar la lista de
        // recientes (boton X por entrada + "Limpiar inexistentes"). Cuando
        // hay cambios, sincronizamos `m_recentProjects` con la lista del
        // UI y persistimos al `editor_state.json`.
        if (m_ui.consumeRecentsDirty()) {
            m_recentProjects.assign(m_ui.recentProjects().begin(),
                                     m_ui.recentProjects().end());
            saveEditorState();
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
        // Demos del menu Ayuda + handlers de drag&drop al viewport. Hito 16:
        // implementaciones movidas a `DemoSpawners.cpp` para mantener `run()`
        // legible.
        processSpawnRotatorRequest();
        processSpawnPhysicsBoxRequest();
        processSpawnEnvironmentRequest();
        processSpawnShadowDemoRequest();
        processSpawnPointLightRequest();
        processSpawnAudioSourceRequest();
        processSavePrefabRequest();

        // 2.4) Click-to-select (Hito 13 Bloque 2): raycast desde el cursor
        //      y selecciona la entidad mas cercana. Click en vacio deselecciona.
        //      Solo en Editor Mode — en Play Mode el mouse es para la camara.
        //      Si el gizmo se llevo el click este frame, descartar el select
        //      para que clickear sobre una flecha no mueva la seleccion.
        const ViewportPanel::ClickSelect click = m_ui.viewport().consumeClickSelect();
        const bool skipClickDueToGizmo = m_gizmoConsumedClick;
        m_gizmoConsumedClick = false;
        if (click.pending && !skipClickDueToGizmo &&
            m_mode == EditorMode::Editor && m_scene) {
            const float aspect = (m_viewportFb->height() > 0)
                ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
                : 1.0f;
            const glm::mat4 view = m_editorCamera.viewMatrix();
            const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
            const ScenePickResult hit = pickEntity(*m_scene, view, projection,
                                                    glm::vec2(click.ndcX, click.ndcY));
            if (hit) {
                m_ui.setSelectedEntity(hit.entity);
                if (hit.entity.hasComponent<TagComponent>()) {
                    Log::editor()->info("Click-select: '{}'",
                        hit.entity.getComponent<TagComponent>().name);
                }
            } else {
                m_ui.setSelectedEntity(Entity{});
            }
        }

        // 2.5/2.6/2.7) Drops desde AssetBrowser (textura / mesh / prefab).
        // Hito 16: implementaciones movidas a  para
        // mantener  legible.
        processViewportTextureDrop();
        processViewportMeshDrop();
        processViewportPrefabDrop();

        // 3) Input -> camaras.
        updateCameras(dt);

        // 3.4) Fisica (Jolt, Hito 12): materializa bodies nuevos siempre y
        //      stepea la sim solo en Play Mode. Despues del input y antes
        //      de scripts — asi un script puede leer la posicion post-step.
        updateRigidBodies(dt);

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
