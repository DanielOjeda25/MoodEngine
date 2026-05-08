# Plan — F2H26: Runtime-load de mesh compilada en MoodPlayer

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "En curso" ítem 2,
> `DECISIONS.md` entries de F2H20 (compileMap) + F2H25 (cull overlap).

---

## Objetivo

Habilitar que el `MoodPlayer` (runtime, no editor) cargue una **mesh
estática unificada precompilada** del mapa en lugar de procesar los
brushes CSG individuales. Esto realiza la idea original del plan F2H14:
**"brushes solo en el editor"** — son herramienta de autoría; la entrega
final es la mesh estática.

**Beneficios**: loading más rápido (sin overhead CSG), menos memoria
(sin estructuras intermedias), mesh ya con cull aplicado de F2H20+F2H25.

---

## Filosofía de diseño

- **Schema bump aditivo v12→v13**: campo `compiledMesh` opcional. v12
  sigue siendo legible (cae al fallback "procesar brushes en runtime").
- **Editor sigue editando brushes**: no usa `compiledMesh` para render
  — sigue procesando `BrushComponent` con `buildBrushMesh`. La mesh
  compilada es OUTPUT del editor, INPUT del player.
- **Player skipea brushes si hay compiled**: no instancia
  `BrushComponent` en su scene. Crea UNA entity "WorldMesh" con un
  componente nuevo `CompiledMeshComponent` que carga la mesh
  precompilada al GPU.
- **Inline en JSON**: persistir vertices+indices dentro del `.moodmap`
  (mismo archivo). Texto, depurable, editable. Peso aumenta pero
  aceptable (~ N×32 bytes/vertex en JSON).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Schema | v12→v13. `compiledMesh` opcional al top-level del JSON. |
| 2 | Cuándo compilar | Editor llama `compileMap` al `SceneSerializer::save` y persiste el resultado. Brushes individuales también se persisten (para que el editor pueda re-editar). |
| 3 | Player render path | Nuevo componente `CompiledMeshComponent { vector<unique_ptr<IMesh>> meshes, vector<MaterialAssetId> materials }`. SceneRenderer agrega `compiledMeshPass`. |
| 4 | Modo Editor vs Player | Flag `bool useCompiledMesh` en `SceneLoader::applyEntitiesToScene`. Editor pasa `false` (sigue creando BrushComponent), Player pasa `true` (crea CompiledMeshComponent si existe). |
| 5 | Fallback | Si v13 sin `compiledMesh` o si `useCompiledMesh=false`, comportamiento idéntico a v12. |
| 6 | Tests | Round-trip schema v13; Player simula con flag → 1 entity con CompiledMeshComponent en vez de N BrushComponents. |
| 7 | Stats serializadas | Sí — para diagnóstico (`stats.totalTriangles` queda visible). |

---

## Bloques

### A — Plan F2H26 (este archivo)

### B — Schema v13 + persistencia
- `JsonHelpers.h`: bump `k_MoodmapFormatVersion = 13` + comentario.
- `SceneSerializer.h`: structs `SavedCompiledSubmesh` / `SavedCompiledMesh`. Agregar `std::optional<SavedCompiledMesh> compiledMesh` a `SavedMap`.
- `SceneSerializer.cpp`: serializar / parsear el campo nuevo. Helpers para `vertices` y `indices` arrays.

### C — `CompiledMeshComponent` + render path
- `engine/scene/components/CompiledMeshComponent.h`: nuevo struct, move-only (igual patrón que BrushComponent).
- `SceneRenderer.cpp`: agregar `compiledMeshPass` o extender el flow PBR.

### D — SceneLoader con flag editor/player
- Firma nueva: `applyEntitiesToScene(savedMap, scene, assets, bool useCompiledMesh = false)`.
- Si `useCompiledMesh && savedMap.compiledMesh.has_value()`: crear entity con `CompiledMeshComponent` y skipear el loop de brushes.
- Sino: comportamiento actual.

### E — Editor escribe + Player lee
- `EditorApplication`: en `handleSave` (o el flow del save), llamar `compileMap` y poblar `SavedMap.compiledMesh` antes de `SceneSerializer::save`.
- `PlayerApplication_Init`: pasar `useCompiledMesh=true` a `applyEntitiesToScene`.

### F — Tests
- Round-trip schema v13: save+load preserva `compiledMesh`.
- Editor mode: `useCompiledMesh=false` ignora compiledMesh y crea BrushComponents.
- Player mode: `useCompiledMesh=true` con compiledMesh → 1 entidad WorldMesh.
- Player mode: `useCompiledMesh=true` SIN compiledMesh (v12 legacy) → fallback brushes.

### G — Cierre (junto con F2H25 + F2H27)
Commits agrupados al final.

---

## Lo que NO entra

- Persistencia binaria (formato propio `.compiledmap`). Diferido.
- Streaming/load chunks por sector. Diferido.
- Recompilación incremental (cambiar 1 brush → recompilar solo lo afectado). Diferido.
- F6 panel — F2H27.
- Hammer 4-viewport — F2H28.
