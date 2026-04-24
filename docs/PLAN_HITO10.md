# Plan — Hito 10: Importación de modelos 3D con assimp

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 4.9 y 10 (Hito 10) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Romper el límite "todo es un cubo" del motor: permitir **importar modelos 3D** (OBJ/glTF primero) y dibujarlos como entidades del Scene. Es la primera vez que el render trabaja con geometría venida de un archivo.

- **`assimp` v5.x** como librería de importación.
- **`MeshAsset`** como tercer tipo de asset del motor (después de `Texture` y `AudioClip`).
- **`MeshRendererComponent` evolucionado**: referencia a `MeshAssetId` en vez de `IMesh*` crudo, + soporte multi-submesh (1 mesh por material).
- **Render scene-driven** real: `renderSceneToViewport` itera `Scene::forEach<Transform, MeshRenderer>` en vez del loop por grid. El grid sobrevive como fuente de `Tile_*` entities creadas en `rebuildSceneFromMap`.
- **Drag & drop** de un `.obj`/`.gltf` al Viewport crea una entidad con `MeshRendererComponent` apuntando al asset importado.
- Persistencia: `MeshRendererComponent` se serializa en el `.moodmap` (primer componente ECS que lo hace; antecesor del Scene authoritative completo).

No-goals del hito: animación esquelética (Hito 19), materiales PBR (Hito 17), shadow mapping (Hito 16), light components funcionales (Hito 11).

---

## Criterios de aceptación

### Automáticos

- [x] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos (asimp wrapper no puede agregar warnings a nuestros targets).
- [x] Log del canal `assets`: `cargado mesh <path> -> id N (<M> submeshes, <V> vertices)` al importar.
- [x] Tests: round-trip `MeshRendererComponent` en `.moodmap` + smoke test de carga de un `.obj` mínimo (cubo o triángulo) desde `assets/meshes/`.
- [x] Cierre limpio, exit 0.

### Visuales

- [x] Al arrastrar un `.obj` desde el Asset Browser al Viewport, aparece una entidad con el modelo importado posicionada bajo el cursor (tile picking del Hito 6).
- [x] AssetBrowser muestra sección "Meshes" con los archivos encontrados en `assets/meshes/`.
- [x] Inspector muestra sección `MeshRenderer` con dropdown del mesh y metadata (submeshes, vertices).
- [x] `Archivo > Guardar` persiste las entidades no-tile en el `.moodmap` con su `MeshRendererComponent`. `Abrir` las recrea idénticas.

---

## Bloque 0 — Pendientes arrastrados del Hito 9

- [x] Drag & drop de audio desde AssetBrowser a entidades — **diferido**: no fue necesario para este hito y se alinea mejor con el flow Scene-authoritative post-Hito 14. Sigue en el roadmap del Hito 9 como pendiente con trigger.

## Bloque 1 — Dependencia assimp

- [x] `CPMAddPackage` para `assimp/assimp v5.4.3`. Opciones: `BUILD_SHARED_LIBS OFF`, `ASSIMP_BUILD_TESTS OFF`, `ASSIMP_BUILD_ASSIMP_TOOLS OFF`, `ASSIMP_INSTALL OFF`, `ASSIMP_BUILD_SAMPLES OFF`, `ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF` + habilitar solo `ASSIMP_BUILD_OBJ_IMPORTER`, `ASSIMP_BUILD_GLTF_IMPORTER`, `ASSIMP_BUILD_FBX_IMPORTER`. `ASSIMP_WARNINGS_AS_ERRORS OFF` forzado antes del CPM (su codigo tiene warnings en /W4).
- [x] `target_link_libraries(MoodEditor PRIVATE assimp::assimp)` + mismo para `mood_tests`.

## Bloque 2 — MeshAsset y MeshLoader

- [x] `src/engine/render/MeshAsset.h`: `struct SubMesh { unique_ptr<IMesh> mesh; u32 materialIndex; u32 vertexCount; }`, `struct MeshAsset { string logicalPath; vector<SubMesh> submeshes; totalVertexCount(); }`.
- [x] `src/engine/assets/MeshLoader.{h,cpp}`: funcion libre `loadMeshWithAssimp(logicalPath, filesystemPath, MeshFactory) -> unique_ptr<MeshAsset>`. Flags: `Triangulate | GenNormals | FlipUVs | CalcTangentSpace`. Expande el index buffer de assimp a vertices planos (sin EBO) para matchear el pipeline actual. Vertex layout: pos(3) + color(3) + uv(2) stride 8.
- [x] `AssetManager` extendido con `MeshAssetId` + `loadMesh` + `getMesh` + `meshPathOf` + `meshCount`. `MeshFactory` inyectable con default `NullMesh` stub (tests sin GL). Slot 0 = cubo primitivo generado en el ctor via `createCubeMesh()` + factory — path sentinela `"__missing_cube"`.
- [x] Fallback: si assimp falla, `loadMesh` cachea el path apuntando al slot 0 y loguea al canal `assets`.

## Bloque 3 — MeshRendererComponent evolucionado

- [x] `MeshRendererComponent { MeshAssetId mesh; vector<TextureAssetId> materials; }`. Helper `materialOrMissing(i)` cae al slot 0 si el array es mas corto que el numero de submeshes.
- [x] `rebuildSceneFromMap` ahora crea tiles con `missingMeshId()` + textura del tile en `materials[0]`.
- [x] `renderSceneToViewport` itera submeshes y cambia textura por submesh (1 draw call por submesh).

## Bloque 4 — Scene authoritative en render

- [x] Render unificado a un solo loop scene-driven. `GridRenderer` eliminado (dead code tras la migracion) — archivos borrados, CMake actualizado.
- [x] `rebuildSceneFromMap` sigue siendo la unica fuente de tiles.
- [x] Drop de textura ahora usa `updateTileEntity(x, y, tex)` helper que encuentra la entidad `Tile_X_Y` y edita su `MeshRenderer.materials[0]` in-place — preserva seleccion del Hierarchy (el `rebuildSceneFromMap` completo la invalidaba). Si la tile era Empty, crea la entidad con los mismos defaults.

## Bloque 5 — AssetBrowser + drag & drop de mesh

- [x] AssetBrowser seccion "Meshes" (puesta antes de Audio para que no quede fuera de vista). Escanea `assets/meshes/` por `.obj`/`.gltf`/`.glb`/`.fbx`. Lista plana con metadata (submesh count, vertex count); thumbnails 3D diferidos (ver pendientes).
- [x] Drag payload `"MOOD_MESH_ASSET"`. `ViewportPanel` acepta ambos payloads (textura + mesh) en el mismo `BeginDragDropTarget`; cada uno escribe a su propio `m_pendingDrop`.
- [x] Drop crea una nueva entidad `Mesh_<path>` con `Transform` centrado en el tile (altura = 0.5*tileSize) + `MeshRendererComponent(meshId, slot 0)`.
- [ ] "Drop sobre una entidad existente (via Hierarchy) reemplaza su mesh" — **diferido** a post-Hito 10. Hoy siempre spawnea nueva entidad; reemplazar es un caso mas sutil (ambiguidad: reemplazar submeshes? appendear? renombrar el tag?). Trigger: cuando aparezca un flow de edicion avanzado de entidades.
- [x] Fix detectado durante smoke test: `BeginDragDropSource` tiene que ir INMEDIATAMENTE tras `Selectable`; si va despues del `SameLine + TextDisabled` se asocia al texto gris que no es draggeable.

## Bloque 6 — Serialización Scene (primer pase)

- [x] `k_MoodmapFormatVersion` 1 -> 2. Archivos v1 siguen siendo leibles (la seccion `entities` nueva es opcional al parsear).
- [x] `SceneSerializer::save` ahora acepta `Scene*` opcional. Si no-null, itera entidades filtrando `Tile_*` y solo las que tienen `MeshRendererComponent` (primer pase). Schema: `{tag, transform:{pos, rotEuler, scale}, mesh_renderer:{mesh_path, materials[]}}`. Paths logicos para que los ids volatiles no se filtren.
- [x] `SavedMap` extendido con `vector<SavedEntity>`; nuevos tipos `SavedEntity`, `SavedMeshRenderer` (con `std::optional`).
- [x] `EditorApplication::tryOpenProjectPath` aplica las entidades persistidas tras `rebuildSceneFromMap`.
- [x] Test round-trip: 3 entidades (una no-tile con MeshRenderer, una tile-like filtrada por prefijo, una sin MeshRenderer filtrada por falta de componentes); solo la primera aparece en el JSON con position/rotation/scale/materials preservados. Test adicional de backward-compat: `.moodmap v1` manual se carga con `entities=[]`.

## Bloque 7 — Tests

- [x] `tests/test_mesh_asset.cpp` (5 casos): triangulo temporal -> 1 submesh/3 verts; archivo inexistente -> nullptr; archivo corrupto -> nullptr; `AssetManager::loadMesh` fallback a slot 0 para paths invalidos; `AssetManager::loadMesh("meshes/pyramid.obj")` carga el asset real y cachea.
- [x] `tests/test_scene_serializer.cpp` extendido (+2 casos): round-trip de entidades + load de archivo v1 legacy.
- [x] `add_test` con `WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}` para que los tests que abren archivos reales resuelvan paths desde la raiz del repo.

Suite total: 90 casos / 380 asserciones (antes 83/346).

## Bloque 8 — Cierre

- [x] Recompilar, tests verdes, round-trip end-to-end verificado en el editor: drag pyramid x3 -> Ctrl+S -> Cerrar -> Abrir, las 3 entidades reaparecen en la misma posicion (log: `3 entidades`).
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.10.0-hito10` + push.
- [x] Crear `docs/PLAN_HITO11.md` (iluminación Phong/Blinn-Phong).

---

## Qué NO hacer en el Hito 10

- **No** animación esquelética / skinning (Hito 19).
- **No** materiales PBR. El material por submesh es solo `TextureAssetId`; color/metallic/etc. llegan en Hito 17.
- **No** luces reales (son placeholders del Hito 7, se activan en Hito 11).
- **No** shadow mapping (Hito 16).
- **No** hot-reload de meshes (útil pero no crítico; agregable post-hito).
- **No** LOD / frustum culling / streaming de meshes grandes.

---

## Decisiones durante implementación

Detalle en `DECISIONS.md` bajo fecha 2026-04-24 (Hito 10).

- **assimp v5.4.3 como static lib, solo OBJ/glTF/FBX habilitados**. Desactivar todos los formatos y habilitar los tres primarios reduce ~10 MB del binario. `ASSIMP_WARNINGS_AS_ERRORS OFF` forzado en cache para que su codigo con warnings /W4 no rompa nuestro build.
- **Expansion de indices a vertices planos**: assimp produce `(vertices, indices)`; nuestro `OpenGLMesh` usa `glDrawArrays` sin EBO, asi que el loader replica cada vertice referenciado en el index buffer. Para meshes pequenos (Suzanne, pyramid demo) el overhead de RAM es despreciable. Migrar a `OpenGLIndexedMesh` cuando aparezca un mesh real grande.
- **`NullMesh` stub como default MeshFactory** en `AssetManager`: permite construir el manager sin contexto GL (tests) sin que los ctors fallen al crear el slot 0. Produccion pasa siempre una factory real con `OpenGLMesh`.
- **`MeshAssetId` con path sentinela `"__missing_cube"`** para el slot 0: no es un path real del VFS; uso interno del serializer y logs. Si alguien persiste un MeshRenderer con `mesh=0`, el load reconstruye usando la misma logica de fallback.
- **Render scene-driven unificado via prefijo de tag `Tile_*`**: alternativa a agregar un `TileBackedComponent` marker. Prefijo reusa data existente, no suma componentes por caso puntual. Cuando el render ya no distinga grid/no-grid (Hito 14+), el filtro desaparece.
- **`updateTileEntity` como alternativa a `rebuildSceneFromMap` completo** en el flow de drop de textura: un `registry.clear()` completo invalida la seleccion actual del Hierarchy. El helper localizado busca la entidad por tag y edita in-place.
- **Schema v2 del `.moodmap` con seccion `entities` opcional**: v1 se sigue leyendo (entities queda vacio). Primer componente ECS que se persiste — antecesor de Scene authoritative completo.
- **Solo se serializa `MeshRendererComponent` en este hito**: `Script`/`Audio` quedan fuera por scope. Comportamiento documentado: spawnear y guardar un rotador demo o audio source los perdera al reabrir.
- **`.gitignore` fix**: el patron `*.obj` (para objetos MSVC) colisiona con Wavefront OBJ de assets. Agregada excepcion `!assets/meshes/*.obj`.

---

## Pendientes que quedan para Hito 11 o posterior

- **Preview 3D de meshes en el AssetBrowser** (thumbnails renderizados). Requiere un mini-pipeline: FB por mesh + pasada offscreen con camara orbital fija al escanear. El usuario lo pidio durante Bloque 5; diferido para no desviarse del plan. **Trigger:** polish de UX post-Hito 11 o cuando los meshes importados se vuelvan numerosos.
- **EBO / IndexedMesh real**. Hoy el loader expande indices a vertices planos. **Trigger:** primer mesh importado con >50k vertices o memoria GPU pinchando.
- **Hot-reload de meshes**. El hot-reload de texturas existe (Hito 5); para meshes seria analogo — comparar mtime y re-importar. **Trigger:** iteracion artistica con archivos editados en Blender/Maya externos.
- **Drop de mesh reemplaza entidad existente** (en vez de siempre spawnear nueva). El plan lo listaba en Bloque 5 pero se difirio por ambiguedad semantica (reemplazar submeshes? tag?). **Trigger:** cuando aparezca una necesidad concreta de "editar una entidad cambiandole el mesh".
- **Serializar `Script`/`Audio` components**. No está en scope hasta que Scene sea authoritative de verdad. **Trigger:** Hito 14 (prefabs) o hito posterior que migre el schema `.moodmap` a entity-centric.
- **Materiales reales** (color, normal map, metallic/roughness). Hoy materials solo es una lista de textures. **Trigger:** Hito 17 (PBR).
- **Drag & drop de audio** desde AssetBrowser a entidades. Arrastrado del Hito 9, diferido aqui tambien. **Trigger:** Scene authoritative.
- **Light components funcionales**. Son placeholders del Hito 7. **Trigger:** Hito 11 (iluminacion).
