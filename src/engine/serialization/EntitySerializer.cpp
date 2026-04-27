#include "engine/serialization/EntitySerializer.h"

#include "engine/assets/AssetManager.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/serialization/JsonHelpers.h" // adapters glm::vec3 <-> json

namespace Mood {

using json = nlohmann::json;

json serializeEntityToJson(Entity entity, const AssetManager& assets) {
    json je;
    const auto& tag = entity.getComponent<TagComponent>();
    je["tag"] = tag.name;

    const auto& t = entity.getComponent<TransformComponent>();
    json jt;
    jt["position"]      = t.position;
    jt["rotationEuler"] = t.rotationEuler;
    jt["scale"]         = t.scale;
    je["transform"] = jt;

    if (entity.hasComponent<MeshRendererComponent>()) {
        const auto& mr = entity.getComponent<MeshRendererComponent>();
        json jmr;
        jmr["mesh_path"] = assets.meshPathOf(mr.mesh);
        jmr["materials"] = json::array();
        for (TextureAssetId texId : mr.materials) {
            jmr["materials"].push_back(assets.pathOf(texId));
        }
        je["mesh_renderer"] = jmr;
    }

    if (entity.hasComponent<LightComponent>()) {
        const auto& lc = entity.getComponent<LightComponent>();
        json jl;
        jl["type"] = (lc.type == LightComponent::Type::Directional)
            ? "directional" : "point";
        jl["color"]     = lc.color;
        jl["intensity"] = lc.intensity;
        jl["radius"]    = lc.radius;
        jl["direction"] = lc.direction;
        jl["enabled"]   = lc.enabled;
        // Hito 16: solo persistimos castShadows si es true para no ensuciar
        // mapas viejos con un campo nuevo. Los archivos sin el campo lo
        // interpretan como false al cargar (default del SavedLight).
        if (lc.castShadows) jl["cast_shadows"] = true;
        je["light"] = jl;
    }

    if (entity.hasComponent<RigidBodyComponent>()) {
        const auto& rb = entity.getComponent<RigidBodyComponent>();
        json jrb;
        switch (rb.type) {
            case RigidBodyComponent::Type::Static:    jrb["type"] = "static";    break;
            case RigidBodyComponent::Type::Kinematic: jrb["type"] = "kinematic"; break;
            case RigidBodyComponent::Type::Dynamic:   jrb["type"] = "dynamic";   break;
        }
        switch (rb.shape) {
            case RigidBodyComponent::Shape::Box:     jrb["shape"] = "box";     break;
            case RigidBodyComponent::Shape::Sphere:  jrb["shape"] = "sphere";  break;
            case RigidBodyComponent::Shape::Capsule: jrb["shape"] = "capsule"; break;
        }
        jrb["halfExtents"] = rb.halfExtents;
        jrb["mass"]        = rb.mass;
        je["rigid_body"] = jrb;
    }

    if (entity.hasComponent<EnvironmentComponent>()) {
        const auto& env = entity.getComponent<EnvironmentComponent>();
        json je2;
        je2["skybox_path"] = env.skyboxPath;
        // FogMode -> string. Mantener orden alineado con el enum runtime.
        const char* fogModes[] = {"off", "linear", "exp", "exp2"};
        const u32 fm = (env.fogMode < 4) ? env.fogMode : 0;
        je2["fog_mode"]         = fogModes[fm];
        je2["fog_color"]        = env.fogColor;
        je2["fog_density"]      = env.fogDensity;
        je2["fog_linear_start"] = env.fogLinearStart;
        je2["fog_linear_end"]   = env.fogLinearEnd;
        je2["exposure"]         = env.exposure;
        const char* toneModes[] = {"none", "reinhard", "aces"};
        const u32 tm = (env.tonemapMode < 3) ? env.tonemapMode : 0;
        je2["tonemap_mode"] = toneModes[tm];
        je["environment"] = je2;
    }

    // Link suave al prefab (Hito 14 Bloque 6). Solo se persiste si la
    // entidad tiene un `PrefabLinkComponent`. Sin propagacion bidireccional
    // por ahora; es solo un breadcrumb para futuras features ("revertir a
    // prefab", "aplicar overrides", etc.).
    if (entity.hasComponent<PrefabLinkComponent>()) {
        const auto& link = entity.getComponent<PrefabLinkComponent>();
        if (!link.path.empty()) {
            je["prefab_path"] = link.path;
        }
    }
    return je;
}

SavedEntity parseEntityFromJson(const json& j) {
    SavedEntity se;
    se.tag = j.value("tag", std::string{});
    if (j.contains("transform")) {
        const auto& jt = j.at("transform");
        se.position      = jt.value("position",      glm::vec3{0.0f});
        se.rotationEuler = jt.value("rotationEuler", glm::vec3{0.0f});
        se.scale         = jt.value("scale",         glm::vec3{1.0f});
    }
    if (j.contains("mesh_renderer")) {
        const auto& jmr = j.at("mesh_renderer");
        SavedMeshRenderer mr;
        mr.meshPath = jmr.value("mesh_path", std::string{});
        if (jmr.contains("materials")) {
            for (const auto& m : jmr.at("materials")) {
                mr.materials.push_back(m.get<std::string>());
            }
        }
        se.meshRenderer = std::move(mr);
    }
    if (j.contains("light")) {
        const auto& jl = j.at("light");
        SavedLight sl;
        sl.type      = jl.value("type", std::string{"point"});
        sl.color     = jl.value("color",     glm::vec3{1.0f});
        sl.intensity = jl.value("intensity", 1.0f);
        sl.radius    = jl.value("radius",    10.0f);
        sl.direction = jl.value("direction", glm::vec3{0.0f, -1.0f, 0.0f});
        sl.enabled   = jl.value("enabled",   true);
        sl.castShadows = jl.value("cast_shadows", false); // Hito 16, opcional
        se.light = std::move(sl);
    }
    if (j.contains("rigid_body")) {
        const auto& jrb = j.at("rigid_body");
        SavedRigidBody srb;
        srb.type        = jrb.value("type",        std::string{"dynamic"});
        srb.shape       = jrb.value("shape",       std::string{"box"});
        srb.halfExtents = jrb.value("halfExtents", glm::vec3{0.5f});
        srb.mass        = jrb.value("mass",        1.0f);
        se.rigidBody = std::move(srb);
    }
    if (j.contains("environment")) {
        const auto& je = j.at("environment");
        SavedEnvironment se2;
        se2.skyboxPath     = je.value("skybox_path",     std::string{"skyboxes/sky_day"});
        se2.fogMode        = je.value("fog_mode",        std::string{"off"});
        se2.fogColor       = je.value("fog_color",       glm::vec3{0.55f, 0.65f, 0.75f});
        se2.fogDensity     = je.value("fog_density",     0.015f);
        se2.fogLinearStart = je.value("fog_linear_start", 5.0f);
        se2.fogLinearEnd   = je.value("fog_linear_end",   50.0f);
        se2.exposure       = je.value("exposure",        0.0f);
        se2.tonemapMode    = je.value("tonemap_mode",    std::string{"aces"});
        se.environment = std::move(se2);
    }
    se.prefabPath = j.value("prefab_path", std::string{});
    return se;
}

} // namespace Mood
