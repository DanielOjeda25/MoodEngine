// F2H58 Bloque A: Inspector — EnvironmentComponent en un panel propio
// con secciones colapsables. Pre-F2H58 la funcion vivia en
// InspectorPanel_Light.cpp por accidente historico (Hito 15 las metio
// juntas porque ambos componentes eran "ambiente"). El dispatch en
// InspectorPanel.cpp ya separaba `LightComponent` de
// `EnvironmentComponent`, asi que mover la funcion no cambia que se
// muestra; lo que cambia es como se ven los sub-bloques: cada uno
// (sky/fog, tonemap, bloom, AO) es un CollapsingHeader independiente
// — convencion Unity Volume / Unreal Post Process Volume / Godot
// WorldEnvironment.
//
// Color grading queda como CollapsingHeader propio en el Bloque B de
// F2H58 (este archivo solo se ocupa del reordenamiento).
//
// pushEditIfDone records undo via InspectorEditTracker (Hito 32). Las
// secciones bloom/SSAO propagan en vivo via applyEnvironmentFromScene
// que corre cada frame en SceneRenderer::renderScene.
//
// break-B6 (auditoria): cada `// ----- X -----` block pasa a un helper
// file-local `drawEnvXxx(env, tracker, ui, e, editedFlag)`. La función
// `renderEnvironmentSection` queda en ~30 LOC + lista de calls. Cero
// cambio de comportamiento (los bloques se relocaron tal cual).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <cstring>
#include <filesystem>
#include <memory>

namespace Mood {

namespace {

// F2H58 Bloque G: instancia default para obtener valores de fabrica de cada
// campo. Construida una vez por proceso (no por frame). El reset por seccion
// asigna desde este snapshot.
const EnvironmentComponent kEnvDefaults{};

// Helper: pinta un boton "Restablecer" pequeno alineado a la derecha.
// Llama `apply` si el dev clickea. Convencion Unity Inspector / Unreal
// Details panel: el reset es per-seccion, no per-field.
//
// F2H60 polish iter2: label visible queda en "Restablecer" plano (pedido
// del dev: "solo diga Restablecer"). La separacion entre secciones se
// resuelve aparte con drawSectionDivider() — convencion de panel propio
// estilo Unity Volume.
template<typename ApplyFn>
void drawSectionResetButton(const char* idSuffix, ApplyFn&& apply) {
    ImGui::Spacing();
    const std::string visibleLabel = std::string(ICON_FA_ROTATE " ") +
        I18n::T("editor.panel.inspector.environment.reset");
    const std::string buttonId = visibleLabel + "##envreset_" + idSuffix;
    const float btnW = ImGui::CalcTextSize(visibleLabel.c_str()).x +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > btnW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW));
    }
    if (ImGui::SmallButton(buttonId.c_str())) {
        apply();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.environment.reset_tooltip").c_str());
    }
}

// F2H60 polish iter2: separador visual entre secciones del panel Environment.
// El CollapsingHeader de ImGui no agrega margen vertical entre items —
// dos secciones cerradas quedan pegadas y se confunden. Agregamos
// Spacing + linea separadora + Spacing para que el ojo pueda
// distinguir donde termina una y arranca la otra. Equivalente al
// `ImGui::Separator()` interno pero con respiro arriba y abajo.
void drawSectionDivider() {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

// break-B6: sub-secciones extraidas. Cada una pinta un CollapsingHeader
// con sus campos + reset. Signatura uniforme (env, tracker, ui, e,
// editedFlag) — `editedFlag` es referencia al `m_editedThisFrame` del
// panel para que los edits in-place propaguen igual que antes.

void drawEnvSkyAndFog(EnvironmentComponent& env, InspectorEditTracker& tracker,
                      EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.fog").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // F2H86: dropdown de skybox/HDRI con presets + "Personalizado...".
    // Mismo patron que el dropdown de Color Grading LUT (F2H58 Bloque H).
    // El SceneRenderer detecta el cambio en applyEnvironmentFromScene y
    // hace swap del skybox + IBL bakeado. Si el HDRI custom no tiene
    // bake, el shader cae a ambient escalar (skybox sigue visible).
    struct SkyPreset { const char* labelKey; const char* path; };
    constexpr SkyPreset kSkyPresets[] = {
        { "editor.panel.inspector.environment.skybox_preset_kloofendal", "skyboxes/sky_kloofendal" },
        { "editor.panel.inspector.environment.skybox_preset_day",        "skyboxes/sky_day"        },
    };
    constexpr int kSkyPresetCount = static_cast<int>(sizeof(kSkyPresets) / sizeof(kSkyPresets[0]));
    constexpr int kSkyCustomIndex = kSkyPresetCount;

    int currentSkyIdx = kSkyCustomIndex;
    for (int i = 0; i < kSkyPresetCount; ++i) {
        if (env.skyboxPath == kSkyPresets[i].path) {
            currentSkyIdx = i;
            break;
        }
    }
    std::string skyPreviewStr;
    if (currentSkyIdx == kSkyCustomIndex) {
        namespace fs = std::filesystem;
        const std::string fname =
            fs::path(env.skyboxPath).filename().generic_string();
        skyPreviewStr = I18n::T(
            "editor.panel.inspector.environment.skybox_preset_custom_active",
            fname);
    } else {
        skyPreviewStr = I18n::T(kSkyPresets[currentSkyIdx].labelKey);
    }

    const std::string skyComboLabel =
        I18n::T("editor.panel.inspector.environment.skybox_preset") + "##envsky";
    if (ImGui::BeginCombo(skyComboLabel.c_str(), skyPreviewStr.c_str())) {
        for (int i = 0; i < kSkyPresetCount; ++i) {
            const bool selected = (currentSkyIdx == i);
            const std::string itemLabel = I18n::T(kSkyPresets[i].labelKey);
            if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                env.skyboxPath = kSkyPresets[i].path;
                editedFlag = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::Separator();
        const std::string customItem = I18n::T(
            "editor.panel.inspector.environment.skybox_preset_custom");
        if (ImGui::Selectable(customItem.c_str(), false)) {
            namespace fs = std::filesystem;
            const fs::path skyboxesDir = fs::current_path() / "assets" / "skyboxes";
            const std::string startDir = fs::exists(skyboxesDir)
                ? skyboxesDir.generic_string()
                : (fs::current_path() / "assets").generic_string();
            const std::string filterPng = I18n::T(
                "editor.panel.inspector.environment.skybox_filter");
            const auto picked = pfd::open_file(
                I18n::T("editor.panel.inspector.environment.skybox_pick"),
                startDir,
                { filterPng, "*.png" }).result();
            if (!picked.empty()) {
                fs::path abs(picked[0]);
                const fs::path assetsRoot = fs::current_path() / "assets";
                std::error_code ec;
                fs::path rel = fs::relative(abs, assetsRoot, ec);
                if (!ec && !rel.empty()) {
                    env.skyboxPath = rel.generic_string();
                } else {
                    env.skyboxPath = abs.generic_string();
                }
                editedFlag = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.environment.skybox_hint").c_str());

    const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
    int fogIdx = static_cast<int>(env.fogMode);
    const std::string fogModeLabel =
        I18n::T("editor.panel.inspector.environment.mode") + "##env";
    if (ImGui::Combo(fogModeLabel.c_str(), &fogIdx, fogModes, 4)) {
        env.fogMode = static_cast<u32>(fogIdx);
        editedFlag = true;
    }
    if (detail::fieldColorEdit3(tracker, ui, e,
            "editor.panel.inspector.environment.color", "##envfog", env.fogColor,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<EnvironmentComponent>().fogColor = v;
            },
            "Editar fog color")) {
        editedFlag = true;
    }
    if (env.fogMode == 1) {
        if (detail::fieldDragFloat(tracker, ui, e,
                "editor.panel.inspector.environment.start", "##env", env.fogLinearStart,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearStart = v;
                },
                "Editar fog linear start", 0.1f, 0.0f, 500.0f)) {
            editedFlag = true;
        }
        if (detail::fieldDragFloat(tracker, ui, e,
                "editor.panel.inspector.environment.end", "##env", env.fogLinearEnd,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearEnd = v;
                },
                "Editar fog linear end", 0.1f, 0.0f, 500.0f)) {
            editedFlag = true;
        }
    } else if (env.fogMode == 2 || env.fogMode == 3) {
        if (detail::fieldDragFloat(tracker, ui, e,
                "editor.panel.inspector.environment.density", "##env", env.fogDensity,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogDensity = v;
                },
                "Editar fog density", 0.001f, 0.0f, 1.0f)) {
            editedFlag = true;
        }
    }
    drawSectionResetButton("fog", [&]() {
        env.fogMode        = kEnvDefaults.fogMode;
        env.fogColor       = kEnvDefaults.fogColor;
        env.fogDensity     = kEnvDefaults.fogDensity;
        env.fogLinearStart = kEnvDefaults.fogLinearStart;
        env.fogLinearEnd   = kEnvDefaults.fogLinearEnd;
        editedFlag = true;
    });
}

void drawEnvTonemapExposureIbl(EnvironmentComponent& env,
                                InspectorEditTracker& tracker,
                                EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.tonemap_section").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (detail::fieldDragFloat(tracker, ui, e,
            "editor.panel.inspector.environment.exposure", "##env", env.exposure,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().exposure = v;
            },
            "Editar exposure", 0.05f, -5.0f, 5.0f)) {
        editedFlag = true;
    }

    const char* tonemaps[] = {"None", "Reinhard", "ACES"};
    int toneIdx = static_cast<int>(env.tonemapMode);
    const std::string tonemapLabel =
        I18n::T("editor.panel.inspector.environment.tonemap") + "##env";
    if (ImGui::Combo(tonemapLabel.c_str(), &toneIdx, tonemaps, 3)) {
        env.tonemapMode = static_cast<u32>(toneIdx);
        editedFlag = true;
    }

    // Hito 18: control del aporte del IBL al ambient. Util cuando el
    // skybox es muy claro y "ahoga" la directional + point lights.
    const std::string iblLabel =
        I18n::T("editor.panel.inspector.environment.ibl_intensity") + "##env";
    if (ImGui::SliderFloat(iblLabel.c_str(),
                            &env.iblIntensity, 0.0f, 2.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, env.iblIntensity,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().iblIntensity = v;
        },
        "Editar IBL intensity");
    drawSectionResetButton("tonemap", [&]() {
        env.exposure     = kEnvDefaults.exposure;
        env.tonemapMode  = kEnvDefaults.tonemapMode;
        env.iblIntensity = kEnvDefaults.iblIntensity;
        editedFlag = true;
    });
}

// F2H55: Bloom.
void drawEnvBloom(EnvironmentComponent& env, InspectorEditTracker& tracker,
                  EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.bloom").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const std::string bloomEnabledLabel =
        I18n::T("editor.panel.inspector.environment.bloom_enabled") + "##env";
    if (ImGui::Checkbox(bloomEnabledLabel.c_str(), &env.bloomEnabled)) {
        editedFlag = true;
    }
    if (env.bloomEnabled) {
        const std::string bloomThLabel =
            I18n::T("editor.panel.inspector.environment.bloom_threshold") + "##env";
        if (ImGui::SliderFloat(bloomThLabel.c_str(),
                                &env.bloomThreshold, 0.0f, 3.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.bloomThreshold,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomThreshold = v;
            },
            "Editar bloom threshold");
        const std::string bloomIntLabel =
            I18n::T("editor.panel.inspector.environment.bloom_intensity") + "##env";
        if (ImGui::SliderFloat(bloomIntLabel.c_str(),
                                &env.bloomIntensity, 0.0f, 2.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.bloomIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomIntensity = v;
            },
            "Editar bloom intensity");
        const std::string bloomRadLabel =
            I18n::T("editor.panel.inspector.environment.bloom_radius") + "##env";
        if (ImGui::SliderFloat(bloomRadLabel.c_str(),
                                &env.bloomRadius, 0.5f, 3.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.bloomRadius,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomRadius = v;
            },
            "Editar bloom radius");
    }
    drawSectionResetButton("bloom", [&]() {
        env.bloomEnabled   = kEnvDefaults.bloomEnabled;
        env.bloomThreshold = kEnvDefaults.bloomThreshold;
        env.bloomIntensity = kEnvDefaults.bloomIntensity;
        env.bloomRadius    = kEnvDefaults.bloomRadius;
        editedFlag = true;
    });
}

// F2H56: SSAO.
void drawEnvSsao(EnvironmentComponent& env, InspectorEditTracker& tracker,
                 EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.ssao").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const std::string ssaoEnabledLabel =
        I18n::T("editor.panel.inspector.environment.ssao_enabled") + "##env";
    if (ImGui::Checkbox(ssaoEnabledLabel.c_str(), &env.ssaoEnabled)) {
        editedFlag = true;
    }
    if (env.ssaoEnabled) {
        const std::string ssaoRadLabel =
            I18n::T("editor.panel.inspector.environment.ssao_radius") + "##env";
        if (ImGui::SliderFloat(ssaoRadLabel.c_str(),
                                &env.ssaoRadius, 0.05f, 2.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.ssaoRadius,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssaoRadius = v;
            },
            "Editar SSAO radius");
        const std::string ssaoIntLabel =
            I18n::T("editor.panel.inspector.environment.ssao_intensity") + "##env";
        if (ImGui::SliderFloat(ssaoIntLabel.c_str(),
                                &env.ssaoIntensity, 0.0f, 3.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.ssaoIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssaoIntensity = v;
            },
            "Editar SSAO intensity");
    }
    drawSectionResetButton("ssao", [&]() {
        env.ssaoEnabled   = kEnvDefaults.ssaoEnabled;
        env.ssaoRadius    = kEnvDefaults.ssaoRadius;
        env.ssaoIntensity = kEnvDefaults.ssaoIntensity;
        editedFlag = true;
    });
}

// F2H60: Shadows (CSM). polish iter2: la seccion ya no tiene checkbox
// propio. Las sombras se gobiernan por LightComponent::castShadows
// (per-light). Aca solo viven los knobs de calidad (cantidad de
// cascadas + split lambda) -- aplican cuando hay una directional con
// castShadows en la escena.
void drawEnvCsmShadows(EnvironmentComponent& env, InspectorEditTracker& tracker,
                       EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.csm").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.environment.csm_hint").c_str());
    // Cascadas: 1..4. Slider int.
    int cascades = static_cast<int>(env.csmCascadeCount);
    const std::string csmCascLabel =
        I18n::T("editor.panel.inspector.environment.csm_cascades") + "##env";
    if (ImGui::SliderInt(csmCascLabel.c_str(), &cascades, 1, 4)) {
        env.csmCascadeCount = static_cast<u32>(cascades);
        editedFlag = true;
    }
    detail::pushEditIfDone<u32>(tracker, ui, e, env.csmCascadeCount,
        [](Entity& en, const u32& v) {
            en.getComponent<EnvironmentComponent>().csmCascadeCount = v;
        },
        "Editar CSM cascadas");

    // Lambda: 0..1. 0=lineal, 1=log, 0.5=hybrid.
    const std::string csmLambdaLabel =
        I18n::T("editor.panel.inspector.environment.csm_lambda") + "##env";
    if (ImGui::SliderFloat(csmLambdaLabel.c_str(),
                            &env.csmSplitLambda, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, env.csmSplitLambda,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().csmSplitLambda = v;
        },
        "Editar CSM lambda");
    drawSectionResetButton("csm", [&]() {
        env.csmCascadeCount = kEnvDefaults.csmCascadeCount;
        env.csmSplitLambda  = kEnvDefaults.csmSplitLambda;
        editedFlag = true;
    });
}

// F2H58 Bloque H: Color Grading.
void drawEnvColorGrading(EnvironmentComponent& env, InspectorEditTracker& tracker,
                          EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.color_grading").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const std::string cgEnabledLabel =
        I18n::T("editor.panel.inspector.environment.color_grading_enabled") + "##env";
    if (ImGui::Checkbox(cgEnabledLabel.c_str(), &env.colorGradingEnabled)) {
        editedFlag = true;
    }
    if (env.colorGradingEnabled) {
        // F2H58 Bloque H: dropdown de presets. Reemplaza el InputText +
        // boton "..." crudos por un Combo con looks nombrados — UX al
        // estilo "Camera Filter" de Unity / "Cine Looks" de Premiere.
        // Los 4 built-ins son los LUTs shipados en assets/luts/. El
        // dev solo elige por nombre; el path interno se setea solo.
        // Opcion final "Personalizado..." abre file picker para LUTs
        // externas (escape hatch — no debe ser el flow principal).
        struct CgPreset { const char* labelKey; const char* path; };
        constexpr CgPreset kPresets[] = {
            { "editor.panel.inspector.environment.color_grading_preset_none",     ""                              },
            { "editor.panel.inspector.environment.color_grading_preset_warm",     "luts/cinema_warm.png"          },
            { "editor.panel.inspector.environment.color_grading_preset_cool",     "luts/matrix_cool.png"          },
            { "editor.panel.inspector.environment.color_grading_preset_noir",     "luts/noir_high_contrast.png"   },
            { "editor.panel.inspector.environment.color_grading_preset_identity", "luts/identity.png"             },
        };
        constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
        constexpr int kCustomIndex = kPresetCount; // sentinel index para "Personalizado..."

        // Resolver indice actual: si el path matchea un preset, mostrar
        // ese; sino "Personalizado: <filename>".
        int currentIdx = kCustomIndex;
        for (int i = 0; i < kPresetCount; ++i) {
            if (env.colorGradingLutPath == kPresets[i].path) {
                currentIdx = i;
                break;
            }
        }
        // F2H60 polish fix: el preview tiene que vivir hasta despues
        // del BeginCombo. Pre-fix: `I18n::T(...).c_str()` daba un
        // puntero dangling al string temporal liberado, lo que
        // renderizaba "?????????" como preview. Bug clasico de
        // lifetime en C++. Fix: asignar a string local antes de pasar
        // el .c_str() a ImGui.
        std::string previewStr;
        if (currentIdx == kCustomIndex) {
            namespace fs = std::filesystem;
            const std::string fname =
                fs::path(env.colorGradingLutPath).filename().generic_string();
            previewStr = I18n::T("editor.panel.inspector.environment.color_grading_preset_custom_active",
                                   fname);
        } else {
            previewStr = I18n::T(kPresets[currentIdx].labelKey);
        }
        const char* preview = previewStr.c_str();

        const std::string comboLabel =
            I18n::T("editor.panel.inspector.environment.color_grading_preset") + "##env";
        if (ImGui::BeginCombo(comboLabel.c_str(), preview)) {
            for (int i = 0; i < kPresetCount; ++i) {
                const bool selected = (currentIdx == i);
                const std::string itemLabel = I18n::T(kPresets[i].labelKey);
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    env.colorGradingLutPath = kPresets[i].path;
                    editedFlag = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            // Item "Personalizado..." — abre file picker. No se selecciona
            // visualmente (se selecciona el resultado del picker).
            const std::string customItem = I18n::T(
                "editor.panel.inspector.environment.color_grading_preset_custom");
            if (ImGui::Selectable(customItem.c_str(), false)) {
                namespace fs = std::filesystem;
                const fs::path lutsDir = fs::current_path() / "assets" / "luts";
                const std::string startDir = fs::exists(lutsDir)
                    ? lutsDir.generic_string()
                    : (fs::current_path() / "assets").generic_string();
                const std::string filterPng =
                    I18n::T("editor.panel.inspector.environment.color_grading_filter");
                const auto picked = pfd::open_file(
                    I18n::T("editor.panel.inspector.environment.color_grading_pick"),
                    startDir,
                    { filterPng, "*.png" }).result();
                if (!picked.empty()) {
                    fs::path abs(picked[0]);
                    const fs::path assetsRoot = fs::current_path() / "assets";
                    std::error_code ec;
                    fs::path rel = fs::relative(abs, assetsRoot, ec);
                    if (!ec && !rel.empty()) {
                        env.colorGradingLutPath = rel.generic_string();
                    } else {
                        env.colorGradingLutPath = abs.generic_string();
                    }
                    editedFlag = true;
                }
            }
            ImGui::EndCombo();
        }

        // Intensity slider.
        const std::string cgIntLabel =
            I18n::T("editor.panel.inspector.environment.color_grading_intensity") + "##env";
        if (ImGui::SliderFloat(cgIntLabel.c_str(),
                                &env.colorGradingIntensity, 0.0f, 1.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.colorGradingIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().colorGradingIntensity = v;
            },
            "Editar color grading intensity");
    }
    drawSectionResetButton("cgrade", [&]() {
        env.colorGradingEnabled   = kEnvDefaults.colorGradingEnabled;
        env.colorGradingLutPath   = kEnvDefaults.colorGradingLutPath;
        env.colorGradingIntensity = kEnvDefaults.colorGradingIntensity;
        editedFlag = true;
    });
}

// F2H61: SSR.
void drawEnvSsr(EnvironmentComponent& env, InspectorEditTracker& tracker,
                EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.ssr").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.environment.ssr_hint").c_str());

    const std::string ssrEnabledLabel =
        I18n::T("editor.panel.inspector.environment.ssr_enabled") + "##env";
    if (ImGui::Checkbox(ssrEnabledLabel.c_str(), &env.ssrEnabled)) {
        editedFlag = true;
    }
    if (env.ssrEnabled) {
        // Intensity 0..1.
        const std::string ssrIntLabel =
            I18n::T("editor.panel.inspector.environment.ssr_intensity") + "##env";
        if (ImGui::SliderFloat(ssrIntLabel.c_str(),
                                &env.ssrIntensity, 0.0f, 1.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.ssrIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssrIntensity = v;
            },
            "Editar SSR intensity");

        // Max steps 8..128. SliderInt requiere int&; usamos un proxy.
        int steps = static_cast<int>(env.ssrMaxSteps);
        const std::string ssrStepsLabel =
            I18n::T("editor.panel.inspector.environment.ssr_max_steps") + "##env";
        if (ImGui::SliderInt(ssrStepsLabel.c_str(), &steps, 8, 128)) {
            env.ssrMaxSteps = static_cast<u32>(steps);
            editedFlag = true;
        }
        detail::pushEditIfDone<u32>(tracker, ui, e, env.ssrMaxSteps,
            [](Entity& en, const u32& v) {
                en.getComponent<EnvironmentComponent>().ssrMaxSteps = v;
            },
            "Editar SSR max steps");

        // Step size view-space units.
        const std::string ssrStepSizeLabel =
            I18n::T("editor.panel.inspector.environment.ssr_step_size") + "##env";
        if (ImGui::SliderFloat(ssrStepSizeLabel.c_str(),
                                &env.ssrStepSize, 0.05f, 1.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.ssrStepSize,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssrStepSize = v;
            },
            "Editar SSR step size");

        // Thickness view-space units.
        const std::string ssrThickLabel =
            I18n::T("editor.panel.inspector.environment.ssr_thickness") + "##env";
        if (ImGui::SliderFloat(ssrThickLabel.c_str(),
                                &env.ssrThickness, 0.01f, 2.0f, "%.2f")) {
            editedFlag = true;
        }
        detail::pushEditIfDone<f32>(tracker, ui, e, env.ssrThickness,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssrThickness = v;
            },
            "Editar SSR thickness");
    }
    drawSectionResetButton("ssr", [&]() {
        env.ssrEnabled   = kEnvDefaults.ssrEnabled;
        env.ssrMaxSteps  = kEnvDefaults.ssrMaxSteps;
        env.ssrThickness = kEnvDefaults.ssrThickness;
        env.ssrStepSize  = kEnvDefaults.ssrStepSize;
        env.ssrIntensity = kEnvDefaults.ssrIntensity;
        editedFlag = true;
    });
}

} // namespace

void InspectorPanel::renderEnvironmentSection(Entity e) {
    auto& env = e.getComponent<EnvironmentComponent>();
    if (!beginComponentSection<EnvironmentComponent>(e, ICON_FA_TREE " Environment")) return;

    drawEnvSkyAndFog(env, m_editTracker, m_ui, e, m_editedThisFrame);
    drawSectionDivider();

    // F2H58 Bloque I: Post-Process consolidado. Wrapper header top-level
    // que agrupa los 4 sub-pases (Tonemap+Exposure / Bloom / SSAO /
    // Color Grading) bajo un mismo nodo, estilo Unreal Post Process
    // Volume y Unity Volume. Cada sub-header sigue siendo independiente
    // con su propio reset. Indent visual los hace ver subordinados al
    // wrapper sin requerir TreeNode anidados (que duplican el chevron).
    if (ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.post_process").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        drawEnvTonemapExposureIbl(env, m_editTracker, m_ui, e, m_editedThisFrame);
        drawSectionDivider();
        drawEnvBloom(env, m_editTracker, m_ui, e, m_editedThisFrame);
        drawSectionDivider();
        drawEnvSsao(env, m_editTracker, m_ui, e, m_editedThisFrame);
        drawSectionDivider();
        drawEnvCsmShadows(env, m_editTracker, m_ui, e, m_editedThisFrame);
        drawSectionDivider();
        drawEnvColorGrading(env, m_editTracker, m_ui, e, m_editedThisFrame);
        drawSectionDivider();
        drawEnvSsr(env, m_editTracker, m_ui, e, m_editedThisFrame);
        ImGui::Unindent();
    }
}

} // namespace Mood
