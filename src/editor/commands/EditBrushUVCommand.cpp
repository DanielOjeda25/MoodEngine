#include "editor/commands/EditBrushUVCommand.h"

#include "core/Log.h"
#include "core/math/Plane.h"  // kPlaneEpsilon
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

#include <cmath>
#include <utility>

namespace Mood {

BrushUVSnapshot captureBrushUV(const Csg::Brush& brush) {
    BrushUVSnapshot snap;
    snap.uAxes.reserve(brush.faces.size());
    snap.vAxes.reserve(brush.faces.size());
    snap.uvOffsets.reserve(brush.faces.size());
    snap.uvScales.reserve(brush.faces.size());
    snap.uvRotations.reserve(brush.faces.size());
    snap.lockToWorlds.reserve(brush.faces.size());
    for (const auto& f : brush.faces) {
        snap.uAxes.push_back(f.uAxis);
        snap.vAxes.push_back(f.vAxis);
        snap.uvOffsets.push_back(f.uvOffset);
        snap.uvScales.push_back(f.uvScale);
        snap.uvRotations.push_back(f.uvRotation);
        snap.lockToWorlds.push_back(f.lockToWorld ? 1u : 0u);
    }
    return snap;
}

void applyBrushUV(Csg::Brush& brush, const BrushUVSnapshot& snap) {
    if (snap.uAxes.size() != brush.faces.size()) {
        Log::editor()->warn(
            "applyBrushUV: snapshot tiene {} faces, brush tiene {}; saltea",
            snap.uAxes.size(), brush.faces.size());
        return;
    }
    for (usize i = 0; i < brush.faces.size(); ++i) {
        auto& f = brush.faces[i];
        f.uAxis       = snap.uAxes[i];
        f.vAxis       = snap.vAxes[i];
        f.uvOffset    = snap.uvOffsets[i];
        f.uvScale     = snap.uvScales[i];
        f.uvRotation  = snap.uvRotations[i];
        f.lockToWorld = (snap.lockToWorlds[i] != 0);
    }
}

bool snapshotsEqual(const BrushUVSnapshot& a, const BrushUVSnapshot& b) {
    if (a.uAxes.size() != b.uAxes.size()) return false;
    const usize n = a.uAxes.size();
    for (usize i = 0; i < n; ++i) {
        // vec3 / vec2: comparar componente a componente con kPlaneEpsilon.
        auto closeVec3 = [&](const glm::vec3& x, const glm::vec3& y) {
            return std::fabs(x.x - y.x) < kPlaneEpsilon
                && std::fabs(x.y - y.y) < kPlaneEpsilon
                && std::fabs(x.z - y.z) < kPlaneEpsilon;
        };
        auto closeVec2 = [&](const glm::vec2& x, const glm::vec2& y) {
            return std::fabs(x.x - y.x) < kPlaneEpsilon
                && std::fabs(x.y - y.y) < kPlaneEpsilon;
        };
        if (!closeVec3(a.uAxes[i], b.uAxes[i])) return false;
        if (!closeVec3(a.vAxes[i], b.vAxes[i])) return false;
        if (!closeVec2(a.uvOffsets[i], b.uvOffsets[i])) return false;
        if (!closeVec2(a.uvScales[i], b.uvScales[i])) return false;
        if (std::fabs(a.uvRotations[i] - b.uvRotations[i]) > kPlaneEpsilon) {
            return false;
        }
        if (a.lockToWorlds[i] != b.lockToWorlds[i]) return false;
    }
    return true;
}

EditBrushUVCommand::EditBrushUVCommand(
    Scene* scene,
    std::string entityTag,
    BrushUVSnapshot oldSnap,
    BrushUVSnapshot newSnap,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_oldSnap(std::move(oldSnap)),
      m_newSnap(std::move(newSnap)),
      m_label(std::move(label)) {}

void EditBrushUVCommand::execute() {
    applySnapshot(m_newSnap);
}

void EditBrushUVCommand::undo() {
    applySnapshot(m_oldSnap);
}

void EditBrushUVCommand::applySnapshot(const BrushUVSnapshot& snap) {
    if (m_scene == nullptr) return;
    Entity target;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(target) && tag.name == m_entityTag) {
            target = e;
        }
    });
    if (!static_cast<bool>(target) ||
        !target.hasComponent<BrushComponent>()) {
        Log::editor()->warn(
            "EditBrushUVCommand: entidad '{}' invalida o sin BrushComponent",
            m_entityTag);
        return;
    }
    auto& bc = target.getComponent<BrushComponent>();
    applyBrushUV(bc.brush, snap);
    // Recompute cache de lockToWorld + marcar dirty.
    bc.anyFaceLockToWorld = false;
    for (const auto& f : bc.brush.faces) {
        if (f.lockToWorld) {
            bc.anyFaceLockToWorld = true;
            break;
        }
    }
    bc.dirty = true;
}

} // namespace Mood
