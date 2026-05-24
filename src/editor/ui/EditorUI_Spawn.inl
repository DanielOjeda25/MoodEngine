// Inline impls de los request/consume de spawn (AUDIT-3 split).
// Patron uniforme: request setea flag; consume devuelve el flag y lo
// resetea atomicamente (single-frame edge trigger). Incluido al final
// de EditorUI.h — no incluir directo desde otro modulo.
//
// post-v2.0.2 cleanup: borrados 12 request/consume pares de demos sin
// entry point UI (Rotator, HudDemo, EnemyDemo, ShadowDemo, PbrSpheres,
// AnimatedChar, FireParticles, DialogDemo, NarrativeDemoMap,
// FullStressScene, OpenDemoMap, OpenNarrativeDemo).

namespace Mood {

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

inline void EditorUI::requestSpawnLightStress() { m_spawnLightStressRequested = true; }
inline bool EditorUI::consumeSpawnLightStressRequest() {
    const bool r = m_spawnLightStressRequested;
    m_spawnLightStressRequested = false;
    return r;
}

inline void EditorUI::requestSpawnTrigger() { m_spawnTriggerRequested = true; }
inline bool EditorUI::consumeSpawnTriggerRequest() {
    const bool r = m_spawnTriggerRequested;
    m_spawnTriggerRequested = false;
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

} // namespace Mood
