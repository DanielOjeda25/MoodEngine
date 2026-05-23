#pragma once

// Panel dedicado de edicion de materiales (Hito 42 — version "lite").
//
// Antes del Hito 42 los materiales se editaban SOLO via el Inspector
// cuando una entidad con MeshRenderer estaba seleccionada. Eso obligaba
// a tener una entidad en el scene para tweakear un material — friccion
// para el dev que solo quiere armar un material standalone.
//
// V1 (este hito):
//   - Combo con todos los `MaterialAsset` del AssetManager.
//   - Drop targets para texturas (albedo/MR/normal/AO) reusando el
//     payload `MOOD_TEXTURE_ASSET` del AssetBrowser.
//   - Sliders escalares (albedoTint, metallic, roughness, ao) — mismo
//     patron que el Inspector pero sin requerir entidad seleccionada.
//   - Sin undo de los edits (los Inspector edits si lo tienen — diff
//     entre paneles).
//   - Sin preview esferico dedicado: el dev ve el cambio asignando el
//     material a una entidad del viewport. Documentado.
//
// V2 (F2H21 cerrado):
//   - Preview esferico off-screen en un FBO 256x256 dentro del panel.
//   - Save .material desde el panel (boton "Guardar").
// V3 (pendiente):
//   - Node graph (estilo Substance/UE) — feature mayor, hito propio
//     (probable F2H23 tras 4-viewport Hammer).

#include "core/Types.h"
#include "editor/panels/IPanel.h"
#include "editor/panels/assets/AssetEditTracker.h"  // F2H84

#include <glm/vec3.hpp>

namespace Mood {

class AssetManager;
class EditorUI;  // F2H84
class MaterialPreviewRenderer;

class MaterialEditorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Material Editor"; }
    const char* category() const override { return "Assets"; }

    /// @brief Inyecta el AssetManager para poblar el combo + resolver
    ///        ids de texturas dropeadas. Non-owning.
    void setAssetManager(AssetManager* am) { m_assets = am; }

    /// @brief F2H21: inyecta el preview renderer (lo crea
    ///        EditorApplication una vez). Non-owning. Si null, el panel
    ///        omite la columna de preview.
    void setPreviewRenderer(MaterialPreviewRenderer* p) { m_preview = p; }

    /// @brief F2H84: acceso al `HistoryStack` global para hacer los edits
    ///        de campos del material undoable via Ctrl+Z (mismo patron que
    ///        el Inspector). Non-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief True si el usuario edito un campo este frame. EditorApplication
    ///        lo consume para markDirty().
    bool consumeEditedFlag() {
        const bool r = m_editedThisFrame;
        m_editedThisFrame = false;
        return r;
    }

private:
    AssetManager* m_assets = nullptr;
    EditorUI* m_ui = nullptr;  // F2H84: para historyStack()
    MaterialPreviewRenderer* m_preview = nullptr;
    AssetEditTracker m_editTracker;  // F2H84
    bool m_editedThisFrame = false;
    /// Index del combo (offset dentro de la lista de materiales del AM).
    /// -1 = sin seleccionar todavia (la primera vez que el panel se abre,
    /// elegimos el primer material no-sentinel — slot 0 es magenta
    /// deliberado y __runtime#/__tex# son sentinels).
    int m_selectedMatIdx = -1;
    /// F2H21: feedback transitorio del boton Guardar (verde/rojo). Cuenta
    /// frames restantes para mostrar el mensaje.
    int m_saveStatusFrames = 0;
    bool m_saveStatusOk = false;

    /// F2H21 polish: tracking del estado entre frames para emitir logs
    /// solo en eventos discretos (cambio de material). F2H84 reemplazo el
    /// tracking manual por-slider con `AssetEditTracker`.
    int m_lastLoggedMatIdx = -1;
};

} // namespace Mood
