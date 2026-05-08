#pragma once

// F2H26: componente que representa la mesh estatica unificada de TODO
// el mapa, precompilada por `Csg::compileMap` al guardar el `.moodmap`.
// El MoodPlayer la usa en runtime para skipear el procesamiento CSG
// brush-por-brush — habilita "brushes solo en el editor" del plan
// original F2H14. El Editor NO lo crea; sigue editando los brushes
// individuales via `BrushComponent`.
//
// La mesh se sube al GPU al cargar el mapa (en `SceneLoader` cuando
// el flag `useCompiledMesh=true`). Es estatica: no se regenera en
// runtime. Solo se actualiza cuando el editor re-guarda el mapa.
//
// Mismo patron move-only que `BrushComponent` (F2H17): el
// `unique_ptr<IMesh>` impone delete del copy ctor. La entidad que la
// monta tiene un solo `CompiledMeshComponent` por mapa (no se
// instancia mas de uno).

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h"  // MaterialAssetId
#include "engine/render/rhi/IMesh.h"

#include <memory>
#include <vector>

namespace Mood {

struct CompiledMeshComponent {
    /// @brief 1 IMesh por submesh (1 por path de material distinto en
    ///        uso). Cada submesh se dibuja como 1 draw call con su
    ///        material correspondiente. Mismo patron que
    ///        BrushComponent.meshCache (F2H17).
    std::vector<std::unique_ptr<IMesh>> meshes;
    /// @brief Paralelo a `meshes`. `materials[i]` es el material que
    ///        bindea el SceneRenderer antes de dibujar `meshes[i]`.
    std::vector<MaterialAssetId> materials;

    CompiledMeshComponent() = default;
    CompiledMeshComponent(const CompiledMeshComponent&) = delete;
    CompiledMeshComponent& operator=(const CompiledMeshComponent&) = delete;
    CompiledMeshComponent(CompiledMeshComponent&&) = default;
    CompiledMeshComponent& operator=(CompiledMeshComponent&&) = default;
};

} // namespace Mood
