# PLAN HITO F2H47 — Dialog Editor (editor side: schema + visual editor + inspector)

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-10).
> **Tag previsto**: `v1.37.0-fase2-hito47`.
> **Origen**: Bloque 2 del plan `PLAN_SUBFASE_2_5.md` (Sub-fase 2.5
> Sistemas de juego). Primer editor real construido sobre la infra
> del node-graph framework de F2H46. Se enfoca en EL LADO EDITOR:
> permitir al dev crear, editar, guardar y abrir árboles de diálogo
> visualmente. El runtime (interpretar el árbol en Play Mode + HUD +
> Lua bindings) queda para **F2H48**.

## Filosofía

F2H47 entrega la **herramienta de autoría**, no el sistema de runtime.
Cuando cierre F2H47, el dev podrá:

- Crear un `.mooddialog` desde el editor.
- Armar un árbol visualmente: agregar nodos `dialog_line`, conectarlos
  con sockets de flow + choice, editar texto/portrait/audio/condiciones
  por nodo en un Inspector contextual.
- Guardar/cargar `.mooddialog` con persistencia JSON versionada.
- Tener tests del schema + serialización.

Lo que NO podrá hacer todavía: ver el diálogo correr en Play Mode con
un NPC. Eso es F2H48 (DialogSystem runtime + HUD widget + Lua bindings
+ DialogComponent). Mantengo el split editor/runtime porque:

1. Es un checkpoint natural de validación — el dev autoriza la API del
   asset ANTES de que el runtime quede atado a ese schema.
2. Reduce riesgo: si el runtime emerge con requisitos extra del schema,
   bumpear `.mooddialog` v1 → v2 cuesta poco antes de tener saves
   reales.
3. Cada hito mantenible: F2H47 ~10h, F2H48 ~8-10h. Total Dialog ~20h
   distribuidos. Match con scope B Pro Tools.

**Principios engine-grade aplicados (no-negociables del marco
estratégico):**

1. **Data-driven**: el dialog tree vive en `.mooddialog` JSON, editable
   100% visualmente. Cero código C++ para crear contenido.
2. **Sin semántica hardcodeada de gameplay**: el schema NO conoce
   "XP", "stats", "items" específicos. Hooks Lua hacen ese trabajo.
3. **Editor visual real**: el grafo node-based es la interfaz primaria.
   El JSON crudo es output del editor, no algo que el dev edita a mano.
4. **State vs rendering separados**: schema + serializer en
   `engine/dialog/` puro (testeable sin ImGui); editor visual en
   `editor/panels/narrative/`.

## Scope (decisiones a confirmar con el dev al arrancar)

### `.mooddialog` schema v1

```jsonc
{
  "_version": 1,
  "graph": { /* output de NodeGraph::Graph::toJson() */ },
  "metadata": {
    "name": "intro_npc_guard",   // ID estable del dialogo (sin extension)
    "start_node_id": 7,           // que nodo arranca cuando se llama dialog.start()
    "default_portrait": "characters/guard.png",  // fallback si un nodo no especifica
    "default_audio_bus": "voice"  // bus de audio para los voiceovers
  }
}
```

El campo `graph` reusa toda la maquinaria de `NodeGraph::Graph` de
F2H46 — el wrapper de Dialog solo agrega `metadata` arriba y define
qué `typeTag` puede aparecer en los nodos.

### Tipos de nodos v1

**`dialog_line`** — único tipo de nodo en v1.

`customData` schema:
```jsonc
{
  "text_key": "dialog.intro_npc.line_1",   // i18n key del texto del NPC
  "text_literal": "",                       // fallback si text_key falta — escape hatch
  "portrait": "characters/guard.png",       // path opcional (sobrescribe default)
  "audio": "audio/voice/npc_intro_1.ogg",   // path opcional
  "animation": "talk_idle",                 // animation id opcional (NPC.Animator)
  "choices": [
    {
      "label_key": "dialog.intro_npc.choice_1",
      "condition_lua": "",   // Lua expr que retorna bool ("" = siempre disponible)
      "on_select_lua": ""    // Lua hook al elegir esta opcion ("" = no-op)
    }
  ]
}
```

**Sockets del nodo**:
- 1 socket `Input` de typeTag `"flow"` (donde entra el flow del nodo anterior).
- **N sockets `Output` `flow`** — uno por cada `choice` en `customData.choices`.
  El orden de sockets matchea el orden del array `choices`.
- Si `choices` está vacío: 1 socket `Output` `flow` "default next" — al
  agotar las líneas el dialogo continúa sin esperar input del jugador.

**Validación**: el editor garantiza que el nodo SIEMPRE tenga al menos
1 output socket flow. Si el dev borra todas las choices del array sin
agregar un fallback, el editor mantiene 1 output "Continue".

### Asset lifecycle

- **Path en el filesystem**: `<project_root>/assets/dialogs/<name>.mooddialog`.
- **AssetManager** gana un loader nuevo: `DialogAsset` cargado vía
  `am.loadDialog("dialogs/intro_npc_guard.mooddialog")` (path lógico
  via VFS, mismo patrón que `loadMaterial`/`loadAudio`/etc).
- **Save**: el Editor escribe directo al filesystem (no via packaging).
  Auto-save on edit OFF en v1 — botón "Save" explícito en el panel
  (igual que Script Editor del Hito 28 F).

### Panels nuevos

#### `DialogBrowserPanel` (categoría Assets)

Lista todos los `.mooddialog` del proyecto activo + acciones:
- `+ New Dialog` (modal con name input → crea archivo vacío con 1
  nodo dialog_line de placeholder).
- Click sobre un dialog → abre en el `DialogEditorPanel`.
- Right-click sobre item → rename / delete.
- Refresh automático cuando se guarda un dialog (notifica desde Editor).

**Layout default en workspace "Narrativa"**: bottom-left, tab al lado
del NarrativeIntroPanel (que se queda como referencia para devs nuevos).

#### `DialogEditorPanel` (categoría Narrative)

Edita UN dialog específico. Reusa `NodeGraphEditor` (wrapper de F2H46)
para el canvas; agrega:
- **Toolbar arriba**: Save / Save As / Close / Set Start Node /
  Validate (chequea que no haya ciclos infinitos + nodos huérfanos).
- **Canvas central**: NodeGraphEditor sobre el `Graph` del dialog.
- **Status bar abajo**: nodos / links / start_node_id activo / file
  path activo.

**Layout default en workspace "Narrativa"**: center, donde estaba el
Sandbox. El Sandbox queda accesible vía menú Ver > Debug para
inspección del framework subyacente, pero NO es default del workspace
post-F2H47.

#### `DialogNodeInspectorPanel` (categoría Narrative)

Inspector contextual que se actualiza con la selección del
`DialogEditorPanel`. Cuando el dev clickea un nodo del grafo, este
panel muestra los campos editables del `customData` (text_key,
portrait, audio, animation, choices con add/remove buttons).

**Layout default en workspace "Narrativa"**: right column.

### `engine/dialog/` modulo nuevo

- `engine/dialog/DialogAsset.h/cpp`: struct con `name`, `Graph`,
  `metadata`, `customData` typed accessors (parsea customData JSON →
  `DialogLine struct` por nodo on demand, cached). Serialización
  `toJson()` / `fromJson()`. NO depende de ImGui — testeable.
- `engine/dialog/DialogValidator.h/cpp`: utilities puras para validar
  un Graph como `.mooddialog` válido (start_node existe, no cycles,
  customData parseable, sockets coherentes con choices array).

### Tests (Bloque G)

- `tests/test_dialog_asset.cpp`: schema roundtrip, customData parsing,
  start_node validation, bumped-version rejection, missing-field
  defaults.
- `tests/test_dialog_validator.cpp`: detecta cycles, nodos sin output
  socket, choices sin label_key, start_node_id huérfano,
  socket-count vs choices-array mismatch.

### Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 30 min | Aprobar con dev antes de B |
| B | `engine/dialog/DialogAsset.h/cpp` + DialogValidator | ~2h | Schema struct + JSON serialization + validation utilities |
| C | `AssetManager::loadDialog` + path resolution VFS | ~1h | Mismo patrón que loadMaterial F2H42 |
| D | `DialogBrowserPanel` — lista + new/rename/delete | ~1.5h | Asset browser del proyecto activo |
| E | `DialogEditorPanel` — reusa NodeGraphEditor + toolbar + multi-output sockets segun choices | ~2.5h | Inicializa Graph con sockets correctos por dialog_line; mantiene sync sockets↔choices array |
| F | `DialogNodeInspectorPanel` — campos editables del customData + add/remove choices | ~2h | Inspector contextual; choices como subform repetible |
| G | Tests del asset + validator | ~1.5h | `test_dialog_asset.cpp` + `test_dialog_validator.cpp` |
| H | Integración workspace "Narrativa" + dock layout actualizado (Editor center + Inspector right + Browser tab con Intro) | ~30 min | Update buildNarrativeWorkspace en Dockspace.cpp |
| I | Sample dialog `.mooddialog` de demo (3-4 nodos pre-armados) | ~30 min | Carga via Welcome modal botón "Cargar demo de diálogo" (igual que F2H44 demo Fox) |
| J | Cierre: validación visual + docs + commit + tag + push | ~1h | Patrón estándar |

**Total estimado**: ~12h. Hito mediano-largo.

## Decisiones técnicas abiertas

### Edición de `text_key` vs `text_literal`

¿El campo de texto en el Inspector contextual es:
- (a) **i18n key con dropdown** de keys existentes en `assets/i18n/es.json`?
- (b) **Texto libre** (literal) que el dev escribe inline?
- (c) **Toggle entre ambos** ("usa key" / "usa literal")?

**Recomendación v1**: (c) — toggle. La práctica de muchos juegos es
escribir el texto inline durante prototyping y promover a i18n key
después. v2 podría agregar el dropdown de keys existentes.

### Conexión socket↔choice array

Al editar un nodo en el Inspector contextual y agregar/borrar choices,
el grafo subyacente debe actualizar los sockets output del nodo. Hay
dos opciones:
- (a) **Auto-sync**: cuando el Inspector modifica `choices`, regenera
  los sockets output del Node y dispara un event para el editor.
- (b) **Manual**: dev edita choices Y manualmente agrega/remueve
  sockets del nodo en el grafo.

**Recomendación v1**: (a) auto-sync. (b) es propenso a desync errors
(choices con N elementos pero N+1 sockets visualmente). Auto-sync
preserva la invariante "1 socket por choice".

### Validación de cycles

¿Bloquear el save si el grafo tiene ciclos infinitos sin condition Lua
que pueda romperlos? Algunos juegos (puzzles narrativos) USAN ciclos
controlados via flags. Pero ciclos no-controlados son bug.

**Recomendación v1**: warn (no bloquear). El validator detecta cycles
y los lista en un panel; el dev decide si guardar igual. v2 puede
agregar análisis de "este cycle es escapable via condition X" si
emerge necesidad.

### Auto-save vs Save explícito

**Recomendación v1**: Save explícito (botón en toolbar). Auto-save es
feature deseable pero requiere debouncing + handling de "save en medio
de un edit complicado" → mejor v2.

## Diferidos a hitos futuros

- **F2H48 — Dialog runtime**: DialogSystem (state machine que recorre
  el árbol según el flow), HUD widget default (caja inferior estilo
  HL2, override-able vía Lua), Lua bindings (`dialog.start`,
  `set_var`, `get_var`, `on_node_enter`, `on_choice`), `DialogComponent`
  para entidades, tests de integración runtime.
- **Tipo de nodo `condition`** (nodo de branching puro basado en una
  expresión Lua, sin texto): emergente cuando un dialog real lo pida.
  v1 cubre el caso con `condition_lua` en cada choice.
- **Tipo de nodo `action`** (nodo que ejecuta un hook Lua sin
  presentar texto al jugador, ej. "set flag X"): emergente cuando
  emerja el caso. v1 cubre con `on_select_lua` en cada choice.
- **Tipo de nodo `jump`** (saltar a otro `.mooddialog` por
  composición): scope grande, requiere lifecycle de "dialog within
  dialog". Diferido.
- **Localización de strings de gameplay** (extender i18n F2H43 a
  dialog text): tratable hoy via `text_key` + JSON i18n existente,
  pero un sistema dedicado de string tables por contexto podría ser
  más práctico cuando los diálogos crezcan en cantidad. Diferido.
- **Voiceover sync** (highlight de palabras según timing del audio):
  feature de pulido, diferido a hito propio si emerge.
- **Animation per-line con waitFor** (esperar a que la animation
  termine antes de avanzar): customData ya tiene campo `animation`
  pero v1 sólo se reproduce, no se espera. v2 puede agregar flag
  `wait_for_animation`.
- **Theming custom del grafo** (paleta narrativa): herencia de F2H46
  diferido — sigue sin urgencia hasta que el grafo se vuelva confuso
  visualmente.

## Riesgos y tradeoffs

- **Riesgo: Inspector contextual con campos dinámicos según choices**.
  Los nodos `dialog_line` con N choices muestran N subforms; agregar/
  borrar requiere mantener consistencia del array + sockets del Node.
  **Mitigación**: auto-sync explícito + tests de la lógica add/remove
  en el data model.

- **Riesgo: i18n keys vs literal text confusion para el dev**.
  Si el flujo no es claro el dev puede mezclar inadvertidamente keys
  y literales. **Mitigación**: el toggle del Inspector tiene labels
  claros + warning visual si una key apunta a una traducción
  inexistente.

- **Tradeoff: schema v1 sin tipo `condition`/`action`/`jump`**. Casos
  de juego complejos requerirían tipos dedicados. Aceptable: el `choice`
  con `condition_lua` + `on_select_lua` cubre el 80% de los casos
  prácticos. Agregar tipos cuando emerja necesidad real.

- **Tradeoff: cycles permitidos con warning**. Algún dev puede crear
  un cycle accidental y romper su Dialog en runtime. Aceptable: warn
  es claro + F2H48 runtime puede aplicar cycle-break con max-iterations
  como safety.

- **Tradeoff: Save explícito vs Auto-save**. Algunos devs (yo) prefieren
  auto-save; otros prefieren control explícito. v1 con explicit es más
  seguro (no overwrite accidental durante experimentación).

## Validación

- Build limpio Debug + Release.
- Suite completa (tests previos + nuevos de DialogAsset + DialogValidator) verde.
- **Tour visual con dev en workspace "Narrativa"** (cada paso debe
  funcionar de punta a punta):
  1. Welcome modal → "Cargar demo de diálogo" → editor carga el
     `.mooddialog` sample con 3-4 nodos pre-armados.
  2. Click en un nodo del grafo → DialogNodeInspector aparece a la
     derecha con campos del customData editables.
  3. Editar `text_literal` → guardar → cerrar dialog → re-abrir →
     cambios persisten en el JSON.
  4. Agregar choice nuevo en el Inspector → socket output nuevo
     aparece en el nodo automáticamente.
  5. DialogBrowserPanel lista todos los `.mooddialog` del proyecto;
     "+ New Dialog" crea un archivo nuevo con 1 nodo placeholder.
  6. Validate button del toolbar detecta cycles o nodos huérfanos si
     los hay; muestra warning visible.

## NO entra en F2H47

- **Runtime del dialog** (state machine + HUD + Lua + DialogComponent
  + integración con Play Mode + tests de integración runtime).
  Todo eso es F2H48.
- Tipos de nodo `condition`, `action`, `jump` — solo `dialog_line`.
- Voiceover sync / waveform UI.
- Animation `wait_for` flag.
- Theming custom del grafo a paleta narrativa.
- String tables dedicadas para gameplay (vs reusar i18n actual).
- DialogComponent del lado entidad (eso es del runtime).
- Localización inline (todo via `text_key` del i18n actual).

---

## Bloques de ejecución resumidos para el commit (cuando llegue a Bloque J)

| Bloque | Entregable |
|--------|-----------|
| A | Plan (este doc) — archivado en cierre |
| B | `DialogAsset.h/cpp` + `DialogValidator.h/cpp` (puros, sin ImGui) |
| C | AssetManager::loadDialog + VFS path resolution |
| D | `DialogBrowserPanel` (categoría Assets) |
| E | `DialogEditorPanel` (categoría Narrative, reusa NodeGraphEditor) |
| F | `DialogNodeInspectorPanel` (categoría Narrative, contextual) |
| G | Tests del asset + validator |
| H | Update workspace "Narrativa" dock layout (Editor center / Inspector right / Browser+Intro bottom) |
| I | Sample `.mooddialog` demo + botón Welcome modal |
| J | Cierre: docs + commit + tag + push |
