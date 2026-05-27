#pragma once

// F3H18: Panel "Problemas" del proyecto. Lista todos los issues detectados
// por `AssetValidator::validateProject` (refs muertas / load failed) con
// quick-actions: click en una entrada selecciona la entity fuente (si
// existe) en Hierarchy + Inspector para que el dev pueda corregir el
// field roto en sitio. Refresh manual (F5) o al cargar proyecto.
//
// Default oculto; se abre desde el menu `View > Asset Issues` o al
// clickear el badge `! N` que la MenuBar pinta cuando hay issues > 0.
//
// Estado:
//   - `m_issues` cache local. Se refresca via `refresh(scene, assets)`
//     llamado por `EditorApplication` al abrir/cargar proyecto + cuando
//     el dev clickea el boton Refresh. NO refresca por frame (escaneo
//     completo de la scene + materials).
//   - `m_pendingSelect` push del go-to: el panel pide al `EditorUI`
//     seleccionar una entity (read/consume single-frame).

#include "editor/panels/IPanel.h"
#include "engine/assets/validation/AssetValidator.h"
#include "engine/scene/core/Entity.h"

#include <vector>

namespace Mood {

class Scene;
class AssetManager;
class EditorUI;

class AssetIssuesPanel : public IPanel {
public:
    AssetIssuesPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Asset Issues"; }
    const char* category() const override { return "Project"; }

    /// @brief Re-escanea la scene + assets cargados y actualiza `m_issues`.
    ///        Llamar al abrir proyecto, despues de rename, o cuando el dev
    ///        presiona Refresh. Costo: O(entities + materials).
    void refresh(Scene& scene, const AssetManager& assets);

    /// @brief Cantidad de issues detectados en la ultima refresh. Lo usa
    ///        la MenuBar para pintar el badge `! N`.
    usize issueCount() const { return m_issues.size(); }

    /// @brief Consume el request pendiente de seleccion de entity. Si hay
    ///        request, devuelve la entity y la limpia internamente.
    ///        Falsy si no hay request en este frame.
    Entity consumePendingSelect();

    /// @brief Push externo del refresh: el caller pide refresh la proxima
    ///        vez que el panel pueda. Util para "marca cuando hay scene
    ///        change" — el panel se refresca solo si lo abren.
    void requestRefresh() { m_refreshRequested = true; }
    bool refreshRequested() const { return m_refreshRequested; }
    void consumeRefreshRequest() { m_refreshRequested = false; }

    /// @brief Lectura de los issues (para que MenuBar / Inspector
    ///        consulten "este path/entity esta roto?"). Sin copia —
    ///        la UI no muta el vector durante render.
    const std::vector<asset_validation::AssetIssue>& issues() const {
        return m_issues;
    }

    /// @brief F3H18: helper de consulta inline para el Inspector. Devuelve
    ///        true si hay un issue donde `entity == e` y `assetPath == path`.
    ///        Los panels del Inspector llaman esto despues de pintar un
    ///        InputText/Drop slot para decidir si dibujar el border rojo.
    ///        O(N) por llamada — N tipicamente < 20, aceptable per-frame.
    bool isFieldBroken(Entity e, std::string_view path) const;

private:
    std::vector<asset_validation::AssetIssue> m_issues;
    Entity m_pendingSelect{};
    bool m_refreshRequested = false;
};

} // namespace Mood
