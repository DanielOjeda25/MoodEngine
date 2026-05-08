#pragma once

// Serializacion de mapas (.moodmap) a/desde JSON. Las texturas y meshes se
// guardan por path logico (string) porque los asset ids son volatiles entre
// sesiones.
//
// Schema v2 (Hito 10):
//   {
//     "version": 2,
//     "name": "prueba_8x8",
//     "width": 8,
//     "height": 8,
//     "tileSize": 3.0,
//     "tiles": [
//       {"type": "solid_wall", "texture": "textures/grid.png"},
//       {"type": "empty",      "texture": "textures/missing.png"},
//       ...
//     ],
//     "entities": [   // nuevo en v2 — entidades no-tile spawneadas (meshes)
//       {
//         "tag": "Mesh_meshes/pyramid.obj",
//         "transform": {
//           "position":      [x, y, z],
//           "rotationEuler": [x, y, z],
//           "scale":         [x, y, z]
//         },
//         "mesh_renderer": {
//           "mesh_path": "meshes/pyramid.obj",
//           "materials": ["textures/brick.png", ...]
//         }
//       },
//       ...
//     ]
//   }
// `tiles` tiene `width*height` entradas en orden row-major (y*width + x).
// `entities` solo contiene entidades cuyo tag NO empieza con "Tile_" (esas
// se re-crean desde `GridMap` via `rebuildSceneFromMap`). Archivos v1 se
// leen igual (entities queda vacio).

#include "core/Types.h"
#include "engine/scripting/exposed/ExposedProperty.h"
#include "engine/world/grid/GridMap.h"

#include <glm/vec2.hpp>  // F2H15: SavedBrushFace.uvOffset/uvScale
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mood {

class AssetManager;
class Scene;

/// @brief Copia persistida de un MeshRendererComponent para round-trip.
///        `meshPath` es el path logico que resuelve `AssetManager::loadMesh`;
///        fallback a missing (cubo) si el archivo no existe.
///        `materials` guarda un path logico por submesh; entradas vacias o
///        faltantes caen al slot 0 del AssetManager (missing.png).
struct SavedMeshRenderer {
    std::string meshPath;
    std::vector<std::string> materials;
};

/// @brief Copia persistida de un LightComponent (Hito 11).
///        `type` se guarda como string ("directional"/"point") para no atar
///        el archivo a la enumeracion del enum.
struct SavedLight {
    std::string type{"point"}; // "directional" | "point"
    glm::vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 radius = 10.0f;                    // solo Point
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // solo Directional
    bool enabled = true;
    bool castShadows = false;              // solo Directional (Hito 16)
};

/// @brief Copia persistida de un RigidBodyComponent (Hito 12).
///        `type` y `shape` como string para robustez frente a renumeracion.
///        La pose actual NO se persiste: Jolt es authoritative en runtime
///        y al cargar, el body se crea en la posicion del Transform.
struct SavedRigidBody {
    std::string type{"dynamic"};     // "static" | "kinematic" | "dynamic"
    std::string shape{"box"};        // "box" | "sphere" | "capsule"
    glm::vec3 halfExtents{0.5f};
    f32 mass = 1.0f;
    f32 friction = 0.5f;             // Hito 34 A — opcional en JSON, default = 0.5
};

/// @brief Copia persistida de un EnvironmentComponent (Hito 15).
///        Mismos campos que el componente runtime. `fogMode` y
///        `tonemapMode` como string por estabilidad ante renumeraciones.
struct SavedEnvironment {
    std::string skyboxPath{"skyboxes/sky_day"};
    std::string fogMode{"off"};            // "off" | "linear" | "exp" | "exp2"
    glm::vec3 fogColor{0.55f, 0.65f, 0.75f};
    f32 fogDensity = 0.015f;
    f32 fogLinearStart = 5.0f;
    f32 fogLinearEnd = 50.0f;
    f32 exposure = 0.0f;
    std::string tonemapMode{"aces"};       // "none" | "reinhard" | "aces"
    f32 iblIntensity = 1.0f;               // Hito 18, opcional en JSON
};

/// @brief Copia persistida de un ScriptComponent (Hito 24). Persiste el
///        path logico del `.lua` + el map de overrides de exposed
///        properties (name → value). Tipos serializables: number, bool,
///        string, vec3 (los del `ExposedValue` variant). Estado runtime
///        (loaded, lastError, exposedProps descubiertas) no se persiste.
struct SavedScript {
    std::string path;
    std::unordered_map<std::string, ExposedValue> overrides;
};

/// @brief Copia persistida de un TriggerComponent (Hito 33).
///        Solo `halfExtents`; el flag runtime `playerInside` se reinicia
///        al cargar (la AABB-test del primer frame redetectara estado).
struct SavedTrigger {
    glm::vec3 halfExtents{1.0f, 1.0f, 1.0f};
};

/// @brief Copia persistida de un ParticleEmitterComponent (Hito 29).
///        Solo la configuracion editable; el estado runtime (positions,
///        ages, rngState, etc.) no se persiste — la simulacion arranca
///        de cero al cargar.
struct SavedParticleEmitter {
    f32 emitRate    = 60.0f;
    f32 lifetimeMin = 1.0f;
    f32 lifetimeMax = 1.5f;
    glm::vec3 velocityMin{-0.4f, 1.0f, -0.4f};
    glm::vec3 velocityMax{ 0.4f, 2.0f,  0.4f};
    f32 sizeStart = 0.30f;
    f32 sizeEnd   = 0.05f;
    glm::vec4 colorStart{1.0f, 0.75f, 0.2f, 1.0f};
    glm::vec4 colorEnd  {1.0f, 0.10f, 0.0f, 0.0f};
    f32 gravityFactor = 0.0f;
    std::string texturePath;             // vacio = sin textura (cae a missing)
    u32 maxParticles = 256;
    bool emitting = true;
    bool additive = false;
    bool localSpace = false;             // Hito 31 F
    // Hito 37 C: shape de emision. "point" | "box" | "sphere" | "disc".
    // Default "point" = comportamiento Hito 29. Solo se persiste en JSON
    // si != "point" (back-compat con mapas viejos).
    std::string emissionShape{"point"};
    f32         emissionShapeSize = 1.0f;
    // Hito 40 A: axis del cono. Default +Y.
    glm::vec3   emissionConeAxis{0.0f, 1.0f, 0.0f};
};

/// @brief Copia persistida de una entidad no-tile. Hito 10 agrego mesh
///        renderer; Hito 11 agrega light; Hito 12 agrega rigid body;
///        Hito 14 agrega prefabPath (link suave al asset del que se
///        instancio, vacio si no vino de prefab); Hito 24 agrega script
///        (path + overrides de exposed properties).
///        Otros componentes (Audio) siguen sin persistirse.
struct SavedEntity {
    std::string tag;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 scale{1.0f};
    std::optional<SavedMeshRenderer> meshRenderer;
    std::optional<SavedLight> light;
    std::optional<SavedRigidBody> rigidBody;
    std::optional<SavedEnvironment> environment; // Hito 15
    std::optional<SavedScript> script;            // Hito 24
    std::optional<SavedParticleEmitter> particleEmitter; // Hito 29
    std::optional<SavedTrigger> trigger;                  // Hito 33
    std::string prefabPath; // Hito 14: vacio = no vino de prefab
};

/// @brief F2H11+F2H15: copia persistida de una BrushFace. Guarda el
///        plano + materialIndex (F2H11) + UV params per-cara (F2H15).
///        Faces v10 sin UV params se leen con defaults sensatos
///        (uAxis/vAxis auto desde la normal en SceneLoader).
struct SavedBrushFace {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    f32       distance = 0.0f;
    u32       materialIndex = 0;
    // F2H15: UV params. Si todos son default y el JSON no los
    // tiene, no se escriben (campos opcionales).
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, 1.0f, 0.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};
    glm::vec2 uvScale{1.0f, 1.0f};
    f32       uvRotation = 0.0f;
    bool      lockToWorld = false;
};

/// @brief F2H11: copia persistida de un BrushComponent. La AABB no se
///        persiste (se recomputa al rebuild de la mesh). El `dirty`
///        tampoco — al cargar siempre arranca dirty=true. El material
///        global del brush (F2H11) viaja como path logico de
///        `.material`; vacio = sin material asignado (look "blank
///        gris"). En F2H14 esto se reemplaza por material per-cara.
struct SavedBrush {
    std::string tag;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 scale{1.0f};
    /// @brief F2H11 (v11): material singular global. Preserved como
    ///        slot 0 cuando se carga.
    std::string materialPath;
    /// @brief F2H17 (v12): slots de material per-cara. Cada
    ///        `face.materialIndex` indexea este array. Si esta
    ///        vacio (mapas v11 sin el campo), el loader sintetiza
    ///        `materialPaths = [materialPath]`. Si v12 con materialPaths
    ///        no-vacio, materialPath se ignora (el slot 0 es
    ///        materialPaths[0]).
    std::vector<std::string> materialPaths;
    std::vector<SavedBrushFace> faces;
};

/// @brief F2H26: submesh de la mesh estatica precompilada del mapa.
///        `vertices` es interleaved con 11 floats por vertice
///        (pos.xyz + color.rgb + uv.xy + normal.xyz) — el mismo
///        layout que `Csg::brushSubmeshToInterleaved` produce, asi el
///        SceneRenderer reusa el shader PBR estandar y los IMesh se
///        crean con el mismo `kPbrAttrs` que los brushes runtime.
///        Indices ya estan expandidos (sin EBO; cada vertex aparece
///        repetido por triangulo). `materialPath` es el path logico
///        del material (mismo formato que SavedBrush.materialPaths).
struct SavedCompiledSubmesh {
    std::string materialPath;
    std::vector<f32> vertices;  // interleaved 11 floats per vertex
};

/// @brief F2H26: la mesh estatica precompilada de TODO el mapa,
///        producida por `Csg::compileMap` al guardar. Cada submesh
///        agrupa caras con el mismo material. El Player la usa
///        directamente; el Editor la escribe pero la ignora para
///        render (sigue editando los `BrushComponent`).
struct SavedCompiledMesh {
    std::vector<SavedCompiledSubmesh> submeshes;
};

struct SavedMap {
    std::string name;
    GridMap map;
    std::vector<SavedEntity> entities;
    std::vector<SavedBrush> brushes;     // F2H11
    /// @brief F2H26: opcional. Si presente, el Player la prefiere
    ///        sobre los brushes individuales. Mapas v12 (sin compile)
    ///        leen este campo como nullopt.
    std::optional<SavedCompiledMesh> compiledMesh;
};

/// @brief F2H26: construye un `SavedCompiledMesh` a partir de los
///        brushes presentes en la scene actual. Llama internamente a
///        `Csg::collectBrushSourcesFromScene` + `Csg::compileMap` y
///        convierte cada submesh al layout PBR interleaved (11 floats
///        por vertex). El editor lo usa antes de `SceneSerializer::save`
///        para persistir la mesh compilada en el `.moodmap`.
SavedCompiledMesh buildSavedCompiledMeshFromScene(Scene& scene,
                                                    AssetManager& assets);

class SceneSerializer {
public:
    /// @brief Guarda `map` + entidades no-tile de `scene` como JSON en `path`.
    ///        Las texturas/meshes se escriben como paths logicos via
    ///        `assets.pathOf(id)` / `assets.meshPathOf(id)`.
    ///        `scene` puede ser null (tests / flujos legacy que solo manejan
    ///        tiles): en ese caso `entities` queda vacio en el JSON.
    /// @param compiledMesh F2H26: opcional. Si no es null, se persiste
    ///        bajo `compiledMesh` en el JSON. El Player la usa al cargar
    ///        para skipear el procesamiento CSG. El editor lo construye
    ///        via `Csg::compileMap` antes de llamar a save.
    /// @throws std::runtime_error si no se puede abrir el archivo para escritura.
    static void save(const GridMap& map, const std::string& name,
                     const Scene* scene, const AssetManager& assets,
                     const std::filesystem::path& path,
                     const SavedCompiledMesh* compiledMesh = nullptr);

    /// @brief Carga un mapa desde JSON. Las texturas y meshes se reabren via
    ///        `assets.loadTexture(...)` / `assets.loadMesh(...)` — si fallan,
    ///        caen al missing.
    /// @return `nullopt` si el archivo no existe, esta mal formado, o la
    ///         version declarada supera la soportada. Loguea el motivo en
    ///         el canal `assets`.
    static std::optional<SavedMap> load(const std::filesystem::path& path,
                                        AssetManager& assets);
};

} // namespace Mood
