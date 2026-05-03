#pragma once

// Mini editor de scripts in-place (Hito 28 F). Cuando se selecciona una
// entidad con `ScriptComponent`, abre el archivo .lua referenciado y
// permite editarlo dentro del editor — para tweaks rapidos sin alt-tabbear
// a VS Code. Para edits grandes (escribir un script desde cero) sigue
// siendo mejor un editor real.
//
// Diseño:
// - InputTextMultiline puro (sin syntax highlighting). Trade-off: cero
//   nuevas dependencias vs ImGuiColorTextEdit que es la dep canonica
//   pero pesada para el MVP.
// - Save explicito: boton "Guardar" o Ctrl+S. Escribe el buffer al disco.
//   El hot-reload del ScriptSystem (Hito 22, mtime-based) detecta el
//   cambio y re-ejecuta el chunk top-level.
// - Reload automatico al cambiar de entidad seleccionada. Si el dev tenia
//   cambios sin guardar y cambia de seleccion, los pierde — aceptable
//   para v1, mostrar warning antes podria ser un polish futuro.

#include "editor/panels/IPanel.h"
#include "engine/scene/core/Entity.h"

#include <string>

namespace Mood {

class ScriptEditorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Script Editor"; }

    /// @brief El EditorUI llama esto cada frame antes del draw para que
    ///        el panel sepa que entidad esta seleccionada. Si cambia de
    ///        entidad o de path, el panel hace lazy-reload del archivo.
    void setSelectedEntity(Entity e) { m_selected = e; }

private:
    /// Carga `m_buffer` desde disco y resetea el dirty flag. Llamado al
    /// detectar un cambio de path entre frames.
    void loadFromDisk(const std::string& path);

    /// Escribe `m_buffer` al disco. Devuelve true si OK.
    bool saveToDisk();

    Entity      m_selected;
    std::string m_loadedPath;        // path que esta actualmente en m_buffer
    std::string m_buffer;             // contenido editable
    bool        m_dirty = false;     // hay cambios sin guardar
    bool        m_loadFailed = false; // error al cargar (path invalido, etc)
};

} // namespace Mood
