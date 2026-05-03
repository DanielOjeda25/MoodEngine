#pragma once

// MeshLoader (Hito 10): carga de modelos 3D usando assimp.
//
// `loadMeshWithAssimp` es una funcion libre (no clase) porque el loader no
// tiene estado propio: cada llamada crea su `Assimp::Importer` local. El
// uso desde AssetManager es puntual (un path -> un MeshAsset).
//
// `MeshFactory` es inyectable para testear sin GL: la version de produccion
// devuelve `OpenGLMesh`; los tests devuelven mocks que solo guardan el
// vertex count.
//
// Decision: el pipeline actual de render usa `glDrawArrays` (sin EBO), asi
// que el loader expande el index buffer de assimp a triangulos planos. Para
// modelos pequenos (Suzanne, props de prueba) el overhead de RAM es
// despreciable. Cuando aparezcan meshes grandes, migrar IMesh + esta
// expansion juntos a un `OpenGLIndexedMesh` que use EBOs.

#include "core/Types.h"
#include "engine/render/rhi/RendererTypes.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Mood {

class IMesh;
class AssetManager;
struct MeshAsset;

/// @brief Factoria que produce un IMesh a partir de datos interleaved +
///        layout. Igual firma que usa `OpenGLMesh` directo; permite inyectar
///        mocks para tests sin contexto GL.
using MeshFactory =
    std::function<std::unique_ptr<IMesh>(const std::vector<f32>& vertices,
                                          const std::vector<VertexAttribute>& attributes)>;

/// @brief Importa un modelo 3D con assimp y devuelve un MeshAsset con un
///        `SubMesh` por cada `aiMesh` encontrado. Tipos soportados en este
///        hito: OBJ + glTF + FBX (configurado en CMakeLists).
/// @param logicalPath Path logico que se guarda en el MeshAsset (p.ej.
///        "meshes/suzanne.obj"). Lo usa el serializer.
/// @param filesystemPath Path real del archivo (ya resuelto por VFS).
/// @param meshFactory Produce IMesh por submesh. Obligatorio.
/// @param assetManager Opcional (puede ser null para tests sin GL): si esta
///        presente, el loader extrae las texturas albedo de cada material
///        del archivo y las registra en el AssetManager (embedded via
///        `loadEmbeddedTexture`, external via `loadTexture`). Sin esto el
///        mesh carga geometria pero las texturas quedan en 0 -> el spawn
///        cae al material default (chequer magenta del missing).
/// @return MeshAsset pobkado o nullptr si assimp fallo (archivo no existe,
///         formato no soportado, etc.). Loguea al canal `assets`.
std::unique_ptr<MeshAsset> loadMeshWithAssimp(const std::string& logicalPath,
                                                const std::string& filesystemPath,
                                                const MeshFactory& meshFactory,
                                                AssetManager* assetManager = nullptr);

} // namespace Mood
