#include "engine/shader_graph/ShaderGraphCache.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/shader_graph/ShaderGraphAsset.h"
#include "engine/shader_graph/ShaderGraphGenerator.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace Mood {

namespace {

// Hash del contenido GLSL. std::hash<std::string> es suficiente para
// detectar cambios entre ediciones (no necesitamos crypto-grade).
u64 hashGlsl(const std::string& glsl) {
    return static_cast<u64>(std::hash<std::string>{}(glsl));
}

} // namespace

ShaderGraphCache::ShaderGraphCache(std::filesystem::path vertexFsPath)
    : m_vertexFsPath(std::move(vertexFsPath)) {
    Log::render()->info("[ShaderGraphCache] vertex base: '{}'",
                          m_vertexFsPath.generic_string());
}

ShaderGraphCache::~ShaderGraphCache() = default;

IShader* ShaderGraphCache::getOrCompile(std::string_view moodshaderLogicalPath,
                                            AssetManager& assets) {
    if (moodshaderLogicalPath.empty()) return nullptr;
    const std::string key(moodshaderLogicalPath);

    // 1) Resolver path logico a fs.
    const auto fsPath = assets.resolvePath(moodshaderLogicalPath);
    if (fsPath.empty()) {
        // resolvePath ya loggea -- no spamear.
        return nullptr;
    }

    // 2) Cargar el asset .moodshader desde disco.
    auto loadedOpt = ShaderGraph::Asset::loadFromFile(fsPath);
    if (!loadedOpt.has_value()) {
        Log::render()->warn("[ShaderGraphCache] no se pudo cargar '{}'",
                              fsPath.generic_string());
        return nullptr;
    }
    const ShaderGraph::Asset& asset = *loadedOpt;

    // 3) Cargar plantilla GLSL (cached en disco; baratisimo de releer
    // cada vez, pero igual lo hacemos solo en miss). Si no existe la
    // template, abortamos -- algo grave esta mal con la instalacion.
    const std::string tpl = ShaderGraph::loadDefaultTemplate();
    if (tpl.empty()) {
        Log::render()->warn("[ShaderGraphCache] plantilla GLSL no disponible");
        return nullptr;
    }

    // 4) Generar GLSL del fragment shader.
    const auto gen = ShaderGraph::generateGlsl(asset, tpl);
    if (!gen.succeeded) {
        const auto errs = gen.messagesOf(ShaderGraph::GenSeverity::Error);
        Log::render()->warn(
            "[ShaderGraphCache] '{}' genero {} errores, fallback al PBR",
            key, static_cast<u32>(errs.size()));
        for (const auto* m : errs) {
            Log::render()->warn("  - {}", m->text);
        }
        return nullptr;
    }
    const u64 newHash = hashGlsl(gen.glsl);

    // 5) Cache lookup. Si hit con hash matching, devolver shader vivo.
    auto it = m_byPath.find(key);
    if (it != m_byPath.end() && it->second.glslHash == newHash && it->second.shader) {
        return it->second.shader.get();
    }

    // 6) Miss o hash mismatch: compilar nuevo programa GL.
    std::unique_ptr<IShader> shader;
    try {
        shader = std::make_unique<OpenGLShader>(
            OpenGLShader::k_fromSource,
            m_vertexFsPath.generic_string(),
            gen.glsl,
            std::string("shader_graph:") + key);
    } catch (const std::exception& e) {
        Log::render()->warn(
            "[ShaderGraphCache] compilacion fallo en '{}': {}\nfallback al PBR",
            key, e.what());
        // Removemos la entry vieja si habia (para que la proxima
        // pasada vuelva a intentar -- el dev podria editar y arreglar
        // el grafo).
        if (it != m_byPath.end()) {
            m_byPath.erase(it);
        }
        return nullptr;
    }

    Entry e;
    e.shader = std::move(shader);
    e.glslHash = newHash;
    IShader* raw = e.shader.get();
    m_byPath[key] = std::move(e);
    Log::render()->info(
        "[ShaderGraphCache] compilado '{}' ({} chars, hash {:#x}), cache size = {}",
        key, static_cast<u32>(gen.glsl.size()), newHash, m_byPath.size());
    return raw;
}

void ShaderGraphCache::clear() {
    const usize n = m_byPath.size();
    m_byPath.clear();
    if (n > 0) {
        Log::render()->info("[ShaderGraphCache] cleared ({} entries)", n);
    }
}

} // namespace Mood
