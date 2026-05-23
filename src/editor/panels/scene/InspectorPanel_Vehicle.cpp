// F2H67 Polish 3: Inspector — VehicleComponent.
// Edita el configPath del .moodvehicle + muestra estado runtime (vehicleId,
// wheel handles) en read-only. El input (throttle/brake/etc) NO se edita
// desde el Inspector — lo escribe el script Lua o el Player F2H67 cuando
// el player esta montado.
//
// Ediciones soportadas:
//   - configPath: InputText. Cambio marca dirty=true para que el
//     VehicleSystem destruya el vehiculo viejo y materialice uno nuevo
//     con el config nuevo en el proximo tick.
//   - "Reset" button: marca dirty=true (re-materializa).
//
// Inspector NO undoable v1 (mantengo footprint chico; agendable polish).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"  // F2H81: beginComponentSection

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/assets/manager/AssetManager.h"      // F2H82: live tuning
#include "engine/physics/vehicle/VehicleConfig.h"    // F2H82: live tuning
#include "engine/physics/vehicle/VehiclePresets.h"   // F2H82: preset dropdown
#include "engine/scene/components/Components.h"

#include <array>
#include <imgui.h>

#include <string>

namespace Mood {

namespace {

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
    char buf[256] = {0};
    std::snprintf(buf, sizeof(buf), "%s", veh.configPath.c_str());
    if (ImGui::InputText("config path##vehicle", buf, sizeof(buf),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string newPath(buf);
        if (newPath != veh.configPath) {
            veh.configPath = newPath;
            veh.dirty = true;
            m_editedThisFrame = true;
        }
    }
    // F2H70.3 Bloque F: drop target del Vehicle Browser. Un InputText no
    // funciona como BeginDragDropTarget (es un widget activo que consume el
    // drag), asi que usamos un boton dedicado como zona de drop — mismo patron
    // que InspectorPanel_Animation. Arrastrar un `.moodvehicle` asigna su path
    // al config + marca dirty para que el VehicleSystem rematerialice.
    ImGui::Button("Soltar .moodvehicle aqui##vehicle_drop", ImVec2(-1.0f, 28.0f));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("MOOD_VEHICLE_ASSET")) {
            const char* dropped = static_cast<const char*>(payload->Data);
            const std::string newPath(dropped);
            if (!newPath.empty() && newPath != veh.configPath) {
                veh.configPath = newPath;
                veh.dirty = true;
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
                        const auto cls = static_cast<vehicle::VehicleClass>(presetIdx);
                        applyPresetToConfig(vehicle::presetFor(cls), *cfg);
                        s_lastPresetApplied = presetIdx;
                        veh.dirty = true;
                        m_editedThisFrame = true;
                    }
                }
                ImGui::PopItemWidth();
                ImGui::PushItemWidth(-160.0f);
                bool changed = false;
                changed |= ImGui::DragFloat("Masa (kg)##vt_mass",
                    &cfg->chassisMass, 5.0f, 100.0f, 30000.0f);
                changed |= ImGui::DragFloat("Torque pico (Nm)##vt_torque",
                    &cfg->engine.maxTorque, 5.0f, 50.0f, 3000.0f);
                changed |= ImGui::DragFloat("RPM torque pico##vt_torque_rpm",
                    &cfg->engine.maxTorqueRPM, 50.0f, 1000.0f, 9000.0f);
                changed |= ImGui::DragFloat("Redline RPM##vt_redline",
                    &cfg->engine.maxRPM, 50.0f, 3000.0f, 12000.0f);
                changed |= ImGui::DragFloat("Torque freno (Nm)##vt_brake",
                    &cfg->engine.brakeTorque, 50.0f, 100.0f, 10000.0f);
                changed |= ImGui::DragFloat("Torque handbrake (Nm)##vt_hbrake",
                    &cfg->engine.handbrakeTorque, 50.0f, 100.0f, 20000.0f);
                changed |= ImGui::DragFloat("Max steer (deg)##vt_steer",
                    &cfg->maxSteerAngleDeg, 1.0f, 5.0f, 60.0f);
                changed |= ImGui::DragFloat("Steer lerp##vt_steer_lerp",
                    &cfg->steerLerpSpeed, 0.1f, 1.0f, 20.0f);
                changed |= ImGui::DragFloat("Damping lineal##vt_dlin",
                    &cfg->chassisLinearDamping, 0.02f, 0.0f, 1.0f);
                changed |= ImGui::DragFloat("Damping angular##vt_dang",
                    &cfg->chassisAngularDamping, 0.02f, 0.0f, 1.0f);
                changed |= ImGui::DragFloat3("CoM local (m)##vt_com",
                    &cfg->centerOfMassLocal.x, 0.01f);

                // Friccion: aplicada a las 4 ruedas a la vez (UX simple).
                // Si en el futuro hace falta per-eje, dividimos en F/R.
                f32 frictLong = cfg->wheels[0].longitudinalFriction;
                f32 frictLat  = cfg->wheels[0].lateralFriction;
                if (ImGui::DragFloat("Friccion long. (4 ruedas)##vt_flong",
                                      &frictLong, 0.05f, 0.5f, 3.0f)) {
                    for (auto& w : cfg->wheels) w.longitudinalFriction = frictLong;
                    changed = true;
                }
                if (ImGui::DragFloat("Friccion lat. (4 ruedas)##vt_flat",
                                      &frictLat, 0.05f, 0.5f, 3.0f)) {
                    for (auto& w : cfg->wheels) w.lateralFriction = frictLat;
                    changed = true;
                }
                ImGui::PopItemWidth();

                if (changed) {
                    // Marca el vehiculo para rematerializar con la nueva config.
                    veh.dirty = true;
                    m_editedThisFrame = true;
                }
            }
        }
    }

    ImGui::Separator();
}

} // namespace Mood
