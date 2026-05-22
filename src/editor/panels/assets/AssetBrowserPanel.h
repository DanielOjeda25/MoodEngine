#pragma once

// Panel que lista las texturas de assets/textures/ como grid de miniaturas
// (Hito 5 Bloque 4). Click sobre una miniatura selecciona el path logico;
// el callsite puede leer `selected()` y, p.ej., asignarlo a un tile del
// mapa. Drag & drop entra en Hito 6.

#include "editor/panels/IPanel.h"
#include "engine/assets/manager/AssetManager.h"

#include <optional>
#include <string>
#include <vector>

namespace Mood {

class MeshThumbnailRenderer;       // F2H80
class AnimationPreviewRenderer;    // F2H81
class MaterialPreviewRenderer;     // F2H81

class AssetBrowserPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Asset Browser"; }
    const char* category() const override { return "Assets"; }

    /// @brief El panel queda inerte (muestra "Asset manager no inyectado")
    ///        hasta que `EditorApplication` le pasa el manager real.
    void setAssetManager(AssetManager* am) { m_assetManager = am; }

    /// @brief F2H80: renderer de miniaturas 3D (inyectado por EditorApplication,
    ///        non-owning). Si es null, la sección de meshes cae al listado de
    ///        texto sin preview.
    void setThumbnailRenderer(MeshThumbnailRenderer* t) { m_thumbnails = t; }

    /// @brief F2H81: preview de animaciones (inyectado por EditorApplication,
    ///        non-owning). Si es null, el tab Animations cae al listado de texto.
    void setAnimationPreviewRenderer(AnimationPreviewRenderer* a) { m_animPreview = a; }

    /// @brief F2H81: preview de materiales (esfera, miniaturas cacheadas).
    ///        Non-owning. Null → tab Materiales cae al listado de texto.
    void setMaterialPreviewRenderer(MaterialPreviewRenderer* m) { m_matPreview = m; }

    /// @brief Path logico (relativo a la raiz de assets) del item seleccionado
    ///        por click en este panel. `nullopt` si nada esta seleccionado.
    const std::optional<std::string>& selected() const { return m_selected; }

    /// @brief `true` una unica vez despues de que el usuario apreto Recargar.
    ///        El consumidor (EditorApplication) debe llamar
    ///        `AssetManager::reloadChanged()` entre frames para upload seguro.
    bool consumeReloadRequest() {
        const bool r = m_reloadRequested;
        m_reloadRequested = false;
        return r;
    }

    /// @brief Re-escanea texturas / audio / meshes / prefabs. Publico para
    ///        que `EditorApplication` lo invoque tras guardar un prefab
    ///        nuevo (asi aparece en la sección sin reabrir el editor).
    void rescan();

private:
    // F2H81 (auditoría): cada tab del browser se renderea en su propio
    // método (cuerpo en AssetBrowserPanel_Tabs.cpp). `onImGuiRender` queda
    // como shell del TabBar. Cada método arma su BeginTabItem/EndTabItem.
    void renderTexturesTab();
    void renderMeshesTab();
    void renderVehiclesTab();
    void renderAnimationsTab();
    void renderPrefabsTab();
    void renderMaterialsTab();
    void renderScriptsTab();
    void renderAudioTab();

    struct Entry {
        std::string logicalPath; // "textures/foo.png"
        std::string displayName; // "foo.png"
        TextureAssetId id = 0;   // cargado en scan
    };

    struct AudioEntry {
        std::string logicalPath; // "audio/foo.wav"
        std::string displayName; // "foo.wav"
        AudioAssetId id = 0;
    };

    struct MeshEntry {
        std::string logicalPath; // "meshes/foo.obj"
        std::string displayName; // "foo.obj"
        MeshAssetId id = 0;
    };

    struct PrefabEntry {
        std::string logicalPath; // "prefabs/foo.moodprefab"
        std::string displayName; // "foo.moodprefab"
        PrefabAssetId id = 0;
    };

    struct MaterialEntry {
        std::string logicalPath; // "materials/foo.material"
        std::string displayName; // "foo.material"
        MaterialAssetId id = 0;
    };

    // Hito 22 Bloque 1: scripts Lua. No tienen un AssetId en AssetManager
    // (se cargan on-demand por ScriptSystem desde el path). Guardamos el
    // line count como metadata para el browser.
    struct ScriptEntry {
        std::string logicalPath; // "scripts/foo.lua"
        std::string displayName; // "foo.lua"
        u32 lineCount = 0;
    };

    // F2H49: clips de animacion standalone (FBX anim-only, convencion
    // `anim_*.fbx`). El AssetManager los carga via MeshLoader_StandaloneClip
    // y devuelve un `AnimationClipAssetId` que apunta al cache compartido.
    struct AnimationClipEntry {
        std::string logicalPath; // "characters/player/anim_walk.fbx"
        std::string displayName; // "player/anim_walk.fbx"
        AnimationClipAssetId id = 0;
    };

    // F2H70.3 Bloque F: catalogo de vehiculos `.moodvehicle`. La metadata
    // (name / mass / hp) se parsea del JSON al escanear — el VehicleConfig
    // struct no guarda el bloque `metadata`, asi que leemos el JSON directo
    // para mostrar nombres legibles en el browser. El `id` carga el config
    // via AssetManager para validar que parsea OK (fallback a generico si no).
    struct VehicleEntry {
        std::string logicalPath; // "vehicles/delorean/delorean_dmc12.moodvehicle"
        std::string displayName; // "delorean/delorean_dmc12.moodvehicle"
        std::string vehicleName; // metadata.name -> "DeLorean DMC-12"
        f32 massKg = 0.0f;       // body.mass_kg
        f32 horsepower = 0.0f;   // engine.horsepower
        VehicleConfigAssetId id = 0;
    };

    AssetManager* m_assetManager = nullptr;
    MeshThumbnailRenderer* m_thumbnails = nullptr;  // F2H80, non-owning
    AnimationPreviewRenderer* m_animPreview = nullptr;  // F2H81, non-owning
    MaterialPreviewRenderer* m_matPreview = nullptr;    // F2H81, non-owning
    // F2H81: estado del preview de animaciones (tab Animations).
    AnimationClipAssetId m_animPreviewClip = 0;  // clip seleccionado (0 = ninguno)
    MeshAssetId m_animPreviewNpc = 0;            // NPC de referencia (lazy-load)
    f32 m_animPreviewTime = 0.0f;                // tiempo de reproducción (loop)
    std::vector<Entry> m_entries;
    std::vector<AudioEntry> m_audioEntries;
    std::vector<MeshEntry> m_meshEntries;
    std::vector<PrefabEntry> m_prefabEntries;
    std::vector<MaterialEntry> m_materialEntries;
    std::vector<ScriptEntry> m_scriptEntries;
    std::vector<AnimationClipEntry> m_animClipEntries;
    std::vector<VehicleEntry> m_vehicleEntries;
    std::optional<std::string> m_selected;
    bool m_scanned = false;
    bool m_reloadRequested = false;
};

} // namespace Mood
