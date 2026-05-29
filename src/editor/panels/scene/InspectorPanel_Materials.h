#pragma once

// F3H29: helpers compartidos de UI de materiales del Inspector. Antes
// vivían inline en `InspectorPanel_MeshRenderer.cpp` y el Brush solo
// listaba slots como texto plano (`TextDisabled`) sin permitir editar
// nada. El dev pidió unificar la UI estilo Blender: misma lista
// ListBox + panel del slot seleccionado tanto en MeshRenderer como en
// Brush.
//
// Los 3 helpers operan sobre un `MaterialAsset*` (que vive en el
// `AssetManager`), por lo que son agnósticos al componente owner. El
// drop-target queda local en cada caller porque ahí sí cambia la
// estructura mutada (MeshRenderer.materials vs Brush.materials).

#include "core/Types.h"  // F3H29: u32 para MaterialAssetId
#include "engine/render/resources/MaterialAsset.h"

namespace Mood {

class AssetManager;
class EditorUI;
class Entity;
struct InspectorEditTracker;

// Local typedef — el real está en AssetManager.h pero queremos evitar
// arrastrar todo el include en este header público del módulo.
using MaterialAssetId = u32;

namespace InspectorMaterials {

/// PBR multipliers (albedoTint / metallic / roughness / ao). Undoable
/// via `pushEditIfDone`. Toca `mat` in-place; el AssetManager queda
/// como source of truth, los renderers leen del mismo MaterialAsset.
void drawPbrMultipliers(MaterialAsset* mat,
                          AssetManager* assets,
                          MaterialAssetId matId,
                          InspectorEditTracker& tracker,
                          EditorUI* ui, Entity e,
                          bool& editedFlag);

/// Shader graph picker (PBR estándar vs .moodshader del proyecto).
/// `isSkinned` / `isInstanced` solo aplican al MeshRenderer para
/// mostrar el warning de fallback; en Brush ambos son false.
void drawShaderGraph(MaterialAsset* mat, AssetManager* assets,
                       MaterialAssetId matId,
                       EditorUI* ui,
                       bool isSkinned, bool isInstanced,
                       bool& editedFlag);

/// Blending (Opaque / Translucent / Additive) + opacity + IOR +
/// refractionStrength + castTranslucentShadow.
void drawBlending(MaterialAsset* mat, AssetManager* assets,
                    MaterialAssetId matId,
                    InspectorEditTracker& tracker,
                    EditorUI* ui, Entity e, bool& editedFlag);

} // namespace InspectorMaterials

} // namespace Mood
