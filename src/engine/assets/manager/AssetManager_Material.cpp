// F2H24 Bloque C: AssetManager — operaciones sobre materiales (Hito 17).
// loadMaterial / loadMaterialFromTexture / createMaterialFromTexture /
// createMaterialsForMesh / createMaterial / getMaterial / materialPathOf /
// saveMaterial.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/render/rhi/IMesh.h"  // dtor de SubMesh::mesh (unique_ptr<IMesh>)
#include "engine/render/rhi/ITexture.h"  // dtor de m_textures elements
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace Mood {

namespace {
constexpr const char* k_defaultMaterialPath = "__default_material";
} // namespace

MaterialAssetId AssetManager::loadMaterial(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_materialCache.find(key); it != m_materialCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: material path '{}' rechazado por VFS. Fallback al default.",
            logicalPath);
        m_materialCache.emplace(key, missingMaterialId());
        return missingMaterialId();
    }

    std::ifstream in(fs);
    if (!in.good()) {
        Log::assets()->warn(
            "AssetManager: no se pudo abrir material '{}'. Fallback al default.",
            fs.generic_string());
        m_materialCache.emplace(key, missingMaterialId());
        return missingMaterialId();
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "AssetManager: material '{}' no es JSON valido: {}. Fallback al default.",
            fs.generic_string(), e.what());
        m_materialCache.emplace(key, missingMaterialId());
        return missingMaterialId();
    }

    auto mat = std::make_unique<MaterialAsset>();
    mat->logicalPath = key;

    // Texturas opcionales: si el path esta presente y no vacio, se carga
    // (puede caer al missing si el archivo no existe). Sin path => slot 0
    // (textura "no asignada", el shader usa los multiplicadores escalares).
    auto loadTexField = [&](const char* field) -> TextureAssetId {
        if (!j.contains(field)) return 0;
        const auto& v = j.at(field);
        if (!v.is_string()) return 0;
        const std::string p = v.get<std::string>();
        if (p.empty()) return 0;
        return loadTexture(p);
    };
    mat->albedo            = loadTexField("albedo");
    mat->metallicRoughness = loadTexField("metallic_roughness");
    mat->normal            = loadTexField("normal");
    mat->ao                = loadTexField("ao");
    // Inferimos useAlbedoMap de la presencia del campo en el JSON (no del
    // id resuelto): "albedo": "textures/missing.png" -> id 0 pero el
    // usuario pidio explicitamente textura -> useAlbedoMap=true.
    mat->useAlbedoMap = j.contains("albedo")
                     && j.at("albedo").is_string()
                     && !j.at("albedo").get<std::string>().empty();

    // Multiplicadores escalares con defaults razonables.
    if (j.contains("albedo_tint") && j.at("albedo_tint").is_array()
        && j.at("albedo_tint").size() == 3) {
        mat->albedoTint = glm::vec3(
            j.at("albedo_tint")[0].get<f32>(),
            j.at("albedo_tint")[1].get<f32>(),
            j.at("albedo_tint")[2].get<f32>());
    }
    if (j.contains("metallic"))  mat->metallicMult  = j.at("metallic").get<f32>();
    if (j.contains("roughness")) mat->roughnessMult = j.at("roughness").get<f32>();
    if (j.contains("ao"))        mat->aoMult        = j.at("ao").get<f32>();

    // F2H62 Bloque D: campo opcional "shader_graph" con path logico al
    // .moodshader. Si esta presente y no vacio, el SceneRenderer usara
    // ese shader compilado en vez del PBR estandar.
    if (j.contains("shader_graph") && j.at("shader_graph").is_string()) {
        mat->shaderGraphPath = j.at("shader_graph").get<std::string>();
    }

    // F2H63: transparencia + refraccion. Schema aditivo — campos ausentes
    // preservan los defaults (Opaque + opacity=1 + ior=1 + strength=0),
    // que matchean el comportamiento pre-F2H63.
    if (j.contains("blend_mode") && j.at("blend_mode").is_string()) {
        const std::string mode = j.at("blend_mode").get<std::string>();
        if (mode == "translucent")     mat->blendMode = BlendMode::Translucent;
        else if (mode == "additive")   mat->blendMode = BlendMode::Additive;
        else                           mat->blendMode = BlendMode::Opaque;
    }
    if (j.contains("opacity"))              mat->opacity            = j.at("opacity").get<f32>();
    if (j.contains("ior"))                  mat->ior                = j.at("ior").get<f32>();
    if (j.contains("refraction_strength"))  mat->refractionStrength = j.at("refraction_strength").get<f32>();
    if (j.contains("cast_translucent_shadow")) mat->castTranslucentShadow = j.at("cast_translucent_shadow").get<bool>();

    const MaterialAssetId id = static_cast<MaterialAssetId>(m_materials.size());
    m_materials.push_back(std::move(mat));
    m_materialPaths.push_back(key);
    m_materialCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado material {} -> id {}", logicalPath, id);
    return id;
}

MaterialAssetId AssetManager::loadMaterialFromTexture(TextureAssetId textureId) {
    // Cache key sintetica: `__tex#<id>`. Como los paths con `#` no son
    // validos en el VFS (solo permitimos paths logicos relativos), no
    // colisionan con materiales reales que cargan por path.
    const std::string key = "__tex#" + std::to_string(textureId);
    if (auto it = m_materialCache.find(key); it != m_materialCache.end()) {
        return it->second;
    }

    auto mat = std::make_unique<MaterialAsset>();
    mat->logicalPath = key;
    mat->albedo = textureId;
    mat->useAlbedoMap = true;
    // Mate completo: aproxima el shading Blinn-Phong del Hito 11 con un
    // PBR dieléctrico de roughness alta (los archivos v6 fueron creados
    // con texturas mate, asi se preserva la apariencia).
    mat->albedoTint    = glm::vec3(1.0f);
    mat->metallicMult  = 0.0f;
    mat->roughnessMult = 1.0f;
    mat->aoMult        = 1.0f;

    const MaterialAssetId id = static_cast<MaterialAssetId>(m_materials.size());
    m_materials.push_back(std::move(mat));
    m_materialPaths.push_back(key);
    m_materialCache.emplace(key, id);
    return id;
}

MaterialAssetId AssetManager::createMaterialFromTexture(TextureAssetId textureId) {
    // Mismos defaults que loadMaterialFromTexture (mate completo, tint
    // blanco) pero SIN cache: dos llamadas con el mismo texId devuelven
    // ids distintos. Asi cada entidad spawneada/dropeada tiene material
    // propio editable desde el Inspector sin contagiar a otras.
    const std::string sentinel = "__runtime_tex#" + std::to_string(textureId);
    auto mat = std::make_unique<MaterialAsset>();
    mat->logicalPath   = sentinel;
    mat->albedo        = textureId;
    mat->useAlbedoMap  = true;
    mat->albedoTint    = glm::vec3(1.0f);
    mat->metallicMult  = 0.0f;
    mat->roughnessMult = 1.0f;
    mat->aoMult        = 1.0f;

    const MaterialAssetId id = static_cast<MaterialAssetId>(m_materials.size());
    m_materials.push_back(std::move(mat));
    // El sentinel va a m_materialPaths (NO empty) para que materialPathOf
    // lo devuelva al serializer, que lo reconoce y persiste el path de
    // la textura subyacente. NO se cachea (no entra a m_materialCache).
    m_materialPaths.push_back(sentinel);
    return id;
}

std::vector<MaterialAssetId> AssetManager::createMaterialsForMesh(MeshAssetId meshId) {
    std::vector<MaterialAssetId> out;
    MeshAsset* mesh = getMesh(meshId);
    if (mesh == nullptr) return out;

    out.reserve(mesh->submeshes.size());
    for (const auto& sm : mesh->submeshes) {
        TextureAssetId tex = 0;
        if (sm.materialIndex < mesh->materialAlbedoTextures.size()) {
            tex = mesh->materialAlbedoTextures[sm.materialIndex];
        }
        if (tex != 0) {
            // Material instance unico envolviendo la textura extraida del archivo.
            out.push_back(createMaterialFromTexture(tex));
            continue;
        }
        // F2H49.1: sin texture map. Si el material trae un diffuse color plano
        // (caso template Mixamo X/Y Bot: blanco/gris/negro via aiColor4D), lo
        // usamos como `albedoTint` puro con `useAlbedoMap=false`. Asi el
        // personaje se ve con sus colores reales en lugar del missing magenta.
        std::optional<glm::vec3> color;
        if (sm.materialIndex < mesh->materialAlbedoColors.size()) {
            color = mesh->materialAlbedoColors[sm.materialIndex];
        }
        if (color.has_value()) {
            MaterialAsset proto{};
            proto.albedo        = 0;
            proto.useAlbedoMap  = false;
            proto.albedoTint    = *color;
            proto.metallicMult  = 0.0f;
            proto.roughnessMult = 0.7f;
            proto.aoMult        = 1.0f;
            out.push_back(createMaterial(proto));
            continue;
        }
        // Sin textura ni color -> clone del default; mostrara missing.png como
        // warning visible (Hito 25).
        MaterialAsset proto{};
        if (const MaterialAsset* def = getMaterial(missingMaterialId());
            def != nullptr) {
            proto = *def;
        }
        out.push_back(createMaterial(proto));
    }
    return out;
}

MaterialAssetId AssetManager::createMaterial(const MaterialAsset& prototype) {
    auto mat = std::make_unique<MaterialAsset>(prototype);
    const MaterialAssetId id = static_cast<MaterialAssetId>(m_materials.size());
    const std::string key = "__runtime#" + std::to_string(id);
    mat->logicalPath = key;
    m_materials.push_back(std::move(mat));
    m_materialPaths.push_back(key);
    m_materialCache.emplace(key, id);
    return id;
}

MaterialAsset* AssetManager::getMaterial(MaterialAssetId id) const {
    if (id >= m_materials.size()) {
        return m_materials[missingMaterialId()].get();
    }
    return m_materials[id].get();
}

std::string AssetManager::materialPathOf(MaterialAssetId id) const {
    if (id >= m_materialPaths.size()) {
        return m_materialPaths[missingMaterialId()];
    }
    return m_materialPaths[id];
}

bool AssetManager::saveMaterial(MaterialAssetId id) {
    if (id == 0 || id >= m_materials.size()) {
        Log::assets()->warn("saveMaterial: id {} fuera de rango", id);
        return false;
    }
    const std::string& logicalPath = m_materialPaths[id];
    if (logicalPath.empty() ||
        logicalPath == k_defaultMaterialPath ||
        logicalPath.rfind("__tex#", 0) == 0 ||
        logicalPath.rfind("__runtime#", 0) == 0) {
        Log::assets()->warn(
            "saveMaterial: path logico '{}' es sentinel interno, no persistible",
            logicalPath);
        return false;
    }
    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "saveMaterial: VFS no resuelve '{}' (proyecto cerrado?)",
            logicalPath);
        return false;
    }

    const MaterialAsset* mat = m_materials[id].get();
    if (mat == nullptr) {
        Log::assets()->warn("saveMaterial: material id {} sin instancia", id);
        return false;
    }

    nlohmann::json j;
    // Texturas: solo escribir el campo si el slot esta asignado (id != 0).
    // Esto preserva el contrato del loader (campo ausente = sin textura).
    if (mat->albedo != 0 && mat->useAlbedoMap) {
        j["albedo"] = pathOf(mat->albedo);
    }
    if (mat->metallicRoughness != 0) {
        j["metallic_roughness"] = pathOf(mat->metallicRoughness);
    }
    if (mat->normal != 0) {
        j["normal"] = pathOf(mat->normal);
    }
    if (mat->ao != 0) {
        j["ao"] = pathOf(mat->ao);
    }

    j["albedo_tint"] = nlohmann::json::array({
        mat->albedoTint.x, mat->albedoTint.y, mat->albedoTint.z});
    j["metallic"]  = mat->metallicMult;
    j["roughness"] = mat->roughnessMult;
    j["ao"]        = mat->aoMult;

    // F2H62 Bloque D: solo se persiste si esta seteado.
    if (!mat->shaderGraphPath.empty()) {
        j["shader_graph"] = mat->shaderGraphPath;
    }

    // F2H63: solo persistir blending fields si difieren del default Opaque.
    // Mismo patron que `friction` (Hito 34 A) — materiales viejos no se
    // ensucian con campos nuevos.
    if (mat->blendMode != BlendMode::Opaque) {
        j["blend_mode"] = (mat->blendMode == BlendMode::Translucent) ? "translucent" : "additive";
        if (mat->opacity != 1.0f)              j["opacity"]             = mat->opacity;
        if (mat->ior != 1.0f)                  j["ior"]                 = mat->ior;
        if (mat->refractionStrength != 0.0f)   j["refraction_strength"] = mat->refractionStrength;
        // F2H64: persistir cast_translucent_shadow solo si difiere del default (true)
        // y el material es Translucent (no aplica a Additive).
        if (mat->blendMode == BlendMode::Translucent && !mat->castTranslucentShadow) {
            j["cast_translucent_shadow"] = false;
        }
    }

    std::ofstream out(fs);
    if (!out.is_open()) {
        Log::assets()->warn(
            "saveMaterial: no se pudo abrir '{}' para escritura",
            fs.generic_string());
        return false;
    }
    out << j.dump(2) << '\n';
    out.flush();

    Log::assets()->info("saveMaterial: '{}' escrito ({} bytes)",
                         logicalPath, j.dump().size());
    return true;
}

} // namespace Mood
