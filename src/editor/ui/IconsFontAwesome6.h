#pragma once

// F2H36 — Subset de iconos FontAwesome 6 free solid usados en el editor.
// Solo definimos los macros que efectivamente usamos en Toolbar + MapEditorTopBar
// para no contaminar autocompletado con ~2000 nombres y dejar explicito que
// agregar un icono nuevo requiere extender este header.
//
// Encoding: cada macro es una string literal UTF-8 con los 3 bytes que codifican
// el codepoint del icono (rango 0xF000-0xF8FF ocupa 3 bytes UTF-8). Usamos
// escapes hex \xNN en lugar de literales unicode para que MSVC no emita warning
// C4566 sobre la code page del source.
//
// Mantener en sync con assets/ui/fonts/fa-solid-900.ttf (FA6 free solid).
// Si un icono no renderea en build (caja vacia / tofu), revisar primero que
// esta presente en la version de FA6 free que tenemos descargada.

namespace Mood {

// Rango de codepoints que pasamos a `AddFontFromFileTTF` como GlyphRanges.
// Cubre el bloque "Private Use Area" usado por FontAwesome 6 para sus icons:
// FA6 puso muchos nuevos en E000-EFFF y mantuvo legacy en F000-F8FF.
inline constexpr unsigned short k_FontAwesomeRange[] = { 0xE005, 0xF8FF, 0 };

} // namespace Mood

// Cada macro = string literal UTF-8 del icono. Listo para concat con el label:
//     ImGui::Button(ICON_FA_CUBE " Box");

// --- Gizmo modes (Toolbar) ---
#define ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT          "\xef\x82\xb2" // 0xF0B2 translate
#define ICON_FA_ROTATE                              "\xef\x8b\xb1" // 0xF2F1 rotate
#define ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER  "\xef\x90\xa4" // 0xF424 scale/expand

// --- Brushes / shapes ---
#define ICON_FA_CUBE                                "\xef\x86\xb2" // 0xF1B2
#define ICON_FA_CIRCLE                              "\xef\x84\x91" // 0xF111

// --- Tools / sub-mode (MapEditorTopBar) ---
#define ICON_FA_ARROW_POINTER                       "\xef\x89\x85" // 0xF245 mouse pointer
#define ICON_FA_PAINTBRUSH                          "\xef\x87\xbc" // 0xF1FC pincel
#define ICON_FA_SCISSORS                            "\xef\x83\x84" // 0xF0C4 clip
#define ICON_FA_OBJECT_GROUP                        "\xef\x89\x87" // 0xF247 object mode
#define ICON_FA_CIRCLE_DOT                          "\xef\x86\x92" // 0xF192 vertex mode
#define ICON_FA_MINUS                               "\xef\x81\xa8" // 0xF068 edge mode
#define ICON_FA_VECTOR_SQUARE                       "\xef\x97\x8b" // 0xF5CB face mode

// --- Snap / labels / acciones ---
#define ICON_FA_MAGNET                              "\xef\x81\xb6" // 0xF076 snap-to-vertex
#define ICON_FA_TAG                                 "\xef\x80\xab" // 0xF02B labels toggle
#define ICON_FA_CIRCLE_MINUS                        "\xef\x81\x96" // 0xF056 carve (subtract)
