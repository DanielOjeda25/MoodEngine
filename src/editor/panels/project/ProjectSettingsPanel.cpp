#include "editor/panels/project/ProjectSettingsPanel.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"  // F3H4 polish: ICON_FA_ROTATE_LEFT
#include "engine/project/ProjectSettings.h"
#include "engine/scene/serialization/ProjectSerializer.h"  // Project struct

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Presets de Target FPS (estilo Unity Quality > Target Frame Rate). Si
// el .moodproj trae un valor que no esta en esta lista (caso edge: dev
// edito el JSON a mano), se prepende como una entry extra "Personalizado"
// para no perder el valor — el dev puede seguir editandolo solo via JSON
// hasta que aparezca un control de input numerico en hitos siguientes.
constexpr int kFpsPresets[]   = {30, 60, 120, 144};
constexpr int kFpsPresetCount = 4;
const char*   kFpsLabels[]    = {"30 FPS", "60 FPS", "120 FPS", "144 FPS"};

int findPresetIndex(int targetFps) {
    for (int i = 0; i < kFpsPresetCount; ++i) {
        if (kFpsPresets[i] == targetFps) return i;
    }
    return -1;  // no es un preset estandar
}

// Layout estilo Unity: label a la izquierda con ancho fijo, control a
// la derecha. Hint debajo en color disabled.
constexpr float kLabelColumnWidth = 160.0f;
constexpr float kControlWidth     = 200.0f;

// F3H4 polish: boton ↺ chiquito a la derecha del control que solo
// aparece si el valor difiere del default. Click → resetea + dirty.
// Pattern Unity/Unreal: no agregar visual noise para fields default,
// surfacear cuando hay un override del usuario para que sea facil
// volver a la "receta original".
template <typename T>
bool resetButton(const char* widgetIdSuffix, T& value, T defaultValue) {
    if (value == defaultValue) return false;  // no visual noise

    ImGui::SameLine();
    const std::string btnLabel =
        std::string(ICON_FA_ROTATE_LEFT) + "##reset_" + widgetIdSuffix;
    bool clicked = false;
    if (ImGui::SmallButton(btnLabel.c_str())) {
        value = defaultValue;
        clicked = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.project_settings.reset_default").c_str());
    }
    return clicked;
}

} // namespace

void ProjectSettingsPanel::onImGuiRender() {
    if (!visible) return;

    // Ventana flotante centrada + tamano fijo, no dockeable, sin
    // resize/collapse — estilo Unity/Unreal Project Settings. Convencion
    // de engines reales: Project Settings es para SET-AND-FORGET (defaults
    // de gameplay/quality/rendering), NO para tunear en vivo durante
    // Play. El use case de live tuning lo resuelve otra superficie en
    // hitos futuros (componente PlayerController editable en Inspector
    // durante Play, o un HUD "Quick Tuning" overlay).
    //
    // F3H4 intento dockable + resize libre para soportar live tuning
    // pero fue revertido — contradecia la convencion y el dev pidio
    // volver a la separacion clara de paradigmas. Ver DECISIONS.md.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540.0f, 360.0f), ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking;

    if (!ImGui::Begin(name(), &visible, kFlags)) {
        ImGui::End();
        return;
    }

    Project* project = (m_ui != nullptr) ? m_ui->currentProject() : nullptr;
    if (project == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.project_settings.no_project").c_str());
        ImGui::End();
        return;
    }

    ImGui::Spacing();

    // F3H4: TabBar reintroducido (F3H1 polish lo elimino por scope chico
    // con 1 sola seccion; ahora hay 2 con contenido real).
    if (ImGui::BeginTabBar("##project_settings_tabs")) {
        if (ImGui::BeginTabItem(
                I18n::T("editor.project_settings.section.performance").c_str())) {
            ImGui::Spacing();
            drawPerformanceSection(project->settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                I18n::T("editor.project_settings.section.gameplay").c_str())) {
            ImGui::Spacing();
            drawGameplaySection(project->settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                I18n::T("editor.project_settings.section.character").c_str())) {
            ImGui::Spacing();
            drawCharacterSection(project->settings);
            ImGui::EndTabItem();
        }
        // F3H6 polish: tab Snap movida a MapEditorTopBar (popover).
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ProjectSettingsPanel::drawPerformanceSection(ProjectSettings& settings) {
    // F3H4: SeparatorText eliminado — el TabBar ya rotula la seccion.
    ImGui::Indent();

    // === Target FPS (Combo de presets) ===
    int currentIdx = findPresetIndex(settings.targetFps);

    // Si el valor cargado no es un preset, prepender una entry extra al
    // combo para mostrarlo sin perderlo. El i18n `target_fps.custom_label`
    // usa interpolacion estilo fmt (`{}`).
    std::string customLabel;
    const bool isCustom = (currentIdx < 0);
    const char* labels[kFpsPresetCount + 1];
    int comboCount = kFpsPresetCount;
    if (isCustom) {
        customLabel = I18n::T(
            "editor.project_settings.target_fps.custom_label",
            settings.targetFps);
        labels[0] = customLabel.c_str();
        for (int i = 0; i < kFpsPresetCount; ++i) labels[i + 1] = kFpsLabels[i];
        comboCount = kFpsPresetCount + 1;
        currentIdx = 0;  // entry custom en posicion 0
    } else {
        for (int i = 0; i < kFpsPresetCount; ++i) labels[i] = kFpsLabels[i];
    }

    ImGui::TextUnformatted(I18n::T("editor.project_settings.target_fps").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);

    if (ImGui::Combo("##target_fps", &currentIdx, labels, comboCount)) {
        // Mapear el index seleccionado al int real. Si habia entry Custom
        // en index 0 y el dev eligio un preset, sumamos -1 para skipearla.
        int newVal = settings.targetFps;
        if (isCustom) {
            if (currentIdx == 0) {
                newVal = settings.targetFps;  // re-seleccionar Custom = no-op
            } else {
                newVal = kFpsPresets[currentIdx - 1];
            }
        } else {
            newVal = kFpsPresets[currentIdx];
        }
        if (newVal != settings.targetFps) {
            settings.targetFps = newVal;
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
    }

    // F3H4 polish: reset a default (60) si el dev lo cambio.
    if (resetButton("target_fps", settings.targetFps,
                    ProjectSettings{}.targetFps)) {
        if (m_ui != nullptr) m_ui->requestProjectDirty();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.project_settings.target_fps_hint").c_str());

    ImGui::Unindent();
}

// F3H4: seccion Gameplay (walk/crouch/jump). 4 SliderFloat con tooltip
// hint para cada uno. Cada cambio dispara dirty -> EditorApplication
// llama markDirty() en pumpUiRequests. Las lecturas en PlayerApplication
// y EditorPlayMode leen settings.gameplay live cada frame (no se cachea
// al cargar el proyecto — el dev puede editar y sentir el cambio sin
// reiniciar Play).
void ProjectSettingsPanel::drawGameplaySection(ProjectSettings& settings) {
    ImGui::Indent();

    // F3H4 polish: cada slider tiene su boton ↺ reset a default,
    // visible solo si el valor difiere. Pasamos `defaultValue` para
    // que el helper sepa cuando emitir el boton.
    const GameplaySettings defaults;

    auto drawSlider = [&](const char* keyLabel,
                          const char* keyHint,
                          const char* widgetId,
                          const char* resetIdSuffix,
                          f32& value,
                          f32 defaultValue,
                          f32 minVal,
                          f32 maxVal,
                          const char* fmt) {
        ImGui::TextUnformatted(I18n::T(keyLabel).c_str());
        ImGui::SameLine(kLabelColumnWidth);
        ImGui::SetNextItemWidth(kControlWidth);
        if (ImGui::SliderFloat(widgetId, &value, minVal, maxVal, fmt)) {
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(keyHint).c_str());
        }
        if (resetButton(resetIdSuffix, value, defaultValue)) {
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
    };

    drawSlider("editor.project_settings.gameplay.walk_speed",
               "editor.project_settings.gameplay.walk_speed_hint",
               "##walk_speed", "walk_speed",
               settings.gameplay.walkSpeed, defaults.walkSpeed,
               1.0f, 12.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.crouch_speed",
               "editor.project_settings.gameplay.crouch_speed_hint",
               "##crouch_speed", "crouch_speed",
               settings.gameplay.crouchSpeed, defaults.crouchSpeed,
               0.5f, 6.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.jump_velocity",
               "editor.project_settings.gameplay.jump_velocity_hint",
               "##jump_velocity", "jump_velocity",
               settings.gameplay.jumpVelocity, defaults.jumpVelocity,
               1.0f, 15.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.jump_cooldown",
               "editor.project_settings.gameplay.jump_cooldown_hint",
               "##jump_cooldown", "jump_cooldown",
               settings.gameplay.jumpCooldownSec, defaults.jumpCooldownSec,
               0.0f, 1.0f, "%.2f s");

    ImGui::Unindent();
}

// F3H5: seccion Character (capsule + eye + headbob). 7 SliderFloat con
// reset buttons + tooltips. Mismo patron exacto que drawGameplaySection
// — copy-paste validado. Si llega F3H6/F3H7 con mas tabs, refactorear
// el helper drawSlider a metodo de clase o namespace helper.
void ProjectSettingsPanel::drawCharacterSection(ProjectSettings& settings) {
    ImGui::Indent();

    const CharacterSettings defaults;

    auto drawSlider = [&](const char* keyLabel,
                          const char* keyHint,
                          const char* widgetId,
                          const char* resetIdSuffix,
                          f32& value,
                          f32 defaultValue,
                          f32 minVal,
                          f32 maxVal,
                          const char* fmt) {
        ImGui::TextUnformatted(I18n::T(keyLabel).c_str());
        ImGui::SameLine(kLabelColumnWidth);
        ImGui::SetNextItemWidth(kControlWidth);
        if (ImGui::SliderFloat(widgetId, &value, minVal, maxVal, fmt)) {
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(keyHint).c_str());
        }
        if (resetButton(resetIdSuffix, value, defaultValue)) {
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
    };

    drawSlider("editor.project_settings.character.half_height_stand",
               "editor.project_settings.character.half_height_stand_hint",
               "##half_height_stand", "half_height_stand",
               settings.character.halfHeightStand, defaults.halfHeightStand,
               0.25f, 1.25f, "%.2f m");

    drawSlider("editor.project_settings.character.half_height_crouch",
               "editor.project_settings.character.half_height_crouch_hint",
               "##half_height_crouch", "half_height_crouch",
               settings.character.halfHeightCrouch, defaults.halfHeightCrouch,
               0.05f, 0.75f, "%.2f m");

    drawSlider("editor.project_settings.character.radius",
               "editor.project_settings.character.radius_hint",
               "##char_radius", "char_radius",
               settings.character.radius, defaults.radius,
               0.2f, 1.0f, "%.2f m");

    drawSlider("editor.project_settings.character.eye_height_stand",
               "editor.project_settings.character.eye_height_stand_hint",
               "##eye_height_stand", "eye_height_stand",
               settings.character.eyeHeightStand, defaults.eyeHeightStand,
               0.0f, 1.5f, "%.2f m");

    drawSlider("editor.project_settings.character.eye_height_crouch",
               "editor.project_settings.character.eye_height_crouch_hint",
               "##eye_height_crouch", "eye_height_crouch",
               settings.character.eyeHeightCrouch, defaults.eyeHeightCrouch,
               0.0f, 0.8f, "%.2f m");

    drawSlider("editor.project_settings.character.headbob_frequency",
               "editor.project_settings.character.headbob_frequency_hint",
               "##headbob_freq", "headbob_freq",
               settings.character.headbobFrequency, defaults.headbobFrequency,
               0.5f, 10.0f, "%.1f Hz");

    drawSlider("editor.project_settings.character.headbob_amplitude",
               "editor.project_settings.character.headbob_amplitude_hint",
               "##headbob_amp", "headbob_amp",
               settings.character.headbobAmplitude, defaults.headbobAmplitude,
               0.0f, 0.2f, "%.3f m");

    ImGui::Unindent();
}

} // namespace Mood


