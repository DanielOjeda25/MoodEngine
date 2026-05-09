#pragma once

// F2H12: comando undoable para operaciones booleanas CSG entre dos
// brushes. Approach destructivo: la op reemplaza los brushes input
// (A, B) por el resultado (0-N brushes nuevos). Undo restaura los
// originales y destruye los nuevos.
//
// Snapshot strategy: capturamos SavedBrush de A, B y de cada brush
// resultante. Entre execute y undo los handles cambian (entidades
// destruidas y recreadas), por lo que vivimos contra snapshots y
// no contra handles persistentes — igual que CreateEntityCommand.
//
// Compatibilidad con HistoryStack: si la op produce 0 brushes
// (subtract con B conteniendo A) el redo vuelve a destruir los
// originales. Si produce N brushes, los recrea con sus tags
// originales.

#include "editor/commands/Command.h"
#include "engine/scene/serialization/SceneSerializer.h"  // SavedBrush

#include <string>
#include <vector>

namespace Mood {

class Scene;
class AssetManager;

enum class BooleanOpKind {
    Subtract,
    Union,
    Intersect,
    /// F2H32 Bloque B: clip tool. Conceptualmente A se splittea por un
    /// plano sin un B explicito (B = empty snapshot con tag = ""). El
    /// comando trata empty bSnapshot como "no hay B para destruir/
    /// recrear" en execute/undo.
    Clip,
};

class BooleanOpCommand : public ICommand {
public:
    /// @brief Construye el comando. `aSnapshot` y `bSnapshot` son los
    ///        brushes pre-op tal como vivian en el scene (incluye tag,
    ///        transform, faces, material). El comando NO captura los
    ///        handles vivos — los reencuentra por tag al hacer undo.
    BooleanOpCommand(BooleanOpKind kind,
                     SavedBrush aSnapshot,
                     SavedBrush bSnapshot,
                     std::vector<SavedBrush> resultSnapshots,
                     Scene* scene,
                     AssetManager* assets,
                     std::string label);

    void execute() override;
    void undo() override;
    std::string name() const override;

private:
    BooleanOpKind m_kind;
    SavedBrush m_aSnapshot;
    SavedBrush m_bSnapshot;
    std::vector<SavedBrush> m_resultSnapshots;
    Scene* m_scene = nullptr;
    AssetManager* m_assets = nullptr;
    std::string m_label;
    bool m_currentlyApplied = true;  // true tras execute, false tras undo

    void destroyByTags(const std::vector<std::string>& tags);
    void recreateOriginals();
    void recreateResults();
};

} // namespace Mood
