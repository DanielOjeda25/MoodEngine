# PLAN HITO F2H57 — Workflow "Crear + Convertir entidad" estilo Hammer + fix bug SIDE ortho + remove Demos

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-14).
> **Tag previsto**: `v1.44.0-fase2-hito57`.
> **Origen**: durante el tour visual de F2H56 el dev pidió un workflow estilo Hammer Editor para crear entidades + fixear el bug del SIDE ortho + eliminar los Demos del menú Ayuda.
>
> F2H57 es un **pivot temporal de la Sub-fase 2.6 (render polish) a UX del editor**. Después de cerrar F2H57, retomamos el plan con F2H58 (color grading) y F2H59 (god rays).

---

## 🎮 Mecánicas del flujo (lo que vivís como dev)

El workflow Hammer Editor en mecánicas, ajustado al feedback del 2026-05-14:

### Paso 1 — Crear

Hay un botón **"+ Crear Entidad"** arriba del panel Escena (también accesible desde menú `Mapa > Crear Entidad…`). Click → se abre el **file picker del SO** → elegís un modelo (`.fbx`, `.obj`, `.gltf`) → spawna en el viewport una entidad-modelo con:
- `TransformComponent` posicionada al centro de la cámara.
- `MeshRenderer` apuntando al modelo elegido (importado al toque al asset browser si no está ya cargado).
- Nombre derivado del filename (`npc.fbx` → entidad "npc").

**Al spawnear ya queda seleccionada** + Inspector enfocado en ella. El dev la posiciona con el gizmo a gusto.

### Paso 2 — Convertir (transformar en otra cosa)

**Click derecho sobre la entidad** (en el panel Escena O en el viewport directamente, ambos paths) → menú contextual con opciones:
- **Cambiar tipo de entidad…** → abre modal "Convertir entidad".
- Renombrar.
- Duplicar.
- Eliminar (ya existía).
- (eventualmente otros: Move to layer, etc.)

El **modal "Convertir entidad"** te muestra una lista de "kits" (presets de componentes) que podés aplicar a la entidad:

- **Vacía (sólo Transform)** — vuelve a la entidad básica, quita componentes extra.
- **Modelo decorativo** — Transform + MeshRenderer (default al crear).
- **NPC con diálogo** — agrega `TriggerComponent` + `DialogComponent` (te pide elegir un `.mooddialog`).
- **Player** — Transform + Mesh + `CharacterController` + Camera child (kit de F2H30/F2H31).
- **Item pickeable** — agrega `ItemPickupComponent` (F2H52) — te pide elegir un `.mooditem`.
- **Quest runner** — agrega `ScriptComponent` apuntando a un .lua placeholder con `quest.start(...)`.
- **Cámara** — convierte a entidad cámara.
- **Luz puntual** — agrega `LightComponent` tipo Point.
- **Luz direccional** — agrega `LightComponent` tipo Directional + `casts shadows = on` por default.

El modal te muestra **qué componentes va a agregar / quitar** antes de confirmar — el dev decide. Click "Aplicar" → la entidad muta + se guarda en la escena dirty. **Undo del cambio queda disponible** vía Ctrl+Z (reaprovechando `HistoryStack` F2H27/32).

### Paso 3 — Bugs reactivos + cleanup

- **Bug SIDE ortho**: arrastrar izquierda-a-derecha en la vista lateral (ZY) dibuja brushes con eje invertido. Fix puntual en `OrthoViewportPanel.cpp`.
- **Eliminar Demos del menú Ayuda**: una vez existe el workflow real, los Demos son muleta innecesaria. Borrar entries + handlers + helpers asociados. Mantener los assets útiles que generaban (`assets/dialogs/demo_intro.mooddialog`, etc.) hasta que el dev confirme que no los usa.

---

## 🎯 Preguntas concretas para confirmar antes de arrancar

### Pregunta 1 — ¿El file picker arranca de "Assets del proyecto" o del filesystem del SO?

- **A. Filesystem del SO** (Hammer-style): el file picker abre el SO normal — el dev navega a `npc.fbx` en cualquier carpeta de su PC, el motor lo importa automáticamente a `assets/meshes/` del proyecto si no está ya. Workflow rápido para drop de assets nuevos.
- **B. Asset Browser del proyecto** (Unity-style): popup que muestra solo los modelos ya importados al proyecto. Si el dev quiere un modelo nuevo, primero lo importa al Asset Browser.

**Propuesta: A** — pega con el workflow Hammer que mencionaste. El usuario espera que "Crear Entidad" sea un atajo desde cualquier filename.

### Pregunta 2 — Click derecho funciona en el panel Escena Y en el viewport, o solo en uno?

**Propuesta: ambos** — discoverability. Mismo menú contextual; ambos disparan el modal Convertir.

### Pregunta 3 — Si la entidad ya tiene componentes (ej. ya era NPC con DialogComponent), ¿el modal los preserva o los reemplaza?

Hay 2 modos posibles:
- **A. Aditivo**: convertir "Modelo decorativo" → "NPC con diálogo" agrega Trigger+Dialog sin tocar Transform/Mesh. Convertir "NPC" → "Vacía" quita TODO menos Transform.
- **B. Destructivo confirmando**: el modal muestra "Te voy a sacar X / Y / Z componentes — ¿confirmás?" cada vez.

**Propuesta: A con preview** — al elegir un kit, el modal te muestra qué componentes quedan (verde = se mantiene, amarillo = se agrega, rojo = se quita). Click Aplicar ejecuta. Más predecible que un confirm.

### Pregunta 4 — Eliminar Demos: ¿todos o algunos?

- **A. Todos**: borrar el menú "Ayuda > Demos" entero + todos los `DemoSpawners_*.cpp`.
- **B. Mantener el demo narrativo** porque tiene assets útiles (NPC + diálogo de ejemplo) — pero moverlo a "Plantillas de escena" si emerge.

**Propuesta: A** — eliminar todo. Los assets que los demos creaban quedan en el proyecto (`assets/dialogs/demo_intro.mooddialog` etc.) — no se borran. Solo eliminamos el código que los regeneraba on-demand. Si el dev quiere "una escena narrativa de arranque" después, lo agregamos como template en el modal Convertir.

---

## Filosofía

**El workflow del editor debe ser predecible para alguien que viene de Hammer / Unity / Unreal.** El dev no está aprendiendo MoodEditor en abstracto — está extendiendo su mental model de editores que ya conoce. Apartarse de las convenciones (right-click → properties, file picker para drop rápido) cuesta fricción.

**Engine-grade preservado**: los "kits" del modal Convertir NO son tipos de entidad fijos del motor — son **presets de componentes** del editor. El motor sigue tratando entidades como bolsas de componentes; el editor le agrega azúcar UX para que el dev no arme NPCs componente-por-componente.

---

## Bloques de ejecución (DRAFT — refinable post-confirmación de las 4 preguntas)

### Bloque A — Fix bug SIDE ortho (pre-requisito chico, reactive fix)

Investigación + fix puntual en `OrthoViewportPanel.cpp`. Probable causa: signo invertido en la conversión screen-coord → world-coord del eje Z en la vista lateral. Repro con tour antes de fixear para confirmar. ~1h.

### Bloque B — "+ Crear Entidad" button + file picker → entidad-modelo

- Botón nuevo en `HierarchyPanel.cpp` (panel "Escena") arriba de la lista de entidades.
- Menú `Mapa > Crear Entidad…` entry en `MenuBar.cpp`.
- Click → abrir file picker del SO (reuso de `portable_file_dialogs`, ya en deps via CPM).
- Importar el modelo al `AssetManager::loadMesh` si no está cargado.
- Spawnar entidad nueva con Transform + MeshRenderer.
- Seleccionar la nueva entidad + focalizar Inspector.

~2-3h.

### Bloque C — Menú contextual right-click (panel Escena + viewport)

- `HierarchyPanel.cpp`: agregar `ImGui::BeginPopupContextItem` por entidad.
- Viewport: detectar click derecho sobre entidad seleccionable (ya hay raycast del gizmo F2H13 + tooltip handler — reusar).
- Menu items: Cambiar tipo… / Renombrar / Duplicar / Eliminar.
- Renombrar / Duplicar / Eliminar: reusan comandos existentes (`CreateEntityCommand`, `DeleteEntityCommand` de F2H27/32).

~2h.

### Bloque D — Modal "Convertir entidad"

- Nuevo `ConvertEntityModal.{h,cpp}` (popup ImGui).
- Lista de 9 kits con icono + descripción corta + lista de componentes que aplica.
- Preview de qué componentes se mantienen / agregan / quitan al elegir un kit.
- File picker secundario para los kits que lo necesitan (.mooddialog para NPC, .mooditem para Item).
- Click Aplicar: ejecuta un `ConvertEntityCommand` nuevo en el HistoryStack (undoable).

~3-4h.

### Bloque E — Eliminar Demos del menú Ayuda

- Remove menu entries en `MenuBar.cpp` (sección Ayuda > Demos).
- Remove handlers en `EditorApplication_Run.cpp` (consumers de los Request flags).
- Remove `DemoSpawners_Basic.cpp`, `DemoSpawners_Drop.cpp`, `DemoSpawners_Stress.cpp`.
- Cleanup de helpers (`ensureDemoIntroDialogExists`, etc.) — si algún kit del modal Convertir los reusaba, mover a helper compartido.
- Tests: si algún test del editor referencia los Demos, ajustar / eliminar.

~1h.

### Bloque F — Validación visual

Dev abre proyecto vacío → `+ Crear Entidad` → elige `npc.fbx` → spawna → Inspector. Click derecho sobre la entidad → Cambiar tipo → "NPC con diálogo" → elige `.mooddialog` → entidad muta. Play → la entidad funciona como NPC. Vista SIDE ortho dibuja correctamente. Menú Ayuda sin Demos. ~15 min.

### Bloque G — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag. ~30 min.

---

## Total estimado

**7 bloques A-G, ~10-12h**. Hito mediano-grande UX (más grande que F2H56 porque toca múltiples paneles + introduce el modal y los kits).

---

## Criterios de aceptación

1. Proyecto nuevo + `+ Crear Entidad` + elegir `.fbx` → cubo (o lo que sea el modelo) spawnea visible en viewport, queda seleccionado.
2. Click derecho sobre entidad (en panel Escena Y en viewport) → menú contextual aparece.
3. Modal Convertir entidad muestra los 9 kits + preview de cambios + aplica con undo.
4. Vista SIDE ortho dibuja brushes en la dirección correcta.
5. Menú Ayuda sin Demos.
6. Tour visual del dev: *"se siente como un editor de verdad"* o equivalente.

---

## NO entra en F2H57

- **F2H58**: Color grading LUT-based (continuación render polish, próximo hito tras F2H57).
- **F2H59**: Rayos volumétricos / god rays.
- **F2H6X (backlog)**: Shadow polish (CSM, contact shadows).
- Drag-and-drop desde explorador del SO al viewport (workflow Unreal — futuro, complementa el botón).
- Plugin system para que devs externos registren tipos de entidad / kits custom — Sub-fase 3+.
- Templates compuestos de escena ("escena narrativa completa") — diferido. Si emerge demanda, agregar como kit del modal Convertir.

---

## Riesgos identificados

- **`ConvertEntityCommand` undoable**: convertir una entidad muta varios componentes. Captura del state pre/post requiere snapshot de la entidad completa para roundtrip. Reusable: ya hay `EntitySerializer::serialize` que captura todos los componentes → el comando puede guardar el JSON pre y restaurarlo en undo.
- **DemoSpawners removal — tests dependientes**: probable que tests del editor que validan "spawn de escena" referencian los handlers. Mapping inicial: identificar tests con `grep "processGenerateNarrativeDemoMapRequest\|StressScene\|spawnFullStressScene"` → ajustar o eliminar.
- **File picker bloqueante**: portable_file_dialogs es modal sync (bloquea el thread main). Si el dev cierra el picker sin elegir, manejar el caso (no spawn entidad). Patrón ya usado en F2H8 Save Map As y F2H38 Save Game.
- **Right-click en viewport conflict con gizmo**: el gizmo de F2H13 ya consume click derecho en algunos modos (rotate gizmo, etc.). Investigar el handler — probable que solo consume click derecho con drag, no con click puro.
