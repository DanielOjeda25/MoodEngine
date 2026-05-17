# PLAN HITO F2H60 — Source paradigm (Brushes = estructura, Meshes = objetos) + UX polish acumulado

> **Estado**: DRAFT pendiente arranque (2026-05-16).
> **Tag previsto**: `v1.47.0-fase2-hito60`.
> **Origen**: durante el tour visual de F2H59 (primitivas + reorg UX) el dev detectó que el cubo brush con RigidBody Dynamic no caía visualmente. Tras explicación del Source/Hammer paradigm — *"sigamos esa direccion"*. F2H60 adopta explícitamente la separación: brushes son estructura estática del mapa (estilo Hammer); meshes (procedurales o importados) son los objetos del juego con física dinámica (estilo Half-Life 2 props).

---

## 🎮 Mecánicas (lo que vivís como dev)

### Antes de F2H60

Las primitivas del modal "+ Crear Entidad" → tab "Primitivas" spawnean **brushes** (Plano / Quad / Box / Cylinder / Sphere / Cone / Capsule / Pyramid / Wedge / Prism). Si querés que cualquiera de estos sea dinámico (caiga con gravedad, rebote, sea empujable), agregás un `RigidBodyComponent` Dynamic desde el Inspector... pero **el visual no se mueve** porque el mesh cache del brush se construye con `worldMatrix` incrustado en los vértices.

### Después de F2H60

La tab "Primitivas" se divide en 2 sub-secciones que reflejan la intención del dev:

- **Estructura** (los 11 brushes actuales) → paredes / pisos / columnas / escaleras. **Estática** por diseño — el Inspector deshabilita el modo Dynamic del RigidBody con tooltip explicativo. CSG ops siguen disponibles (boolean / vertex / face edit).
- **Objetos** (5-6 meshes procedurales) → cubos / esferas / cilindros / cápsulas / conos como **meshes puros** generados en runtime. Soportan física dinámica nativamente. Físicas, animación y ragdoll van acá.

El dev elige según intención: ¿es estructura del mapa o es un objeto del juego?

### Convención industria

Source/Hammer (HL2, 2004) trazó esta línea con Havok Physics:
- Brushes (BSP world) = siempre estáticos.
- `prop_static` / `prop_dynamic` / `prop_physics` / `prop_ragdoll` = meshes con física.

MoodEngine F2H60 adopta esta separación con UI explícita (sub-secciones nombradas en lugar del paradigma binario implícito de Source).

---

## 🧱 Bloques de ejecución

### Bloque A — Rename UI "Brush" → "Estructura" / "Geometría del nivel"

- **Donde aplica:** título del menú top-level (`editor.menu.brush` → `editor.menu.structure`), labels en el modal Crear Entidad ("Primitivas" → mantener), nombres de log (`"Anadir Box Brush:"` → `"Anadir Box (estructura):"`), tooltip del Inspector, panel "Brush Operations" → "Operaciones de estructura".
- **Internamente NO cambia nada:** `BrushComponent`, `Csg::Brush`, `makeBoxBrush` siguen como están. Es solo i18n + algunas strings hardcoded.
- **i18n nuevas** (es.json + en.json): ~10 keys.

**Estimado:** 1h. Puramente cosmético.

**Criterio de aceptación:** el menú dice "Estructura > Operaciones Booleanas" en lugar de "Brush > Operaciones Booleanas". El dev no se confunde con la palabra "brush".

---

### Bloque B — Meshes procedurales en runtime para "Objetos"

Generar 5-6 meshes procedurales con vertices/normales/UVs puros:
- **Cubo** (8 vertices, 12 triangles, UVs per-face).
- **Esfera** UV-sphere (32 sectors × 16 stacks default, configurable).
- **Cilindro** (N segmentos, default 16).
- **Cápsula** (cilindro + 2 hemisferios — paridad con Cápsula brush pero como mesh real).
- **Cono** (N caras laterales + cap base).

Implementación en `engine/render/resources/ProceduralMeshes.{h,cpp}` (nuevo). Cada `make*Mesh()` devuelve un `MeshAsset` con `vertices`, `indices`, `normals`, `uvs`, `aabbMin`, `aabbMax`. Registrar el mesh en `AssetManager` con path lógico `"__primitive_cube"`, `"__primitive_sphere"`, etc. (prefix `__` ya usado para skip en el picker SFM).

**Estimado:** 4-5h. Bloque grande pero patrón conocido (similar a CSG mesh build).

**Criterio de aceptación:** desde la tab "Primitivas > Objetos", click en Cubo spawnea una entidad con `MeshRendererComponent(primitiveCubeId)` + Transform default. Se ve idéntico al cubo brush visualmente. Tests unitarios validan: 8 vertices, 12 triangles, AABB (-0.5..+0.5).

---

### Bloque C — Modal Crear Entidad → sub-secciones "Estructura" + "Objetos"

Refactor del tab "Primitivas":
- Header colapsable "Estructura (brushes)" con los 11 brushes actuales — grid 3-col.
- Header colapsable "Objetos (meshes)" con los 5-6 meshes procedurales — grid 3-col.
- Default ambos headers abiertos.

**i18n nuevas:** `editor.pick_mesh_modal.section_structure`, `editor.pick_mesh_modal.section_objects` + descripciones.

Cada botón de "Objetos" dispara un nuevo `ProjectAction` (`AddProceduralCubeMesh`, etc.) que spawnea entidad con `MeshRendererComponent(primitiveId)`.

**Estimado:** 2h. Refactor UI + nuevos handlers.

**Criterio de aceptación:** el dev abre el modal, ve las 2 sub-secciones, elige según intención. La división conceptual es clara sin tooltip extra.

---

### Bloque D — Warning en Inspector: RigidBody Dynamic deshabilitado sobre Brush

En `InspectorPanel_Physics.cpp::renderRigidBodySection`:
- Si la entidad tiene `BrushComponent`, el combo del `RigidBodyComponent::Type` muestra solo Static / Kinematic. La opción Dynamic queda disabled con tooltip:
  - ES: "Las estructuras son estáticas por diseño. Para un objeto dinámico, creá un Mesh desde el modal Crear Entidad → tab Objetos."
  - EN: "Structures are static by design. For a dynamic object, create a Mesh from the Create Entity modal → Objects tab."

Si el dev ya tenía RigidBody Dynamic sobre un Brush (caso F2H59 que no caía visualmente), al renderear la sección detectar el caso y mostrar warning visible (icon FA + texto rojo).

**Estimado:** 1h.

**Criterio de aceptación:** el dev no puede setear RigidBody Dynamic sobre un Brush desde la UI. Si abre un mapa pre-F2H60 con esa config, ve warning claro.

---

### Bloque E — Kit "Physics Box" en modal Convert Entity

En `EditorProjectActions_CreateEntity.cpp::renderConvertEntityModal`:
- Nuevo kit "Objeto físico" disponible solo si la entidad tiene `MeshRendererComponent`. Agrega `RigidBodyComponent` con Type Dynamic + Shape auto-detectado por AABB del mesh + halfExtents auto-calculados + mass default 1.0kg.
- Disabled si la entidad ya tiene RigidBody o si NO tiene MeshRenderer (en cuyo caso tooltip explica "Necesitás un Mesh; creá uno desde Crear Entidad → tab Objetos").

**i18n nuevas:** `editor.convert_modal.kit.physics_box`, `editor.convert_modal.kit.physics_box_desc`.

**Estimado:** 1h.

**Criterio de aceptación:** el dev spawna un Cubo desde tab Objetos, le da right-click → Convertir → Objeto físico → cae con gravedad al entrar Play.

---

### Bloque F — Map Tools híbrido Hammer+Blender

Pedido del dev: *"y esta idea me gusta"* (sobre dockable + opcionalmente desplegable como overlay).

- En workspace `map_editor`, el panel **Map Tools** sigue dockable lateral por default (Hammer-style).
- Agregar un toggle "Modo overlay" en el header del panel (icono + tooltip "Desplegar como overlay del viewport ortográfico"). Al click, el panel se cierra y aparece como sub-window flotante sobre el viewport ortográfico activo (similar al overlay flotante de F2H59 Bloque F pero con TODOS los buttons del Map Tools).
- Toggle reversible: en modo overlay, botón "Volver al panel" lo restaura.

**Estimado:** 3h. Refactor del MapEditorTopBar.

**Criterio de aceptación:** el dev en map_editor puede elegir entre el panel lateral fijo (más espacio para tools, default Hammer) o el overlay flotante (más espacio para viewport, default Blender).

---

### Bloque G — Overlay context-aware del viewport por workspace

El overlay flotante de F2H59 actualmente muestra los mismos 4 botones (Mover/Rotar/Escala/F) en todos los workspaces. F2H60 lo hace context-aware:

- **scripting / materials / gameplay**: Mover / Rotar / Escala (sin F porque no aplica edición de mesh).
- **narrative**: Mover / Rotar / Escala (mismo, F oculto).
- **map_editor**: Mover / Rotar / Escala / F + sub-modos Objeto/Vertex/Edge (los del Map Tools, condicionados).

**Estimado:** 2h.

**Criterio de aceptación:** el dev no ve botones que no aplican a su workspace actual. Reduce ruido visual.

---

### Bloque H — Modificadores Blender-style para operaciones booleanas

Pedido futuro del dev: *"a futuro que usemos modificadores como blender para los booleanos"*. F2H60 sienta la base:

- Refactor del panel "Operaciones Booleanas" del menú Brush (que se renombra a Estructura) hacia un **modifier stack** en el Inspector cuando hay BrushComponent seleccionado.
- Cada modifier es un (Op, TargetBrush): "Restar Brush_Sphere_03", "Unir con Brush_Cylinder_05", etc.
- El usuario puede activar/desactivar cada modifier (checkbox), reordenarlos (drag-handle), eliminarlos (X). Aplicación al brush original es non-destructive (el brush base no se modifica; el render aplica los modifiers en orden).
- Convención Blender: el resultado SOLO se materializa al "Apply" (boton al pie del modifier stack), que reemplaza el brush original con el resultado y limpia el stack.

**Estimado:** 5-6h. Bloque grande — sienta arquitectura nueva. Puede dividirse a 2 hitos si se infla.

**Criterio de aceptación:** el dev ve un stack de modifiers en el Inspector cuando selecciona un brush con boolean ops aplicadas. Puede activar/desactivar cada operación sin perder el brush original.

---

### Bloque I — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.47.0-fase2-hito60`. ~30 min.

---

## Total estimado

**9 bloques A-I, ~20-24h**. Hito grande — candidato a partir si Bloque H (modifiers) se infla. Decisión al arrancar Bloque H: si modifiers va a tomar 1+ día, defererilo a F2H61 propio.

---

## NO entra en F2H60

- **Homogeneización completa del sistema de iconos del editor** — pedido del dev de F2H59 ("creo que eso es algo que debermos homogeneizar mas adelante"). Scope grande (auditar todo el editor + reemplazar icons inconsistentes). Hito propio.
- **Toro como brush no-convexo** — sigue requiriendo CSG refactor o N segmentos auto-spawned. Re-evaluar si emerge demanda.
- **Cápsula técnica real** (cilindro central + 2 hemisferios convexos preservados) — F2H59 entrega Sphere estirada Y como aproximación. Si emerge demanda de paredes laterales rectas, follow-up.
- **Cleanup completo del Toolbar.h/.cpp dead code** — pre-F2H59 el Toolbar era panel dockable; F2H59 lo sacó del dockspace pero dejó el struct linkeable. Cleanup completo si emerge demanda futura.
- **F2H61+ (Sub-fase 2.6 retomada)**: god rays / shadow polish (CSM cascades, contact shadows, soft shadows PCF) — el orden de impacto visual original (bloom F2H55 → SSAO F2H56 → color grading F2H58 → god rays F2H6X) sigue vigente.

---

## Riesgos identificados

- **Bloque B (meshes procedurales)** es el bloque crítico — si los meshes generados no matchean visualmente a sus equivalentes brush (Cubo vs Box brush), el dev nota inconsistencia. Mitigación: tests visuales lado a lado en validación del Bloque G.
- **Bloque H (modifier stack)** puede inflar el hito. Si al arrancarlo se ve que requiere >1 día, partir a F2H61 propio.
- **Migración silenciosa para devs con mapas pre-F2H60**: si un dev ya tenía `RigidBody Dynamic` sobre brushes (caso F2H59 que no caía), F2H60 Bloque D les muestra warning pero NO migra automáticamente. Documentación clara en el CHANGELOG: "los mapas pre-F2H60 con RigidBody Dynamic sobre Brushes deben re-crear esos objetos como Mesh".
- **Performance de meshes procedurales**: 5-6 meshes generados al startup del editor = costo de inicialización +. Mitigación: lazy generation (solo cuando se usa el botón "Crear Entidad → tab Objetos → Cubo"), no en ctor del AssetManager.
