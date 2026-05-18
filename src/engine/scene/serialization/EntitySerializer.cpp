#include "engine/scene/serialization/EntitySerializer.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/VisGroup.h"  // F2H33: visgroupIdOf
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"  // F2H65: lookup target via entityFromHandle
#include "engine/scene/serialization/JsonHelpers.h" // adapters glm::vec3 <-> json

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
        // F2H55: bloom. Solo persistir si difiere del default para
        // mantener limpios los .moodmap pre-F2H55 que round-tripeen
        // sin tocar el componente.
        if (!env.bloomEnabled)              je2["bloom_enabled"]   = env.bloomEnabled;
        if (env.bloomThreshold != 1.0f)     je2["bloom_threshold"] = env.bloomThreshold;
        if (env.bloomIntensity != 0.6f)     je2["bloom_intensity"] = env.bloomIntensity;
        if (env.bloomRadius != 1.0f)        je2["bloom_radius"]    = env.bloomRadius;
        // F2H56: SSAO aditivo igual que bloom.
        if (!env.ssaoEnabled)               je2["ssao_enabled"]   = env.ssaoEnabled;
        if (env.ssaoRadius    != 0.5f)      je2["ssao_radius"]    = env.ssaoRadius;
        if (env.ssaoIntensity != 1.0f)      je2["ssao_intensity"] = env.ssaoIntensity;
        // F2H58: Color Grading aditivo. Default OFF -- el flag se
        // serializa solo cuando se prende. LUT path string vacio se
        // serializa siempre que diff del default (vacio).
        if (env.colorGradingEnabled)        je2["color_grading_enabled"]   = env.colorGradingEnabled;
        if (!env.colorGradingLutPath.empty()) je2["color_grading_lut"]       = env.colorGradingLutPath;
        if (env.colorGradingIntensity != 1.0f) je2["color_grading_intensity"] = env.colorGradingIntensity;
        // F2H60: CSM. Aditivo -- solo persiste si difiere del default.
        // El "enabled" global fue removido en F2H60 polish iter2 (gate
        // per-light via LightComponent::castShadows).
        if (env.csmCascadeCount != 4u)   je2["csm_cascades"] = env.csmCascadeCount;
        if (env.csmSplitLambda  != 0.5f) je2["csm_lambda"]   = env.csmSplitLambda;
        // F2H61: SSR aditivo. Default OFF.
        if (env.ssrEnabled)            je2["ssr_enabled"]   = env.ssrEnabled;
        if (env.ssrMaxSteps  != 32u)   je2["ssr_max_steps"] = env.ssrMaxSteps;
        if (env.ssrThickness != 0.5f)  je2["ssr_thickness"] = env.ssrThickness;
        if (env.ssrStepSize  != 0.2f)  je2["ssr_step_size"] = env.ssrStepSize;
        if (env.ssrIntensity != 0.5f)  je2["ssr_intensity"] = env.ssrIntensity;
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
            // Hito 40 A: cone axis solo se persiste si != +Y default.
            if (em.emissionShape == ParticleEmitterComponent::EmissionShape::Cone
                && em.emissionConeAxis != glm::vec3(0.0f, 1.0f, 0.0f)) {
                jpe["emission_cone_axis"] = em.emissionConeAxis;
            }
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

    // F2H48.1: DialogComponent. Solo dialogPath + autoStartOnInteract;
    // el cachedDialogId runtime no se persiste (loadDialog al primer
    // tick del DialogInteractSystem lo repuebla via VFS).
    if (entity.hasComponent<DialogComponent>()) {
        const auto& dc = entity.getComponent<DialogComponent>();
        if (!dc.dialogPath.empty()) {
            json jd;
            jd["path"]              = dc.dialogPath;
            jd["autoStartOnInteract"] = dc.autoStartOnInteract;
            je["dialog"] = jd;
        }
    }

    // F2H52: ItemPickupComponent. itemPath + quantity + destroyOnPickup;
    // cachedItemId runtime no se persiste (loadItem al primer trigger
    // del ItemPickupSystem lo repuebla via VFS). Convencion identica a
    // DialogComponent: si itemPath esta vacio NO se serializa (no tiene
    // sentido un pickup sin target).
    if (entity.hasComponent<ItemPickupComponent>()) {
        const auto& ip = entity.getComponent<ItemPickupComponent>();
        if (!ip.itemPath.empty()) {
            json ji;
            ji["path"]            = ip.itemPath;
            ji["quantity"]        = ip.quantity;
            ji["destroyOnPickup"] = ip.destroyOnPickup;
            je["item_pickup"] = ji;
        }
    }

    // F2H50 Bloque D: AnimatorComponent. clipName/speed/playing/loop +
    // lista `externalClips` (alias + path logico del .fbx standalone).
    // El `externalBindCache` runtime no se persiste — el AnimationSystem
    // lo re-genera al primer evaluate via `bindClipToSkeleton`. El `time`
    // tampoco se persiste (siempre arranca en 0 al cargar — comportamiento
    // estandar de "respawn" en motores: la anim empieza desde el frame 1
    // cuando la escena se restaura).
    if (entity.hasComponent<AnimatorComponent>()) {
        const auto& a = entity.getComponent<AnimatorComponent>();
        json ja;
        ja["clip_name"] = a.clipName;
        ja["speed"]     = a.speed;
        ja["playing"]   = a.playing;
        ja["loop"]      = a.loop;
        if (!a.externalClips.empty()) {
            ja["external_clips"] = json::array();
            for (const auto& [alias, clipId] : a.externalClips) {
                json jc;
                jc["alias"] = alias;
                jc["path"]  = assets.animationClipPathOf(clipId);
                ja["external_clips"].push_back(jc);
            }
        }
        je["animator"] = ja;
    }

    // F2H51 Bloque I: InventoryComponent. layout_mode + capacity per
    // modo (max_items / grid_w x grid_h / equipment_slots) + entries
    // como pairs {item_path, quantity, slot_index}. Mismo patron
    // paths-no-ids del animator: persistimos rutas logicas via
    // `AssetManager::itemPathOf(id)`. El SceneLoader las re-resuelve
    // con `loadItem` al cargar.
    if (entity.hasComponent<InventoryComponent>()) {
        const auto& inv = entity.getComponent<InventoryComponent>();
        const auto& st = inv.state;
        json ji;
        // Layout mode string.
        const char* modeStr = "flat_list";
        if (st.mode == Inventory::LayoutMode::Grid2D)         modeStr = "grid_2d";
        if (st.mode == Inventory::LayoutMode::EquipmentSlots) modeStr = "equipment_slots";
        ji["layout_mode"] = modeStr;
        // Capacity por modo. Persistimos los 3 grupos siempre — costos
        // bajos y el roundtrip preserva la configuracion aunque el dev
        // cambie de mode.
        ji["max_items"]   = st.config.max_items;
        ji["grid_width"]  = st.config.grid_width;
        ji["grid_height"] = st.config.grid_height;
        if (!st.config.equipment_slots.empty()) {
            ji["equipment_slots"] = json::array();
            for (const auto& s : st.config.equipment_slots) {
                json js;
                js["name"]       = s.name;
                js["tag_filter"] = s.tag_filter;
                ji["equipment_slots"].push_back(js);
            }
        }
        if (!st.entries.empty()) {
            ji["entries"] = json::array();
            for (const auto& e : st.entries) {
                json jen;
                jen["item_path"]  = assets.itemPathOf(e.itemId);
                jen["quantity"]   = e.quantity;
                jen["slot_index"] = e.slot_index;
                ji["entries"].push_back(jen);
            }
        }
        je["inventory"] = ji;
    }

    // F2H65: JointComponent. El target body B se persiste por TAG
    // (string) en lugar de raw entt handle — los handles no son estables
    // entre sesiones. El SceneLoader hace una segunda pasada post-load
    // para resolver tag -> nuevo handle.
    if (entity.hasComponent<JointComponent>()) {
        const auto& jc = entity.getComponent<JointComponent>();
        json jj;
        const char* typeStr = "hinge";
        switch (jc.type) {
            case JointComponent::Type::Hinge:    typeStr = "hinge";    break;
            case JointComponent::Type::Distance: typeStr = "distance"; break;
            case JointComponent::Type::Point:    typeStr = "point";    break;
        }
        jj["type"] = typeStr;
        // Resolver targetEntity (raw handle) -> tag via scene back-ref.
        // Si el handle no esta vivo o no tiene tag, target_tag queda vacio
        // (el load tratara la entidad como "sin target asignado").
        if (jc.targetEntity != kJointNoTarget && entity.scene() != nullptr) {
            const auto handle = static_cast<entt::entity>(jc.targetEntity);
            Entity tgt = entity.scene()->entityFromHandle(handle);
            if (static_cast<bool>(tgt) && tgt.hasComponent<TagComponent>()) {
                jj["target_tag"] = tgt.getComponent<TagComponent>().name;
            }
        }
        // pivotLocal siempre se persiste (default (0,0,0) es valido pero el
        // round-trip explicito evita confusion al editar el JSON a mano).
        jj["pivotLocal"] = jc.pivotLocal;
        // Campos especificos por tipo. Solo se escriben si != default para
        // no ensuciar el JSON con valores triviales.
        if (jc.type == JointComponent::Type::Hinge) {
            if (jc.axisLocal != glm::vec3(0.0f, 1.0f, 0.0f)) {
                jj["axisLocal"] = jc.axisLocal;
            }
            if (jc.limitMinDeg != -180.0f) jj["limit_min_deg"] = jc.limitMinDeg;
            if (jc.limitMaxDeg !=  180.0f) jj["limit_max_deg"] = jc.limitMaxDeg;
        } else if (jc.type == JointComponent::Type::Distance) {
            jj["min_distance"] = jc.minDistance;
            jj["max_distance"] = jc.maxDistance;
        }
        // Point: no extra fields.
        je["joint"] = jj;
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

    // F2H33 (v14): visgroupId opcional. Solo se emite si la entity tiene
    // componente con groupId != 0.
    const u64 vgId = visgroupIdOf(entity);
    if (vgId != 0) {
        je["visgroupId"] = vgId;
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
        // F2H55: bloom. F2H60 polish: defaults a OFF para mapas que no
        // tienen las keys (pre-F2H60 los mapas no las traian con OFF,
        // las traian con ON; este cambio rompe back-compat consciente
        // -- el dev quiere los efectos OFF por default).
        se2.bloomEnabled   = je.value("bloom_enabled",   false);
        se2.bloomThreshold = je.value("bloom_threshold", 1.0f);
        se2.bloomIntensity = je.value("bloom_intensity", 0.6f);
        se2.bloomRadius    = je.value("bloom_radius",    1.0f);
        // F2H56: SSAO. F2H60 polish: default OFF.
        se2.ssaoEnabled    = je.value("ssao_enabled",    false);
        se2.ssaoRadius     = je.value("ssao_radius",     0.5f);
        se2.ssaoIntensity  = je.value("ssao_intensity",  1.0f);
        // F2H58: Color Grading defaults para mapas pre-F2H58.
        se2.colorGradingEnabled   = je.value("color_grading_enabled",   false);
        se2.colorGradingLutPath   = je.value("color_grading_lut",       std::string{});
        se2.colorGradingIntensity = je.value("color_grading_intensity", 1.0f);
        // F2H60: CSM. Solo knobs de calidad -- "csm_enabled" legacy
        // ignorado (gate per-light via LightComponent::castShadows).
        se2.csmCascadeCount = je.value("csm_cascades", 4u);
        se2.csmSplitLambda  = je.value("csm_lambda",   0.5f);
        // F2H61: SSR defaults para mapas pre-F2H61.
        se2.ssrEnabled   = je.value("ssr_enabled",   false);
        se2.ssrMaxSteps  = je.value("ssr_max_steps", 32u);
        se2.ssrThickness = je.value("ssr_thickness", 0.5f);
        se2.ssrStepSize  = je.value("ssr_step_size", 0.2f);
        se2.ssrIntensity = je.value("ssr_intensity", 0.5f);
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
        pe.emissionConeAxis  = jpe.value("emission_cone_axis",  pe.emissionConeAxis);
        pe.texturePath  = jpe.value("texture_path",   std::string{});
        se.particleEmitter = std::move(pe);
    }

    if (j.contains("trigger")) {
        const auto& jtr = j.at("trigger");
        SavedTrigger st;
        st.halfExtents = jtr.value("halfExtents", glm::vec3{1.0f});
        se.trigger = std::move(st);
    }

    // F2H48.1: dialog.
    if (j.contains("dialog")) {
        const auto& jd = j.at("dialog");
        SavedDialog sd;
        sd.dialogPath = jd.value("path", std::string{});
        sd.autoStartOnInteract = jd.value("autoStartOnInteract", true);
        if (!sd.dialogPath.empty()) {
            se.dialog = std::move(sd);
        }
    }

    // F2H52: item_pickup.
    if (j.contains("item_pickup")) {
        const auto& ji = j.at("item_pickup");
        SavedItemPickup sip;
        sip.itemPath        = ji.value("path", std::string{});
        sip.quantity        = ji.value("quantity", 1);
        sip.destroyOnPickup = ji.value("destroyOnPickup", true);
        if (!sip.itemPath.empty()) {
            se.itemPickup = std::move(sip);
        }
    }

    // F2H50 Bloque D: animator.
    if (j.contains("animator")) {
        const auto& ja = j.at("animator");
        SavedAnimator sa;
        sa.clipName = ja.value("clip_name", std::string{});
        sa.speed    = ja.value("speed", 1.0f);
        sa.playing  = ja.value("playing", true);
        sa.loop     = ja.value("loop", true);
        if (ja.contains("external_clips") && ja.at("external_clips").is_array()) {
            for (const auto& jc : ja.at("external_clips")) {
                SavedAnimatorExternalClip ec;
                ec.alias = jc.value("alias", std::string{});
                ec.path  = jc.value("path", std::string{});
                if (!ec.alias.empty() && !ec.path.empty()) {
                    sa.externalClips.push_back(std::move(ec));
                }
            }
        }
        se.animator = std::move(sa);
    }

    // F2H51 Bloque I: inventory.
    if (j.contains("inventory")) {
        const auto& ji = j.at("inventory");
        SavedInventory si;
        si.layoutMode = ji.value("layout_mode", std::string{"flat_list"});
        si.maxItems   = ji.value("max_items",   20);
        si.gridWidth  = ji.value("grid_width",   4);
        si.gridHeight = ji.value("grid_height",  6);
        if (ji.contains("equipment_slots") && ji.at("equipment_slots").is_array()) {
            for (const auto& js : ji.at("equipment_slots")) {
                SavedInventoryEquipmentSlot s;
                s.name      = js.value("name",       std::string{});
                s.tagFilter = js.value("tag_filter", std::string{});
                si.equipmentSlots.push_back(std::move(s));
            }
        }
        if (ji.contains("entries") && ji.at("entries").is_array()) {
            for (const auto& jen : ji.at("entries")) {
                SavedInventoryEntry se2;
                se2.itemPath  = jen.value("item_path", std::string{});
                se2.quantity  = jen.value("quantity",  0);
                se2.slotIndex = jen.value("slot_index", -1);
                if (!se2.itemPath.empty() && se2.quantity > 0) {
                    si.entries.push_back(std::move(se2));
                }
            }
        }
        se.inventory = std::move(si);
    }

    // F2H65: joint. Aditivo — mapas pre-F2H65 sin el campo se leen igual
    // (la entidad queda sin JointComponent).
    if (j.contains("joint")) {
        const auto& jj = j.at("joint");
        SavedJoint sj;
        sj.type        = jj.value("type",            std::string{"hinge"});
        sj.targetTag   = jj.value("target_tag",      std::string{});
        sj.pivotLocal  = jj.value("pivotLocal",      glm::vec3{0.0f});
        sj.axisLocal   = jj.value("axisLocal",       glm::vec3{0.0f, 1.0f, 0.0f});
        sj.limitMinDeg = jj.value("limit_min_deg",   -180.0f);
        sj.limitMaxDeg = jj.value("limit_max_deg",    180.0f);
        sj.minDistance = jj.value("min_distance",      0.0f);
        sj.maxDistance = jj.value("max_distance",      1.0f);
        se.joint = std::move(sj);
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

    // F2H33 (v14): visgroupId opcional, default 0 (sin grupo).
    se.visgroupId = j.value("visgroupId", u64{0});
    return se;
}

} // namespace Mood
