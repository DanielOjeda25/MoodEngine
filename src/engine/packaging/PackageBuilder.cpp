#include "engine/packaging/PackageBuilder.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <system_error>

namespace Mood::PackageBuilder {

namespace {

// Hito 37 A: paths que SIEMPRE se incluyen en el smart-pack. Cubren los
// fallbacks del runtime (missing.png/missing.wav) + assets que el motor
// resuelve sin pasar por `.moodmap` (skyboxes default del SkyboxRenderer).
const char* k_engineWhitelist[] = {
    "textures/missing.png",
    "audio/missing.wav",
};

// Hito 37 A: trims un prefijo `assets/` si existe. ScriptComponent.path se
// guarda con ese prefijo (ver DemoSpawners.cpp); el resto de paths del
// `.moodmap` son ya logicos. Normalizamos todo a path logico relativo a
// `assets/`.
std::string stripAssetsPrefix(std::string_view p) {
    constexpr std::string_view kPrefix = "assets/";
    if (p.size() >= kPrefix.size() &&
        p.compare(0, kPrefix.size(), kPrefix) == 0) {
        return std::string(p.substr(kPrefix.size()));
    }
    return std::string(p);
}

// Recolecta paths de un objeto JSON entity (schema del EntitySerializer).
void collectEntityRefs(const nlohmann::json& jent,
                        std::unordered_set<std::string>& out) {
    if (jent.contains("mesh_renderer")) {
        const auto& jmr = jent.at("mesh_renderer");
        if (jmr.contains("mesh_path") && jmr.at("mesh_path").is_string()) {
            const auto p = jmr.at("mesh_path").get<std::string>();
            if (!p.empty()) out.insert(stripAssetsPrefix(p));
        }
        if (jmr.contains("materials") && jmr.at("materials").is_array()) {
            for (const auto& m : jmr.at("materials")) {
                if (m.is_string()) {
                    const auto p = m.get<std::string>();
                    if (!p.empty()) out.insert(stripAssetsPrefix(p));
                }
            }
        }
    }
    if (jent.contains("environment")) {
        const auto& jenv = jent.at("environment");
        if (jenv.contains("skybox_path") && jenv.at("skybox_path").is_string()) {
            const auto p = jenv.at("skybox_path").get<std::string>();
            if (!p.empty()) out.insert(stripAssetsPrefix(p));
        }
    }
    if (jent.contains("particle_emitter")) {
        const auto& jpe = jent.at("particle_emitter");
        if (jpe.contains("texture_path") && jpe.at("texture_path").is_string()) {
            const auto p = jpe.at("texture_path").get<std::string>();
            if (!p.empty()) out.insert(stripAssetsPrefix(p));
        }
    }
    if (jent.contains("script")) {
        const auto& js = jent.at("script");
        if (js.contains("path") && js.at("path").is_string()) {
            const auto p = js.at("path").get<std::string>();
            if (!p.empty()) out.insert(stripAssetsPrefix(p));
        }
    }
    if (jent.contains("prefab_path") && jent.at("prefab_path").is_string()) {
        const auto p = jent.at("prefab_path").get<std::string>();
        if (!p.empty()) out.insert(stripAssetsPrefix(p));
    }
}

// Lee un `.material` y agrega sus texturas albedo/MR/normal/AO al set.
// `materialLogicalPath` es el path del `.material` (ya en out, no lo
// reagregamos). Los paths internos del .material son logicos absolutos
// (ej. "textures/brick.png"), no relativos a la carpeta del .material.
void collectMaterialRefs(const std::filesystem::path& engineAssetsDir,
                          const std::string& materialLogicalPath,
                          std::unordered_set<std::string>& out) {
    namespace fs = std::filesystem;
    const fs::path matFs = engineAssetsDir / materialLogicalPath;
    if (!fs::exists(matFs)) return;
    std::ifstream f(matFs);
    if (!f.is_open()) return;
    nlohmann::json j;
    try { j = nlohmann::json::parse(f); } catch (...) { return; }
    static const char* kTexFields[] = {
        "albedo", "metallic_roughness", "normal", "ao"
    };
    for (const char* field : kTexFields) {
        if (j.contains(field) && j.at(field).is_string()) {
            const auto p = j.at(field).get<std::string>();
            if (!p.empty()) out.insert(stripAssetsPrefix(p));
        }
    }
}

// std::filesystem::copy con recursive + overwrite_existing, contando
// archivos copiados. Throws std::filesystem::filesystem_error en error.
int copyTreeCounted(const std::filesystem::path& src,
                    const std::filesystem::path& dst) {
    namespace fs = std::filesystem;
    fs::create_directories(dst);
    int count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(src)) {
        const auto rel = fs::relative(entry.path(), src);
        const auto target = dst / rel;
        if (entry.is_directory()) {
            fs::create_directories(target);
        } else if (entry.is_regular_file()) {
            fs::copy_file(entry.path(), target,
                          fs::copy_options::overwrite_existing);
            ++count;
        }
    }
    return count;
}

bool copyFileChecked(const std::filesystem::path& src,
                     const std::filesystem::path& dst,
                     std::string& outError) {
    namespace fs = std::filesystem;
    if (!fs::exists(src)) {
        outError = "Archivo fuente no existe: " + src.generic_string();
        return false;
    }
    fs::create_directories(dst.parent_path());
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        outError = "Fallo copy " + src.generic_string() + " -> " +
                   dst.generic_string() + ": " + ec.message();
        return false;
    }
    return true;
}

} // namespace

std::unordered_set<std::string> collectReferencedAssetPaths(
        const Project& project,
        const std::filesystem::path& engineAssetsDir) {
    namespace fs = std::filesystem;
    std::unordered_set<std::string> out;

    // 1) Walk de cada `.moodmap` del proyecto.
    for (const auto& mapRel : project.maps) {
        const fs::path mapAbs = project.root / mapRel;
        if (!fs::exists(mapAbs)) {
            Log::engine()->warn(
                "PackageBuilder: mapa '{}' del proyecto no existe en disco — skip",
                mapAbs.generic_string());
            continue;
        }
        std::ifstream f(mapAbs);
        if (!f.is_open()) continue;
        nlohmann::json j;
        try { j = nlohmann::json::parse(f); }
        catch (const std::exception& e) {
            Log::engine()->warn(
                "PackageBuilder: '{}' no parsea como JSON ({}); skip",
                mapAbs.generic_string(), e.what());
            continue;
        }
        if (j.contains("entities") && j.at("entities").is_array()) {
            for (const auto& jent : j.at("entities")) {
                collectEntityRefs(jent, out);
            }
        }
    }

    // 2) Para cada `.material` recolectado, expandir a sus texturas.
    //    Solo si el caller paso engineAssetsDir (sin el dir no podemos
    //    abrir los archivos). Snapshot del set previo porque vamos a
    //    insertar mientras iteramos.
    if (!engineAssetsDir.empty()) {
        std::vector<std::string> materials;
        for (const auto& p : out) {
            if (p.size() >= 9 && p.compare(p.size() - 9, 9, ".material") == 0) {
                materials.push_back(p);
            }
        }
        for (const auto& matPath : materials) {
            collectMaterialRefs(engineAssetsDir, matPath, out);
        }
    }

    return out;
}

BuildResult build(const BuildConfig& cfg) {
    namespace fs = std::filesystem;
    BuildResult result{};

    // Validacion preliminar.
    if (cfg.project.name.empty()) {
        result.error = "El proyecto no tiene 'name'";
        return result;
    }
    if (cfg.destDir.empty()) {
        result.error = "destDir vacio";
        return result;
    }
    if (!fs::exists(cfg.engineExeDir)) {
        result.error = "engineExeDir no existe: " +
                        cfg.engineExeDir.generic_string();
        return result;
    }

    const fs::path outDir = cfg.destDir / cfg.project.name;

    // Guard contra recursion: si outDir cae adentro de project.root (o
    // viceversa), copiar el proyecto a un subdir de si mismo entra en
    // recursion infinita. Comparamos paths normalizados.
    std::error_code ecCanon;
    const fs::path projAbs = fs::weakly_canonical(cfg.project.root, ecCanon);
    const fs::path outAbs  = fs::weakly_canonical(outDir, ecCanon);
    auto isSubpath = [](const fs::path& parent, const fs::path& child) {
        const auto p = parent.generic_string();
        const auto c = child.generic_string();
        return c.size() >= p.size() &&
               c.compare(0, p.size(), p) == 0 &&
               (c.size() == p.size() || c[p.size()] == '/');
    };
    if (isSubpath(projAbs, outAbs) || isSubpath(outAbs, projAbs) ||
        projAbs == outAbs) {
        result.error =
            "El destino del paquete no puede estar dentro de la carpeta "
            "del proyecto (ni viceversa). Elegi una carpeta distinta — "
            "p.ej. el escritorio o una carpeta nueva.";
        return result;
    }
    result.outputDir = outDir;

    Log::engine()->info("PackageBuilder: empaquetando '{}' -> '{}'",
                         cfg.project.name, outDir.generic_string());

    try {
        fs::create_directories(outDir);

        // 1) MoodPlayer.exe (junto al editor en build/<config>/).
        const fs::path playerExe = cfg.engineExeDir / "MoodPlayer.exe";
        if (!copyFileChecked(playerExe, outDir / "MoodPlayer.exe", result.error)) {
            return result;
        }
        ++result.filesCopied;

        // 2) SDL2 DLL — variante segun el build del editor.
        const char* sdlDllName = cfg.isDebugBuild ? "SDL2d.dll" : "SDL2.dll";
        if (!copyFileChecked(cfg.engineExeDir / sdlDllName,
                             outDir / sdlDllName, result.error)) {
            return result;
        }
        ++result.filesCopied;

        // 3) Engine assets + shaders. Hito 37 A: smart-pack opcional.
        //    Si smartPack=true, scanea los `.moodmap` del proyecto +
        //    `.material` referenciados, y copia solo esos paths +
        //    whitelist defensiva (missing.*, skyboxes/* recursivo).
        //    Si false, fallback al modo V1 (copy assets/ entero).
        const fs::path assetsSrc  = cfg.engineExeDir / "assets";
        const fs::path shadersSrc = cfg.engineExeDir / "shaders";
        if (fs::exists(assetsSrc)) {
            if (cfg.smartPack) {
                auto refs = collectReferencedAssetPaths(cfg.project, assetsSrc);
                // Whitelist defensiva: fallbacks runtime + skyboxes default.
                for (const char* w : k_engineWhitelist) refs.insert(w);
                // Skyboxes completo (cubemaps + equirect, ~5 archivos chicos).
                const fs::path skyDir = assetsSrc / "skyboxes";
                if (fs::exists(skyDir)) {
                    for (const auto& entry :
                            fs::recursive_directory_iterator(skyDir)) {
                        if (!entry.is_regular_file()) continue;
                        const auto rel = fs::relative(entry.path(), assetsSrc);
                        refs.insert(rel.generic_string());
                    }
                }
                int copied = 0;
                int missing = 0;
                for (const auto& logical : refs) {
                    const fs::path src = assetsSrc / logical;
                    if (!fs::exists(src) || !fs::is_regular_file(src)) {
                        ++missing;
                        continue;
                    }
                    const fs::path dst = outDir / "assets" / logical;
                    fs::create_directories(dst.parent_path());
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                    ++copied;
                }
                Log::engine()->info(
                    "PackageBuilder: smart-pack — {} archivos referenciados "
                    "copiados ({} no existen en disco, omitidos)",
                    copied, missing);
                result.filesCopied += copied;
            } else {
                result.filesCopied += copyTreeCounted(assetsSrc, outDir / "assets");
            }
        } else {
            Log::engine()->warn(
                "PackageBuilder: '{}' no existe — el paquete saldra sin "
                "assets default (puede romper IBL, skybox, missing textures)",
                assetsSrc.generic_string());
        }
        if (fs::exists(shadersSrc)) {
            result.filesCopied += copyTreeCounted(shadersSrc, outDir / "shaders");
        } else {
            result.error = "shaders/ no existe junto al editor: " +
                           shadersSrc.generic_string();
            return result;
        }

        // 4) Proyecto del usuario: copia recursiva del root entero a
        //    `<outDir>/project/`. Incluye `.moodproj`, `maps/`, y
        //    cualquier otra cosa que el dev haya guardado en su carpeta.
        if (!fs::exists(cfg.project.root)) {
            result.error = "Project root no existe: " +
                           cfg.project.root.generic_string();
            return result;
        }
        result.filesCopied += copyTreeCounted(cfg.project.root, outDir / "project");

        // 5) Generar game.json. Path al .moodproj relativo a outDir
        //    (el manifest vive en outDir; el .moodproj en outDir/project/).
        const fs::path projFileName = cfg.projectFilePath.filename();
        nlohmann::json manifest;
        manifest["version"]     = 1;
        manifest["name"]        = cfg.project.name;
        manifest["project"]     = ("project/" + projFileName.generic_string());
        if (!cfg.project.defaultMap.empty()) {
            manifest["default_map"] =
                cfg.project.defaultMap.generic_string();
        }
        std::ofstream f(outDir / "game.json");
        if (!f.is_open()) {
            result.error = "No se pudo crear game.json en " +
                           outDir.generic_string();
            return result;
        }
        f << manifest.dump(2) << "\n";
        ++result.filesCopied;

        Log::engine()->info(
            "PackageBuilder: OK — {} archivos copiados en '{}'",
            result.filesCopied, outDir.generic_string());

        result.ok = true;
        return result;

    } catch (const std::exception& e) {
        result.error = std::string("Excepcion en build: ") + e.what();
        Log::engine()->error("PackageBuilder: {}", result.error);
        return result;
    }
}

} // namespace Mood::PackageBuilder
