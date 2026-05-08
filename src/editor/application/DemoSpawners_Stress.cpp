// F2H24: demos pesados / showcase de "Ayuda > Agregar X":
// EnemyDemo (CesiumMan), ShadowDemo, PbrSpheres grid 3x3, LightStress
// (64 point lights), AnimatedChar (Fox), StressTris (cubos en grid).

#include "editor/application/EditorApplication.h"
#include "editor/application/DemoSpawners_Internal.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

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
    const auto wy = detail::rotatedAabbWorldY(asset->aabbMin, asset->aabbMax,
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

} // namespace Mood
