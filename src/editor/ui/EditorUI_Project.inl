// Inline impls de los accessors de proyecto / recents / mapas /
// workspace switching (AUDIT-3 split). Incluido al final de
// EditorUI.h.

namespace Mood {

inline void EditorUI::requestProjectAction(ProjectAction a) { m_projectAction = a; }
inline ProjectAction EditorUI::consumeProjectAction() {
    const auto a = m_projectAction;
    m_projectAction = ProjectAction::None;
    return a;
}

inline void EditorUI::setRecentProjects(std::vector<std::filesystem::path> paths) {
    m_recentProjects = std::move(paths);
}

inline bool EditorUI::consumeRecentsDirty() {
    const bool r = m_recentsDirty;
    m_recentsDirty = false;
    return r;
}

inline std::optional<std::filesystem::path> EditorUI::consumeOpenProjectPath() {
    auto p = std::move(m_openProjectPath);
    m_openProjectPath.reset();
    return p;
}

inline void EditorUI::requestOpenMap(std::filesystem::path p) {
    m_openMapRequest = std::move(p);
}
inline std::optional<std::filesystem::path> EditorUI::consumeOpenMapRequest() {
    auto p = std::move(m_openMapRequest);
    m_openMapRequest.reset();
    return p;
}

inline void EditorUI::requestBooleanOp(BooleanOpRequestKind kind) {
    m_booleanOpRequest = kind;
}
inline std::optional<EditorUI::BooleanOpRequestKind> EditorUI::consumeBooleanOpRequest() {
    auto r = std::move(m_booleanOpRequest);
    m_booleanOpRequest.reset();
    return r;
}

inline void EditorUI::setProjectMapsSnapshot(std::vector<std::filesystem::path> maps,
                                              std::filesystem::path currentMap,
                                              std::filesystem::path defaultMap) {
    m_projectMaps    = std::move(maps);
    m_currentMapPath = std::move(currentMap);
    m_defaultMapPath = std::move(defaultMap);
}

} // namespace Mood
