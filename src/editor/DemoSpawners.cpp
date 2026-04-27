// Implementacion de los handlers de "Ayuda > Agregar X" + drops del
// viewport (Hito 16 refactor — extraido de EditorApplication.cpp).
//
// Cada `processXxx()` es un metodo privado de `EditorApplication` que
// (a) consume su request del UI con `m_ui.consume*Request()`, (b) si esta
// pendiente, crea/edita la entidad correspondiente. Sin request: no-op.
// El `run()` del EditorApplication llama a estos handlers en cadena tras
// `m_ui.draw()` para que la UI emita y el modelo reaccione en el mismo
// frame.

#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/MaterialAsset.h"
#include "engine/render/MeshAsset.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ScenePick.h"
#include "engine/scene/ViewportPick.h"
#include "engine/serialization/PrefabSerializer.h"

#include <portable-file-dialogs.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

void EditorApplication::processSpawnRotatorRequest() {
    if (!(m_ui.consumeSpawnRotatorRequest() && m_scene)) return;
    Entity r = m_scene->createEntity("Rotador");
    auto& t = r.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 4.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    // Usa el cubo fallback del AssetManager (slot 0) como mesh del rotador.
    const MaterialAssetId rotMat =
        m_assetManager->loadMaterialFromTexture(m_wallTextureId);
    r.addComponent<MeshRendererComponent>(m_assetManager->missingMeshId(), rotMat);
    r.addComponent<ScriptComponent>(std::string{"assets/scripts/rotator.lua"});
    Log::editor()->info("Spawned rotador demo en (0, 4, 0)");
}

void EditorApplication::processSpawnPhysicsBoxRequest() {
    if (!(m_ui.consumeSpawnPhysicsBoxRequest() && m_scene && m_assetManager)) return;
    Entity box = m_scene->createEntity("CajaFisica");
    auto& t = box.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 6.0f, 0.0f);
    t.scale    = glm::vec3(1.0f);
    const MaterialAssetId boxMat =
        m_assetManager->loadMaterialFromTexture(m_wallTextureId);
    box.addComponent<MeshRendererComponent>(
        m_assetManager->missingMeshId(), boxMat);
    box.addComponent<RigidBodyComponent>(
        RigidBodyComponent::Type::Dynamic,
        RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f, 0.5f, 0.5f),
        5.0f);
    Log::physics()->info("Spawned caja fisica en (0, 6, 0) con mass=5kg");
    markDirty();
}

void EditorApplication::processSpawnEnvironmentRequest() {
    if (!(m_ui.consumeSpawnEnvironmentRequest() && m_scene)) return;
    // Si ya hay una, avisar y seleccionarla en lugar de duplicar.
    Entity existing{};
    m_scene->forEach<EnvironmentComponent>(
        [&](Entity e, EnvironmentComponent&) {
            if (!static_cast<bool>(existing)) existing = e;
        });
    if (static_cast<bool>(existing)) {
        Log::editor()->warn("Ya existe un Environment; seleccionando el existente.");
        m_ui.setSelectedEntity(existing);
    } else {
        Entity env = m_scene->createEntity("Environment");
        env.addComponent<EnvironmentComponent>();
        m_ui.setSelectedEntity(env);
        Log::editor()->info("Spawned entidad Environment con defaults");
        markDirty();
    }
}

void EditorApplication::processSpawnShadowDemoRequest() {
    if (!(m_ui.consumeSpawnShadowDemoRequest() && m_scene && m_assetManager)) return;

    // Texturas: grid para el piso (lectura clara de la sombra), brick para la
    // columna. Ambos ya estan cargados durante buildInitialTestMap, asi que
    // loadTexture deberia ser un cache hit.
    const TextureAssetId gridTex  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brickTex = m_assetManager->loadTexture("textures/brick.png");
    const MaterialAssetId gridMat  =
        m_assetManager->loadMaterialFromTexture(gridTex);
    const MaterialAssetId brickMat =
        m_assetManager->loadMaterialFromTexture(brickTex);
    const MeshAssetId    cube     = m_assetManager->missingMeshId(); // cubo primitivo

    // 1) Piso plano grande (cubo escalado y=0.1). Centrado en el origen, se
    //    apoya con su cara superior en y=0.1 — un poco encima del suelo de
    //    tiles (y=0.0) para evitar z-fighting si conviven.
    {
        Entity floor = m_scene->createEntity("DemoSombras_Piso");
        auto& t = floor.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 0.05f, 0.0f);
        t.scale    = glm::vec3(20.0f, 0.1f, 20.0f);
        floor.addComponent<MeshRendererComponent>(cube, gridMat);
    }

    // 2) Columna alta (cubo 1x4x1) que proyecta una sombra alargada bien
    //    visible sobre el piso.
    {
        Entity col = m_scene->createEntity("DemoSombras_Columna");
        auto& t = col.getComponent<TransformComponent>();
        t.position = glm::vec3(2.0f, 2.0f, 0.0f);
        t.scale    = glm::vec3(1.0f, 4.0f, 1.0f);
        col.addComponent<MeshRendererComponent>(cube, brickMat);
    }

    // 3) Sol direccional con castShadows. Direccion oblicua (no vertical) para
    //    que la sombra de la columna salga LARGA contra el piso. Intensidad
    //    1.0 — alcanza para ver claro contra el ambient 0.08.
    {
        Entity sun = m_scene->createEntity("DemoSombras_Sol");
        LightComponent lc{};
        lc.type        = LightComponent::Type::Directional;
        lc.color       = glm::vec3(1.0f, 0.97f, 0.92f);
        lc.intensity   = 1.0f;
        lc.direction   = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        lc.enabled     = true;
        lc.castShadows = true;
        sun.addComponent<LightComponent>(lc);
        m_ui.setSelectedEntity(sun);
    }

    Log::editor()->info(
        "Spawned demo de sombras: piso 20x20, columna 1x4x1 en (2, 2, 0), "
        "sol direccional con castShadows=true");
    markDirty();
}

void EditorApplication::processSpawnPbrSpheresRequest() {
    if (!(m_ui.consumeSpawnPbrSpheresRequest() && m_scene && m_assetManager)) return;

    const MeshAssetId sphere = m_assetManager->primitiveSphereId();
    if (sphere == 0) {
        Log::editor()->warn("Spawn esferas PBR: primitive sphere no disponible");
        return;
    }

    // Showcase grid 3x3 (Hito 17 Bloque 5): eje X = metallic creciente
    // (0.0, 0.5, 1.0), eje Y = roughness creciente (0.05, 0.5, 1.0).
    // Las 9 esferas comparten un albedoTint neutro (gris claro) para
    // que las diferencias se vean en el shading, no en el color.
    // Convencion del eje Z: las esferas miran al +X del mundo, asi al
    // orbitar la camara con el default editor camera quedan en cuadro.
    const glm::vec3 baseTint(0.85f, 0.85f, 0.88f);
    const f32 spacing  = 2.5f;
    const f32 baseY    = 4.0f;
    const f32 baseX    = -spacing; // columna del medio en x=0
    const f32 baseZ    = -spacing; // fila inferior en z=0
    const f32 metVals[3]   = {0.0f, 0.5f, 1.0f};
    const f32 rouVals[3]   = {0.05f, 0.5f, 1.0f}; // 0.05 evita NaN en GGX

    int spawned = 0;
    for (int my = 0; my < 3; ++my) {            // filas (roughness)
        for (int mx = 0; mx < 3; ++mx) {         // columnas (metallic)
            MaterialAsset prototype{};
            prototype.albedoTint    = baseTint;
            prototype.metallicMult  = metVals[mx];
            prototype.roughnessMult = rouVals[my];
            const MaterialAssetId matId =
                m_assetManager->createMaterial(prototype);

            char name[64];
            std::snprintf(name, sizeof(name),
                          "PBR_M%.1f_R%.2f", metVals[mx], rouVals[my]);
            Entity sph = m_scene->createEntity(name);
            auto& t = sph.getComponent<TransformComponent>();
            t.position = glm::vec3(
                baseX + static_cast<f32>(mx) * spacing,
                baseY + static_cast<f32>(my) * spacing,
                baseZ);
            t.scale = glm::vec3(1.0f);
            sph.addComponent<MeshRendererComponent>(sphere, matId);
            ++spawned;
        }
    }
    Log::editor()->info(
        "Spawned showcase PBR grid 3x3 ({} esferas): metallic en X "
        "(0/0.5/1), roughness en Y (0.05/0.5/1)", spawned);
    markDirty();
}

void EditorApplication::processSpawnLightStressRequest() {
    if (!(m_ui.consumeSpawnLightStressRequest() && m_scene)) return;

    // 64 point lights en grid 8x8 sobre y=2, espaciado 2.5m. Cubre un
    // area de ~17.5m x 17.5m centrada en el origen. Colores procedurales
    // tipo HSV (hue rotando, saturacion alta, value alto) para que cada
    // luz se distinga claramente del vecino.
    constexpr int kRows = 8;
    constexpr int kCols = 8;
    constexpr f32 kSpacing = 2.5f;
    constexpr f32 kHeight  = 2.0f;
    constexpr f32 kRadius  = 3.5f;     // pequenas para que el grid sea util
    constexpr f32 kIntensity = 1.2f;

    // HSV -> RGB inline. h en [0,1).
    auto hsvToRgb = [](f32 h, f32 s, f32 v) -> glm::vec3 {
        const f32 i = std::floor(h * 6.0f);
        const f32 f = h * 6.0f - i;
        const f32 p = v * (1.0f - s);
        const f32 q = v * (1.0f - f * s);
        const f32 t = v * (1.0f - (1.0f - f) * s);
        switch (static_cast<int>(i) % 6) {
            case 0: return {v, t, p};
            case 1: return {q, v, p};
            case 2: return {p, v, t};
            case 3: return {p, q, v};
            case 4: return {t, p, v};
            case 5: return {v, p, q};
        }
        return {v, v, v};
    };

    const f32 baseX = -static_cast<f32>(kCols - 1) * 0.5f * kSpacing;
    const f32 baseZ = -static_cast<f32>(kRows - 1) * 0.5f * kSpacing;

    int spawned = 0;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const f32 hue = static_cast<f32>(spawned) / 64.0f;
            const glm::vec3 color = hsvToRgb(hue, 0.85f, 1.0f);

            char name[64];
            std::snprintf(name, sizeof(name), "StressLight_%02d_%02d", r, c);
            Entity light = m_scene->createEntity(name);
            auto& t = light.getComponent<TransformComponent>();
            t.position = glm::vec3(
                baseX + static_cast<f32>(c) * kSpacing,
                kHeight,
                baseZ + static_cast<f32>(r) * kSpacing);
            LightComponent lc{};
            lc.type      = LightComponent::Type::Point;
            lc.color     = color;
            lc.intensity = kIntensity;
            lc.radius    = kRadius;
            lc.enabled   = true;
            light.addComponent<LightComponent>(lc);
            ++spawned;
        }
    }
    Log::editor()->info(
        "Spawned stress test {} point lights ({}x{} grid, radius={}m)",
        spawned, kCols, kRows, kRadius);
    markDirty();
}

void EditorApplication::processSpawnAnimatedCharacterRequest() {
    if (!(m_ui.consumeSpawnAnimatedCharacterRequest() && m_scene
          && m_assetManager)) return;

    // Asset CC0 de glTF Sample Assets (KhronosGroup). Trae 3 clips:
    // "Survey", "Walk", "Run". El Animator con clipName vacio toma el
    // primero ("Survey").
    const MeshAssetId foxMesh = m_assetManager->loadMesh("meshes/Fox.glb");
    if (foxMesh == m_assetManager->missingMeshId()) {
        Log::editor()->warn(
            "Spawn personaje animado: 'meshes/Fox.glb' no se pudo cargar");
        return;
    }
    const MeshAsset* asset = m_assetManager->getMesh(foxMesh);
    if (asset == nullptr || !asset->hasSkeleton()) {
        Log::editor()->warn(
            "Spawn personaje animado: 'meshes/Fox.glb' no tiene esqueleto");
        return;
    }

    Entity fox = m_scene->createEntity("Fox");
    auto& t = fox.getComponent<TransformComponent>();
    // Fox.glb viene en cm escalado por glTF — assimp lo deja en escala
    // ~100 unidades. Lo bajamos a 0.01 para que mida ~1.5m que es la
    // escala real del modelo, comparable al jugador (1.8m).
    t.position = glm::vec3(0.0f, 0.0f, -3.0f);
    t.scale    = glm::vec3(0.01f);

    // Material default (gris mate). El AssetManager ya tiene el slot 0
    // como default; los materiales por submesh se asignan al primero.
    fox.addComponent<MeshRendererComponent>(foxMesh, /*material=*/0u);

    AnimatorComponent anim{};
    anim.clipName = "";   // primer clip del MeshAsset
    anim.playing  = true;
    anim.loop     = true;
    fox.addComponent<AnimatorComponent>(anim);

    SkeletonComponent skel{};
    // skinningMatrices se rellena en el primer update de AnimationSystem.
    fox.addComponent<SkeletonComponent>(skel);

    Log::editor()->info(
        "Spawned personaje animado 'Fox' ({} bones, {} clips). "
        "Editar 'clipName' del Animator desde el Inspector para cambiar "
        "entre Survey/Walk/Run.",
        asset->skeleton->bones.size(), asset->animations.size());
    markDirty();
}

void EditorApplication::processSpawnPointLightRequest() {
    if (!(m_ui.consumeSpawnPointLightRequest() && m_scene)) return;
    Entity light = m_scene->createEntity("Luz demo");
    auto& t = light.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 4.0f, 0.0f);
    LightComponent lc{};
    lc.type      = LightComponent::Type::Point;
    lc.color     = glm::vec3(1.0f, 0.95f, 0.85f); // tibia, no neon
    lc.intensity = 1.5f;
    lc.radius    = 12.0f;
    lc.enabled   = true;
    light.addComponent<LightComponent>(lc);
    Log::editor()->info("Spawned luz puntual en (0, 4, 0) con radius=12");
    markDirty();
}

void EditorApplication::processSpawnAudioSourceRequest() {
    if (!(m_ui.consumeSpawnAudioSourceRequest() && m_scene && m_assetManager)) return;
    const AudioAssetId beepId = m_assetManager->loadAudio("audio/beep.wav");
    Entity src = m_scene->createEntity("AudioSource demo");
    auto& t = src.getComponent<TransformComponent>();
    t.position = glm::vec3(-10.0f, 1.5f, -10.0f);
    // Sin MeshRenderer: es una entidad invisible (marcador audio).
    AudioSourceComponent asrc{beepId};
    asrc.loop = true;
    asrc.is3D = true;
    asrc.playOnStart = true;
    asrc.volume = 0.8f;
    src.addComponent<AudioSourceComponent>(asrc);
    Log::editor()->info("Spawned audio source demo en (-10, 1.5, -10)");
}

void EditorApplication::processSavePrefabRequest() {
    if (!m_ui.consumeSavePrefabRequest()) return;
    Entity sel = m_ui.selectedEntity();
    if (!static_cast<bool>(sel)) {
        Log::editor()->warn("Guardar prefab: no hay entidad seleccionada");
        return;
    }
    if (!m_assetManager) {
        Log::editor()->warn("Guardar prefab: AssetManager no disponible");
        return;
    }

    // Default dir = <cwd>/assets/prefabs/ (creado al vuelo si no existe).
    // Esa es la carpeta que el AssetBrowser escanea.
    const auto prefabsDir =
        std::filesystem::current_path() / "assets" / "prefabs";
    std::error_code mkec;
    std::filesystem::create_directories(prefabsDir, mkec);

    // Sanitizar el filename: tags pueden tener caracteres invalidos para
    // Windows shell (`/` aparece p.ej. en tags tipo "Mesh_meshes/pyramid.obj_1"
    // del drop de mesh). Si el path default tiene un caracter invalido,
    // pfd::save_file en Windows devuelve "" SIN mostrar el dialogo —
    // el usuario ve "no aparece nada". Reemplazamos `<>:"/\|?*` por `_`.
    auto sanitize = [](std::string s) {
        for (char& c : s) {
            switch (c) {
                case '<': case '>': case ':': case '"':
                case '/': case '\\': case '|':
                case '?': case '*': c = '_'; break;
                default: break;
            }
        }
        return s;
    };
    const std::string baseTag =
        sel.hasComponent<TagComponent>()
            ? sel.getComponent<TagComponent>().name
            : std::string{"untitled"};
    const std::string defaultName = sanitize(baseTag) + ".moodprefab";

    // pfd::save_file en Windows usa el `default_path` como SetFolder +
    // SetFileName. Cuando el path tiene FILE+DIR, pfd espera separadores
    // nativos (backslashes en Windows); `generic_string()` los convierte a
    // forward slashes y el shell de Windows interpreta el path como
    // invalido => devuelve "" sin mostrar el dialogo.
    const auto sel_result = pfd::save_file(
        "Guardar como prefab",
        (prefabsDir / defaultName).string(),
        {"Prefabs MoodEngine (*.moodprefab)", "*.moodprefab"},
        pfd::opt::none).result();
    if (sel_result.empty()) {
        Log::editor()->info("Guardar prefab: cancelado");
        return;
    }
    auto outPath = std::filesystem::path(sel_result);
    if (outPath.extension() != ".moodprefab") {
        outPath += ".moodprefab";
    }
    try {
        const std::string prefabName = outPath.stem().generic_string();
        PrefabSerializer::save(sel, prefabName, *m_assetManager, outPath);

        // Path logico para el PrefabLinkComponent: relativo al cwd si el
        // archivo cayo en `assets/prefabs/`, si no, dejamos solo el filename
        // como referencia (loguea un warn para que el usuario sepa que el
        // browser no lo va a listar).
        std::error_code rec;
        const auto absOut = std::filesystem::absolute(outPath, rec);
        const auto absRoot = std::filesystem::absolute(
            std::filesystem::current_path(), rec);
        const auto rel = std::filesystem::relative(absOut, absRoot, rec);
        std::string logicalPath = rel.generic_string();
        const bool insideAssets =
            !rec && !logicalPath.empty()
                 && logicalPath.rfind("assets/prefabs/", 0) == 0;
        if (insideAssets) {
            // logical path para AssetManager: sacar el prefijo "assets/"
            // para que matchee con loadPrefab().
            logicalPath = logicalPath.substr(std::string("assets/").size());
        } else {
            Log::editor()->warn(
                "Guardar prefab: '{}' fuera de assets/prefabs/. "
                "El AssetBrowser solo lista prefabs en esa carpeta.",
                outPath.generic_string());
            logicalPath = outPath.filename().generic_string();
        }
        sel.addComponent<PrefabLinkComponent>(logicalPath);

        // Refrescar el AssetBrowser para que el nuevo prefab aparezca sin
        // reabrir el editor.
        m_ui.assetBrowser().rescan();
        m_ui.setStatusMessage("Prefab guardado: " + prefabName);
        markDirty();
    } catch (const std::exception& e) {
        Log::editor()->warn("Guardar prefab fallo: {}", e.what());
        pfd::message("MoodEngine",
            std::string("Error al guardar prefab: ") + e.what(),
            pfd::choice::ok, pfd::icon::error);
    }
}

void EditorApplication::processViewportTextureDrop() {
    const ViewportPanel::TextureDrop drop = m_ui.viewport().consumeTextureDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor)) return;

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(drop.ndcX, drop.ndcY));
    if (!hit.hit) return;

    const auto texId = static_cast<TextureAssetId>(drop.textureId);
    // Mantener el GridMap sincronizado (physics + serializer).
    m_map.setTile(hit.tileX, hit.tileY, TileType::SolidWall, texId);
    // Edit localizado en la Scene (preserva handles + seleccion).
    updateTileEntity(hit.tileX, hit.tileY, texId);
    Log::editor()->info("Drop textura id={} -> tile ({}, {})",
                         drop.textureId, hit.tileX, hit.tileY);
    markDirty();
}

void EditorApplication::processViewportMeshDrop() {
    const ViewportPanel::MeshDrop mdrop = m_ui.viewport().consumeMeshDrop();
    if (!(mdrop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(mdrop.ndcX, mdrop.ndcY));
    if (!hit.hit) return;

    const auto meshId = static_cast<MeshAssetId>(mdrop.meshId);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    // Nombre incremental por si spawneamos varios del mismo mesh.
    const std::string meshName = m_assetManager->meshPathOf(meshId);
    Entity e = m_scene->createEntity("Mesh_" + meshName);
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(
        origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize, // sobre el piso, altura = 0.5 tile
        origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);
    t.scale = glm::vec3(1.0f);
    // Material por defecto: slot 0 (default material). El usuario puede
    // dropear texturas/materiales encima despues.
    e.addComponent<MeshRendererComponent>(meshId, MaterialAssetId{0});
    Log::editor()->info("Drop mesh '{}' -> tile ({}, {})",
                         meshName, hit.tileX, hit.tileY);
    markDirty();
}

void EditorApplication::processViewportPrefabDrop() {
    const ViewportPanel::PrefabDrop pdrop = m_ui.viewport().consumePrefabDrop();
    if (!(pdrop.pending && m_mode == EditorMode::Editor && m_scene && m_assetManager)) return;

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(pdrop.ndcX, pdrop.ndcY));
    const auto pid = static_cast<PrefabAssetId>(pdrop.prefabId);
    const SavedPrefab* sp = m_assetManager->getPrefab(pid);
    if (sp == nullptr || !hit.hit || pid == m_assetManager->missingPrefabId()) return;

    const std::string prefabPath = m_assetManager->prefabPathOf(pid);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    const glm::vec3 spawnPos(
        origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize,
        origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);

    // Tag del root: usamos el tag guardado en el prefab + un sufijo
    // numerico simple para distinguir multiples instancias (no garantiza
    // unicidad — entt::null permite duplicados).
    static u32 s_prefabSpawnCounter = 0;
    ++s_prefabSpawnCounter;
    const std::string instanceTag = (sp->root.tag.empty()
        ? std::string("Prefab")
        : sp->root.tag) + "_" + std::to_string(s_prefabSpawnCounter);

    Entity e = m_scene->createEntity(instanceTag);
    auto& t = e.getComponent<TransformComponent>();
    // Posicion = world-point del pick. Rotacion / scale del prefab se
    // preservan (override local viene despues via Inspector).
    t.position      = spawnPos;
    t.rotationEuler = sp->root.rotationEuler;
    t.scale         = sp->root.scale;

    if (sp->root.meshRenderer.has_value()) {
        const auto& mr = *sp->root.meshRenderer;
        const MeshAssetId meshId = m_assetManager->loadMesh(mr.meshPath);
        // Hito 17: cada slot del prefab persiste como `material_path` (puede
        // ser un .material o vacio). Empty -> default material slot 0.
        std::vector<MaterialAssetId> mats;
        mats.reserve(mr.materials.size());
        for (const auto& matPath : mr.materials) {
            mats.push_back(matPath.empty()
                ? m_assetManager->missingMaterialId()
                : m_assetManager->loadMaterial(matPath));
        }
        e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
    }
    if (sp->root.light.has_value()) {
        const auto& l = *sp->root.light;
        LightComponent lc;
        lc.type = (l.type == "directional")
            ? LightComponent::Type::Directional
            : LightComponent::Type::Point;
        lc.color     = l.color;
        lc.intensity = l.intensity;
        lc.radius    = l.radius;
        lc.direction = l.direction;
        lc.enabled   = l.enabled;
        lc.castShadows = l.castShadows; // Hito 16
        e.addComponent<LightComponent>(lc);
    }
    if (sp->root.rigidBody.has_value()) {
        const auto& r = *sp->root.rigidBody;
        RigidBodyComponent rb;
        if      (r.type == "static")    rb.type = RigidBodyComponent::Type::Static;
        else if (r.type == "kinematic") rb.type = RigidBodyComponent::Type::Kinematic;
        else                            rb.type = RigidBodyComponent::Type::Dynamic;
        if      (r.shape == "sphere")  rb.shape = RigidBodyComponent::Shape::Sphere;
        else if (r.shape == "capsule") rb.shape = RigidBodyComponent::Shape::Capsule;
        else                           rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = r.halfExtents;
        rb.mass        = r.mass;
        e.addComponent<RigidBodyComponent>(rb);
    }
    // Link al prefab — clave para futuro "revert/apply" (no implementado
    // en Hito 14 pero el breadcrumb queda).
    e.addComponent<PrefabLinkComponent>(prefabPath);

    m_ui.setSelectedEntity(e);
    Log::editor()->info("Drop prefab '{}' -> tile ({}, {})",
                         prefabPath, hit.tileX, hit.tileY);
    markDirty();
}

void EditorApplication::processViewportMaterialDrop() {
    const ViewportPanel::MaterialDrop drop = m_ui.viewport().consumeMaterialDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    // Pick por entidad (usa los AABBs de meshes — mismo flow que el
    // click-select). Asignar material al PRIMER slot del MeshRenderer.
    ScenePickResult hit = pickEntity(*m_scene, view, projection,
        glm::vec2(drop.ndcX, drop.ndcY),
        m_assetManager.get());
    if (!hit) {
        Log::editor()->info("Drop material: no hay entidad bajo el cursor");
        return;
    }
    if (!hit.entity.hasComponent<MeshRendererComponent>()) {
        Log::editor()->info("Drop material: la entidad no tiene MeshRenderer");
        return;
    }

    const auto matId = static_cast<MaterialAssetId>(drop.materialId);
    auto& mr = hit.entity.getComponent<MeshRendererComponent>();
    if (mr.materials.empty()) {
        mr.materials.push_back(matId);
    } else {
        mr.materials[0] = matId;
    }
    m_ui.setSelectedEntity(hit.entity);
    const std::string tagName = hit.entity.hasComponent<TagComponent>()
        ? hit.entity.getComponent<TagComponent>().name
        : std::string{"(sin tag)"};
    Log::editor()->info("Drop material id {} -> entidad '{}'",
                         drop.materialId, tagName);
    markDirty();
}

} // namespace Mood
