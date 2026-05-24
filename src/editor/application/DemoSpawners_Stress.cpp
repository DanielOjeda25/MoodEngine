// Stress tests accesibles desde el menu "Debug": LightStress (64 point
// lights en grid) y StressTris (cubos en grid 3D). Pensados para
// benchmarkear perf cuando se toca un subsistema (Forward+, scene
// iteration, draw-call cost).
//
// post-v2.0.2 cleanup: borrados EnemyDemo, ShadowDemo, PbrSpheres,
// AnimatedCharacter, FullStressScene — muletas de desarrollo de Fase 1-2
// sin entry point UI desde F2H57.
//
// Nota historica: nombre del archivo se mantiene "DemoSpawners_Stress" por
// costo de rename (cross-CMakeLists). Renaming queda como polish opcional.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/entity_type/EntityType.h"  // F3H9

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace Mood {

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
            light.getComponent<TagComponent>().entityType = EntityType::Light;  // F3H9
            created.push_back(light);
        }
    }
    Log::editor()->info(
        "Spawned stress test {} point lights ({}x{} grid, radius={}m)",
        created.size(), kCols, kRows, kRadius);
    pushCreatedEntities(std::move(created), "Spawn stress test 64 luces");
}

void EditorApplication::processSpawnStressTrisRequest() {
    const int targetTris = m_ui.consumeSpawnStressTrisRequest();
    if (targetTris <= 0 || !m_scene || !m_assetManager) return;

    // Cubo = 12 tris. Numero de cubos para alcanzar el target.
    constexpr int k_trisPerCube = 12;
    const int cubeCount = (targetTris + k_trisPerCube - 1) / k_trisPerCube;

    // Grid 3D centrado en el origen. side = ceil(cbrt(cubeCount)) +
    // spacing 2.0m (un cubo de lado 1m con holgura). Centramos sobre
    // la altura y=2 para que no se pisen con el piso del editor.
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
                cube.getComponent<TagComponent>().entityType = EntityType::Mesh;  // F3H9
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
