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
#include "editor/ui/IconsFontAwesome6.h"  // F2H80: íconos de luz
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/preview/MeshThumbnailRenderer.h"  // F2H80
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glad/gl.h>  // F2H80: GLuint del thumbnail
#include <imgui.h>
#include <portable-file-dialogs.h>

#include <algorithm>  // F2H80: std::max para el cálculo de columnas
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

    // F2H80: NoResize — es un diálogo de tamaño fijo, no una ventana de
    // trabajo. El dev no debería poder estirarlo.
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

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
        ImGui::TextUnformatted(
            I18n::T("editor.modal.common.no_scene_assets").c_str());
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
        // F2H80: grilla de cards con miniatura 3D del modelo real (estilo SFM /
        // Unreal Content Browser). Cae a un botón con el nombre si no hay
        // thumbnail renderer / falla el render.
        ImGui::BeginChild("##mesh_list", ImVec2(0.0f, kTabContentHeight), true);
        MeshAssetId selectedToSpawn = 0;
        constexpr float kThumb = 88.0f;
        const float availW = ImGui::GetContentRegionAvail().x;
        const float cell = kThumb + 12.0f;
        const int cols = std::max(1, static_cast<int>(availW / cell));
        int drawn = 0;
        for (usize i = 1; i < meshCount; ++i) {
            const auto id = static_cast<MeshAssetId>(i);
            const std::string path = m_assetManager->meshPathOf(id);
            // Skip primitivos sintetizados (sphere/cube generados en ctor).
            if (path.rfind("__", 0) == 0) continue;

            const std::string fileName =
                std::filesystem::path(path).filename().generic_string();
            const GLuint thumb = (m_meshThumbnails != nullptr)
                ? m_meshThumbnails->thumbnailFor(id, *m_assetManager) : 0u;

            ImGui::PushID(static_cast<int>(id));
            ImGui::BeginGroup();
            bool clicked = false;
            if (thumb != 0u) {
                // FBO color texture: bottom-up → uv flip (0,1)-(1,0).
                clicked = ImGui::ImageButton("##meshthumb",
                                (ImTextureID)(uintptr_t)thumb,
                                ImVec2(kThumb, kThumb),
                                ImVec2(0, 1), ImVec2(1, 0));
            } else {
                clicked = ImGui::Button(fileName.c_str(), ImVec2(kThumb, kThumb));
            }
            if (clicked) selectedToSpawn = id;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());

            // Label truncado al ancho de la card.
            const float textW = ImGui::CalcTextSize(fileName.c_str()).x;
            if (textW <= kThumb) {
                ImGui::TextUnformatted(fileName.c_str());
            } else {
                std::string truncated = fileName;
                while (!truncated.empty() &&
                        ImGui::CalcTextSize((truncated + "..").c_str()).x > kThumb) {
                    truncated.pop_back();
                }
                ImGui::Text("%s..", truncated.c_str());
            }
            ImGui::EndGroup();
            ImGui::PopID();

            if (static_cast<int>((drawn + 1) % cols) != 0) ImGui::SameLine();
            ++drawn;
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
            e.getComponent<TagComponent>().entityType = EntityType::Mesh;  // F3H9

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

        // F2H80: cards con ícono (una luz no tiene modelo 3D que renderizar —
        // el estándar de los engines es un ícono claro: sol = direccional,
        // foco = puntual).
        // F3H22: la card "Environment" del modal Add Entity fue removida —
        // el Environment ahora es auto-spawn al cargar/crear scene (Blender
        // World Properties pattern) y no se puede borrar. Ofrecer "Crear
        // Environment" en este modal era confuso (siempre disabled).
        struct LightSpec { const char* labelKey; const char* icon; ProjectAction action; bool disabled; };
        LightSpec kLights[] = {
            { "editor.menu.light.directional", ICON_FA_SUN,       ProjectAction::AddDirectionalLight, false },
            { "editor.menu.light.point",       ICON_FA_LIGHTBULB, ProjectAction::AddPointLight,       false },
        };
        constexpr int kLightCount = static_cast<int>(sizeof(kLights) / sizeof(kLights[0]));
        constexpr float kCard = 96.0f;

        ProjectAction pendingAction = static_cast<ProjectAction>(-1);
        bool actionPicked = false;
        for (int i = 0; i < kLightCount; ++i) {
            if (i > 0) ImGui::SameLine();
            const std::string label = I18n::T(kLights[i].labelKey);
            ImGui::PushID(i);
            ImGui::BeginGroup();
            // Ícono grande centrado (font scale 2.6x) como label de una card.
            if (kLights[i].disabled) ImGui::BeginDisabled();
            ImGui::SetWindowFontScale(2.6f);
            const bool clicked = ImGui::Button(kLights[i].icon, ImVec2(kCard, kCard));
            ImGui::SetWindowFontScale(1.0f);
            if (kLights[i].disabled) ImGui::EndDisabled();
            if (clicked) { pendingAction = kLights[i].action; actionPicked = true; }
            if (ImGui::IsItemHovered()) {
                const char* tipKey = kLights[i].disabled
                    ? "editor.menu.world.environment_already_present"
                    : kLights[i].labelKey;
                ImGui::SetTooltip("%s", I18n::T(tipKey).c_str());
            }

            const float textW = ImGui::CalcTextSize(label.c_str()).x;
            if (textW <= kCard) {
                ImGui::TextUnformatted(label.c_str());
            } else {
                std::string truncated = label;
                while (!truncated.empty() &&
                        ImGui::CalcTextSize((truncated + "..").c_str()).x > kCard) {
                    truncated.pop_back();
                }
                ImGui::Text("%s..", truncated.c_str());
            }
            ImGui::EndGroup();
            ImGui::PopID();
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

    // F4H1+B: tab "Gameplay". Items con HealthComponent / componentes de
    // combate. Maniquí (F4H1) + Arma vacía (F4H2 Bloque B). Futuros (F4H6+):
    // enemigos data-driven.
    if (ImGui::BeginTabItem(I18n::T("editor.pick_mesh_modal.tab_gameplay").c_str())) {
        ImGui::BeginChild("##gameplay_grid", ImVec2(0.0f, kTabContentHeight), false);
        ImGui::TextDisabled("%s",
            I18n::T("editor.pick_mesh_modal.gameplay_hint").c_str());
        ImGui::Spacing();

        constexpr float kCard = 96.0f;

        struct GameplayCard {
            const char*   labelKey;
            const char*   tooltipKey;
            const char*   icon;
            ProjectAction action;
            const char*   pushId;
        };
        // F4H2 Bloque B follow-up: card "Weapon empty" removida — el Player
        // ya viene con arma auto-asignada. Pickups con mesh+trigger son F4H4.
        const GameplayCard kCards[] = {
            { "editor.menu.gameplay.player", "editor.menu.gameplay.player.tooltip",
              ICON_FA_USER,                    ProjectAction::AddPlayer, "##player_card" },
            { "editor.menu.gameplay.dummy",  "editor.menu.gameplay.dummy.tooltip",
              ICON_FA_GAMEPAD,                 ProjectAction::AddDummy,  "##dummy_card"  },
            { "editor.menu.gameplay.enemy",  "editor.menu.gameplay.enemy.tooltip",
              ICON_FA_SKULL,                   ProjectAction::AddEnemy,  "##enemy_card"  },
        };
        constexpr int kCardCount = static_cast<int>(sizeof(kCards) / sizeof(kCards[0]));

        ProjectAction pendingAction = static_cast<ProjectAction>(-1);
        bool actionPicked = false;
        for (int i = 0; i < kCardCount; ++i) {
            if (i > 0) ImGui::SameLine();
            const std::string label = I18n::T(kCards[i].labelKey);
            ImGui::PushID(kCards[i].pushId);
            ImGui::BeginGroup();
            ImGui::SetWindowFontScale(2.6f);
            const bool clicked = ImGui::Button(kCards[i].icon, ImVec2(kCard, kCard));
            ImGui::SetWindowFontScale(1.0f);
            if (clicked) { pendingAction = kCards[i].action; actionPicked = true; }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", I18n::T(kCards[i].tooltipKey).c_str());
            }
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndGroup();
            ImGui::PopID();
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

        // F2H80: cards con miniatura 3D real de cada primitiva (el renderer
        // construye el brush + mesh una vez y cachea). Cae a un botón con el
        // nombre si no hay thumbnail renderer.
        using PK = MeshThumbnailRenderer::PrimitiveKind;
        struct PrimSpec { const char* labelKey; ProjectAction action; PK kind; };
        const PrimSpec kPrims[] = {
            { "editor.menu.brush.plane",     ProjectAction::AddPlaneBrush,           PK::Plane    },
            { "editor.menu.brush.quad",      ProjectAction::AddQuadBrush,            PK::Quad     },
            { "editor.menu.brush.box",       ProjectAction::AddBoxBrush,             PK::Box      },
            { "editor.menu.brush.cylinder",  ProjectAction::AddCylinderBrush,        PK::Cylinder },
            { "editor.menu.brush.sphere",    ProjectAction::AddSphereBrush,          PK::Sphere   },
            { "editor.menu.brush.cone",      ProjectAction::AddConeBrush,            PK::Cone     },
            { "editor.menu.brush.capsule",   ProjectAction::AddCapsuleBrush,         PK::Capsule  },
            { "editor.menu.brush.pyramid",   ProjectAction::AddPyramidBrush,         PK::Pyramid  },
            { "editor.menu.brush.wedge",     ProjectAction::AddWedgeBrush,           PK::Wedge    },
            { "editor.menu.brush.prism_tri", ProjectAction::AddPrismTriangularBrush, PK::PrismTri },
            { "editor.menu.brush.prism_hex", ProjectAction::AddPrismHexagonalBrush,  PK::PrismHex },
        };
        constexpr int kPrimCount = static_cast<int>(sizeof(kPrims) / sizeof(kPrims[0]));
        constexpr float kThumb = 88.0f;
        const float availW = ImGui::GetContentRegionAvail().x;
        const int cols = std::max(1, static_cast<int>(availW / (kThumb + 12.0f)));

        ProjectAction pendingAction = static_cast<ProjectAction>(-1);
        bool actionPicked = false;
        for (int i = 0; i < kPrimCount; ++i) {
            const std::string label = I18n::T(kPrims[i].labelKey);
            const GLuint thumb = (m_meshThumbnails != nullptr)
                ? m_meshThumbnails->thumbnailForPrimitive(kPrims[i].kind, *m_assetManager)
                : 0u;

            ImGui::PushID(i);
            ImGui::BeginGroup();
            bool clicked = false;
            if (thumb != 0u) {
                clicked = ImGui::ImageButton("##primthumb",
                                (ImTextureID)(uintptr_t)thumb,
                                ImVec2(kThumb, kThumb), ImVec2(0, 1), ImVec2(1, 0));
            } else {
                clicked = ImGui::Button(label.c_str(), ImVec2(kThumb, kThumb));
            }
            if (clicked) { pendingAction = kPrims[i].action; actionPicked = true; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label.c_str());

            const float textW = ImGui::CalcTextSize(label.c_str()).x;
            if (textW <= kThumb) {
                ImGui::TextUnformatted(label.c_str());
            } else {
                std::string truncated = label;
                while (!truncated.empty() &&
                        ImGui::CalcTextSize((truncated + "..").c_str()).x > kThumb) {
                    truncated.pop_back();
                }
                ImGui::Text("%s..", truncated.c_str());
            }
            ImGui::EndGroup();
            ImGui::PopID();

            if (static_cast<int>((i + 1) % cols) != 0) ImGui::SameLine();
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
