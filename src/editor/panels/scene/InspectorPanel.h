#pragma once

#include "editor/commands/EditBrushUVCommand.h"  // F2H16: BrushUVSnapshot
#include "editor/panels/IPanel.h"
#include "editor/panels/scene/InspectorEditTracker.h"
#include "engine/scene/core/Entity.h"  // F2H24: secciones de render reciben Entity

namespace Mood {

class EditorUI;
class AssetManager;

class InspectorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Inspector"; }

    /// @brief Inyecta EditorUI para leer la entidad seleccionada.
    ///        Non-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief Inyecta el AssetManager para poblar dropdowns (texturas, audio)
    ///        sin acoplar Inspector a EditorApplication. Non-owning.
    void setAssetManager(AssetManager* am) { m_assets = am; }

    /// @brief Flag de cambio: true cuando el usuario edito un componente
    ///        este frame. `EditorApplication` lo consume tras `ui.draw()`
    ///        para llamar `markDirty()`.
    bool consumeEditedFlag() {
        const bool r = m_editedThisFrame;
        m_editedThisFrame = false;
        return r;
    }

    /// @brief F2H17: true cuando el dev esta editando UV params del
    ///        brush (snapshot pre capturado, drag de slider en
    ///        progreso). Usado por EditorRenderPass para ocultar
    ///        la capa rellena del face highlight durante la edicion
    ///        — asi se ve la textura mientras se ajusta.
    bool isEditingBrushUV() const { return m_uvSnapshotValid; }

private:
    // F2H24: secciones de render extraidas a archivos parciales
    // (InspectorPanel_*.cpp). Cada una opera sobre `e` y los miembros
    // `m_ui` / `m_assets` / `m_editTracker` / `m_editedThisFrame`.
    // Privadas: la API publica del panel sigue siendo solo
    // `onImGuiRender` + `consumeEditedFlag` + `isEditingBrushUV`.
    void renderTagSection(Entity e);
    void renderTransformSection(Entity e);
    void renderMeshRendererSection(Entity e);
    void renderCameraSection(Entity e);
    void renderLightSection(Entity e);
    void renderEnvironmentSection(Entity e);
    void renderScriptSection(Entity e);
    void renderRigidBodySection(Entity e);
    void renderAudioSourceSection(Entity e);
    void renderAnimatorSection(Entity e);
    void renderParticleEmitterSection(Entity e);
    void renderTriggerSection(Entity e);
    void renderBrushSection(Entity e);

    EditorUI* m_ui = nullptr;
    AssetManager* m_assets = nullptr;
    bool m_editedThisFrame = false;

    /// Hito 32: tracker de edits del Inspector para Undo/Redo. Solo un
    /// widget puede estar activo a la vez — un solo buffer alcanza.
    InspectorEditTracker m_editTracker;

    /// F2H16: snapshot pre-edicion del UV editor del brush. Capturado
    /// al ImGui::IsItemActivated() de cualquier slider de UV; usado
    /// como `oldSnap` del EditBrushUVCommand cuando el widget se
    /// deactiva after-edit.
    BrushUVSnapshot m_uvSnapshotPre;
    bool m_uvSnapshotValid = false;
    /// Tag de la entidad sobre la que se inicio la edicion. Robusto
    /// a cambios de seleccion durante el drag (improbable pero
    /// defensivo).
    std::string m_uvSnapshotEntityTag;
};

} // namespace Mood
