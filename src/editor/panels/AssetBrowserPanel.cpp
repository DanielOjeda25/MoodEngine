#include "editor/panels/AssetBrowserPanel.h"

#include "core/Log.h"
#include "engine/audio/AudioClip.h"
#include "engine/render/ITexture.h"
#include "engine/render/MeshAsset.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace Mood {

namespace {

// Directorio relativo al cwd donde buscamos texturas. Cuando el VFS soporte
// lookup inverso, esto sale de ahi.
constexpr const char* k_textureDir = "assets/textures";
constexpr const char* k_audioDir   = "assets/audio";
constexpr const char* k_meshDir    = "assets/meshes";
constexpr float k_thumbSize = 64.0f;
constexpr const char* k_logicalPrefix      = "textures/";
constexpr const char* k_audioLogicalPrefix = "audio/";
constexpr const char* k_meshLogicalPrefix  = "meshes/";

bool isPng(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png";
}

bool isAudio(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

bool isMesh(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx";
}

} // namespace

void AssetBrowserPanel::rescan() {
    m_entries.clear();
    if (m_assetManager == nullptr) return;

    std::error_code ec;
    auto it = std::filesystem::directory_iterator(k_textureDir, ec);
    if (ec) {
        Log::assets()->warn("AssetBrowserPanel: no pude listar '{}': {}",
                            k_textureDir, ec.message());
        return;
    }
    for (const auto& entry : it) {
        if (!entry.is_regular_file() || !isPng(entry.path())) continue;
        Entry e;
        e.displayName = entry.path().filename().string();
        e.logicalPath = std::string(k_logicalPrefix) + e.displayName;
        e.id = m_assetManager->loadTexture(e.logicalPath);
        m_entries.push_back(std::move(e));
    }
    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry& a, const Entry& b) {
                  return a.displayName < b.displayName;
              });

    // Audio: mismo patron, busca en assets/audio/ por wav/ogg/mp3/flac.
    m_audioEntries.clear();
    std::error_code ac_ec;
    auto audio_it = std::filesystem::directory_iterator(k_audioDir, ac_ec);
    if (!ac_ec) {
        for (const auto& entry : audio_it) {
            if (!entry.is_regular_file() || !isAudio(entry.path())) continue;
            AudioEntry ae;
            ae.displayName = entry.path().filename().string();
            ae.logicalPath = std::string(k_audioLogicalPrefix) + ae.displayName;
            ae.id = m_assetManager->loadAudio(ae.logicalPath);
            m_audioEntries.push_back(std::move(ae));
        }
        std::sort(m_audioEntries.begin(), m_audioEntries.end(),
                  [](const AudioEntry& a, const AudioEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    // Meshes: busca en assets/meshes/ por obj/gltf/glb/fbx. El AssetManager
    // carga con assimp; si algun import falla cae a `missingMeshId()` (cubo)
    // y el log del canal `assets` avisa.
    m_meshEntries.clear();
    std::error_code mesh_ec;
    auto mesh_it = std::filesystem::directory_iterator(k_meshDir, mesh_ec);
    if (!mesh_ec) {
        for (const auto& entry : mesh_it) {
            if (!entry.is_regular_file() || !isMesh(entry.path())) continue;
            MeshEntry me;
            me.displayName = entry.path().filename().string();
            me.logicalPath = std::string(k_meshLogicalPrefix) + me.displayName;
            me.id = m_assetManager->loadMesh(me.logicalPath);
            m_meshEntries.push_back(std::move(me));
        }
        std::sort(m_meshEntries.begin(), m_meshEntries.end(),
                  [](const MeshEntry& a, const MeshEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    m_scanned = true;
    Log::assets()->info("AssetBrowserPanel: {} texturas, {} audios, {} meshes listados",
                         m_entries.size(), m_audioEntries.size(), m_meshEntries.size());
}

void AssetBrowserPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_assetManager == nullptr) {
        ImGui::TextDisabled("Asset manager no inyectado");
        ImGui::End();
        return;
    }

    if (!m_scanned) rescan();

    if (ImGui::Button("Recargar")) {
        rescan();
        // El reupload a GPU de las texturas cambiadas lo maneja
        // EditorApplication entre frames para no borrar un GLuint que
        // ImGui pueda estar a punto de usar.
        m_reloadRequested = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu texturas", m_entries.size());
    ImGui::Separator();

    // Grid de miniaturas. Columnas calculadas segun el ancho disponible del
    // panel para que se reacomode al redimensionar.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float cell = k_thumbSize + 12.0f;
    int cols = std::max(1, static_cast<int>(avail / cell));

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        ITexture* tex = m_assetManager->getTexture(e.id);
        if (tex == nullptr) continue;

        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();

        // Flip V (0,1)-(1,0): OpenGL textures cargadas con flip vertical se
        // veen invertidas en ImGui (que asume origen arriba-izquierda).
        const bool isSelected = m_selected.has_value() && *m_selected == e.logicalPath;
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
        }
        if (ImGui::ImageButton("##thumb", tex->handle(),
                               ImVec2(k_thumbSize, k_thumbSize),
                               ImVec2(0, 1), ImVec2(1, 0))) {
            m_selected = e.logicalPath;
            Log::assets()->info("AssetBrowserPanel: seleccionado '{}'", e.logicalPath);
        }
        if (isSelected) ImGui::PopStyleColor();

        // Drag source: arrastrar una miniatura permite soltarla sobre el
        // Viewport para asignarla como textura de un tile. Payload = id de
        // la textura (u32); el consumidor lo castea a TextureAssetId.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_TEXTURE_ASSET", &e.id, sizeof(e.id));
            // Preview del payload: miniatura + nombre.
            ImGui::Image(tex->handle(), ImVec2(48.0f, 48.0f),
                         ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::TextUnformatted(e.displayName.c_str());
            ImGui::EndDragDropSource();
        }

        // Nombre de archivo, truncado si no entra en el ancho del thumb.
        const float textW = ImGui::CalcTextSize(e.displayName.c_str()).x;
        if (textW <= k_thumbSize) {
            ImGui::TextUnformatted(e.displayName.c_str());
        } else {
            // Mostrar "abc.." + tooltip con el nombre completo.
            std::string truncated = e.displayName;
            while (!truncated.empty() &&
                   ImGui::CalcTextSize((truncated + "..").c_str()).x > k_thumbSize) {
                truncated.pop_back();
            }
            ImGui::Text("%s..", truncated.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", e.displayName.c_str());
            }
        }

        ImGui::EndGroup();
        ImGui::PopID();

        if (static_cast<int>((i + 1) % cols) != 0) {
            ImGui::SameLine();
        }
    }

    // --- Sección Meshes (Hito 10 Bloque 5) ---
    // Puesta justo despues de las texturas para que los modelos importados
    // queden a la vista sin scroll. Lista plana (sin thumbnails todavia)
    // con metadata resuelta por assimp. Cada item es arrastrable al
    // Viewport para spawnear una entidad con `MeshRendererComponent`.
    if (!m_meshEntries.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& me : m_meshEntries) {
                MeshAsset* asset = m_assetManager->getMesh(me.id);
                ImGui::PushID(me.logicalPath.c_str());
                // Selectable para habilitar drag source; no cambia seleccion
                // global del panel (esa la usa el flow de texturas).
                ImGui::Selectable(me.displayName.c_str(), false);
                // Drag source INMEDIATAMENTE despues del Selectable:
                // BeginDragDropSource opera sobre el ultimo item, y si se llama
                // tras el `SameLine + TextDisabled` siguiente se asociaria al
                // texto gris (que no es draggeable).
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MOOD_MESH_ASSET", &me.id, sizeof(me.id));
                    ImGui::TextUnformatted(me.displayName.c_str());
                    ImGui::EndDragDropSource();
                }
                if (asset != nullptr) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[%u submeshes, %u v]",
                                         static_cast<u32>(asset->submeshes.size()),
                                         asset->totalVertexCount());
                }
                ImGui::PopID();
            }
        }
    }

    // --- Sección Audio (Hito 9 Bloque 4) ---
    // Por ahora solo texto: path lógico + duracion + canales. Drag & drop a
    // entidades viene en hitos posteriores (Hito 10+).
    if (!m_audioEntries.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& ae : m_audioEntries) {
                AudioClip* clip = m_assetManager->getAudio(ae.id);
                if (clip == nullptr) continue;
                ImGui::Text("%s", ae.displayName.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("[%.2fs, %uHz, %uch]",
                                     clip->durationSeconds(),
                                     clip->sampleRate(),
                                     clip->channels());
            }
        }
    }

    ImGui::End();
}

} // namespace Mood
