#pragma once

// Panel que lista las texturas de assets/textures/ como grid de miniaturas
// (Hito 5 Bloque 4). Click sobre una miniatura selecciona el path logico;
// el callsite puede leer `selected()` y, p.ej., asignarlo a un tile del
// mapa. Drag & drop entra en Hito 6.

#include "editor/panels/IPanel.h"
#include "engine/assets/AssetManager.h"

#include <optional>
#include <string>
#include <vector>

namespace Mood {

class AssetBrowserPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Asset Browser"; }

    /// @brief El panel queda inerte (muestra "Asset manager no inyectado")
    ///        hasta que `EditorApplication` le pasa el manager real.
    void setAssetManager(AssetManager* am) { m_assetManager = am; }

    /// @brief Path logico (relativo a la raiz de assets) del item seleccionado
    ///        por click en este panel. `nullopt` si nada esta seleccionado.
    const std::optional<std::string>& selected() const { return m_selected; }

    /// @brief `true` una unica vez despues de que el usuario apreto Recargar.
    ///        El consumidor (EditorApplication) debe llamar
    ///        `AssetManager::reloadChanged()` entre frames para upload seguro.
    bool consumeReloadRequest() {
        const bool r = m_reloadRequested;
        m_reloadRequested = false;
        return r;
    }

private:
    struct Entry {
        std::string logicalPath; // "textures/foo.png"
        std::string displayName; // "foo.png"
        TextureAssetId id = 0;   // cargado en scan
    };

    void rescan();

    AssetManager* m_assetManager = nullptr;
    std::vector<Entry> m_entries;
    std::optional<std::string> m_selected;
    bool m_scanned = false;
    bool m_reloadRequested = false;
};

} // namespace Mood
