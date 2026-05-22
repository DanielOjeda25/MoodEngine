#include "editor/ui/EditorThemes.h"

#include <imgui.h>

namespace Mood::EditorThemes {

namespace {

const std::vector<ThemeInfo> k_themes = {
    {"dark",     "theme.dark"},
    {"light",    "theme.light"},
    {"midnight", "theme.midnight"},
    {"sepia",    "theme.sepia"},
};

// Midnight: base dark con tinte azulado + acento celeste. Mas contraste que
// el dark estandar, look "nocturno".
void applyMidnight(ImGuiStyle& style) {
    ImGui::StyleColorsDark(&style);
    ImVec4* c = style.Colors;
    const ImVec4 bg0(0.07f, 0.08f, 0.12f, 1.00f);  // ventana
    const ImVec4 bg1(0.10f, 0.12f, 0.18f, 1.00f);  // frames
    const ImVec4 bg2(0.13f, 0.16f, 0.24f, 1.00f);  // headers/hover
    const ImVec4 acc(0.26f, 0.55f, 0.95f, 1.00f);  // acento celeste
    const ImVec4 accHi(0.36f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_WindowBg]        = bg0;
    c[ImGuiCol_ChildBg]         = bg0;
    c[ImGuiCol_PopupBg]         = bg0;
    c[ImGuiCol_FrameBg]         = bg1;
    c[ImGuiCol_FrameBgHovered]  = bg2;
    c[ImGuiCol_FrameBgActive]   = bg2;
    c[ImGuiCol_TitleBg]         = bg0;
    c[ImGuiCol_TitleBgActive]   = bg1;
    c[ImGuiCol_MenuBarBg]       = bg1;
    c[ImGuiCol_Header]          = bg2;
    c[ImGuiCol_HeaderHovered]   = acc;
    c[ImGuiCol_HeaderActive]    = accHi;
    c[ImGuiCol_Button]          = bg2;
    c[ImGuiCol_ButtonHovered]   = acc;
    c[ImGuiCol_ButtonActive]    = accHi;
    c[ImGuiCol_CheckMark]       = accHi;
    c[ImGuiCol_SliderGrab]      = acc;
    c[ImGuiCol_SliderGrabActive]= accHi;
    c[ImGuiCol_Tab]             = bg1;
    c[ImGuiCol_TabHovered]      = acc;
    c[ImGuiCol_TabActive]       = bg2;
    c[ImGuiCol_TitleBgCollapsed]= bg0;
    c[ImGuiCol_Separator]       = bg2;
    c[ImGuiCol_FrameBgActive]   = bg2;
}

// Sepia: base light con tinte calido (papel/madera). Para sesiones largas con
// luz, mas suave que el blanco puro.
void applySepia(ImGuiStyle& style) {
    ImGui::StyleColorsLight(&style);
    ImVec4* c = style.Colors;
    const ImVec4 bg0(0.93f, 0.89f, 0.81f, 1.00f);  // ventana (papel)
    const ImVec4 bg1(0.88f, 0.83f, 0.73f, 1.00f);  // frames
    const ImVec4 bg2(0.82f, 0.75f, 0.62f, 1.00f);  // headers/hover
    const ImVec4 acc(0.62f, 0.42f, 0.22f, 1.00f);  // acento marron
    const ImVec4 accHi(0.74f, 0.52f, 0.30f, 1.00f);
    const ImVec4 txt(0.24f, 0.18f, 0.10f, 1.00f);  // texto marron oscuro
    c[ImGuiCol_Text]            = txt;
    c[ImGuiCol_WindowBg]        = bg0;
    c[ImGuiCol_ChildBg]         = bg0;
    c[ImGuiCol_PopupBg]         = bg0;
    c[ImGuiCol_FrameBg]         = bg1;
    c[ImGuiCol_FrameBgHovered]  = bg2;
    c[ImGuiCol_FrameBgActive]   = bg2;
    c[ImGuiCol_TitleBg]         = bg1;
    c[ImGuiCol_TitleBgActive]   = bg2;
    c[ImGuiCol_MenuBarBg]       = bg1;
    c[ImGuiCol_Header]          = bg2;
    c[ImGuiCol_HeaderHovered]   = acc;
    c[ImGuiCol_HeaderActive]    = accHi;
    c[ImGuiCol_Button]          = bg2;
    c[ImGuiCol_ButtonHovered]   = acc;
    c[ImGuiCol_ButtonActive]    = accHi;
    c[ImGuiCol_CheckMark]       = acc;
    c[ImGuiCol_SliderGrab]      = acc;
    c[ImGuiCol_SliderGrabActive]= accHi;
    c[ImGuiCol_Tab]             = bg1;
    c[ImGuiCol_TabHovered]      = acc;
    c[ImGuiCol_TabActive]       = bg2;
}

// F2H76/F2H77: geometria comun a todos los temas (redondeo + espaciado). Las
// funciones StyleColorsX y las paletas custom solo tocan colores, no geometria,
// asi que estas metricas las seteamos aparte y se aplican igual a los 4 temas
// (y sobreviven al theme-switch). Look mas suave/moderno y un espaciado
// consistente para que todos los paneles "respiren" igual.
void applyMetrics(ImGuiStyle& style) {
    // --- F2H76: redondeo (vs. bordes en punta) ---
    // F2H78 polish: redondeo solo en contenedores "flotantes" y widgets
    // (ventanas, popups, botones, grabs). Las regiones hijas, scrollbars y
    // tabs van RECTAS — redondearlas dentro de un panel cuadrado se ve mal
    // (esquinas que no cierran contra el borde del dock).
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 0.0f;  // regiones internas / tablas: rectas
    style.FrameRounding     = 4.0f;  // botones, combos, inputs
    style.PopupRounding     = 6.0f;  // menus desplegables + modales
    style.ScrollbarRounding = 0.0f;  // se integran con el borde del panel
    style.GrabRounding      = 4.0f;  // grab de sliders
    style.TabRounding       = 0.0f;  // tabs de workspace / docking: rectas

    // --- F2H77: espaciado/padding unificado (defaults de ImGui daban un look
    // apretado e inconsistente entre paneles). Valores algo mas generosos para
    // que las filas respiren y los click-targets sean comodos. ---
    style.WindowPadding     = ImVec2(10.0f, 8.0f);   // margen interno de ventanas
    style.FramePadding      = ImVec2(8.0f, 5.0f);    // padding de widgets (rows mas altas)
    style.ItemSpacing       = ImVec2(8.0f, 7.0f);    // separacion vertical entre items
    style.ItemInnerSpacing  = ImVec2(6.0f, 5.0f);    // label<->widget
    style.CellPadding       = ImVec2(6.0f, 5.0f);    // tablas (stats de items, etc.)
    style.IndentSpacing     = 20.0f;                 // arbol de jerarquia
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 11.0f;
}

} // namespace

const std::vector<ThemeInfo>& available() { return k_themes; }

void apply(const std::string& id) {
    ImGuiStyle& style = ImGui::GetStyle();
    if (id == "light") {
        ImGui::StyleColorsLight(&style);
    } else if (id == "midnight") {
        applyMidnight(style);
    } else if (id == "sepia") {
        applySepia(style);
    } else {
        // "dark" + cualquier id desconocido.
        ImGui::StyleColorsDark(&style);
    }
    applyMetrics(style);  // F2H76/F2H77: redondeo + espaciado comun a todos los temas
}

} // namespace Mood::EditorThemes
