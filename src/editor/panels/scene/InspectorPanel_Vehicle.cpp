// F2H67 Polish 3: Inspector — VehicleComponent.
// Edita el configPath del .moodvehicle + muestra estado runtime (vehicleId,
// wheel handles) en read-only. El input (throttle/brake/etc) NO se edita
// desde el Inspector — lo escribe el script Lua o el Player F2H67 cuando
// el player esta montado.
//
// Ediciones soportadas:
//   - configPath: InputText / drop target. Cambio marca dirty=true para que
//     el VehicleSystem destruya el vehiculo viejo y materialice uno nuevo
//     con el config nuevo en el proximo tick.
//   - "Reset" button: marca dirty=true (re-materializa).
//   - F3H12: live tuning del VehicleConfig (11 DragFloats + 2 friccion +
//     combo preset) — todos undoable. Live tuning edita el VehicleConfig
//     en memoria (asset compartido) — el undo aplica via setter que
//     captura `assets + configId` + flagea `veh.dirty = true`.

#include "editor/commands/Command.h"
#include "editor/commands/HistoryStack.h"
#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"  // F2H81: beginComponentSection
#include "editor/ui/DragDropFeedback.h"  // F3H17: halo overlay

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/assets/manager/AssetManager.h"      // F2H82: live tuning
#include "engine/physics/vehicle/VehicleConfig.h"    // F2H82: live tuning
#include "engine/physics/vehicle/VehiclePresets.h"   // F2H82: preset dropdown
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#include <array>
#include <functional>
#include <imgui.h>
#include <memory>
#include <utility>

#include <string>

namespace Mood {

namespace {

// F3H12: comando custom para edits batch del VehicleConfig — el preset
// aplica ~10 fields a la vez, un solo Ctrl+Z los revierte todos. Mismo
// patron que EditEnvironmentSubsetCommand. El VehicleConfig vive en el
// AssetManager (no en el componente); el cmd captura `assets + configId`
// para re-resolverlo en undo/redo. Tambien flagea `veh.dirty = true` para
// que el VehicleSystem rematerialice el vehiculo.
class EditVehicleConfigCommand : public ICommand {
public:
    using Apply = std::function<void(vehicle::VehicleConfig&)>;

    EditVehicleConfigCommand(Entity entity, AssetManager* assets,
                              VehicleConfigAssetId configId,
                              Apply applyBefore, Apply applyAfter,
                              std::string label)
        : m_entity(entity)
        , m_assets(assets)
        , m_configId(configId)
        , m_before(std::move(applyBefore))
        , m_after(std::move(applyAfter))
        , m_label(std::move(label)) {}

    void execute() override {
        if (m_assets == nullptr || !m_after) return;
        if (auto* cfg = m_assets->getMutableVehicleConfig(m_configId)) {
            m_after(*cfg);
        }
        markDirty();
    }

    void undo() override {
        if (m_assets == nullptr || !m_before) return;
        if (auto* cfg = m_assets->getMutableVehicleConfig(m_configId)) {
            m_before(*cfg);
        }
        markDirty();
    }

    std::string name() const override { return m_label; }

    void onEntityRemap(entt::entity oldH, entt::entity newH) override {
        if (m_entity.handle() == oldH) {
            m_entity = Entity(newH, m_entity.scene());
        }
    }

private:
    void markDirty() {
        if (m_entity.scene() == nullptr) return;
        if (!m_entity.scene()->registry().valid(m_entity.handle())) return;
        if (!m_entity.hasComponent<VehicleComponent>()) return;
        m_entity.getComponent<VehicleComponent>().dirty = true;
    }

    Entity                m_entity;
    AssetManager*         m_assets;
    VehicleConfigAssetId  m_configId;
    Apply                 m_before;
    Apply                 m_after;
    std::string           m_label;
};

// F2H82: aplica un VehiclePhysicsPreset (tabla de presets por clase) a una
// VehicleConfig en vivo, preservando la GEOMETRIA (attachLocal, radius, width,
// meshSubName, meshHubOffset) — solo cambia el FEEL fisico. Misma formula de
// brake torque que AssetManager_Vehicle.cpp: T = m * decel * r / 4.
void applyPresetToConfig(const vehicle::VehiclePhysicsPreset& p,
                         vehicle::VehicleConfig& cfg) {
    cfg.chassisMass           = p.massKg;
    cfg.chassisLinearDamping  = p.chassisLinearDamping;
    cfg.chassisAngularDamping = p.chassisAngularDamping;

    cfg.engine.maxTorque       = p.peakTorqueNm;
    cfg.engine.maxTorqueRPM    = p.peakTorqueRpm;
    cfg.engine.maxRPM          = p.redlineRpm;
    cfg.engine.minRPM          = p.idleRpm;
    cfg.engine.finalDriveRatio = p.finalDrive;

    cfg.maxSteerAngleDeg = p.maxSteerDeg;
    cfg.steerLerpSpeed   = p.steerLerp;

    const f32 wheelR = cfg.wheels[vehicle::WheelFL].radius;
    cfg.engine.brakeTorque =
        (cfg.chassisMass * p.decelTargetMps2 * wheelR) / 4.0f;
    cfg.engine.handbrakeTorque =
        cfg.engine.brakeTorque * (1.0f + p.handbrakeRatio * 2.0f);

    for (auto& w : cfg.wheels) {
        w.longitudinalFriction  = p.frictionLong;
        w.lateralFriction       = p.frictionLat;
        w.suspensionFrequency   = p.suspFrequencyHz;
        w.suspensionDamping     = p.suspDamping;
        w.suspensionMaxLength   = p.suspMaxLenMm / 1000.0f;
        w.suspensionMinLength   = p.suspMinLenMm / 1000.0f;
    }

    // Drivetrain: marcar driven en las ruedas del eje (o ambos para AWD).
    // Geometria (qual es FL/FR/RL/RR) se preserva.
    const bool frontDriven = (p.drivetrain == vehicle::Drivetrain::FWD ||
                              p.drivetrain == vehicle::Drivetrain::AWD);
    const bool rearDriven  = (p.drivetrain == vehicle::Drivetrain::RWD ||
                              p.drivetrain == vehicle::Drivetrain::AWD);
    cfg.wheels[vehicle::WheelFL].driven = frontDriven;
    cfg.wheels[vehicle::WheelFR].driven = frontDriven;
    cfg.wheels[vehicle::WheelRL].driven = rearDriven;
    cfg.wheels[vehicle::WheelRR].driven = rearDriven;
}

} // namespace

void InspectorPanel::renderVehicleSection(Entity e) {
    auto& veh = e.getComponent<VehicleComponent>();
    if (!beginComponentSection<VehicleComponent>(e, ICON_FA_GAUGE " Vehicle")) return;

    // configPath — InputText con buffer estatico-ish.
    // F3H12: undo via pushAtomicEdit<std::string> (EnterReturnsTrue dispara
    // 1 vez al commit del Enter — semantica atomica, no drag).
    char buf[256] = {0};
    std::snprintf(buf, sizeof(buf), "%s", veh.configPath.c_str());
    if (ImGui::InputText("config path##vehicle", buf, sizeof(buf),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string newPath(buf);
        if (newPath != veh.configPath) {
            detail::pushAtomicEdit<std::string>(m_ui, e, veh.configPath, newPath,
                [](Entity& en, const std::string& v) {
                    if (!en.hasComponent<VehicleComponent>()) return;
                    auto& vc = en.getComponent<VehicleComponent>();
                    vc.configPath = v;
                    vc.dirty = true;
                },
                "Editar vehicle configPath");
            m_editedThisFrame = true;
        }
    }
    // F2H70.3 Bloque F: drop target del Vehicle Browser. Un InputText no
    // funciona como BeginDragDropTarget (es un widget activo que consume el
    // drag), asi que usamos un boton dedicado como zona de drop — mismo patron
    // que InspectorPanel_Animation. Arrastrar un `.moodvehicle` asigna su path
    // al config + marca dirty para que el VehicleSystem rematerialice.
    ImGui::Button("Soltar .moodvehicle aqui##vehicle_drop", ImVec2(-1.0f, 28.0f));
    // F3H17: halo durante drag activo de vehicle.
    if (DragDropFeedback::isDragActiveOfType("MOOD_VEHICLE_ASSET")) {
        DragDropFeedback::drawItemDropHalo(ImGui::IsItemHovered());
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("MOOD_VEHICLE_ASSET")) {
            const char* dropped = static_cast<const char*>(payload->Data);
            const std::string newPath(dropped);
            if (!newPath.empty() && newPath != veh.configPath) {
                // F3H12: undo del drop.
                detail::pushAtomicEdit<std::string>(m_ui, e, veh.configPath, newPath,
                    [](Entity& en, const std::string& v) {
                        if (!en.hasComponent<VehicleComponent>()) return;
                        auto& vc = en.getComponent<VehicleComponent>();
                        vc.configPath = v;
                        vc.dirty = true;
                    },
                    "Drop vehicle configPath");
                m_editedThisFrame = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::TextDisabled(
        "%s",
        "Vacio = fallback generico (warn). Arrastra un .moodvehicle al boton "
        "de arriba o edita + Enter para rematerializar.");

    ImGui::Spacing();
    ImGui::TextDisabled("Runtime (read-only):");
    ImGui::Text("vehicleId: %u", veh.vehicleId);
    ImGui::Text("wheelEntities: [%u, %u, %u, %u]",
        veh.wheelEntities[0], veh.wheelEntities[1],
        veh.wheelEntities[2], veh.wheelEntities[3]);
    ImGui::Text("input (throttle/brake/steer/handbrake): %.2f / %.2f / %.2f / %.2f",
        veh.inputThrottle, veh.inputBrake,
        veh.inputSteer, veh.inputHandbrake);

    ImGui::Spacing();
    if (ImGui::Button("Rematerializar (dirty=true)##vehicle")) {
        veh.dirty = true;
        // Limpiamos handles para que el VehicleSystem destruya el viejo
        // y arme uno nuevo desde cero. VehicleId queda con el handle que
        // tenia (el system lo destruira y reseteara antes del create).
        m_editedThisFrame = true;
    }

    // F2H82: live tuning del VehicleConfig en memoria. Mutar + dirty=true
    // rematerializa el vehiculo en el siguiente tick con los valores nuevos.
    // Los cambios NO persisten al cerrar el editor — son para iterar feel en
    // vivo (re-importa o edita el .moodvehicle a mano para guardar).
    if (m_assets != nullptr && !veh.configPath.empty()) {
        const auto configId = m_assets->loadVehicleConfig(veh.configPath);
        vehicle::VehicleConfig* cfg = m_assets->getMutableVehicleConfig(configId);
        if (cfg != nullptr) {
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Live tuning (F2H82)")) {
                ImGui::TextDisabled("Cambios en memoria solo. Re-importa para persistir.");

                // Combo de preset: aplica un preset entero al config en vivo
                // (preserva geometria — solo cambia masa/motor/frenos/etc).
                constexpr std::array<const char*, 4> kClassLabels = {
                    "Deportivo", "Sedan", "Camioneta", "Blindado"
                };
                static int s_lastPresetApplied = -1;
                int presetIdx = s_lastPresetApplied;
                ImGui::PushItemWidth(-160.0f);
                if (ImGui::Combo("Aplicar preset##vt_preset", &presetIdx,
                                  kClassLabels.data(),
                                  static_cast<int>(kClassLabels.size()))) {
                    if (presetIdx >= 0 && presetIdx <
                            static_cast<int>(vehicle::VehicleClass::Count)) {
                        // F3H12: undo via EditVehicleConfigCommand —
                        // snapshot del cfg pre, apply preset al post, 1
                        // Ctrl+Z revierte todos los fields del preset.
                        const auto cls = static_cast<vehicle::VehicleClass>(presetIdx);
                        const auto preset = vehicle::presetFor(cls);
                        if (HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr) {
                            const vehicle::VehicleConfig cfgBefore = *cfg;
                            auto cmd = std::make_unique<EditVehicleConfigCommand>(
                                e, m_assets, configId,
                                [cfgBefore](vehicle::VehicleConfig& c) {
                                    c = cfgBefore;
                                },
                                [preset](vehicle::VehicleConfig& c) {
                                    applyPresetToConfig(preset, c);
                                },
                                "Aplicar vehicle preset");
                            h->push(std::move(cmd));
                        } else {
                            applyPresetToConfig(preset, *cfg);
                            veh.dirty = true;
                        }
                        s_lastPresetApplied = presetIdx;
                        m_editedThisFrame = true;
                    }
                }
                ImGui::PopItemWidth();
                ImGui::PushItemWidth(-160.0f);
                // F3H12: cada DragFloat con undo via pushEditIfDone<f32>.
                // Setter captura `m_assets + configId` y flagea `dirty=true`
                // — el VehicleConfig vive en el AssetManager, no en el
                // componente. Mismo patron que MeshRenderer con materials.
                AssetManager* assetsCap = m_assets;
                const VehicleConfigAssetId cfgIdCap = configId;
                auto makeSetter = [assetsCap, cfgIdCap](
                    std::function<void(vehicle::VehicleConfig&, const f32&)> fieldSet
                ) -> EditPropertyCommand<f32>::Setter {
                    return [assetsCap, cfgIdCap,
                            fieldSet = std::move(fieldSet)](Entity& en, const f32& v) {
                        if (!en.hasComponent<VehicleComponent>()) return;
                        if (auto* c = assetsCap->getMutableVehicleConfig(cfgIdCap)) {
                            fieldSet(*c, v);
                        }
                        en.getComponent<VehicleComponent>().dirty = true;
                    };
                };

                bool changed = false;
                changed |= ImGui::DragFloat("Masa (kg)##vt_mass",
                    &cfg->chassisMass, 5.0f, 100.0f, 30000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->chassisMass,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.chassisMass = v; }),
                    "Editar vehicle mass");

                changed |= ImGui::DragFloat("Torque pico (Nm)##vt_torque",
                    &cfg->engine.maxTorque, 5.0f, 50.0f, 3000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->engine.maxTorque,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.engine.maxTorque = v; }),
                    "Editar vehicle maxTorque");

                changed |= ImGui::DragFloat("RPM torque pico##vt_torque_rpm",
                    &cfg->engine.maxTorqueRPM, 50.0f, 1000.0f, 9000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->engine.maxTorqueRPM,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.engine.maxTorqueRPM = v; }),
                    "Editar vehicle maxTorqueRPM");

                changed |= ImGui::DragFloat("Redline RPM##vt_redline",
                    &cfg->engine.maxRPM, 50.0f, 3000.0f, 12000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->engine.maxRPM,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.engine.maxRPM = v; }),
                    "Editar vehicle maxRPM");

                changed |= ImGui::DragFloat("Torque freno (Nm)##vt_brake",
                    &cfg->engine.brakeTorque, 50.0f, 100.0f, 10000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->engine.brakeTorque,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.engine.brakeTorque = v; }),
                    "Editar vehicle brakeTorque");

                changed |= ImGui::DragFloat("Torque handbrake (Nm)##vt_hbrake",
                    &cfg->engine.handbrakeTorque, 50.0f, 100.0f, 20000.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->engine.handbrakeTorque,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.engine.handbrakeTorque = v; }),
                    "Editar vehicle handbrakeTorque");

                changed |= ImGui::DragFloat("Max steer (deg)##vt_steer",
                    &cfg->maxSteerAngleDeg, 1.0f, 5.0f, 60.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->maxSteerAngleDeg,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.maxSteerAngleDeg = v; }),
                    "Editar vehicle maxSteerAngleDeg");

                changed |= ImGui::DragFloat("Steer lerp##vt_steer_lerp",
                    &cfg->steerLerpSpeed, 0.1f, 1.0f, 20.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->steerLerpSpeed,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.steerLerpSpeed = v; }),
                    "Editar vehicle steerLerpSpeed");

                changed |= ImGui::DragFloat("Damping lineal##vt_dlin",
                    &cfg->chassisLinearDamping, 0.02f, 0.0f, 1.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->chassisLinearDamping,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.chassisLinearDamping = v; }),
                    "Editar vehicle linearDamping");

                changed |= ImGui::DragFloat("Damping angular##vt_dang",
                    &cfg->chassisAngularDamping, 0.02f, 0.0f, 1.0f);
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cfg->chassisAngularDamping,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) { c.chassisAngularDamping = v; }),
                    "Editar vehicle angularDamping");

                changed |= ImGui::DragFloat3("CoM local (m)##vt_com",
                    &cfg->centerOfMassLocal.x, 0.01f);
                {
                    auto comSetter = [assetsCap, cfgIdCap](Entity& en, const glm::vec3& v) {
                        if (!en.hasComponent<VehicleComponent>()) return;
                        if (auto* c = assetsCap->getMutableVehicleConfig(cfgIdCap)) {
                            c->centerOfMassLocal = v;
                        }
                        en.getComponent<VehicleComponent>().dirty = true;
                    };
                    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e,
                        cfg->centerOfMassLocal, std::move(comSetter),
                        "Editar vehicle CoM");
                }

                // Friccion: aplicada a las 4 ruedas a la vez (UX simple).
                // Si en el futuro hace falta per-eje, dividimos en F/R.
                f32 frictLong = cfg->wheels[0].longitudinalFriction;
                f32 frictLat  = cfg->wheels[0].lateralFriction;
                if (ImGui::DragFloat("Friccion long. (4 ruedas)##vt_flong",
                                      &frictLong, 0.05f, 0.5f, 3.0f)) {
                    for (auto& w : cfg->wheels) w.longitudinalFriction = frictLong;
                    changed = true;
                }
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, frictLong,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) {
                        for (auto& w : c.wheels) w.longitudinalFriction = v;
                    }),
                    "Editar vehicle longitudinalFriction");

                if (ImGui::DragFloat("Friccion lat. (4 ruedas)##vt_flat",
                                      &frictLat, 0.05f, 0.5f, 3.0f)) {
                    for (auto& w : cfg->wheels) w.lateralFriction = frictLat;
                    changed = true;
                }
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, frictLat,
                    makeSetter([](vehicle::VehicleConfig& c, const f32& v) {
                        for (auto& w : c.wheels) w.lateralFriction = v;
                    }),
                    "Editar vehicle lateralFriction");
                ImGui::PopItemWidth();

                if (changed) {
                    // Marca el vehiculo para rematerializar con la nueva config.
                    // (Necesario solo si NO hubo push de command — push.execute()
                    // ya marca dirty via setter. Idempotente.)
                    veh.dirty = true;
                    m_editedThisFrame = true;
                }
            }
        }
    }

    ImGui::Separator();
}

} // namespace Mood
