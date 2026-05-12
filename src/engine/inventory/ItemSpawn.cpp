#include "engine/inventory/ItemSpawn.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>
#include <vector>

namespace Mood::Inventory {

bool spawnPickupInWorld(Scene* scene,
                         AssetManager* assets,
                         const std::string& itemPath,
                         const glm::vec3& worldPos,
                         int quantity) {
    if (scene == nullptr) {
        Log::script()->warn("[ItemSpawn] no hay scene");
        return false;
    }
    if (assets == nullptr) {
        Log::script()->warn("[ItemSpawn] no hay AssetManager");
        return false;
    }
    const ItemAssetId id = assets->loadItem(itemPath);
    if (id == 0) {
        Log::script()->warn("[ItemSpawn] item '{}' no existe", itemPath);
        return false;
    }
    const Asset* asset = assets->getItem(id);
    const int q = (quantity > 0) ? quantity : 1;

    // Mesh: model_path si esta seteado, sino cubo default.
    MeshAssetId meshId = 0;
    const bool hasModel = (asset != nullptr && !asset->model_path.empty());
    if (hasModel) {
        meshId = assets->loadMesh(asset->model_path);
    }
    // Material: model_path -> createMaterialsForMesh;
    //           sino icon_path -> textura como albedo;
    //           sino default (placeholder).
    std::vector<MaterialAssetId> mats;
    const bool hasIcon = (asset != nullptr && !asset->icon_path.empty());
    if (hasModel) {
        mats = assets->createMaterialsForMesh(meshId);
    } else if (hasIcon) {
        const TextureAssetId tex = assets->loadTexture(asset->icon_path);
        mats.push_back(assets->createMaterialFromTexture(tex));
    } else {
        mats = assets->createMaterialsForMesh(meshId);
    }

    std::string displayName;
    if (asset != nullptr && !asset->name_literal.empty()) {
        displayName = asset->name_literal;
    } else if (asset != nullptr && !asset->id.empty()) {
        displayName = asset->id;
    } else {
        displayName = "item";
    }

    Entity e = scene->createEntity("Pickup_" + displayName);
    auto& t = e.getComponent<TransformComponent>();
    t.position = worldPos;
    t.scale    = glm::vec3(1.0f);

    e.addComponent<MeshRendererComponent>(meshId, std::move(mats));

    TriggerComponent trig;
    trig.halfExtents = glm::vec3(0.5f);
    e.addComponent<TriggerComponent>(trig);

    ItemPickupComponent ip;
    ip.itemPath = itemPath;
    ip.quantity = q;
    e.addComponent<ItemPickupComponent>(ip);

    return true;
}

} // namespace Mood::Inventory
