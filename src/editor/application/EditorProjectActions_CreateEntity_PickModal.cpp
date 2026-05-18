// EditorApplication::renderPickFromLoadedMeshesModal — modal SFM-style
// "Elegir mesh del proyecto" + TabBar (Meshes / Primitivas / Luces).
//
// Extraido de `EditorProjectActions_CreateEntity.cpp` en AUDIT-2
// (2026-05-17) para mantener ese archivo bajo el hard cap de 800 LOC.
// El helper `rotatedAabbWorldY_local` se movio a
// `EditorProjectActions_CreateEntity_Internal.h` para compartirlo.

#include "core/Log.h"
#include "editor/application/EditorApplication.h"
#include "editor/application/EditorProjectActions_CreateEntity_Internal.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Mood {

using CreateEntityHelpers::WorldYBoundsLocal;
using CreateEntityHelpers::rotatedAabbWorldY_local;

// ============================================================================
// F2H57 followup: Modal "Elegir mesh del proyecto" (estilo SFM)
//
// Lista los meshes ya cargados en el AssetManager. El dev clickea uno
// y spawnea una entidad con ese mesh. No abre file picker del SO -
// cubre el caso "ya tengo modelos importados, quiero reusarlos sin
// re-navegar el filesystem". Convencion Source Film Maker / Asset
// Browser de Unity-Unreal.
// ============================================================================

void EditorApplication::renderPickFromLoadedMeshesModal() {
    // F2H59: el popup ID interno de ImGui sigue siendo "pick_mesh_modal"
    // (estable para layout/persistence); el title visible se compone con
    // sufijo `##pick_mesh_modal` para que ImGui muestre el label traducido
    // pero matchee el OpenPopup contra el mismo ID. Si cambiamos el ID,
    // BeginPopupModal no abre.
    constexpr const char* kPopupId = "##pick_mesh_modal";
    const std::string popupTitle =
        I18n::T("editor.pick_mesh_modal.window_title") + kPopupId;

    if (m_ui.consumePickFromLoadedMeshesRequest()) {
        m_pickMeshModalActive = true;
        ImGui::OpenPopup(popupTitle.c_str());
    }

    if (!m_pickMeshModalActive) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->GetCenter().x, vp->GetCenter().y),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620.0f, 480.0f), ImGuiCond_Appearing);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse;

    // F2H59: pasar &m_pickMeshModalActive como p_open hace que ImGui
    // pinte la X arriba a la derecha. Al cliquear, ImGui setea el bool
    // a false; nosotros llamamos CloseCurrentPopup en el siguiente
    // chequeo abajo. Reemplaza al boton "Cerrar" inferior pre-F2H59.
    if (!ImGui::BeginPopupModal(popupTitle.c_str(), &m_pickMeshModalActive, flags)) {
        return;
    }

    // Si el dev clickeo la X, ImGui ya seteo m_pickMeshModalActive=false;
    // cerramos el popup y salimos limpio.
    if (!m_pickMeshModalActive) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    if (!(m_scene && m_assetManager)) {
        ImGui::TextUnformatted("Sin escena/assets.");
        ImGui::EndPopup();
        return;
    }

    ImGui::TextUnformatted(I18n::T("editor.pick_mesh_modal.title").c_str());
    ImGui::TextDisabled("%s", I18n::T("editor.pick_mesh_modal.subtitle").c_str());
    ImGui::Separator();

    // F2H59: TabBar con dos sub-secciones. "Meshes del proyecto" mantiene
    // el flujo SFM original; "Primitivas" agrupa los brushes CSG (Plano /
    // Quad / Box / Cylinder / Sphere / Cone / Capsule / Pyramid / Wedge /
    // Prism tri / Prism hex) que antes vivian en el menu top-level
    // Brush > Anadir y en el Toolbar. Punto unico de "como creo geometria".
    if (ImGui::BeginTabBar("##create_entity_tabs")) {

    // F2H59 fix: altura uniforme del contenido de las tabs para que el
    // footer del modal quede siempre en la misma posicion vertical sin
    // importar que tab esta activa. Pedido del dev: "en primitivas no
    // esta el footer bien posicionado".
    constexpr float kTabContentHeight = 320.0f;

    if (ImGui::BeginTabItem(I18n::T("editor.pick_mesh_modal.tab_meshes").c_str())) {
    // Listado: itera meshIds [1..count). Skip id 0 = missing-mesh
    // (ya es accesible via "New (empty)").
    const usize meshCount = m_assetManager->meshCount();
    if (meshCount <= 1) {
        ImGui::BeginChild("##mesh_empty", ImVec2(0.0f, kTabContentHeight), false);
        ImGui::TextDisabled("%s", I18n::T("editor.pick_mesh_modal.empty").c_str());
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("##mesh_list", ImVec2(0.0f, kTabContentHeight), true);
        MeshAssetId selectedToSpawn = 0;
        for (usize i = 1; i < meshCount; ++i) {
            const auto id = static_cast<MeshAssetId>(i);
            const std::string path = m_assetManager->meshPathOf(id);
            // Skip primitivos sintetizados (sphere/cube generados en ctor).
            if (path.rfind("__", 0) == 0) continue;
            ImGui::PushID(static_cast<int>(id));
            char buf[512];
            std::snprintf(buf, sizeof(buf), "%s  (id %u)", path.c_str(), id);
            if (ImGui::Selectable(buf, false,
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
                selectedToSpawn = id;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (selectedToSpawn != 0) {
            // Reusar la logica de spawn de processCreateEntityFromModelRequest
            // pero con el meshId ya conocido.
            const MeshAsset* asset = m_assetManager->getMesh(selectedToSpawn);
            const std::string logicalStr =
                m_assetManager->meshPathOf(selectedToSpawn);
            const std::filesystem::path logicalPath(logicalStr);
            std::string baseName = logicalPath.stem().generic_string();
            if (baseName.empty()) baseName = "Entity";
            std::string finalName = baseName;
            int suffix = 1;
            while (true) {
                bool collision = false;
                m_scene->forEach<TagComponent>(
                    [&](Entity, TagComponent& tag) {
                        if (tag.name == finalName) collision = true;
                    });
                if (!collision) break;
                ++suffix;
                finalName = baseName + "_" + std::to_string(suffix);
            }

            Entity e = m_scene->createEntity(finalName);
            auto& t = e.getComponent<TransformComponent>();
            const glm::vec3 importEuler = (asset != nullptr)
                ? asset->importRotationEuler : glm::vec3(0.0f);
            t.rotationEuler = importEuler;

            f32 autoScale = 1.0f;
            WorldYBoundsLocal wy{};
            if (asset != nullptr) {
                wy = rotatedAabbWorldY_local(asset->aabbMin, asset->aabbMax, importEuler);
                const f32 height = wy.maxY - wy.minY;
                if (height > 3.0f) autoScale = 1.5f / height;
                else if (height > 0.001f && height < 0.1f) autoScale = 1.5f / height;
            }
            t.scale = glm::vec3(autoScale);
            const f32 yFloorOffset = -autoScale * wy.minY;
            t.position = glm::vec3(0.0f, yFloorOffset, 0.0f);

            auto mats = m_assetManager->createMaterialsForMesh(selectedToSpawn);
            e.addComponent<MeshRendererComponent>(selectedToSpawn, std::move(mats));

            if (asset != nullptr && asset->hasSkeleton()) {
                AnimatorComponent anim{};
                anim.playing = true;
                anim.loop = true;
                e.addComponent<AnimatorComponent>(anim);
                e.addComponent<SkeletonComponent>(SkeletonComponent{});
            }

            Log::editor()->info(
                "[pick_mesh] Spawned '{}' from project mesh '{}' (id {})",
                finalName, logicalStr, selectedToSpawn);

            replaceWithSingle(m_ui.selectionSet(), e);
            pushCreatedEntities({e}, std::string("Crear entidad '") + finalName + "'");

            m_pickMeshModalActive = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndTabItem();
            ImGui::EndTabBar();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndTabItem();
    } // end TabItem "Meshes"

    // F2H60 polish iter2: tab "Luces". Spawn directo de Directional /
    // Point con defaults sensatos (Directional con castShadows ON --
    // funciona out-of-the-box). El icono de la luz en el overlay del
    // editor la hace visible aunque la entidad no tenga mesh.
    if (ImGui::BeginTabItem(I18n::T("editor.pick_mesh_modal.tab_lights").c_str())) {
        ImGui::BeginChild("##lights_grid", ImVec2(0.0f, kTabContentHeight), false);
        ImGui::TextDisabled("%s",
            I18n::T("editor.pick_mesh_modal.lights_hint").c_str());
        ImGui::Spacing();

        struct LightSpec { const char* labelKey; ProjectAction action; };
        constexpr LightSpec kLights[] = {
            { "editor.menu.light.directional", ProjectAction::AddDirectionalLight },
            { "editor.menu.light.point",       ProjectAction::AddPointLight       },
        };
        constexpr int kLightCount = static_cast<int>(sizeof(kLights) / sizeof(kLights[0]));
        constexpr float kBtnW = 220.0f;
        constexpr float kBtnH = 56.0f;

        ProjectAction pendingAction = static_cast<ProjectAction>(-1);
        bool actionPicked = false;
        for (int i = 0; i < kLightCount; ++i) {
            if (i > 0) ImGui::SameLine();
            const std::string label = I18n::T(kLights[i].labelKey);
            if (ImGui::Button(label.c_str(), ImVec2(kBtnW, kBtnH))) {
                pendingAction = kLights[i].action;
                actionPicked = true;
            }
        }

        if (actionPicked) {
            m_ui.requestProjectAction(pendingAction);
            m_pickMeshModalActive = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndChild();
            ImGui::EndTabItem();
            ImGui::EndTabBar();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    // F2H59: tab "Primitivas". Cada boton dispara el ProjectAction
    // correspondiente y cierra el modal. Grid 3-columns para que entren
    // las 11 primitivas sin scroll.
    if (ImGui::BeginTabItem(I18n::T("editor.pick_mesh_modal.tab_primitives").c_str())) {
        ImGui::BeginChild("##primitives_grid", ImVec2(0.0f, kTabContentHeight), false);
        ImGui::TextDisabled("%s",
            I18n::T("editor.pick_mesh_modal.primitives_hint").c_str());
        ImGui::Spacing();

        struct PrimSpec { const char* labelKey; ProjectAction action; };
        constexpr PrimSpec kPrims[] = {
            { "editor.menu.brush.plane",     ProjectAction::AddPlaneBrush             },
            { "editor.menu.brush.quad",      ProjectAction::AddQuadBrush              },
            { "editor.menu.brush.box",       ProjectAction::AddBoxBrush               },
            { "editor.menu.brush.cylinder",  ProjectAction::AddCylinderBrush          },
            { "editor.menu.brush.sphere",    ProjectAction::AddSphereBrush            },
            { "editor.menu.brush.cone",      ProjectAction::AddConeBrush              },
            { "editor.menu.brush.capsule",   ProjectAction::AddCapsuleBrush           },
            { "editor.menu.brush.pyramid",   ProjectAction::AddPyramidBrush           },
            { "editor.menu.brush.wedge",     ProjectAction::AddWedgeBrush             },
            { "editor.menu.brush.prism_tri", ProjectAction::AddPrismTriangularBrush   },
            { "editor.menu.brush.prism_hex", ProjectAction::AddPrismHexagonalBrush    },
        };
        constexpr int kPrimCount = static_cast<int>(sizeof(kPrims) / sizeof(kPrims[0]));
        constexpr int kCols = 3;
        constexpr float kBtnW = 180.0f;
        constexpr float kBtnH = 40.0f;

        ProjectAction pendingAction = static_cast<ProjectAction>(-1);
        bool actionPicked = false;
        for (int i = 0; i < kPrimCount; ++i) {
            if (i % kCols != 0) ImGui::SameLine();
            const std::string label = I18n::T(kPrims[i].labelKey);
            if (ImGui::Button(label.c_str(), ImVec2(kBtnW, kBtnH))) {
                pendingAction = kPrims[i].action;
                actionPicked = true;
            }
        }

        if (actionPicked) {
            m_ui.requestProjectAction(pendingAction);
            m_pickMeshModalActive = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndChild();
            ImGui::EndTabItem();
            ImGui::EndTabBar();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    } // end TabBar

    // F2H59: footer del modal estilo "engine generico" (Unreal Content
    // Browser / Unity Asset Window): los 2 botones primarios siempre
    // presentes ("Importar..." abre file picker del SO, "Nuevo (vacio)"
    // crea una entidad placeholder). Cerrar via X arriba (sacamos el
    // boton "Cerrar" inferior pre-F2H59 -- convencion ventana estandar).
    ImGui::Separator();
    if (ImGui::Button(I18n::T("editor.pick_mesh_modal.import").c_str(),
                       ImVec2(180.0f, 0.0f))) {
        m_pickMeshModalActive = false;
        ImGui::CloseCurrentPopup();
        m_ui.requestCreateEntityFromModel();
        ImGui::EndPopup();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.pick_mesh_modal.new_empty").c_str(),
                       ImVec2(180.0f, 0.0f))) {
        m_pickMeshModalActive = false;
        ImGui::CloseCurrentPopup();
        m_ui.requestCreateEntityPlaceholder();
        ImGui::EndPopup();
        return;
    }

    ImGui::EndPopup();
}

} // namespace Mood
