// Inline impls de los request/consume de demos spawn (AUDIT-3 split).
// Patron uniforme: request setea flag; consume devuelve el flag y lo
// resetea atomicamente (single-frame edge trigger). Incluido al final
// de EditorUI.h — no incluir directo desde otro modulo.

namespace Mood {

inline void EditorUI::requestSpawnRotator() { m_spawnRotatorRequested = true; }
inline bool EditorUI::consumeSpawnRotatorRequest() {
    const bool r = m_spawnRotatorRequested;
    m_spawnRotatorRequested = false;
    return r;
}

inline void EditorUI::requestSpawnEnemyDemo() { m_spawnEnemyDemoRequested = true; }
inline bool EditorUI::consumeSpawnEnemyDemoRequest() {
    const bool r = m_spawnEnemyDemoRequested;
    m_spawnEnemyDemoRequested = false;
    return r;
}

inline void EditorUI::requestSpawnHudDemo() { m_spawnHudDemoRequested = true; }
inline bool EditorUI::consumeSpawnHudDemoRequest() {
    const bool r = m_spawnHudDemoRequested;
    m_spawnHudDemoRequested = false;
    return r;
}

inline void EditorUI::requestSpawnAudioSource() { m_spawnAudioSourceRequested = true; }
inline bool EditorUI::consumeSpawnAudioSourceRequest() {
    const bool r = m_spawnAudioSourceRequested;
    m_spawnAudioSourceRequested = false;
    return r;
}

inline void EditorUI::requestSpawnPointLight() { m_spawnPointLightRequested = true; }
inline bool EditorUI::consumeSpawnPointLightRequest() {
    const bool r = m_spawnPointLightRequested;
    m_spawnPointLightRequested = false;
    return r;
}

inline void EditorUI::requestSpawnPhysicsBox() { m_spawnPhysicsBoxRequested = true; }
inline bool EditorUI::consumeSpawnPhysicsBoxRequest() {
    const bool r = m_spawnPhysicsBoxRequested;
    m_spawnPhysicsBoxRequested = false;
    return r;
}

inline void EditorUI::requestSpawnEnvironment() { m_spawnEnvironmentRequested = true; }
inline bool EditorUI::consumeSpawnEnvironmentRequest() {
    const bool r = m_spawnEnvironmentRequested;
    m_spawnEnvironmentRequested = false;
    return r;
}

inline void EditorUI::requestSpawnShadowDemo() { m_spawnShadowDemoRequested = true; }
inline bool EditorUI::consumeSpawnShadowDemoRequest() {
    const bool r = m_spawnShadowDemoRequested;
    m_spawnShadowDemoRequested = false;
    return r;
}

inline void EditorUI::requestSpawnPbrSpheres() { m_spawnPbrSpheresRequested = true; }
inline bool EditorUI::consumeSpawnPbrSpheresRequest() {
    const bool r = m_spawnPbrSpheresRequested;
    m_spawnPbrSpheresRequested = false;
    return r;
}

inline void EditorUI::requestSpawnLightStress() { m_spawnLightStressRequested = true; }
inline bool EditorUI::consumeSpawnLightStressRequest() {
    const bool r = m_spawnLightStressRequested;
    m_spawnLightStressRequested = false;
    return r;
}

inline void EditorUI::requestSpawnAnimatedCharacter() { m_spawnAnimatedCharacterRequested = true; }
inline bool EditorUI::consumeSpawnAnimatedCharacterRequest() {
    const bool r = m_spawnAnimatedCharacterRequested;
    m_spawnAnimatedCharacterRequested = false;
    return r;
}

inline void EditorUI::requestSpawnFireParticles() { m_spawnFireParticlesRequested = true; }
inline bool EditorUI::consumeSpawnFireParticlesRequest() {
    const bool r = m_spawnFireParticlesRequested;
    m_spawnFireParticlesRequested = false;
    return r;
}

inline void EditorUI::requestSpawnTrigger() { m_spawnTriggerRequested = true; }
inline bool EditorUI::consumeSpawnTriggerRequest() {
    const bool r = m_spawnTriggerRequested;
    m_spawnTriggerRequested = false;
    return r;
}

inline void EditorUI::requestSpawnDialogDemo() { m_spawnDialogDemoRequested = true; }
inline bool EditorUI::consumeSpawnDialogDemoRequest() {
    const bool r = m_spawnDialogDemoRequested;
    m_spawnDialogDemoRequested = false;
    return r;
}

inline void EditorUI::requestGenerateNarrativeDemoMap() { m_generateNarrativeDemoMapRequested = true; }
inline bool EditorUI::consumeGenerateNarrativeDemoMapRequest() {
    const bool r = m_generateNarrativeDemoMapRequested;
    m_generateNarrativeDemoMapRequested = false;
    return r;
}

inline void EditorUI::requestSpawnFullStressScene() { m_spawnFullStressSceneRequested = true; }
inline bool EditorUI::consumeSpawnFullStressSceneRequest() {
    const bool r = m_spawnFullStressSceneRequested;
    m_spawnFullStressSceneRequested = false;
    return r;
}

inline void EditorUI::requestSpawnStressTris(int targetTris) {
    m_spawnStressTrisRequested = targetTris;
}
inline int EditorUI::consumeSpawnStressTrisRequest() {
    const int n = m_spawnStressTrisRequested;
    m_spawnStressTrisRequested = 0;
    return n;
}

inline void EditorUI::requestOpenDemoMap() { m_openDemoMapRequested = true; }
inline bool EditorUI::consumeOpenDemoMapRequest() {
    const bool r = m_openDemoMapRequested;
    m_openDemoMapRequested = false;
    return r;
}

inline void EditorUI::requestOpenNarrativeDemo() { m_openNarrativeDemoRequested = true; }
inline bool EditorUI::consumeOpenNarrativeDemoRequest() {
    const bool r = m_openNarrativeDemoRequested;
    m_openNarrativeDemoRequested = false;
    return r;
}

} // namespace Mood
