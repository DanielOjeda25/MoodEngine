#include "engine/scene/serialization/EntitySerializer.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/VisGroup.h"  // F2H33: visgroupIdOf
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"  // F2H65: lookup target via entityFromHandle
#include "engine/scene/entity_type/EntityTypeTable.h"  // F3H9
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

// break-B3 (auditoria): la función `serializeEntityToJson` original era una
// pared de 467 LOC con un bloque por componente. La partimos en `writeXxx`
// estáticos por componente — un item del orden del doc, sin cambio de
// comportamiento (los bloques se relocaron tal cual). El orquestador
// expone el orden de serialización a simple vista.

void writeMeshRenderer(json& je, const MeshRendererComponent& mr,
                       const AssetManager& assets) {
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
    // F2H67: sub_mesh_name opcional (aditivo).
    if (!mr.subMeshName.empty()) {
        jmr["sub_mesh_name"] = mr.subMeshName;
    }
    je["mesh_renderer"] = jmr;
}

void writeLight(json& je, const LightComponent& lc) {
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

void writeRigidBody(json& je, const RigidBodyComponent& rb) {
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
    // F2H68: solo persistir isSensor cuando es true (campo opcional
    // aditivo). Default false en SavedRigidBody.
    if (rb.isSensor) jrb["is_sensor"] = true;
    je["rigid_body"] = jrb;
}

void writeEnvironment(json& je, const EnvironmentComponent& env) {
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
    // F3H31: sky procedural. Aditivo — solo persiste si difiere del default
    // (que es HDRI legacy + params neutros). Maps pre-F3H31 cargan como HDRI.
    if (env.skyboxSource == EnvironmentComponent::SkyboxSource::Procedural) {
        je2["skybox_source"] = "procedural";
    }
    if (env.timeOfDay != 12.0f)        je2["time_of_day"]   = env.timeOfDay;
    if (env.turbidity != 2.5f)         je2["turbidity"]     = env.turbidity;
    if (env.groundAlbedo != glm::vec3(0.3f, 0.3f, 0.3f)) {
        je2["ground_albedo"] = env.groundAlbedo;
    }
    je["environment"] = je2;
}

// Hito 29: ParticleEmitterComponent. Solo configuracion editable;
// estado runtime (positions/ages/rngState) NO se persiste — la
// simulacion arranca limpia al cargar.
void writeParticleEmitter(json& je, const ParticleEmitterComponent& em,
                          const AssetManager& assets) {
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
void writeScript(json& je, const ScriptComponent& sc) {
    if (sc.path.empty()) return;
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

// Hito 33: TriggerComponent. Solo halfExtents; el flag runtime
// playerInside no se persiste (lo redetecta el TriggerSystem al
// primer frame post-load).
void writeTrigger(json& je, const TriggerComponent& tc) {
    json jtr;
    jtr["halfExtents"] = tc.halfExtents;
    // F2H73: campos avanzados — solo si != default (JSON limpio +
    // backward-compat: mapas viejos no los traen y cargan igual).
    if (!tc.requiredTag.empty())  jtr["required_tag"]      = tc.requiredTag;
    if (!tc.triggersOnPlayer)     jtr["triggers_on_player"] = false;
    if (tc.oneShot)               jtr["one_shot"]          = true;
    if (!tc.enabled)              jtr["enabled"]           = false;
    je["trigger"] = jtr;
}

// F2H72: ForceFieldComponent. Estado runtime no hay — todos los campos
// se persisten. Enums como string para legibilidad/edicion a mano.
void writeForceField(json& je, const ForceFieldComponent& ff) {
    json jff;
    jff["shape"]         = (ff.shape == ForceFieldComponent::Shape::Sphere)
                               ? "sphere" : "box";
    jff["mode"]          = (ff.mode == ForceFieldComponent::Mode::Radial)
                               ? "radial" : "directional";
    jff["halfExtents"]   = ff.halfExtents;
    jff["radius"]        = ff.radius;
    jff["direction"]     = ff.direction;
    jff["strength"]      = ff.strength;
    jff["linearFalloff"] = ff.linearFalloff;
    jff["ignoreMass"]    = ff.ignoreMass;
    jff["enabled"]       = ff.enabled;
    je["force_field"] = jff;
}

// F2H75: ClothComponent. Solo params (sin estado runtime: clothId,
// dynamicMeshId, dirty los rematerializa el ClothSystem). Anchor como
// string para edicion a mano.
void writeCloth(json& je, const ClothComponent& cl) {
    json jcl;
    const char* anchorStr = "top_edge";
    switch (cl.anchor) {
        case ClothComponent::Anchor::None:       anchorStr = "none"; break;
        case ClothComponent::Anchor::TopEdge:    anchorStr = "top_edge"; break;
        case ClothComponent::Anchor::TopCorners: anchorStr = "top_corners"; break;
        case ClothComponent::Anchor::LeftEdge:   anchorStr = "left_edge"; break;
    }
    jcl["width"]      = cl.width;
    jcl["height"]     = cl.height;
    jcl["resX"]       = cl.resX;
    jcl["resY"]       = cl.resY;
    jcl["anchor"]     = anchorStr;
    jcl["totalMass"]  = cl.totalMass;
    jcl["stiffness"]  = cl.stiffness;
    jcl["damping"]    = cl.damping;
    jcl["useGravity"] = cl.useGravity;
    jcl["color"]      = cl.color;
    je["cloth"] = jcl;
}

// F2H48.1: DialogComponent. Solo dialogPath + autoStartOnInteract;
// el cachedDialogId runtime no se persiste (loadDialog al primer
// tick del DialogInteractSystem lo repuebla via VFS).
void writeDialog(json& je, const DialogComponent& dc) {
    if (dc.dialogPath.empty()) return;
    json jd;
    jd["path"]              = dc.dialogPath;
    jd["autoStartOnInteract"] = dc.autoStartOnInteract;
    je["dialog"] = jd;
}

// F2H52: ItemPickupComponent. itemPath + quantity + destroyOnPickup;
// cachedItemId runtime no se persiste (loadItem al primer trigger
// del ItemPickupSystem lo repuebla via VFS). Convencion identica a
// DialogComponent: si itemPath esta vacio NO se serializa (no tiene
// sentido un pickup sin target).
void writeItemPickup(json& je, const ItemPickupComponent& ip) {
    if (ip.itemPath.empty()) return;
    json ji;
    ji["path"]            = ip.itemPath;
    ji["quantity"]        = ip.quantity;
    ji["destroyOnPickup"] = ip.destroyOnPickup;
    je["item_pickup"] = ji;
}

// F2H50 Bloque D: AnimatorComponent. clipName/speed/playing/loop +
// lista `externalClips` (alias + path logico del .fbx standalone).
// El `externalBindCache` runtime no se persiste — el AnimationSystem
// lo re-genera al primer evaluate via `bindClipToSkeleton`. El `time`
// tampoco se persiste (siempre arranca en 0 al cargar — comportamiento
// estandar de "respawn" en motores: la anim empieza desde el frame 1
// cuando la escena se restaura).
void writeAnimator(json& je, const AnimatorComponent& a,
                   const AssetManager& assets) {
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
void writeInventory(json& je, const InventoryComponent& inv,
                    const AssetManager& assets) {
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
void writeJoint(json& je, const JointComponent& jc, Entity entity) {
    json jj;
    const char* typeStr = "hinge";
    switch (jc.type) {
        case JointComponent::Type::Hinge:    typeStr = "hinge";    break;
        case JointComponent::Type::Distance: typeStr = "distance"; break;
        case JointComponent::Type::Point:    typeStr = "point";    break;
        case JointComponent::Type::Slider:   typeStr = "slider";   break;
        case JointComponent::Type::Fixed:    typeStr = "fixed";    break;
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
    } else if (jc.type == JointComponent::Type::Slider) {
        if (jc.axisLocal != glm::vec3(0.0f, 1.0f, 0.0f)) {
            jj["axisLocal"] = jc.axisLocal;
        }
        jj["slider_limit_min"] = jc.sliderLimitMin;
        jj["slider_limit_max"] = jc.sliderLimitMax;
    }
    // Point / Fixed: no extra fields.
    je["joint"] = jj;
}

// F2H66: RagdollComponent. Solo los tweaks de config; state arranca
// Animated al cargar (NPCs muertos persistidos NO es scope v1).
void writeRagdoll(json& je, const RagdollComponent& rag) {
    json jr;
    jr["totalMass"]   = rag.totalMass;
    jr["limbRadius"]  = rag.limbRadius;
    if (!rag.useGravity) jr["useGravity"] = false;  // default ON, aditivo
    if (glm::length(rag.spawnImpulse) > 1e-4f) {
        jr["spawnImpulse"] = rag.spawnImpulse;
    }
    je["ragdoll"] = jr;
}

// F2H67: VehicleComponent. Solo el path al .moodvehicle; input state +
// handles runtime no se persisten (re-materializa al cargar).
void writeVehicle(json& je, const VehicleComponent& veh) {
    json jv;
    jv["configPath"] = veh.configPath;
    je["vehicle"] = jv;
}

void writeVehicleSeat(json& je, const VehicleSeatComponent& seat) {
    json js;
    js["seatOffsetLocal"] = seat.seatOffsetLocal;
    je["vehicle_seat"] = js;
}

// F3H11: persistir AudioSourceComponent. El runtime `clip` (AudioAssetId)
// se traduce a `clipPath` via assets; los flags persisten as-is. Estado
// runtime (handle, started) NO se escribe.
void writeAudio(json& je, const AudioSourceComponent& a,
                  const AssetManager& assets) {
    json ja;
    ja["clipPath"]    = (a.clip == 0u) ? std::string{} : assets.audioPathOf(a.clip);
    ja["volume"]      = a.volume;
    ja["loop"]        = a.loop;
    ja["playOnStart"] = a.playOnStart;
    ja["is3D"]        = a.is3D;
    je["audio_source"] = ja;
}

// F3H11: persistir CameraComponent (3 fields scalar).
void writeCamera(json& je, const CameraComponent& c) {
    json jc;
    jc["fovDeg"]    = c.fovDeg;
    jc["nearPlane"] = c.nearPlane;
    jc["farPlane"]  = c.farPlane;
    je["camera"] = jc;
}

// F4H1: persistir HealthComponent. lastDamageTime/hitFlashTimer son
// transients runtime (no se guardan).
void writeHealth(json& je, const HealthComponent& h) {
    json jh;
    jh["current"] = h.current;
    jh["max"]     = h.max;
    jh["dead"]    = h.dead;
    je["health"] = jh;
}

// F4H2: persistir WeaponComponent. weaponPath se reconstruye desde
// `weaponAssetId` via `AssetManager::weaponPathOf`. Si el path es
// sentinela vacio ("__empty_weapon") no se persiste — equivale a
// "sin arma". Timers son transients.
void writeWeapon(json& je, const WeaponComponent& w,
                  const AssetManager& assets) {
    if (w.weaponAssetId == 0) return; // sin arma equipada
    const std::string path = assets.weaponPathOf(w.weaponAssetId);
    if (path.empty() || path == "__empty_weapon") return;
    json jw;
    jw["path"]        = path;
    jw["currentAmmo"] = w.currentAmmo;
    je["weapon"] = jw;
}

// Link suave al prefab (Hito 14 Bloque 6). Solo se persiste si la
// entidad tiene un `PrefabLinkComponent`. Sin propagacion bidireccional
// por ahora; es solo un breadcrumb para futuras features ("revertir a
// prefab", "aplicar overrides", etc.).
void writePrefabLink(json& je, const PrefabLinkComponent& link) {
    if (!link.path.empty()) {
        je["prefab_path"] = link.path;
    }
}

} // namespace

json serializeEntityToJson(Entity entity, const AssetManager& assets) {
    json je;
    const auto& tag = entity.getComponent<TagComponent>();
    je["tag"] = tag.name;
    // F3H9: persistir el entityType si != Generic (defaults se skipean
    // para mantener .moodmap limpio, mismo patron que F3H4-F3H7).
    if (tag.entityType != EntityType::Generic) {
        je["entity_type"] = EntityTypeTable::toString(tag.entityType);
    }

    const auto& t = entity.getComponent<TransformComponent>();
    json jt;
    jt["position"]      = t.position;
    jt["rotationEuler"] = t.rotationEuler;
    jt["scale"]         = t.scale;
    je["transform"] = jt;
    // F3H27: serializar `parent_tag` solo si la entity tiene padre. El
    // tag del padre es resistente a remap de handles (vs serializar
    // raw entt handle que cambia entre saves). Resolucion en 2-pass en
    // SceneLoader::applyEntities.
    if (t.parent != entt::null && entity.scene() != nullptr) {
        Entity parentE(t.parent, entity.scene());
        if (parentE && parentE.hasComponent<TagComponent>()) {
            je["parent_tag"] = parentE.getComponent<TagComponent>().name;
        }
    }

    if (entity.hasComponent<MeshRendererComponent>())
        writeMeshRenderer(je, entity.getComponent<MeshRendererComponent>(), assets);
    if (entity.hasComponent<LightComponent>())
        writeLight(je, entity.getComponent<LightComponent>());
    if (entity.hasComponent<RigidBodyComponent>())
        writeRigidBody(je, entity.getComponent<RigidBodyComponent>());
    if (entity.hasComponent<EnvironmentComponent>())
        writeEnvironment(je, entity.getComponent<EnvironmentComponent>());
    if (entity.hasComponent<ParticleEmitterComponent>())
        writeParticleEmitter(je, entity.getComponent<ParticleEmitterComponent>(), assets);
    if (entity.hasComponent<ScriptComponent>())
        writeScript(je, entity.getComponent<ScriptComponent>());
    if (entity.hasComponent<TriggerComponent>())
        writeTrigger(je, entity.getComponent<TriggerComponent>());
    if (entity.hasComponent<ForceFieldComponent>())
        writeForceField(je, entity.getComponent<ForceFieldComponent>());
    if (entity.hasComponent<ClothComponent>())
        writeCloth(je, entity.getComponent<ClothComponent>());
    if (entity.hasComponent<DialogComponent>())
        writeDialog(je, entity.getComponent<DialogComponent>());
    if (entity.hasComponent<ItemPickupComponent>())
        writeItemPickup(je, entity.getComponent<ItemPickupComponent>());
    if (entity.hasComponent<AnimatorComponent>())
        writeAnimator(je, entity.getComponent<AnimatorComponent>(), assets);
    if (entity.hasComponent<InventoryComponent>())
        writeInventory(je, entity.getComponent<InventoryComponent>(), assets);
    if (entity.hasComponent<JointComponent>())
        writeJoint(je, entity.getComponent<JointComponent>(), entity);
    if (entity.hasComponent<RagdollComponent>())
        writeRagdoll(je, entity.getComponent<RagdollComponent>());
    if (entity.hasComponent<VehicleComponent>())
        writeVehicle(je, entity.getComponent<VehicleComponent>());
    if (entity.hasComponent<VehicleSeatComponent>())
        writeVehicleSeat(je, entity.getComponent<VehicleSeatComponent>());
    // F3H11: Audio + Camera (gap F2 cerrado).
    if (entity.hasComponent<AudioSourceComponent>())
        writeAudio(je, entity.getComponent<AudioSourceComponent>(), assets);
    if (entity.hasComponent<CameraComponent>())
        writeCamera(je, entity.getComponent<CameraComponent>());
    if (entity.hasComponent<HealthComponent>())           // F4H1
        writeHealth(je, entity.getComponent<HealthComponent>());
    if (entity.hasComponent<WeaponComponent>())           // F4H2
        writeWeapon(je, entity.getComponent<WeaponComponent>(), assets);
    if (entity.hasComponent<PrefabLinkComponent>())
        writePrefabLink(je, entity.getComponent<PrefabLinkComponent>());

    // F2H33 (v14): visgroupId opcional. Solo se emite si la entity tiene
    // componente con groupId != 0.
    const u64 vgId = visgroupIdOf(entity);
    if (vgId != 0) {
        je["visgroupId"] = vgId;
    }
    return je;
}

} // namespace Mood
