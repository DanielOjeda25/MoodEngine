#include "editor/commands/HistoryStack.h"

#include "core/Log.h"

namespace Mood {

void HistoryStack::push(std::unique_ptr<ICommand> cmd) {
    if (cmd == nullptr) return;
    cmd->execute();
    Log::editor()->info("Comando ejecutado: {}", cmd->name());
    m_undo.push_back(std::move(cmd));
    // Cualquier comando deshecho previamente ya no es alcanzable: lo
    // descartamos. (Igual que en cualquier editor estandar — al hacer
    // una accion nueva tras Ctrl+Z, el redo stack se pierde.)
    m_redo.clear();
    while (m_undo.size() > k_maxSize) {
        m_undo.pop_front();
    }
}

bool HistoryStack::undo() {
    if (m_undo.empty()) return false;
    std::unique_ptr<ICommand> cmd = std::move(m_undo.back());
    m_undo.pop_back();
    cmd->undo();
    Log::editor()->info("Deshacer: {}", cmd->name());
    m_redo.push_back(std::move(cmd));
    return true;
}

bool HistoryStack::redo() {
    if (m_redo.empty()) return false;
    std::unique_ptr<ICommand> cmd = std::move(m_redo.back());
    m_redo.pop_back();
    cmd->execute();
    Log::editor()->info("Rehacer: {}", cmd->name());
    m_undo.push_back(std::move(cmd));
    return true;
}

void HistoryStack::clear() {
    m_undo.clear();
    m_redo.clear();
}

std::string HistoryStack::undoName() const {
    return m_undo.empty() ? std::string{} : m_undo.back()->name();
}

std::string HistoryStack::redoName() const {
    return m_redo.empty() ? std::string{} : m_redo.back()->name();
}

} // namespace Mood
