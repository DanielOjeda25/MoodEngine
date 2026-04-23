#include "platform/VFS.h"

namespace Mood {

VFS::VFS(std::filesystem::path root)
    : m_root(std::move(root)) {}

bool VFS::isSafeLogicalPath(std::string_view logical) {
    if (logical.empty()) return false;

    // Leading "/" o "\" son intento de escape aunque en Windows no cuenten
    // como is_absolute() (falta drive letter). Los rechazamos explicitamente.
    if (logical.front() == '/' || logical.front() == '\\') return false;

    // Un path absoluto del FS (unix "/foo" o windows "C:/foo") nunca es un
    // path logico valido: el caller deberia haber usado el path crudo si
    // queria eso, y el VFS no acepta sandbox breakouts.
    const std::filesystem::path p{std::string(logical)};
    if (p.is_absolute()) return false;

    // Rechazar cualquier componente "..". Tambien rechazamos "." explicito
    // (no tiene sentido como path de asset y ensucia normalizacion).
    for (const auto& part : p) {
        const auto s = part.string();
        if (s == ".." || s == ".") return false;
    }
    return true;
}

std::filesystem::path VFS::resolve(std::string_view logical) const {
    if (!isSafeLogicalPath(logical)) {
        return {};
    }
    return m_root / std::filesystem::path(std::string(logical));
}

} // namespace Mood
