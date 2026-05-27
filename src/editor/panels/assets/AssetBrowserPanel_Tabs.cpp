// F2H81 (auditoría): cuerpo de cada tab del Asset Browser. Extraído de
// AssetBrowserPanel.cpp (que pasaba las 800 líneas) — el shell del TabBar y
// rescan() quedan allá; cada `render*Tab()` arma su propio BeginTabItem.

#include "editor/panels/assets/AssetBrowserPanel.h"
#include "editor/panels/assets/AssetBrowserPanel_Internal.h"

#include "core/Log.h"
#include "core/UserSettings.h"  // F3H16: hoverPreviewDelayMs
#include "core/i18n/I18n.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/animation/clips/AnimationClip.h"
#include "engine/audio/clips/AudioClip.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/render/preview/AnimationPreviewRenderer.h"
#include "engine/render/preview/MaterialPreviewRenderer.h"
#include "engine/render/preview/MeshThumbnailRenderer.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/ITexture.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace Mood {

using assetbrowser_detail::bigIconButton;
using assetbrowser_detail::cardGridCols;
using assetbrowser_detail::cardLabel;

namespace {

// F3H16: FNV-1a 32-bit hash de strings, para una key estable per-item al
// trackear hover. Mas chico que el FNV-64 del AssetThumbnailDiskCache —
// 32 bits sobran para distinguir items en un panel.
u32 hashItemKey(const std::string& s) {
    constexpr u32 k_offset = 0x811c9dc5u;
    constexpr u32 k_prime  = 0x01000193u;
    u32 h = k_offset;
    for (unsigned char c : s) {
        h ^= c;
        h *= k_prime;
    }
    return h == 0u ? 1u : h;  // reservamos 0 para "ningun item"
}

}  // namespace

// F3H16: helper de hover prolongado. Llamado inmediatamente despues del
// ImageButton del thumb. Devuelve true cuando el cursor estuvo quieto
// sobre el item al menos `hoverPreviewDelayMs` ms consecutivos.
bool AssetBrowserPanel::hoverPreviewElapsed(u32 itemKey) {
    if (!ImGui::IsItemHovered()) {
        // El cursor no esta sobre este item este frame. Si era el ultimo
        // hovered, resetear el tracker — al volver a entrar arranca de 0.
        if (m_hoverItemKey == itemKey) {
            m_hoverItemKey  = 0;
            m_hoverTimerSec = 0.0f;
        }
        return false;
    }
    // Hover activo sobre este item.
    if (m_hoverItemKey != itemKey) {
        // Cambio de item — resetear el tracker. El usuario tiene que
        // dejar el cursor quieto sobre este item desde cero.
        m_hoverItemKey  = itemKey;
        m_hoverTimerSec = 0.0f;
    } else {
        m_hoverTimerSec += ImGui::GetIO().DeltaTime;
    }
    const f32 thresholdSec =
        static_cast<f32>(UserSettings::editor().hoverPreviewDelayMs) / 1000.0f;
    return m_hoverTimerSec >= thresholdSec;
}

// ============================================================
// TAB: Texturas (grid de miniaturas)
// ============================================================
void AssetBrowserPanel::renderTexturesTab() {
    const std::string label = std::string(ICON_FA_IMAGE " ") +
        I18n::T("editor.panel.assets.tab.textures");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    constexpr float kThumb = 64.0f;
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.textures", m_entries.size()).c_str());
    ImGui::BeginChild("##texturas_scroll", ImVec2(0.0f, 0.0f), false);

    const int cols = cardGridCols(kThumb);
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        ITexture* tex = m_assetManager->getTexture(e.id);
        if (tex == nullptr) continue;

        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();

        const bool isSelected = m_selected.has_value() && *m_selected == e.logicalPath;
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
        }
        if (ImGui::ImageButton("##thumb", tex->handle(),
                                ImVec2(kThumb, kThumb), ImVec2(0, 1), ImVec2(1, 0))) {
            m_selected = e.logicalPath;
            Log::assets()->info("AssetBrowserPanel: seleccionado '{}'", e.logicalPath);
        }
        if (isSelected) ImGui::PopStyleColor();

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_TEXTURE_ASSET", &e.id, sizeof(e.id));
            ImGui::Image(tex->handle(), ImVec2(48.0f, 48.0f), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::TextUnformatted(e.displayName.c_str());
            ImGui::EndDragDropSource();
        }

        // Textura conserva tooltip al truncar (el resto de tabs usa cardLabel).
        const float textW = ImGui::CalcTextSize(e.displayName.c_str()).x;
        if (textW <= kThumb) {
            ImGui::TextUnformatted(e.displayName.c_str());
        } else {
            std::string truncated = e.displayName;
            while (!truncated.empty() &&
                    ImGui::CalcTextSize((truncated + "..").c_str()).x > kThumb) {
                truncated.pop_back();
            }
            ImGui::Text("%s..", truncated.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.displayName.c_str());
        }

        ImGui::EndGroup();
        ImGui::PopID();

        if (static_cast<int>((i + 1) % cols) != 0) ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Meshes (grid con miniatura 3D, F2H80)
// ============================================================
void AssetBrowserPanel::renderMeshesTab() {
    const std::string label = std::string(ICON_FA_CUBE " ") +
        I18n::T("editor.panel.assets.tab.meshes");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.meshes", m_meshEntries.size()).c_str());
    ImGui::BeginChild("##meshes_scroll", ImVec2(0.0f, 0.0f), false);

    constexpr float kThumb = 80.0f;
    const int cols = cardGridCols(kThumb);
    int drawn = 0;
    for (const auto& me : m_meshEntries) {
        MeshAsset* asset = m_assetManager->getMesh(me.id);
        const GLuint thumb = (m_thumbnails != nullptr)
            ? m_thumbnails->thumbnailFor(me.id, *m_assetManager) : 0u;

        ImGui::PushID(me.logicalPath.c_str());
        ImGui::BeginGroup();

        const bool isSelected = m_selected.has_value() && *m_selected == me.logicalPath;
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
        }
        bool clicked = false;
        if (thumb != 0u) {
            // FBO color texture: bottom-up → uv flip (0,1)-(1,0).
            clicked = ImGui::ImageButton("##meshthumb", (ImTextureID)(uintptr_t)thumb,
                            ImVec2(kThumb, kThumb), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            clicked = ImGui::Button("##meshnothumb", ImVec2(kThumb, kThumb));
        }
        if (isSelected) ImGui::PopStyleColor();
        if (clicked) m_selected = me.logicalPath;

        // Drag-source: arrastrar al viewport spawnea la entidad.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_MESH_ASSET", &me.id, sizeof(me.id));
            if (thumb != 0u) {
                ImGui::Image((ImTextureID)(uintptr_t)thumb, ImVec2(48.0f, 48.0f),
                              ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
            }
            ImGui::TextUnformatted(me.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        // F3H16: tooltip ampliado al hover prolongado — preview 384x384 del
        // mesh + metadata. Si el hover es corto (debajo de hoverPreviewDelayMs),
        // cae al tooltip simple legacy.
        if (asset != nullptr && m_thumbnails != nullptr &&
            hoverPreviewElapsed(hashItemKey(me.logicalPath))) {
            const GLuint large = m_thumbnails->thumbnailLargeFor(
                me.id, *m_assetManager);
            ImGui::BeginTooltip();
            if (large != 0u) {
                ImGui::Image((ImTextureID)(uintptr_t)large,
                              ImVec2(384.0f, 384.0f),
                              ImVec2(0, 1), ImVec2(1, 0));
            }
            ImGui::TextUnformatted(me.displayName.c_str());
            ImGui::TextDisabled("%s", me.logicalPath.c_str());
            ImGui::Separator();
            ImGui::Text("%s",
                I18n::T("editor.panel.assets.mesh_meta",
                        static_cast<u32>(asset->submeshes.size()),
                        asset->totalVertexCount()).c_str());
            const glm::vec3 ext = asset->aabbMax - asset->aabbMin;
            ImGui::Text("AABB: %.2f x %.2f x %.2f", ext.x, ext.y, ext.z);
            if (asset->hasSkeleton()) {
                ImGui::Text("%s", I18n::T(
                    "editor.panel.assets.mesh_meta_skeleton",
                    static_cast<u32>(asset->animations.size())).c_str());
            }
            ImGui::EndTooltip();
        } else if (ImGui::IsItemHovered() && asset != nullptr) {
            ImGui::SetTooltip("%s\n%s", me.displayName.c_str(),
                I18n::T("editor.panel.assets.mesh_meta",
                        static_cast<u32>(asset->submeshes.size()),
                        asset->totalVertexCount()).c_str());
        }

        cardLabel(me.displayName, kThumb);

        ImGui::EndGroup();
        ImGui::PopID();

        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Vehículos (F2H70.3 Bloque F; miniatura 3D F2H81)
// ============================================================
void AssetBrowserPanel::renderVehiclesTab() {
    const std::string label = std::string(ICON_FA_GAUGE " ") + "Vehiculos";
    if (!ImGui::BeginTabItem(label.c_str())) return;

    // F2H82: boton para abrir el modal de importacion (.glb/.fbx -> .moodvehicle).
    if (ImGui::Button("+ Importar...##veh_import")) {
        openImportVehicleModal();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Importar un modelo de auto (.glb/.fbx) y generar su .moodvehicle.");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu vehiculos", m_vehicleEntries.size());

    // Dibuja el modal si esta abierto (la llamada es cheap si no lo esta).
    drawImportVehicleModal();
    // F2H82: modal de confirmacion de borrado (cheap si no hay pending).
    confirmAndDeleteVehicle();

    ImGui::BeginChild("##vehicles_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 110.0f;
    const int cols = cardGridCols(kCard);
    int drawn = 0;
    for (const auto& ve : m_vehicleEntries) {
        // Miniatura 3D del mesh del vehículo (reusa el thumbnail renderer de
        // meshes). Cae al ícono de tablero si no hay mesh.
        GLuint tex = 0u;
        if (m_thumbnails != nullptr) {
            const vehicle::VehicleConfig* cfg = m_assetManager->getVehicleConfig(ve.id);
            if (cfg != nullptr && !cfg->meshPath.empty()) {
                const MeshAssetId mid = m_assetManager->loadMesh(cfg->meshPath);
                tex = m_thumbnails->thumbnailFor(mid, *m_assetManager);
            }
        }

        ImGui::PushID(ve.logicalPath.c_str());
        ImGui::BeginGroup();
        const bool isSelected = m_selected.has_value() && *m_selected == ve.logicalPath;
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
        }
        bool clicked = false;
        if (tex != 0u) {
            clicked = ImGui::ImageButton("##vehthumb", (ImTextureID)(uintptr_t)tex,
                            ImVec2(kCard, kCard), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            clicked = bigIconButton(ICON_FA_GAUGE, kCard);
        }
        if (isSelected) ImGui::PopStyleColor();
        if (clicked) m_selected = ve.logicalPath;

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            constexpr int kPayloadBufSize = 256;
            char buf[kPayloadBufSize] = {0};
            const auto n = std::min(ve.logicalPath.size(),
                                      static_cast<size_t>(kPayloadBufSize - 1));
            std::memcpy(buf, ve.logicalPath.data(), n);
            ImGui::SetDragDropPayload("MOOD_VEHICLE_ASSET", buf, kPayloadBufSize);
            if (tex != 0u) {
                ImGui::Image((ImTextureID)(uintptr_t)tex, ImVec2(48.0f, 48.0f),
                              ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
            }
            ImGui::TextUnformatted(ve.vehicleName.c_str());
            ImGui::EndDragDropSource();
        }
        // F2H82: right-click sobre la card abre menu contextual con "Eliminar".
        // La eliminacion pasa por un modal de confirmacion (no se borra al toque).
        if (ImGui::BeginPopupContextItem("##veh_ctx_menu")) {
            if (ImGui::MenuItem("Eliminar...")) {
                m_pendingDeleteVehicle = ve.logicalPath;
            }
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered()) {
            if (ve.massKg > 0.0f || ve.horsepower > 0.0f) {
                ImGui::SetTooltip("%s\n[%.0f kg, %.0f HP]",
                                  ve.vehicleName.c_str(), ve.massKg, ve.horsepower);
            } else {
                ImGui::SetTooltip("%s", ve.vehicleName.c_str());
            }
        }
        cardLabel(ve.vehicleName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();
        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Animations (F2H49; hover-to-play F2H81)
// ============================================================
void AssetBrowserPanel::renderAnimationsTab() {
    const std::string label = std::string(ICON_FA_PERSON_RUNNING " ") +
        I18n::T("editor.panel.assets.tab.animations");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.anim_clips", m_animClipEntries.size()).c_str());

    // Grilla de cards, una por clip, posadas sobre el NPC de referencia
    // (Mixamo). La card con el MOUSE ENCIMA se reproduce en vivo; el resto
    // muestra una miniatura estática cacheada. Hover con 1 frame de lag.
    const bool hasPreview = (m_animPreview != nullptr);
    if (hasPreview && m_animPreviewNpc == 0) {
        m_animPreviewNpc = m_assetManager->loadMesh("characters/npc/npc.fbx");
    }
    m_animPreviewTime += ImGui::GetIO().DeltaTime;

    ImGui::BeginChild("##anim_clips_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 110.0f;
    const int cols = cardGridCols(kCard);
    AnimationClipAssetId nextHover = 0;
    int drawn = 0;
    for (const auto& ce : m_animClipEntries) {
        AnimationClip* clip = m_assetManager->getAnimationClip(ce.id);
        GLuint tex = 0u;
        if (hasPreview && m_animPreviewNpc != 0) {
            tex = (ce.id == m_animPreviewClip)  // hovered el frame pasado → vivo
                ? m_animPreview->renderClip(m_animPreviewNpc, ce.id,
                                            m_animPreviewTime, *m_assetManager)
                : m_animPreview->staticThumbnail(m_animPreviewNpc, ce.id,
                                                 *m_assetManager);
        }

        ImGui::PushID(ce.logicalPath.c_str());
        ImGui::BeginGroup();
        if (tex != 0u) {
            ImGui::ImageButton("##animthumb", (ImTextureID)(uintptr_t)tex,
                               ImVec2(kCard, kCard), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImGui::Button("##animnothumb", ImVec2(kCard, kCard));
        }
        if (ImGui::IsItemHovered()) nextHover = ce.id;

        // Drag-source a la entidad (intacto).
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_ANIMCLIP_ASSET", &ce.id, sizeof(ce.id));
            ImGui::TextUnformatted(ce.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered() && clip != nullptr) {
            ImGui::SetTooltip("%s\n[%u tracks, %.2fs]", ce.displayName.c_str(),
                              static_cast<u32>(clip->tracks.size()), clip->duration);
        }

        cardLabel(ce.displayName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();

        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();

    // Actualizar el clip que se reproduce en vivo (resetea el tiempo al cambiar).
    if (nextHover != m_animPreviewClip) {
        m_animPreviewClip = nextHover;
        m_animPreviewTime = 0.0f;
    }
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Prefabs
// ============================================================
void AssetBrowserPanel::renderPrefabsTab() {
    const std::string label = std::string(ICON_FA_BOX_OPEN " ") +
        I18n::T("editor.panel.assets.tab.prefabs");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.prefabs", m_prefabEntries.size()).c_str());
    ImGui::BeginChild("##prefabs_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 96.0f;
    const int cols = cardGridCols(kCard);
    int drawn = 0;
    for (const auto& pe : m_prefabEntries) {
        ImGui::PushID(pe.logicalPath.c_str());
        ImGui::BeginGroup();
        bigIconButton(ICON_FA_BOX_OPEN, kCard);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_PREFAB_ASSET", &pe.id, sizeof(pe.id));
            ImGui::TextUnformatted(pe.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", pe.displayName.c_str());
        cardLabel(pe.displayName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();
        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Materiales (miniatura de esfera cacheada, F2H81)
// ============================================================
void AssetBrowserPanel::renderMaterialsTab() {
    const std::string label = std::string(ICON_FA_PALETTE " ") +
        I18n::T("editor.panel.assets.tab.materials");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.materials", m_materialEntries.size()).c_str());
    ImGui::BeginChild("##materiales_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 96.0f;
    const int cols = cardGridCols(kCard);
    int drawn = 0;
    for (const auto& me : m_materialEntries) {
        GLuint tex = (m_matPreview != nullptr)
            ? m_matPreview->thumbnail(me.id, *m_assetManager) : 0u;

        ImGui::PushID(me.logicalPath.c_str());
        ImGui::BeginGroup();
        if (tex != 0u) {
            ImGui::ImageButton("##matthumb", (ImTextureID)(uintptr_t)tex,
                               ImVec2(kCard, kCard), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            bigIconButton(ICON_FA_PALETTE, kCard);
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("MOOD_MATERIAL_ASSET", &me.id, sizeof(me.id));
            ImGui::TextUnformatted(me.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        // F3H16: tooltip ampliado al hover prolongado — esfera 384x384 +
        // metadata del material.
        MaterialAsset* matAsset = m_assetManager
            ? m_assetManager->getMaterial(me.id) : nullptr;
        if (matAsset != nullptr && m_matPreview != nullptr &&
            hoverPreviewElapsed(hashItemKey(me.logicalPath))) {
            const GLuint large = m_matPreview->thumbnailLarge(
                me.id, *m_assetManager);
            ImGui::BeginTooltip();
            if (large != 0u) {
                ImGui::Image((ImTextureID)(uintptr_t)large,
                              ImVec2(384.0f, 384.0f),
                              ImVec2(0, 1), ImVec2(1, 0));
            }
            ImGui::TextUnformatted(me.displayName.c_str());
            ImGui::TextDisabled("%s", me.logicalPath.c_str());
            ImGui::Separator();
            ImGui::Text("Albedo: (%.2f, %.2f, %.2f)",
                          matAsset->albedoTint.x,
                          matAsset->albedoTint.y,
                          matAsset->albedoTint.z);
            ImGui::Text("Metallic: %.2f", matAsset->metallicMult);
            ImGui::Text("Roughness: %.2f", matAsset->roughnessMult);
            ImGui::Text("AO: %.2f", matAsset->aoMult);
            int mapCount = 0;
            if (matAsset->useAlbedoMap)       ++mapCount;
            if (matAsset->metallicRoughness)  ++mapCount;
            if (matAsset->normal)             ++mapCount;
            if (matAsset->ao)                 ++mapCount;
            ImGui::Text("%s",
                I18n::T("editor.panel.assets.material_meta_maps",
                        mapCount).c_str());
            ImGui::EndTooltip();
        } else if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", me.displayName.c_str());
        }
        cardLabel(me.displayName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();
        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Scripts
// ============================================================
void AssetBrowserPanel::renderScriptsTab() {
    const std::string label = std::string(ICON_FA_FILE_CODE " ") +
        I18n::T("editor.panel.assets.tab.scripts");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.scripts", m_scriptEntries.size()).c_str());
    ImGui::BeginChild("##scripts_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 96.0f;
    const int cols = cardGridCols(kCard);
    int drawn = 0;
    for (const auto& se : m_scriptEntries) {
        ImGui::PushID(se.logicalPath.c_str());
        ImGui::BeginGroup();
        bigIconButton(ICON_FA_FILE_CODE, kCard);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            constexpr int kPayloadBufSize = 256;
            char buf[kPayloadBufSize] = {0};
            const auto n = std::min(se.logicalPath.size(),
                                      static_cast<size_t>(kPayloadBufSize - 1));
            std::memcpy(buf, se.logicalPath.data(), n);
            ImGui::SetDragDropPayload("MOOD_SCRIPT_ASSET", buf, kPayloadBufSize);
            ImGui::TextUnformatted(se.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s", se.displayName.c_str(),
                I18n::T("editor.panel.assets.script_lines", se.lineCount).c_str());
        }
        cardLabel(se.displayName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();
        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

// ============================================================
// TAB: Audio
// ============================================================
void AssetBrowserPanel::renderAudioTab() {
    const std::string label = std::string(ICON_FA_MUSIC " ") +
        I18n::T("editor.panel.assets.tab.audio");
    if (!ImGui::BeginTabItem(label.c_str())) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.assets.count.clips", m_audioEntries.size()).c_str());
    ImGui::BeginChild("##audio_scroll", ImVec2(0.0f, 0.0f), false);
    constexpr float kCard = 96.0f;
    const int cols = cardGridCols(kCard);
    int drawn = 0;
    for (const auto& ae : m_audioEntries) {
        AudioClip* clip = m_assetManager->getAudio(ae.id);
        if (clip == nullptr) continue;
        ImGui::PushID(ae.logicalPath.c_str());
        ImGui::BeginGroup();
        bigIconButton(ICON_FA_MUSIC, kCard);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n[%.2fs, %uHz, %uch]", ae.displayName.c_str(),
                              clip->durationSeconds(), clip->sampleRate(),
                              clip->channels());
        }
        cardLabel(ae.displayName, kCard);
        ImGui::EndGroup();
        ImGui::PopID();
        if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
        ++drawn;
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}

} // namespace Mood
