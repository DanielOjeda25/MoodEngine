# PLAN F3H13 — Reset to default per-field del Inspector (CIERRA Sub-fase 3.2)

**Estado:** **CERRADO** (`v2.13.0-fase3-hito13`, 2026-05-26).
**Predecesor:** F3H12 (Undo coverage audit del Inspector).
**Cierra Sub-fase 3.2** (6/6).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.2 lista "Reset to default en cada Inspector field".

---

## Resumen

Convención Unity/Unreal: cada field editable del Inspector tiene un botón `↺` (rotate-left) que aparece **solo cuando `current != default`**, sin visual noise para valores en default. Hace fácil al dev devolver un override al estado canónico sin pisar el valor a mano.

Reusa la infra de F3H12 (`pushAtomicEdit<T>`, `EditPropertyCommand<T>`, `HistoryStack`): el reset es un edit más en el stack — Ctrl+Z lo deshace y devuelve al valor que el dev tenía antes.

---

## Diseño

### Helper template — `detail::inspectorResetButton<T>`

Ubicación: `src/editor/panels/scene/InspectorPanel_Internal.h` (compartido por todos los partials del Inspector). Firma:

```cpp
template <typename T>
inline bool inspectorResetButton(EditorUI* ui, Entity e,
        const char* idSuffix,
        const T& current,
        const T& defaultValue,
        typename EditPropertyCommand<T>::Setter setter,
        const std::string& cmdLabel);
```

- Returns `true` si se hizo reset este frame (caller setea `m_editedThisFrame`).
- Si `current == defaultValue` no renderea NADA (visual noise zero).
- Llamada **inmediatamente después** del widget editable (SameLine + SmallButton).
- Internamente: `pushAtomicEdit<T>(ui, e, current, defaultValue, setter, cmdLabel)`.

### Style polish (iteraciones con el dev)

Las primeras versiones del botón se veían "muy alejadas" del control. Tras 5 iteraciones de feedback visual el ajuste final fue:

- `ImGui::SameLine(0.0f, 3.0f)` — 3 px externos (apenas aire entre el label del widget y el icono).
- `ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f))` — padding interno fino para que el icono ↺ respire dentro del botón sin inflarlo.

Total visual entre el label y el icono: ~7 px. Suficientemente cerca para sentirse parte del control, suficientemente lejos para no fusionarse.

### Tooltip

Key i18n nueva: `editor.panel.inspector.reset_default` ("Restablecer al valor por defecto" / "Reset to default value"). Se muestra al hover. Una sola key compartida por todos los reset buttons (no hace falta una por field — la semántica es la misma).

---

## Cobertura

Tier 1 acotado del hito — los 6 paneles más usados:

| Panel | Fields con reset | LOC añadidas approx |
|---|---|---|
| **Light** (`InspectorPanel_Light.cpp`) | enabled, type, color, intensity, radius, direction, castShadows | 7 |
| **Trigger** (`InspectorPanel_Misc.cpp`) | halfExtents, triggerOnEnter, triggerOnExit, fireOnce | 4 |
| **ForceField** (`InspectorPanel_Misc.cpp`) | strength | 1 |
| **ParticleEmitter** (`InspectorPanel_Particles.cpp`) | emitting, additive, emitRate, maxParticles | 4 |
| **AudioSource** (`InspectorPanel_Audio.cpp`) | volume, loop, playOnStart, is3D | 4 |
| **RigidBody** (`InspectorPanel_Physics.cpp`) | type, mass, friction, isSensor | 4 |

Total: **6 paneles, ~24 reset buttons**. Defaults declarados como `constexpr`/`static const` al inicio de cada `renderXxxSection`, en sync con la construcción `{}` del componente.

### Diferidos (con backlog en memoria `project_reset_button_coverage`)

- **Cloth/Joint/Ragdoll** — semántica especializada (defaults discutibles, fields de joint varían por tipo).
- **MeshRenderer** — defaults per-slot de material no son triviales (cada slot trae su textura/color/factor distinto, no hay "canónico" genérico).
- **Brush** — vertices/faces no son property-drawer; el "reset" semántico es vaciar el brush, ya cubierto por otro flow.

El helper queda en Internal.h listo para extender — replicar el patrón cuando un dev pida resets en alguno de los diferidos.

---

## Decisiones

1. **Helper en Internal.h vs duplicar `resetButton<T>` de `ProjectSettingsPanel`/`UserPreferencesPanel`**: archivo separado por contexto. Internal.h es del Inspector y entiende `Entity` + `EditorUI` + `HistoryStack` (sus reset buttons necesitan undo); los de Settings/Preferences operan sobre copias locales del struct + `dirty/saveNow` flags (no van al HistoryStack del Inspector). Duplicar es más limpio que generalizar.

2. **`pushAtomicEdit<T>` reusado, no nuevo helper**: el reset es semánticamente idéntico a un combo/checkbox change que F3H12 ya cubría — un cambio atómico que mueve `current → defaultValue`. Sin tracker drag, sin live preview. Reuso directo.

3. **No-render cuando `current == default`**: convención Unity/Unreal probada — el override es información, lo canónico no. Evita ruido en Inspector con todos los fields en default.

4. **Scope acotado a 6 paneles**: F3H13 cierra Sub-fase 3.2 con el hito que el plan original prevé, pero NO infla scope a Cloth/Joint/Ragdoll/MeshRenderer/Brush. Esos quedan en backlog explícito (memoria `project_reset_button_coverage`) con el helper listo para reusar.

---

## Lo que NO toca F3H13

- Cloth/Joint/Ragdoll resets (diferidos por uso bajo + semántica especializada).
- MeshRenderer per-slot reset (defaults compuestos no triviales).
- Brush reset (no es property-drawer; flow distinto).
- Script/Animator/Vehicle resets (no incluidos — quedan en backlog si el dev los reclama).
- Sub-fase 3.3 (Asset Browser): F3H14+.
