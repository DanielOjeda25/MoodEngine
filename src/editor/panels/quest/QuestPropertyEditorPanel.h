#pragma once

// QuestPropertyEditorPanel (F2H53 Bloque D): edita los campos del quest
// seleccionado en el `QuestBrowserPanel`. Renderiza:
//
// - Identity: id (readonly), name (toggle key/literal), description (toggle),
//   category (string libre).
// - Objectives: lista con add/remove/reorder. Cada objective tiene:
//   id, name (toggle), description_literal, type dropdown + campos
//   type-specific (item_path+min_quantity / var_name / custom_predicate).
// - Rewards: lista con add/remove. Type dropdown + campos type-specific
//   (item_path+quantity / var_name+var_value / lua_script).
//
// Save explicito (boton "Guardar"). Flag `m_dirty` cambia el titulo a
// "* Quest Property Editor" si hay cambios sin guardar — mismo patron que
// ItemPropertyEditor (F2H51 Bloque G).

#include "editor/panels/IPanel.h"
#include "engine/quest/QuestAsset.h"

#include <filesystem>
#include <string>

namespace Mood {

class EditorUI;

class QuestPropertyEditorPanel : public IPanel {
public:
    QuestPropertyEditorPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Quest Property Editor"; }
    const char* category() const override { return "Assets"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    void loadFromPath(const std::filesystem::path& fsPath);
    void syncWithBrowserSelection();

    void drawIdentitySection();
    void drawObjectivesSection();
    void drawRewardsSection();
    void drawSaveBar();

    // Renderer por objective (devuelve true si hay que borrarlo).
    bool drawObjective(size_t idx, Quest::Objective& o);
    // Renderer por reward (devuelve true si hay que borrarlo).
    bool drawReward(size_t idx, Quest::Reward& r);

    EditorUI*                  m_ui = nullptr;

    std::filesystem::path      m_loadedPath;
    Quest::Asset               m_loaded;
    bool                       m_dirty = false;

    // Toggle UI (no se persiste — derivado de los campos del asset):
    bool                       m_useNameKey = false;
    bool                       m_useDescKey = false;
};

} // namespace Mood
