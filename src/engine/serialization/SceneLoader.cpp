#include "engine/serialization/SceneLoader.h"

#include "engine/assets/AssetManager.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/SceneSerializer.h"

#include <utility>
#include <vector>

namespace Mood::SceneLoader {

void applyEntitiesToScene(const SavedMap& saved,
                          Scene& scene,
                          AssetManager& assets) {
    for (const auto& se : saved.entities) {
        Entity e = scene.createEntity(se.tag);
        auto& t = e.getComponent<TransformComponent>();
        t.position      = se.position;
        t.rotationEuler = se.rotationEuler;
        t.scale         = se.scale;

        if (se.meshRenderer.has_value()) {
            const auto& mr = *se.meshRenderer;
            const MeshAssetId meshId = assets.loadMesh(mr.meshPath);
            // Hito 17 upgrader: el campo `materials` puede contener
            // paths de `.material` (v7) o paths de texturas (v6 con
            // back-compat). Detectamos por extension.
            std::vector<MaterialAssetId> mats;
            mats.reserve(mr.materials.size());
            for (const auto& path : mr.materials) {
                if (path.empty()) {
                    mats.push_back(assets.missingMaterialId());
                    continue;
                }
                const bool isMaterial =
                    path.size() >= 9 &&
                    path.compare(path.size() - 9, 9, ".material") == 0;
                if (isMaterial) {
                    mats.push_back(assets.loadMaterial(path));
                } else {
                    const TextureAssetId tex = assets.loadTexture(path);
                    mats.push_back(assets.loadMaterialFromTexture(tex));
                }
            }
            e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
        }

        if (se.light.has_value()) {
            const auto& sl = *se.light;
            LightComponent lc{};
            lc.type = (sl.type == "directional")
                ? LightComponent::Type::Directional
                : LightComponent::Type::Point;
            lc.color       = sl.color;
            lc.intensity   = sl.intensity;
            lc.radius      = sl.radius;
            lc.direction   = sl.direction;
            lc.enabled     = sl.enabled;
            lc.castShadows = sl.castShadows;
            e.addComponent<LightComponent>(lc);
        }

        if (se.rigidBody.has_value()) {
            const auto& srb = *se.rigidBody;
            RigidBodyComponent rb{};
            if (srb.type == "static")         rb.type = RigidBodyComponent::Type::Static;
            else if (srb.type == "kinematic") rb.type = RigidBodyComponent::Type::Kinematic;
            else                              rb.type = RigidBodyComponent::Type::Dynamic;
            if (srb.shape == "sphere")        rb.shape = RigidBodyComponent::Shape::Sphere;
            else if (srb.shape == "capsule")  rb.shape = RigidBodyComponent::Shape::Capsule;
            else                              rb.shape = RigidBodyComponent::Shape::Box;
            rb.halfExtents = srb.halfExtents;
            rb.mass        = srb.mass;
            e.addComponent<RigidBodyComponent>(rb);
        }

        if (se.environment.has_value()) {
            const auto& s = *se.environment;
            EnvironmentComponent env{};
            env.skyboxPath = s.skyboxPath;
            if      (s.fogMode == "linear") env.fogMode = 1;
            else if (s.fogMode == "exp")    env.fogMode = 2;
            else if (s.fogMode == "exp2")   env.fogMode = 3;
            else                            env.fogMode = 0;
            env.fogColor       = s.fogColor;
            env.fogDensity     = s.fogDensity;
            env.fogLinearStart = s.fogLinearStart;
            env.fogLinearEnd   = s.fogLinearEnd;
            env.exposure       = s.exposure;
            if      (s.tonemapMode == "reinhard") env.tonemapMode = 1;
            else if (s.tonemapMode == "aces")     env.tonemapMode = 2;
            else                                  env.tonemapMode = 0;
            env.iblIntensity   = s.iblIntensity;
            e.addComponent<EnvironmentComponent>(env);
        }

        if (!se.prefabPath.empty()) {
            e.addComponent<PrefabLinkComponent>(se.prefabPath);
        }
    }
}

} // namespace Mood::SceneLoader
