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

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos (asimp wrapper no puede agregar warnings a nuestros targets).
- [ ] Log del canal `assets`: `cargado mesh <path> -> id N (<M> submeshes, <V> vertices)` al importar.
- [ ] Tests: round-trip `MeshRendererComponent` en `.moodmap` + smoke test de carga de un `.obj` mínimo (cubo o triángulo) desde `assets/meshes/`.
- [ ] Cierre limpio, exit 0.

### Visuales

- [ ] Al arrastrar un `.obj` desde el Asset Browser al Viewport, aparece una entidad con el modelo importado posicionada bajo el cursor (tile picking del Hito 6).
- [ ] AssetBrowser muestra sección "Meshes" con los archivos encontrados en `assets/meshes/`.
- [ ] Inspector muestra sección `MeshRenderer` con dropdown del mesh y metadata (submeshes, vertices).
- [ ] `Archivo > Guardar` persiste las entidades no-tile en el `.moodmap` con su `MeshRendererComponent`. `Abrir` las recrea idénticas.

---

## Bloque 0 — Pendientes arrastrados del Hito 9

- [ ] Drag & drop de audio desde AssetBrowser a entidades (solo si sirve al flujo "arrastrar asset al Viewport" de este hito).

## Bloque 1 — Dependencia assimp

- [ ] CPMAddPackage para `assimp` (último tag, ~5.4.x). Opciones que nos interesan: disable por default los formatos raros (WAV, BVH, etc.) para bajar binary size; habilitar OBJ + glTF + FBX.
- [ ] `target_link_libraries(MoodEditor PRIVATE assimp::assimp)`. Mismo para `mood_tests` si aparece algún test que use assimp directo.

## Bloque 2 — MeshAsset y MeshLoader

- [ ] `src/engine/render/MeshAsset.h`: struct liviano `{ logicalPath, vector<SubMesh> }` donde `SubMesh { ibuf, vbuf, materialIdx, vertexCount }`. SubMesh owns los GL handles via un `std::unique_ptr<IMesh>`.
- [ ] `src/engine/assets/MeshLoader.{h,cpp}`: función libre `loadMeshWithAssimp(path) -> std::unique_ptr<MeshAsset>`. Usa `aiImportFile` con `aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace`.
- [ ] Ampliar `AssetManager` con `MeshAssetId` + `loadMesh(logicalPath) -> MeshAssetId` + `getMesh(id) -> MeshAsset*` (misma receta que texturas/audio).
- [ ] Fallback: si assimp falla, log warn y devolver el "missing mesh" (cubo del Hito 3 precompilado en el manager).

## Bloque 3 — MeshRendererComponent evolucionado

- [ ] `MeshRendererComponent` pasa de `IMesh* mesh + TextureAssetId texture` a `MeshAssetId mesh + vector<TextureAssetId> materials` (1 textura por submesh + fallback a slot 0 si corto).
- [ ] Migrar el código existente que setea `MeshRendererComponent` en `rebuildSceneFromMap` a usar `MeshAssetId` del cubo (crear un `MeshAsset` built-in del cubo en lugar del `IMesh*` directo).
- [ ] Actualizar `renderSceneToViewport`: el loop scene-driven del Hito 8 se refactoriza para iterar submeshes y cambiar textura por submesh.

## Bloque 4 — Scene authoritative en render

- [ ] Render actual: loop por grid (Hito 4) + loop scene-driven filtrando `Tile_` (workaround Hito 8). Objetivo: **un solo loop** scene-driven.
- [ ] `rebuildSceneFromMap` sigue siendo la única fuente de tiles (crea entidades con `Tile_X_Y` tag); el renderer no distingue tiles de modelos importados.
- [ ] Drop de textura ahora edita el `MeshRendererComponent` de la entidad tile en vez de `GridMap.setTile` + rebuild. Menos lag, selección directa.

## Bloque 5 — AssetBrowser + drag & drop de mesh

- [ ] AssetBrowser sección "Meshes": lista `.obj`/`.gltf`/`.fbx` en `assets/meshes/`.
- [ ] Drag payload nuevo `"MOOD_MESH_ASSET"`. Drop al Viewport crea una nueva entidad en el tile bajo el cursor con `TransformComponent(tile_center)` + `MeshRendererComponent(meshId, default_material)`.
- [ ] Drop sobre una entidad existente (via Hierarchy) reemplaza su mesh.

## Bloque 6 — Serialización Scene (primer pase)

- [ ] `SceneSerializer` extendido: además de tiles, serializa entidades no-tile (filtradas por prefijo de tag `Tile_`). Schema añade sección `entities: [{ tag, transform, mesh_renderer: { mesh_path, materials[] } }]`.
- [ ] Bumpear `k_MoodmapFormatVersion` de 1 a 2. Load de v1 sigue funcionando (sin entidades custom).
- [ ] Test: crear escena con cubo tile + modelo importado → save → load → comparar.

## Bloque 7 — Tests

- [ ] `tests/test_mesh_asset.cpp`: carga de un `.obj` simple desde memoria o `assets/meshes/test_triangle.obj` (generado programáticamente si no existe), verificar submesh count + vertex count.
- [ ] `tests/test_scene_serializer.cpp` ampliado: round-trip con entidades no-tile.

## Bloque 8 — Cierre

- [ ] Recompilar, tests verdes, demo visual (importar un modelo pequeño tipo Suzanne y verlo rotar).
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.10.0-hito10` + push.
- [ ] Crear `docs/PLAN_HITO11.md` (iluminación Phong/Blinn-Phong).

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

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 11 o posterior

_(llenar al cerrar el hito)_
