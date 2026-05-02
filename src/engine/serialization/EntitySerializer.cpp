#include "engine/serialization/EntitySerializer.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/MaterialAsset.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/serialization/JsonHelpers.h" // adapters glm::vec3 <-> json

#include <variant>

namespace Mood {

using json = nlohmann::json;

namespace {

// Hito 24: serializa un `ExposedValue` (variant) al primitivo JSON que le
// corresponde. vec3 se escribe como array de 3 floats (mismo formato que
// glm::vec3 via el adl_serializer de JsonHelpers).
json exposedValueToJson(const ExposedValue& v) {
    return std::visit([](auto&& val) -> json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, glm::vec3>) {
            return json(val); // adl_serializer<glm::vec3> -> [x, y, z]
        } else {
            return json(val);
        }
    }, v);
}

// Lee un primitivo JSON y lo devuelve como `ExposedValue`. Tipos no
// soportados (objetos, arrays != size 3, null) devuelven nullopt — el
// caller loggea warning + skip.
std::optional<ExposedValue> jsonToExposedValue(const json& jv) {
    if (jv.is_boolean()) return ExposedValue{jv.get<bool>()};
    if (jv.is_number()) return ExposedValue{jv.get<f32>()};
    if (jv.is_string()) return ExposedValue{jv.get<std::string>()};
    if (jv.is_array() && jv.size() == 3) {
        return ExposedValue{jv.get<glm::vec3>()};
    }
    return std::nullopt;
}

} // namespace

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
        // Hito 17 (v7): el campo `materials` ahora persiste paths de
        // `.material`. Para materiales auto-generados desde una textura
        // (path interno tipo `__tex#<id>`) reescribimos al path logico
        // de la textura referenciada, asi un round-trip "drop textura,
        // guardar, cargar" sigue funcionando sin requerir un .material
        // explicito en disco. El loader detecta la extension al cargar.
        jmr["materials"] = json::array();
        for (MaterialAssetId matId : mr.materials) {
            std::string matPath = assets.materialPathOf(matId);
            // Auto-generated material wrapper: persist the underlying
            // texture path en su lugar (back-compat con escenas pre-PBR).
            // Cubre tanto el cacheado `__tex#` como el instance-unico
            // `__runtime_tex#` (este ultimo introducido para no compartir
            // material entre cubos spawneados con la misma textura).
            const bool isTexWrapper =
                matPath.rfind("__tex#", 0) == 0 ||
                matPath.rfind("__runtime_tex#", 0) == 0;
            if (isTexWrapper) {
                const MaterialAsset* m = assets.getMaterial(matId);
                matPath = (m != nullptr)
                    ? assets.pathOf(m->albedo)
                    : std::string{};
            } else if (matPath.rfind("__", 0) == 0) {
                // Default material u otro sentinela: persistir como vacio.
                matPath.clear();
            }
            jmr["materials"].push_back(matPath);
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
        // Hito 34 A: solo persistir friction si difiere del default (0.5)
        // para no ensuciar mapas viejos con un campo nuevo. El loader
        // interpreta ausente como 0.5 (default del SavedRigidBody).
        if (rb.friction != 0.5f) jrb["friction"] = rb.friction;
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
        // Hito 18: solo persistir si != 1.0 (default) para no ensuciar
        // archivos viejos con un campo nuevo.
        if (env.iblIntensity != 1.0f) je2["ibl_intensity"] = env.iblIntensity;
        je["environment"] = je2;
    }

    // Hito 29: ParticleEmitterComponent. Solo configuracion editable;
    // estado runtime (positions/ages/rngState) NO se persiste — la
    // simulacion arranca limpia al cargar.
    if (entity.hasComponent<ParticleEmitterComponent>()) {
        const auto& em = entity.getComponent<ParticleEmitterComponent>();
        json jpe;
        jpe["emit_rate"]      = em.emitRate;
        jpe["lifetime_min"]   = em.lifetimeMin;
        jpe["lifetime_max"]   = em.lifetimeMax;
        jpe["velocity_min"]   = em.velocityMin;
        jpe["velocity_max"]   = em.velocityMax;
        jpe["size_start"]     = em.sizeStart;
        jpe["size_end"]       = em.sizeEnd;
        jpe["color_start"]    = em.colorStart;
        jpe["color_end"]      = em.colorEnd;
        jpe["gravity_factor"] = em.gravityFactor;
        jpe["max_particles"]  = em.maxParticles;
        jpe["emitting"]       = em.emitting;
        jpe["additive"]       = em.additive;
        jpe["local_space"]    = em.localSpace;
        // Hito 37 C: shape solo se persiste si != Point default.
        if (em.emissionShape != ParticleEmitterComponent::EmissionShape::Point) {
            const char* shapeStr = "point";
            switch (em.emissionShape) {
                case ParticleEmitterComponent::EmissionShape::Box:    shapeStr = "box";    break;
                case ParticleEmitterComponent::EmissionShape::Sphere: shapeStr = "sphere"; break;
                case ParticleEmitterComponent::EmissionShape::Disc:   shapeStr = "disc";   break;
                case ParticleEmitterComponent::EmissionShape::Cone:   shapeStr = "cone";   break;
                default: break;
            }
            jpe["emission_shape"]      = shapeStr;
            jpe["emission_shape_size"] = em.emissionShapeSize;
        }
        // Texture path logico (no el id volátil). Vacio si no hay.
        if (em.texture != 0) {
            jpe["texture_path"] = assets.pathOf(em.texture);
        }
        je["particle_emitter"] = jpe;
    }

    // Hito 24: ScriptComponent (path + overrides de exposed properties).
    // Solo persistimos si el path es no-vacio (un script vacio en la
    // entidad sin nada cargado no aporta nada al round-trip). Los
    // exposedProps descubiertos NO se serializan; se redescubren al
    // cargar el script.
    if (entity.hasComponent<ScriptComponent>()) {
        const auto& sc = entity.getComponent<ScriptComponent>();
        if (!sc.path.empty()) {
            json js;
            js["path"] = sc.path;
            if (!sc.overrides.empty()) {
                json jov = json::object();
                for (const auto& [name, val] : sc.overrides) {
                    jov[name] = exposedValueToJson(val);
                }
                js["overrides"] = jov;
            }
            je["script"] = js;
        }
    }

    // Hito 33: TriggerComponent. Solo halfExtents; el flag runtime
    // playerInside no se persiste (lo redetecta el TriggerSystem al
    // primer frame post-load).
    if (entity.hasComponent<TriggerComponent>()) {
        const auto& tc = entity.getComponent<TriggerComponent>();
        json jtr;
        jtr["halfExtents"] = tc.halfExtents;
        je["trigger"] = jtr;
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
        srb.friction    = jrb.value("friction",    0.5f);   // Hito 34 A
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
        se2.iblIntensity   = je.value("ibl_intensity",   1.0f); // Hito 18
        se.environment = std::move(se2);
    }
    if (j.contains("particle_emitter")) {
        const auto& jpe = j.at("particle_emitter");
        SavedParticleEmitter pe;
        pe.emitRate     = jpe.value("emit_rate",      pe.emitRate);
        pe.lifetimeMin  = jpe.value("lifetime_min",   pe.lifetimeMin);
        pe.lifetimeMax  = jpe.value("lifetime_max",   pe.lifetimeMax);
        pe.velocityMin  = jpe.value("velocity_min",   pe.velocityMin);
        pe.velocityMax  = jpe.value("velocity_max",   pe.velocityMax);
        pe.sizeStart    = jpe.value("size_start",     pe.sizeStart);
        pe.sizeEnd      = jpe.value("size_end",       pe.sizeEnd);
        pe.colorStart   = jpe.value("color_start",    pe.colorStart);
        pe.colorEnd     = jpe.value("color_end",      pe.colorEnd);
        pe.gravityFactor = jpe.value("gravity_factor", pe.gravityFactor);
        pe.maxParticles = jpe.value("max_particles",  pe.maxParticles);
        pe.emitting     = jpe.value("emitting",       pe.emitting);
        pe.additive     = jpe.value("additive",       pe.additive);
        pe.localSpace   = jpe.value("local_space",    pe.localSpace);
        // Hito 37 C: emission shape opcional, default "point".
        pe.emissionShape     = jpe.value("emission_shape",      pe.emissionShape);
        pe.emissionShapeSize = jpe.value("emission_shape_size", pe.emissionShapeSize);
        pe.texturePath  = jpe.value("texture_path",   std::string{});
        se.particleEmitter = std::move(pe);
    }

    if (j.contains("trigger")) {
        const auto& jtr = j.at("trigger");
        SavedTrigger st;
        st.halfExtents = jtr.value("halfExtents", glm::vec3{1.0f});
        se.trigger = std::move(st);
    }

    if (j.contains("script")) {
        const auto& js = j.at("script");
        SavedScript ss;
        ss.path = js.value("path", std::string{});
        if (js.contains("overrides") && js.at("overrides").is_object()) {
            for (const auto& item : js.at("overrides").items()) {
                const auto& name = item.key();
                const auto& jv   = item.value();
                if (auto opt = jsonToExposedValue(jv); opt.has_value()) {
                    ss.overrides[name] = std::move(*opt);
                } else {
                    Log::script()->warn(
                        "EntitySerializer: override '{}' tiene tipo no soportado, skipeando",
                        name);
                }
            }
        }
        if (!ss.path.empty() || !ss.overrides.empty()) {
            se.script = std::move(ss);
        }
    }

    se.prefabPath = j.value("prefab_path", std::string{});
    return se;
}

} // namespace Mood
