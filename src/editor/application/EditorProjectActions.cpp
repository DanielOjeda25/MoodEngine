// F2H24: nucleo de EditorProjectActions. El cuerpo de cada handler
// vive en archivos parciales `EditorProjectActions_<Dominio>.cpp`
// (FileIO, Package, Map, Brush, Boolean, Compile). Aca solo viven los
// helpers compartidos por todos: confirmDiscardChanges (popup
// Si/No/Cancelar), addToRecentProjects (dedup + limite),
// loadEditorState / saveEditorState (.mood/editor_state.json).
//
// Todos son metodos de `EditorApplication`. El header (EditorApplication.h)
// declara la firma; cada partial implementa su grupo de metodos.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>

#include <algorithm>
#include <fstream>

namespace Mood {

bool EditorApplication::confirmDiscardChanges() {
    if (!m_projectDirty) return true;

    const auto choice = pfd::message(
        "MoodEngine — Cambios sin guardar",
        "Hay cambios sin guardar en el proyecto actual.\n\n"
        "Si - guardar antes de continuar\n"
        "No - descartar los cambios\n"
        "Cancelar - volver al editor",
        pfd::choice::yes_no_cancel,
        pfd::icon::question).result();

    switch (choice) {
        case pfd::button::yes:
            handleSave();
            // Si el save fallo (p.ej. disco lleno), m_projectDirty sigue
            // en true y abortamos la accion.
            return !m_projectDirty;
        case pfd::button::no:
            return true;
        case pfd::button::cancel:
        default:
            return false;
    }
}

void EditorApplication::addToRecentProjects(const std::filesystem::path& moodproj) {
    const auto canonical = std::filesystem::absolute(moodproj);
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [&canonical](const std::filesystem::path& p) {
                return std::filesystem::absolute(p) == canonical;
            }),
        m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), canonical);
    if (m_recentProjects.size() > 8) m_recentProjects.resize(8);
    m_ui.setRecentProjects(m_recentProjects);
}

void EditorApplication::loadEditorState() {
    const auto path = std::filesystem::path(".mood") / "editor_state.json";
    std::ifstream in(path);
    if (!in.is_open()) return;

    nlohmann::json j;
    try { in >> j; } catch (...) { return; }

    // Preferencias globales.
    m_debugDraw = j.value("debugDraw", false);

    // Lista de proyectos recientes. El mas reciente esta al principio.
    // Se carga aca para alimentar el modal Welcome — pero NO se auto-abre
    // ninguno (el dev pidio explicitamente que cada relanzamiento arranque
    // limpio con la ventana de bienvenida; ver memoria de feedback). Si en
    // el futuro se quiere "auto-abrir el ultimo" como toggle de preferencias,
    // poner el `tryOpenProjectPath(m_recentProjects.front())` detras de un
    // flag opt-in.
    m_recentProjects.clear();
    if (j.contains("recentProjects") && j["recentProjects"].is_array()) {
        for (const auto& p : j["recentProjects"]) {
            m_recentProjects.emplace_back(p.get<std::string>());
        }
    }
    m_ui.setRecentProjects(m_recentProjects);
}

void EditorApplication::saveEditorState() const {
    std::error_code ec;
    std::filesystem::create_directories(".mood", ec);
    if (ec) return;

    nlohmann::json j;
    j["debugDraw"] = m_debugDraw;
    j["recentProjects"] = nlohmann::json::array();
    for (const auto& p : m_recentProjects) {
        j["recentProjects"].push_back(p.generic_string());
    }

    std::ofstream out(std::filesystem::path(".mood") / "editor_state.json");
    if (!out.is_open()) return;
    out << j.dump(2);
}

} // namespace Mood
