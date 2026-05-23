#pragma once

// break-B5: storage + cache uniforme para una familia de assets.
// Reemplaza el triplete (unordered_map<string,Id> cache +
// vector<unique_ptr<T>> items + vector<string> paths) que existia
// repetido por cada familia en AssetManager. Cada familia se migra
// independiente (un commit por familia).
//
// Convencion de slot 0 ("missing" / fallback):
//   - Se inicializa via `initFallback(...)` antes de cualquier add()/
//     get()/pathOf().
//   - `get(id)` jamas devuelve null si el slot 0 esta inicializado: ids
//     fuera de rango caen al slot 0 (mismo patron que la implementacion
//     manual pre-break).
//   - `pathOf(0)` devuelve el `sentinelPath` que paso initFallback (ej.
//     "__missing_cube", "__empty_dialog", "__fallback_generic_sedan").
//
// Convencion de paths:
//   - `add(path, asset)` guarda el path paralelo al asset y lo registra
//     en el cache map. El path es lo que escribio el dev en moodproj /
//     moodmap / serial (ej. "items/iron_sword.mooditem"). El registry
//     NO hace VFS resolution — eso le toca al loader que llama add().
//   - `findByPath(path)` devuelve 0 si el path no fue agregado (semantic
//     pre-break: el caller de loadX usa eso para decidir si ya esta
//     cacheado).

#include "core/Types.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Mood {

template <typename T>
class AssetRegistry {
public:
    using Id = u32;

    /// @brief Inicializa el slot 0 con el fallback de la familia. Llamar
    ///        UNA vez desde el ctor de AssetManager. Idempotente: si ya
    ///        fue inicializado, no hace nada.
    void initFallback(std::unique_ptr<T> fallback, std::string sentinelPath) {
        if (!m_items.empty()) return;
        m_items.push_back(std::move(fallback));
        m_paths.push_back(sentinelPath);
        m_cache.emplace(std::move(sentinelPath), Id{0});
    }

    /// @brief Inserta un asset nuevo. Devuelve su id. Tambien registra el
    ///        path en el cache map para que `findByPath` lo devuelva.
    Id add(std::string logicalPath, std::unique_ptr<T> asset) {
        const Id id = static_cast<Id>(m_items.size());
        m_items.push_back(std::move(asset));
        m_paths.push_back(logicalPath);
        m_cache.emplace(std::move(logicalPath), id);
        return id;
    }

    /// @brief Inserta sin cachear el path. Usado por la familia Material
    ///        para `createMaterialFromTexture`: dos llamadas con la misma
    ///        textura producen ids distintos (cada entity tiene material
    ///        propio editable). El path va a `m_paths` para que
    ///        `pathOf(id)` lo devuelva al serializer, pero `findByPath`
    ///        no lo encontrara.
    Id addUncached(std::string logicalPath, std::unique_ptr<T> asset) {
        const Id id = static_cast<Id>(m_items.size());
        m_items.push_back(std::move(asset));
        m_paths.push_back(std::move(logicalPath));
        return id;
    }

    /// @brief Registra `logicalPath` como sinonimo del slot 0 (fallback).
    ///        Lo usan los loaders cuando un asset falla a cargar pero
    ///        quieren memorizar el intento para no reintentar cada frame.
    void cacheAsFallback(std::string_view logicalPath) {
        m_cache.emplace(std::string{logicalPath}, Id{0});
    }

    /// @brief Devuelve el id cacheado para `logicalPath`, o 0 si no esta.
    ///        Notese que `0` tambien es un id valido (el fallback) — el
    ///        caller distingue "no encontrado" mediante `contains()`.
    Id findByPath(std::string_view logicalPath) const {
        auto it = m_cache.find(std::string{logicalPath});
        return (it != m_cache.end()) ? it->second : Id{0};
    }

    /// @brief True si `logicalPath` esta en el cache (incluso si apunta
    ///        al slot 0 via `cacheAsFallback`). Para el caller de loadX
    ///        que necesita decidir si ya intento cargar este path.
    bool contains(std::string_view logicalPath) const {
        return m_cache.find(std::string{logicalPath}) != m_cache.end();
    }

    /// @brief Devuelve el asset del id. Jamas null si el slot 0 esta
    ///        inicializado (ids fuera de rango caen al slot 0).
    const T* get(Id id) const {
        if (m_items.empty()) return nullptr;
        if (id >= m_items.size()) return m_items[0].get();
        return m_items[id].get();
    }

    /// @brief Variante mutable (live-tuning en Inspector). Mismo fallback.
    T* get(Id id) {
        if (m_items.empty()) return nullptr;
        if (id >= m_items.size()) return m_items[0].get();
        return m_items[id].get();
    }

    /// @brief Reemplaza el asset en `id` por uno nuevo. El path y el cache
    ///        quedan intactos (caso de uso: hot-reload de textura — la
    ///        textura cambio en disco pero el path logico es el mismo).
    ///        No-op si `id` esta fuera de rango.
    void replace(Id id, std::unique_ptr<T> newAsset) {
        if (id >= m_items.size()) return;
        m_items[id] = std::move(newAsset);
    }

    /// @brief Path con el que se agrego. Slot 0 devuelve el sentinela.
    ///        Ids fuera de rango tambien caen al slot 0.
    std::string pathOf(Id id) const {
        if (m_paths.empty()) return {};
        if (id >= m_paths.size()) return m_paths[0];
        return m_paths[id];
    }

    /// @brief Cantidad total incluyendo el slot 0.
    std::size_t count() const { return m_items.size(); }

    /// @brief Acceso al vector (iteracion, ej. packager).
    const std::vector<std::unique_ptr<T>>& all() const { return m_items; }
    const std::vector<std::string>& paths() const { return m_paths; }

private:
    std::unordered_map<std::string, Id> m_cache;
    std::vector<std::unique_ptr<T>> m_items;
    std::vector<std::string> m_paths;
};

} // namespace Mood
