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
    // F2H86: Environment (config global) tiene prioridad alta — convencion
    // Unity Volume / Unreal PostProcessVolume / Godot WorldEnvironment.
    // Antes de MeshRenderer porque si alguien adjunta ambos (defensivo),
    // el rol del entity es "Environment", no "geometria".
    if (e.hasComponent<EnvironmentComponent>())       return ICON_FA_GLOBE;
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

// F3H27: overload con hint de jerarquia. Empty con hijos => OBJECT_GROUP
// (icon canonico Blender/Unity para Group). Empty sin hijos => CIRCLE.
inline const char* iconForEntity(Entity e, bool hasChildren) {
    if (e.hasComponent<EnvironmentComponent>())       return ICON_FA_GLOBE;
    if (e.hasComponent<MeshRendererComponent>())     return ICON_FA_CUBE;
    if (e.hasComponent<BrushComponent>())             return ICON_FA_CUBES_STACKED;
    if (e.hasComponent<LightComponent>())             return ICON_FA_LIGHTBULB;
    if (e.hasComponent<AudioSourceComponent>())       return ICON_FA_VOLUME_HIGH;
    if (e.hasComponent<ScriptComponent>())            return ICON_FA_FILE_CODE;
    if (e.hasComponent<TriggerComponent>())           return ICON_FA_BORDER_NONE;
    if (e.hasComponent<CameraComponent>())            return ICON_FA_VIDEO;
    if (e.hasComponent<ParticleEmitterComponent>())   return ICON_FA_FIRE;
    // F3H27: FOLDER es seguro (rasterizado en el atlas — lo usa MenuBar
    // "Archivo"). OBJECT_GROUP / CIRCLE salieron tofu en el primer
    // intento sobre este atlas. Semantica "carpeta = contenedor de
    // hijos" es estandar (Unity GameObject empty, Source Hammer group).
    return hasChildren ? ICON_FA_FOLDER : ICON_FA_CIRCLE;
}

} // namespace Mood
