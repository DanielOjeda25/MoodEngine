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

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>

namespace Mood {

void InspectorPanel::renderVehicleSection(Entity e) {
    auto& veh = e.getComponent<VehicleComponent>();
    ImGui::SeparatorText("Vehicle");

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
    ImGui::TextDisabled(
        "%s",
        "Vacio = makeDefaultSA(). Cambio + Enter rematerializa el vehicle.");

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

    ImGui::Separator();
}

} // namespace Mood
