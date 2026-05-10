#include "editor/panels/assets/AssetBrowserPanel.h"

#include "core/Log.h"
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en tabs + filas
#include "engine/audio/clips/AudioClip.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MeshAsset.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace Mood {

namespace {

// Directorio relativo al cwd donde buscamos texturas. Cuando el VFS soporte
// lookup inverso, esto sale de ahi.
constexpr const char* k_textureDir  = "assets/textures";
constexpr const char* k_audioDir    = "assets/audio";
constexpr const char* k_meshDir     = "assets/meshes";
constexpr const char* k_prefabDir   = "assets/prefabs";
constexpr const char* k_materialDir = "assets/materials";
constexpr const char* k_scriptDir   = "assets/scripts";
constexpr float k_thumbSize = 64.0f;
constexpr const char* k_logicalPrefix         = "textures/";
constexpr const char* k_audioLogicalPrefix    = "audio/";
constexpr const char* k_meshLogicalPrefix     = "meshes/";
constexpr const char* k_prefabLogicalPrefix   = "prefabs/";
constexpr const char* k_materialLogicalPrefix = "materials/";
constexpr const char* k_scriptLogicalPrefix   = "scripts/";

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

bool isPrefab(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".moodprefab";
}

bool isMaterial(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".material";
}

bool isLuaScript(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".lua";
}

// Cuenta lineas de un archivo de texto sin cargar todo a memoria. Devuelve
// 0 si el archivo no existe o no se puede abrir — el browser solo lo usa
// como metadata informativa.
u32 countLines(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f.is_open()) return 0;
    u32 n = 0;
    std::string line;
    while (std::getline(f, line)) ++n;
    return n;
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

    // Meshes: busca recursivamente en assets/meshes/ por obj/gltf/glb/fbx.
    // El AssetManager carga con assimp; si algun import falla cae a
    // `missingMeshId()` (cubo) y el log del canal `assets` avisa.
    // Hito 26: cambio a recursive_directory_iterator para que paquetes
    // como Kenney Survival Kit (assets/meshes/kenney_survival/*.glb)
    // aparezcan en la lista. El displayName usa el path relativo
    // ("kenney_survival/barrel.glb") para distinguir entre packs.
    m_meshEntries.clear();
    std::error_code mesh_ec;
    auto mesh_it = std::filesystem::recursive_directory_iterator(k_meshDir, mesh_ec);
    if (!mesh_ec) {
        for (const auto& entry : mesh_it) {
            if (!entry.is_regular_file() || !isMesh(entry.path())) continue;
            MeshEntry me;
            const auto rel = std::filesystem::relative(entry.path(), k_meshDir);
            me.displayName = rel.generic_string();
            me.logicalPath = std::string(k_meshLogicalPrefix) + me.displayName;
            me.id = m_assetManager->loadMesh(me.logicalPath);
            m_meshEntries.push_back(std::move(me));
        }
        std::sort(m_meshEntries.begin(), m_meshEntries.end(),
                  [](const MeshEntry& a, const MeshEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    // Prefabs: busca en assets/prefabs/ por *.moodprefab. AssetManager
    // los lazy-parsea con `loadPrefab`; si alguno falla cae a `missingPrefabId()`.
    m_prefabEntries.clear();
    std::error_code prefab_ec;
    auto prefab_it = std::filesystem::directory_iterator(k_prefabDir, prefab_ec);
    if (!prefab_ec) {
        for (const auto& entry : prefab_it) {
            if (!entry.is_regular_file() || !isPrefab(entry.path())) continue;
            PrefabEntry pe;
            pe.displayName = entry.path().filename().string();
            pe.logicalPath = std::string(k_prefabLogicalPrefix) + pe.displayName;
            pe.id = m_assetManager->loadPrefab(pe.logicalPath);
            m_prefabEntries.push_back(std::move(pe));
        }
        std::sort(m_prefabEntries.begin(), m_prefabEntries.end(),
                  [](const PrefabEntry& a, const PrefabEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    // Materiales: busca en assets/materials/ por *.material. Los carga
    // con `loadMaterial` (lee JSON + cachea); si alguno falla cae al
    // default material y el log avisa.
    m_materialEntries.clear();
    std::error_code mat_ec;
    auto mat_it = std::filesystem::directory_iterator(k_materialDir, mat_ec);
    if (!mat_ec) {
        for (const auto& entry : mat_it) {
            if (!entry.is_regular_file() || !isMaterial(entry.path())) continue;
            MaterialEntry me;
            me.displayName = entry.path().filename().string();
            me.logicalPath = std::string(k_materialLogicalPrefix) + me.displayName;
            me.id = m_assetManager->loadMaterial(me.logicalPath);
            m_materialEntries.push_back(std::move(me));
        }
        std::sort(m_materialEntries.begin(), m_materialEntries.end(),
                  [](const MaterialEntry& a, const MaterialEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    // Scripts Lua: busca en assets/scripts/ por *.lua (Hito 22 Bloque 1).
    // No pasa por AssetManager — los scripts los carga ScriptSystem on
    // demand desde el path. Solo guardamos line count como metadata.
    m_scriptEntries.clear();
    std::error_code script_ec;
    auto script_it = std::filesystem::directory_iterator(k_scriptDir, script_ec);
    if (!script_ec) {
        for (const auto& entry : script_it) {
            if (!entry.is_regular_file() || !isLuaScript(entry.path())) continue;
            ScriptEntry se;
            se.displayName = entry.path().filename().string();
            se.logicalPath = std::string(k_scriptLogicalPrefix) + se.displayName;
            se.lineCount   = countLines(entry.path());
            m_scriptEntries.push_back(std::move(se));
        }
        std::sort(m_scriptEntries.begin(), m_scriptEntries.end(),
                  [](const ScriptEntry& a, const ScriptEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    m_scanned = true;
    Log::assets()->info(
        "AssetBrowserPanel: {} texturas, {} audios, {} meshes, {} prefabs, "
        "{} materiales, {} scripts listados",
        m_entries.size(), m_audioEntries.size(), m_meshEntries.size(),
        m_prefabEntries.size(), m_materialEntries.size(),
        m_scriptEntries.size());
}

void AssetBrowserPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_assetManager == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.assets.no_manager").c_str());
        ImGui::End();
        return;
    }

    if (!m_scanned) rescan();

    // F2H23 polish: header compacto — boton chico "R" (recargar) a la
    // derecha en lugar de un boton grande "Recargar" que se comia una
    // linea entera. Tooltip explicativo al hover.
    {
        const float rightOffset =
            ImGui::GetContentRegionAvail().x - 30.0f;
        if (rightOffset > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightOffset);
        }
        if (ImGui::SmallButton("R")) {
            rescan();
            // El reupload a GPU de las texturas cambiadas lo maneja
            // EditorApplication entre frames para no borrar un GLuint
            // que ImGui pueda estar a punto de usar.
            m_reloadRequested = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.panel.assets.reload_tooltip").c_str());
        }
    }

    // F2H22: tabs por categoria — antes eran CollapsingHeaders apilados
    // verticalmente, lo que con muchos meshes inflaba el panel a varios
    // viewports de altura. Cada tab tiene scroll interno (BeginChild) si
    // su contenido excede el alto disponible.
    if (ImGui::BeginTabBar("##asset_tabs",
                            ImGuiTabBarFlags_None)) {

        // ============================================================
        // TAB: Texturas (con grid de miniaturas)
        // ============================================================
        const std::string texTabLabel = std::string(ICON_FA_IMAGE " ") +
            I18n::T("editor.panel.assets.tab.textures");
        if (ImGui::BeginTabItem(texTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.textures",
                        m_entries.size()).c_str());
            ImGui::BeginChild("##texturas_scroll", ImVec2(0.0f, 0.0f), false);

            const float avail = ImGui::GetContentRegionAvail().x;
            const float cell = k_thumbSize + 12.0f;
            int cols = std::max(1, static_cast<int>(avail / cell));

            for (size_t i = 0; i < m_entries.size(); ++i) {
                const Entry& e = m_entries[i];
                ITexture* tex = m_assetManager->getTexture(e.id);
                if (tex == nullptr) continue;

                ImGui::PushID(static_cast<int>(i));
                ImGui::BeginGroup();

                const bool isSelected = m_selected.has_value() &&
                                          *m_selected == e.logicalPath;
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                            ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
                }
                if (ImGui::ImageButton("##thumb", tex->handle(),
                                        ImVec2(k_thumbSize, k_thumbSize),
                                        ImVec2(0, 1), ImVec2(1, 0))) {
                    m_selected = e.logicalPath;
                    Log::assets()->info(
                        "AssetBrowserPanel: seleccionado '{}'", e.logicalPath);
                }
                if (isSelected) ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MOOD_TEXTURE_ASSET",
                                                &e.id, sizeof(e.id));
                    ImGui::Image(tex->handle(), ImVec2(48.0f, 48.0f),
                                  ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::SameLine();
                    ImGui::TextUnformatted(e.displayName.c_str());
                    ImGui::EndDragDropSource();
                }

                const float textW = ImGui::CalcTextSize(e.displayName.c_str()).x;
                if (textW <= k_thumbSize) {
                    ImGui::TextUnformatted(e.displayName.c_str());
                } else {
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

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // TAB: Meshes
        // ============================================================
        const std::string meshTabLabel = std::string(ICON_FA_CUBE " ") +
            I18n::T("editor.panel.assets.tab.meshes");
        if (ImGui::BeginTabItem(meshTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.meshes",
                        m_meshEntries.size()).c_str());
            ImGui::BeginChild("##meshes_scroll", ImVec2(0.0f, 0.0f), false);
            for (const auto& me : m_meshEntries) {
                MeshAsset* asset = m_assetManager->getMesh(me.id);
                ImGui::PushID(me.logicalPath.c_str());
                ImGui::Selectable(me.displayName.c_str(), false);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MOOD_MESH_ASSET", &me.id, sizeof(me.id));
                    ImGui::TextUnformatted(me.displayName.c_str());
                    ImGui::EndDragDropSource();
                }
                if (asset != nullptr) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s",
                        I18n::T("editor.panel.assets.mesh_meta",
                                static_cast<u32>(asset->submeshes.size()),
                                asset->totalVertexCount()).c_str());
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // TAB: Prefabs
        // ============================================================
        const std::string prefabTabLabel = std::string(ICON_FA_BOX_OPEN " ") +
            I18n::T("editor.panel.assets.tab.prefabs");
        if (ImGui::BeginTabItem(prefabTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.prefabs",
                        m_prefabEntries.size()).c_str());
            ImGui::BeginChild("##prefabs_scroll", ImVec2(0.0f, 0.0f), false);
            for (const auto& pe : m_prefabEntries) {
                ImGui::PushID(pe.logicalPath.c_str());
                ImGui::Selectable(pe.displayName.c_str(), false);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MOOD_PREFAB_ASSET",
                                                &pe.id, sizeof(pe.id));
                    ImGui::TextUnformatted(pe.displayName.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.assets.kind.prefab").c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // TAB: Materiales
        // ============================================================
        const std::string matTabLabel = std::string(ICON_FA_PALETTE " ") +
            I18n::T("editor.panel.assets.tab.materials");
        if (ImGui::BeginTabItem(matTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.materials",
                        m_materialEntries.size()).c_str());
            ImGui::BeginChild("##materiales_scroll", ImVec2(0.0f, 0.0f), false);
            for (const auto& me : m_materialEntries) {
                ImGui::PushID(me.logicalPath.c_str());
                ImGui::Selectable(me.displayName.c_str(), false);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("MOOD_MATERIAL_ASSET",
                                                &me.id, sizeof(me.id));
                    ImGui::TextUnformatted(me.displayName.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.assets.kind.material").c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // TAB: Scripts
        // ============================================================
        const std::string scriptTabLabel = std::string(ICON_FA_FILE_CODE " ") +
            I18n::T("editor.panel.assets.tab.scripts");
        if (ImGui::BeginTabItem(scriptTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.scripts",
                        m_scriptEntries.size()).c_str());
            ImGui::BeginChild("##scripts_scroll", ImVec2(0.0f, 0.0f), false);
            for (const auto& se : m_scriptEntries) {
                ImGui::PushID(se.logicalPath.c_str());
                ImGui::Selectable(se.displayName.c_str(), false);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    constexpr int kPayloadBufSize = 256;
                    char buf[kPayloadBufSize] = {0};
                    const auto n = std::min(se.logicalPath.size(),
                                              static_cast<size_t>(kPayloadBufSize - 1));
                    std::memcpy(buf, se.logicalPath.data(), n);
                    ImGui::SetDragDropPayload("MOOD_SCRIPT_ASSET",
                                                buf, kPayloadBufSize);
                    ImGui::TextUnformatted(se.displayName.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.assets.script_lines",
                            se.lineCount).c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // TAB: Audio
        // ============================================================
        const std::string audioTabLabel = std::string(ICON_FA_MUSIC " ") +
            I18n::T("editor.panel.assets.tab.audio");
        if (ImGui::BeginTabItem(audioTabLabel.c_str())) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.assets.count.clips",
                        m_audioEntries.size()).c_str());
            ImGui::BeginChild("##audio_scroll", ImVec2(0.0f, 0.0f), false);
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
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace Mood
