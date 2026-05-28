#pragma once

// Barra de estado inferior del editor. Muestra FPS, modo actual y un mensaje
// libre de estado.

#include "core/Types.h"
#include "editor/application/EditorMode.h"

#include <string>

namespace Mood {

class StatusBar {
public:
    /// @brief Dibuja la status bar. Se posiciona pegada al borde inferior de
    ///        la viewport principal.
    void draw(EditorMode mode, EditorSubMode subMode = EditorSubMode::Object);

    void setFps(f32 fps) { m_fps = fps; }
    void setMessage(std::string msg) { m_message = std::move(msg); }

    /// @brief F3H23: actualizado por `EditorApplication::tickFrameMetrics`
    ///        cada frame. La status bar los muestra como chips opcionales
    ///        según `UserSettings.editor.statsOverlay.show<X>`.
    void setDrawCalls(u32 v)   { m_drawCalls = v; }
    void setTriangles(u32 v)   { m_triangles = v; }
    void setEntityCount(u32 v) { m_entityCount = v; }
    void setActiveLights(u32 v) { m_activeLights = v; }
    /// @brief F3H23: 0 = no disponible (driver sin NVX_gpu_memory). El
    ///        render muestra "—" en ese caso.
    void setVramUsedBytes(u64 v) { m_vramUsedBytes = v; }
    void setRssBytes(u64 v)      { m_rssBytes = v; }
    /// @brief F2H16: nombre del ultimo comando ejecutado (Blender-style
    ///        "Last Operator"). Sirve para que el dev sepa que va a
    ///        deshacer Ctrl+Z. Vacio = no muestra nada.
    void setLastCommand(std::string name) { m_lastCommand = std::move(name); }
    /// @brief F2H77: hay cambios en el proyecto sin guardar. Surfacea el
    ///        `m_projectDirty` de EditorApplication (que ya pone el " *" en el
    ///        titulo del SO) dentro del editor, donde se ve. Sincronizado en
    ///        `updateWindowTitle()` (cubre todas las transiciones de dirty).
    void setProjectDirty(bool v) { m_projectDirty = v; }

private:
    f32 m_fps = 0.0f;
    std::string m_message = "Listo";
    std::string m_lastCommand;
    bool m_projectDirty = false;
    // F3H23: stats chips (drawcalls/triangles/entityCount + lights + mem).
    u32 m_drawCalls = 0;
    u32 m_triangles = 0;
    u32 m_entityCount = 0;
    u32 m_activeLights = 0;
    u64 m_vramUsedBytes = 0;  // 0 = no disponible (driver sin NVX)
    u64 m_rssBytes = 0;
};

} // namespace Mood
