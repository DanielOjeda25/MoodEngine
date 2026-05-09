# PLAN HITO F2H37 — FontAwesome icons en el resto del editor + polish UX general

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.27.0-fase2-hito37`.
> **Origen**: pedido explícito del dev al validar F2H36 (*"deberemos
> integrarlos en otras areas del proyecto para que todo sea
> equitativo"*) + fusión con el pendiente "Pase de polish UX general
> continuo" anotado desde F2H21+F2H22.

## Objetivo

Extender los iconos FontAwesome 6 free solid (mergeados al atlas en
F2H36) al resto de los paneles del editor para coherencia visual
total. **Unifica** dos pendientes en un solo hito por solapamiento
de paneles tocados:

1. **Iconos** en MenuBar / Hierarchy / Inspector / AssetBrowser /
   Console / StatusBar / paneles auxiliares.
2. **Polish UX** que arrastra desde F2H21+F2H22: drop targets en
   Inspector, feedback de multi-select en Hierarchy, filtrado por
   categoría en Console, layout/colores en StatusBar.

## Scope (decisiones confirmadas con dev)

1. **9 paneles cubiertos** según audit del Bloque A:
   - MenuBar (`src/editor/ui/MenuBar.cpp`, 408 LOC) — prefijos icon
     en menus principales (Archivo / Editar / Mapa / Ver / Ayuda) +
     icons en workspace tabs.
   - StatusBar (`src/editor/ui/StatusBar.cpp`, 77 LOC) — icons en
     mode/submode indicators (Play/Editor/Vertex/Edge/Face).
   - HierarchyPanel (`src/editor/panels/scene/HierarchyPanel.cpp`,
     196 LOC) — reemplazar ASCII `[M]/[B]/[L]/[A]/[S]/[T]/[C]/[P]`
     por iconos FA via helper compartido `iconForEntityType`.
   - InspectorPanel (`src/editor/panels/scene/InspectorPanel*.cpp`,
     1683 LOC distribuidos en 1 dispatcher + 10 partials) — icon
     en cada header de componente (13 tipos).
   - AssetBrowserPanel (`src/editor/panels/assets/AssetBrowserPanel.cpp`,
     474 LOC) — icon por tab (6 tabs: Texturas / Meshes / Prefabs /
     Materiales / Scripts / Audio) + helper `iconForAssetType` para
     el row inicial donde aplique.
   - ConsolePanel (`src/editor/panels/debug/ConsolePanel.cpp`, 146
     LOC) — icons por nivel (trace/debug/info/warn/err/critical) +
     dropdown de level filter (polish UX del pendiente).
   - VisGroupsPanel (`src/editor/panels/scene/VisGroupsPanel.cpp`,
     335 LOC) — migrado al helper compartido `iconForEntityType`
     (consolidación de la duplicación `entityIconStr` detectada).
   - MaterialEditorPanel + ScriptEditorPanel (paneles auxiliares):
     icon-prefijo en buttons (Save / Reload / etc.) si aplica.

2. **Helper compartido `iconForEntityType` y `iconForAssetType`**
   en `src/editor/ui/IconsFontAwesome6.h` o un header nuevo
   `editor/ui/IconHelpers.h`. Razón: HierarchyPanel + VisGroupsPanel
   tenían `entityIconStr` duplicado; consolidar antes de extender
   evita triple-duplicación al sumar Inspector. Hardcode mapping en
   un solo lugar = un solo punto de cambio si se renombra un icono.

3. **Polish UX consolidado en cada bloque relevante**:
   - **Inspector**: drop targets visibles con hint text + icon.
     Already partially done en MaterialEditor (F2H21); extender
     consistentemente en mesh selector / material picker / script
     selector / etc. dentro de InspectorPanel partials.
   - **Hierarchy**: feedback visual mejorado para multi-selection
     (pre-F2H37 solo el active tiene color naranja; las secundarias
     no se distinguen visualmente). Color amarillo claro para
     non-active selected.
   - **Console**: dropdown / checkboxes para filter por level (hoy
     solo filtra por canal text). Mostrar/ocultar TRC/DBG/INF/WRN/
     ERR/CRT independientemente.
   - **StatusBar**: icons en mode indicators (Play/Editor/Sub-modes)
     ya tiene colors, agregar icons completa el feedback.

4. **NO se incluye**:
   - Refactor profundo de paneles (extracción de funciones,
     reorganización de partials) — preservar API y estructura
     existentes.
   - Cambio de la default font global a Lato — fuera de scope, hito
     propio si emerge.
   - Splash / about / popups marginales — solo paneles del flow
     principal.
   - HUD del MoodPlayer — diferido, no es editor.
   - Iconos color (multi-color SVG-style) — FA6 free solid es
     monocromo; cambio de pack es scope mayor.

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 10 min | Audit ya hecho. Se archiva en cierre |
| B | Extender `IconsFontAwesome6.h` (~30 macros) + helper `iconForEntityType` + `iconForAssetType` + iconos en MenuBar + workspace tabs | ~45 min | Header subset crece de ~15 a ~45 macros. Mapping puro: switch sobre tipo |
| C | HierarchyPanel: usar helper + multi-select polish (color secundario) | ~30 min | Reemplazar `entityIconStr` por helper compartido. Color non-active selected |
| D | VisGroupsPanel: migrado al helper compartido | ~15 min | Drop-in del helper, eliminar duplicado local |
| E | InspectorPanel: icons en headers de los 13 components + polish drop targets | ~1h | 10 partials touchados (uno por componente). Drop target hints donde aplique |
| F | AssetBrowserPanel: icons en 6 tabs + icon-por-tipo en rows + audio drag-drop si encaja | ~45 min | TabBarItem con icon prefix + helper `iconForAssetType` |
| G | ConsolePanel: icons por nivel + level filter dropdown | ~30 min | Icon prefix en cada log row + checkbox grupo o dropdown para filter |
| H | StatusBar polish: icons en mode/submode indicators | ~15 min | Mini bloque, el panel ya está pulido. Solo icons |
| I | Build + validación visual con dev | ~15 min | Compilar + arrancar + tour por cada panel. Si tofu, sustitución |
| J | Cierre: docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits + tag | ~30 min | Patrón estándar |

**Total estimado**: ~5h. Mini-hito grande pero acotado a UX puro.

## Iconos a agregar al header (extension de F2H36)

Mapeo confirmado tras audit:

**Entity types (HierarchyPanel + VisGroupsPanel + Inspector)**:
- MeshRenderer → `ICON_FA_CUBE` (ya existe)
- Brush (CSG) → `ICON_FA_CUBES_STACKED` (nuevo)
- Light → `ICON_FA_LIGHTBULB` (nuevo)
- Audio → `ICON_FA_VOLUME_HIGH` (nuevo)
- Script → `ICON_FA_FILE_CODE` (nuevo)
- Trigger → `ICON_FA_BORDER_NONE` (nuevo) o `ICON_FA_DICE_FOUR` placeholder
- Camera → `ICON_FA_VIDEO` (nuevo)
- Particle → `ICON_FA_FIRE` (nuevo)
- Empty/None → `ICON_FA_CIRCLE` (ya existe)

**Inspector component headers (13 types adicionales más allá de
los entity types listados arriba)**:
- TagComponent → `ICON_FA_TAG` (ya existe)
- TransformComponent → `ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT` (ya existe)
- RigidBodyComponent (Physics) → `ICON_FA_WEIGHT_HANGING` (nuevo)
- AnimatorComponent → `ICON_FA_PERSON_RUNNING` (nuevo)
- EnvironmentComponent → `ICON_FA_TREE` (nuevo) o `ICON_FA_CLOUD_SUN`
- CameraComponent → `ICON_FA_VIDEO` (mismo que entity type)

**Asset types (AssetBrowserPanel tabs)**:
- Texture → `ICON_FA_IMAGE` (nuevo)
- Mesh → `ICON_FA_CUBE` (mismo que entity)
- Prefab → `ICON_FA_BOX_OPEN` (nuevo) o `ICON_FA_BOX`
- Material → `ICON_FA_PALETTE` (nuevo)
- Script → `ICON_FA_FILE_CODE` (mismo que entity)
- Audio → `ICON_FA_MUSIC` (nuevo) o `ICON_FA_VOLUME_HIGH`

**Log levels (ConsolePanel)**:
- trace → `ICON_FA_BUG` (nuevo) o `ICON_FA_BARCODE`
- debug → `ICON_FA_BUG_SLASH` (nuevo) o reuso BUG
- info → `ICON_FA_CIRCLE_INFO` (nuevo)
- warn → `ICON_FA_TRIANGLE_EXCLAMATION` (nuevo)
- err → `ICON_FA_CIRCLE_XMARK` (nuevo)
- critical → `ICON_FA_SKULL` (nuevo) o `ICON_FA_FIRE_FLAME_CURVED`

**MenuBar items principales**:
- Archivo → `ICON_FA_FOLDER` (nuevo)
- Editar → `ICON_FA_PEN_TO_SQUARE` (nuevo)
- Mapa → `ICON_FA_MAP` (nuevo)
- Brush (CSG) → reuso CUBES_STACKED
- Ver → `ICON_FA_EYE` (nuevo)
- Ayuda → `ICON_FA_CIRCLE_QUESTION` (nuevo)
- Play / Stop (workspace toolbar) → `ICON_FA_PLAY` / `ICON_FA_STOP` (nuevos)
- Workspaces → `ICON_FA_TABLE_COLUMNS` (Layout) / `ICON_FA_CODE`
  (Scripting) / `ICON_FA_GAUGE` (Profile) / `ICON_FA_PALETTE`
  (Materials, reuso) / `ICON_FA_MAP` (Editor de mapas, reuso)

**Total nuevos macros**: ~30. Final del header ~45 macros (vs. 15
de F2H36).

## NO entra en F2H37

- Cambio de la default font global (Lato).
- Refactor de partials del InspectorPanel (solo edit puntual).
- Iconos color / multi-tone / SVG.
- HUD MoodPlayer.
- About popup / splash / decoraciones marginales.

## Riesgos y tradeoffs

- **Riesgo: alguno de los ~30 codepoints nuevos no existe en FA6
  free solid.** Mitigación: priorizar icons clásicos y antiguos
  (FA4/FA5 carryovers); en validación visual (Bloque I) sustituir
  los que aparezcan como tofu. Documentar en DECISIONS.md el
  resultado.
- **Tradeoff: scope grande comparado con F2H36.** Estimado ~5h en 8
  bloques. Mitigación: cada bloque cubre un panel completo con
  rollback claro (revert del bloque) si emerge pressure de tiempo;
  los bloques son independientes (no se acoplan entre sí).
- **Tradeoff: el helper compartido `iconForEntityType` cierra
  dependencia entre 3 paneles** (HierarchyPanel + VisGroupsPanel +
  potencialmente InspectorPanel para el header del componente
  primario). Si en un hito futuro alguien renombra los components,
  el helper hay que actualizarlo. Es scope chico (un switch); el
  beneficio (un solo lugar de mapping) supera el costo.
- **Polish UX puede ser subjetivo** — el dev validó en cada hito
  previo. Si en algún Bloque emerge desacuerdo (ej. el dev quiere
  el multi-select en otro color), iterar en el bloque sin abrir
  hito propio.

## Validación

Cada bloque tiene su validación específica enumerada en el bloque
correspondiente. La validación final (Bloque I) es un tour visual:
arrancar el editor, abrir un proyecto, recorrer cada panel
confirmando que los icons se ven y los polish UX (multi-select,
drop targets, level filter) funcionan.

## Cambio importante (anotar en cierre si aplica)

Si en validación visual emergen sustituciones de iconos por tofu o
feedback del dev sobre mapping (ej. "Audio debería ser MUSIC, no
VOLUME_HIGH"), anotar en DECISIONS.md el mapping final con razón.
