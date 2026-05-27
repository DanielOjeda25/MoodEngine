#include "editor/panels/assets/AssetBrowserPanel.h"

#include "core/Log.h"
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en tabs + filas
#include "engine/animation/clips/AnimationClip.h"  // F2H49: metadata de clips
#include "engine/audio/clips/AudioClip.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/preview/MeshThumbnailRenderer.h"  // F2H80
#include "engine/render/preview/AnimationPreviewRenderer.h"  // F2H81
#include "engine/render/preview/MaterialPreviewRenderer.h"  // F2H81
#include "engine/physics/vehicle/VehicleConfig.h"  // F2H81: meshPath del vehículo
#include "engine/render/resources/MaterialAsset.h"  // F2H81: getMaterial

#include <imgui.h>
#include <nlohmann/json.hpp>  // F2H70.3: parse metadata del .moodvehicle

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace Mood {

namespace {

// Directorio relativo al cwd donde buscamos texturas. Cuando el VFS soporte
// lookup inverso, esto sale de ahi.
constexpr const char* k_textureDir    = "assets/textures";
constexpr const char* k_audioDir      = "assets/audio";
constexpr const char* k_meshDir       = "assets/meshes";
constexpr const char* k_charactersDir = "assets/characters";  // F2H49
constexpr const char* k_prefabDir     = "assets/prefabs";
constexpr const char* k_materialDir   = "assets/materials";
constexpr const char* k_scriptDir     = "assets/scripts";
constexpr const char* k_vehicleDir    = "assets/vehicles";  // F2H70.3
constexpr float k_thumbSize = 64.0f;
constexpr const char* k_logicalPrefix           = "textures/";
constexpr const char* k_audioLogicalPrefix      = "audio/";
constexpr const char* k_meshLogicalPrefix       = "meshes/";
constexpr const char* k_charactersLogicalPrefix = "characters/";  // F2H49
constexpr const char* k_prefabLogicalPrefix     = "prefabs/";
constexpr const char* k_materialLogicalPrefix   = "materials/";
constexpr const char* k_scriptLogicalPrefix     = "scripts/";
constexpr const char* k_vehicleLogicalPrefix    = "vehicles/";  // F2H70.3

// F2H81 (auditoría): los helpers visuales de card (bigIconButton, cardLabel,
// cardGridCols) viven en AssetBrowserPanel_Internal.h — los usan los
// render*Tab() de AssetBrowserPanel_Tabs.cpp.

// F2H81 (auditoría/DRY): un solo helper para la extensión en minúsculas, en
// vez de repetir el `transform(tolower)` en cada filtro `isX`.
std::string lowerExt(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool isPng(const std::filesystem::path& p) { return lowerExt(p) == ".png"; }

bool isAudio(const std::filesystem::path& p) {
    const std::string e = lowerExt(p);
    return e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac";
}

bool isMesh(const std::filesystem::path& p) {
    const std::string e = lowerExt(p);
    return e == ".obj" || e == ".gltf" || e == ".glb" || e == ".fbx";
}

// F2H49: archivos `anim_*.fbx` son clips standalone (Mixamo "Without Skin"),
// no meshes con esqueleto. El tab Meshes los filtra y el tab Animations los
// recoge — asi un anim_walk.fbx no aparece como "mesh vacio" en el browser.
bool isAnimClip(const std::filesystem::path& p) {
    if (lowerExt(p) != ".fbx") return false;
    auto stem = p.stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return stem.rfind("anim_", 0) == 0;
}

bool isPrefab(const std::filesystem::path& p) { return lowerExt(p) == ".moodprefab"; }
bool isMaterial(const std::filesystem::path& p) { return lowerExt(p) == ".material"; }
bool isLuaScript(const std::filesystem::path& p) { return lowerExt(p) == ".lua"; }
bool isMoodVehicle(const std::filesystem::path& p) { return lowerExt(p) == ".moodvehicle"; }

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

    // F2H80/F2H81: invalidar miniaturas cacheadas — un re-scan implica que un
    // asset pudo cambiar (material editado, mesh reimportado, etc.).
    if (m_thumbnails != nullptr) m_thumbnails->clear();
    if (m_matPreview != nullptr) m_matPreview->clearThumbnailCache();

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
    //
    // F2H49: tambien escaneamos assets/characters/ (FBX de Mixamo). Mismo
    // tipo de asset (mesh con esqueleto + animaciones) pero separado del
    // directorio de meshes estaticos para mantener orden semantico. El
    // logicalPath conserva el prefijo "characters/" para distinguirlos.
    m_meshEntries.clear();
    const std::array<std::pair<const char*, const char*>, 2> meshDirs{{
        {k_meshDir,       k_meshLogicalPrefix},
        {k_charactersDir, k_charactersLogicalPrefix},
    }};
    for (const auto& [dir, prefix] : meshDirs) {
        std::error_code mesh_ec;
        auto mesh_it = std::filesystem::recursive_directory_iterator(dir, mesh_ec);
        if (mesh_ec) continue;
        for (const auto& entry : mesh_it) {
            if (!entry.is_regular_file() || !isMesh(entry.path())) continue;
            // F2H49: los `anim_*.fbx` los agarra el tab Animations.
            if (isAnimClip(entry.path())) continue;
            MeshEntry me;
            const auto rel = std::filesystem::relative(entry.path(), dir);
            me.displayName = rel.generic_string();
            me.logicalPath = std::string(prefix) + me.displayName;
            me.id = m_assetManager->loadMesh(me.logicalPath);
            m_meshEntries.push_back(std::move(me));
        }
    }
    std::sort(m_meshEntries.begin(), m_meshEntries.end(),
              [](const MeshEntry& a, const MeshEntry& b) {
                  return a.displayName < b.displayName;
              });

    // F2H49: clips standalone. Recursivo sobre assets/characters/ filtrando
    // por convencion de nombre `anim_*.fbx`. Los carga via
    // `AssetManager::loadAnimationClip` (MeshLoader_StandaloneClip) — devuelven
    // un AnimationClip con tracks `boneIndex=-1`; el bind contra un skeleton
    // concreto lo hace AnimationSystem on demand.
    m_animClipEntries.clear();
    std::error_code clip_ec;
    auto clip_it = std::filesystem::recursive_directory_iterator(
        k_charactersDir, clip_ec);
    if (!clip_ec) {
        for (const auto& entry : clip_it) {
            if (!entry.is_regular_file() || !isAnimClip(entry.path())) continue;
            AnimationClipEntry ce;
            const auto rel = std::filesystem::relative(entry.path(), k_charactersDir);
            ce.displayName = rel.generic_string();
            ce.logicalPath = std::string(k_charactersLogicalPrefix) + ce.displayName;
            ce.id = m_assetManager->loadAnimationClip(ce.logicalPath);
            m_animClipEntries.push_back(std::move(ce));
        }
        std::sort(m_animClipEntries.begin(), m_animClipEntries.end(),
                  [](const AnimationClipEntry& a, const AnimationClipEntry& b) {
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

    // F2H70.3 Bloque F: vehiculos `.moodvehicle`. Scan recursivo de
    // assets/vehicles/ (estructura asset-centric: 1 carpeta por vehiculo).
    // La metadata (name/mass/hp) se parsea del JSON directo — el
    // VehicleConfig no guarda el bloque `metadata`. El `id` carga el config
    // via AssetManager para validar (fallback a generico si el JSON es malo).
    m_vehicleEntries.clear();
    std::error_code veh_ec;
    auto veh_it = std::filesystem::recursive_directory_iterator(
        k_vehicleDir, veh_ec);
    if (!veh_ec) {
        for (const auto& entry : veh_it) {
            if (!entry.is_regular_file() || !isMoodVehicle(entry.path())) continue;
            VehicleEntry ve;
            const auto rel = std::filesystem::relative(entry.path(), k_vehicleDir);
            ve.displayName = rel.generic_string();
            ve.logicalPath = std::string(k_vehicleLogicalPrefix) + ve.displayName;
            ve.id = m_assetManager->loadVehicleConfig(ve.logicalPath);
            // Metadata legible: parseo liviano del JSON (best-effort).
            ve.vehicleName = entry.path().stem().string();  // fallback
            std::ifstream vf(entry.path());
            if (vf.good()) {
                try {
                    nlohmann::json vj;
                    vf >> vj;
                    if (vj.contains("metadata") && vj.at("metadata").is_object()) {
                        ve.vehicleName = vj.at("metadata").value("name", ve.vehicleName);
                    }
                    if (vj.contains("body") && vj.at("body").is_object()) {
                        ve.massKg = vj.at("body").value("mass_kg", 0.0f);
                    }
                    if (vj.contains("engine") && vj.at("engine").is_object()) {
                        ve.horsepower = vj.at("engine").value("horsepower", 0.0f);
                    }
                } catch (const std::exception&) {
                    // JSON malo — dejamos los defaults; el browser igual lo
                    // lista (con el filename) para que el dev lo vea y corrija.
                }
            }
            m_vehicleEntries.push_back(std::move(ve));
        }
        std::sort(m_vehicleEntries.begin(), m_vehicleEntries.end(),
                  [](const VehicleEntry& a, const VehicleEntry& b) {
                      return a.displayName < b.displayName;
                  });
    }

    m_scanned = true;
    Log::assets()->info(
        "AssetBrowserPanel: {} texturas, {} audios, {} meshes, {} prefabs, "
        "{} materiales, {} scripts, {} clips de animacion, {} vehiculos listados",
        m_entries.size(), m_audioEntries.size(), m_meshEntries.size(),
        m_prefabEntries.size(), m_materialEntries.size(),
        m_scriptEntries.size(), m_animClipEntries.size(),
        m_vehicleEntries.size());
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
    if (ImGui::BeginTabBar("##asset_tabs", ImGuiTabBarFlags_None)) {
        // F2H81 (auditoría): cada tab vive en su propio método
        // (AssetBrowserPanel_Tabs.cpp). El orden de las pestañas se preserva.
        renderTexturesTab();
        renderMeshesTab();
        renderVehiclesTab();
        renderAnimationsTab();
        renderPrefabsTab();
        renderMaterialsTab();
        renderScriptsTab();
        renderAudioTab();
        ImGui::EndTabBar();
    }

    // F3H19: modal de rename con cascada, single instance compartido por
    // todos los tabs. El context menu de cada item lo abre via
    // openRenameModal(logicalPath).
    drawRenameModal();

    ImGui::End();
}

} // namespace Mood
