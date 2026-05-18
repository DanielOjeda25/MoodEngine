// Inline impls de los request/consume de creacion y conversion de
// entidades (AUDIT-3 split). Create from model / placeholder / pick
// from loaded / convert / delete / save prefab. Incluido al final
// de EditorUI.h.

namespace Mood {

inline void EditorUI::requestSavePrefabDialog() { m_savePrefabRequested = true; }
inline bool EditorUI::consumeSavePrefabRequest() {
    const bool r = m_savePrefabRequested;
    m_savePrefabRequested = false;
    return r;
}

inline void EditorUI::requestCreateEntityFromModel() { m_createEntityFromModelRequested = true; }
inline bool EditorUI::consumeCreateEntityFromModelRequest() {
    const bool r = m_createEntityFromModelRequested;
    m_createEntityFromModelRequested = false;
    return r;
}

inline void EditorUI::requestCreateEntityPlaceholder() { m_createEntityPlaceholderRequested = true; }
inline bool EditorUI::consumeCreateEntityPlaceholderRequest() {
    const bool r = m_createEntityPlaceholderRequested;
    m_createEntityPlaceholderRequested = false;
    return r;
}

inline void EditorUI::requestPickFromLoadedMeshes() { m_pickFromLoadedMeshesRequested = true; }
inline bool EditorUI::consumePickFromLoadedMeshesRequest() {
    const bool r = m_pickFromLoadedMeshesRequested;
    m_pickFromLoadedMeshesRequested = false;
    return r;
}

inline void EditorUI::requestEntityConvertModal(entt::entity e) {
    m_convertModalRequested = true;
    m_convertModalTarget    = e;
}
inline bool EditorUI::consumeEntityConvertModalRequest(entt::entity& outTarget) {
    if (!m_convertModalRequested) return false;
    m_convertModalRequested = false;
    outTarget = m_convertModalTarget;
    return true;
}

inline void EditorUI::requestDeleteSelectedEntity() { m_deleteSelectedRequested = true; }
inline bool EditorUI::consumeDeleteSelectedRequest() {
    const bool r = m_deleteSelectedRequested;
    m_deleteSelectedRequested = false;
    return r;
}

} // namespace Mood
