#pragma once

// HistoryStack (Hito 27 Bloque 1): pila de comandos undo/redo.
//
// Modelo estandar:
// - `m_undo`: comandos ya ejecutados, en orden cronologico (back = mas reciente).
// - `m_redo`: comandos que se deshicieron, listos para rehacer (back = el que
//   se deshacria con redo siguiente).
// - `push(cmd)`: ejecuta el comando, lo agrega al `m_undo`, LIMPIA `m_redo`
//   (cualquier rehecho previo deja de tener sentido tras una nueva accion).
// - `undo()`: pop del `m_undo`, llama `undo()`, push al `m_redo`. No-op si vacia.
// - `redo()`: pop del `m_redo`, llama `execute()`, push al `m_undo`. No-op si vacia.
// - Cap en `k_maxSize` (~100): si `push` excede, descarta el mas viejo del `m_undo`
//   (ese trabajo deja de poder deshacerse, pero el editor no consume RAM ilimitada).
//
// Thread safety: editor single-threaded. Sin mutex.

#include "core/Types.h"
#include "editor/commands/Command.h"

#include <deque>
#include <memory>
#include <string>

namespace Mood {

class HistoryStack {
public:
    static constexpr usize k_maxSize = 100;

    /// @brief Ejecuta el comando + lo guarda en el undo stack. Limpia el
    ///        redo stack (cualquier "rehecho" futuro deja de tener sentido
    ///        tras una accion nueva). Si pasa el limite k_maxSize, descarta
    ///        el comando mas viejo.
    void push(std::unique_ptr<ICommand> cmd);

    /// @brief Deshace el ultimo comando del undo stack (si hay) y lo mueve
    ///        al redo stack. No-op si undo stack vacio.
    /// @return true si efectivamente deshizo algo.
    bool undo();

    /// @brief Rehace el ultimo comando deshecho (si hay) y lo mueve al
    ///        undo stack. No-op si redo stack vacio.
    /// @return true si efectivamente rehizo algo.
    bool redo();

    /// @brief Vacia ambos stacks. Llamar al cerrar/cambiar proyecto: las
    ///        entities a las que apuntan los commands quedaron destruidas.
    void clear();

    /// @brief Hito 32: tras recrear una entidad (DeleteEntityCommand::undo),
    ///        notifica a TODOS los comandos del stack que el handle viejo
    ///        ahora es `newH`. Asi un EditTransformCommand previo que
    ///        apuntaba al handle pre-delete vuelve a ser aplicable post-
    ///        recrearion. Sin esto, edit→delete→undo→undo dejaria el
    ///        segundo undo silencioso.
    void remapEntityInStack(entt::entity oldH, entt::entity newH);

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

    /// @brief Nombre del comando que `undo()` ejecutaria (para el menu).
    ///        Vacio si no hay nada que deshacer.
    std::string undoName() const;
    /// @brief Nombre del comando que `redo()` ejecutaria.
    std::string redoName() const;

    usize undoCount() const { return m_undo.size(); }
    usize redoCount() const { return m_redo.size(); }

private:
    std::deque<std::unique_ptr<ICommand>> m_undo;
    std::deque<std::unique_ptr<ICommand>> m_redo;
};

} // namespace Mood
