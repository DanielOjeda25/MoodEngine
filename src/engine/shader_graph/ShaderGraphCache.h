#pragma once

// ShaderGraphCache (F2H62 Bloque E): cachea programas GL compilados
// desde `.moodshader` assets. El SceneRenderer consulta el cache por
// path logico ANTES de cada draw call de un material con
// `shaderGraphPath` no vacio; si hay hit, usa el program cacheado en
// lugar del PBR estandar.
//
// Cache key:
//   - First-level lookup: path logico del .moodshader (string).
//   - Detection de cambios: hash del contenido (GLSL generado). Si el
//     dev edita el .moodshader en disco y el GLSL resultante cambia,
//     el hash difiere y forzamos recompile.
//
// Fallback:
//   - Si el `.moodshader` no se puede cargar o parsear -> nullptr +
//     warning (el caller usa el PBR estandar).
//   - Si el generador GLSL devuelve succeeded=false -> nullptr.
//   - Si la compilacion GLSL falla a nivel GL -> nullptr + log con el
//     mensaje de GL.
//
// Lifecycle:
//   - El cache es owned por el SceneRenderer. Se destruye con el.
//   - Los IShader cacheados son destruidos al final de la sesion (GL
//     program handles liberados automaticamente via OpenGLShader dtor).
//
// Vertex shader:
//   - v1: SOLO meshes estaticos (combina con `shaders/pbr.vert`).
//   - Instanced y skinned NO usan shader graph todavia -- caen al PBR
//     estandar. Documentado, hito futuro.

#include "core/Types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Mood {

class IShader;
class AssetManager;

namespace ShaderGraph { class Asset; }

class ShaderGraphCache {
public:
    /// @param vertexPath Path del vertex shader que se combina con el
    ///        fragment generado. Normalmente "shaders/pbr.vert".
    explicit ShaderGraphCache(std::filesystem::path vertexFsPath);
    ~ShaderGraphCache();

    ShaderGraphCache(const ShaderGraphCache&) = delete;
    ShaderGraphCache& operator=(const ShaderGraphCache&) = delete;

    /// @brief Devuelve el IShader compilado para el `.moodshader` dado,
    ///        compilandolo + cacheando si es la primera vez O si el
    ///        contenido cambio desde la ultima compilacion.
    /// @param moodshaderLogicalPath Path logico ej. "shaders/graphs/water.moodshader".
    /// @param assets Para resolver el path logico a fs via AssetManager::resolvePath.
    /// @return Puntero al IShader cacheado (non-owning), o nullptr si
    ///         load/generate/compile fallo. En caso nullptr, el caller
    ///         hace fallback al PBR estandar.
    IShader* getOrCompile(std::string_view moodshaderLogicalPath,
                           AssetManager& assets);

    /// @brief Borra el cache entero. Util si el dev resetea proyecto o
    ///        cambia algo global (ej. plantilla GLSL). Los IShader se
    ///        destruyen (libera GL programs).
    void clear();

    /// @brief Cantidad de programas cacheados (testing / debug HUD).
    usize size() const { return m_byPath.size(); }

private:
    /// Entrada por path: el IShader + hash del GLSL con que fue compilado.
    /// Si el hash cambia (recompile pedido), se reemplaza el shader.
    struct Entry {
        std::unique_ptr<IShader> shader;
        u64 glslHash = 0;
    };

    std::filesystem::path m_vertexFsPath;
    std::unordered_map<std::string, Entry> m_byPath;
};

} // namespace Mood
