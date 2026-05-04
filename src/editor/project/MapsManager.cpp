#include "editor/project/MapsManager.h"

#include <algorithm>

namespace Mood {

namespace {

// Comparacion de paths normalizada (matchea `maps/foo` con `maps\foo`).
bool samePath(const std::filesystem::path& a, const std::filesystem::path& b) {
    return a.generic_string() == b.generic_string();
}

} // namespace

void MapsManager::setMaps(std::vector<std::filesystem::path> maps,
                            std::filesystem::path defaultMap,
                            std::filesystem::path currentMap) {
    m_maps = std::move(maps);
    m_default = std::move(defaultMap);
    m_current = std::move(currentMap);

    // Defensivo: si la lista esta vacia, agregar el default para
    // mantener el invariante (>= 1 mapa).
    if (m_maps.empty() && !m_default.empty()) {
        m_maps.push_back(m_default);
    }

    // Defensivo: si current no esta en la lista, fallback al default.
    if (!m_current.empty() && !contains(m_current)) {
        m_current = m_default;
    }

    // Defensivo: si el default no esta en la lista (caso degenerado),
    // promover el primero como default.
    if (!m_maps.empty() && !contains(m_default)) {
        m_default = m_maps.front();
        if (m_current.empty()) m_current = m_default;
    }
}

bool MapsManager::addMap(const std::filesystem::path& path) {
    if (contains(path)) return false;
    m_maps.push_back(path);
    // Si era el primer mapa de un manager virgen, lo promovemos a
    // default + current automaticamente.
    if (m_maps.size() == 1u) {
        if (m_default.empty()) m_default = path;
        if (m_current.empty()) m_current = path;
    }
    return true;
}

bool MapsManager::removeMap(const std::filesystem::path& path) {
    if (m_maps.size() <= 1u) return false;  // invariante: >= 1 mapa
    auto it = std::find_if(m_maps.begin(), m_maps.end(),
        [&](const std::filesystem::path& p) { return samePath(p, path); });
    if (it == m_maps.end()) return false;
    const bool wasDefault = samePath(m_default, path);
    m_maps.erase(it);
    // Si era el default, reasignar al primer restante.
    if (wasDefault && !m_maps.empty()) {
        m_default = m_maps.front();
    }
    // Current NO se toca aca — el caller decide a que cambiar.
    return true;
}

void MapsManager::setCurrentMap(const std::filesystem::path& path) {
    if (!contains(path)) return;
    m_current = path;
}

void MapsManager::setDefaultMap(const std::filesystem::path& path) {
    if (!contains(path)) return;
    m_default = path;
}

bool MapsManager::contains(const std::filesystem::path& path) const {
    return std::any_of(m_maps.begin(), m_maps.end(),
        [&](const std::filesystem::path& p) { return samePath(p, path); });
}

bool MapsManager::isDefault(const std::filesystem::path& path) const {
    return samePath(m_default, path);
}

bool MapsManager::isCurrent(const std::filesystem::path& path) const {
    return samePath(m_current, path);
}

} // namespace Mood
