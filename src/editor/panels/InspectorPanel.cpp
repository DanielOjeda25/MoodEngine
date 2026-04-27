#include "editor/panels/InspectorPanel.h"

#include "editor/EditorUI.h"
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioClip.h"
#include "engine/render/MeshAsset.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"

#include <imgui.h>

#include <cstdio>
#include <string>
#include <vector>

namespace Mood {

void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("Editor UI no inyectado");
        ImGui::End();
        return;
    }

    Entity e = m_ui->selectedEntity();
    if (!e) {
        ImGui::TextDisabled("No hay entidad seleccionada");
        ImGui::TextDisabled("Click en el panel Hierarchy para elegir una.");
        ImGui::End();
        return;
    }

    // --- TagComponent ---
    if (e.hasComponent<TagComponent>()) {
        auto& tag = e.getComponent<TagComponent>();
        ImGui::TextDisabled("Tag");
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", tag.name.c_str());
        if (ImGui::InputText("##tag", buf, sizeof(buf))) {
            tag.name = buf;
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- TransformComponent ---
    // Scale + rotation solo tienen efecto visible si hay MeshRenderer
    // (o es un tile rendereado). Para Light/Audio puros (sin mesh)
    // ocultarlos evita confundir al usuario con controles sin efecto.
    if (e.hasComponent<TransformComponent>()) {
        auto& t = e.getComponent<TransformComponent>();
        ImGui::TextDisabled("Transform");
        if (ImGui::DragFloat3("position##tr", &t.position.x, 0.05f)) {
            m_editedThisFrame = true;
        }
        const bool hasMesh = e.hasComponent<MeshRendererComponent>();
        if (hasMesh) {
            if (ImGui::DragFloat3("rotation (deg)##tr", &t.rotationEuler.x, 0.5f)) {
                m_editedThisFrame = true;
            }
            if (ImGui::DragFloat3("scale##tr", &t.scale.x, 0.05f, 0.01f, 100.0f)) {
                m_editedThisFrame = true;
            }
        }
        ImGui::Separator();
    }

    // --- MeshRendererComponent ---
    // Con Hito 10, `mesh` es un MeshAssetId (no un IMesh* crudo) y `materials`
    // es un vector de TextureAssetId (1 por submesh). Muestra metadata del
    // mesh resuelto y la lista de materiales. Dropdown para cambiar el mesh
    // entra en Bloque 5 (drag & drop desde AssetBrowser).
    if (e.hasComponent<MeshRendererComponent>()) {
        const auto& mr = e.getComponent<MeshRendererComponent>();
        ImGui::TextDisabled("MeshRenderer");
        if (m_assets != nullptr) {
            ImGui::Text("mesh: %s (id %u)",
                         m_assets->meshPathOf(mr.mesh).c_str(), mr.mesh);
            MeshAsset* asset = m_assets->getMesh(mr.mesh);
            if (asset != nullptr) {
                ImGui::Text("submeshes: %u, vertices: %u",
                             static_cast<u32>(asset->submeshes.size()),
                             asset->totalVertexCount());
            }
        } else {
            ImGui::Text("mesh id: %u", mr.mesh);
        }
        ImGui::Text("materials: %u", static_cast<u32>(mr.materials.size()));
        for (usize i = 0; i < mr.materials.size(); ++i) {
            ImGui::BulletText("  [%zu] texture id %u", i,
                               static_cast<unsigned>(mr.materials[i]));
        }
        ImGui::TextDisabled("(editor de materiales en hitos posteriores)");
        ImGui::Separator();
    }

    // --- CameraComponent ---
    if (e.hasComponent<CameraComponent>()) {
        auto& cam = e.getComponent<CameraComponent>();
        ImGui::TextDisabled("Camera");
        if (ImGui::DragFloat("fov (deg)##cam", &cam.fovDeg, 0.1f, 1.0f, 179.0f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat("near##cam", &cam.nearPlane, 0.001f, 0.001f, 100.0f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat("far##cam", &cam.farPlane, 0.1f, 1.0f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- LightComponent (Hito 11) ---
    // Activado: tiene type / color / intensity (Hito 7) + radius (point) +
    // direction (directional) + enabled.
    if (e.hasComponent<LightComponent>()) {
        auto& lt = e.getComponent<LightComponent>();
        ImGui::TextDisabled("Light");

        if (ImGui::Checkbox("enabled##lt", &lt.enabled)) m_editedThisFrame = true;

        const char* items[] = {"Directional", "Point"};
        int current = static_cast<int>(lt.type);
        if (ImGui::Combo("type##lt", &current, items, 2)) {
            lt.type = static_cast<LightComponent::Type>(current);
            m_editedThisFrame = true;
        }
        if (ImGui::ColorEdit3("color##lt", &lt.color.x)) m_editedThisFrame = true;
        if (ImGui::DragFloat("intensity##lt", &lt.intensity, 0.01f, 0.0f, 100.0f)) {
            m_editedThisFrame = true;
        }

        if (lt.type == LightComponent::Type::Point) {
            if (ImGui::DragFloat("radius (m)##lt", &lt.radius, 0.1f, 0.1f, 1000.0f)) {
                m_editedThisFrame = true;
            }
        } else {
            if (ImGui::DragFloat3("direction##lt", &lt.direction.x, 0.01f, -1.0f, 1.0f)) {
                m_editedThisFrame = true;
            }
            // Hito 16: solo directional puede emitir shadow map (point shadows
            // requeririan cubemap depth, fuera de scope).
            if (ImGui::Checkbox("castShadows##lt", &lt.castShadows)) {
                m_editedThisFrame = true;
            }
        }
        ImGui::Separator();
    }

    // --- EnvironmentComponent (Hito 15 Bloque 4) ---
    // Sky + fog + post-process. Solo el primer Environment encontrado en la
    // Scene se aplica al frame; varios = warning suave (no aca, en
    // EditorApplication). Cambios en vivo via DragFloat / Combo.
    if (e.hasComponent<EnvironmentComponent>()) {
        auto& env = e.getComponent<EnvironmentComponent>();
        ImGui::TextDisabled("Environment");

        ImGui::TextDisabled("Skybox: %s (asset catalog futuro)",
                             env.skyboxPath.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted("Fog");
        const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
        int fogIdx = static_cast<int>(env.fogMode);
        if (ImGui::Combo("mode##env", &fogIdx, fogModes, 4)) {
            env.fogMode = static_cast<u32>(fogIdx);
            m_editedThisFrame = true;
        }
        if (ImGui::ColorEdit3("color##envfog", &env.fogColor.x)) m_editedThisFrame = true;
        if (env.fogMode == 1) {
            if (ImGui::DragFloat("start (m)##env", &env.fogLinearStart, 0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
            if (ImGui::DragFloat("end (m)##env",   &env.fogLinearEnd,   0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
        } else if (env.fogMode == 2 || env.fogMode == 3) {
            if (ImGui::DragFloat("density##env", &env.fogDensity, 0.001f, 0.0f, 1.0f)) m_editedThisFrame = true;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Post-process");
        if (ImGui::DragFloat("exposure (EV)##env", &env.exposure, 0.05f, -5.0f, 5.0f)) m_editedThisFrame = true;
        const char* tonemaps[] = {"None", "Reinhard", "ACES"};
        int toneIdx = static_cast<int>(env.tonemapMode);
        if (ImGui::Combo("tonemap##env", &toneIdx, tonemaps, 3)) {
            env.tonemapMode = static_cast<u32>(toneIdx);
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- ScriptComponent ---
    // Cambiar el path o pulsar Recargar pone `loaded=false`: el ScriptSystem
    // crea un sol::state fresco la proxima vez que corra update(). Eso es
    // mas fuerte que el hot-reload por mtime (que reutiliza el state);
    // cuando el usuario recarga manualmente lo habitual es querer un reset
    // limpio de globals.
    if (e.hasComponent<ScriptComponent>()) {
        auto& sc = e.getComponent<ScriptComponent>();
        ImGui::TextDisabled("Script");
        char buf[512];
        std::snprintf(buf, sizeof(buf), "%s", sc.path.c_str());
        if (ImGui::InputText("path##sc", buf, sizeof(buf))) {
            sc.path = buf;
            sc.loaded = false;
            sc.lastError.clear();
            m_editedThisFrame = true;
        }
        if (ImGui::Button("Recargar##sc")) {
            sc.loaded = false;
            sc.lastError.clear();
        }
        if (!sc.lastError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("Error: %s", sc.lastError.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
    }

    // --- RigidBodyComponent (Hito 12) ---
    // Edit del shape/mass requiere recrear el body en Jolt, asi que por
    // ahora los campos quedan read-only en Play Mode y editables en Editor.
    // Al salir de Play, cualquier cambio toma efecto via rebuildSceneFromMap
    // si se recarga el proyecto, o al destruir/recrear el body manualmente.
    if (e.hasComponent<RigidBodyComponent>()) {
        auto& rb = e.getComponent<RigidBodyComponent>();
        ImGui::TextDisabled("RigidBody");

        const char* typeNames[] = {"Static", "Kinematic", "Dynamic"};
        int typeIdx = static_cast<int>(rb.type);
        if (ImGui::Combo("type##rb", &typeIdx, typeNames, 3)) {
            rb.type = static_cast<RigidBodyComponent::Type>(typeIdx);
            m_editedThisFrame = true;
        }

        const char* shapeNames[] = {"Box", "Sphere", "Capsule"};
        int shapeIdx = static_cast<int>(rb.shape);
        if (ImGui::Combo("shape##rb", &shapeIdx, shapeNames, 3)) {
            rb.shape = static_cast<RigidBodyComponent::Shape>(shapeIdx);
            m_editedThisFrame = true;
        }

        if (ImGui::DragFloat3("halfExtents##rb", &rb.halfExtents.x, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        if (rb.type == RigidBodyComponent::Type::Dynamic) {
            if (ImGui::DragFloat("mass (kg)##rb", &rb.mass, 0.1f, 0.001f, 10000.0f)) {
                m_editedThisFrame = true;
            }
        }
        ImGui::TextDisabled("body id: %u (0 = no materializado)", rb.bodyId);
        ImGui::Separator();
    }

    // --- AudioSourceComponent (Hito 9) ---
    // Dropdown poblado a partir de los clips cargados en el AssetManager.
    // Botón "Reproducir" resetea `started` para que AudioSystem dispare de
    // nuevo en el próximo update (util para preview sin entrar a Play Mode).
    if (e.hasComponent<AudioSourceComponent>()) {
        auto& asrc = e.getComponent<AudioSourceComponent>();
        ImGui::TextDisabled("AudioSource");

        // Lista de clips del manager como combo. Si aun no hay assets manager
        // inyectado, mostrar solo el id crudo.
        if (m_assets != nullptr) {
            std::vector<std::string> labels;
            labels.reserve(m_assets->audioCount());
            for (AudioAssetId i = 0; i < m_assets->audioCount(); ++i) {
                labels.push_back(m_assets->audioPathOf(i));
            }
            int current = static_cast<int>(asrc.clip);
            if (current < 0 || current >= static_cast<int>(labels.size())) {
                current = 0;
            }
            // BeginCombo para soportar paths largos con scroll.
            if (ImGui::BeginCombo("clip##as", labels[current].c_str())) {
                for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
                    const bool selected = (current == i);
                    if (ImGui::Selectable(labels[i].c_str(), selected)) {
                        asrc.clip = static_cast<AudioAssetId>(i);
                        asrc.started = false; // obligar a re-arrancar con el nuevo clip
                        m_editedThisFrame = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::Text("clip id: %u (asset manager no inyectado)", asrc.clip);
        }

        if (ImGui::SliderFloat("volume##as", &asrc.volume, 0.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::Checkbox("loop##as", &asrc.loop)) { m_editedThisFrame = true; }
        ImGui::SameLine();
        if (ImGui::Checkbox("playOnStart##as", &asrc.playOnStart)) {
            m_editedThisFrame = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("is3D##as", &asrc.is3D)) { m_editedThisFrame = true; }

        // Preview: resetear `started` fuerza al AudioSystem a volver a
        // disparar la reproduccion en el proximo frame. Requiere playOnStart.
        if (ImGui::Button("Reproducir##as")) {
            asrc.started = false;
            if (!asrc.playOnStart) {
                // Preview implicito: encender playOnStart temporalmente no
                // es ideal (persiste). Mejor: advertir al usuario.
                ImGui::OpenPopup("audio_preview_note");
            }
        }
        if (ImGui::BeginPopup("audio_preview_note")) {
            ImGui::TextUnformatted("Activa 'playOnStart' para que suene al disparar.");
            ImGui::EndPopup();
        }
        ImGui::Separator();
    }

    ImGui::End();
}

} // namespace Mood
