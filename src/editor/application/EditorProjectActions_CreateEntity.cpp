// F2H57 Bloque B: handler del workflow "Crear Entidad" estilo Hammer
// Editor. Consume el request del panel Escena, abre file picker del
// SO, importa el modelo al proyecto (copia a assets/meshes/ si esta
// afuera), carga via AssetManager y spawnea una entidad nueva con
// Transform + MeshRenderer (+ Animator/Skeleton si el rig es
// skinneado).
//
// Posicion del spawn: world origin con offset Y para que el bottom
// del AABB rotado quede al ras del piso (mismo criterio que
// processViewportMeshDrop F2H50). El dev mueve con el gizmo si quiere.
//
// Undo: pushCreatedEntities envuelve la creacion en un
// CreateEntityCommand del HistoryStack F2H27/32.

#include "core/Log.h"
#include "editor/application/EditorApplication.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"
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

// Reuso de helpers internos definidos en DemoSpawners_Drop.cpp para
// el bottom-anchor del AABB tras importRotationEuler. Declarados como
// inline locales aca para no exportarlos del archivo demo (que se
// elimina en Bloque E). Igual logica que el handler de drop.
namespace {

struct WorldYBoundsLocal {
    float minY = 0.0f;
    float maxY = 0.0f;
};

WorldYBoundsLocal rotatedAabbWorldY_local(const glm::vec3& aabbMin,
                                          const glm::vec3& aabbMax,
                                          const glm::vec3& importEuler) {
    const glm::vec3 corners[8] = {
        {aabbMin.x, aabbMin.y, aabbMin.z}, {aabbMax.x, aabbMin.y, aabbMin.z},
        {aabbMin.x, aabbMax.y, aabbMin.z}, {aabbMax.x, aabbMax.y, aabbMin.z},
        {aabbMin.x, aabbMin.y, aabbMax.z}, {aabbMax.x, aabbMin.y, aabbMax.z},
        {aabbMin.x, aabbMax.y, aabbMax.z}, {aabbMax.x, aabbMax.y, aabbMax.z},
    };
    const glm::mat4 rot =
        glm::rotate(glm::mat4(1.0f), importEuler.y, glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(1.0f), importEuler.x, glm::vec3(1, 0, 0)) *
        glm::rotate(glm::mat4(1.0f), importEuler.z, glm::vec3(0, 0, 1));
    WorldYBoundsLocal wy{1e30f, -1e30f};
    for (const auto& c : corners) {
        const glm::vec3 r = glm::vec3(rot * glm::vec4(c, 1.0f));
        wy.minY = std::min(wy.minY, r.y);
        wy.maxY = std::max(wy.maxY, r.y);
    }
    return wy;
}

} // namespace

void EditorApplication::processCreateEntityFromModelRequest() {
    if (!m_ui.consumeCreateEntityFromModelRequest()) return;
    if (!(m_scene && m_assetManager && m_project.has_value())) {
        Log::editor()->warn("[create_entity] Sin proyecto/escena/assets - skip");
        return;
    }

    // Punto de inicio del file picker: assets/meshes/ del proyecto
    // (donde live el catalogo importado), fallback al root del proyecto.
    std::filesystem::path startDir = m_project->root / "assets" / "meshes";
    if (!std::filesystem::exists(startDir)) {
        startDir = m_project->root;
    }

    const auto sel = pfd::open_file(
        "Elegir modelo para la entidad",
        startDir.string(),
        {"Modelos 3D (*.fbx *.obj *.gltf *.glb)",
         "*.fbx *.obj *.gltf *.glb",
         "Todos los archivos", "*"},
        pfd::opt::none).result();
    if (sel.empty()) {
        Log::editor()->info("[create_entity] cancelado");
        return;
    }

    const std::filesystem::path picked(sel.front());
    if (!std::filesystem::exists(picked)) {
        Log::editor()->warn("[create_entity] Archivo no existe: {}",
                              picked.generic_string());
        return;
    }

    // Resolver el path logico relativo al proyecto. Si el archivo esta
    // afuera del proyecto, copiarlo a assets/meshes/ y usar ese path.
    std::filesystem::path logicalPath;
    const auto rel = std::filesystem::relative(picked, m_project->root);
    const bool insideProject = !rel.empty() &&
        rel.native().find(L"..") == std::wstring::npos;
    if (insideProject) {
        logicalPath = rel;
    } else {
        // Copiar a assets/meshes/<filename>.
        const auto dstDir = m_project->root / "assets" / "meshes";
        std::error_code ec;
        std::filesystem::create_directories(dstDir, ec);
        const auto dst = dstDir / picked.filename();
        std::filesystem::copy_file(picked, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            Log::editor()->warn(
                "[create_entity] No se pudo copiar '{}' a '{}': {} - skip",
                picked.generic_string(), dst.generic_string(), ec.message());
            return;
        }
        logicalPath = std::filesystem::relative(dst, m_project->root);
        Log::editor()->info("[create_entity] modelo externo copiado a {}",
                              logicalPath.generic_string());
    }

    // Cargar el mesh via AssetManager. Si falla cae al missing-mesh
    // (cubo placeholder); el dev igual obtiene una entidad utilizable.
    const std::string logicalStr = logicalPath.generic_string();
    const MeshAssetId meshId = m_assetManager->loadMesh(logicalStr);
    const MeshAsset* asset = m_assetManager->getMesh(meshId);

    // Crear la entidad. Nombre = stem del archivo (npc.fbx -> "npc")
    // con sufijo incremental si ya existe en la escena.
    std::string baseName = picked.stem().generic_string();
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

    // Rotacion de import del MeshAsset (FBX Z-up etc.).
    const glm::vec3 importEuler = (asset != nullptr)
        ? asset->importRotationEuler : glm::vec3(0.0f);
    t.rotationEuler = importEuler;

    // Auto-escala bidireccional (mismo criterio F2H50 / processViewportMeshDrop):
    //   - height >3m: downscale a 1.5m (cm-units sin metros).
    //   - height <0.1m: upscale a 1.5m (cm-units que quedaron sub-metro).
    //   - 0.1..3m: sin autoscale (kenney barriles, props normales).
    f32 autoScale = 1.0f;
    WorldYBoundsLocal wy{};
    if (asset != nullptr) {
        wy = rotatedAabbWorldY_local(asset->aabbMin, asset->aabbMax, importEuler);
        const f32 height = wy.maxY - wy.minY;
        if (height > 3.0f) {
            autoScale = 1.5f / height;
        } else if (height > 0.001f && height < 0.1f) {
            autoScale = 1.5f / height;
        }
    }
    t.scale = glm::vec3(autoScale);

    // Posicion: world origin XZ; Y para anclar el bottom del AABB al piso.
    const f32 yFloorOffset = -autoScale * wy.minY;
    t.position = glm::vec3(0.0f, yFloorOffset, 0.0f);

    // MeshRenderer con materiales generados.
    auto mats = m_assetManager->createMaterialsForMesh(meshId);
    e.addComponent<MeshRendererComponent>(meshId, std::move(mats));

    // F2H50: si tiene esqueleto, auto-agregar Animator + Skeleton.
    if (asset != nullptr && asset->hasSkeleton()) {
        AnimatorComponent anim{};
        anim.clipName = "";
        anim.playing  = true;
        anim.loop     = true;
        // El attach de external clips por filename sibling se maneja
        // en otro helper (DemoSpawners_Drop.cpp); para F2H57 v1 se
        // omite y el dev puede agregar clips a mano via Inspector si
        // los necesita. Si emerge demanda, extraer el helper aparte.
        e.addComponent<AnimatorComponent>(anim);
        e.addComponent<SkeletonComponent>(SkeletonComponent{});
    }

    Log::editor()->info(
        "[create_entity] Spawned '{}' from '{}' (mesh id {}, scale={:.3f}{})",
        finalName, logicalStr, meshId, autoScale,
        (asset && asset->hasSkeleton()) ? ", skinned" : "");

    // Seleccionar la nueva entidad para que el dev la vea en el
    // Inspector inmediatamente.
    replaceWithSingle(m_ui.selectionSet(), e);

    pushCreatedEntities({e}, std::string("Crear entidad '") + finalName + "'");
}

// ============================================================================
// F2H57 followup: spawn de entidad con mesh placeholder
//
// Workflow Hammer: el dev pide "una entidad nueva" sin browsing de
// archivos. Spawnea con el missing-mesh (cubo placeholder) del
// AssetManager. El dev luego cambia el mesh via Inspector o pasa al
// modal "Cambiar tipo" para convertirla en NPC / luz / item / etc.
// ============================================================================

void EditorApplication::processCreateEntityPlaceholderRequest() {
    if (!m_ui.consumeCreateEntityPlaceholderRequest()) return;
    if (!(m_scene && m_assetManager)) {
        Log::editor()->warn("[create_entity_placeholder] Sin escena/assets - skip");
        return;
    }

    // Nombre base + sufijo incremental.
    const std::string baseName = "Entity";
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
    t.position = glm::vec3(0.0f, 0.5f, 0.0f); // medio cubo arriba del piso
    t.scale    = glm::vec3(1.0f);

    // MeshRenderer con missing-mesh (cubo placeholder del AssetManager).
    const MeshAssetId placeholderId = m_assetManager->missingMeshId();
    auto mats = m_assetManager->createMaterialsForMesh(placeholderId);
    e.addComponent<MeshRendererComponent>(placeholderId, std::move(mats));

    Log::editor()->info(
        "[create_entity_placeholder] Spawned '{}' con placeholder cube",
        finalName);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear entidad '") + finalName + "'");
}

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

// ============================================================================
// F2H57 Bloque D: Modal "Convertir entidad"
//
// Patron Hammer Editor: right-click sobre una entidad -> elegir un "kit"
// preset que aplica componentes especificos (NPC con dialogo, Player,
// Luz, etc.). Los kits SON aditivos: agregan componentes sin remover los
// existentes. Si el dev quiere bajar a entidad vacia, usa Inspector
// para remover componentes individualmente.
//
// v1 ships 4 kits: NPC con dialogo / Player / Luz puntual / Luz
// direccional / Item pickeable. Mas kits (Camera, Quest runner) y
// undo del paso convert quedan como follow-up sub-blocks. Los ediits
// individuales de componentes post-convert SI son undoable via
// Inspector (F2H32 InspectorEditCommand).
// ============================================================================

void EditorApplication::renderConvertEntityModal() {
    // Latch del target cuando llega un request nuevo.
    entt::entity req;
    if (m_ui.consumeEntityConvertModalRequest(req)) {
        m_convertModalActiveTarget = req;
        ImGui::OpenPopup("convert_entity_modal");
    }

    // Modal solo visible mientras hay target activo.
    if (m_convertModalActiveTarget == entt::null) return;

    // Centrar.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->GetCenter().x, vp->GetCenter().y),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Appearing);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!ImGui::BeginPopupModal("convert_entity_modal", nullptr, flags)) {
        return;
    }

    if (m_scene == nullptr) {
        ImGui::TextUnformatted("Sin escena activa.");
        if (ImGui::Button("Cerrar")) {
            m_convertModalActiveTarget = entt::null;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    Entity target(m_convertModalActiveTarget, m_scene.get());
    if (!target.hasComponent<TagComponent>()) {
        // Entidad invalida o destruida entre frames.
        m_convertModalActiveTarget = entt::null;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const std::string& tagName = target.getComponent<TagComponent>().name;
    ImGui::Text("%s", I18n::T("editor.convert_modal.title", tagName).c_str());
    ImGui::TextDisabled("%s",
        I18n::T("editor.convert_modal.subtitle").c_str());
    ImGui::Separator();

    // Helper para mostrar boton de kit que solo se habilita si el componente
    // requerido no esta ya presente.
    auto kitButton = [&](const char* labelKey, const char* descKey, bool alreadyHas) -> bool {
        const std::string label = I18n::T(labelKey);
        ImGui::BeginGroup();
        if (alreadyHas) ImGui::BeginDisabled();
        const bool clicked = ImGui::Button(label.c_str(), ImVec2(-1.0f, 0.0f));
        if (alreadyHas) ImGui::EndDisabled();
        ImGui::TextDisabled("%s%s",
            alreadyHas ? "[ya aplicado] " : "",
            I18n::T(descKey).c_str());
        ImGui::Spacing();
        ImGui::EndGroup();
        return clicked && !alreadyHas;
    };

    bool applied = false;

    // --- Kit 1: NPC con dialogo ---
    const bool hasDialog = target.hasComponent<DialogComponent>();
    if (kitButton("editor.convert_modal.kit.npc",
                  "editor.convert_modal.kit.npc_desc",
                  hasDialog)) {
        if (!target.hasComponent<TriggerComponent>()) {
            target.addComponent<TriggerComponent>(TriggerComponent{});
        }
        DialogComponent dlg{};
        dlg.dialogPath = "dialogs/example.mooddialog";
        dlg.autoStartOnInteract = true;
        target.addComponent<DialogComponent>(dlg);
        Log::editor()->info("[convert] '{}' -> NPC con dialogo (path placeholder)",
                              tagName);
        applied = true;
    }

    // --- Kit 2: Item pickeable ---
    const bool hasPickup = target.hasComponent<ItemPickupComponent>();
    if (kitButton("editor.convert_modal.kit.item",
                  "editor.convert_modal.kit.item_desc",
                  hasPickup)) {
        if (!target.hasComponent<TriggerComponent>()) {
            target.addComponent<TriggerComponent>(TriggerComponent{});
        }
        ItemPickupComponent pickup{};
        pickup.itemPath = "items/example.mooditem";
        pickup.quantity = 1;
        pickup.destroyOnPickup = true;
        target.addComponent<ItemPickupComponent>(pickup);
        Log::editor()->info("[convert] '{}' -> Item pickeable (path placeholder)",
                              tagName);
        applied = true;
    }

    // --- Kit 3: Luz puntual ---
    const bool hasLight = target.hasComponent<LightComponent>();
    if (kitButton("editor.convert_modal.kit.point_light",
                  "editor.convert_modal.kit.point_light_desc",
                  hasLight)) {
        LightComponent light{};
        light.type = LightComponent::Type::Point;
        light.color = glm::vec3(1.0f, 0.9f, 0.7f);
        light.intensity = 1.0f;
        light.radius = 10.0f;
        light.enabled = true;
        target.addComponent<LightComponent>(light);
        Log::editor()->info("[convert] '{}' -> Luz puntual", tagName);
        applied = true;
    }

    // --- Kit 4: Luz direccional ---
    if (kitButton("editor.convert_modal.kit.dir_light",
                  "editor.convert_modal.kit.dir_light_desc",
                  hasLight)) {
        LightComponent light{};
        light.type = LightComponent::Type::Directional;
        light.color = glm::vec3(1.0f);
        light.intensity = 1.0f;
        light.direction = glm::vec3(-0.3f, -1.0f, -0.2f);
        light.castShadows = true;
        light.enabled = true;
        target.addComponent<LightComponent>(light);
        Log::editor()->info("[convert] '{}' -> Luz direccional", tagName);
        applied = true;
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s", I18n::T("editor.convert_modal.note_undo").c_str());
    ImGui::Spacing();

    if (ImGui::Button(I18n::T("editor.convert_modal.close").c_str(), ImVec2(120.0f, 0.0f))) {
        m_convertModalActiveTarget = entt::null;
        ImGui::CloseCurrentPopup();
    }

    if (applied) {
        markDirty();
        // No cerramos al aplicar: el dev puede agregar varios kits al
        // mismo target en una sola pasada (Item + NPC + Luz puntual).
    }

    ImGui::EndPopup();
}

} // namespace Mood
