#pragma once

// F2H37 — Helpers compartidos para mapear tipos del dominio a iconos
// FontAwesome. Consolida lo que pre-F2H37 estaba duplicado en
// `entityIconStr` de HierarchyPanel + VisGroupsPanel.
//
// `iconForEntity(Entity)` revisa los componentes de la entidad y
// devuelve el icono FA correspondiente al primer match en orden de
// prioridad (mismo orden que `entityIconStr` original — preserva
// behavior). Devuelve `ICON_FA_CIRCLE` para entidades sin
// componentes visibles (raras pero posibles).
//
// Inline para que el dispatch sea trivial — sin .cpp separado.

#include "editor/ui/IconsFontAwesome6.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"

namespace Mood {

inline const char* iconForEntity(Entity e) {
    if (e.hasComponent<MeshRendererComponent>())     return ICON_FA_CUBE;
    if (e.hasComponent<BrushComponent>())             return ICON_FA_CUBES_STACKED;
    if (e.hasComponent<LightComponent>())             return ICON_FA_LIGHTBULB;
    if (e.hasComponent<AudioSourceComponent>())       return ICON_FA_VOLUME_HIGH;
    if (e.hasComponent<ScriptComponent>())            return ICON_FA_FILE_CODE;
    if (e.hasComponent<TriggerComponent>())           return ICON_FA_BORDER_NONE;
    if (e.hasComponent<CameraComponent>())            return ICON_FA_VIDEO;
    if (e.hasComponent<ParticleEmitterComponent>())   return ICON_FA_FIRE;
    return ICON_FA_CIRCLE;
}

} // namespace Mood
