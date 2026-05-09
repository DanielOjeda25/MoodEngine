# PLAN HITO F2H38 — Default font ImGui a Lato

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.28.0-fase2-hito38`.
> **Origen**: pendiente nice-to-have anotado en F2H37 cierre. La
> font `LatoLatin-Regular.ttf` ya está en `assets/ui/fonts/` desde
> antes pero nunca se cargó — F2H37 solo agregó FA mergeada al
> ProggyClean default.

## Objetivo

Reemplazar la default font de ImGui (ProggyClean, 13px pixely)
por **Lato Latin Regular** a tamaño legible. Mantiene el merge de
FontAwesome 6 que F2H36 introdujo, ajustando ambas fonts a la misma
reference size para que ImGui 1.92 no tire los asserts de mismatch.

## Ganancias esperadas

1. **Legibilidad mejorada** — Lato es una sans-serif diseñada para
   pantalla, smooth scaling, kerning correcto. ProggyClean es bitmap
   monoespaciada de 13px diseñada para terminales.
2. **Coverage Unicode más amplio** — Lato cubre Basic Latin +
   Latin-1 Supplement + General Punctuation (em-dash U+2014, en-dash
   U+2013, ellipsis U+2026, comillas curvas, etc.) — ProggyClean
   solo cubre Basic Latin + Latin-1.
3. **Consola más leíble** — el Console panel con muchos logs es el
   panel más text-heavy y donde más se nota el upgrade.
4. **Side effect del fix em-dash de F2H37** — el tofu del Welcome
   modal era síntoma de la coverage limitada de ProggyClean. Lato lo
   resolvía. Pero el fix `—`→`-` queda en su lugar: es approach más
   robusto (no depende del font coverage).

## Scope (decisiones confirmadas con dev)

1. **Lato 15px** como default. Razón: ProggyClean a 13px es muy
   pixely; tipografías sans-serif "rinden" a 14-16px en ImGui. 15px
   matchea lo que VSCode / JetBrains usan por default. Bumpeable a
   14 o 16 si en validación se ve mal.

2. **FA mergeada con misma reference size (15px)**. Razón: ImGui
   1.92 tira `IM_ASSERT((font->Flags & ImFontFlags_ImplicitRefSize)
   == 0 && "Cannot use MergeMode with an explicit reference size
   when the destination font used an implicit reference size!")` si
   las fonts no comparten la convención. F2H36 resolvió pasando 0.0f
   al merge porque AddFontDefault es implicit. Ahora con Lato
   explicit, el merge también tiene que ser explicit. 13px de FA
   funciona bien con texto a 15px (icons ~85% del texto = legible).

3. **Custom GlyphRanges para Lato** que cubra Basic Latin + Latin-1
   + General Punctuation (rango `{0x0020, 0x00FF, 0x2010, 0x2027,
   0}`). Razón: General Punctuation incluye em-dash (U+2014),
   en-dash (U+2013), ellipsis (U+2026), comillas curvas (U+2018-201D)
   — caracteres que aparecen naturalmente en strings en español si
   alguien los escribe sin querer. Lato los cubre todos.

4. **NO se revierte el fix em-dash de F2H37** (`—`→`-` en
   EditorUI.cpp). Razón: el `-` es universal Latin, funciona con
   cualquier font; mantener el fix evita reintroducir dependencia
   en el coverage de Lato si alguien cambia la font en el futuro.

5. **NO se cambia la font del MoodPlayer** — el Player usa su propio
   init de ImGui (PlayerApplication_Init.cpp). Si el dev quiere
   coherencia visual entre Editor y Player, mini-fix posterior. Pero
   F2H38 acota a Editor.

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 5 min | Se archiva en cierre |
| B | Cargar Lato + ajustar merge FA + glyph ranges | ~20 min | Edit en `EditorApplication_Init.cpp`, ~15 LOC neto. Validar build OK |
| C | Build + tour visual | ~15 min | Recorrer cada panel buscando re-flow / truncate / overflow / spacing roto |
| D | Ajustes puntuales | ~30-60 min | Probable: button widths que F2H36/F2H37 ajusté para ProggyClean (Toolbar 92px, Play/Stop 80px) pueden necesitar bump. Hierarchy / Inspector labels pueden wrap si los Drag/Slider están al límite |
| E | Docs cierre + commits + tag | ~20 min | Patrón estándar |

**Total estimado**: ~1.5h. Mini-hito chico.

## Riesgos y tradeoffs

- **Riesgo: re-flow de UI** — Lato es proporcional vs ProggyClean
  monoespaciada. Botones / labels que se ajustaron al pixel para
  ProggyClean pueden quedar cortos o con padding inconsistente.
  Mitigación: Bloque C tour exhaustivo + Bloque D presupuestado
  para ajustes.
- **Riesgo: tamaño 15px puede verse muy chico o muy grande** —
  validación visual decide. Bumpeable a 14 / 16 sin tocar nada más.
- **Tradeoff: peso atlas + binary** — Lato Latin es ~150 KB, agrega
  glyphs al atlas runtime. Aceptable: el editor ya tiene MB de
  assets.
- **Tradeoff: no toca MoodPlayer** — coherencia visual entre Editor y
  Player se rompe si el dev compara side-by-side. Si emerge presión,
  fix chico posterior (mismo patrón en PlayerApplication_Init.cpp).

## Validación

- Build limpio.
- Editor arranca sin asserts.
- Tour visual cubre:
  - MenuBar: top items + workspace tabs + Play/Stop legibles.
  - Toolbar lateral: 6 botones (Mover/Rotar/Escala/Box/Cilindro/Cara)
    legibles, sin truncate.
  - MapEditorTopBar: 11 botones legibles, sin truncate.
  - Hierarchy: rows legibles, icons FA alineadas con el texto Lato.
  - Inspector: SeparatorText headers + sliders + labels OK.
  - AssetBrowser: tabs + rows OK.
  - Console: logs legibles (este es el panel donde más se nota el
    upgrade).
  - StatusBar: FPS / mode / sub-mode legibles.
  - Welcome modal: sin tofu (ya estaba resuelto en F2H37 con `-`).

## NO entra en F2H38

- Cambiar font del MoodPlayer (deuda chica si emerge).
- Cargar otra variante de Lato (Bold / Italic) — solo Regular.
- Mejorar el atlas (custom rasterizer / FreeType backend).
- Theme custom de ImGui (StyleColors) — out of scope.
