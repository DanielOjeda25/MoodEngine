#include "editor/panels/scene/InspectorPanel.h"

#include "editor/commands/AddComponentCommand.h"      // F2H45 Bloque A
#include "editor/commands/HistoryStack.h"             // F2H45 Bloque A
#include "editor/commands/PasteComponentCommand.h"   // F3H9
#include "editor/components/ComponentClipboard.h"    // F3H9
#include "editor/selection/SelectionSet.h"           // F2H13
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"              // F3H9: ICON_FA_PASTE
#include "engine/scene/entity_type/EntityTypeTable.h"  // F3H9: type label
#include "core/UserSettings.h"  // F3H22: inspectorActiveCategory
#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <vector>

namespace Mood {

namespace {

// F2H44 Bloque A: case-insensitive substring match para el search del
// popup Add Component.
bool fuzzyMatch(const std::string& haystack, const char* needle) {
    if (needle == nullptr || needle[0] == '\0') return true;
    std::string a = haystack;
    std::string b = needle;
    std::transform(a.begin(), a.end(), a.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return a.find(b) != std::string::npos;
}

// F3H22: mapeo componente → categoría del Inspector. Decisión D3 del
// plan F3H22: Render absorbe Light/Camera/Particles; Physics absorbe
// Trigger/ForceField; Gameplay absorbe Inventory/Dialog/Vehicle/Quest.
// El helper retorna true si la categoría activa coincide con la
// declarada del componente. El modo "all" fue eliminado en revisión
// reactiva del dev (confundía) — siempre hay una categoría única activa.
bool catActive(const std::string& active, const char* expected) {
    return active == expected;
}

// F3H22: ¿la entity activa tiene al menos un componente de la categoría?
// Usado por la barra de icons del Inspector para mostrar solo categorías
// con contenido (no spammear icons grises). Object siempre devuelve true
// (toda entity tiene Transform).
bool entityHasCategory(Entity e, const char* category) {
    if (std::strcmp(category, "object") == 0) return true;
    if (std::strcmp(category, "render") == 0) {
        return e.hasComponent<MeshRendererComponent>()
            || e.hasComponent<BrushComponent>()
            || e.hasComponent<LightComponent>()
            || e.hasComponent<CameraComponent>()
            || e.hasComponent<ParticleEmitterComponent>();
    }
    if (std::strcmp(category, "animation") == 0) {
        return e.hasComponent<AnimatorComponent>();
    }
    if (std::strcmp(category, "audio") == 0) {
        return e.hasComponent<AudioSourceComponent>();
    }
    if (std::strcmp(category, "physics") == 0) {
        return e.hasComponent<RigidBodyComponent>()
            || e.hasComponent<JointComponent>()
            || e.hasComponent<RagdollComponent>()
            || e.hasComponent<ClothComponent>()
            || e.hasComponent<TriggerComponent>()
            || e.hasComponent<ForceFieldComponent>();
    }
    if (std::strcmp(category, "gameplay") == 0) {
        return e.hasComponent<ScriptComponent>()
            || e.hasComponent<VehicleComponent>()
            || e.hasComponent<InventoryComponent>()
            || e.hasComponent<HealthComponent>()    // F4H1
            || e.hasComponent<WeaponComponent>();   // F4H2
    }
    if (std::strcmp(category, "environment") == 0) {
        return e.hasComponent<EnvironmentComponent>();
    }
    return false;
}

} // namespace

// F2H24: nucleo del Inspector. El cuerpo de cada componente vive en
// archivos parciales `InspectorPanel_<Dominio>.cpp` (Transform,
// MeshRenderer, Light, Script, Physics, Audio, Animation, Particles,
// Brush, Misc). Este archivo solo:
//   1) chequea visibilidad / inyeccion de UI / entidad seleccionada
//   2) muestra el header de multi-seleccion
//   3) dispatchea a cada `renderXxxSection(e)` segun los componentes
//      presentes en `e`.
// Sin logica de edicion ni helpers; eso queda en los partials +
// InspectorPanel_Internal.h.
void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.ui_not_injected").c_str());
        ImGui::End();
        return;
    }

    const std::string activeCat = UserSettings::editor().inspectorActiveCategory;
    // F3H22: Environment es scene-wide — accesible sin seleccionar la
    // entity portadora (mismo pattern que Blender World Properties).
    // Si la categoría activa es "environment", buscamos el singleton
    // en la scene en vez de requerir selección.
    const bool isSceneWideCat = (activeCat == "environment");
    // F3H28: categorias scene-global (Grupos + Map Tools) no requieren
    // ni selección ni singleton. Listan/configuran state global del mapa.
    const bool isGlobalCat = (activeCat == "groups" || activeCat == "maptools");

    Entity e = m_ui->selectedEntity();

    // F3H22: buscar la entity portadora del EnvironmentComponent para
    // las categorías scene-wide. Si no hay (escena nueva sin Env), `eForRender`
    // queda invalid y el dispatcher muestra el mensaje vacío.
    Entity eForRender = e;
    if (isSceneWideCat) {
        Entity foundEnv{};
        if (m_ui->scene() != nullptr) {
            m_ui->scene()->forEach<EnvironmentComponent>(
                [&](Entity en, EnvironmentComponent&) {
                    if (!foundEnv) foundEnv = en;
                });
        }
        eForRender = foundEnv;
    }

    // Si NO es scene-wide y no hay selección, mostrar hint clásico — pero
    // dejar la category bar visible para que el dev pueda saltar a una
    // categoría scene-wide (Environment / Grupos / Map Tools).
    const bool needsSelection = !isSceneWideCat && !isGlobalCat && !e;

    // F3H22: layout en dos columnas — barra de icons vertical a la
    // izquierda + body del Inspector a la derecha.
    constexpr float kCategoryBarWidth = 36.0f;
    ImGui::BeginChild("##inspector_category_bar",
                       ImVec2(kCategoryBarWidth, 0.0f),
                       false /* border */,
                       ImGuiWindowFlags_NoScrollbar);
    renderCategoryBar(e);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 4.0f);

    ImGui::BeginChild("##inspector_body",
                       ImVec2(0.0f, 0.0f),
                       false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    // F3H28: categorias globales se dibujan directo y salen — no entran al
    // dispatch per-entity ni muestran "no selection" / "+Add Component".
    if (isGlobalCat) {
        if (activeCat == "groups")        renderGroupsSection();
        else if (activeCat == "maptools") renderMapToolsSection();
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    if (needsSelection) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.no_selection").c_str());
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.no_selection_hint").c_str());
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    if (isSceneWideCat && !eForRender) {
        // F3H29: Blender World Properties pattern — el singleton de
        // Environment SIEMPRE existe. Si por algún flow no se creó
        // (proyectos pre-F3H22, mapas viejos, dev borró la entity con
        // delete forzado), lo recreamos en el momento que el dev entra
        // a la categoría. El próximo frame ya verá el panel completo.
        if (m_ui != nullptr && activeCat == "environment") {
            m_ui->requestEnsureEnvironment();
        }
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    Entity dispatchEntity = isSceneWideCat ? eForRender : e;

    // F3H9: type label (Blender Object Type / Hammer entity class) en
    // el header del Inspector. Solo cuando NO es scene-wide (el singleton
    // de Environment tiene su propio header).
    if (!isSceneWideCat && dispatchEntity.hasComponent<TagComponent>()) {
        const auto& tag = dispatchEntity.getComponent<TagComponent>();
        const std::string typeLabel = I18n::T(
            EntityTypeTable::i18nKey(tag.entityType));
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.entity_type_label",
                    typeLabel).c_str());
    }

    // F2H13: header "+N adicionales" cuando hay multi-seleccion.
    // Inspector solo edita la `active`; multi-edit es diferido (excepto
    // Transform — ver renderTransformSection). Solo cuando NO scene-wide.
    if (!isSceneWideCat) {
        const SelectionSet& set = m_ui->selectionSet();
        if (set.selected.size() > 1) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.multi_extra",
                        static_cast<int>(set.selected.size() - 1)).c_str());
            ImGui::Separator();
        }
    }

    // F3H22: barra "Plegar/Expandir todo" (F2H81) eliminada — con
    // categorías filtrando, cada vista tiene pocos componentes y la
    // toolbar ya no aporta. Default colapsado en beginComponentSection.

    // Dispatch por componente. F3H22: gateado por categoría activa.
    if (catActive(activeCat, "object")) {
        if (dispatchEntity.hasComponent<TagComponent>())          renderTagSection(dispatchEntity);
        if (dispatchEntity.hasComponent<TransformComponent>())    renderTransformSection(dispatchEntity);
    }
    if (catActive(activeCat, "render")) {
        if (dispatchEntity.hasComponent<MeshRendererComponent>())     renderMeshRendererSection(dispatchEntity);
        if (dispatchEntity.hasComponent<CameraComponent>())           renderCameraSection(dispatchEntity);
        if (dispatchEntity.hasComponent<LightComponent>())            renderLightSection(dispatchEntity);
        if (dispatchEntity.hasComponent<ParticleEmitterComponent>())  renderParticleEmitterSection(dispatchEntity);
        if (dispatchEntity.hasComponent<BrushComponent>())            renderBrushSection(dispatchEntity);
    }
    if (catActive(activeCat, "environment")) {
        if (dispatchEntity.hasComponent<EnvironmentComponent>())  renderEnvironmentSection(dispatchEntity);
    }
    if (catActive(activeCat, "gameplay")) {
        if (dispatchEntity.hasComponent<ScriptComponent>())       renderScriptSection(dispatchEntity);
        if (dispatchEntity.hasComponent<VehicleComponent>())      renderVehicleSection(dispatchEntity);  // F2H67
        if (dispatchEntity.hasComponent<InventoryComponent>())    renderInventorySection(dispatchEntity);  // F2H51
        if (dispatchEntity.hasComponent<HealthComponent>())       renderHealthSection(dispatchEntity);    // F4H1
        if (dispatchEntity.hasComponent<WeaponComponent>())       renderWeaponSection(dispatchEntity);    // F4H2
    }
    if (catActive(activeCat, "physics")) {
        if (dispatchEntity.hasComponent<RigidBodyComponent>())    renderRigidBodySection(dispatchEntity);
        if (dispatchEntity.hasComponent<JointComponent>())        renderJointSection(dispatchEntity);  // F2H65
        if (dispatchEntity.hasComponent<RagdollComponent>())      renderRagdollSection(dispatchEntity);  // break-A4
        if (dispatchEntity.hasComponent<TriggerComponent>())      renderTriggerSection(dispatchEntity);
        if (dispatchEntity.hasComponent<ForceFieldComponent>())   renderForceFieldSection(dispatchEntity);  // F2H72
        if (dispatchEntity.hasComponent<ClothComponent>())        renderClothSection(dispatchEntity);  // F2H75
    }
    if (catActive(activeCat, "audio")) {
        if (dispatchEntity.hasComponent<AudioSourceComponent>())  renderAudioSourceSection(dispatchEntity);
    }
    if (catActive(activeCat, "animation")) {
        if (dispatchEntity.hasComponent<AnimatorComponent>())     renderAnimatorSection(dispatchEntity);
    }

    // F2H44 Bloque A + F3H29 polish: boton "+ Add Component" — solo en la
    // categoría "object" (home natural de cualquier entity, equivalente al
    // panel general de Unity). Antes aparecía al final de TODA categoría
    // no-scene-wide; el dev lo veía repetido en Object/Render/Physics/etc.
    // y se confundía. Con un único punto de entrada el mental model queda
    // claro: para sumar capacidades a una entity, voy a Object → "+".
    if (catActive(activeCat, "object")) {
        renderAddComponentSection(dispatchEntity);
    }

    ImGui::EndChild();
    ImGui::End();
}

// F3H22: barra de icons VERTICAL estilo Properties Editor de Blender.
// Columna lateral izquierda del Inspector. Cada icon solo se muestra si
// la entity tiene componentes de esa categoría — Object siempre presente
// (toda entity tiene Transform). Environment es scene-wide: siempre
// visible aunque la entity actual no tenga el componente — buscando el
// singleton en otra entity de la scene (mismo pattern que Blender World
// Properties). Categoría activa con background cyan (mismo lenguaje
// visual que el viewport render mode bar de F3H21).
void InspectorPanel::renderCategoryBar(Entity e) {
    const std::string activeCat = UserSettings::editor().inspectorActiveCategory;
    const ImU32 kActiveBg = IM_COL32(60, 140, 200, 255);
    constexpr float kBtnSize = 28.0f;  // botón cuadrado para layout vertical

    auto categoryButton = [&](const char* id, const char* icon,
                                const char* labelKey, const char* tooltipKey,
                                bool sceneWide) {
        // Visibilidad de cada icon:
        //   - scene-wide (Environment) SIEMPRE visible (Blender pattern —
        //     World Properties está incluso sin scene setup). Sin singleton,
        //     el body muestra mensaje "crear Environment".
        //   - Object requiere selección.
        //   - Otras categorías per-entity requieren selección + componente.
        bool visible;
        if (sceneWide) {
            visible = true;
        } else if (std::strcmp(id, "object") == 0) {
            visible = static_cast<bool>(e);
        } else {
            visible = e && entityHasCategory(e, id);
        }
        if (!visible) return;

        const bool isActive = (activeCat == id);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        kActiveBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kActiveBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kActiveBg);
        }
        // Label invisible único por id para evitar colisiones de ImGui.
        const std::string btnLabel = std::string(icon) + "##cat_" + id;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(kBtnSize, kBtnSize))) {
            auto ed = UserSettings::editor();
            ed.inspectorActiveCategory = id;
            UserSettings::setEditor(ed);
            UserSettings::save();
        }
        if (isActive) ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s",
                I18n::T(labelKey).c_str(),
                I18n::T(tooltipKey).c_str());
        }
        // Layout vertical: SIN SameLine — cada botón en su línea.
    };

    // 7 categorías en columna. Environment va al tope — es scene-wide y
    // siempre visible, igual que World Properties en Blender (siempre
    // accesible aunque no haya selección).
    categoryButton("environment", ICON_FA_GLOBE,
                    "editor.inspector.category.environment",
                    "editor.inspector.category.environment.tooltip", true);
    categoryButton("object",      ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
                    "editor.inspector.category.object",
                    "editor.inspector.category.object.tooltip", false);
    categoryButton("render",      ICON_FA_CUBE,
                    "editor.inspector.category.render",
                    "editor.inspector.category.render.tooltip", false);
    categoryButton("animation",   ICON_FA_PERSON_RUNNING,
                    "editor.inspector.category.animation",
                    "editor.inspector.category.animation.tooltip", false);
    categoryButton("audio",       ICON_FA_VOLUME_HIGH,
                    "editor.inspector.category.audio",
                    "editor.inspector.category.audio.tooltip", false);
    categoryButton("physics",     ICON_FA_BOLT,
                    "editor.inspector.category.physics",
                    "editor.inspector.category.physics.tooltip", false);
    categoryButton("gameplay",    ICON_FA_GAMEPAD,
                    "editor.inspector.category.gameplay",
                    "editor.inspector.category.gameplay.tooltip", false);
    // F3H28: 2 categorías scene-global nuevas. Siempre visibles (sceneWide=true),
    // no requieren selección ni singleton — sirven incluso en escena vacía
    // para configurar tools / crear grupos.
    categoryButton("groups",      ICON_FA_LAYER_GROUP,
                    "editor.inspector.category.groups",
                    "editor.inspector.category.groups.tooltip", true);
    categoryButton("maptools",    ICON_FA_SCREWDRIVER_WRENCH,
                    "editor.inspector.category.maptools",
                    "editor.inspector.category.maptools.tooltip", true);

    // Fallback: si la categoría activa NO está disponible y NO es
    // scene-wide ni global, auto-switch a "object" silencioso. Solo
    // cuando hay entity seleccionada (sin selección, dejamos la activa
    // para que el dev pueda ver el hint de "selecciona algo").
    if (e && activeCat != "environment" &&
        activeCat != "groups" && activeCat != "maptools" &&
        !entityHasCategory(e, activeCat.c_str())) {
        auto ed = UserSettings::editor();
        ed.inspectorActiveCategory = "object";
        UserSettings::setEditor(ed);
        // NO llamamos save() — auto-switch transitorio.
    }
}

// F2H81 → eliminado en F3H22: la barra "Plegar/Expandir todo" pierde
// sentido con categorías (cada categoría muestra pocos componentes,
// default colapsado en beginComponentSection).

void InspectorPanel::renderAddComponentSection(Entity e) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Centrado horizontal: calculamos width del boton + padding y
    // empujamos el cursor para que quede al medio.
    const std::string label = I18n::T("editor.panel.inspector.add.button");
    const ImVec2 textSz = ImGui::CalcTextSize(label.c_str());
    const float btnW = textSz.x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > btnW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW) * 0.5f);
    }
    if (ImGui::Button(label.c_str(), ImVec2(btnW, 0.0f))) {
        m_addComponentSearch[0] = '\0';  // reset search cada vez
        ImGui::OpenPopup("##add_component_popup");
    }

    drawAddComponentPopup(e);
}

void InspectorPanel::drawAddComponentPopup(Entity e) {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::BeginPopup("##add_component_popup", flags)) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.add.popup_title").c_str());
    ImGui::Separator();

    // F3H9: top item "Pegar <Tipo> como nuevo componente" — visible solo
    // cuando el clipboard tiene contenido Y la entidad NO tiene ese
    // componente. Reusa el clipboard de EditorUI poblado por el menu
    // contextual de beginComponentSection ("Copiar valores").
    if (m_ui != nullptr && m_assets != nullptr) {
        const auto& clip = m_ui->clipboardComponent();
        if (clip.has_value() &&
            !ComponentClipboard::entityHasComponent(clip->componentKey, e) &&
            ComponentClipboard::isSupported(clip->componentKey)) {
            const std::string typeNameKey =
                ComponentClipboard::componentNameKey(clip->componentKey);
            const std::string typeName = I18n::T(typeNameKey);
            const std::string pasteAsNewLabel =
                std::string(ICON_FA_PASTE " ") +
                I18n::T("editor.panel.inspector.add.paste_as_new", typeName);
            if (ImGui::Selectable(pasteAsNewLabel.c_str())) {
                auto cmd = std::make_unique<PasteComponentCommand>(
                    e, clip->componentKey,
                    nlohmann::json{},          // before vacio (no habia componente)
                    clip->payload,              // copy del payload
                    /*hadComponentBefore=*/false,
                    m_assets,
                    I18n::T("editor.panel.inspector.context.cmd_paste_as_new", typeName));
                HistoryStack* h = m_ui->historyStack();
                if (h != nullptr) {
                    h->push(std::move(cmd));
                } else {
                    cmd->execute();
                }
                m_editedThisFrame = true;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::Separator();
        }
    }

    // Search input. Auto-focus al primer frame del popup para typing
    // inmediato sin click extra.
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputTextWithHint("##add_search",
                              I18n::T("editor.panel.inspector.add.search").c_str(),
                              m_addComponentSearch,
                              sizeof(m_addComponentSearch));
    ImGui::Separator();

    // Spec de cada componente agregable: nombre + descripcion + grupo +
    // hash flag (`hasComponent<X>(e)`) + makeCmdFn que arma un
    // AddComponentCommand<T> tipado en T (F2H45 Bloque A: undoable).
    struct Item {
        const char* nameKey;
        const char* descKey;
        const char* catKey;
        bool        alreadyHas;
        std::function<std::unique_ptr<ICommand>(Entity, std::string)> makeCmdFn;
    };
    std::vector<Item> items;
    items.reserve(12);

    auto add = [&](const char* nameKey, const char* descKey,
                    const char* catKey, bool alreadyHas,
                    std::function<std::unique_ptr<ICommand>(Entity, std::string)> fn) {
        items.push_back({nameKey, descKey, catKey, alreadyHas, std::move(fn)});
    };

    // Render
    add("component.name.mesh_renderer", "component.desc.mesh_renderer",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<MeshRendererComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<MeshRendererComponent>(en, std::move(lbl));
        });
    add("component.name.light", "component.desc.light",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<LightComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<LightComponent>(en, std::move(lbl));
        });
    add("component.name.environment", "component.desc.environment",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<EnvironmentComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<EnvironmentComponent>(en, std::move(lbl));
        });
    add("component.name.animator", "component.desc.animator",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<AnimatorComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<AnimatorComponent>(en, std::move(lbl));
        });
    add("component.name.particle_emitter", "component.desc.particle_emitter",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<ParticleEmitterComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<ParticleEmitterComponent>(en, std::move(lbl));
        });
    add("component.name.camera", "component.desc.camera",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<CameraComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<CameraComponent>(en, std::move(lbl));
        });

    // Physics
    add("component.name.rigid_body", "component.desc.rigid_body",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<RigidBodyComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<RigidBodyComponent>(en, std::move(lbl));
        });
    add("component.name.trigger", "component.desc.trigger",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<TriggerComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<TriggerComponent>(en, std::move(lbl));
        });
    // F2H65: JointComponent en Physics.
    add("component.name.joint", "component.desc.joint",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<JointComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<JointComponent>(en, std::move(lbl));
        });
    // F2H72: ForceFieldComponent en Physics.
    add("component.name.force_field", "component.desc.force_field",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<ForceFieldComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<ForceFieldComponent>(en, std::move(lbl));
        });
    // F2H75: ClothComponent en Physics.
    add("component.name.cloth", "component.desc.cloth",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<ClothComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<ClothComponent>(en, std::move(lbl));
        });

    // Audio
    add("component.name.audio_source", "component.desc.audio_source",
        "editor.panel.inspector.add.cat.audio",
        e.hasComponent<AudioSourceComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<AudioSourceComponent>(en, std::move(lbl));
        });

    // Logic
    add("component.name.script", "component.desc.script",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<ScriptComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<ScriptComponent>(en, std::move(lbl));
        });
    add("component.name.nav_agent", "component.desc.nav_agent",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<NavAgentComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<NavAgentComponent>(en, std::move(lbl));
        });
    // F2H48: DialogComponent en Logic (mismo grupo que Script/NavAgent).
    add("component.name.dialog", "component.desc.dialog",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<DialogComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<DialogComponent>(en, std::move(lbl));
        });
    // F2H51: InventoryComponent en Logic.
    add("component.name.inventory", "component.desc.inventory",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<InventoryComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<InventoryComponent>(en, std::move(lbl));
        });
    // F2H52: ItemPickupComponent en Logic.
    add("component.name.item_pickup", "component.desc.item_pickup",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<ItemPickupComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<ItemPickupComponent>(en, std::move(lbl));
        });

    // World
    add("component.name.brush", "component.desc.brush",
        "editor.panel.inspector.add.cat.world",
        e.hasComponent<BrushComponent>(),
        [](Entity en, std::string lbl) {
            return makeAddComponentCommand<BrushComponent>(en, std::move(lbl));
        });

    // F3H9: componentKey derivado del nameKey (`component.name.light`
    // -> `light`). Usado para filtrar por EntityType (canAddComponent
    // descarta componentes que no tienen sentido para el type, ej.
    // BrushComponent en una Light).
    constexpr const char* kNamePrefix = "component.name.";
    constexpr size_t kNamePrefixLen = 15;  // strlen("component.name.")
    auto componentKeyFromNameKey = [&](const char* nameKey) -> std::string {
        const std::string s(nameKey);
        if (s.compare(0, kNamePrefixLen, kNamePrefix) == 0) {
            return s.substr(kNamePrefixLen);
        }
        return {};
    };

    // F3H9: leer el EntityType de la entity activa para filtrar.
    const EntityType entType = e.hasComponent<TagComponent>()
        ? e.getComponent<TagComponent>().entityType
        : EntityType::Generic;

    // F3H9: helper para disparar el AddComponentCommand desde un item del
    // popup. Centralizado porque la rama "submenus" y la rama "flat
    // search" lo llaman ambas.
    auto runAdd = [&](const Item& it) {
        const std::string nameTr = I18n::T(it.nameKey);
        std::string label = I18n::T("editor.cmd.add_component", nameTr);
        auto cmd = it.makeCmdFn(e, std::move(label));
        HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
        if (h != nullptr) {
            h->push(std::move(cmd));
        } else {
            cmd->execute();  // fallback defensivo sin history
        }
        m_editedThisFrame = true;
    };

    // F3H9 Stage 7: dos modos de render del popup.
    //
    //  (a) search vacio  → submenus Unity-style (Component > Rendering
    //      > Light). Mas claro cuando el type acepta muchas extensions
    //      (ej. Mesh tiene 15) — cada categoria se expande on-hover.
    //
    //  (b) search activo → lista flat con headers de categoria. Filtrar
    //      por nombre + ocultar la lista en submenus juntos serian
    //      contraintuitivos: el dev quiere ver coincidencias rapido.
    //
    // Filtro comun: solo items que la entity NO tiene Y son validos para
    // el EntityType (canAddComponent).
    std::vector<const Item*> filtered;
    filtered.reserve(items.size());
    for (auto& it : items) {
        if (it.alreadyHas) continue;
        const std::string componentKey = componentKeyFromNameKey(it.nameKey);
        if (!componentKey.empty() &&
            !EntityTypeTable::canAddComponent(entType, componentKey)) {
            continue;
        }
        filtered.push_back(&it);
    }

    const bool searching = m_addComponentSearch[0] != '\0';
    bool any = false;

    if (searching) {
        // Modo (b): flat con headers de categoria.
        const char* lastCat = nullptr;
        for (const auto* it : filtered) {
            const std::string nameTr = I18n::T(it->nameKey);
            if (!fuzzyMatch(nameTr, m_addComponentSearch)) continue;
            any = true;
            if (lastCat == nullptr ||
                std::strcmp(lastCat, it->catKey) != 0) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", I18n::T(it->catKey).c_str());
                lastCat = it->catKey;
            }
            if (ImGui::Selectable(nameTr.c_str(), false,
                                    ImGuiSelectableFlags_None,
                                    ImVec2(360.0f, 0.0f))) {
                runAdd(*it);
                ImGui::CloseCurrentPopup();
            }
            ImGui::Indent(16.0f);
            ImGui::TextDisabled("%s", I18n::T(it->descKey).c_str());
            ImGui::Unindent(16.0f);
        }
    } else {
        // Modo (a): submenus por categoria. Agrupamos sin reordenar para
        // preservar el orden de declaracion (Render > Physics > Audio >
        // Logic > World — orden semantico armado en `items` mas arriba).
        // `seenCats` evita renderizar 2 veces la misma categoria si los
        // items quedaron entrelazados.
        std::vector<const char*> seenCats;
        seenCats.reserve(8);
        for (const auto* it : filtered) {
            // Skip si ya rendereamos esta categoria en una iteracion
            // anterior (orden de declaracion ya la agrupo).
            bool already = false;
            for (const char* c : seenCats) {
                if (std::strcmp(c, it->catKey) == 0) { already = true; break; }
            }
            if (already) continue;
            seenCats.push_back(it->catKey);

            const std::string catTr = I18n::T(it->catKey);
            if (ImGui::BeginMenu(catTr.c_str())) {
                // Segundo loop sobre filtered: render todos los items de
                // ESTA categoria. Mantiene Selectable + descripcion gris.
                for (const auto* it2 : filtered) {
                    if (std::strcmp(it2->catKey, it->catKey) != 0) continue;
                    any = true;
                    const std::string nameTr = I18n::T(it2->nameKey);
                    if (ImGui::Selectable(nameTr.c_str(), false,
                                            ImGuiSelectableFlags_None,
                                            ImVec2(320.0f, 0.0f))) {
                        runAdd(*it2);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::Indent(16.0f);
                    ImGui::TextDisabled("%s", I18n::T(it2->descKey).c_str());
                    ImGui::Unindent(16.0f);
                }
                ImGui::EndMenu();
            }
        }
        // `any` se setea adentro del BeginMenu — si no se abrio ninguno,
        // significa que el set de items quedo vacio.
        if (!filtered.empty()) any = true;
    }

    if (!any) {
        ImGui::Spacing();
        if (searching) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.add.no_match",
                        std::string(m_addComponentSearch)).c_str());
        } else {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.add.empty").c_str());
        }
    }

    ImGui::EndPopup();
}

} // namespace Mood
