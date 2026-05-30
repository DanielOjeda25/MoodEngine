#pragma once

#include "editor/commands/EditBrushUVCommand.h"  // F2H16: BrushUVSnapshot
#include "editor/panels/IPanel.h"
#include "editor/panels/scene/InspectorEditTracker.h"
#include "editor/panels/scene/MultiEditTracker.h"  // F3H8
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
    void renderJointSection(Entity e);  // F2H65
    void renderRagdollSection(Entity e);  // break-A4 (2026-05-23)
    void renderVehicleSection(Entity e);  // F2H67
    void renderAudioSourceSection(Entity e);
    void renderAnimatorSection(Entity e);
    void renderParticleEmitterSection(Entity e);
    void renderTriggerSection(Entity e);
    void renderForceFieldSection(Entity e);  // F2H72
    void renderClothSection(Entity e);  // F2H75
    void renderBrushSection(Entity e);
    void renderInventorySection(Entity e);  // F2H51
    void renderHealthSection(Entity e);     // F4H1
    void renderWeaponSection(Entity e);     // F4H2

    /// @brief F3H28: categorias scene-wide nuevas del Inspector.
    /// `renderGroupsSection` lista los Empty Group_<N> del mapa (backend
    /// F3H27: SetParentCommand / GroupSelectionCommand). `renderMapToolsSection`
    /// concentra configuracion global del workspace Editor de Mapas
    /// (snap-to-vertex, snap grid, labels, sub-modes Object/V/E/F,
    /// MapTool actual). Ninguna depende de Entity seleccionada — el
    /// dispatch las invoca cuando `activeCat == "groups" | "maptools"`.
    void renderGroupsSection();
    void renderMapToolsSection();

    /// F2H44 Bloque A: boton "+ Add Component" + popup con lista
    /// agrupada por categoria + search. Solo lista los componentes que
    /// `e` NO tiene aun.
    void renderAddComponentSection(Entity e);
    void drawAddComponentPopup(Entity e);

    /// F2H81: header plegable de seccion de componente. Reemplaza el
    /// `SeparatorText` siempre-abierto que mareaba al apilar 17 secciones.
    /// Devuelve `true` si la seccion esta desplegada — el cuerpo debe
    /// guardarse con `if (!beginComponentSection<T>(e, label)) return;`.
    /// Con `removable=true` agrega menu contextual (clic derecho) "Quitar
    /// componente" (undoable). Templado en T para tipar el remove command;
    /// definido en InspectorPanel_Internal.h. Tag/Transform pasan
    /// `removable=false` (son nucleo, no estan en el popup Add Component).
    template<typename T>
    bool beginComponentSection(Entity e, const char* label, bool removable = true);

    /// F3H22: barra de icons laterales estilo Properties Editor de Blender.
    /// 7 botones (Object/Render/Animation/Audio/Physics/Gameplay/Environment)
    /// + botón "All" para volver al modo legacy. Cada icon solo se muestra
    /// si la entity tiene >=1 componente de esa categoría (decisión D3 del
    /// plan F3H22 — Blender oculta tabs irrelevantes). Object siempre
    /// visible. La categoría activa se persiste en
    /// `UserSettings.editor.inspectorActiveCategory`.
    void renderCategoryBar(Entity e);

    EditorUI* m_ui = nullptr;
    AssetManager* m_assets = nullptr;
    bool m_editedThisFrame = false;

    /// Hito 32: tracker de edits del Inspector para Undo/Redo. Solo un
    /// widget puede estar activo a la vez — un solo buffer alcanza.
    InspectorEditTracker m_editTracker;

    /// F3H8: tracker hermano para multi-edit (N entidades a la vez).
    /// Solo poblado cuando selectionSet.size() > 1; cuando hay 1 sola
    /// entidad seleccionada los helpers caen al `m_editTracker` single.
    MultiEditTracker m_multiEditTracker;

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

    /// F2H33: estado del checkbox "Treat as one face" del Face Edit
    /// Sheet. Visible solo si hay > 1 caras seleccionadas; al bajar a
    /// single se resetea a false.
    bool m_treatAsOneFace = false;

    /// F2H44 Bloque A: buffer del search input del popup Add Component.
    char m_addComponentSearch[64]{};

    /// F4H2 Bloque B: posición target del popup "Componentes disponibles".
    /// Capturada justo después del click sobre `+ Agregar Componente` —
    /// bottom-left del botón + 2px gap. Sin esto, ImGui auto-flippeaba
    /// el popup hacia arriba cuando había poco espacio debajo, tapando el
    /// botón que lo abrió. Aplicado con `SetNextWindowPos` en el primer
    /// frame del popup (`ImGuiCond_Appearing`).
    ImVec2 m_addCompPopupPos{0.0f, 0.0f};

    /// F3H9 Stage 9: slot de material seleccionado en el inspector
    /// MeshRenderer (UI Blender-style: lista compacta arriba + panel del
    /// seleccionado debajo). Sticky entre frames; se clampea contra
    /// `mr.materials.size()` cada frame.
    int m_selectedMaterialSlot = 0;
};

} // namespace Mood
