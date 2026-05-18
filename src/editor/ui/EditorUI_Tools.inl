// Inline impls de los request/consume de tools y modo de edicion
// (AUDIT-3 split). Gizmo modes, sub-mode (Object/Vertex/Edge/Face),
// MapTool, snap-to-vertex, polygon draw, entity labels, carve.
// Incluido al final de EditorUI.h.

namespace Mood {

inline void EditorUI::requestGizmoMode(int mode) { m_gizmoModeRequested = mode; }
inline int EditorUI::consumeGizmoModeRequest() {
    const int r = m_gizmoModeRequested;
    m_gizmoModeRequested = -1;
    return r;
}

inline void EditorUI::requestToggleFaceMode() { m_toggleFaceModeRequested = true; }
inline bool EditorUI::consumeToggleFaceModeRequest() {
    const bool r = m_toggleFaceModeRequested;
    m_toggleFaceModeRequested = false;
    return r;
}

inline void EditorUI::requestSubMode(EditorSubMode mode) {
    m_subModeRequested = mode;
    m_hasSubModeRequest = true;
}
inline bool EditorUI::consumeSubModeRequest(EditorSubMode& outMode) {
    if (!m_hasSubModeRequest) return false;
    outMode = m_subModeRequested;
    m_hasSubModeRequest = false;
    return true;
}

inline void EditorUI::requestTogglePolygonDraw() { m_togglePolygonDrawRequested = true; }
inline bool EditorUI::consumeTogglePolygonDrawRequest() {
    const bool r = m_togglePolygonDrawRequested;
    m_togglePolygonDrawRequested = false;
    return r;
}

inline void EditorUI::requestMapTool(MapTool tool) {
    m_mapToolRequested = tool;
    m_hasMapToolRequest = true;
}
inline bool EditorUI::consumeMapToolRequest(MapTool& outTool) {
    if (!m_hasMapToolRequest) return false;
    outTool = m_mapToolRequested;
    m_hasMapToolRequest = false;
    return true;
}

inline void EditorUI::requestToggleSnapToVertex() { m_toggleSnapToVertexRequested = true; }
inline bool EditorUI::consumeToggleSnapToVertexRequest() {
    const bool r = m_toggleSnapToVertexRequested;
    m_toggleSnapToVertexRequested = false;
    return r;
}

inline void EditorUI::requestToggleEntityLabels() { m_toggleEntityLabelsRequested = true; }
inline bool EditorUI::consumeToggleEntityLabelsRequest() {
    const bool r = m_toggleEntityLabelsRequested;
    m_toggleEntityLabelsRequested = false;
    return r;
}

inline void EditorUI::requestCarve() { m_carveRequested = true; }
inline bool EditorUI::consumeCarveRequest() {
    const bool r = m_carveRequested;
    m_carveRequested = false;
    return r;
}

inline void EditorUI::requestTogglePlay() { m_togglePlayRequested = true; }
inline bool EditorUI::consumeTogglePlayRequest() {
    const bool r = m_togglePlayRequested;
    m_togglePlayRequested = false;
    return r;
}

} // namespace Mood
