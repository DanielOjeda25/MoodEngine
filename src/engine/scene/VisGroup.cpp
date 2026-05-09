#include "engine/scene/VisGroup.h"

#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

namespace Mood {

u64 visgroupIdOf(Entity entity) {
    if (!entity) return 0;
    if (!entity.hasComponent<VisGroupMembershipComponent>()) return 0;
    return entity.getComponent<VisGroupMembershipComponent>().groupId;
}

bool isEntityHiddenByVisGroup(Scene& scene, Entity entity) {
    const u64 gid = visgroupIdOf(entity);
    if (gid == 0) return false;
    const VisGroup* g = scene.findVisGroup(gid);
    return g != nullptr && g->hidden;
}

} // namespace Mood
