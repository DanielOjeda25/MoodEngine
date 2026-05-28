#include "editor/application/Autosave.h"

#include "core/Log.h"
#include "core/Toasts.h"
#include "core/UserSettings.h"
#include "core/i18n/I18n.h"

#include <system_error>

namespace Mood {

namespace {

constexpr const char* kAutosaveDirName = ".autosave";

std::filesystem::path autosaveDir(const std::filesystem::path& root) {
    return root / kAutosaveDirName;
}

std::filesystem::path autosaveFinalPath(const std::filesystem::path& root,
                                        const std::filesystem::path& mapRelPath) {
    return autosaveDir(root) / mapRelPath.filename();
}

std::filesystem::path autosaveTmpPath(const std::filesystem::path& root,
                                      const std::filesystem::path& mapRelPath) {
    auto p = autosaveFinalPath(root, mapRelPath);
    p += ".tmp";
    return p;
}

} // namespace

void Autosave::setup(const std::filesystem::path& projectRoot,
                     const std::filesystem::path& currentMapRelPath,
                     WriteFn writeFn,
                     DirtyFn dirtyFn) {
    m_projectRoot = projectRoot;
    m_mapRelPath  = currentMapRelPath;
    m_writeFn     = std::move(writeFn);
    m_dirtyFn     = std::move(dirtyFn);
    m_timerMs     = 0.0f;
    m_active      = !m_projectRoot.empty()
                  && !m_mapRelPath.empty()
                  && m_writeFn
                  && m_dirtyFn;
}

void Autosave::teardown() {
    m_projectRoot.clear();
    m_mapRelPath.clear();
    m_writeFn = nullptr;
    m_dirtyFn = nullptr;
    m_timerMs = 0.0f;
    m_active  = false;
}

void Autosave::tick(f32 dtMs) {
    if (!m_active) return;
    const auto& prefs = UserSettings::editor();
    if (!prefs.autosaveEnabled) return;

    m_timerMs += dtMs;
    const f32 intervalMs =
        static_cast<f32>(prefs.autosaveIntervalMin) * 60.0f * 1000.0f;
    if (m_timerMs < intervalMs) return;

    if (!m_dirtyFn || !m_dirtyFn()) {
        // Trigger sin cambios: skip silencioso pero reset del timer
        // para no spammear el check cada frame.
        m_timerMs = 0.0f;
        return;
    }

    const auto tmp   = autosaveTmpPath(m_projectRoot, m_mapRelPath);
    const auto final_ = autosaveFinalPath(m_projectRoot, m_mapRelPath);
    std::error_code ec;
    std::filesystem::create_directories(tmp.parent_path(), ec);
    if (ec) {
        Log::editor()->warn("[autosave] no se pudo crear '{}': {}",
                            tmp.parent_path().generic_string(), ec.message());
        return;
    }
    try {
        m_writeFn(tmp);
    } catch (const std::exception& e) {
        Log::editor()->warn("[autosave] write a '{}' falló: {}",
                            tmp.generic_string(), e.what());
        // No reset del timer: el próximo tick reintentará. Sin toast
        // de error para no spammear si la falla es persistente
        // (filesystem read-only, disco lleno, etc).
        return;
    }
    // Rename atómico tmp → final. En Windows requiere que el destino
    // no exista; remove first si ya hay autosave previo.
    std::filesystem::remove(final_, ec);  // ignora "no existe"
    std::filesystem::rename(tmp, final_, ec);
    if (ec) {
        Log::editor()->warn("[autosave] rename '{}' → '{}' falló: {}",
                            tmp.generic_string(), final_.generic_string(), ec.message());
        std::filesystem::remove(tmp);  // best-effort cleanup
        return;
    }
    Log::editor()->info("[autosave] mapa guardado en '{}'", final_.generic_string());
    Toasts::pushInfo(I18n::T("editor.toast.autosaved", m_mapRelPath.filename().generic_string()));
    m_timerMs = 0.0f;
}

void Autosave::clearOnDisk() {
    if (m_projectRoot.empty() || m_mapRelPath.empty()) return;
    std::error_code ec;
    std::filesystem::remove(autosaveFinalPath(m_projectRoot, m_mapRelPath), ec);
    std::filesystem::remove(autosaveTmpPath(m_projectRoot, m_mapRelPath), ec);
}

std::filesystem::path Autosave::targetPath() const {
    if (m_projectRoot.empty() || m_mapRelPath.empty()) return {};
    return autosaveFinalPath(m_projectRoot, m_mapRelPath);
}

std::filesystem::path Autosave::canonicalMapPath() const {
    if (m_projectRoot.empty() || m_mapRelPath.empty()) return {};
    return m_projectRoot / m_mapRelPath;
}

bool Autosave::autosaveMoreRecentThanCanonical() const {
    const auto autoP = targetPath();
    const auto canonP = canonicalMapPath();
    if (autoP.empty() || canonP.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(autoP, ec)) return false;
    if (!std::filesystem::exists(canonP, ec)) {
        // El canónico no existe pero el autosave sí — recovery candidate
        // (caso raro: el dev movió/borró el .moodmap manualmente).
        return true;
    }
    const auto autoMtime = std::filesystem::last_write_time(autoP, ec);
    if (ec) return false;
    const auto canonMtime = std::filesystem::last_write_time(canonP, ec);
    if (ec) return false;
    return autoMtime > canonMtime;
}

} // namespace Mood
