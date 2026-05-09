# PLAN HITO F2H36 — FontAwesome icons en el editor

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.26.0-fase2-hito36`.
> **Origen**: deuda explícita de F2H22 anotada en `PENDIENTES.md`
> ("FontAwesome icons en el editor — bajo riesgo, alto impacto
> visual"). Recomendado tras F2H35 como mini-hito chico de cierre
> visual del Hammer Editor.

## Objetivo

Reemplazar los labels de texto plano del **Toolbar** lateral
(`Mover`/`Rotar`/`Escala`/...) y del **MapEditorTopBar**
(`Selecc.`/`Bloque`/`Pincel`/...) por **iconos FontAwesome 6 free
solid + label corto**. Eleva la presentación del editor a un nivel
profesional sin agregar features ni tocar lógica de CSG / scene
graph / render. Patrón estándar de IDEs y editores 3D modernos
(Blender / Unreal / VS Code).

## Scope (decisiones confirmadas con dev)

1. **Solo Toolbar + MapEditorTopBar** — los 2 toolbars laterales del
   workspace "Editor de mapas". Quedan FUERA en este hito:
   - MenuBar (`Archivo`/`Editar`/`Ver`/...) — texto está bien.
   - Inspector / Hierarchy / Console / StatusBar — son de scope
     mayor y emergen como pase de polish UX general aparte.
2. **FontAwesome 6 free solid (`fa-solid-900.ttf`)** — descargada
   directo del repo oficial `FortAwesome/Font-Awesome` en GitHub al
   path `assets/ui/fonts/fa-solid-900.ttf`. Coverage gigante (~2000
   icons), estable, libre y desplegada via el `mood_runtime_files`
   custom target existente.
3. **Merge con la default font (ProggyClean)** via `MergeMode = true`
   — los iconos quedan disponibles en cualquier `ImGui::Text/Button`
   con un único atlas. Tamaño 13.0f para que coincida con el alto de
   la default font.
4. **Labels mantienen el texto castellano** corto al lado del icono
   (`ICON_FA_CUBE " Box"` no `ICON_FA_CUBE " "` solo). El texto sigue
   siendo accesible / leíble; el icono lo enriquece.
5. **Header `IconsFontAwesome6.h`** propio en `src/editor/ui/` con
   solo los ~15-20 macros que usamos. NO traemos el header full de
   ~2000 macros (peso innecesario, contamina autocompletado).
6. **Solo en MoodEditor** — el `MoodPlayer` no comparte init de ImGui
   con el editor y sus labels (Main Menu) son texto largo que NO se
   beneficia del icon merge.

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 5 min | Se archiva en cierre |
| B | Asset + atlas merge | 30 min | Descarga `fa-solid-900.ttf` al `assets/ui/fonts/` + header `IconsFontAwesome6.h` con subset + hook en `EditorApplication_Init.cpp` (AddFontDefault + AddFontFromFileTTF merge) |
| C | Iconos en toolbars | 30 min | Toolbar.cpp (Mover/Rotar/Escala/Box/Cilindro/Cara) + MapEditorTopBar.cpp (Selecc./Bloque/Pincel/Clip/Objeto/Vertex/Edge/Cara/Snap V/Nombres/Carve) |
| D | Build + validación visual | 15 min | `cmake --build build/debug --config Debug` + arrancar editor + revisar que los 17 botones renderean icon + label correctamente |
| E | Cierre | 20 min | Docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits agrupados + tag `v1.26.0-fase2-hito36` |

## NO entra en F2H36

- Iconos en MenuBar / Inspector / Hierarchy / Console / StatusBar.
- Reemplazar la font default (Lato — ya existe en assets pero no se
  carga; activarla es un hito propio "polish tipográfico").
- Subsets a la carta (rango ImWchar acotado solo a los icons que
  usamos para reducir el atlas a la mitad). Si el peso del atlas
  resulta problemático, optimizar después; FA6 free solid en TTF =
  ~400 KB en disco, atlas de ImGui no escala lineal con el rango.
- `IconsFontAwesome6.h` con macros completos (~2000 icons): solo el
  subset que usamos. Si en hitos futuros se agregan más controles
  con icon, se extiende este header.

## Riesgos y tradeoffs

- **Riesgo: descarga del TTF puede fallar** (timeout / cambio de URL
  en GitHub). Mitigación: descargar de `raw.githubusercontent.com`
  con `-LfsS` (follow redirects, fail on error, silent), validar el
  tamaño del archivo (~400 KB), commitear el binary al repo (es
  asset estático, no se regenera). Riesgo cero después.
- **Riesgo: los codepoints elegidos no existen en FA6 free solid**.
  Mitigación: usé solo iconos comunes y antiguos del FA pre-Pro
  (`CUBE`, `ROTATE`, `SCISSORS`, `MAGNET`, `TAG`, `DRAW_POLYGON`,
  etc.). En validación (Bloque D) si alguno renderea como caja vacía
  (glyph faltante), reemplazo por uno cercano.
- **Tradeoff: peso del binary del editor**. Atlas con FA6 free solid
  añade ~150 KB al texture atlas runtime + 400 KB del TTF en disco.
  Aceptable para un editor — `MoodEditor.exe` ya usa varios MB del
  asset directory en cada deploy.
- **Tradeoff: si el dev decide cambiar de pack** (Lucide / Material
  Symbols) en el futuro, hay que reescribir el header de macros.
  Bajo costo (~17 macros) — y FA6 es el estándar de facto que es
  improbable migrar.

## Validación

- `cmake --build build/debug --config Debug` debe compilar sin
  warnings nuevos.
- `MoodEditor.exe` arranca, abre el workspace "Editor de mapas".
- Toolbar lateral muestra:
  - 3 botones gizmo (Mover/Rotar/Escala) con flechas + rotate +
    expand icons.
  - 2 botones brush (Box/Cilindro) con cube + circle icons.
  - 1 botón Cara con vector-square icon.
- MapEditorTopBar muestra (sección Herramienta):
  - Selecc. con arrow-pointer.
  - Bloque con cube.
  - Pincel con paintbrush.
  - Clip con scissors.
- Sub-modo: Objeto (object-group), Vertex (circle-dot), Edge (minus),
  Cara (vector-square).
- Snap: Snap V con magnet icon.
- Visualización: Nombres con tag icon.
- Acciones: Carve con circle-minus icon.

## Cambio importante (anotar en cierre si aplica)

Si en el Bloque D algún codepoint renderea como caja vacía o el dev
pide reemplazar un icono específico, anotar la decisión en
`DECISIONS.md` con la razón y el icono final usado.
