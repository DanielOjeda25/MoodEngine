# PLAN F3H27 — Parenting jerárquico de transforms (Empty/Group padre + hijos)

**Estado:** ✅ **CERRADO** — `v2.27.0-fase3-hito27` (2026-05-28).
**Predecesor:** F3H26 (Polish UX modal/menubar/toasts/preferences).
**Origen:** stub original F3H27 incluía 4 items (Grupos+MapTools / Parenting / Mundo grande / HDRI dinámico). Dev eligió **splitting en 4 hitos separados** (F3H27-F3H30) y arrancar por **Parenting** (Item 2 del stub).

---

## Sub-fase 3.4 (post-split)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – ✅ Parenting jerárquico de transforms ⬅ este hito
F3H28 – 📝 Grupos + Map Tools como categorías del Properties Editor (stub)
F3H29 – 📝 Mundo grande (camera limits / far plane / reverse-Z) (stub)
F3H30 – 📝 HDRI dinámico + ciclo día/noche (stub)
```

---

## Norte

Hoy un mapa "edificio + 6 cubos hijos" es una flat list de 7 entidades sin relación lógica. Mover el edificio = seleccionar las 7 + drag, propenso a error. F3H27 introduce el modelo **parent/child estándar de la industria** (Blender/Unity/Unreal): un Empty/Group como padre + N hijos cuyos transforms se acumulan recursivamente. Mover el padre = mover hijos. Borrar el padre = cascada destructiva.

Sin parenting, el level design escala mal: el dev no puede manipular "el edificio entero" como unidad lógica, y no puede separar lógicamente "torre" + "cuerpo principal" en sub-jerarquías.

---

## Mecánicas cerradas (vista del dev)

1. **Outliner jerárquico** — entries con depth indent (14px/nivel) + arrow ▸/▾ (caret FA) expand/collapse subtrees. Sin re-ordering en el panel, sigue el orden del registry.
2. **Ctrl+G** — agrupa la selección. Crea un Empty `Group_<N>` (N = max existing +1) posicionado en el centroide del AABB combinado de los seleccionados. Los seleccionados quedan como hijos del Empty preservando world-space.
3. **Shift+Ctrl+G** — desagrupa. Los hijos vuelven a root preservando world-space. El Empty queda vacío (no se borra automático — anti-sorpresa). El dev decide si borrarlo después.
4. **Drag-drop reparent en Outliner** — arrastrar entity sobre otra reparenta como hijo. Anti-ciclo: prohibido arrastrar un ancestro sobre su descendiente.
5. **Cascade delete** — Delete sobre un padre borra padre + todos los descendientes. Ctrl+Z deshace de a uno (price honesto — agendizable a compound atomic en F3H28+).
6. **Top-level filter del gizmo multi-select** — si seleccionás padre + hijo y arrastrás el gizmo, solo el padre se mueve (el hijo lo sigue por jerarquía sin doble delta).
7. **Persistencia** — el `.moodmap` guarda `parent_tag` (string, no handle) en cada child + el Empty padre se guarda aunque no tenga componentes "serializables" si es referenciado como padre.
8. **Outline englobador** — al seleccionar un Group, el outline del viewport dibuja un AABB axis-aligned combinado de padre + descendientes (no el cubito chico del Empty solo).

---

## Decisiones cerradas

**D1 — Delete sobre padre = cascada destructiva (vs detach hijos + borrar solo padre).** Convención Unity/Unreal/Blender: borrar padre borra hijos. Mental model "scene graph" — un sub-tree es una unidad lógica. Alternativa "detach + borrar padre" rompe el principio de menor sorpresa: el dev espera que Delete sobre "el edificio" borre el edificio entero, no que aparezcan 6 cubos sueltos en root. Implementación simple: N `DeleteEntityCommand` individuales (children → parent, DFS reverse order) pushed al history. Compound atomic delete = backlog F3H28+.

**D2 — Multi-select gizmo con padre + hijo: filtro top-level (hijos se ignoran).** Convención Blender/Maya/Unity. Si el dev seleccionó padre + hijo y mueve el gizmo, aplicar delta a ambos por separado duplica el movimiento del hijo (recibiría delta del padre + delta propio). Filtro: `Scene::topLevelAncestors(selected)` devuelve solo entities cuyo ancestor NO está también en el set. Edge case documentado: si el `active` (primary del SelectionSet) es un hijo con padre selected, el active igual recibe delta — aceptado por simplicidad.

**D3 — Posición del Empty al agrupar: centroide del AABB combinado.** Calculado vía `brushAabbWorld` (brushes) / `meshAabbWorld` (meshes) / pivot position (point entities). Alternativas descartadas: (a) origen del mundo (0,0,0) — disruptivo, los hijos quedan con offsets enormes; (b) pivot del primer seleccionado — sesga hacia el orden de selección. El centroide del AABB es lo que hace Blender (Object > Set Origin > Origin to Geometry) y Unity (Create Empty Parent al seleccionar N).

**D4 — Serialización: `parent_tag` (string) vs `parent_handle` (u32).** Tags son estables entre saves; handles del `entt::registry` cambian arbitrariamente (orden de creación, undo/redo, recompose). Patrón gemelo a F2H65 (Joint targetEntity). 2-pass resolve en `SceneLoader::applyEntitiesToScene` tras materializar TODAS las entities: busca child por tag → busca parent por tag → setea `tc.parent`. Si el padre fue borrado entre saves (`parent_tag` apunta a tag inexistente), el child queda como root + warn en log.

---

## Implementación (5 fases)

**Phase 1 — Backend + Render path.**
- `TransformComponent` gana `entt::entity parent = entt::null` (54 callsites de `worldMatrix()` quedan local-only; documento en el header).
- `Scene` gana 4 helpers: `worldMatrixOf(handle)`, `descendantsOf(root)`, `topLevelAncestors(selected)`, `isAncestorOf(ancestor, descendant)`. Implementación iterativa con clamp 32-niveles para resistir ciclos.
- Render path actualizado a `worldMatrixOf` en 6 archivos: `SceneRenderer_Render.cpp` (static+skinned+brush+translucent), `SceneRenderer_Ortho.cpp`, `RenderBatching.cpp`, `ShadowPass.cpp` (3 lambdas), `OpenGLParticleRenderer.cpp` (localSpace), `EditorRenderPass.cpp` + `EditorRenderPass_Overlay.cpp`.

**Phase 2 — Commands + Cascade delete.**
- `SetParentCommand` (~120 LOC): preserva world-space via `glm::decompose`. Snapshot del local TRS antes/después en el ctor; execute/undo aplican.
- `GroupSelectionCommand` (~165 LOC): centroide AABB combinado + crea Empty + reparenta hijos preservando world.
- `UngroupSelectionCommand` (~90 LOC): sets `parent=null` preservando world.
- `EditorScene::deleteSelectedEntity`: cascade — DFS reverse, push N `DeleteEntityCommand` individuales antes del padre.

**Phase 3 — Outliner jerárquico.**
- `HierarchyEntry` gana `int depth` + `bool hasChildren`. `HierarchyPanel` gana `std::unordered_set<entt::entity> m_collapsed`.
- `collectHierarchyEntries` reescrito como DFS pre-order: build roots list (parent==null || parent inválido) → iterative stack DFS push children en reverse.
- `HierarchyPanel.cpp`: indent per-depth + arrow caret FA (`ICON_FA_CARET_DOWN`/`ICON_FA_CARET_RIGHT`) clickeable + drag-drop target con anti-ciclo (`isAncestorOf`).

**Phase 4 — Hotkeys + top-level filter.**
- `EditorApplication`: handlers Ctrl+G / Shift+Ctrl+G gated por KMOD_CTRL + repeat==0 + Editor mode + !WantTextInput.
- `EditorScene::groupSelectedEntities`: aplica `topLevelAncestors` antes de pushear `GroupSelectionCommand`.
- `EditorOverlay_Gizmo::populateOtherStarts`: filtro topLevel para multi-select (skipea hijos cuyo ancestor está en set).

**Phase 5 — Serialización + tests.**
- `SavedEntity` gana `parentTag` (string).
- `EntitySerializer.cpp`: writes `parent_tag` si `t.parent != entt::null`.
- `EntitySerializer_Parse.cpp`: reads via `j.value("parent_tag", "")`.
- `SceneLoader.cpp`: 3er pass post-Joint resolution (busca child por tag → busca parent por tag → setea `tc.parent`).
- `SceneSerializer.cpp`: Empty padre se persiste aunque NO tenga componentes "serializables" si es referenciado como parent (sin esto, el Empty se descarta al guardar y los hijos quedan huérfanos al cargar).
- `TilePersistence.cpp::isTileModified`: tile con `parent != entt::null` es modificado (sin esto, los Tiles agrupados se descartan al guardar y se regeneran como roots al cargar).
- `tests/test_scene.cpp`: 6 cases nuevos / 31 asserts F3H27 (worldMatrixOf sin/con parent, descendantsOf DFS, topLevelAncestors filter, isAncestorOf, anti-cycle).

---

## Ajustes reactivos post-validación visual

**(R1) Icon del Group en Outliner — tofu `?`.** Primer intento de `iconForEntity` para Empty con hijos usaba `ICON_FA_OBJECT_GROUP` (0xF247); el dev reportó tofu `?` en el viewport del Outliner. Investigamos: el range cubre el codepoint, pero el atlas FA (subset free solid del proyecto) no rasteriza ese glyph en este TTF. Fallback final: `ICON_FA_FOLDER` (📁) — garantizado en el atlas (lo usa MenuBar "Archivo"). Semántica "carpeta = contenedor de hijos" es Unity GameObject empty / Hammer group estándar.

**(R2) Arrow ▶/▼ del expand/collapse — tofu `?` también.** Usábamos los geometric shapes Unicode U+25B6 / U+25BC. El range de Lato cubre Basic Latin + Latin-1 + General Punctuation (0x2010–0x2027), NO Geometric Shapes (0x25A0–0x25FF). Tofu garantizado. Fix: macros nuevas `ICON_FA_CARET_DOWN` (0xF0D7) + `ICON_FA_CARET_RIGHT` (0xF0DA) en `IconsFontAwesome6.h` + usar en `HierarchyPanel`. Convención Hammer/Unreal.

**(R3) Outline del Group no envolvía a los hijos.** Al seleccionar el Empty `Group_1`, el outline dibujaba un cubito 0.5m centrado en el centroide (point marker para entities sin geometría). El dev pidió que envolviera ambos cubos hijos. Fix: en `EditorRenderPass_Overlay.cpp::drawEditorScene3DOverlay`, si la entity selected tiene descendants, computar el AABB axis-aligned combinado de su geometría propia + la de todos sus descendientes (brush/mesh) y dibujar esa caja. Sin descendants → comportamiento original (OBB orientado por mesh local). Sin geometría en ningún descendant → fallback al point marker.

**(R4) Persistencia rota: Group_1 + tiles agrupados no se guardaban.** Después de Ctrl+G → save → reopen, el Group_1 desaparecía y los Tile_4_5/Tile_4_2 volvían a ser roots. Dos bugs encadenados:
- `SceneSerializer.cpp:256-258` filtraba entities sin componentes "serializables" (MeshRenderer/Light/RigidBody/...). El Empty Group_1 cae fuera del filtro → no se persiste.
- `TilePersistence.cpp::isTileModified` no chequeaba `parent`. Tiles agrupados con scale/material default se consideraban "no modificados" → no se persisten → al cargar se regeneran del grid como roots SIN parent.
- Fix: agregar check `isParent` en SceneSerializer (Empty referenciado como parent por alguien → persistir) + agregar `tc.parent != entt::null → modified` en isTileModified.

**(R5) Modal welcome decía "Versión 2.25.0 — Fase 3 cerrada".** Mientras debugueaba persist, el dev pidió sacar "Fase 3". Memoria `no-internal-milestone-refs-in-ui`. Cambio: `editor.modal.about.version` a "Versión 2.27.0" (ES) / "Version 2.27.0" (EN).

**(R6) Gizmo no cambiaba a Rotate/Scale sobre el Group.** Post-cierre, el dev reportó: "el gismo no cambia, al rotar o escalar, solo si presiono 2 veces para rotar o escalar". `EditorOverlay_Gizmo.cpp:41-48` forzaba `effectiveMode = Translate` para entities sin `MeshRenderer` ni `BrushComponent` — clamp original de F2H14 para que Light/Audio puros no mostraran handles inútiles. F3H27 introdujo un nuevo caso no contemplado: Empty padre de un sub-tree, donde rotar/escalar SÍ tiene efecto visual (los hijos heredan vía `worldMatrixOf`). Fix: extender `hasGeometry` con `|| !m_scene->descendantsOf(selected.handle()).empty()`. Light/Audio sueltos siguen cayendo a Translate; Group con hijos acepta los 3 modos. Los modales E×2 / R×2 ya funcionaban porque operan directo sobre `tform.rotationEuler`/`scale` sin pasar por el clamp.

**(R7) Outline del Group no rotaba con el padre — "se re-adapta" axis-aligned.** Post-fix R6, el dev reportó al rotar el Group: "el outline no rota se re adapta". `EditorRenderPass_Overlay.cpp:273-298` (R3) acumulaba AABB **world-space** de cada descendant — al rotar el padre, los corners world de los hijos cambian y el AABB axis-aligned se re-computa cada frame envolviéndolos, en vez de rotar como OBB con el padre. Fix: computar el AABB en el espacio **local del padre** vía `parentWorldInv × childWorld × localCorner`, después dibujar los 8 corners proyectados por `parentWorld` → OBB que rota/escala con el Group. Patrón Blender (`Set Origin > Origin to Geometry` + outline) / Unity (parent gizmo bounds).

**(R8) Empty no era pickable desde el viewport 3D + falta de marker visual + priority.** Post-cierre, el dev reportó: "desde la escena puedo seleccionar el grupo pero desde el area 3D no puedo". El `Group_<N>` (Empty) no tenía ninguno de los componentes que `ScenePick::pickEntityFromRay` considera pickables (Mesh/Brush/Light/Audio/Trigger/Camera/Particle) → caía al `return;` final. Tres cambios encadenados:
- **Empty pickable** — `ScenePick.cpp:184`: nuevo `else if (!scene.descendantsOf(e.handle()).empty())` aplicando `raySphere` contra `t.position` con `k_iconPickRadius = 0.6f`. Solo Empties que son padres de algo (`descendants != empty`) son pickables; Empties huérfanos no aportan click target útil.
- **Marker XYZ visible permanente** — `EditorRenderPass_Overlay.cpp` post-loop selección: itera entities con `forEach<TransformComponent>` y para cada Empty con descendants dibuja 3 líneas cruzadas (X/Y/Z, 30cm cada lado, color gris medio `(0.55, 0.55, 0.55)`) proyectadas por `worldMatrixOf`. Patrón Blender `Empty > Axes`. Sin este marker el dev no sabía dónde estaba el pivot del Group cuando no estaba seleccionado.
- **Priority sobre geometría de fondo** — `ScenePick.cpp`: track separado `bestEmpty / bestEmptyT` para los hits del icono Empty. Al final del `forEach`, si `bestEmpty.entity` válido → return directo, ignorando `best` general. Patrón Blender/Unity: el icono del Empty es overlay editor y siempre gana al click cuando el rayo lo pega, aunque haya un brush hijo entre la cámara y el pivot. Sin priority, el dev no podía seleccionar un Group si los hijos cubrían el pivot world-space.

---

## Limitaciones / Edge cases conocidos

- **Multi-select gizmo edge case D2**: si el `active` (primary del SelectionSet) es un hijo con su padre también selected, el active igual recibe delta. Documentado, no es bug. Si emerge demanda, agregar filtro al active.
- **Cascade delete = N undo steps**: Ctrl+Z deshace de a uno (price honesto). Compound atomic = backlog F3H28+.
- **Empty huérfano tras ungroup**: Shift+Ctrl+G no borra el Empty automáticamente. Decisión explícita anti-sorpresa.
- **Physics/picking/editor tools usan local-as-world** (54 callsites de `TransformComponent::worldMatrix()` no migrados a `worldMatrixOf`). Para parents simples (Group con identity local) no se nota; para parents rotados/scaled, picking de hijos puede ser impreciso. Agendizable a F3H28+ si el dev lo nota.

---

## Tests

`tests/test_scene.cpp` — 6 cases nuevos F3H27 / 31 asserts:
- `worldMatrixOf sin parent devuelve local` (sin parent, world == local).
- `worldMatrixOf con parent acumula world` (parent at X=10 + child local X=2 → world X=12).
- `descendantsOf devuelve DFS pre-order` (subtree completo, 3 descendants).
- `topLevelAncestors filtra hijos del set` (set {p, c1, c2, orphan} → top {p, orphan}).
- `isAncestorOf detecta cadenas` (gp→p→c verifica grandparent y self-relations).
- `worldMatrixOf resiste ciclos triviales` (entity = self.parent → clamp 32-niveles, no NaN/inf).

Suite **previa F3H26 +6 cases / +31 asserts** verde.

---

## Backlog post-F3H27

- **Compound atomic delete** — un solo `CascadeDeleteCommand` que Ctrl+Z deshace en 1 step (vs N hoy).
- **Auto-borrar Empty huérfano post-ungroup** — opt-in en User Preferences si el dev lo pide.
- **Inspector parent display** — campo "Parent" readonly en Inspector con click-to-select. Hoy se ve solo en el Outliner via indent.
- **Top-level filter del active edge case** — si emerge demanda, filtrar también el active del multi-select.
- **Migrar physics/picking/editor tools a `worldMatrixOf`** — solo si el dev nota imprecisión en parents rotados/scaled.

---

## Lo que NO toca F3H27

- F3H28: Grupos + Map Tools como categorías del Properties Editor.
- F3H29: Mundo grande (camera limits / far plane / reverse-Z).
- F3H30: HDRI dinámico + ciclo día/noche.
- Reordenar entries del Outliner (drag para cambiar orden entre hermanos) — Maya/Unity sí, fuera de scope.
- Lock / hide per-entity en el Outliner — Blender sí, fuera de scope.
