#include "engine/packaging/PackageBuilder.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

namespace Mood::PackageBuilder {

namespace {

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

        // 3) Engine assets + shaders. Se copian enteros — V1 simple, no
        //    se filtra por refs. El paquete pesa ~5-10 MB de mas pero
        //    no rompe nada y nos saltamos un walker complejo.
        const fs::path assetsSrc  = cfg.engineExeDir / "assets";
        const fs::path shadersSrc = cfg.engineExeDir / "shaders";
        if (fs::exists(assetsSrc)) {
            result.filesCopied += copyTreeCounted(assetsSrc, outDir / "assets");
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
