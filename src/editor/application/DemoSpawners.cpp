// Implementacion de los handlers de "Ayuda > Agregar X" + drops del
// viewport (Hito 16 refactor — extraido de EditorApplication.cpp).
//
// Cada `processXxx()` es un metodo privado de `EditorApplication` que
// (a) consume su request del UI con `m_ui.consume*Request()`, (b) si esta
// pendiente, crea/edita la entidad correspondiente. Sin request: no-op.
// El `run()` del EditorApplication llama a estos handlers en cadena tras
// `m_ui.draw()` para que la UI emita y el modelo reaccione en el mismo
// frame.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/CreateEntityCommand.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/scene/components/BrushComponent.h"  // F2H15: drop sobre brushes
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/scene/queries/ViewportPick.h"
#include "engine/scene/serialization/PrefabSerializer.h"

#include <portable-file-dialogs.h>

#include <glm/ext/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

namespace {

// Hito 23: rota un AABB por un Euler en orden YXZ (mismo que
// TransformComponent::worldMatrix) y devuelve el rango Y resultante en
// world-space. Util para autoscale + floor offset post-rotacion.
struct WorldYBounds { f32 minY = 0.0f; f32 maxY = 0.0f; };
WorldYBounds rotatedAabbWorldY(const glm::vec3& aabbMin,
                                 const glm::vec3& aabbMax,
                                 const glm::vec3& eulerDeg) {
    glm::mat4 R(1.0f);
    R = glm::rotate(R, glm::radians(eulerDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    R = glm::rotate(R, glm::radians(eulerDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    R = glm::rotate(R, glm::radians(eulerDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));

    f32 minY = std::numeric_limits<f32>::max();
    f32 maxY = std::numeric_limits<f32>::lowest();
    for (int xi = 0; xi <= 1; ++xi)
    for (int yi = 0; yi <= 1; ++yi)
    for (int zi = 0; zi <= 1; ++zi) {
        const glm::vec3 p(
            xi ? aabbMax.x : aabbMin.x,
            yi ? aabbMax.y : aabbMin.y,
            zi ? aabbMax.z : aabbMin.z);
        const glm::vec3 rp = glm::vec3(R * glm::vec4(p, 1.0f));
        minY = std::min(minY, rp.y);
        maxY = std::max(maxY, rp.y);
    }
    return {minY, maxY};
}

} // namespace

void EditorApplication::processSpawnRotatorRequest() {
    if (!(m_ui.consumeSpawnRotatorRequest() && m_scene)) return;
    Entity r = m_scene->createEntity("Rotador");
    auto& t = r.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 4.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    // Usa el cubo fallback del AssetManager (slot 0) como mesh del rotador.
    // Material instance unico (createMaterialFromTexture, no loadMaterialFromTexture):
    // si reusaramos el material cacheado, editar el tint de UN rotador
    // pintaria a TODOS los rotadores spawneados.
    const MaterialAssetId rotMat =
        m_assetManager->createMaterialFromTexture(m_wallTextureId);
    r.addComponent<MeshRendererComponent>(m_assetManager->missingMeshId(), rotMat);
    r.addComponent<ScriptComponent>(std::string{"assets/scripts/rotator.lua"});
    Log::editor()->info("Spawned rotador demo en (0, 4, 0)");
    pushCreatedEntities({r}, "Spawn rotador demo");
}

void EditorApplication::processSpawnEnemyDemoRequest() {
    if (!(m_ui.consumeSpawnEnemyDemoRequest() && m_scene && m_assetManager)) return;

    // CesiumMan.glb (CC0, glTF Sample Assets): humanoide simple con
    // animacion de caminata. Mejor que Fox para "enemigo" porque
    // bipedo se siente como adversario, no mascota.
    const MeshAssetId enemyMesh = m_assetManager->loadMesh("meshes/CesiumMan.glb");
    if (enemyMesh == m_assetManager->missingMeshId()) {
        Log::editor()->warn(
            "Spawn enemigo demo: 'meshes/CesiumMan.glb' no se pudo cargar");
        return;
    }
    const MeshAsset* asset = m_assetManager->getMesh(enemyMesh);
    if (asset == nullptr || !asset->hasSkeleton()) {
        Log::editor()->warn(
            "Spawn enemigo demo: 'meshes/CesiumMan.glb' no tiene esqueleto");
        return;
    }

    Entity enemy = m_scene->createEntity("Enemigo");
    auto& t = enemy.getComponent<TransformComponent>();
    t.scale = glm::vec3(1.0f);
    // Orientacion segun el rootNode del .glb: para CesiumMan (Z-up)
    // queda -90° X automaticamente; para Fox queda 0.
    t.rotationEuler = asset->importRotationEuler;
    // Piso al ras: usamos el AABB rotado a world-Y para anclar los
    // pies a y=0.
    const auto wy = rotatedAabbWorldY(asset->aabbMin, asset->aabbMax,
                                        asset->importRotationEuler);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    t.position = glm::vec3(
        origin.x + 1.5f * tileSize,
        origin.y + (-wy.minY),
        origin.z + 1.5f * tileSize);

    // Hito 26: usa las texturas extraidas del .glb si las hay (CesiumMan
    // trae diffuse embedded). Si la extraccion fallo, cae al default
    // material clone (chequer magenta = warning visible).
    auto enemyMats = m_assetManager->createMaterialsForMesh(enemyMesh);
    enemy.addComponent<MeshRendererComponent>(enemyMesh, std::move(enemyMats));

    // Animator con clipName vacio = primer clip del MeshAsset.
    // CesiumMan tiene un solo clip de caminata.
    AnimatorComponent anim{};
    anim.clipName = "";
    anim.playing  = true;
    anim.loop     = true;
    enemy.addComponent<AnimatorComponent>(anim);
    enemy.addComponent<SkeletonComponent>(SkeletonComponent{});

    // NavAgent: el target lo settea EditorApplication cada frame al
    // playCamera.position(). Speed conservadora (2 m/s = caminata).
    NavAgentComponent nav{};
    nav.speed = 2.0f;
    nav.active = true;
    enemy.addComponent<NavAgentComponent>(nav);

    Log::editor()->info(
        "Spawned enemigo demo en (1.5, 1.5) tiles. Entrar en Play Mode "
        "para que persiga al jugador.");
    pushCreatedEntities({enemy}, "Spawn enemigo demo");
}

void EditorApplication::processSpawnHudDemoRequest() {
    if (!(m_ui.consumeSpawnHudDemoRequest() && m_scene)) return;
    // No tiene mesh visible — la entidad existe solo para hostear el
    // ScriptComponent. En Editor Mode aparece como icono Audio/Light? No
    // tiene componente especial, asi que no aparece en el overlay; queda
    // solo en el Hierarchy. En Play Mode el script ejercita la tabla
    // `hud` y los efectos se ven en el HUD/menu de pausa del juego.
    Entity e = m_scene->createEntity("HudDemo");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 0.0f, 0.0f);
    e.addComponent<ScriptComponent>(std::string{"assets/scripts/hud_demo.lua"});
    Log::editor()->info(
        "Spawned HudDemo. Entrar en Play Mode para ver el HUD reaccionar "
        "(HP drena 1/s; HP=0 -> pausa forzada).");
    pushCreatedEntities({e}, "Spawn HUD demo");
}

void EditorApplication::processSpawnPhysicsBoxRequest() {
    if (!(m_ui.consumeSpawnPhysicsBoxRequest() && m_scene && m_assetManager)) return;
    // Hito 41 fix: tag unico para que SaveLoad pueda matchear cada caja
    // por separado. Con tags duplicados los snapshots se sobrescribian
    // en el byTag map del load → cajas quedaban apiladas y Jolt las
    // disparaba como si "desaparecieran".
    int suffix = 1;
    std::string tagName;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "CajaFisica_%02d", suffix);
        tagName = buf;
        bool collision = false;
        m_scene->forEach<TagComponent>(
            [&](Entity, TagComponent& tag) {
                if (tag.name == tagName) collision = true;
            });
        if (!collision) break;
        ++suffix;
    }
    Entity box = m_scene->createEntity(tagName);
    auto& t = box.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 6.0f, 0.0f);
    t.scale    = glm::vec3(1.0f);
    // Material instance unico por caja (ver nota en processSpawnRotatorRequest).
    const MaterialAssetId boxMat =
        m_assetManager->createMaterialFromTexture(m_wallTextureId);
    box.addComponent<MeshRendererComponent>(
        m_assetManager->missingMeshId(), boxMat);
    box.addComponent<RigidBodyComponent>(
        RigidBodyComponent::Type::Dynamic,
        RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f, 0.5f, 0.5f),
        5.0f);
    Log::physics()->info("Spawned caja fisica en (0, 6, 0) con mass=5kg");
    pushCreatedEntities({box}, "Spawn caja fisica");
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
        pushCreatedEntities({env}, "Spawn Environment");
    }
}

void EditorApplication::processSpawnShadowDemoRequest() {
    if (!(m_ui.consumeSpawnShadowDemoRequest() && m_scene && m_assetManager)) return;

    // Texturas: grid para el piso (lectura clara de la sombra), brick para la
    // columna. Ambos ya estan cargados durante buildInitialTestMap, asi que
    // loadTexture deberia ser un cache hit.
    const TextureAssetId gridTex  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brickTex = m_assetManager->loadTexture("textures/brick.png");
    // Materiales unicos por entidad del demo: si el dev quiere editar el
    // tint de la columna sin afectar al piso, debe ser asi (ver nota en
    // processSpawnRotatorRequest).
    const MaterialAssetId gridMat  =
        m_assetManager->createMaterialFromTexture(gridTex);
    const MaterialAssetId brickMat =
        m_assetManager->createMaterialFromTexture(brickTex);
    const MeshAssetId    cube     = m_assetManager->missingMeshId(); // cubo primitivo

    std::vector<Entity> created;
    created.reserve(3);

    // 1) Piso plano grande (cubo escalado y=0.1). Centrado en el origen, se
    //    apoya con su cara superior en y=0.1 — un poco encima del suelo de
    //    tiles (y=0.0) para evitar z-fighting si conviven.
    {
        Entity floor = m_scene->createEntity("DemoSombras_Piso");
        auto& t = floor.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 0.05f, 0.0f);
        t.scale    = glm::vec3(20.0f, 0.1f, 20.0f);
        floor.addComponent<MeshRendererComponent>(cube, gridMat);
        created.push_back(floor);
    }

    // 2) Columna alta (cubo 1x4x1) que proyecta una sombra alargada bien
    //    visible sobre el piso.
    {
        Entity col = m_scene->createEntity("DemoSombras_Columna");
        auto& t = col.getComponent<TransformComponent>();
        t.position = glm::vec3(2.0f, 2.0f, 0.0f);
        t.scale    = glm::vec3(1.0f, 4.0f, 1.0f);
        col.addComponent<MeshRendererComponent>(cube, brickMat);
        created.push_back(col);
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
        created.push_back(sun);
    }

    Log::editor()->info(
        "Spawned demo de sombras: piso 20x20, columna 1x4x1 en (2, 2, 0), "
        "sol direccional con castShadows=true");
    pushCreatedEntities(std::move(created), "Spawn demo sombras");
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

    std::vector<Entity> created;
    created.reserve(9);
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
            created.push_back(sph);
        }
    }
    Log::editor()->info(
        "Spawned showcase PBR grid 3x3 ({} esferas): metallic en X "
        "(0/0.5/1), roughness en Y (0.05/0.5/1)", created.size());
    pushCreatedEntities(std::move(created), "Spawn esferas PBR");
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

    std::vector<Entity> created;
    created.reserve(static_cast<size_t>(kRows * kCols));
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const f32 hue = static_cast<f32>(created.size()) / 64.0f;
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
            created.push_back(light);
        }
    }
    Log::editor()->info(
        "Spawned stress test {} point lights ({}x{} grid, radius={}m)",
        created.size(), kCols, kRows, kRadius);
    pushCreatedEntities(std::move(created), "Spawn stress test 64 luces");
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

    // Hito 26: usa las texturas extraidas del .glb si las hay (Fox trae
    // diffuse embedded). Cada spawn tiene su propio material instance.
    auto foxMats = m_assetManager->createMaterialsForMesh(foxMesh);
    fox.addComponent<MeshRendererComponent>(foxMesh, std::move(foxMats));

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
    pushCreatedEntities({fox}, "Spawn personaje animado");
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
    pushCreatedEntities({light}, "Spawn luz puntual");
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
    pushCreatedEntities({src}, "Spawn audio source");
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

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    // F2H15: prioridad 1 — si hay un BrushComponent bajo el cursor,
    // asignar la textura al brush (crear material wrapper). Esto
    // evita que el drop sobre un brush genere un tile-pared en el
    // grid de abajo.
    const auto texId = static_cast<TextureAssetId>(drop.textureId);
    if (m_scene && m_assetManager) {
        ScenePickResult brushHit = pickEntity(*m_scene, view, projection,
            glm::vec2(drop.ndcX, drop.ndcY),
            m_assetManager.get());
        if (brushHit && brushHit.entity.hasComponent<BrushComponent>()) {
            const MaterialAssetId matId =
                m_assetManager->createMaterialFromTexture(texId);
            auto& bc = brushHit.entity.getComponent<BrushComponent>();
            bc.material = matId;
            bc.dirty = true;
            m_ui.setSelectedEntity(brushHit.entity);
            Log::editor()->info(
                "Drop textura id={} -> brush '{}' (material {})",
                drop.textureId,
                brushHit.entity.hasComponent<TagComponent>()
                    ? brushHit.entity.getComponent<TagComponent>().name
                    : std::string{"(sin tag)"},
                matId);
            markDirty();
            return;
        }
    }

    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(drop.ndcX, drop.ndcY));
    if (!hit.hit) return;

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

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(mdrop.ndcX, mdrop.ndcY));
    if (!hit.hit) return;

    const auto meshId = static_cast<MeshAssetId>(mdrop.meshId);
    const MeshAsset* asset = m_assetManager->getMesh(meshId);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    // Nombre incremental por si spawneamos varios del mismo mesh.
    const std::string meshName = m_assetManager->meshPathOf(meshId);
    Entity e = m_scene->createEntity("Mesh_" + meshName);
    auto& t = e.getComponent<TransformComponent>();

    // Aplicar la rotacion de import primero — el AABB rotado nos da
    // la altura correcta para autoscale + floor offset (CesiumMan
    // tipo Z-up tiene altura en Z; si usaramos aabb.y crudo seria el
    // ancho de brazos y los calculos de abajo darian basura).
    const glm::vec3 importEuler = (asset != nullptr)
        ? asset->importRotationEuler : glm::vec3(0.0f);
    t.rotationEuler = importEuler;

    // Auto-escala bidireccional (Hito 28 G):
    //  - Meshes en cm-units (Fox.glb bind pose ~150 unidades, height >3m
    //    en metros): downscale a ~1.5m de altura.
    //  - Meshes ultra-chicos (height <0.1m, probablemente cm-units que
    //    quedaron sub-metro tras un import): upscale a ~1.5m.
    //  - Meshes en su escala nativa razonable (0.1m..3m: Kenney barriles
    //    de 0.27m, props normales, cubos): SIN autoscale. Antes un
    //    Kenney barril recibia scale 5.4x (1.5/0.276) y se veia grueso
    //    contra los tiles del mundo. Ahora se respeta el authoring.
    //  Si el dev quiere ajustar la escala despues del drop, usa el
    //  gizmo de scale (Ctrl+R) — y desde Hito 27 es undoable.
    f32 autoScale = 1.0f;
    WorldYBounds wy{};
    if (asset != nullptr) {
        wy = rotatedAabbWorldY(asset->aabbMin, asset->aabbMax, importEuler);
        const f32 height = wy.maxY - wy.minY;
        if (height > 3.0f) {
            autoScale = 1.5f / height;
        } else if (height > 0.001f && height < 0.1f) {
            autoScale = 1.5f / height;
        }
    }
    t.scale = glm::vec3(autoScale);

    // Posicion: XZ en el centro del tile bajo el cursor, Y al ras del
    // piso (world y=0). Anclamos el bottom del AABB rotado al piso.
    const f32 yFloorOffset = -autoScale * wy.minY;
    t.position = glm::vec3(
        origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
        origin.y + yFloorOffset,
        origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);

    // Hito 26: usa las texturas extraidas del archivo si las hay
    // (Kenney Survival Kit, Fox.glb, etc). Cada drop tiene materiales
    // instance unicos para no contagiar edits.
    auto mats = m_assetManager->createMaterialsForMesh(meshId);
    e.addComponent<MeshRendererComponent>(meshId, std::move(mats));

    // Hito 19: si el mesh tiene esqueleto + animaciones, auto-agregar
    // Animator + Skeleton para que se anime al instante. Sin esto el
    // mesh queda en bind pose estatico, lo cual es confuso para el
    // usuario que espera ver el zorro caminar.
    if (asset != nullptr && asset->hasSkeleton() && !asset->animations.empty()) {
        AnimatorComponent anim{};
        anim.clipName = "";   // primer clip
        anim.playing  = true;
        anim.loop     = true;
        e.addComponent<AnimatorComponent>(anim);
        e.addComponent<SkeletonComponent>(SkeletonComponent{});
    }

    Log::editor()->info(
        "Drop mesh '{}' -> tile ({}, {}), autoscale={:.4f}{}",
        meshName, hit.tileX, hit.tileY, autoScale,
        (asset && asset->hasSkeleton()) ? " (skinned + animator)" : "");
    pushCreatedEntities({e}, "Drop mesh '" + meshName + "'");
}

void EditorApplication::processViewportPrefabDrop() {
    const ViewportPanel::PrefabDrop pdrop = m_ui.viewport().consumePrefabDrop();
    if (!(pdrop.pending && m_mode == EditorMode::Editor && m_scene && m_assetManager)) return;

    const float aspect = viewportAspect();
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
    pushCreatedEntities({e}, "Drop prefab '" + prefabPath + "'");
}

void EditorApplication::processViewportMaterialDrop() {
    const ViewportPanel::MaterialDrop drop = m_ui.viewport().consumeMaterialDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = viewportAspect();
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

    const auto matId = static_cast<MaterialAssetId>(drop.materialId);

    // F2H15: si la entidad bajo el cursor tiene BrushComponent,
    // asignar el material al brush (no al MeshRenderer). Brushes
    // CSG no tienen MeshRendererComponent.
    if (hit.entity.hasComponent<BrushComponent>()) {
        auto& bc = hit.entity.getComponent<BrushComponent>();
        bc.material = matId;
        bc.dirty = true;
        m_ui.setSelectedEntity(hit.entity);
        const std::string tagName = hit.entity.hasComponent<TagComponent>()
            ? hit.entity.getComponent<TagComponent>().name
            : std::string{"(sin tag)"};
        Log::editor()->info("Drop material id {} -> brush '{}'",
                              drop.materialId, tagName);
        markDirty();
        return;
    }

    if (!hit.entity.hasComponent<MeshRendererComponent>()) {
        Log::editor()->info("Drop material: la entidad no tiene MeshRenderer");
        return;
    }

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

void EditorApplication::processViewportScriptDrop() {
    ViewportPanel::ScriptDrop drop = m_ui.viewport().consumeScriptDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    ScenePickResult hit = pickEntity(*m_scene, view, projection,
        glm::vec2(drop.ndcX, drop.ndcY),
        m_assetManager.get());
    if (!hit) {
        Log::editor()->info("Drop script: no hay entidad bajo el cursor");
        return;
    }

    // Asignar ScriptComponent. Si la entidad ya tenia uno, lo
    // sobreescribimos (replace path). entt permite get-or-emplace via
    // emplace_or_replace, pero la API de Entity no lo expone — usamos
    // hasComponent + addComponent / get.
    const std::string fullPath = std::string("assets/") + drop.scriptPath;
    if (hit.entity.hasComponent<ScriptComponent>()) {
        auto& sc = hit.entity.getComponent<ScriptComponent>();
        sc.path      = fullPath;
        sc.loaded    = false; // forzar recarga via mtime check
        sc.lastError.clear();
    } else {
        hit.entity.addComponent<ScriptComponent>(fullPath);
    }
    m_ui.setSelectedEntity(hit.entity);

    const std::string tagName = hit.entity.hasComponent<TagComponent>()
        ? hit.entity.getComponent<TagComponent>().name
        : std::string{"(sin tag)"};
    Log::editor()->info("Drop script '{}' -> entidad '{}'",
                         drop.scriptPath, tagName);
    markDirty();
}

void EditorApplication::processSpawnFireParticlesRequest() {
    if (!(m_ui.consumeSpawnFireParticlesRequest() && m_scene
          && m_assetManager)) return;

    Entity e = m_scene->createEntity("Fuego");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = glm::vec3(0.0f, 0.5f, 0.0f);

    ParticleEmitterComponent em{};
    em.emitRate     = 60.0f;
    em.lifetimeMin  = 1.0f;
    em.lifetimeMax  = 1.5f;
    em.velocityMin  = glm::vec3(-0.4f, 1.2f, -0.4f);
    em.velocityMax  = glm::vec3( 0.4f, 2.0f,  0.4f);
    em.sizeStart    = 0.30f;
    em.sizeEnd      = 0.05f;
    em.colorStart   = glm::vec4(1.0f, 0.75f, 0.20f, 1.0f);
    em.colorEnd     = glm::vec4(1.0f, 0.10f, 0.0f, 0.0f);
    em.gravityFactor = -0.05f;     // negativo = sube ligeramente
    em.maxParticles = 256;
    em.emitting     = true;
    em.additive     = true;
    em.texture = m_assetManager->loadTexture("textures/particle_fire.png");

    e.addComponent<ParticleEmitterComponent>(em);

    Log::editor()->info(
        "Spawned demo de particulas (fuego) en (0, 0.5, 0): rate={}, "
        "lifetime={:.1f}-{:.1f}s, additive blend",
        em.emitRate, em.lifetimeMin, em.lifetimeMax);
    pushCreatedEntities({e}, "Spawn particulas demo");
}

void EditorApplication::processSpawnTriggerRequest() {
    if (!(m_ui.consumeSpawnTriggerRequest() && m_scene)) return;

    Entity e = m_scene->createEntity("Trigger demo");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = glm::vec3(0.0f, 1.0f, 0.0f);

    TriggerComponent tc{};
    tc.halfExtents = glm::vec3(1.0f, 1.0f, 1.0f);  // 2x2x2m
    e.addComponent<TriggerComponent>(tc);

    // Adjuntamos el script demo (loguea enter/exit). El usuario puede
    // sobreescribir el path desde el Inspector si quiere otro callback.
    e.addComponent<ScriptComponent>(std::string{"assets/scripts/trigger_demo.lua"});

    Log::editor()->info(
        "Spawned trigger demo en (0, 1, 0) con halfExtents=(1,1,1) + "
        "script trigger_demo.lua. Solo dispatcha en Play Mode.");
    pushCreatedEntities({e}, "Spawn trigger demo");
}

void EditorApplication::processSpawnStressTrisRequest() {
    const int targetTris = m_ui.consumeSpawnStressTrisRequest();
    if (targetTris <= 0 || !m_scene || !m_assetManager) return;

    // Cubo = 12 tris. Numero de cubos para alcanzar el target.
    constexpr int k_trisPerCube = 12;
    const int cubeCount = (targetTris + k_trisPerCube - 1) / k_trisPerCube;

    // Grid 3D centrado en el origen. side = ceil(cbrt(cubeCount)) +
    // spacing 2.0m (un cubo de lado 1m con holgura). Centramos sobre
    // la altura y=2 para que no se pisen con el piso del demo de sombras.
    const int side = static_cast<int>(std::ceil(std::cbrt(
        static_cast<double>(cubeCount))));
    const f32 spacing = 2.0f;
    const f32 half = static_cast<f32>(side - 1) * 0.5f * spacing;

    const MeshAssetId cubeMesh = m_assetManager->missingMeshId();
    // Material default unico compartido — en este test no nos importa que
    // tengan materiales independientes (lo que medimos es draw call cost
    // + scene iteration, no Inspector).
    const MaterialAssetId mat =
        m_assetManager->createMaterialFromTexture(m_wallTextureId);

    std::vector<Entity> created;
    created.reserve(static_cast<usize>(cubeCount));
    int idx = 0;
    for (int yi = 0; yi < side && idx < cubeCount; ++yi) {
        for (int zi = 0; zi < side && idx < cubeCount; ++zi) {
            for (int xi = 0; xi < side && idx < cubeCount; ++xi) {
                char name[64];
                std::snprintf(name, sizeof(name), "StressCube_%06d", idx);
                Entity cube = m_scene->createEntity(name);
                auto& t = cube.getComponent<TransformComponent>();
                t.position = glm::vec3(
                    static_cast<f32>(xi) * spacing - half,
                    static_cast<f32>(yi) * spacing + 2.0f,
                    static_cast<f32>(zi) * spacing - half);
                t.scale = glm::vec3(1.0f);
                cube.addComponent<MeshRendererComponent>(cubeMesh, mat);
                created.push_back(cube);
                ++idx;
            }
        }
    }
    Log::editor()->info(
        "Spawned stress test {} cubos ({} tris) en grid {}x{}x{} "
        "centrado en origen, spacing {:.1f}m",
        cubeCount, cubeCount * k_trisPerCube, side, side, side, spacing);
    pushCreatedEntities(std::move(created), "Spawn stress test");
}

void EditorApplication::pushCreatedEntities(std::vector<Entity> created,
                                              std::string label) {
    if (created.empty()) return;

    CreateEntityCommand::BodyCleanup cleanup;
    if (m_physicsWorld) {
        PhysicsWorld* pw = m_physicsWorld.get();
        cleanup = [pw](u32 bodyId) { pw->destroyBody(bodyId); };
    }
    auto cmd = std::make_unique<CreateEntityCommand>(
        std::move(created), m_scene.get(), m_assetManager.get(),
        std::move(cleanup), std::move(label));
    m_history.push(std::move(cmd));
    markDirty();
}

} // namespace Mood
