# Plan — F2H12: CSG operaciones booleanas

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H11 cerrado),
> `PLAN_FASE2.md` sección 4 (este hito era F2H10 plan original = "CSG
> operaciones booleanas"; renumerado a F2H12 por la cadena de swaps).

---

## Objetivo

Implementar las **3 operaciones booleanas** clásicas entre brushes
CSG: **Subtract**, **Union**, **Intersect**. Approach destructivo
estilo Hammer/TrenchBroom: las ops reemplazan los brushes input por
el resultado (1-N brushes convexos), commit definitivo, undoable
via `HistoryStack`.

**Lo que entra en F2H12**:
- Algoritmo de plane clipping (~300 LOC) en `engine/world/csg/`.
- 3 funciones puras testeables: `subtract`, `unionOp`, `intersectOp`.
- Integración con UI: menú **Mapa > Boolean > {Subtract, Union, Intersect}** con multi-selección de 2 entidades brush.
- Undo/redo via comandos.
- Tests pesados con casos sintéticos (cubo - cubo desplazado, cubos disjuntos, contención total, etc.).

**Lo que NO entra en F2H12**:
- Booleanos no-destructivos (modifier stack estilo Blender) — diferido como hito futuro si emerge necesidad.
- Booleanos de >2 brushes en una sola operación (cascadear es responsabilidad del user).
- Primitivas extendidas (cilindro, esfera, etc.) — F2H13.
- Texturizado per-cara — F2H14.

---

## Filosofía de diseño

- **Destructivo, undoable**: matchea el workflow de Hammer/TrenchBroom. El user selecciona A y B, aplica la op, los brushes input desaparecen y aparecen los pedazos resultantes. Si no le gusta, **Ctrl+Z** los restaura.
- **Convexos por construcción**: cada brush resultante es la intersección de half-spaces — siempre convexo. El resultado de un boolean entre dos brushes convexos puede ser cóncavo, así que se descompone en N brushes convexos.
- **Algoritmo a mano** (igual que F2H11): plane clipping puro, sin libs externas. Documentado en TrenchBroom source y en el libro de Ericson.
- **Tipos puros, sin OpenGL ni ImGui**: todo el subsystem `engine/world/csg/` sigue siendo testeable headless con doctest.
- **Tolerancia geométrica unificada**: reusar `kPlaneEpsilon = 1e-4f` de F2H11 para tests de coincidencia de planos / "vértice está dentro del brush".

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Carpeta del subsystem | `src/engine/world/csg/` (mismo header `BrushOps.h` para las 3 ops) |
| 2 | Tipo de retorno | `std::vector<Brush>` (0-N brushes resultantes; N=0 si vacío, N=1 si simple, N>1 si descomposición convexa) |
| 3 | Modelo de la op | Destructivo: `subtract(A, B) -> [pedazos de A fuera de B]`. B no se toca (sigue siendo el "tool brush"). |
| 4 | Algoritmo Subtract | Para cada plano `P_i` de `B`, generar `A_i = A ∪ {plano-complementario de P_i}` y conservar si tiene volumen. Reemplazar A con `A ∩ P_i` en la siguiente iteración. Approach clásico Quake/Hammer. |
| 5 | Algoritmo Intersect | Construir brush con `A.faces ∪ B.faces`, computar AABB, validar que tiene volumen (al menos 4 vertices únicos del brush). Si no → resultado vacío. |
| 6 | Algoritmo Union | Si `A ⊆ B` → `[B]`. Si `B ⊆ A` → `[A]`. Si disjuntos → `[A, B]`. Sino → `(A subtract B) ∪ {B}`. |
| 7 | Validación de volumen | Helper `bool isBrushValid(const Brush&)` que cuenta vertices únicos del brush via tripletes; >= 4 vertices con `buildBrushMesh` no degenerado = válido. |
| 8 | Comandos undo/redo | `BooleanOpCommand` que captura snapshot pre-op (los brushes A, B + sus tags + transforms) y aplica/revierte spawneando entidades. Reusa `pushCreatedEntities`/`pushDeletedEntities` del `HistoryStack`. |
| 9 | UI | Menú **Mapa > Boolean** con 3 items habilitados solo si hay exactamente 2 entidades seleccionadas con `BrushComponent`. Atajos opcionales: `Ctrl+Shift+S` (subtract), `Ctrl+Shift+U` (union), `Ctrl+Shift+I` (intersect). |
| 10 | Material del resultado | Hereda del brush "base" (A en subtract; el primero seleccionado en union/intersect). En F2H14 esto pasa a per-cara con preservación del material original de cada cara. |

---

## Tipos públicos del subsystem

```cpp
// engine/world/csg/BrushOps.h
namespace Mood::Csg {

/// @brief True si el brush tiene volumen (>= 4 vertices unicos
///        producidos por intersect-de-tripletes filtrando puntos
///        fuera del brush). Usado para descartar brushes
///        degenerados que las ops booleanas pueden producir
///        (ej: A subtract B donde B contiene a A devuelve [] sin
///        crashear).
bool isBrushValid(const Brush& b);

/// @brief A subtract B: pedazos de A que estan fuera de B.
///        Algoritmo: para cada plano P_i de B, generar el brush
///        A_i = A ∪ {plano complementario de P_i} (recortando con
///        los planos previos). Cada A_i valido se conserva.
///        Cardinalidad del resultado:
///        - 0 si B contiene a A.
///        - 1 si A y B son disjuntos (devuelve copia de A).
///        - N > 1 si B se mete parcialmente en A (descomposicion
///          convexa de A \ B).
std::vector<Brush> subtract(const Brush& A, const Brush& B);

/// @brief A intersect B: el brush convexo formado por la
///        interseccion de los half-spaces de ambos. Si la
///        interseccion es vacia (no se tocan) o degenerada,
///        devuelve nullopt.
std::optional<Brush> intersectOp(const Brush& A, const Brush& B);

/// @brief A union B: descomposicion convexa de A ∪ B.
///        - 1 brush si uno contiene al otro.
///        - 2 brushes si son disjuntos.
///        - N > 2 si overlap parcial: (A subtract B) ∪ {B}.
std::vector<Brush> unionOp(const Brush& A, const Brush& B);

} // namespace Mood::Csg
```

---

## Bloques

### A — Plan F2H12 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Helper `isBrushValid` + tests

Crear:
- `src/engine/world/csg/BrushOps.h` (decl).
- `src/engine/world/csg/BrushOps.cpp` (impl de `isBrushValid` solo).
- `tests/test_csg_brush_ops.cpp` (5-7 cases iniciales):
  - Box unitaria valida.
  - Brush con < 4 caras invalido.
  - Brush con caras coplanares duplicadas invalido.
  - Brush con planos contradictorios (interseccion vacia) invalido.
  - Box trasladada/rotada/escalada sigue valida.

Compilar + tests verdes. Commit:
`feat(F2H12 Bloque B): isBrushValid + tests`

### C — `subtract` + tests pesados

Implementar el algoritmo de plane clipping. Casos a cubrir en
`test_csg_brush_ops.cpp` (~12-15 cases):
- A subtract B donde B esta completamente fuera de A → `[A]` (copia, 1 brush).
- A subtract B donde B contiene completamente a A → `[]` (vacio).
- A subtract B donde B = A → `[]`.
- A subtract B donde B se mete por una esquina → 3 brushes resultantes (descomposicion en 3 piezas).
- A subtract B donde B atraviesa A (un lado a otro) → 2 brushes (los dos lados).
- A subtract B donde B es un "disco" mas chico en el centro de A → resultado en forma de "anillo" descompuesto en N brushes.
- Validacion semantica end-to-end: `union de mesh(resultado) + mesh(B) ≅ mesh(A)` (volumen conservado mod tolerance).
- Determinismo bit-a-bit.

Commit: `feat(F2H12 Bloque C): subtract via plane clipping + tests`

### D — `intersectOp` + tests

Implementar la interseccion convexa pura. Tests (~6-8 cases):
- A ∩ B donde B contiene a A → resultado = A (copia).
- A ∩ B donde A contiene a B → resultado = B (copia).
- A ∩ B disjuntos → `nullopt`.
- A ∩ B parcial → brush convexo con caras de ambos (subset).
- Conmutatividad: `intersect(A, B) == intersect(B, A)` (mod orden de caras).
- isBrushValid del resultado siempre true cuando hay valor.

Commit: `feat(F2H12 Bloque D): intersectOp + tests`

### E — `unionOp` + tests

Implementar la union via `subtract + glue`. Tests (~8-10 cases):
- A ∪ B disjuntos → `[A, B]` (2 brushes).
- B contiene a A → `[B]`.
- A contiene a B → `[A]`.
- A ∪ B overlap parcial → N brushes con suma de volumen ≅ vol(A) + vol(B) - vol(A ∩ B).
- Conmutatividad sintactica con tolerancia (resultado puede tener distinto orden pero misma union).
- Validacion: cada brush resultante es valido (isBrushValid).

Commit: `feat(F2H12 Bloque E): unionOp + tests`

### F — UI + comando undoable

- `BooleanOpCommand` en `src/editor/commands/`. Captura snapshot
  de los brushes input + sus tags + sus transforms; en `execute`
  destruye los inputs y crea entidades para cada brush resultante;
  en `undo` revierte (destruye los nuevos, recrea los originales).
- Multi-selection: el editor ya tiene `m_ui.selectedEntities()`
  (Hito 32). Usar para chequear que hay exactamente 2 entidades
  con BrushComponent.
- Menú `Archivo > Mapa > Boolean`:
  - "Subtract (A − B)": A = primero seleccionado, B = segundo.
  - "Union (A ∪ B)".
  - "Intersect (A ∩ B)".
  - Items disabled si no hay 2 brushes seleccionados.
  - Tooltip explicando A vs B.
- Atajos opcionales: ver decision arquitectonica #9 (evaluar
  conflicto con atajos existentes — si hay, omitir y dejar solo
  menú).

Commit: `feat(F2H12 Bloque F): UI - menu Boolean + comandos undoable`

### G — Cierre + tag

- `docs/HITOS.md`: F2H12 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H12 cerrado, F2H11 movido a "anterior".
- `docs/DECISIONS.md` con fecha 2026-05-XX:
  - "CSG booleanos destructivos (no modifier stack) — rationale: matchea workflow Hammer/TrenchBroom, simple, undoable, suficiente para v1".
  - "Algoritmo plane clipping a mano: Subtract via half-space carving, Intersect via union de planos, Union via complemento + B".
- Archivar `docs/PLAN_HITO_F2H12.md` a `docs/archive/plans/`.
- Tag: `v1.3.0-fase2-hito12`.

Commit final: `docs(F2H12 Bloque G): cierre + archivar plan`

---

## Suite

Cases nuevos esperados:
- `test_csg_brush_ops.cpp`: ~30-35 cases, ~100-150 asserts.

**Suite estimada: 447 → ~480 cases / 7562 → ~7700 asserts.**

---

## Riesgos

- **Robustez numérica**: brushes con caras casi paralelas pueden
  producir resultados numéricamente inestables (vertices con
  signedDistance ≈ kEpsilon). Mitigación: tests específicos con
  brushes "casi tocándose" + escalas extremas (0.01, 100). Si
  emerge problema duro, agregar pasada de "snap vertices to
  shared planes within kEpsilon".
- **Explosión combinatoria**: `subtract` con un brush B de N caras
  puede producir hasta N brushes resultantes. Para B con 6 caras
  (Box) son hasta 6 brushes, manejable. Si después de F2H13 alguien
  hace subtract con un cilindro de 32 caras, son hasta 32 brushes
  por op. Documentar como expected behavior; F2H16 (compilación
  unificada) lo absorbe en una mesh estática al guardar.
- **Performance**: cada op tiene complejidad O(N_A × N_B²)
  aproximada (por cada plano de B, validar todos los vertices de
  A). Para brushes con <20 caras es submilisegundo. Si emerge
  caso patológico, profile y optimizar.
- **Edge case "B tangencial a una cara de A"**: el plano de B
  coincide con un plano de A (mod kEpsilon). El algoritmo debe
  detectar y descartar el plano duplicado para no producir un
  brush degenerado. Mitigación: filter explícito en `subtract`
  que skipea planos coincidentes con caras de A.
- **Multi-selección no expone el orden**: el editor guarda un
  set/vector ordenado por tiempo de selección. Documentar en el
  menú que A = primero, B = segundo. Si surge confusión, agregar
  un swap visible en el dialog.

---

## Criterios de cierre

- [ ] `src/engine/world/csg/BrushOps.h` y `.cpp` con
      `isBrushValid`, `subtract`, `intersectOp`, `unionOp`. Cero
      includes de OpenGL/ImGui.
- [ ] `tests/test_csg_brush_ops.cpp` con ~30+ cases verde.
- [ ] `BooleanOpCommand` en `src/editor/commands/` con
      execute/undo correctos. Tests si emerge necesidad (la suite
      de comandos existente sirve de baseline).
- [ ] Menú `Archivo > Mapa > Boolean > {Subtract, Union, Intersect}`
      activo cuando hay 2 brushes seleccionados.
- [ ] Validación visual end-to-end: spawn 2 brushes, hacer
      subtract, ver el "hueco" en el viewport. Ctrl+Z restaura los
      originales.
- [ ] Suite ~480+ cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado a `docs/archive/plans/`.
- [ ] Tag `v1.3.0-fase2-hito12` pusheado.

---

## Refs canónicas

- "Real-Time Collision Detection" (Christer Ericson, 2005) — cap 8 sobre BSP / convex polyhedra clipping.
- TrenchBroom source: https://github.com/TrenchBroom/TrenchBroom — `common/src/Model/BrushBuilder.cpp` y `common/src/Model/Brush.cpp` para subtract / intersect / union.
- Quake source (id Software, 1996) — `qbsp` source con la implementación canónica de plane clipping.
