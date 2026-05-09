#pragma once

// Estado global del editor: que camara esta activa, si se capturan
// entradas como FPS, etc. Compartido entre EditorApplication, EditorUI y
// los sub-componentes de la UI (MenuBar, StatusBar).

namespace Mood {

enum class EditorMode {
    /// Editor Mode: camara orbital, paneles interactivos, mouse libre.
    Editor,
    /// Play Mode: camara FPS, WASD + mouse capturado. Sale con Esc.
    Play,
};

/// @brief F2H17: sub-modo del Editor Mode estilo Blender. Aplica
///        SOLO cuando EditorMode == Editor; en Play Mode siempre
///        es Object implicito.
///
///        Toggle con teclas (mismo orden que Blender):
///        - `1`: Vertex Mode (F2H30).
///        - `2`: Edge Mode (F2H30).
///        - `3`: Face Mode (F2H17).
///        - `Esc`: vuelve a Object Mode.
enum class EditorSubMode {
    Object,  // Default: pickea brushes enteros, gizmo de transform.
    Vertex,  // F2H30: pickea + mueve vertices del brush activo.
    Edge,    // F2H30: pickea + mueve edges del brush activo.
    Face,    // F2H17: pickea caras individuales del brush activo.
};

/// @brief F2H31 Bloque B: tool del workspace "Editor de mapas". Selecciona
///        que pasa con un drag en EMPTY SPACE de un orto:
///        - Select:      drag dibuja rectangulo amarillo de marquee y al
///                       soltar selecciona N brushes que toca el rectangulo
///                       (replace / Shift=add / Ctrl=toggle). Click en un
///                       brush sigue siendo click-select; drag sobre un
///                       brush sigue siendo drag-edit. Default Hammer-style.
///        - CreateBlock: drag dibuja preview celeste del brush a spawnear,
///                       como block tool de F2H29. Click en empty space
///                       no hace nada.
///        - Pincel:      drag/click agrega vertices al poligono. Setea
///                       m_polyDraw.active=true (mismo flag que F2H30
///                       Bloque C). Mutually exclusive con Select / Block.
///        El sub-modo Vertex/Edge/Face es ortogonal — opera sobre brushes
///        ya seleccionados, no sobre empty space.
enum class MapTool {
    Select = 0,
    CreateBlock = 1,
    Pincel = 2,
};

/// @brief Acciones de proyecto que la UI solicita y EditorApplication atiende.
enum class ProjectAction {
    None,
    NewProject,
    OpenProject,
    Save,
    SaveAs,
    CloseProject,
    PackageProject, // Hito 21 Bloque 5: empaqueta el proyecto activo
    NewScript,      // Hito 22 Bloque 3: crea assets/scripts/<nombre>.lua
    // F2H8: gestion multi-mapa intra-proyecto.
    NewMap,
    SaveMapAs,
    SetCurrentMapAsDefault,
    DeleteCurrentMap,
    AddBoxBrush, // F2H11: agrega un brush CSG Box al mapa actual
    // F2H14: primitivas extendidas.
    AddCylinderBrush,
    AddSphereBrush,
    AddPyramidBrush,
    AddWedgeBrush,
    AddPrismTriangularBrush,
    AddPrismHexagonalBrush,
    // F2H20: compilacion brush -> mesh estatica + export OBJ.
    CompileMap,
    ExportObj,
};

} // namespace Mood
