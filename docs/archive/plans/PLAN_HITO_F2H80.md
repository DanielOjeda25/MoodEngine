# PLAN HITO F2H80 — Miniaturas 3D en "Crear Entidad" + Asset Browser

> **Estado**: en ejecución (alcance aprobado por el dev: Crear Entidad + Asset Browser).
> **Sub-fase**: 2.7 (UI/UX final + cierre Fase 2). Quinto hito de la sub-fase.
> **Predecesor**: F2H79 (pulido de modales + Welcome).

## Qué siente el usuario

Cuando abre **+ Crear Entidad → Meshes del proyecto**, en vez de una lista de
texto (`ruta (id 7)`) ve una **grilla de cards con una miniatura 3D del modelo
real** — como en Source Filmmaker / el Content Browser de Unreal. Reconoce el
auto, el personaje o el prop **de un vistazo**, sin leer nombres. La misma
grilla de miniaturas aparece en el **Asset Browser** (sección de meshes).

## Estado de partida (auditoría)

- `MaterialPreviewRenderer` (`src/engine/render/preview/`): renderiza una
  **esfera con un material** a un FBO LDR (256²), reusando el shader PBR +
  IBL del `SceneRenderer`. Patrón a imitar — pero necesitamos renderizar un
  **mesh arbitrario** (todos sus submeshes con sus materiales), no la esfera.
- Modal Crear Entidad (`EditorProjectActions_CreateEntity_PickModal.cpp`): tab
  "Meshes" lista `meshIds [1..count)` con `ImGui::Selectable` (texto plano).
- Asset Browser (`AssetBrowserPanel`): sección de meshes (a auditar su layout).
- `MeshAsset`: `submeshes[]` (cada uno `IMesh` + `materialIndex`), `aabbMin/Max`
  (mesh-space, para encuadrar la cámara), `importRotationEuler`.
- `AssetManager::createMaterialsForMesh(id) -> vector<MaterialAssetId>`.
- Wiring de referencia: `EditorApplication` posee `m_materialPreview`, le pasa el
  IBL del `SceneRenderer` en `_Init`, y lo inyecta al `MaterialEditorPanel` con
  `setPreviewRenderer`.

## Bloques

### A — `MeshThumbnailRenderer` (engine/render/preview/)
Renderer que produce una **miniatura cacheada por mesh**. Diferencias clave vs.
`MaterialPreviewRenderer`:
- Renderiza **el mesh real**: itera `submeshes`, bindea el material de cada uno
  (`materialIndex → createMaterialsForMesh`) y dibuja. Reusa el setup PBR
  (directional 3/4 + IBL opcional + SSBOs vacíos Forward+).
- **Cámara encuadrada al AABB**: distancia desde el radio de la bounding sphere
  + ángulo 3/4 (frente-arriba-derecha) para que se lea el volumen. Aplica
  `importRotationEuler` para que el modelo salga derecho.
- **Cache `meshId → OpenGLFramebuffer`** (textura persistente por mesh): se
  renderiza **una sola vez** (lazy, al primer pedido) y se reusa. **Sin
  animación** (una grilla de N thumbnails rotando sería distractora y cara).
  Tamaño chico (128² default) por costo de memoria.
- API: `GLuint thumbnailFor(MeshAssetId, AssetManager&)` (0 si no se pudo);
  `setIblTextures(...)`; `clear()`/`invalidate(id)` para el futuro (re-import).

### B — Wiring (ownership + IBL)
`EditorApplication` posee `m_meshThumbnails` (creado en `_Init`, IBL del
`SceneRenderer`, igual que `m_materialPreview`). Lo usa el modal Crear Entidad
(vive en `EditorApplication`) y se inyecta al `AssetBrowserPanel` vía setter
(`setThumbnailRenderer`). Render de thumbnails ocurre on-demand dentro del frame
del editor (hay contexto GL).

### C — Grilla en el modal Crear Entidad (tab "Meshes")
Reemplazar el `Selectable` por una **grilla de cards**: por cada mesh no
sintético, un `ImGui::ImageButton` con la miniatura + label (nombre del archivo)
debajo. Click/doble-click spawnea (misma lógica de spawn ya existente). Layout
responsivo (N columnas según ancho). Mantener el footer (Importar / Nuevo vacío)
y la altura uniforme de tabs.

### D — Grilla en el Asset Browser (meshes)
Aplicar la misma card-grid a la sección de meshes del `AssetBrowserPanel`,
reusando el `MeshThumbnailRenderer`. Mantener selección/single-select existente.

### E — Cierre
`docs/hitos/F2H80.md`, one-liner en `HITOS.md`, sección 0.1 de `ESTADO_ACTUAL.md`,
nota en `DECISIONS.md` (mesh-thumb cache: render-once vs. animado), archivar este
plan. Tag `v1.71.0-fase2-hito80`.

## Decisiones / riesgos

1. **Render-once cacheado, no animado**: una grilla con muchas miniaturas
   rotando es cara (N draws/frame) y distrae. Se renderiza una vez a un ángulo
   3/4 fijo. (El preview animado del Material Editor sigue como está — es 1 sola
   esfera con foco.)
2. **FBO por mesh**: cada miniatura es su propia textura persistente (no se puede
   compartir 1 FBO como el material preview, que es 1 a la vez). Costo: 128² LDR
   por mesh cacheado. Aceptable; `clear()` disponible si crece.
3. **Materiales del mesh**: se usan los `createMaterialsForMesh` (mismos que al
   spawnear) → la miniatura coincide con cómo se verá la entidad.
4. **Clase nueva vs. extender MaterialPreviewRenderer**: clase nueva para no
   tocar el preview de materiales (que anda) y porque la lógica de cache +
   camera-fit + multi-submesh es distinta. Algo de duplicación del setup PBR es
   aceptable (cada archivo < 350 LOC).
5. **Riesgo IBL no cargado al render**: si el IBL no está listo cae al ambient
   escalar (igual que el material preview). Si las miniaturas salen planas, se
   re-setea el IBL antes de cada render batch.

## Orden
A → B → C → validación rápida (Crear Entidad) → D → validación (Asset Browser) → E.
