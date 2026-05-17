# BACKLOG — Follow-ups acumulados fuera del plan original

> **Propósito de este documento**: registrar pendientes que **NO entran en el plan original** (`PLAN_FASE2.md`) pero pueden volverse hito propio si emerge demanda real. Sirve como memoria del proyecto para no perder ideas + sirve como freno: si no está aquí ni en el plan original, **no es scope**.
>
> **Creado en F2H60 reset (2026-05-16)** tras pedido explícito del dev: *"hagamos todo lo que falta que venia del plan original, y guardemos lo que falta, de pendientes, para no desviarnos"*.
>
> Ordenado de **mayor a menor presión inferida** (qué pidió el dev explícitamente vs qué surgió como sugerencia técnica).

---

## 1. Pedido explícitamente por el dev en algún tour (alta presión)

### 1.1. Map Tools híbrido Hammer+Blender (F2H59 tour)

**Contexto**: en el workspace `map_editor`, el panel **Map Tools** (Bloque/Pincel/Clip/Objeto/Vertex/Edge/Cara/Snap/Nombres/Carve) ocupa una columna lateral fija estilo Hammer. El dev sugirió: que pueda ser dockable a un lado por default (Hammer) **pero** opcionalmente desplegable como overlay flotante sobre el viewport ortográfico cuando quiera más espacio (Blender). Cita: *"y esta idea me gusta"*.

**Scope estimado**: 3h. Toggle "Modo overlay" en el header del panel + posicionamiento sticky al viewport ortográfico activo. El struct existente `MapEditorTopBar` ya tiene la lógica de todos los buttons; solo cambia la presentación.

**Por qué NO atacamos ahora**: pide cambio de UX en workspace específico que NO está en el plan original. Conviene atacar cuando el dev viva un escenario donde la fricción sea concreta.

---

### 1.2. Brushes con física dinámica (F2H59 tour)

**Contexto**: el dev creó un cubo brush, le agregó `RigidBodyComponent` Dynamic y al darle Play el cubo no cayó visualmente. Causa raíz: mesh cache del brush se construye con `worldMatrix` incrustado y solo se rebuilda si `bc.dirty=true`. Decisión tomada en F2H59: adoptar Source paradigm (brushes = estructura estática, meshes = objetos dinámicos). **Pero después el dev pidió "no quiero alejarme tanto de la vision y el plan original"** y descartamos el Source paradigm.

**Opciones residuales**:
- **A**: fix mínimo — forzar `bc.dirty = true` cada frame si el brush tiene `RigidBodyComponent::Type::Dynamic`. ~30 min. Cost runtime: rebuild mesh cache per-frame para esos brushes (aceptable si son pocos).
- **B**: refactor shader para que el `worldMatrix` sea uniform (no incrustado en vertices). Cost mediano. Pero mantiene la limitación de que el AABB de física no se actualiza si las caras del brush cambian.

**Por qué NO atacamos ahora**: F2H60 retoma plan original. El workaround documentado (usar mesh en vez de brush para objetos dinámicos) cubre el caso 100%.

---

### 1.3. Homogeneización del sistema de iconos del editor (F2H59 tour)

**Contexto**: durante F2H59 Bloque F se eligió texto "F" para el botón Face del overlay flotante (más claro que `ICON_FA_VECTOR_SQUARE`). El dev observó: *"eso es algo que debermos homogeneizar mas adelante"*.

**Scope estimado**: auditoría completa del editor para identificar iconos inconsistentes + design pass de un set unificado. Probablemente medio día.

**Por qué NO atacamos ahora**: cosmético. No bloquea workflow.

---

### 1.4. Modifiers Blender-style para operaciones booleanas (F2H59 tour)

**Contexto**: el dev planea: *"planeo que a futuro que usemos modificadores como blender para los booleanos"*. Pre-F2H60 las booleanas se aplican como acción destructiva desde el menú Brush. Blender las maneja como **modifier stack** non-destructivo: cada modifier es un (Op, TargetBrush) que se puede activar/desactivar/reordenar/eliminar sin afectar el brush original.

**Scope estimado**: 5-6h. Refactor del sistema CSG ops + nueva sección en el Inspector para modifier stack.

**Por qué NO atacamos ahora**: el sistema actual (boolean destructivo) funciona. Modifier stack es UX upgrade significativo pero no bloqueante.

---

## 2. Sugerido por el agente durante turnos previos (presión media)

### 2.1. Overlay context-aware por workspace (F2H59)

**Contexto**: el overlay flotante de F2H59 muestra 4 botones (Mover/Rotar/Escala/F) en todos los workspaces. Podría adaptarse: scripting/materials/gameplay/narrative no necesitan "F" (no editan mesh). map_editor podría sumar sub-modos Vertex/Edge.

**Scope estimado**: 2h.

**Por qué NO atacamos ahora**: el overhead actual del botón F no usado en otros workspaces es mínimo.

---

### 2.2. Source paradigm explícito (F2H60 propuesta descartada)

**Contexto**: propuesta del agente que separaba **Estructura (brushes)** vs **Objetos (meshes procedurales)** con sub-secciones en el modal Crear Entidad. Rechazado por el dev en este reset.

**Por qué NO atacamos**: el dev confirmó que prefiere **mantener brushes como brushes** sin reorganizar el paradigma. La fricción del cubo dynamic se cubre con "usá mesh si querés física dinámica" (workaround documentado).

---

### 2.3. Kit "Physics Box" en modal Convert Entity (F2H59 propuesta)

**Contexto**: complemento de Source paradigm — kit en el modal Convert que aplica `RigidBodyComponent` Dynamic con shape/halfExtents auto-calculados desde el AABB del mesh.

**Por qué NO atacamos ahora**: descartado con el Source paradigm. Si emerge demanda real, ~1h.

---

### 2.4. Toro como N segmentos curvos (F2H59)

**Contexto**: el Toro NO entró en F2H59 porque no es geométricamente convexo (brushes CSG asumen convexidad). Posible solución: spawn N segmentos auto-grouped que forman el anillo (cada uno convexo).

**Scope estimado**: 4h. Requiere primer "primitive group" del sistema.

**Por qué NO atacamos ahora**: workaround documentado (2 cilindros + boolean Subtract manual).

---

### 2.5. Cápsula técnica real (F2H59)

**Contexto**: F2H59 entrega cápsula como `makeSphereBrush` estirada Y 2× (elipsoide). Cápsula técnica tiene paredes laterales rectas (cilindro + 2 hemisferios). Útil si emerge gameplay con player controller cápsula con casteo de raycast preciso.

**Scope estimado**: 2h.

**Por qué NO atacamos ahora**: la aproximación elipsoide cubre el caso visual.

---

## 3. Tech debt residual (baja presión, atacar si bloquea)

### 3.1. Cleanup del Toolbar dead code (F2H59)

**Contexto**: F2H59 movió las herramientas a overlay flotante. El struct `Toolbar` queda declarado en `EditorUI` pero no agregado a `m_panels`. Linkea pero no se usa.

**Por qué NO atacamos ahora**: el archivo `Toolbar.{h,cpp}` queda intacto por si emerge demanda de reactivarlo como panel separado. Cleanup = eliminar el archivo.

---

### 3.2. `ResetSectionCommand` undoable (F2H58)

**Contexto**: F2H58 Bloque G entrega botones "⟲ Restablecer" por sección del Environment panel **sin undo**. Snapshot multi-campo requeriría un Command custom.

**Por qué NO atacamos ahora**: el reset es operación clara intencional. Si emerge demanda real de undo del reset, ~3h.

---

### 3.3. `.cube` LUT parser (F2H58)

**Contexto**: F2H58 solo soporta LUTs PNG 256×16 (convención Unity URP). `.cube` es formato Adobe ASCII estándar de cine. Si los coloristas entregan `.cube` pueden no querer convertirlo manualmente.

**Por qué NO atacamos ahora**: workflow actual (tools/gen_luts.py + DaVinci export como PNG) cubre el caso.

---

### 3.4. Scan dinámico de `assets/luts/` para el preset dropdown (F2H58)

**Contexto**: F2H58 Bloque H tiene 5 built-ins hardcoded en el dropdown. Si el dev agrega `mi_lut.png` en `assets/luts/`, no aparece automáticamente — debe ir por "Personalizado..." (file picker).

**Por qué NO atacamos ahora**: 5 built-ins cubren los casos comunes; LUTs adicionales viables vía file picker.

---

### 3.5. Preview 3D del LUT en el panel (F2H58)

**Contexto**: visualizar el cubo RGB del LUT cargado como helper visual (similar a Photoshop "Color Table").

**Por qué NO atacamos ahora**: nicho de uso muy específico. El dev ve el efecto en la escena en vivo.

---

### 3.6. sRGB vs linear loading explícito del LUT (F2H58)

**Contexto**: F2H58 confía en los defaults del `AssetManager::loadTexture` para cargar la LUT. Convención técnica: la LUT es tabla de mapeo (NO imagen para mirar) → debería ser linear, no sRGB. Riesgo: si el AssetManager carga PNG como sRGB por default (probable para meshes con albedos), la LUT sufre doble gamma correction.

**Por qué NO atacamos ahora**: los 4 LUTs sample shipados generados por `gen_luts.py` son PNGs lineales y se ven correctamente. Si emerge artifact de color con LUTs externas, atacar como sub-hito puntual.

---

### 3.7. Preview 3D del mesh en modal Crear Entidad (F2H57)

**Contexto**: anotado en F2H57 cierre como memoria de proyecto. El dev pidió: *"a futuro me gustaria que haya una preview en 3D del elemento a importar"*. Requiere FB nuevo + turntable rotation off-screen render.

**Scope estimado**: 6-8h. Hito propio.

**Por qué NO atacamos ahora**: workflow actual (path lógico + nombre del mesh + ID) cubre el caso. Preview es nice-to-have.

---

### 3.8. Snapshot undoable del paso "Convertir entidad" (F2H57)

**Contexto**: el modal Convertir aplica kits aditivos sin undo del paso (los edits individuales post-convert SÍ son undoable).

**Scope estimado**: 3h.

**Por qué NO atacamos ahora**: limitación documentada de F2H57 v1. Si emerge demanda real, atacar.

---

### 3.9. Drag-and-drop SO → viewport para crear entidad (F2H57)

**Contexto**: workflow Unreal — arrastrar un .fbx desde el explorador del SO al viewport del editor crea entidad. Complementa el modal "+ Crear Entidad".

**Por qué NO atacamos ahora**: el modal cubre el caso. Drag-drop es UX paralelo.

---

### 3.10. Cleanup completo de DemoSpawners_*.cpp dead code (F2H57)

**Contexto**: F2H57 Bloque E eliminó las entries del menú Ayuda > Demos pero dejó los handlers `processSpawn*Request` en `DemoSpawners_*.cpp` como dead code linkeable (no triggerable desde UI). Razón: helpers compartidos como `ensureDemoIntroDialogExists` que F2H47 podría estar usando.

**Por qué NO atacamos ahora**: el dead code no tiene cost runtime. Cleanup completo requeriría auditar consumers internos.

---

## 4. Sin presión (capturar antes que olvidar)

- **Plugin system para kits custom del modal Convertir** (F2H57) — Sub-fase 3+.
- **Templates compuestos de escena** ("escena narrativa completa" como un kit) (F2H57).
- **Vista ortográfica isométrica** (`OrthoCamera::View::Iso`) si emerge demanda de visualización 3/4 — F2H28.
- **GUI custom de Box collider sin RigidBody** (helper visual del AABB en el viewport).
- **Hot-reload de shaders** ya existe parcialmente; consolidar en panel dedicado.

---

## Política

- **Para mover un item a hito propio**: el dev lo solicita explícitamente o emerge bloqueo concreto.
- **Para borrar un item del backlog**: el problema ya no aplica (ej. F2H62 refactor del shader hace obsoleto el item 1.2).
- **Si nadie toca el backlog en 6 meses**, revisarlo entero — items pueden estar obsoletos por evolución natural del proyecto.
