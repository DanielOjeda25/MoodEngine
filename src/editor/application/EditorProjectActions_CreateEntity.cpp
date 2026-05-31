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
#include "editor/application/EditorProjectActions_CreateEntity_Internal.h"  // AUDIT-2: helper compartido con _PickModal.cpp
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"
#include "engine/gameplay/weapon/WeaponSpec.h"  // F4H2 Bloque B: handleAddPlayer auto-asigna arma
#include "engine/gameplay/enemy/EnemySpec.h"    // F4H7: handleAddEnemy auto-asigna .moodenemy
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/BrushComponent.h"  // F3H10: kit brush (no esta en Components.h)
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/entity_type/EntityType.h"  // F3H9

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Mood {

using CreateEntityHelpers::WorldYBoundsLocal;
using CreateEntityHelpers::rotatedAabbWorldY_local;

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
    e.getComponent<TagComponent>().entityType = EntityType::Mesh;  // F3H9

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
    e.getComponent<TagComponent>().entityType = EntityType::Mesh;  // F3H9

    Log::editor()->info(
        "[create_entity_placeholder] Spawned '{}' con placeholder cube",
        finalName);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear entidad '") + finalName + "'");
}


// NOTE: renderPickFromLoadedMeshesModal vive en
// `EditorProjectActions_CreateEntity_PickModal.cpp` (extraido en AUDIT-2,
// 2026-05-17). El helper `rotatedAabbWorldY_local` se promovio a
// `EditorProjectActions_CreateEntity_Internal.h` para compartir entre siblings.


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
        ImGui::TextUnformatted(
            I18n::T("editor.convert_modal.no_scene").c_str());
        if (ImGui::Button(I18n::T("editor.modal.common.close").c_str())) {
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
        target.getComponent<TagComponent>().entityType = EntityType::Npc;  // F3H9
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
        target.getComponent<TagComponent>().entityType = EntityType::Pickable;  // F3H9
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
        target.getComponent<TagComponent>().entityType = EntityType::Light;  // F3H9
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
        target.getComponent<TagComponent>().entityType = EntityType::Light;  // F3H9
        Log::editor()->info("[convert] '{}' -> Luz direccional", tagName);
        applied = true;
    }

    // F3H10: 9 kits adicionales para cubrir el resto de EntityTypes. Patron
    // identico a los 4 originales — kitButton + addComponent + set entityType
    // + log. Comportamiento sigue aditivo (no destructivo).

    // --- Kit 5: Trigger volumétrico (solo) ---
    const bool hasTrigger = target.hasComponent<TriggerComponent>();
    if (kitButton("editor.convert_modal.kit.trigger",
                  "editor.convert_modal.kit.trigger_desc",
                  hasTrigger)) {
        target.addComponent<TriggerComponent>(TriggerComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::Trigger;
        Log::editor()->info("[convert] '{}' -> Trigger volumetrico", tagName);
        applied = true;
    }

    // --- Kit 6: Emisor de partículas ---
    const bool hasParticleEmitter = target.hasComponent<ParticleEmitterComponent>();
    if (kitButton("editor.convert_modal.kit.particle_emitter",
                  "editor.convert_modal.kit.particle_emitter_desc",
                  hasParticleEmitter)) {
        target.addComponent<ParticleEmitterComponent>(ParticleEmitterComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::ParticleEmitter;
        Log::editor()->info("[convert] '{}' -> Emisor de particulas", tagName);
        applied = true;
    }

    // --- Kit 7: Campo de fuerza ---
    const bool hasForceField = target.hasComponent<ForceFieldComponent>();
    if (kitButton("editor.convert_modal.kit.force_field",
                  "editor.convert_modal.kit.force_field_desc",
                  hasForceField)) {
        target.addComponent<ForceFieldComponent>(ForceFieldComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::ForceField;
        Log::editor()->info("[convert] '{}' -> Campo de fuerza", tagName);
        applied = true;
    }

    // --- Kit 8: Entorno (skybox + fog + post-process) ---
    const bool hasEnvironment = target.hasComponent<EnvironmentComponent>();
    if (kitButton("editor.convert_modal.kit.environment",
                  "editor.convert_modal.kit.environment_desc",
                  hasEnvironment)) {
        target.addComponent<EnvironmentComponent>(EnvironmentComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::Environment;
        Log::editor()->info("[convert] '{}' -> Entorno", tagName);
        applied = true;
    }

    // --- Kit 9: Fuente de audio ---
    const bool hasAudio = target.hasComponent<AudioSourceComponent>();
    if (kitButton("editor.convert_modal.kit.audio",
                  "editor.convert_modal.kit.audio_desc",
                  hasAudio)) {
        target.addComponent<AudioSourceComponent>(AudioSourceComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::Audio;
        Log::editor()->info("[convert] '{}' -> Fuente de audio", tagName);
        applied = true;
    }

    // --- Kit 10: Cámara ---
    const bool hasCamera = target.hasComponent<CameraComponent>();
    if (kitButton("editor.convert_modal.kit.camera",
                  "editor.convert_modal.kit.camera_desc",
                  hasCamera)) {
        target.addComponent<CameraComponent>(CameraComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::Camera;
        Log::editor()->info("[convert] '{}' -> Camara", tagName);
        applied = true;
    }

    // --- Kit 11: Vehículo stub ---
    const bool hasVehicle = target.hasComponent<VehicleComponent>();
    if (kitButton("editor.convert_modal.kit.vehicle",
                  "editor.convert_modal.kit.vehicle_desc",
                  hasVehicle)) {
        VehicleComponent vc;
        vc.configPath = "";  // dev asigna el .moodvehicle via Inspector despues
        vc.dirty = true;
        target.addComponent<VehicleComponent>(std::move(vc));
        target.getComponent<TagComponent>().entityType = EntityType::Vehicle;
        Log::editor()->info("[convert] '{}' -> Vehiculo stub", tagName);
        applied = true;
    }

    // --- Kit 12: Brush vacío ---
    const bool hasBrush = target.hasComponent<BrushComponent>();
    if (kitButton("editor.convert_modal.kit.brush",
                  "editor.convert_modal.kit.brush_desc",
                  hasBrush)) {
        target.addComponent<BrushComponent>(BrushComponent{});
        target.getComponent<TagComponent>().entityType = EntityType::Brush;
        Log::editor()->info("[convert] '{}' -> Brush vacio (definir caras via Brush Tool)",
                              tagName);
        applied = true;
    }

    // --- Kit 13: Mesh (placeholder, dev cambia el mesh via Inspector) ---
    const bool hasMesh = target.hasComponent<MeshRendererComponent>();
    if (kitButton("editor.convert_modal.kit.mesh",
                  "editor.convert_modal.kit.mesh_desc",
                  hasMesh)) {
        const MeshAssetId placeholderId = m_assetManager
            ? m_assetManager->missingMeshId()
            : 0u;
        auto mats = m_assetManager
            ? m_assetManager->createMaterialsForMesh(placeholderId)
            : std::vector<MaterialAssetId>{};
        target.addComponent<MeshRendererComponent>(placeholderId, std::move(mats));
        target.getComponent<TagComponent>().entityType = EntityType::Mesh;
        Log::editor()->info("[convert] '{}' -> Mesh placeholder", tagName);
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

// ============================================================================
// F2H60 polish iter2: spawn de luz como entidad nueva (sin mesh)
//
// Pre-iter2 el unico flow para crear una luz era spawnar primero un
// placeholder cube y "Convertir > Luz" sobre el. Ahora el modal Crear
// Entidad tiene tab propio "Luces" -- pedido del dev: "deberia poder
// crearse luces directamente desde el panel de entidades".
//
// Spawnean entidad solo con Tag + Transform + LightComponent. Sin
// MeshRenderer; el overlay del editor pinta el ICONO de la luz (gizmo
// 2D ya existente desde Hito 15+) asi el dev la ve en el viewport
// aunque no haya geometria.
// ============================================================================

namespace {

// Genera "Nombre", "Nombre_2", etc. unicos en la escena.
std::string uniqueEntityName(Scene& scene, const std::string& base) {
    std::string finalName = base;
    int suffix = 1;
    while (true) {
        bool collision = false;
        scene.forEach<TagComponent>([&](Entity, TagComponent& tag) {
            if (tag.name == finalName) collision = true;
        });
        if (!collision) return finalName;
        ++suffix;
        finalName = base + "_" + std::to_string(suffix);
    }
}

} // namespace

void EditorApplication::handleAddDirectionalLight() {
    if (!m_scene) return;
    const std::string name = uniqueEntityName(*m_scene, "DirectionalLight");
    Entity e = m_scene->createEntity(name);

    auto& t = e.getComponent<TransformComponent>();
    // Posicion arriba para que el icono del overlay sea visible. La
    // direccion de luz es independiente de la posicion para directionals
    // (la matriz light-space se arma desde el frustum de la camara).
    t.position = glm::vec3(0.0f, 3.0f, 0.0f);

    LightComponent light{};
    light.type        = LightComponent::Type::Directional;
    light.color       = glm::vec3(1.0f);
    light.intensity   = 1.0f;
    light.direction   = glm::vec3(-0.3f, -1.0f, -0.2f);
    light.castShadows = true; // engine-grade default -- pedido del dev.
    light.enabled     = true;
    e.addComponent<LightComponent>(light);
    e.getComponent<TagComponent>().entityType = EntityType::Light;  // F3H9

    Log::editor()->info("[create_light] Spawned '{}' (Directional, castShadows=ON)", name);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear luz '") + name + "'");
}

void EditorApplication::handleAddPointLight() {
    if (!m_scene) return;
    const std::string name = uniqueEntityName(*m_scene, "PointLight");
    Entity e = m_scene->createEntity(name);

    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 2.0f, 0.0f);

    LightComponent light{};
    light.type      = LightComponent::Type::Point;
    light.color     = glm::vec3(1.0f, 0.9f, 0.7f); // calida default.
    light.intensity = 1.0f;
    light.radius    = 10.0f;
    light.enabled   = true;
    e.addComponent<LightComponent>(light);
    e.getComponent<TagComponent>().entityType = EntityType::Light;  // F3H9

    Log::editor()->info("[create_light] Spawned '{}' (Point, radius=10m)", name);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear luz '") + name + "'");
}

// ============================================================================
// F2H86: Environment como entidad de primera clase.
//
// Pre-F2H86, para activar SSR/Bloom/Color Grading/etc el dev tenia que
// spawnar un placeholder (cubo missing-texture o similar) solo para
// colgar el EnvironmentComponent. El resultado: una entidad fantasma
// con un cubo magenta visible en el viewport cuya unica funcion era ser
// portador del config global.
//
// Convencion industrial:
//   - Unity: Render Settings (global, no anclado a GameObject) +
//     Volume actor con icono propio.
//   - Unreal: PostProcessVolume actor con icono y categoria propia.
//   - Godot: WorldEnvironment node con icono y entry point dedicado.
//
// F2H86 sigue ese patron: spawn de entidad solo con Tag + Transform +
// EnvironmentComponent. Sin MeshRenderer; el Hierarchy panel la
// identifica via IconHelpers (ICON_FA_GLOBE). Guard de unicidad: si
// ya hay un Environment en la escena, log y no-op (igual que
// applyEnvironmentFromScene del SceneRenderer que solo aplica el
// primer Environment encontrado).
// ============================================================================

void EditorApplication::handleAddEnvironment() {
    if (!m_scene) return;

    bool alreadyExists = false;
    m_scene->forEach<EnvironmentComponent>(
        [&](Entity, EnvironmentComponent&) { alreadyExists = true; });
    if (alreadyExists) {
        Log::editor()->info(
            "[create_environment] Ya existe un Environment en la escena. Skip.");
        return;
    }

    const std::string name = uniqueEntityName(*m_scene, "Environment");
    Entity e = m_scene->createEntity(name);

    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f); // posicion irrelevante (componente global).

    EnvironmentComponent env{}; // defaults del struct: kloofendal + fog Off + tonemap ACES, etc.
    e.addComponent<EnvironmentComponent>(env);
    e.getComponent<TagComponent>().entityType = EntityType::Environment;  // F3H9

    Log::editor()->info("[create_environment] Spawned '{}' (Environment global)", name);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear environment '") + name + "'");
}

// F3H22: variante silenciosa de handleAddEnvironment. Garantiza el
// singleton SIN push al history y SIN markDirty — el auto-spawn no es
// accion del dev, no debe ensuciar el dirty flag ni aparecer en undo.
// Llamado al cargar proyecto o crear scene nueva. Blender World Properties
// pattern: el World siempre existe aunque el dev no lo configure.
void EditorApplication::ensureEnvironmentExists() {
    if (!m_scene) return;

    bool alreadyExists = false;
    m_scene->forEach<EnvironmentComponent>(
        [&](Entity, EnvironmentComponent&) { alreadyExists = true; });
    if (alreadyExists) return;  // singleton ya presente

    const std::string name = uniqueEntityName(*m_scene, "Environment");
    Entity e = m_scene->createEntity(name);
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f);
    e.addComponent<EnvironmentComponent>(EnvironmentComponent{});
    e.getComponent<TagComponent>().entityType = EntityType::Environment;
    Log::editor()->info(
        "[ensure_environment] Auto-spawned '{}' (proyecto sin Environment).",
        name);
    // NOTA: NO replaceWithSingle, NO pushCreatedEntities, NO markDirty.
    // El dev no clickeo nada — si guarda manualmente, el Environment se
    // persiste; si NO guarda, queda solo runtime hasta el proximo load.
}

// ============================================================================
// F4H1: maniquí de testing.
//
// Spawnea un cubo (mesh = primitiva default) con HealthComponent. El dev
// puede aplicarle daño desde la consola Lua en Play mode:
//   health.damage("Dummy_1", 25)
// Al llegar a 0 vida, el cubo cae con física (RigidBody Dynamic auto-add
// del HealthSystem). NO añade RigidBody mientras vive — se queda quieto.
// ============================================================================

void EditorApplication::handleAddDummy() {
    if (!m_scene) return;
    const std::string name = uniqueEntityName(*m_scene, "Dummy");
    Entity e = m_scene->createEntity(name);

    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 1.0f, 0.0f);

    // Cubo primitiva default (slot 0 del AssetManager = missing mesh = cubo
    // unitario). Sin material custom: hereda el missing.png magenta hasta
    // que el dev le asigne uno — útil para distinguirlo visualmente del
    // entorno final.
    if (m_assetManager) {
        e.addComponent<MeshRendererComponent>(
            m_assetManager->missingMeshId(),
            MaterialAssetId{0});
    }

    // F4H1: HealthComponent con defaults Project Settings (regla nada
    // hardcodeado). Si no hay project settings cargados, fallback a 100.
    HealthComponent hc{};
    hc.current = 100.0f;
    hc.max     = 100.0f;
    e.addComponent<HealthComponent>(hc);

    Log::editor()->info(
        "[create_dummy] Spawned '{}' (cubo + HealthComponent {}/{})",
        name, hc.current, hc.max);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear maniquí '") + name + "'");
}

// ============================================================================
// F4H2 Bloque B — handleAddPlayer
//
// Spawnea una entity con tag literal "player" + WeaponComponent. Es el
// shooter del e2e: el input bridge en `EditorApplication::tickFrame` busca
// entities con tag=="player" + WeaponComponent, setea `wc.firing` según
// `Input.is_action_pressed("fire")` y llama `Weapon::fire(...)` con
// origin/dir del `m_playCamera` cuando puede disparar.
//
// F4H2 Bloque B follow-up (pedido del dev "que aparezca ya el arma, no
// arrastrar"): auto-asigna la primera arma del catalogo (orden alfa, hoy
// = shotgun) + currentAmmo=magazineSize. Si no hay armas, queda con
// weaponAssetId=0 (no-op fire) + warn en log.
//
// Guardia: si ya existe un "player" en la escena, abortamos con log
// (Single-player implícito; multi-player es hito futuro propio).
// ============================================================================

void EditorApplication::handleAddPlayer() {
    if (!m_scene) return;

    bool alreadyExists = false;
    m_scene->forEach<TagComponent>(
        [&](Entity, TagComponent& tag) {
            if (tag.name == "player") alreadyExists = true;
        });
    if (alreadyExists) {
        Log::editor()->warn(
            "[create_player] ya existe una entity con tag 'player' — abortado");
        return;
    }

    Entity e = m_scene->createEntity("player");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 1.0f, 0.0f);

    // F4H4: HP + armor + arsenal con SOLO slot 0 default (escopeta primera
    // alfabetica del catalogo). El resto via pickup. Sandbox F4H3 (4 slots
    // auto) revertido — el flow real es "arrancas con escopeta, encontras
    // el resto en el mapa".
    HealthComponent hc{};
    hc.current = 100.0f;
    hc.max     = 100.0f;
    e.addComponent<HealthComponent>(hc);

    ArmorComponent ac{};
    ac.current     = 0.0f;
    ac.max         = 100.0f;
    ac.absorbRatio = 0.66f;
    e.addComponent<ArmorComponent>(ac);

    WeaponComponent wc{};
    if (m_assetManager) {
        auto weapons = m_assetManager->enumerateWeapons(/*rescanFromDisk=*/true);
        if (!weapons.empty()) {
            const auto& src = weapons.front();
            wc.slots[0].weaponAssetId = src.id;
            if (const Weapon::Spec* spec = m_assetManager->getWeapon(src.id)) {
                wc.slots[0].currentAmmo = static_cast<int>(spec->magazineSize);
            }
            wc.activeSlot = 0;
            wc.lastActiveSlot = 0;
            Log::editor()->info(
                "[create_player] slot 0 = '{}' (resto vacios — encontrar via pickup)",
                src.displayName);
        } else {
            Log::editor()->warn(
                "[create_player] no hay .moodweapon en assets/weapons/ — "
                "player queda sin arma. Agregá uno y reabrí Inspector.");
        }
    }
    e.addComponent<WeaponComponent>(wc);

    // F4H3 — viewmodel entity: marker "__viewmodel" + ViewmodelComponent +
    // MeshRenderer placeholder (cubo del missingMeshId). El sistema
    // `Weapon::tickViewmodel` cada frame en Play mode sincroniza su
    // Transform a la camara del player y swap el mesh segun el slot activo.
    if (m_scene && m_assetManager) {
        Entity vmEntity = m_scene->createEntity("__viewmodel");
        ViewmodelComponent vm{};
        vmEntity.addComponent<ViewmodelComponent>(vm);
        // MeshRenderer con cubo placeholder; tickViewmodel lo va a
        // sobreescribir al primer frame con el viewmodelMesh del arma
        // activa (o mantener el cubo si el spec no trae viewmodelMesh).
        MeshRendererComponent mr{};
        mr.mesh = m_assetManager->missingMeshId();
        mr.materials.push_back(m_assetManager->missingMaterialId());
        vmEntity.addComponent<MeshRendererComponent>(mr);
        Log::editor()->info(
            "[create_player] viewmodel entity '__viewmodel' creada con "
            "cubo placeholder (tickViewmodel la sincroniza a la camara).");
    }

    Log::editor()->info(
        "[create_player] Spawned 'player' (Tag + Transform + WeaponComponent + viewmodel). "
        "Clic izquierdo dispara, R recarga, scroll/Q swap arma, 1-4 swap directo.");

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, "Crear player");
}

// ============================================================================
// F4H7 — handleAddEnemy
//
// Spawnea entidad enemigo basica: cubo placeholder + HealthComponent +
// EnemyComponent con el primer .moodenemy alfabetico del catalogo (auto-
// asignacion como handleAddPlayer). Sin RigidBody: queda Static por default,
// EnemySystem agrega Dynamic al morir (mismo patron Health F4H1).
//
// Sub-fase 4.2 cierra con cubos por strategic deferral del usuario en F4H6;
// Sub-fase 4.3 traera viewmodel mesh real + animaciones Mixamo + ragdoll.
// ============================================================================

void EditorApplication::handleAddEnemy() {
    if (!m_scene) return;
    const std::string name = uniqueEntityName(*m_scene, "Enemy");
    Entity e = m_scene->createEntity(name);

    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 1.0f, 0.0f);

    // Cubo primitivo placeholder. Sub-fase 4.3 reemplazara con NPC Mixamo.
    if (m_assetManager) {
        e.addComponent<MeshRendererComponent>(
            m_assetManager->missingMeshId(),
            MaterialAssetId{0});
    }

    // F4H1 — HealthComponent. Si el spec tiene health propio, usar ese; sino
    // defaults.
    HealthComponent hc{};
    hc.current = 50.0f;
    hc.max     = 50.0f;

    // F4H7 — EnemyComponent con primer .moodenemy del catalogo. Si no hay
    // ningun .moodenemy, queda en slot 0 (defaults sanos del spec vacio).
    EnemyComponent ec{};
    if (m_assetManager) {
        auto enemies = m_assetManager->enumerateEnemies(/*rescanFromDisk=*/true);
        if (!enemies.empty()) {
            const auto& src = enemies.front();
            ec.enemyAssetId = src.id;
            if (const Enemy::Spec* spec = m_assetManager->getEnemy(src.id)) {
                hc.current = spec->health;
                hc.max     = spec->health;
            }
            Log::editor()->info(
                "[create_enemy] '{}' spec = '{}' (HP {})",
                name, src.displayName, hc.max);
        } else {
            Log::editor()->warn(
                "[create_enemy] no hay .moodenemy en assets/enemies/ — "
                "enemy queda con defaults (slot 0). Crea un .moodenemy y "
                "reabri Inspector.");
        }
    }
    e.addComponent<HealthComponent>(hc);
    e.addComponent<EnemyComponent>(ec);

    Log::editor()->info(
        "[create_enemy] Spawned '{}' (cubo + HealthComponent {}/{} + EnemyComponent Idle)",
        name, hc.current, hc.max);

    replaceWithSingle(m_ui.selectionSet(), e);
    pushCreatedEntities({e}, std::string("Crear enemigo '") + name + "'");
}

} // namespace Mood
