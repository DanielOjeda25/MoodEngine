// F2H81 (auditoría): lado READ del EntitySerializer (parseEntityFromJson).
// Extraído de EntitySerializer.cpp (que pasaba las 800 líneas) — el lado
// WRITE (serializeEntityToJson) queda allá. Ambos comparten los Saved*
// structs de EntitySerializer.h.

#include "engine/scene/serialization/EntitySerializer.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"  // ExposedValue
#include "engine/scene/serialization/JsonHelpers.h"  // glm::vec3 <-> json

#include <optional>
#include <variant>

namespace Mood {

using json = nlohmann::json;

namespace {

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

SavedEntity parseEntityFromJson(const json& j) {
    SavedEntity se;
    se.tag = j.value("tag", std::string{});
    // F3H9: entity_type opcional. Vacio = pre-F3H9 (back-compat); el
    // SceneLoader inferira de los componentes presentes.
    se.entityType = j.value("entity_type", std::string{});
    // F3H27: parent_tag opcional. Vacio = root (sin padre); resolucion
    // en 2-pass en SceneLoader::applyEntities tras materializar todo.
    se.parentTag = j.value("parent_tag", std::string{});
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
        // F2H67: sub_mesh_name aditivo (mapas pre-F2H67 lo dejan vacio).
        mr.subMeshName = jmr.value("sub_mesh_name", std::string{});
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
        srb.isSensor    = jrb.value("is_sensor",   false);   // F2H68
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
        // F3H31: sky procedural. Defaults para mapas pre-F3H31 = HDRI
        // legacy + params neutros (12h, turbidity 2.5, ground 0.3 grey).
        se2.skyboxSource = je.value("skybox_source", std::string{"hdri"});
        se2.timeOfDay    = je.value("time_of_day",   12.0f);
        se2.turbidity    = je.value("turbidity",     2.5f);
        se2.groundAlbedo = je.value("ground_albedo", glm::vec3(0.3f, 0.3f, 0.3f));
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
        st.halfExtents      = jtr.value("halfExtents", glm::vec3{1.0f});
        st.requiredTag      = jtr.value("required_tag", std::string{});
        st.triggersOnPlayer = jtr.value("triggers_on_player", true);
        st.oneShot          = jtr.value("one_shot", false);
        st.enabled          = jtr.value("enabled", true);
        se.trigger = std::move(st);
    }

    // F2H72: force_field. Aditivo — mapas viejos sin el campo cargan igual.
    if (j.contains("force_field")) {
        const auto& jff = j.at("force_field");
        SavedForceField sf;
        sf.type          = jff.value("mode",          std::string{"radial"});
        sf.shape         = jff.value("shape",         std::string{"sphere"});
        sf.halfExtents   = jff.value("halfExtents",   glm::vec3{2.0f});
        sf.radius        = jff.value("radius",        3.0f);
        sf.direction     = jff.value("direction",     glm::vec3{0.0f, 1.0f, 0.0f});
        sf.strength      = jff.value("strength",      20.0f);
        sf.linearFalloff = jff.value("linearFalloff", true);
        sf.ignoreMass    = jff.value("ignoreMass",    false);
        sf.enabled       = jff.value("enabled",       true);
        se.forceField = std::move(sf);
    }

    // F2H75: cloth. Aditivo — mapas viejos sin el campo cargan igual.
    if (j.contains("cloth")) {
        const auto& jcl = j.at("cloth");
        SavedCloth sc;
        sc.width      = jcl.value("width",      2.0f);
        sc.height     = jcl.value("height",     2.0f);
        sc.resX       = jcl.value("resX",       16);
        sc.resY       = jcl.value("resY",       16);
        sc.anchor     = jcl.value("anchor",     std::string{"top_edge"});
        sc.totalMass  = jcl.value("totalMass",  1.0f);
        sc.stiffness  = jcl.value("stiffness",  1.0f);
        sc.damping    = jcl.value("damping",    0.1f);
        sc.useGravity = jcl.value("useGravity", true);
        sc.color      = jcl.value("color",      glm::vec3{0.75f, 0.2f, 0.2f});
        se.cloth = std::move(sc);
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

    // F2H66: ragdoll. Aditivo — mapas pre-F2H66 sin el campo se leen igual.
    if (j.contains("ragdoll")) {
        const auto& jr = j.at("ragdoll");
        SavedRagdoll sr;
        sr.totalMass    = jr.value("totalMass",   70.0f);
        sr.limbRadius   = jr.value("limbRadius",  0.05f);
        sr.useGravity   = jr.value("useGravity",  true);
        sr.spawnImpulse = jr.value("spawnImpulse", glm::vec3{0.0f});
        se.ragdoll = std::move(sr);
    }

    // F2H67: vehicle + vehicle_seat. Aditivos -- mapas pre-F2H67 sin el
    // campo se leen igual.
    if (j.contains("vehicle")) {
        const auto& jv = j.at("vehicle");
        SavedVehicle sv;
        sv.configPath = jv.value("configPath", std::string{});
        se.vehicle = std::move(sv);
    }
    if (j.contains("vehicle_seat")) {
        const auto& js = j.at("vehicle_seat");
        SavedVehicleSeat ss;
        ss.seatOffsetLocal = js.value("seatOffsetLocal",
                                       glm::vec3{0.0f, 0.6f, 0.2f});
        se.vehicleSeat = std::move(ss);
    }

    // F3H11: audio_source + camera. Aditivos — mapas pre-F3H11 sin el
    // campo se leen igual (sin AudioSource/Camera component, back-compat).
    if (j.contains("audio_source")) {
        const auto& ja = j.at("audio_source");
        SavedAudio sa;
        sa.clipPath    = ja.value("clipPath",    std::string{});
        sa.volume      = ja.value("volume",      1.0f);
        sa.loop        = ja.value("loop",        false);
        sa.playOnStart = ja.value("playOnStart", true);
        sa.is3D        = ja.value("is3D",        false);
        se.audio = std::move(sa);
    }
    if (j.contains("camera")) {
        const auto& jc = j.at("camera");
        SavedCamera sc;
        sc.fovDeg    = jc.value("fovDeg",    60.0f);
        sc.nearPlane = jc.value("nearPlane",  0.1f);
        // F3H29: default 1000 (era 100). Mapas pre-F3H29 que persistieron
        // explícitamente farPlane=100 lo preservan; mapas que NO escribieron
        // el campo heredan el nuevo default 1000 (raro: el componente
        // serializa siempre el campo, así que en practica casi todos los
        // mapas tienen un valor explícito).
        sc.farPlane  = jc.value("farPlane",  1000.0f);
        se.camera = std::move(sc);
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
        sj.sliderLimitMin = jj.value("slider_limit_min", 0.0f);
        sj.sliderLimitMax = jj.value("slider_limit_max", 1.0f);
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
