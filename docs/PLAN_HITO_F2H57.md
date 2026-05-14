# PLAN HITO F2H57 — Workflow de creación de entidades (Hammer-style) + remove Demos + fix bug SIDE ortho

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-14).
> **Tag previsto**: `v1.44.0-fase2-hito57`.
> **Origen**: durante el tour visual de F2H56 el dev detectó 3 problemas UX del editor:
> 1. No hay un botón claro para crear entidades — el workflow actual obliga a spawn vía Demos del menú Ayuda, que son muleta. El dev pidió un workflow estilo **Hammer Editor (Source Engine)**: botón "Create Entity" → elegir tipo + importar modelo opcional → editar propiedades en Inspector.
> 2. La vista ortográfica **SIDE (ZY) dibuja brushes con eje invertido** — arrastrar izquierda-a-derecha mapea al revés.
> 3. Los **Demos** del menú Ayuda son muletas — una vez exista el workflow real de creación de entidades, eliminarlos para que el editor se comporte como un editor decente.
>
> F2H57 es un **pivot temporal de la Sub-fase 2.6 (render polish) a UX del editor**. Después de cerrar F2H57, retomamos el plan original con F2H58 (color grading) y F2H59 (god rays).

---

## 🎮 Preguntas para el dev (en mecánicas — leer esto primero)

Antes de tocar código, confirmá / ajustá estas 4 cosas:

### 1. ¿Dónde aparece el botón "Create Entity"?

Hammer tiene el botón en el toolbar lateral. Algunas opciones:

- **A. En el panel "Escena"** (lo que antes se llamaba Hierarchy): un botón `+ Crear Entidad` arriba de la lista de entidades. Pros: lugar natural — donde ves la lista de entidades es donde las creás. Contras: hay que abrir el panel para usarlo.
- **B. En la top-toolbar del workspace activo**: un botón siempre visible junto a los demás. Pros: discoverable sin abrir paneles. Contras: la top-toolbar puede llenarse de botones.
- **C. En el menú Mapa**: `Mapa > Crear Entidad…`. Pros: convención de menús. Contras: menos discoverable que un botón.
- **D. Las tres cosas**: botón en Escena + botón en toolbar + entry en menú.

**Propuesta: A + C** (botón en panel Escena + entry en menú Mapa). Cubre discoverability + UX clásico.

### 2. ¿Cómo elegís el tipo de entidad?

Al hacer click en "Create Entity", se abre un popup. ¿Qué tipos ofrecemos al primer corte?

Propuesta de tipos:
- **Vacía** — entidad solo con Transform. El dev agrega componentes después.
- **Mesh** — entidad con Transform + MeshRenderer. Te pide elegir un .fbx/.obj/.gltf del proyecto (o importarlo en el momento).
- **Luz direccional** (sol).
- **Luz puntual** (point light).
- **Cámara** — entidad cámara editable.
- **Player** — entidad con CharacterController + Camera child (kit del MoodPlayer).
- **NPC con diálogo** — entidad con Trigger + DialogComponent.
- **Item pickeable** — entidad con ItemPickupComponent (F2H52).
- **Quest runner** — entidad vacía + ScriptComponent placeholder para correr quests.

**Propuesta**: los 9 tipos arriba, con un icono cada uno. Más de eso es overload — el dev siempre puede crear "Vacía" y armar componentes a mano.

### 3. ¿Importar modelo "en el momento" o solo elegir del proyecto?

Hammer permite agregar modelos al momento de crear la entidad (te abre un file picker). Otra opción: el dev primero importa el .fbx al asset browser, después crea una entidad Mesh y elige el asset ya cargado.

- **Opción A — Solo elegir del proyecto**: el dev importa los assets primero (panel Asset Browser), después crea entidades reusando esos assets. Workflow Unity/Unreal.
- **Opción B — Importar al momento**: el botón "Mesh" abre file picker del SO, el .fbx se importa Y se agrega a la nueva entidad en un paso. Workflow Hammer.
- **Opción C — Ambas**: dropdown con "Elegir del proyecto…" o "Importar nuevo…".

**Propuesta: C** — flexibilidad sin costo, el dev elige según contexto.

### 4. ¿Qué pasa con los Demos del menú Ayuda?

Hoy hay varios demos (cubo PBR, escena narrativa, stress test, etc.). El dev quiere eliminarlos.

- **Opción A — Eliminar todos los Demos del menú Ayuda y los handlers**.
- **Opción B — Eliminar los Demos de "scene poblada" pero MANTENER los que crean un solo asset útil** (cubo PBR de testing, etc.).
- **Opción C — Mover los Demos a "Plantillas de escena"**: en el popup Create Entity, agregar al final un dropdown "Plantillas" con las escenas demo como punto de partida (player solo, NPC solo, etc.). Útil si el dev quiere arrancar de algo más que vacío.

**Propuesta: A** (eliminar todo). Si más adelante emerge demanda de "plantilla rápida", reintroducir como opción del popup Create Entity.

### Mecánicas que F2H57 entrega (resumen)

1. Botón "+ Crear Entidad" arriba del panel Escena.
2. Click → popup con 9 tipos de entidad. Elegís uno + nombre + (opcional) modelo.
3. Entidad spawnea en el viewport en posición central de la cámara.
4. Inspector queda focalizado en la nueva entidad — editás propiedades de cada componente.
5. Bug SIDE ortho fixeado: arrastrar izquierda-a-derecha en la vista lateral mapea correctamente.
6. Menú **Ayuda > Demos** eliminado. Para arrancar una escena nueva, usás el botón "+ Crear Entidad" como en Hammer / Unity / Unreal.

Cuando confirmes las 4 preguntas, arranco con el Bloque A.

---

## Bloques de ejecución (DRAFT — refinable post-confirmación)

### Bloque A — Fix bug SIDE ortho (reactive fix, pre-requisito chico)

Investigación + fix puntual en `OrthoViewportPanel.cpp`. Probable causa: signo invertido en la conversión screen-coord → world-coord del eje Z. ~1h.

### Bloque B — Popup "Create Entity" + 9 tipos hardcoded

Nuevo modal ImGui `CreateEntityPopup.{h,cpp}` con grid de tipos + input de nombre + dropdown "Modelo (opcional)" para tipo Mesh. Spawna entidad en `Scene` con los componentes apropiados. ~3h.

### Bloque C — Botón "+ Crear Entidad" en panel Escena + entry en menú Mapa

`HierarchyPanel.cpp` (o como se llame ahora — "Escena") gana un botón arriba que dispara el popup. `MenuBar.cpp` gana entry `Mapa > Crear Entidad…`. ~1h.

### Bloque D — Eliminar Demos del menú Ayuda

Remove menu entries + handlers de `DemoSpawners_*.cpp` (Basic / Drop / Stress). Cleanup de helpers asociados (`ensureDemoIntroDialogExists`, etc.). Suite tests si alguno depende — actualizar. ~1h.

### Bloque E — Validación visual

Dev abre proyecto vacío → click + Crear Entidad → elige Mesh → spawna cubo. Inspector edita. Bug SIDE confirmado fixeado. Menú Ayuda sin Demos. ~10 min.

### Bloque F — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag. ~30 min.

---

## Total estimado

**6 bloques A-F, ~6-7h**. Hito mediano UX.

---

## Criterios de aceptación

1. Proyecto nuevo + click "+ Crear Entidad" + elegir Mesh + cargar .fbx → cubo spawnea visible en viewport.
2. Crear entidad de cada uno de los 9 tipos sin crash.
3. Vista SIDE ortho dibuja brushes en la dirección correcta (arrastrar izq-der genera brush en world X+ correctamente).
4. Menú Ayuda sin Demos.
5. Crear/eliminar entidades múltiples sin leak / regresión.
6. Tour visual del dev: *"se siente como un editor"* o equivalente.

---

## NO entra en F2H57

- **F2H58**: Color grading LUT-based (continuación del plan render polish).
- **F2H59**: Rayos volumétricos / god rays.
- **F2H6X (backlog)**: Shadow polish (CSM, contact shadows, soft shadows PCF).
- Drag-and-drop de modelos desde explorador del SO al viewport (workflow Unreal, separado del popup Create Entity — futuro).
- Templates compuestos de escena ("escena narrativa completa", "escena combate", etc.) — diferido a Sub-fase 3 si emerge demanda.
- Plugin system para que devs externos registren tipos de entidad custom — Sub-fase 3+.

---

## Riesgos identificados

- **Refactor de DemoSpawners**: hay tests que probablemente referencien funciones de DemoSpawners (`processGenerateNarrativeDemoMapRequest`, etc.). Mapping pre-eliminación: identificar qué tests rompen, ajustar o eliminarlos.
- **Player spawn con CharacterController**: requiere todos los pre-requisitos (Jolt body + Camera child + Controls). Reusar el código de DemoSpawners_Basic antes de borrarlo. Mover a helper compartido.
- **Modelo en el momento (Opción B/C de pregunta 3)**: file picker del SO + import .fbx es un flujo que ya existe en AssetBrowser. Reusar el handler — no escribir nuevo.
