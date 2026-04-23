#pragma once

// Virtual File System (Hito 5 Bloque 3). Resuelve paths logicos de assets
// (ej. "textures/grid.png") a paths reales del filesystem bajo la raiz del
// motor. El objetivo a futuro es soportar empaquetado en `.pak` (Hito 21)
// sin cambiar el codigo que pide assets: solo cambia la implementacion del
// VFS.
//
// Por ahora hace dos cosas:
//   1. Prefijar con la raiz de assets (default "assets/").
//   2. Rechazar path traversal (componentes ".." y paths absolutos) para que
//      el manager no pueda leer archivos fuera de su sandbox.

#include <filesystem>
#include <string>
#include <string_view>

namespace Mood {

class VFS {
public:
    /// @brief Construye el VFS con la raiz dada. La raiz puede ser relativa
    ///        al cwd (ej. "assets") o absoluta; se guarda tal cual.
    explicit VFS(std::filesystem::path root);

    /// @brief Resuelve un path logico a path de filesystem. Si `logical` no
    ///        es seguro (tiene "..", es absoluto o esta vacio) devuelve un
    ///        path vacio.
    std::filesystem::path resolve(std::string_view logical) const;

    /// @brief Valida que un path logico sea seguro. Reglas:
    ///        - No vacio.
    ///        - No absoluto.
    ///        - Ningun componente es "..".
    ///        - No vuelve al root con drive letter (ej. "C:/...").
    static bool isSafeLogicalPath(std::string_view logical);

    const std::filesystem::path& root() const { return m_root; }

private:
    std::filesystem::path m_root;
};

} // namespace Mood
