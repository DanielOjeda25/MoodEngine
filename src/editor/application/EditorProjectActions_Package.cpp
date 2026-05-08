// F2H24: handlePackageProject — empaquetar el proyecto activo +
// runtime + assets en una carpeta destino para distribuir.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/packaging/PackageBuilder.h"

#include <SDL.h>
#include <portable-file-dialogs.h>

namespace Mood {

void EditorApplication::handlePackageProject() {
    if (!m_project.has_value()) return;

    // Si hay cambios sin guardar, ofrecer guardar primero — el packager
    // copia el .moodproj de disco, no el estado en memoria.
    if (m_projectDirty) {
        const auto choice = pfd::message(
            "MoodEngine — Empaquetar proyecto",
            "Tenes cambios sin guardar. Empaquetar el proyecto guardado actualmente "
            "no incluira esos cambios. Guardar primero?",
            pfd::choice::yes_no_cancel,
            pfd::icon::warning).result();
        if (choice == pfd::button::cancel) return;
        if (choice == pfd::button::yes) {
            handleSave();
            if (m_projectDirty) {
                Log::editor()->warn("Empaquetado abortado: el guardado fallo");
                return;
            }
        }
    }

    auto selection = pfd::select_folder(
        "Carpeta destino del paquete",
        m_project->root.parent_path().generic_string()).result();
    if (selection.empty()) return;

    // Hito 41 fix-up: si el destino `<selection>/<projectName>` ya existe,
    // pedir confirmacion al user antes de sobreescribir.
    const std::filesystem::path outDir =
        std::filesystem::path(selection) / m_project->name;
    if (std::filesystem::exists(outDir)) {
        const auto choice = pfd::message(
            "MoodEngine — Carpeta existe",
            "La carpeta:\n" + outDir.generic_string() + "\n\n"
            "ya existe. Sobreescribir su contenido?\n\n"
            "(SI = continuar y reemplazar archivos del paquete; "
            "NO = cancelar empaquetado)",
            pfd::choice::yes_no,
            pfd::icon::warning).result();
        if (choice != pfd::button::yes) {
            Log::editor()->info(
                "Empaquetado cancelado: el destino '{}' ya existe",
                outDir.generic_string());
            return;
        }
    }

    // Localizar el .exe del editor — junto a el viven MoodPlayer.exe,
    // SDL2*.dll, assets/, shaders/.
    char* base = SDL_GetBasePath();
    const std::filesystem::path exeDir = base
        ? std::filesystem::path(base)
        : std::filesystem::current_path();
    if (base) SDL_free(base);

#ifdef NDEBUG
    constexpr bool kIsDebug = false;
#else
    constexpr bool kIsDebug = true;
#endif

    PackageBuilder::BuildConfig cfg{
        *m_project,
        m_project->root / (m_project->name + ".moodproj"),
        std::filesystem::path(selection),
        exeDir,
        kIsDebug
    };
    const auto result = PackageBuilder::build(cfg);

    if (result.ok) {
        const std::string msg =
            "Paquete creado en:\n" + result.outputDir.generic_string() +
            "\n\n" + std::to_string(result.filesCopied) + " archivos copiados.";
        pfd::message("MoodEngine — Empaquetado completo",
                      msg, pfd::choice::ok, pfd::icon::info);
        Log::editor()->info("Empaquetado OK: {} ({} archivos)",
                             result.outputDir.generic_string(), result.filesCopied);
    } else {
        pfd::message("MoodEngine — Empaquetado fallo",
                      "Error: " + result.error,
                      pfd::choice::ok, pfd::icon::error);
        Log::editor()->error("Empaquetado fallo: {}", result.error);
    }
}

} // namespace Mood
