#pragma once

// Panel que lista las texturas de assets/textures/ como grid de miniaturas
// (Hito 5 Bloque 4). Click sobre una miniatura selecciona el path logico;
// el callsite puede leer `selected()` y, p.ej., asignarlo a un tile del
// mapa. Drag & drop entra en Hito 6.

#include "editor/panels/IPanel.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/assets/refs/AssetRefIndex.h"             // F3H19
#include "engine/physics/vehicle/VehicleMeshAnalyzer.h"  // F2H82
#include "engine/physics/vehicle/VehiclePresets.h"       // F2H82

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class MeshThumbnailRenderer;       // F2H80
class AnimationPreviewRenderer;    // F2H81
class MaterialPreviewRenderer;     // F2H81
class Scene;                       // F3H19

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

    /// @brief F3H19: inyecta la Scene activa para que el rename con cascada
    ///        pueda escanear refs en componentes. Non-owning.
    void setScene(Scene* s) { m_scene = s; }

    /// @brief F3H19: pending rename request. EditorApplication la consume en
    ///        pumpUiRequests para construir el RenameAssetCommand y push al
    ///        history. Single-frame consume.
    struct PendingRename {
        std::filesystem::path oldDiskPath;
        std::filesystem::path newDiskPath;
        std::string oldLogical;
        std::string newLogical;
        std::vector<asset_refs::RefSite> refs;
    };

    std::optional<PendingRename> consumePendingRename() {
        if (!m_pendingRename.has_value()) return std::nullopt;
        auto out = std::move(m_pendingRename);
        m_pendingRename.reset();
        return out;
    }

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
    void renderWeaponsTab();           // F4H2 Bloque B

    // F2H82: modal de "Importar vehiculo". File picker → analyzer → form de
    // preset (clase + ajuste fino) → writer .moodvehicle + rescan. Cuerpo en
    // AssetBrowserPanel_ImportVehicle.cpp.
    void openImportVehicleModal();
    void drawImportVehicleModal();
    bool saveImportedVehicle(std::string& err);

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

    // F3H16: tracker manual del tiempo de hover sobre el item activo. Se
    // mantiene entre frames; al cambiar el item hovered, se resetea. Solo
    // uno puede estar hovered a la vez en ImGui, asi que el state global
    // del panel basta (no necesita ser per-entry).
    u32 m_hoverItemKey = 0;   // hash del logical path del item hovered
    f32 m_hoverTimerSec = 0.0f;
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

    // F2H82: estado del modal "Importar vehiculo".
    bool m_importModalOpen = false;
    std::string m_importFsPath;       // filesystem absoluto del .glb/.fbx
    std::string m_importDisplayName;  // editable; default = stem del file
    std::string m_importSaveError;    // mensaje rojo en el modal si guardar falla
    vehicle::VehicleAnalysis m_importAnalysis{};
    vehicle::VehicleClass m_importClass = vehicle::VehicleClass::Sedan;
    vehicle::VehiclePhysicsPreset m_importPreset{};
    // F2H82 polish: factor de escala del mesh (modelos exportados con vertices
    // en cm/mm; default 1.0 = ya esta en metros). El modal sugiere x100 / x1000
    // si las dimensiones detectadas son sospechosas.
    float m_importMeshScale = 1.0f;

    // F4H2 Bloque B: armas `.moodweapon`. Scan plano de `assets/weapons/`.
    // El AssetManager las carga con `loadWeapon` (lee JSON + cachea); el id
    // se usa para resolver getWeapon(id) que devuelve la `Weapon::Spec`.
    struct WeaponEntry {
        std::string   logicalPath; // "weapons/shotgun.moodweapon"
        std::string   displayName; // "shotgun.moodweapon"
        std::string   weaponName;  // Spec::displayName (legible)
        WeaponAssetId id = 0;
    };
    std::vector<WeaponEntry> m_weaponEntries;

    // F2H82: borrado de vehiculos via right-click. La accion abre un modal de
    // confirmacion (no se borra al toque). `m_pendingDeleteVehicle` guarda el
    // path logico mientras espera confirmacion.
    std::string m_pendingDeleteVehicle;
    void confirmAndDeleteVehicle();   // dibuja el modal de confirmacion

    // F3H19: rename con cascada. El context menu de cada tab abre el modal,
    // que muestra preview de refs + InputText del nuevo nombre. Al confirmar,
    // se construye `m_pendingRename` que EditorApplication consume para
    // push el RenameAssetCommand al history.
    Scene* m_scene = nullptr;
    std::optional<PendingRename> m_pendingRename;

    // Estado del modal de rename:
    bool m_renameModalOpen = false;
    std::string m_renameOldLogical;        // path lógico actual (ej. "scripts/player.lua")
    std::filesystem::path m_renameOldDisk; // path filesystem actual (resolvido via VFS)
    std::string m_renameNewName;           // editable: solo el filename + ext nuevo
    std::string m_renameError;             // mensaje rojo en el modal si validation falla
    std::vector<asset_refs::RefSite> m_renameRefsCache;  // snapshot al abrir

    /// @brief F3H19: abre el modal de rename para un asset logical path. Si
    ///        el panel no tiene Scene+AssetManager, no-op (queda silenciado).
    void openRenameModal(const std::string& logicalPath);

    /// @brief F3H19: cuerpo del modal. Llamar UNA vez por frame desde
    ///        `onImGuiRender` si `m_renameModalOpen`.
    void drawRenameModal();

    /// @brief F3H19: dibuja el context menu (right-click sobre el item)
    ///        con la opción "Renombrar...". Llamar inmediatamente después
    ///        del widget (Button/ImageButton) del item para que ImGui
    ///        adjunte el popup al item correcto.
    void addRenameContextMenu(const std::string& logicalPath);

    // F3H16: helper que detecta hover prolongado sobre el ultimo
    // ImGui::ImageButton dibujado. Llamar INMEDIATAMENTE despues del
    // ImageButton del thumb. Devuelve `true` si el cursor estuvo quieto
    // sobre el item al menos `UserSettings.editor.hoverPreviewDelayMs`
    // milisegundos consecutivos. `itemKey` es un hash estable del item
    // (ej. FNV del logical path) para identificarlo entre frames.
    bool hoverPreviewElapsed(u32 itemKey);
};

} // namespace Mood
