#include "editor/commands/PasteComponentCommand.h"

#include "editor/components/ComponentClipboard.h"

namespace Mood {

void PasteComponentCommand::execute() {
    if (!isEntityValid() || m_assets == nullptr) return;
    ComponentClipboard::applyPayload(m_componentKey, m_after, m_target, *m_assets);
}

void PasteComponentCommand::undo() {
    if (!isEntityValid() || m_assets == nullptr) return;
    if (m_hadComponentBefore) {
        // Restaurar fields al estado pre-paste.
        ComponentClipboard::applyPayload(m_componentKey, m_before, m_target, *m_assets);
    } else {
        // Paste como nuevo -> undo lo remueve.
        ComponentClipboard::removeComponent(m_componentKey, m_target);
    }
}

} // namespace Mood
