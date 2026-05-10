// F2H24: main menu + save/load del MoodPlayer (Hito 38 B / Hito 41).
// drawMainMenu — overlay con New Game / Load Game / Quit.
// applyLoadedSave — restaura state desde un .moodsave (HUD + player +
//   bodies + script globals).
// captureCurrentState — snapshot del state runtime para escribir
//   .moodsave.
// quickSave / saveAs — F5 / F6.

#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "engine/game/state/GameState.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "systems/scripting/ScriptSystem.h"

#include <SDL.h>
#include <imgui.h>
#include <portable-file-dialogs.h>

#include <filesystem>
#include <string>
#include <unordered_map>

#include <glm/gtc/quaternion.hpp>

namespace Mood {

namespace {
// Path al exe del MoodPlayer (donde escribimos quicksave + leemos al
// inicio). SDL_GetBasePath devuelve string allocado — wrapper RAII.
std::filesystem::path exeBaseDir() {
    char* p = SDL_GetBasePath();
    if (p == nullptr) return std::filesystem::path{};
    std::filesystem::path out(p);
    SDL_free(p);
    return out;
}
} // namespace

void PlayerApplication::drawMainMenu() {
    // Hito 41 fix-up: estilo del editor — overlay oscuro completo,
    // panel mas grande, padding generoso, titulo grande.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(vp->Pos,
                       ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                       IM_COL32(0, 0, 0, 230));

    // Panel modal centrado mas amplio.
    constexpr float k_panelW = 420.0f;
    constexpr float k_panelH = 360.0f;
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x * 0.5f - k_panelW * 0.5f,
               vp->Pos.y + vp->Size.y * 0.5f - k_panelH * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(k_panelW, k_panelH));

    // Estilo: padding generoso + bordes redondeados + colores oscuros
    // del tipo editor.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.30f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.42f, 0.62f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.62f, 0.85f, 1.0f));

    ImGui::Begin("##MainMenu", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoSavedSettings);

    // Titulo grande centrado (escala 1.8x con la font default).
    ImGui::SetWindowFontScale(2.0f);
    const ImVec2 titleSize = ImGui::CalcTextSize("MoodEngine");
    ImGui::SetCursorPosX((k_panelW - titleSize.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 1.0f, 1.0f));
    ImGui::TextUnformatted("MoodEngine");
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    constexpr ImVec2 k_btn(-FLT_MIN, 48.0f);
    if (ImGui::Button("New Game", k_btn)) {
        Log::engine()->info("[MainMenu] New Game -> reload mapa default + reset state");
        // Hito 41 fix-up: New Game debe RECARGAR el mapa default
        // desde el .moodproj — sin esto el state runtime acumulado
        // (cajas movidas, hud editado, scripts con globals) se queda
        // entre partidas. Reset profesional: cleanup de bodies Jolt
        // + char + scripts, despues tryLoadGameManifest reconstruye.

        // 1) Cleanup de Jolt: destruir char + rigid bodies dynamic
        //    activos. Static se recrean cuando rebuildSceneFromMap
        //    spawnea las entidades-tile.
        if (m_physicsWorld) {
            if (m_playerCharId != 0) {
                m_physicsWorld->destroyCharacter(m_playerCharId);
                m_playerCharId = 0;
            }
            if (m_scene) {
                m_scene->forEach<RigidBodyComponent>(
                    [&](Entity, RigidBodyComponent& rb) {
                        if (rb.bodyId != 0) {
                            m_physicsWorld->destroyBody(rb.bodyId);
                            rb.bodyId = 0;
                        }
                    });
            }
        }

        // 2) Cleanup scripts: tira los sol::states (preserva sin
        //    huerfanos cuando registry().clear() invalida entt::entity).
        if (m_scriptSystem) m_scriptSystem->clear();

        // 3) Recargar el mapa default. tryLoadGameManifest() interno:
        //    - load del .moodproj + default_map .moodmap.
        //    - rebuildSceneFromMap (registry.clear + tiles + floor).
        //    - applyEntitiesToScene (recrea entidades persistidas).
        if (!tryLoadGameManifest()) {
            // Fallback al sample map (proyecto sin game.json).
            buildTestMap();
        }

        // 4) Reset GameState (hud + paused).
        GameState::reset();

        // 5) Reset cámara a la pos inicial. La pos default del
        //    constructor es la del FpsCamera member-init list.
        m_playCamera = FpsCamera(glm::vec3(0.0f, 1.6f, 0.0f), -90.0f, 0.0f);

        m_inMainMenu = false;
    }
    if (ImGui::Button("Load Game", k_btn)) {
        // pfd::open_file devuelve un vector<string>; vacio = cancel.
        const auto results = pfd::open_file(
            "Cargar partida",
            (exeBaseDir()).generic_string(),
            { "MoodSave (*.moodsave)", "*.moodsave",
              "All Files", "*" }).result();
        if (!results.empty()) {
            const std::filesystem::path savePath(results[0]);
            const auto data = SaveLoad::load(savePath);
            if (data.has_value()) {
                applyLoadedSave(*data);
                m_inMainMenu = false;
            } else {
                Log::engine()->warn(
                    "MainMenu: no se pudo cargar '{}' (ver warning anterior)",
                    savePath.generic_string());
            }
        }
    }
    if (ImGui::Button("Quit", k_btn)) {
        Log::engine()->info("[MainMenu] Quit -> exiting MoodPlayer");
        m_running = false;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    // Atajos de teclado en gris suave, centrados.
    const char* hint = "F5 quicksave  |  F6 save as...  |  Esc menu pausa";
    const ImVec2 hintSize = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPosX((k_panelW - hintSize.x) * 0.5f);
    ImGui::TextDisabled("%s", hint);

    ImGui::End();
    ImGui::PopStyleColor(4);  // WindowBg + Button{,Hovered,Active}
    ImGui::PopStyleVar(5);    // padding/rounding/frameRound/frameBorder/itemSpacing
}

void PlayerApplication::applyLoadedSave(const SaveLoad::SaveData& data) {
    // V1: no cambiamos de mapa. Si el data.mapPath no coincide con el
    // map activo, logueamos warning pero seguimos aplicando el state
    // sobre el map actual — es responsabilidad del caller del menu
    // pre-cargar el map correcto si tiene saves de varios niveles.
    if (!data.mapPath.empty() && !m_currentMapPath.empty() &&
        data.mapPath != m_currentMapPath.generic_string()) {
        Log::engine()->warn(
            "Load: el .moodsave referencia '{}' pero el map actual es '{}' — "
            "se aplica el state sobre el map actual",
            data.mapPath, m_currentMapPath.generic_string());
    }

    GameState::reset();
    GameState::hud() = data.hud;

    if (m_physicsWorld && m_playerCharId != 0) {
        m_physicsWorld->setCharacterPosition(m_playerCharId, data.playerPosition);
    }
    m_playCamera.setOrientation(data.playerYaw, data.playerPitch);
    m_playCamera.setPosition(data.playerPosition + glm::vec3(0.0f, 0.7f, 0.0f));

    // Hito 41 A + fix-up: aplicar snapshots de bodies. Buscamos por tag
    // (estable entre sesiones, bodyId es volatil).
    //
    // FIX RAIZ: aplicamos SIEMPRE al `TransformComponent` (que existe
    // sin importar si el body fue materializado). Si el body ya existe
    // (Play Mode activo), tambien al body para que linear/angular vel
    // se preserven. Si NO existe (load desde main menu antes del primer
    // Play), updateRigidBodies materializara el body en el siguiente
    // frame leyendo del Transform — y como el Transform tiene la pose
    // del save, el body queda en la pose correcta.
    int bodiesApplied = 0;
    if (m_scene && !data.bodies.empty()) {
        std::unordered_map<std::string, const SaveLoad::BodySnapshot*> byTag;
        byTag.reserve(data.bodies.size());
        for (const auto& b : data.bodies) {
            // Hito 41 fix: warn ante tags duplicados (V1 limitation: el
            // primero gana; el dev deberia spawnear con tags unicos
            // como hace `Agregar caja fisica demo` desde Hito 41).
            if (byTag.count(b.entityTag) > 0) {
                Log::engine()->warn(
                    "[Load] tag duplicado en .moodsave: '{}' — solo se aplicara el primer snapshot",
                    b.entityTag);
                continue;
            }
            byTag[b.entityTag] = &b;
        }

        // Hito 41 fix-up #2: tracking de cuales tags del save matchean
        // entidades del scene. Lo usamos para loguear los huerfanos al
        // final (snapshots cuya entidad NO existe en el .moodmap actual).
        std::unordered_map<std::string, bool> tagMatched;
        for (const auto& kv : byTag) tagMatched[kv.first] = false;

        m_scene->forEach<TagComponent, TransformComponent, RigidBodyComponent>(
            [&](Entity, TagComponent& tag, TransformComponent& tf, RigidBodyComponent& rb) {
                if (rb.type != RigidBodyComponent::Type::Dynamic) return;
                auto it = byTag.find(tag.name);
                if (it == byTag.end()) return; // entity sin snapshot — OK silencioso
                const auto& snap = *it->second;
                tagMatched[tag.name] = true;

                // 1) SIEMPRE: aplicar al TransformComponent. Cubre el
                //    caso "load desde main menu antes del primer Play"
                //    (body NO materializado todavia → updateRigidBodies
                //    leera Transform + pending vels en el siguiente frame).
                tf.position = snap.position;
                const glm::quat q(snap.rotationQuat.w, snap.rotationQuat.x,
                                   snap.rotationQuat.y, snap.rotationQuat.z);
                tf.rotationEuler = glm::degrees(glm::eulerAngles(q));

                // 2) Si el body YA esta materializado (Play activo),
                //    aplicar al body en vivo. Si no, stash en
                //    pendingVel — updateRigidBodies las aplica al
                //    materializar.
                if (rb.bodyId != 0 && m_physicsWorld) {
                    Log::engine()->info(
                        "    [LOAD] body tag='{}' bodyId={} pos=({:.2f},{:.2f},{:.2f}) [Transform+Body+Vel]",
                        tag.name, rb.bodyId,
                        snap.position.x, snap.position.y, snap.position.z);
                    m_physicsWorld->setBodyPositionRot(
                        rb.bodyId, snap.position, snap.rotationQuat);
                    m_physicsWorld->setBodyLinearVelocity(
                        rb.bodyId, snap.linearVelocity);
                    m_physicsWorld->setBodyAngularVelocity(
                        rb.bodyId, snap.angularVelocity);
                } else {
                    Log::engine()->info(
                        "    [LOAD] body tag='{}' bodyId=0 pos=({:.2f},{:.2f},{:.2f}) [Transform + pendingVel — body se materializara con pose+vel]",
                        tag.name,
                        snap.position.x, snap.position.y, snap.position.z);
                    rb.hasPendingVel = true;
                    rb.pendingLinearVel = snap.linearVelocity;
                    rb.pendingAngularVel = snap.angularVelocity;
                }
                ++bodiesApplied;
            });

        // Snapshots huerfanos: el save tiene tags que NO existen en el
        // scene actual. Tipicamente significa que las entidades viven
        // en runtime (spawneadas via "Agregar caja fisica demo" sin
        // Ctrl+S al .moodmap antes de empaquetar). El usuario lo nota
        // como "las cajas no aparecen al hacer Load Game" — el log
        // explica la causa raiz.
        for (const auto& [tag, matched] : tagMatched) {
            if (!matched) {
                Log::engine()->warn(
                    "[Load] snapshot huerfano: tag='{}' del .moodsave NO matchea ninguna entity Dynamic en el scene. "
                    "Probablemente la entidad fue spawneada en runtime y nunca persistida al .moodmap "
                    "(Ctrl+S en el editor antes de empaquetar).",
                    tag);
            }
        }
    }
    Log::engine()->info(
        "[Load] Applied {}/{} body snapshots (tag matched), hud hp={}, player @ ({:.2f},{:.2f},{:.2f})",
        bodiesApplied, data.bodies.size(),
        data.hud.hp,
        data.playerPosition.x, data.playerPosition.y, data.playerPosition.z);

    // Hito 41 B: restaurar globals Lua. Si un script aun no se cargo,
    // ScriptSystem::restoreGlobals los stash en pendingGlobals.
    int globalsApplied = 0;
    if (m_scene && m_scriptSystem && !data.scriptGlobals.empty()) {
        std::unordered_map<std::string, const SaveLoad::ScriptGlobalsSnapshot*> byPath;
        byPath.reserve(data.scriptGlobals.size());
        for (const auto& sg : data.scriptGlobals) byPath[sg.scriptPath] = &sg;

        m_scene->forEach<ScriptComponent>(
            [&](Entity e, ScriptComponent& sc) {
                auto it = byPath.find(sc.path);
                if (it == byPath.end()) return;
                m_scriptSystem->restoreGlobals(e.handle(), it->second->globals);
                ++globalsApplied;
            });
    }
    Log::engine()->info(
        "[Load] Restored {} script globals snapshots (path matched)",
        globalsApplied);
}

SaveLoad::SaveData PlayerApplication::captureCurrentState() {
    SaveLoad::SaveData d;
    d.mapPath        = m_currentMapPath.generic_string();
    d.hud            = GameState::hud();
    if (m_physicsWorld && m_playerCharId != 0) {
        d.playerPosition = m_physicsWorld->characterPosition(m_playerCharId);
    } else {
        d.playerPosition = m_playCamera.position();
    }
    d.playerYaw   = m_playCamera.yawDeg();
    d.playerPitch = m_playCamera.pitchDeg();

    Log::engine()->info("[Save] capturando state...");
    Log::engine()->info("  - hud: hp={}, ammo={}", d.hud.hp, d.hud.ammo);
    Log::engine()->info("  - player pos=({:.2f},{:.2f},{:.2f}) yaw={:.1f} pitch={:.1f}",
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z,
        d.playerYaw, d.playerPitch);

    // Hito 41 A: capturar snapshots de Dynamic bodies activos.
    if (m_scene && m_physicsWorld) {
        Log::engine()->info("  - capturing Dynamic body snapshots...");
        m_scene->forEach<TagComponent, RigidBodyComponent>(
            [&](Entity, TagComponent& tag, RigidBodyComponent& rb) {
                if (rb.type != RigidBodyComponent::Type::Dynamic) return;
                if (rb.bodyId == 0) return;
                if (tag.name.empty()) return;  // sin tag estable, skip
                SaveLoad::BodySnapshot b;
                b.entityTag = tag.name;
                glm::vec4 quat;
                b.position = m_physicsWorld->bodyPositionRot(rb.bodyId, quat);
                b.rotationQuat = quat;
                b.linearVelocity  = m_physicsWorld->bodyLinearVelocity(rb.bodyId);
                b.angularVelocity = m_physicsWorld->bodyAngularVelocity(rb.bodyId);
                // Hito 41 fix-up: clamp velocidades casi-cero a exactamente
                // cero. Sin esto un body que estaba dormido (sleeping)
                // pero con epsilon de velocity guardada, al cargar se
                // re-activa y empieza a moverse/rotar lentamente.
                // Threshold ~ 0.01 m/s — bajo el umbral de Jolt sleeping.
                constexpr f32 k_velEpsilon = 0.01f;
                if (glm::length(b.linearVelocity) < k_velEpsilon) {
                    b.linearVelocity = glm::vec3(0.0f);
                }
                if (glm::length(b.angularVelocity) < k_velEpsilon) {
                    b.angularVelocity = glm::vec3(0.0f);
                }
                Log::engine()->info(
                    "    [SAVE] body tag='{}' bodyId={} pos=({:.2f},{:.2f},{:.2f}) quat=({:.3f},{:.3f},{:.3f},{:.3f}) linVel=({:.2f},{:.2f},{:.2f})",
                    tag.name, rb.bodyId,
                    b.position.x, b.position.y, b.position.z,
                    quat.x, quat.y, quat.z, quat.w,
                    b.linearVelocity.x, b.linearVelocity.y, b.linearVelocity.z);
                d.bodies.push_back(std::move(b));
            });
        Log::engine()->info("  - {} body snapshots captured", d.bodies.size());
    }

    // Hito 41 B: capturar globals Lua per script.
    if (m_scene && m_scriptSystem) {
        Log::engine()->info("  - capturing Lua script globals...");
        m_scene->forEach<ScriptComponent>(
            [&](Entity e, ScriptComponent& sc) {
                if (sc.path.empty() || !sc.loaded) return;
                Log::engine()->debug("    script path='{}'", sc.path);
                auto globals = m_scriptSystem->captureGlobals(e.handle());
                if (globals.empty()) return;
                SaveLoad::ScriptGlobalsSnapshot sg;
                sg.scriptPath = sc.path;
                sg.globals    = std::move(globals);
                d.scriptGlobals.push_back(std::move(sg));
            });
        Log::engine()->info("  - {} scripts with globals captured", d.scriptGlobals.size());
    }

    return d;
}

void PlayerApplication::quickSave() {
    const auto d = captureCurrentState();
    const auto savePath = exeBaseDir() / "quicksave.moodsave";
    if (SaveLoad::save(d, savePath)) {
        Log::engine()->info("Quicksave OK -> '{}'", savePath.generic_string());
    } else {
        Log::engine()->warn("Quicksave fallo -> '{}'", savePath.generic_string());
    }
}

void PlayerApplication::saveAs() {
    const auto d = captureCurrentState();
    // pfd::save_file abre dialog "Guardar como" con default_path en el
    // exe dir y filtro `.moodsave`. Si el user cancela, retorna "".
    const auto chosen = pfd::save_file(
        "Guardar partida como...",
        (exeBaseDir() / "partida.moodsave").generic_string(),
        { "MoodSave (*.moodsave)", "*.moodsave" }).result();
    if (chosen.empty()) {
        Log::engine()->info("[SaveAs] cancelado por el usuario");
        return;
    }
    std::filesystem::path savePath(chosen);
    // Si el usuario no agrego extension, ponemos .moodsave por nosotros.
    if (savePath.extension() != ".moodsave") {
        savePath += ".moodsave";
    }
    if (SaveLoad::save(d, savePath)) {
        Log::engine()->info("[SaveAs] OK -> '{}'", savePath.generic_string());
    } else {
        Log::engine()->warn("[SaveAs] fallo -> '{}'", savePath.generic_string());
    }
}

} // namespace Mood
