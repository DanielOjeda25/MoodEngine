#pragma once

// Manager de mapas dentro de un proyecto (F2H8). Modela el estado
// "maps[] + default + current" que el `.moodproj` ya soporta desde
// Hito 6. Mantiene los invariantes:
//   - Siempre hay >= 1 mapa.
//   - El default está dentro de la lista.
//   - El current está dentro de la lista (cuando hay current set).
//   - No hay duplicados (paths normalizados como `generic_string`).
//
// PURO: no toca disco ni ImGui ni ProjectSerializer. La capa de UI
// (`EditorApplication::handle*Map*`) es quien hace I/O de archivos
// y sincroniza con `m_project.maps` / `m_project.defaultMap`.

#include "core/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Mood {

class MapsManager {
public:
    /// @brief Inicializa el manager con la lista de mapas del Project +
    ///        el default + el actualmente abierto. Si `maps` está vacía,
    ///        agrega `defaultMap` (sino quedaríamos con 0 mapas, que rompe
    ///        el invariante). Si `currentMap` no está en la lista, se
    ///        ignora y current queda igual al default.
    void setMaps(std::vector<std::filesystem::path> maps,
                  std::filesystem::path defaultMap,
                  std::filesystem::path currentMap);

    /// @brief Agrega un mapa a la lista. Devuelve false si ya existe
    ///        (dedup por `generic_string` para que `maps/foo.moodmap`
    ///        y `maps\foo.moodmap` cuenten como el mismo).
    bool addMap(const std::filesystem::path& path);

    /// @brief Elimina un mapa de la lista. Devuelve false si era el
    ///        último (no podemos quedarnos con 0 mapas) o si no estaba
    ///        en la lista. Si era el `default`, reasigna default al
    ///        primero restante. Si era el `current`, NO toca current
    ///        (el caller decide a qué cambiar y llama setCurrentMap).
    bool removeMap(const std::filesystem::path& path);

    /// @brief Cambia el current. No-op si el path no está en la lista.
    void setCurrentMap(const std::filesystem::path& path);

    /// @brief Cambia el default. No-op si el path no está en la lista
    ///        (el default debe estar en `maps`).
    void setDefaultMap(const std::filesystem::path& path);

    const std::vector<std::filesystem::path>& maps() const { return m_maps; }
    const std::filesystem::path& currentMap() const { return m_current; }
    const std::filesystem::path& defaultMap() const { return m_default; }

    bool contains(const std::filesystem::path& path) const;
    bool isDefault(const std::filesystem::path& path) const;
    bool isCurrent(const std::filesystem::path& path) const;
    usize count() const { return m_maps.size(); }

private:
    std::vector<std::filesystem::path> m_maps;
    std::filesystem::path m_default;
    std::filesystem::path m_current;
};

} // namespace Mood
