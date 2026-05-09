# Plan — F2H32: Geometry tools (clip + carve)

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entry F2H31 cierre (tool selector + selection + snap-to-
> vertex), entry F2H12 (boolean ops API ya existe), entry F2H25 (BSP
> polygon clipping ya existe en CompileMap).

---

## Objetivo

Cerrar las herramientas de geometría tipo Hammer Editor real:

1. **Clip tool**: 2 clicks en orto definen una línea; el plano resultante
   es perpendicular al plano del view y contiene esa línea. Splittea
   los brushes seleccionados en "Front" + "Back". UI cycle (tecla `T`
   o botón) cambia entre `KeepFront` / `KeepBack` / `KeepBoth` antes
   de confirmar.
2. **Carve UI**: con un brush seleccionado, click "Carve" en el toolbar
   resta TODOS los brushes que intersectan al seleccionado. Reusa
   `Csg::subtract` de F2H12 que ya tiene la math + el flow undoable
   de `EditorProjectActions_Boolean.cpp`.

**Beneficios**: cierra 2 de las 3 herramientas core de geometría de
Hammer (la 3ra "Hollow" se deja para futuro si emerge demanda — es
sintáctico azúcar de carve con un cubo interior). Tras F2H32, el
editor mappings flow está cubierto al ~95% del Hammer.

---

## Filosofía de diseño

- **Reuso máximo de la infra existente**:
  - Clip tool reusa `Plane` de core/math + agrega face al `Brush::faces`
    (intersección con half-space). Validación con `isBrushValid` ya
    existente.
  - Carve UI reusa `Csg::subtract` + el flow de `applyBooleanOp` en
    `EditorProjectActions_Boolean.cpp`. Solo se cambia el "qué B" — en
    lugar de la otra entidad seleccionada, todos los brushes que
    intersectan al active.
  - Tool selector reusa el patrón del F2H31 Bloque B: agregar variante
    `MapTool::Clip` al enum + botón en el toolbar lateral. Mutually
    exclusive con Select / CreateBlock / Pincel.
- **Validación incremental**: 3 commits feat (B clip, C carve,
  D+E docs+tag). Cada commit con build OK + verificación visual del dev.
- **Sin schema bump**: ningún cambio persistente al `.moodmap` —
  spawneamos brushes nuevos via `pushCreatedEntities` que ya soporta
  undo (mismo patrón de F2H12 boolean ops).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Clip plane derivada de 2 clicks + view-perpendicular | En orto, los 2 clicks no pueden definir un plano 3D directamente (3 puntos coplanares en el view plane = degenerados). Convención Hammer: la línea entre los 2 clicks + extrusión sobre el eje perpendicular del view define el plano. Implementación: `normal = cross(forwardAxis_orto, lineDir_world)`; `d = -dot(normal, p1)`. |
| 2 | Clip = agregar plane a Brush::faces | Para el lado "Front" (normal positiva): copiar el brush, agregar `BrushFace { plane }` con la nueva normal. Para "Back": agregar la negación (`-normal, -d`). El brush resultante = intersección con el half-space. Validar con `isBrushValid` (descartar si degenerado). |
| 3 | Clip mode cycle Front / Back / Both | Tecla `T` durante la sesión clip cycle entre 3 modos. Both = spawnea ambos brushes (resultado del split tradicional). Default = KeepFront. Visual hint en el toolbar / status bar. |
| 4 | Clip command undoable | Mismo patrón que `EditorProjectActions_Boolean.cpp`: snapshot del brush original + spawn de los brushes resultado + push del command que en undo restaura el original y elimina los nuevos. Reusa `SavedBrush` + `pushCreatedEntities` de F2H12. |
| 5 | Carve = subtract con todos los intersectantes | El active brush A es el "que se carga"; B = cada otro brush que intersecta el AABB de A. Iterar `Csg::subtract(A, B)` en cadena. Si subtract devuelve N>1 fragmentos, A se reemplaza por todos. Si A queda completamente consumido (subtract devuelve []), eliminar A. |
| 6 | Carve broadphase por AABB | Sin esto, escenas con cientos de brushes hacen O(N²) tests booleanos. Mitigación: solo brushes cuyo AABB world intersecta el AABB de A entran al subtract. |
| 7 | Carve no destruye los "carvers" | Convención Hammer: los brushes que cortan A se preservan. El dev decide si los borra después o los mueve. |
| 8 | Sin keyboard shortcut nuevo para carve | Carve es destructivo y requiere selección explícita; vía botón "Carve" del toolbar es más seguro que un atajo (no quiero que un dev presione `C` por accidente). |
| 9 | Tool selector con Clip como 4ta opción | Toolbar "Map Tools" sección "Herramienta": `Selecc / Bloque / Pincel / Clip`. Pincel y Clip son mutually exclusive (no podés clipear y dibujar simultáneamente). |

---

## Bloques

### A — Plan F2H32 (este archivo)

### B — Clip tool

- `EditorMode.h`: extender `MapTool` con `Clip = 3`.
- `EditorApplication.h`: nuevo `struct ClipToolSession { active, orthoIdx, p1World, p2World, hasP1, hasP2, keepMode }` + miembro `m_clipTool`.
- `MapEditorTopBar.cpp`: agregar botón "Clip" debajo de "Pincel" en
  la sección "Herramienta".
- `EditorApplication_Run.cpp`: nuevo bloque 2.4g (clip tool):
  - Si `m_mapTool == Clip` y orto recibe click:
    - 1er click: capturar `p1World` (con snap-to-vertex / grid).
    - 2do click: capturar `p2World`. Si `||p2 - p1|| > snapStep` y la
      distancia 2D sobre el view plane es no-degenerada, el plano
      queda armado.
  - Tecla `Enter` / botón "Confirmar Clip" del toolbar: dispara el
    split.
  - Tecla `T` durante la sesión: cycle KeepMode (Front -> Back -> Both
    -> Front).
  - Tecla `Esc`: cancelar.
- `EditorOverlay.cpp`: si clip activo + `hasP1`, dibujar la línea
  desde `p1` al cursor (rubber band amarillo). Si `hasP2`, dibujar el
  rectángulo del plano sobre el view plane (mismo patrón que el
  marquee select de F2H31 Bloque B).
- Lógica del split: nuevo helper `clipBrushByPlane(brush, plane,
  keepMode) -> vector<Brush>` en `engine/world/csg/BrushOps.cpp`.
  Implementación: copiar brush + agregar plane (o negación o ambas);
  validar; devolver lo que sea válido.
- Command nuevo `ClipBrushesCommand` reutilizando snapshots del
  pattern de Boolean ops (F2H12): captura tags + planes pre + spawn
  data post; execute spawnea, undo restaura.
- Validación: dev arranca con 1 brush selecto, activa Clip, 2 clicks
  en orto, `T` cycle a KeepBoth, Enter → 2 brushes (cada lado del
  plano) reemplazan al original; Ctrl+Z restaura el original.

### C — Carve UI (Hammer-style boolean subtract automático)

- `MapEditorTopBar.cpp`: botón nuevo "Carve" debajo de "Snap V".
  Tooltip: "Resta el brush selecto por todos los brushes que
  intersectan (Hammer-style)".
- `EditorUI.h`: nuevo request `requestCarve()` + consumer.
- `EditorApplication_Run.cpp`: handler — si hay un brush selecto:
  - `A = active brush`
  - `aabbA_world = brushAabbWorld(A)`
  - Para cada brush `B` con `B.handle() != A.handle()`:
    - `aabbB_world = brushAabbWorld(B)`
    - Si NO `intersects(aabbA, aabbB)`, skip.
    - Acumular `B` en una lista `carvers`.
  - Si `carvers` vacía, log info "Carve: no hay brushes intersectantes".
  - Sino: aplicar `Csg::subtract(A, carver)` iterativo. Cada subtract
    puede producir múltiples fragmentos; tomar la lista resultado y
    hacer `subtract(fragment, next_carver)` para cada fragmento.
  - Reemplazar `A` por la lista final de fragmentos (similar al flow
    de `EditorProjectActions_Boolean.cpp`).
- Reuso del command de boolean ops para Ctrl+Z agrupado: examinar si
  el `BooleanOpCommand` existente puede aceptar N carvers o si requiere
  uno nuevo. Probable reuse con un loop que pushea N comandos
  encadenados.
- Validación: dev pone un brush A grande + 2 brushes B/C que
  atraviesan A. Selecciona A, click "Carve" → A se transforma en
  fragmentos respetando los huecos de B y C. B y C se preservan.
  Ctrl+Z restaura A entera.

### D — Cierre

- Test manual completo (dev valida visualmente):
  - Spawn 1 cubo. Activar Clip. 2 clicks en orto Top → línea + rect.
    Tecla T → "Keep Both". Enter → 2 brushes. Mover uno aparte.
    Ctrl+Z 2 veces → vuelve al cubo original.
  - Spawn 2 cubos cruzándose. Seleccionar el primero. Click "Carve"
    → primer cubo queda con un hueco en forma del segundo. Ctrl+Z →
    primer cubo original. El segundo cubo no fue tocado.
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`,
  `PENDIENTES.md`), archive plan, commit + tag `v1.22.0-fase2-hito32`
  + push.

---

## Lo que NO entra en F2H32 (queda para F2H33 / futuro)

- **VisGroups + texture alignment**: van a **F2H33** (organización +
  face polish).
- **Hollow tool** (carve A con un brush "interior" para crear paredes):
  scope incremental sobre carve. Diferido — no típico del flow básico
  de Hammer.
- **Clip tool en perspectiva 3D**: sería gizmo manipulable para mover
  el plano libre. Scope mucho mayor. Hammer no lo tiene en perspective.
- **Vertex split** (mover un vertex genera 2 brushes): scope mayor +
  pocos casos de uso. Diferido.
- **Multi-brush carve simultáneo** (selección N de A's en lugar de 1):
  el reuso de `subtract` lo soporta; UI solo necesita iterar. Si emerge
  necesidad, agregar.

---

## Tradeoffs conocidos

- **Clip de N brushes selectos**: si hay N brushes seleccionados al
  activar Clip + Enter, el plano splittea TODOS los seleccionados con
  el mismo plano. Comportamiento Hammer. Si emerge edge case (ej.
  brushes muy lejos del plano que no intersectan), `isBrushValid` los
  filtrará pero pueden generar trabajo innecesario. Aceptable para
  v1.
- **Carve no preserva UVs perfectamente**: `Csg::subtract` ya tiene el
  flag `lockToWorld` para texture lock (F2H15) que resuelve la mayor
  parte; los fragmentos resultado heredan los UVs de A. Si emerge bug,
  refinar.
- **Carve O(N²) sin spatial hash**: los carvers se filtran por AABB
  intersect del active brush. Si la escena tiene 1000 brushes y el
  active toca 100, hay 100 ops booleanas por click. En práctica < 10.
  Aceptable.
- **Clip plane "stuck" si los 2 clicks coinciden**: degenerate plane.
  Validación: si `||p2 - p1|| < kPlaneEpsilon`, log warn + cancelar la
  sesión.
- **Clip tool no tiene preview del split en vivo** (mostraría los
  brushes "post-split" mientras el dev mueve el cursor): scope grande
  (cambiar la mesh cada frame). Solo preview el plano. Si emerge
  necesidad, agregar como sub-bloque.
