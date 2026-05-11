#pragma once

// AssetManager: catalogo central de assets cargados (Hito 5 Bloque 2).
// Por ahora solo texturas; modelos/shaders/sonidos entran en hitos siguientes.
//
// Los assets se piden por path logico (relativo a la raiz de assets/). Si el
// archivo no existe o la carga falla, se devuelve el id de la textura
// "missing" (chequered rosa/negro) y se loguea en el canal `assets`. No lanza.
//
// El VFS del Bloque 3 va a reemplazar la resolucion simple `assets/<path>`
// por un lookup mas completo; la API publica no cambia.

#include "core/Types.h"
#include "engine/render/rhi/RendererTypes.h"
#include "platform/VFS.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Mood {

class AudioClip;

class IMesh;
class ITexture;
struct MeshAsset;
struct MaterialAsset;
struct SavedPrefab;
struct AnimationClip;  // F2H49

namespace Dialog { class Asset; }  // F2H48

/// @brief Identificador estable de una textura dentro de un `AssetManager`.
///        Valor 0 se reserva para la textura "missing": pedir `getTexture(0)`
///        siempre devuelve algo renderizable.
using TextureAssetId = u32;

/// @brief Identificador estable de un AudioClip. Valor 0 reservado para el
///        clip "missing" (silencio de 100 ms) para que `getAudio(0)` nunca
///        sea null.
using AudioAssetId = u32;

/// @brief Identificador estable de un MeshAsset. Valor 0 reservado para el
///        mesh "missing" (cubo del Hito 3) para que `getMesh(0)` nunca sea
///        null.
using MeshAssetId = u32;

/// @brief Identificador estable de un Prefab (Hito 14). Valor 0 reservado
///        para un "prefab vacio" (root sin componentes), asi `getPrefab(0)`
///        nunca devuelve null. Loadeo lazy: el `.moodprefab` se parsea al
///        invocar `loadPrefab`, no al escanear el directorio.
using PrefabAssetId = u32;

/// @brief Identificador estable de un MaterialAsset (Hito 17). Valor 0
///        reservado para el "default material" (albedo blanco, metallic=0,
///        roughness=0.5) — `getMaterial(0)` nunca es null.
using MaterialAssetId = u32;

/// @brief F2H48: Identificador estable de un DialogAsset (.mooddialog).
///        Valor 0 reservado para el "dialog vacio" (Asset sin nodos, sin
///        start_node) — `getDialog(0)` nunca es null. Loadeo lazy + cache
///        por path logico, mismo patron que Prefab.
using DialogAssetId = u32;

/// @brief F2H49: Identificador estable de un `AnimationClip` cargado standalone
///        (FBX/glTF anim-only, ej. los de Mixamo "Without Skin"). Valor 0
///        reservado para un clip vacio (duracion 0, sin tracks) que el
///        `AnimationSystem` puede recibir sin crashear cuando un asset
///        falla al cargar. Loadeo lazy + cache por path logico.
using AnimationClipAssetId = u32;

class AssetManager {
public:
    /// @brief Factoria de texturas: recibe un path del filesystem (resuelto
    ///        por el VFS) y devuelve una ITexture lista. Inyectable para
    ///        poder testear AssetManager sin contexto GL (mocks).
    using TextureFactory =
        std::function<std::unique_ptr<ITexture>(const std::string& filesystemPath)>;

    /// @brief Factoria de texturas desde memoria (Hito 26): recibe un buffer
    ///        de bytes (PNG/JPG/etc comprimido) + nombre para logs y devuelve
    ///        un ITexture decodificado. Para texturas embedded en .glb que
    ///        no existen como archivo en disco. Si null, `loadEmbeddedTexture`
    ///        falla con missing.
    using TextureMemoryFactory =
        std::function<std::unique_ptr<ITexture>(const std::vector<u8>& bytes,
                                                  const std::string& nameForLog)>;

    /// @brief Factoria de audio clips. Recibe path logico + filesystem y
    ///        devuelve un AudioClip con la metadata poblada. Inyectable para
    ///        testear sin hardware (mocks que devuelven clip con duracion=0).
    using AudioClipFactory =
        std::function<std::unique_ptr<AudioClip>(const std::string& logicalPath,
                                                  const std::string& filesystemPath)>;

    /// @brief Factoria de IMesh (VAO/VBO). Recibe un buffer interleaved de
    ///        floats + layout y devuelve un mesh listo para draw. Produccion
    ///        pasa `OpenGLMesh`; tests inyectan mocks que solo guardan el
    ///        vertex count.
    using MeshFactory =
        std::function<std::unique_ptr<IMesh>(const std::vector<f32>& vertices,
                                              const std::vector<VertexAttribute>& attributes)>;

    /// @brief Construye el manager y carga los fallbacks (missing.png,
    ///        missing.wav y missing mesh = cubo). Si alguno falla, lanza: la
    ///        instalacion esta rota.
    /// @param rootDir Carpeta raiz de assets (default: "assets").
    /// @param textureFactory Factoria de texturas. EditorApplication pasa
    ///        una que crea `OpenGLTexture`. Los tests inyectan mocks.
    /// @param audioFactory Factoria de audio clips. Default: construye
    ///        `AudioClip` directo (no requiere hardware, solo inspecciona
    ///        metadata con ma_decoder). Los tests pueden inyectar stubs.
    /// @param meshFactory Factoria de IMesh. Requerida para poder generar
    ///        el mesh de fallback (cubo primitivo) y para importar modelos
    ///        via assimp.
    AssetManager(std::string rootDir,
                 TextureFactory textureFactory,
                 AudioClipFactory audioFactory = {},
                 MeshFactory meshFactory = {},
                 TextureMemoryFactory textureMemoryFactory = {});
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /// @brief Carga una textura por path logico (ej. "textures/grid.png")
    ///        o devuelve el id cacheado si ya estaba cargada. En fallo,
    ///        devuelve `missingTextureId()` y loguea en `assets`.
    TextureAssetId loadTexture(std::string_view logicalPath);

    /// @brief Registra una textura cargada desde un buffer en memoria
    ///        (Hito 26). Para texturas embedded en .glb extraidas via
    ///        assimp `aiScene::GetEmbeddedTexture`. La cacheKey debe ser
    ///        unica (ej. "<glb_path>#<embeddedName>"). Llamadas repetidas
    ///        con la misma key devuelven el id cacheado. En fallo (memoria
    ///        invalida o sin TextureMemoryFactory) devuelve `missingTextureId()`.
    /// @param cacheKey Identificador unico para la cache.
    /// @param bytes Buffer con el archivo de imagen (PNG/JPG/etc) comprimido.
    TextureAssetId loadEmbeddedTexture(const std::string& cacheKey,
                                         const std::vector<u8>& bytes);

    /// @brief Devuelve el ITexture para el id dado. Nunca es null:
    ///        ids invalidos (fuera de rango) caen al fallback missing.
    ITexture* getTexture(TextureAssetId id) const;

    /// @brief Path logico con el que se cargo la textura. Usado por los
    ///        serializers para persistir la referencia como string estable
    ///        entre sesiones (los ids son volatiles). Id invalido o missing
    ///        devuelve el path del missing.
    std::string pathOf(TextureAssetId id) const;

    /// @brief Id de la textura de fallback. Siempre vale 0.
    TextureAssetId missingTextureId() const { return 0; }

    /// @brief Cantidad de texturas en cache (incluye missing).
    usize textureCount() const { return m_textures.size(); }

    /// @brief Revisa el mtime de cada PNG cargado y re-invoca la factoria
    ///        para las texturas cuyo archivo cambio en disco. Conserva el
    ///        `TextureAssetId` (swapea el unique_ptr); callsites que
    ///        guardaron el id siguen viendo la textura actualizada.
    ///        IMPORTANTE: este metodo borra los GLuint de las texturas
    ///        cambiadas. Llamar ENTRE frames (antes de `beginFrame`) para
    ///        evitar que ImGui dibuje con un handle recien destruido.
    /// @return cantidad de texturas efectivamente recargadas.
    usize reloadChanged();

    // ---- Audio ----

    /// @brief Carga (o devuelve cacheado) un AudioClip por path logico. En
    ///        fallo devuelve `missingAudioId()` (silencio de 100 ms) y loguea
    ///        al canal assets.
    AudioAssetId loadAudio(std::string_view logicalPath);

    /// @brief Devuelve el AudioClip del id. Nunca null: ids invalidos caen
    ///        al fallback missing.
    AudioClip* getAudio(AudioAssetId id) const;

    /// @brief Id del clip fallback ("audio/missing.wav"). Siempre vale 0.
    AudioAssetId missingAudioId() const { return 0; }

    /// @brief Cantidad de clips cacheados (incluye missing).
    usize audioCount() const { return m_audioClips.size(); }

    /// @brief Path logico con el que se cargo un clip (para serializadores y
    ///        el AssetBrowser). Id invalido devuelve el path de missing.
    std::string audioPathOf(AudioAssetId id) const;

    // ---- Mesh (Hito 10) ----

    /// @brief Carga (o devuelve cacheado) un MeshAsset por path logico (p.ej.
    ///        "meshes/suzanne.obj"). En fallo devuelve `missingMeshId()` (cubo
    ///        primitivo) y loguea en el canal assets. Imports via assimp.
    MeshAssetId loadMesh(std::string_view logicalPath);

    /// @brief Devuelve el MeshAsset del id. Nunca null: ids invalidos caen al
    ///        fallback missing (cubo).
    MeshAsset* getMesh(MeshAssetId id) const;

    /// @brief Id del mesh fallback (cubo generado en el constructor). Siempre
    ///        vale 0.
    MeshAssetId missingMeshId() const { return 0; }

    /// @brief Id de la esfera primitiva (Hito 17). Generada en el
    ///        constructor junto con el cubo, slot logico
    ///        "__primitive_sphere". Util para showcase de PBR sin tener
    ///        que importar un .obj.
    MeshAssetId primitiveSphereId() const { return m_primitiveSphereId; }

    /// @brief Path logico asociado al mesh. Slot 0 devuelve el path sentinela
    ///        "__missing_cube" (no existe en disco; solo uso interno del
    ///        serializer para detectar "sin mesh real cargado").
    std::string meshPathOf(MeshAssetId id) const;

    /// @brief Cantidad de meshes cacheados (incluye missing).
    usize meshCount() const { return m_meshes.size(); }

    /// @brief F2H11: crea un IMesh **dinamico** que NO se persiste en
    ///        el cache de meshes. Pensado para geometria runtime
    ///        regenerada (CSG brushes). El llamador es dueno del
    ///        unique_ptr resultante. Si la factoria no esta seteada
    ///        (caso defensivo) devuelve nullptr.
    std::unique_ptr<IMesh> createDynamicMesh(
        const std::vector<f32>& vertices,
        const std::vector<VertexAttribute>& attributes) const;

    // ---- Prefab (Hito 14) ----

    /// @brief Carga (o devuelve cacheado) un prefab por path logico (p.ej.
    ///        "prefabs/torch.moodprefab"). En fallo devuelve `missingPrefabId()`
    ///        (prefab vacio) y loguea al canal `assets`.
    PrefabAssetId loadPrefab(std::string_view logicalPath);

    /// @brief Devuelve el SavedPrefab del id. Nunca null: ids invalidos caen
    ///        al slot 0 (prefab vacio).
    const SavedPrefab* getPrefab(PrefabAssetId id) const;

    /// @brief Id del prefab vacio (placeholder). Siempre vale 0.
    PrefabAssetId missingPrefabId() const { return 0; }

    /// @brief Path logico con el que se cargo el prefab. Slot 0 devuelve el
    ///        sentinela `"__empty_prefab"` (no existe en disco).
    std::string prefabPathOf(PrefabAssetId id) const;

    /// @brief Cantidad de prefabs cacheados (incluye el slot 0).
    usize prefabCount() const { return m_prefabs.size(); }

    // ---- Material (Hito 17) ----

    /// @brief Carga (o devuelve cacheado) un material por path logico
    ///        (p.ej. "materials/brass.material"). En fallo devuelve
    ///        `missingMaterialId()` (default material) y loguea al canal
    ///        `assets`. Las texturas referenciadas se cargan via
    ///        `loadTexture`.
    MaterialAssetId loadMaterial(std::string_view logicalPath);

    /// @brief Crea un material en runtime (sin tocar disco) que envuelve
    ///        una unica textura albedo. Util para el upgrader del .moodmap
    ///        v6 -> v7: cada `texture_path` viejo se envuelve en un
    ///        material auto-generado para preservar la apariencia visual.
    ///        El material tiene `metallicMult=0`, `roughnessMult=1` (mate
    ///        completo) — el `lit.frag` PBR con esos parametros se ve
    ///        practicamente identico al Blinn-Phong del Hito 11.
    /// @param textureId Id de la textura albedo (puede ser 0 = missing).
    /// @return Id de un MaterialAsset cacheado por `texture#<id>` (sin
    ///         conflicto con paths logicos reales que terminan en .material).
    MaterialAssetId loadMaterialFromTexture(TextureAssetId textureId);

    /// @brief Igual que `loadMaterialFromTexture` pero NUNCA cachea: cada
    ///        llamada produce un slot nuevo. Para entidades runtime (cubos
    ///        spawneados, drops, demos) que necesitan material editable
    ///        sin afectar a otras entidades que usan la misma textura.
    /// @param textureId Id de la textura albedo (puede ser 0 = missing).
    /// @return Id estable del material recien creado.
    MaterialAssetId createMaterialFromTexture(TextureAssetId textureId);

    /// @brief Crea un material nuevo con los parametros del prototype.
    ///        Sin cache: cada llamada genera un slot distinto. La cache
    ///        key sintetica (`__runtime#<id>`) evita colisiones con
    ///        materiales cargados desde disco. Util para spawn de
    ///        showcases (Hito 17 esferas PBR) o herramientas que generan
    ///        materiales programaticamente.
    /// @return Id estable del material recien creado.
    MaterialAssetId createMaterial(const MaterialAsset& prototype);

    /// @brief Devuelve el MaterialAsset del id. Nunca null: ids invalidos
    ///        caen al slot 0 (default material).
    MaterialAsset* getMaterial(MaterialAssetId id) const;

    /// @brief Id del default material (slot 0). Albedo blanco, metallic=0,
    ///        roughness=0.5.
    MaterialAssetId missingMaterialId() const { return 0; }

    /// @brief Path logico con el que se cargo el material (para serializar).
    ///        Slot 0 devuelve el sentinela `"__default_material"` (no existe
    ///        en disco). Materiales generados via `loadMaterialFromTexture`
    ///        tienen path interno tipo `"__tex#<id>"`.
    std::string materialPathOf(MaterialAssetId id) const;

    /// @brief F2H21: serializa el material `id` al path logico con el que
    ///        fue cargado (mismo schema que `loadMaterial` lee). Usa el
    ///        VFS para resolver al path FS. Sobreescribe el archivo
    ///        existente. Devuelve true si la escritura fue OK.
    ///
    ///        Falla con false (sin escribir) si:
    ///        - El material no existe (id fuera de rango).
    ///        - El path es un sentinel interno (`__default_material`,
    ///          `__tex#<id>`, `__runtime#<id>`) — esos no se persisten.
    ///        - VFS no resuelve el path (proyecto no abierto / path
    ///          fuera del VFS root).
    ///        - Error de I/O al escribir.
    ///        En todos los casos loguea al canal `assets` con el motivo.
    bool saveMaterial(MaterialAssetId id);

    /// @brief Cantidad de materiales cacheados (incluye slot 0).
    usize materialCount() const { return m_materials.size(); }

    /// @brief Hito 26: arma el vector de MaterialAssetIds para un mesh
    ///        recien spawneado/dropeado, una entrada por submesh. Si el
    ///        material referenciado por el submesh tiene una textura
    ///        albedo extraida del archivo (`mesh->materialAlbedoTextures`),
    ///        crea un `createMaterialFromTexture` (instance unico). Si no,
    ///        clona el default material (mostrara missing como warning).
    ///        Llamarlo SIEMPRE que se construye un `MeshRendererComponent`
    ///        para una entidad nueva.
    std::vector<MaterialAssetId> createMaterialsForMesh(MeshAssetId meshId);

    // ---- Dialog (F2H48) ----

    /// @brief Carga (o devuelve cacheado) un dialog asset por path logico
    ///        (p.ej. "dialogs/intro.mooddialog"). En fallo devuelve
    ///        `missingDialogId()` (asset vacio) y loguea al canal `assets`.
    ///        Mismo patron que `loadPrefab`.
    DialogAssetId loadDialog(std::string_view logicalPath);

    /// @brief Devuelve el Asset del id. Nunca null: ids invalidos caen al
    ///        slot 0 (asset vacio).
    const Dialog::Asset* getDialog(DialogAssetId id) const;

    /// @brief Id del dialog vacio (slot 0). Asset sin nodos.
    DialogAssetId missingDialogId() const { return 0; }

    /// @brief Path logico con el que se cargo el dialog. Slot 0 devuelve
    ///        el sentinela `"__empty_dialog"`.
    std::string dialogPathOf(DialogAssetId id) const;

    /// @brief Cantidad de dialogs cacheados (incluye slot 0).
    usize dialogCount() const;

    // ---- AnimationClip standalone (F2H49) ----

    /// @brief Carga (o devuelve cacheado) un AnimationClip standalone por
    ///        path logico (p.ej. "characters/npc_a/anim_idle.fbx"). En
    ///        fallo devuelve `missingAnimationClipId()` (clip vacio,
    ///        duration=0) y loguea al canal `assets`. Los clips se
    ///        cargan con `loadAnimationClipWithAssimp` — las tracks
    ///        quedan con `boneIndex = -1`; el bind contra un esqueleto
    ///        destino lo hace el `AnimationSystem` al primer frame de
    ///        reproduccion.
    AnimationClipAssetId loadAnimationClip(std::string_view logicalPath);

    /// @brief Devuelve el clip del id. Nunca null: ids invalidos caen al
    ///        slot 0 (clip vacio).
    AnimationClip* getAnimationClip(AnimationClipAssetId id) const;

    /// @brief Id del clip vacio (slot 0). Sin tracks, duration=0.
    AnimationClipAssetId missingAnimationClipId() const { return 0; }

    /// @brief Path logico con el que se cargo. Slot 0 = "__empty_clip".
    std::string animationClipPathOf(AnimationClipAssetId id) const;

    /// @brief Cantidad de clips cacheados (incluye slot 0).
    usize animationClipCount() const;

private:
    VFS m_vfs;
    TextureFactory m_textureFactory;
    TextureMemoryFactory m_textureMemoryFactory;
    AudioClipFactory m_audioFactory;
    MeshFactory m_meshFactory;

    // Texturas (Hito 5).
    std::unordered_map<std::string, TextureAssetId> m_textureCache;
    std::vector<std::unique_ptr<ITexture>> m_textures; // [0] = missing
    std::vector<std::string> m_texturePaths;           // paralelo a m_textures
    std::vector<std::filesystem::file_time_type> m_textureMtimes; // paralelo

    // Audio (Hito 9).
    std::unordered_map<std::string, AudioAssetId> m_audioCache;
    std::vector<std::unique_ptr<AudioClip>> m_audioClips; // [0] = missing.wav

    // Mesh (Hito 10). [0] = cubo primitivo (fallback).
    std::unordered_map<std::string, MeshAssetId> m_meshCache;
    std::vector<std::unique_ptr<MeshAsset>> m_meshes;
    MeshAssetId m_primitiveSphereId = 0; // Hito 17, llenado en el ctor

    // Prefab (Hito 14). [0] = prefab vacio (root sin componentes).
    std::unordered_map<std::string, PrefabAssetId> m_prefabCache;
    std::vector<std::unique_ptr<SavedPrefab>> m_prefabs;
    std::vector<std::string> m_prefabPaths; // paralelo a m_prefabs

    // Material (Hito 17). [0] = default material (albedo blanco, mate).
    std::unordered_map<std::string, MaterialAssetId> m_materialCache;
    std::vector<std::unique_ptr<MaterialAsset>> m_materials;
    std::vector<std::string> m_materialPaths; // paralelo a m_materials

    // Dialog (F2H48). [0] = asset vacio (sin nodos / sin start_node).
    std::unordered_map<std::string, DialogAssetId> m_dialogCache;
    std::vector<std::unique_ptr<Dialog::Asset>> m_dialogs;
    std::vector<std::string> m_dialogPaths; // paralelo a m_dialogs

    // AnimationClip standalone (F2H49). [0] = clip vacio (sin tracks).
    std::unordered_map<std::string, AnimationClipAssetId> m_animationClipCache;
    std::vector<std::unique_ptr<AnimationClip>> m_animationClips;
    std::vector<std::string> m_animationClipPaths; // paralelo a m_animationClips
};

} // namespace Mood
