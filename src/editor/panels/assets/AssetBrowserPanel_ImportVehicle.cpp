// F2H82 Bloque D: modal "Importar vehiculo".
//
// Flujo:
//   1. Boton "+ Importar" en el tab Vehiculos -> openImportVehicleModal()
//      abre el file picker (.glb/.fbx).
//   2. Si el dev elige un archivo, corremos el analyzer (Bloque A) y abrimos
//      el modal con los resultados precargados.
//   3. El modal expone: nombre editable, clase (Deportivo/Sedan/Camioneta/
//      Blindado) que precarga el preset (Bloque C), y un form de ajuste fino
//      de los campos del preset (masa, motor, frenos, suspension, etc.).
//   4. "Guardar" copia el .glb a `assets/vehicles/<nombre>/` y escribe el
//      `.moodvehicle` con el writer (Bloque C). Despues hace rescan() para
//      que el browser lo levante.
//
// Preview 3D in-modal queda diferido: el writer aun no copio la malla a
// assets/ asi que el thumbnail renderer no la puede cargar. Tras Guardar
// el vehiculo aparece con su miniatura en el grid del browser.

#include "editor/panels/assets/AssetBrowserPanel.h"

#include "core/Log.h"
#include "core/i18n/I18n.h"  // F3H29: keys editor.import_vehicle.*
#include "engine/physics/vehicle/VehicleConfig.h"        // WheelCount, WheelFL...
#include "engine/physics/vehicle/VehicleConfigWriter.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace Mood {

namespace {

namespace fs = std::filesystem;

// F2H82 polish: inyecta una escala en el nodo raíz del .glb (chunk JSON) para
// que las vertices nazcan en metros reales al loadearlo. Resuelve el problema
// de modelos Sketchfab/Maya exportados en cm/mm: las dimensiones del AABB del
// analisis ya quedan correctas, el render no necesita ningun runtime scale,
// y el .moodvehicle no necesita el campo `mesh_scale`.
//
// GLB format (v2): header(12) + chunk0(JSON, 4-byte aligned padding spaces) +
// chunk1(BIN). Solo modificamos chunk0; el chunk1 (vertex data / textures) se
// preserva tal cual.
bool injectGlbRootScale(const std::filesystem::path& glbPath,
                         float scale, std::string& err) {
    namespace fs = std::filesystem;
    using nlohmann::json;

    std::ifstream in(glbPath, std::ios::binary);
    if (!in) { err = "no se pudo abrir el .glb"; return false; }
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    in.close();
    if (bytes.size() < 28) { err = ".glb demasiado chico"; return false; }

    // Header: magic + version + total length.
    std::uint32_t magic, version, totalLen;
    std::memcpy(&magic, bytes.data() + 0, 4);
    std::memcpy(&version, bytes.data() + 4, 4);
    std::memcpy(&totalLen, bytes.data() + 8, 4);
    if (magic != 0x46546C67u) { err = "no es un GLB (magic invalido)"; return false; }
    if (version != 2u) { err = "GLB v" + std::to_string(version) + " no soportado"; return false; }

    // Chunk 0 (JSON).
    std::uint32_t c0Len, c0Type;
    std::memcpy(&c0Len, bytes.data() + 12, 4);
    std::memcpy(&c0Type, bytes.data() + 16, 4);
    if (c0Type != 0x4E4F534Au) { err = "chunk 0 no es JSON"; return false; }
    if (20 + static_cast<std::size_t>(c0Len) > bytes.size()) {
        err = "JSON chunk overflow"; return false;
    }
    const std::string jsonStr(
        reinterpret_cast<const char*>(bytes.data() + 20),
        static_cast<std::size_t>(c0Len));

    json j;
    try { j = json::parse(jsonStr); }
    catch (const std::exception& e) {
        err = std::string("JSON parse: ") + e.what();
        return false;
    }

    // Localizar el nodo raiz de la escena por default.
    int sceneIdx = j.value("scene", 0);
    if (!j.contains("scenes") || !j["scenes"].is_array() ||
        sceneIdx < 0 || sceneIdx >= static_cast<int>(j["scenes"].size())) {
        err = "GLB sin scene principal"; return false;
    }
    auto& scn = j["scenes"][sceneIdx];
    if (!scn.contains("nodes") || !scn["nodes"].is_array() || scn["nodes"].empty()) {
        err = "scene principal sin nodes"; return false;
    }
    int rootIdx = scn["nodes"][0].get<int>();
    if (!j.contains("nodes") || rootIdx < 0 ||
        rootIdx >= static_cast<int>(j["nodes"].size())) {
        err = "root node fuera de rango"; return false;
    }
    auto& root = j["nodes"][rootIdx];

    // Inyectar la escala en el nodo raiz. Caso A: el nodo trae una `matrix`
    // 4x4 (16 floats column-major, comun en exports tipo Sketchfab/FBX→GLB).
    // Pre-multiplicamos por diag(s,s,s,1): los elementos correspondientes a
    // las filas 0..2 (basis axes + translation) se escalan; la fila 3 (0 0 0 1)
    // queda igual. En column-major los indices afectados son 0,1,2 / 4,5,6 /
    // 8,9,10 / 12,13,14; salteamos 3,7,11,15. Caso B: trae `scale` array (TRS
    // separado, glTF puro): multiplicamos por el factor. Caso C: ningun
    // transform — agregamos `scale` con el factor.
    if (root.contains("matrix") && root["matrix"].is_array() &&
        root["matrix"].size() == 16) {
        for (int i = 0; i < 16; ++i) {
            if (i == 3 || i == 7 || i == 11 || i == 15) continue;
            const float v = root["matrix"][i].get<float>() * scale;
            root["matrix"][i] = v;
        }
    } else {
        json oldScale = root.value("scale", json::array({1.0f, 1.0f, 1.0f}));
        const float sx = oldScale[0].get<float>() * scale;
        const float sy = oldScale[1].get<float>() * scale;
        const float sz = oldScale[2].get<float>() * scale;
        root["scale"] = json::array({sx, sy, sz});
    }

    // Re-serializar JSON. Padding con espacios para 4-byte alignment (spec).
    std::string newJson = j.dump();
    while ((newJson.size() % 4) != 0) newJson += ' ';
    const std::uint32_t newC0Len = static_cast<std::uint32_t>(newJson.size());

    // Chunk 1 (BIN, opcional): copia desde el offset post chunk0.
    const std::size_t binStart = 20 + static_cast<std::size_t>(c0Len);
    std::vector<std::uint8_t> binChunk;
    if (binStart < bytes.size()) {
        binChunk.assign(bytes.begin() + binStart, bytes.end());
    }
    const std::uint32_t newTotal = 12u + 8u + newC0Len +
        static_cast<std::uint32_t>(binChunk.size());

    std::ofstream out(glbPath, std::ios::binary | std::ios::trunc);
    if (!out) { err = "no se pudo abrir el .glb para escribir"; return false; }
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&newTotal), 4);
    out.write(reinterpret_cast<const char*>(&newC0Len), 4);
    out.write(reinterpret_cast<const char*>(&c0Type), 4);
    out.write(newJson.data(), static_cast<std::streamsize>(newJson.size()));
    if (!binChunk.empty()) {
        out.write(reinterpret_cast<const char*>(binChunk.data()),
                   static_cast<std::streamsize>(binChunk.size()));
    }
    return out.good();
}

// Sanitiza un display name a un slug usable como nombre de carpeta/archivo:
// minusculas, espacios -> '_', solo [a-z0-9_-].
std::string sanitizeSlug(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out.push_back(c);
        } else if (c == ' ' || c == '\t') {
            out.push_back('_');
        }
        // resto se descarta (acentos, simbolos)
    }
    if (out.empty()) out = "vehiculo";
    return out;
}

} // namespace

// ============================================================================
// openImportVehicleModal — file picker + analyzer
// ============================================================================
void AssetBrowserPanel::openImportVehicleModal() {
    // pfd::open_file es bloqueante (modal nativo); aceptable para una accion
    // explicita del dev.
    const auto picked = pfd::open_file(
        "Importar vehiculo (.glb / .fbx)",
        "",
        { "Modelos 3D", "*.glb *.fbx", "Todos", "*" }).result();
    if (picked.empty()) {
        return;  // cancelado
    }

    m_importFsPath = picked[0];
    m_importAnalysis = vehicle::analyzeVehicleMesh(m_importFsPath);

    // Default display name = stem del archivo.
    m_importDisplayName = fs::path(m_importFsPath).stem().generic_string();

    // Preset inicial = Sedan.
    m_importClass  = vehicle::VehicleClass::Sedan;
    m_importPreset = vehicle::presetFor(m_importClass);

    // F2H82 polish: auto-sugerir un mesh_scale si las dimensiones detectadas
    // son sospechosas. Un auto razonable mide entre 0.5 y 30 m de largo.
    m_importMeshScale = 1.0f;
    if (m_importAnalysis.ok) {
        const f32 lengthZ = m_importAnalysis.overallAabbMax.z -
                            m_importAnalysis.overallAabbMin.z;
        if (lengthZ > 0.0f && lengthZ < 0.5f) {
            // Probablemente esta en cm (x100) o mm (x1000); elegimos el que
            // deja el largo en el rango razonable (1.5 - 10 m).
            if (lengthZ * 100.0f >= 1.5f && lengthZ * 100.0f <= 10.0f) {
                m_importMeshScale = 100.0f;
            } else if (lengthZ * 1000.0f >= 1.5f && lengthZ * 1000.0f <= 10.0f) {
                m_importMeshScale = 1000.0f;
            }
        }
    }

    m_importSaveError.clear();
    m_importModalOpen = true;

    if (!m_importAnalysis.ok) {
        Log::editor()->warn(
            "ImportVehicle: analyze fallo para '{}': {}",
            m_importFsPath, m_importAnalysis.error);
    } else {
        Log::editor()->info(
            "ImportVehicle: '{}' analizado — {} ruedas detectadas "
            "(by name={}), wheelbase={:.2f}m, track F/R={:.2f}/{:.2f}m",
            m_importFsPath, m_importAnalysis.wheelsFound,
            m_importAnalysis.wheelsByName ? "si" : "no",
            m_importAnalysis.wheelbase,
            m_importAnalysis.trackFront, m_importAnalysis.trackRear);
    }
}

// ============================================================================
// drawImportVehicleModal — UI del modal
// ============================================================================
void AssetBrowserPanel::drawImportVehicleModal() {
    if (!m_importModalOpen) return;

    constexpr const char* kPopupId = "###import_vehicle_modal";
    const std::string title = std::string("Importar vehiculo") + kPopupId;
    if (!ImGui::IsPopupOpen(kPopupId)) {
        ImGui::OpenPopup(kPopupId);
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(580.0f, 640.0f), ImGuiCond_Appearing);

    // F2H82 polish: NoResize (es un dialogo, no una ventana de trabajo) +
    // footer fijo con Guardar/Cancelar abajo de un child scrollable.
    if (!ImGui::BeginPopupModal(title.c_str(), &m_importModalOpen,
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize)) {
        if (!m_importModalOpen) {
            ImGui::CloseCurrentPopup();
        }
        return;
    }

    // --- Cabecera: archivo + analisis (siempre visible) ---
    ImGui::TextDisabled("%s", m_importFsPath.c_str());
    ImGui::Separator();

    if (!m_importAnalysis.ok) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s",
            I18n::T("editor.import_vehicle.error_analyzing",
                     m_importAnalysis.error).c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Button(I18n::T("editor.modal.common.close").c_str(),
                           ImVec2(120.0f, 0.0f))) {
            m_importModalOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    // Reserva espacio para el footer fijo (botones siempre visibles).
    const float footerH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    ImGui::BeginChild("##importveh_scroll",
                       ImVec2(0.0f, -footerH), false);

    // --- Resumen del analisis ---
    if (ImGui::CollapsingHeader("Analisis del modelo",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        const glm::vec3 dimRaw = m_importAnalysis.overallAabbMax -
                                  m_importAnalysis.overallAabbMin;
        const glm::vec3 dim = dimRaw * m_importMeshScale;

        // F2H82 polish: escala del mesh + warning si dims iniciales eran chicas.
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "Escala del modelo:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("##importveh_scale", &m_importMeshScale, 0.5f,
                          0.001f, 10000.0f, "%.2fx");
        ImGui::SameLine();
        if (ImGui::SmallButton("x1")) m_importMeshScale = 1.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("x100")) m_importMeshScale = 100.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("x1000")) m_importMeshScale = 1000.0f;

        if (dimRaw.z > 0.0f && dimRaw.z < 0.5f) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                "Modelo muy chico (%.3f m). Probablemente esta en cm o mm — usar x100 / x1000.",
                dimRaw.z);
        } else if (dimRaw.z > 30.0f) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                "Modelo muy grande (%.1f m). Probablemente al reves — bajar la escala.",
                dimRaw.z);
        }

        ImGui::BulletText("Dimensiones: %.2f x %.2f x %.2f m  (post-escala)",
            dim.x, dim.y, dim.z);
        ImGui::BulletText("Ruedas detectadas: %d/4 (%s)",
            m_importAnalysis.wheelsFound,
            m_importAnalysis.wheelsByName ? "por nombre" : "por geometria");

        // F2H82 polish: detalle de las 4 ruedas (nombre del nodo + posicion)
        // para que el dev VEA que detecto el analyzer. Si esta mal, los botones
        // de abajo dejan corregir sin re-importar.
        constexpr std::array<const char*, 4> kRoleLabels = {"FL", "FR", "RL", "RR"};
        ImGui::Indent();
        for (int i = 0; i < vehicle::WheelCount; ++i) {
            const auto& w = m_importAnalysis.wheels[i];
            if (w.role == vehicle::WheelRole::Unknown) {
                ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                    "  %s: (no detectada)", kRoleLabels[i]);
            } else {
                ImGui::Text("  %s: %s   (X=%.2f, Z=%.2f, r=%.2fm)",
                    kRoleLabels[i], w.part.nodeName.c_str(),
                    w.part.center.x * m_importMeshScale,
                    w.part.center.z * m_importMeshScale,
                    w.radius * m_importMeshScale);
            }
        }
        ImGui::Unindent();

        // Botones de correccion (cuando el modelo viene con conv. invertida).
        if (m_importAnalysis.wheelsFound > 0) {
            ImGui::Spacing();
            if (ImGui::SmallButton("Invertir adelante/atras")) {
                std::swap(m_importAnalysis.wheels[vehicle::WheelFL],
                          m_importAnalysis.wheels[vehicle::WheelRL]);
                std::swap(m_importAnalysis.wheels[vehicle::WheelFR],
                          m_importAnalysis.wheels[vehicle::WheelRR]);
                // Re-asignar roles para que coincidan con el nuevo indice.
                m_importAnalysis.wheels[vehicle::WheelFL].role = vehicle::WheelRole::FL;
                m_importAnalysis.wheels[vehicle::WheelFR].role = vehicle::WheelRole::FR;
                m_importAnalysis.wheels[vehicle::WheelRL].role = vehicle::WheelRole::RL;
                m_importAnalysis.wheels[vehicle::WheelRR].role = vehicle::WheelRole::RR;
                // Recalcular wheelbase con signo invertido (Z swap).
                m_importAnalysis.wheelbase = -m_importAnalysis.wheelbase;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Invertir izq/der")) {
                std::swap(m_importAnalysis.wheels[vehicle::WheelFL],
                          m_importAnalysis.wheels[vehicle::WheelFR]);
                std::swap(m_importAnalysis.wheels[vehicle::WheelRL],
                          m_importAnalysis.wheels[vehicle::WheelRR]);
                m_importAnalysis.wheels[vehicle::WheelFL].role = vehicle::WheelRole::FL;
                m_importAnalysis.wheels[vehicle::WheelFR].role = vehicle::WheelRole::FR;
                m_importAnalysis.wheels[vehicle::WheelRL].role = vehicle::WheelRole::RL;
                m_importAnalysis.wheels[vehicle::WheelRR].role = vehicle::WheelRole::RR;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                    I18n::T("editor.import_vehicle.swap_tooltip").c_str());
            }
        }

        ImGui::Spacing();
        ImGui::BulletText("Wheelbase: %.2f m  |  Track F/R: %.2f / %.2f m",
            m_importAnalysis.wheelbase,
            m_importAnalysis.trackFront, m_importAnalysis.trackRear);
        ImGui::BulletText("Radio rueda promedio: %.2f m", m_importAnalysis.wheelRadius);
        if (m_importAnalysis.wheelsFound < 4) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                "%s",
                I18n::T("editor.import_vehicle.aviso_wheels").c_str());
        }
    }

    // --- Identidad: nombre + clase ---
    if (ImGui::CollapsingHeader("Identidad",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Como se llama y de que tipo es.");
        char buf[128] = {0};
        std::snprintf(buf, sizeof(buf), "%s", m_importDisplayName.c_str());
        ImGui::PushItemWidth(-150.0f);
        if (ImGui::InputText("Nombre##importveh_name", buf, sizeof(buf))) {
            m_importDisplayName = buf;
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Carpeta destino: assets/vehicles/%s/",
            sanitizeSlug(m_importDisplayName).c_str());

        constexpr std::array<const char*, 4> kClassLabels = {
            "Deportivo", "Sedan", "Camioneta", "Blindado"
        };
        int classIdx = static_cast<int>(m_importClass);
        ImGui::PushItemWidth(-150.0f);
        if (ImGui::Combo("Clase (preset)##importveh_class", &classIdx,
                          kClassLabels.data(),
                          static_cast<int>(kClassLabels.size()))) {
            m_importClass  = static_cast<vehicle::VehicleClass>(classIdx);
            m_importPreset = vehicle::presetFor(m_importClass);
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Cambiar la clase reinicia el resto de los campos al preset.");
    }

    // Helper macro local: drag con tooltip explicativo. Asi el dev sabe que
    // edita aunque el label sea corto.
    auto dragWithTip = [](const char* label, f32* v, f32 step, f32 lo, f32 hi,
                          const char* tip) {
        ImGui::DragFloat(label, v, step, lo, hi);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    };

    ImGui::PushItemWidth(-180.0f);

    // --- Motor ---
    if (ImGui::CollapsingHeader("Motor", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Cuanta fuerza puede entregar a las ruedas.");
        dragWithTip("Masa (kg)##m_mass",
            &m_importPreset.massKg, 5.0f, 100.0f, 30000.0f,
            "Peso total del vehiculo. Mas pesado = inercia, menos volcamientos, pero acelera y frena mas lento.");
        dragWithTip("Torque pico (Nm)##m_tq",
            &m_importPreset.peakTorqueNm, 5.0f, 50.0f, 3000.0f,
            "Fuerza maxima que entrega el motor. Mas torque = arranca y sube cuestas con menos esfuerzo.");
        dragWithTip("RPM en torque pico##m_tqrpm",
            &m_importPreset.peakTorqueRpm, 50.0f, 1000.0f, 9000.0f,
            "RPM donde el motor entrega el torque maximo. Bajo = diesel; alto = deportivo.");
        dragWithTip("Redline RPM##m_redline",
            &m_importPreset.redlineRpm, 50.0f, 3000.0f, 12000.0f,
            "RPM maxima antes de que la transmision suba de marcha.");
        dragWithTip("Final drive##m_fd",
            &m_importPreset.finalDrive, 0.05f, 1.0f, 8.0f,
            "Reduccion final entre el motor y las ruedas. Alto = mucha aceleracion, poca velocidad punta.");

        constexpr std::array<const char*, 3> kDrivetrainLabels = {
            "FWD (delantera)", "RWD (trasera)", "AWD (4x4)"
        };
        int dtIdx = static_cast<int>(m_importPreset.drivetrain);
        const std::string dtLbl =
            I18n::T("editor.import_vehicle.drivetrain") + "##m_dt";
        if (ImGui::Combo(dtLbl.c_str(), &dtIdx,
                          kDrivetrainLabels.data(),
                          static_cast<int>(kDrivetrainLabels.size()))) {
            m_importPreset.drivetrain = static_cast<vehicle::Drivetrain>(dtIdx);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.import_vehicle.drivetrain_tooltip").c_str());
        }
    }

    // --- Frenos y direccion ---
    if (ImGui::CollapsingHeader("Frenos y direccion",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Como detiene y como dobla.");
        dragWithTip("Decel. freno (m/s2)##b_decel",
            &m_importPreset.decelTargetMps2, 0.5f, 1.0f, 25.0f,
            "Desaceleracion objetivo al frenar (0 = no frena; 9.8 = ~1g, deportivo).");
        dragWithTip("Handbrake ratio##b_hb",
            &m_importPreset.handbrakeRatio, 0.05f, 0.0f, 2.0f,
            "Fuerza del freno de mano relativa al freno normal. >1 = derrapa facil.");
        dragWithTip("Max steer (deg)##b_steer",
            &m_importPreset.maxSteerDeg, 1.0f, 5.0f, 60.0f,
            "Cuanto giran las ruedas delanteras al volante (grados). Alto = mas agil pero mas inestable.");
        dragWithTip("Steer lerp##b_steerlerp",
            &m_importPreset.steerLerp, 0.1f, 1.0f, 20.0f,
            "Que tan rapido responde el volante al input. Alto = directo; bajo = pastoso.");
    }

    // --- Suspension y agarre ---
    if (ImGui::CollapsingHeader("Suspension y agarre",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Como absorbe baches y como se pega al asfalto.");
        dragWithTip("Susp. frec. (Hz)##s_freq",
            &m_importPreset.suspFrequencyHz, 0.1f, 0.5f, 5.0f,
            "Que tan dura es la suspension. Baja = blanda (sedan); alta = rigida (deportivo).");
        dragWithTip("Susp. damping##s_damp",
            &m_importPreset.suspDamping, 0.05f, 0.1f, 1.5f,
            "Que tanto absorbe el rebote (0 = pogo stick, 1 = ladrillo).");
        dragWithTip("Susp. max (mm)##s_max",
            &m_importPreset.suspMaxLenMm, 5.0f, 50.0f, 1000.0f,
            "Recorrido maximo de la suspension en extension (rueda colgada).");
        dragWithTip("Susp. min (mm)##s_min",
            &m_importPreset.suspMinLenMm, 5.0f, 20.0f, 500.0f,
            "Recorrido minimo en compresion (tope al fondo).");
        dragWithTip("Friccion long.##s_flong",
            &m_importPreset.frictionLong, 0.05f, 0.5f, 3.0f,
            "Agarre adelante/atras (acelera, frena). Alto = no patina al acelerar.");
        dragWithTip("Friccion lat.##s_flat",
            &m_importPreset.frictionLat, 0.05f, 0.5f, 3.0f,
            "Agarre lateral (curvas). Alto = pega al asfalto; bajo = drift.");
    }

    // --- Chasis ---
    if (ImGui::CollapsingHeader("Chasis (avanzado)")) {
        ImGui::TextDisabled("Resistencia al aire / friccion residual.");
        dragWithTip("Damping lineal##c_dlin",
            &m_importPreset.chassisLinearDamping, 0.02f, 0.0f, 1.0f,
            "Que tan rapido frena solo al soltar el acelerador. Alto = parece pesado / arrastra.");
        dragWithTip("Damping angular##c_dang",
            &m_importPreset.chassisAngularDamping, 0.02f, 0.0f, 1.0f,
            "Resistencia al giro libre del chasis (sin steering). Alto = mas estable, menos derrape.");
    }

    ImGui::PopItemWidth();

    if (!m_importSaveError.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s",
            I18n::T("editor.import_vehicle.save_error",
                     m_importSaveError).c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    // --- Footer fijo: Guardar / Cancelar (siempre visible) ---
    ImGui::Separator();
    if (ImGui::Button(I18n::T("editor.modal.common.save").c_str(),
                       ImVec2(140.0f, 0.0f))) {
        std::string err;
        if (saveImportedVehicle(err)) {
            m_importModalOpen = false;
            ImGui::CloseCurrentPopup();
            rescan();
        } else {
            m_importSaveError = err;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.modal.common.cancel").c_str(),
                       ImVec2(140.0f, 0.0f))) {
        m_importModalOpen = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ============================================================================
// saveImportedVehicle — copia mesh + escribe .moodvehicle + rescan
// ============================================================================
bool AssetBrowserPanel::saveImportedVehicle(std::string& err) {
    if (m_assetManager == nullptr) {
        err = "AssetManager no inyectado.";
        return false;
    }
    if (!m_importAnalysis.ok) {
        err = "Analisis invalido.";
        return false;
    }

    const std::string slug = sanitizeSlug(m_importDisplayName);
    const std::string ext = fs::path(m_importFsPath).extension().generic_string();
    // Path logico (relativo a assets/) del mesh dentro del proyecto.
    const std::string meshLogical = "vehicles/" + slug + "/" + slug + ext;
    const std::string moodLogical = "vehicles/" + slug + "/" + slug + ".moodvehicle";

    const fs::path meshOutFs = m_assetManager->resolvePath(meshLogical);
    const fs::path moodOutFs = m_assetManager->resolvePath(moodLogical);
    if (meshOutFs.empty() || moodOutFs.empty()) {
        err = "VFS rechazo el path (proyecto cerrado?).";
        return false;
    }

    // Crear carpeta destino.
    {
        std::error_code ec;
        fs::create_directories(meshOutFs.parent_path(), ec);
        if (ec) {
            err = "No se pudo crear la carpeta destino: " + ec.message();
            return false;
        }
    }

    // Copiar el .glb/.fbx (sobreescribe si existe — el dev re-importa).
    {
        std::error_code ec;
        fs::copy_file(m_importFsPath, meshOutFs,
                       fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err = "No se pudo copiar el mesh: " + ec.message();
            return false;
        }
    }

    // F2H82 polish (solucion definitiva): si el dev eligio una escala != 1.0,
    // bakeamos el factor en el nodo raiz del .glb copiado. Asi las vertices
    // quedan en metros reales y NO hace falta runtime scale (todo el resto
    // del engine ve unidades consistentes). Solo soportado en GLB (FBX queda
    // como future work — usaria mesh_scale runtime via VehicleConfig).
    if (std::fabs(m_importMeshScale - 1.0f) > 1e-6f &&
        meshOutFs.extension().generic_string() == ".glb") {
        std::string injectErr;
        if (!injectGlbRootScale(meshOutFs, m_importMeshScale, injectErr)) {
            err = "No se pudo bakear la escala en el GLB: " + injectErr;
            return false;
        }
        Log::editor()->info(
            "ImportVehicle: bakeado factor x{} en el GLB raiz '{}'.",
            m_importMeshScale, meshOutFs.generic_string());
    }

    // Escribir el .moodvehicle via Bloque C writer.
    // El writer aplica `meta.meshScale` a las medidas fisicas del analisis para
    // que terminen en metros reales (matchea las vertices ya bakeadas en el
    // .glb). El campo `mesh_scale` en el JSON queda como 1 (no se escribe) —
    // la escala ya esta en el archivo del mesh, no hay scale runtime.
    vehicle::VehicleImportMeta meta{};
    meta.displayName = m_importDisplayName;
    meta.meshPath    = meshLogical;
    meta.meshScale   = m_importMeshScale;

    std::string writerErr;
    if (!vehicle::writeVehicleConfigFile(
            m_importAnalysis, m_importPreset, meta,
            moodOutFs.generic_string(), writerErr)) {
        err = "Writer fallo: " + writerErr;
        return false;
    }

    Log::editor()->info(
        "ImportVehicle: guardado '{}' (mesh '{}', moodvehicle '{}')",
        m_importDisplayName, meshLogical, moodLogical);
    return true;
}

// ============================================================================
// confirmAndDeleteVehicle — modal "Estas seguro?" + delete del .moodvehicle
// ============================================================================
void AssetBrowserPanel::confirmAndDeleteVehicle() {
    if (m_pendingDeleteVehicle.empty()) return;

    constexpr const char* kPopupId = "###delete_vehicle_confirm";
    const std::string title = std::string("Eliminar vehiculo") + kPopupId;
    if (!ImGui::IsPopupOpen(kPopupId)) {
        ImGui::OpenPopup(kPopupId);
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(title.c_str(), nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("%s",
            I18n::T("editor.import_vehicle.delete_confirm",
                     m_pendingDeleteVehicle).c_str());
        ImGui::TextDisabled("%s",
            I18n::T("editor.import_vehicle.delete_hint").c_str());
        ImGui::Separator();
        if (ImGui::Button(I18n::T("editor.modal.common.delete").c_str(),
                           ImVec2(120.0f, 0.0f))) {
            if (m_assetManager != nullptr) {
                const fs::path fp =
                    m_assetManager->resolvePath(m_pendingDeleteVehicle);
                std::error_code ec;
                if (!fp.empty() && fs::remove(fp, ec)) {
                    Log::editor()->info(
                        "DeleteVehicle: '{}' borrado.", m_pendingDeleteVehicle);
                } else {
                    Log::editor()->warn(
                        "DeleteVehicle: no se pudo borrar '{}': {}",
                        m_pendingDeleteVehicle,
                        ec ? ec.message() : std::string("(razon desconocida)"));
                }
            }
            m_pendingDeleteVehicle.clear();
            ImGui::CloseCurrentPopup();
            rescan();
            m_reloadRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::T("editor.modal.common.cancel").c_str(),
                           ImVec2(120.0f, 0.0f))) {
            m_pendingDeleteVehicle.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Mood
