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
#include "platform/VFS.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Mood {

class ITexture;

/// @brief Identificador estable de una textura dentro de un `AssetManager`.
///        Valor 0 se reserva para la textura "missing": pedir `getTexture(0)`
///        siempre devuelve algo renderizable.
using TextureAssetId = u32;

class AssetManager {
public:
    /// @brief Construye el manager y carga la textura "missing". Si missing.png
    ///        no existe genera una excepcion (esta rota la instalacion).
    /// @param rootDir Carpeta raiz de assets (default: "assets").
    explicit AssetManager(std::string rootDir = "assets");
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /// @brief Carga una textura por path logico (ej. "textures/grid.png")
    ///        o devuelve el id cacheado si ya estaba cargada. En fallo,
    ///        devuelve `missingTextureId()` y loguea en `assets`.
    TextureAssetId loadTexture(std::string_view logicalPath);

    /// @brief Devuelve el ITexture para el id dado. Nunca es null:
    ///        ids invalidos (fuera de rango) caen al fallback missing.
    ITexture* getTexture(TextureAssetId id) const;

    /// @brief Id de la textura de fallback. Siempre vale 0.
    TextureAssetId missingTextureId() const { return 0; }

    /// @brief Cantidad de texturas en cache (incluye missing).
    usize textureCount() const { return m_textures.size(); }

private:
    VFS m_vfs;
    std::unordered_map<std::string, TextureAssetId> m_textureCache;
    std::vector<std::unique_ptr<ITexture>> m_textures; // [0] = missing
};

} // namespace Mood
