#pragma once

// DialogInteractSystem (F2H48): conecta los Triggers con DialogComponent
// al DialogSystem. Cada frame de Play Mode:
//   1) Itera entities con (TriggerComponent + DialogComponent).
//   2) Si el player esta dentro de un trigger cuya entidad tiene
//      DialogComponent.autoStartOnInteract=true y el dialog NO esta
//      activo, muestra el interact_prompt del HUD ("[E] Hablar").
//   3) Si `eJustPressed` y se cumplen las condiciones, carga el asset
//      (cacheado en DialogComponent.cachedDialogId tras el primer load)
//      y llama DialogSystem::start.
//
// El caller (EditorPlayMode / PlayerApplication) computa el flanco
// up->down de la tecla E. El sistema en si es agnostico a SDL.

namespace Mood {

class AssetManager;
class Scene;

} // namespace Mood

namespace Mood::Dialog::DialogInteractSystem {

/// @brief Avanza un frame del sistema. `eJustPressed` true = el caller
///        detecto el flanco up->down de la tecla E este frame.
/// @return true si este frame se arranco un dialog (el caller deberia
///         skippear su handler de "advance del dialog activo" para no
///         double-consumir el mismo flanco — la primera linea del NPC
///         debe verse al menos un frame antes de poder avanzar).
bool tick(Scene& scene, AssetManager& assets, bool eJustPressed);

/// @brief Handler de input mientras el dialog ya esta activo. El caller
///        pasa el flanco up->down de E y digit 1..9 (-1 si ningun digito
///        nuevo este frame). El sistema:
///        - Si hay choices Y digitJustPressed entre 1..N, llama advance.
///        - Si no hay choices Y eJustPressed, llama continueNext.
///        No-op si !DialogSystem::isActive().
void tickActiveDialog(bool eJustPressed, int digitJustPressed);

} // namespace Mood::Dialog::DialogInteractSystem
