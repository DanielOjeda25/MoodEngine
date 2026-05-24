// Inline impls de los request/consume de spawn (AUDIT-3 split).
// Patron uniforme: request setea flag; consume devuelve el flag y lo
// resetea atomicamente (single-frame edge trigger). Incluido al final
// de EditorUI.h — no incluir directo desde otro modulo.
//
// post-v2.0.2 cleanup: borrados 12 request/consume pares de demos
// (Rotator, HudDemo, EnemyDemo, ShadowDemo, PbrSpheres, AnimatedChar,
// FireParticles, DialogDemo, NarrativeDemoMap, FullStressScene,
// OpenDemoMap, OpenNarrativeDemo) + 5 spawners legacy (PointLight,
// Environment, PhysicsBox, AudioSource, Trigger) reemplazados por
// "+ Crear Entidad" + Add Component en Inspector.
// Quedan solo los 2 stress tests del menu Debug.

namespace Mood {

inline void EditorUI::requestSpawnLightStress() { m_spawnLightStressRequested = true; }
inline bool EditorUI::consumeSpawnLightStressRequest() {
    const bool r = m_spawnLightStressRequested;
    m_spawnLightStressRequested = false;
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
