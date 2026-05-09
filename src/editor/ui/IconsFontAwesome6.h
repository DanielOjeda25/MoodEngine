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

// === F2H37: extension al resto del editor ===

// --- Entity types (Hierarchy / VisGroups / Inspector headers) ---
#define ICON_FA_CUBES_STACKED                       "\xee\x93\xa6" // 0xE4E6 brush (CSG)
#define ICON_FA_LIGHTBULB                           "\xef\x83\xab" // 0xF0EB light
#define ICON_FA_VOLUME_HIGH                         "\xef\x80\xa8" // 0xF028 audio source
#define ICON_FA_FILE_CODE                           "\xef\x87\x89" // 0xF1C9 script (lua)
#define ICON_FA_BORDER_NONE                         "\xef\xa1\x90" // 0xF850 trigger
#define ICON_FA_VIDEO                               "\xef\x80\xbd" // 0xF03D camera
#define ICON_FA_FIRE                                "\xef\x81\xad" // 0xF06D particle emitter

// --- Inspector component headers (extra mas alla de entity types) ---
#define ICON_FA_WEIGHT_HANGING                      "\xef\x97\x8d" // 0xF5CD rigidbody (physics)
#define ICON_FA_PERSON_RUNNING                      "\xef\x9c\x8c" // 0xF70C animator
#define ICON_FA_TREE                                "\xef\x86\xbb" // 0xF1BB environment

// --- Asset types (AssetBrowserPanel tabs + rows) ---
#define ICON_FA_IMAGE                               "\xef\x80\xbe" // 0xF03E texture
#define ICON_FA_BOX_OPEN                            "\xef\x92\x9e" // 0xF49E prefab
#define ICON_FA_PALETTE                             "\xef\x94\xbf" // 0xF53F material
#define ICON_FA_MUSIC                               "\xef\x80\x81" // 0xF001 audio asset

// --- Console log levels ---
#define ICON_FA_BUG                                 "\xef\x86\x88" // 0xF188 trace
#define ICON_FA_BUG_SLASH                           "\xee\x92\x90" // 0xE490 debug
#define ICON_FA_CIRCLE_INFO                         "\xef\x81\x9a" // 0xF05A info
#define ICON_FA_TRIANGLE_EXCLAMATION                "\xef\x81\xb1" // 0xF071 warn
#define ICON_FA_CIRCLE_XMARK                        "\xef\x81\x97" // 0xF057 err
#define ICON_FA_SKULL                               "\xef\x95\x8c" // 0xF54C critical

// --- MenuBar (top-level menus) ---
#define ICON_FA_FOLDER                              "\xef\x81\xbb" // 0xF07B Archivo
#define ICON_FA_PEN_TO_SQUARE                       "\xef\x81\x84" // 0xF044 Editar
#define ICON_FA_MAP                                 "\xef\x89\xb9" // 0xF279 Mapa
#define ICON_FA_EYE                                 "\xef\x81\xae" // 0xF06E Ver
#define ICON_FA_CIRCLE_QUESTION                     "\xef\x81\x99" // 0xF059 Ayuda

// --- MenuBar (workspace tabs + Play/Stop) ---
#define ICON_FA_PLAY                                "\xef\x81\x8b" // 0xF04B
#define ICON_FA_STOP                                "\xef\x81\x8d" // 0xF04D
#define ICON_FA_TABLE_COLUMNS                       "\xef\x83\x9b" // 0xF0DB Layout workspace
#define ICON_FA_CODE                                "\xef\x84\xa1" // 0xF121 Scripting workspace
#define ICON_FA_GAUGE                               "\xef\x98\xa4" // 0xF624 Profile workspace
