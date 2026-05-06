# Plan — F2H14: Primitivas extendidas

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H13 cerrado),
> `PLAN_FASE2.md` sección 4. Este hito **era F2H13 en el plan
> original** (primitivas extendidas), renumerado +1 por el adelanto
> de multi-selección que entró como F2H13 efectivo. F2H15-F2H17
> se renumeran +1 cada uno también.

---

## Objetivo

Agregar **5 primitivas nuevas** al subsystem CSG: cilindro, prisma
(triangular y hexagonal), esfera poliédrica, pirámide y wedge.
Cada una se expone como helper libre que devuelve un `Csg::Brush`
construido con la misma convención de F2H11 (planos hacia afuera,
`localAabb` computada). Todas funcionan con las ops booleanas
existentes sin cambios al algoritmo CSG core.

**Lo que entra en F2H14**:
- Helpers libres en `engine/world/csg/`: `makeCylinderBrush`,
  `makePrismBrush`, `makeSphereBrush`, `makePyramidBrush`,
  `makeWedgeBrush`.
- Tests unitarios de cada primitiva (forma canónica, transformaciones,
  ops booleanas representativas).
- Entrada de menú **Archivo > Mapa > Añadir Brush > [...]** con 7
  items: Box (existente), Cylinder, Sphere, Pyramid, Wedge,
  Prism Triangular, Prism Hexagonal.
- Defaults razonables para parámetros (segments, sides, etc.).

**Lo que NO entra en F2H14**:
- Diálogo modal con parámetros editables (cilindro con 32 segments,
  esfera con 16, etc.) — F2H14 usa defaults fijos. Si emerge
  necesidad real, hito propio con UI dedicada.
- Texturizado per-cara (F2H15 = era F2H14 original).
- Edición post-spawn de la geometría (mover una cara individual
  sin tocar el transform) — F2H16 (era F2H15: face mode).
- Visgroups / layers — F2H17 (era F2H16).

---

## Filosofía de diseño

- **Primitivas como datos puros, no como clases**: cada primitiva
  es una función `make*Brush(matrix, params...)` que devuelve un
  `Csg::Brush` standalone. Sin herencia, sin polimorfismo. Mismo
  patrón que `makeBoxBrush` de F2H11. La primitiva es solo el
  generador inicial; después es un brush "más" para todo el resto
  del sistema.
- **Convexos por construcción**: cada primitiva es un poliedro
  convexo (intersección de half-spaces). Esfera = aproximación
  icosaédrica convexa. Cilindro = prisma de N caras laterales.
  Pirámide = N caras laterales convergentes + base.
- **Cero cambios al algoritmo CSG core**: las ops booleanas
  (`subtract`, `unionOp`, `intersectOp`) ya operan sobre cualquier
  `Brush` válido. Las primitivas nuevas heredan toda esa
  funcionalidad sin tocar `BrushOps.cpp`.
- **Persistencia transparente**: el schema `.moodmap` v10 (F2H11)
  guarda planos genéricamente. Una primitiva persistida es un
  brush con N planos. **Cero schema bump**.
- **Defaults razonables, no parametrizables en F2H14**: cilindro
  16 segments, esfera 12 segments. Si el dev quiere un cilindro
  de 32 segments, edita los planos del brush en F2H16 (face mode)
  o agregar diálogo de params como hito futuro.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Carpeta del subsystem | `src/engine/world/csg/Primitives.h` y `.cpp` (separado de `Brush.h` que ya tiene `makeBoxBrush`) |
| 2 | Convención del bounding | Todas las primitivas viven en una "unit cell" `[-0.5, +0.5]^3` antes de aplicar `worldFromLocal`. Cylinder/Sphere/Pyramid/Wedge encajan en ese AABB; el `worldFromLocal` lo escala/rota/traslada como Box. |
| 3 | Cylinder default | 16 segments (laterales) + 2 caps. AABB unitario; eje Y es la altura, lados radiales en plano XZ. |
| 4 | Cylinder geometría | N planos laterales con normales radiales (cos(θ), 0, sin(θ)) en el círculo XZ; 2 caps con normales (0,+1,0) y (0,-1,0). |
| 5 | Sphere default | 12 segments → aproximación icosaédrica con 12 caras planas. Más segments emerge como hito futuro si emerge necesidad. |
| 6 | Sphere geometría | 12 caras de un dodecaedro inscripto en una esfera de radio 0.5 (aproximación más cercana al volumen real). Más uniforme que UV-sphere para CSG. |
| 7 | Pyramid default | Pirámide cuadrada (4 caras laterales + base). Punta en `(0, +0.5, 0)`, base en `y=-0.5` con vertices `(±0.5, -0.5, ±0.5)`. |
| 8 | Pyramid geometría | 4 planos laterales con normales angulares hacia afuera; 1 cap inferior `(0, -1, 0)`. |
| 9 | Wedge geometría | "Rampa" cuadrada: 2 triángulos verticales (caras laterales en planos `x=±0.5`), 1 cap base `(0,-1,0)`, 1 cap atrás `(0,0,-1)`, y 1 plano inclinado conectando la arista superior `y=+0.5, z=-0.5` con la inferior `y=-0.5, z=+0.5` (normal hacia arriba-adelante). 5 planos total. |
| 10 | Prism Triangular | 3 planos laterales radiales (120° entre cada uno) + 2 caps top/bottom. Equivale a un cylinder con 3 segments. |
| 11 | Prism Hexagonal | 6 planos laterales radiales (60° entre cada uno) + 2 caps top/bottom. Equivale a un cylinder con 6 segments. |
| 12 | Material por defecto | 0 (look "blank gris" del F2H11). Per-cara ya posible en F2H11 pero no editable hasta F2H15 (UV editor). |
| 13 | Tag generation en spawn | `Brush_Cyl_NN`, `Brush_Sph_NN`, `Brush_Pyr_NN`, `Brush_Wed_NN`, `Brush_PrTri_NN`, `Brush_PrHex_NN` (matchea el patrón `Brush_Box_NN` de F2H11). |

---

## Tipos públicos

```cpp
// engine/world/csg/Primitives.h
namespace Mood::Csg {

/// @brief Brush cilindrico: N planos laterales radiales + 2 caps.
///        La unit cell esta en [-0.5, +0.5]^3 con eje Y como altura.
///        Default 16 segments (matchea TrenchBroom).
Brush makeCylinderBrush(const glm::mat4& worldFromLocal,
                         u32 segments = 16,
                         u32 materialIndex = 0);

/// @brief Brush esfera poliedrica (dodecaedro inscripto en esfera
///        unitaria). 12 caras planas — aproximacion convexa.
Brush makeSphereBrush(const glm::mat4& worldFromLocal,
                       u32 materialIndex = 0);

/// @brief Brush piramide cuadrada: 4 caras laterales convergentes
///        a la cima (0, +0.5, 0) + base en y=-0.5.
Brush makePyramidBrush(const glm::mat4& worldFromLocal,
                        u32 materialIndex = 0);

/// @brief Brush "wedge" (rampa): 5 planos. Util para escaleras /
///        rampas / techos inclinados sin booleanos.
Brush makeWedgeBrush(const glm::mat4& worldFromLocal,
                      u32 materialIndex = 0);

/// @brief Brush prisma N-gonal: N planos laterales radiales + 2 caps.
///        N=3 (triangular), N=6 (hexagonal) son los casos esperados;
///        cualquier N>=3 es valido. Equivale a cylinder con N segments.
Brush makePrismBrush(const glm::mat4& worldFromLocal,
                      u32 sides,
                      u32 materialIndex = 0);

} // namespace Mood::Csg
```

---

## Bloques

### A — Plan F2H14 (este archivo)

Documento del hito. Nace en `docs/`; al cierre se mueve a
`docs/archive/plans/`.

### B — `makeCylinderBrush` + `makePrismBrush` + tests

Crear:
- `src/engine/world/csg/Primitives.h` (decls).
- `src/engine/world/csg/Primitives.cpp` (`makeCylinderBrush` y
  `makePrismBrush` — comparten ~95% del código, factorizar a un
  helper privado `buildPrismaticBrush(matrix, sides, mat)`).
- `tests/test_csg_primitives.cpp` (~12-15 cases iniciales):
  - Cylinder default (16 segments) → 18 caras (16 laterales + 2 caps).
  - Cylinder con segments=4 → 6 caras (igual a un cubo rotado 45°).
  - AABB del cylinder default ≅ unit cube `[-0.5, +0.5]^3` (con
    tolerancia para la aproximación radial).
  - Las normales laterales son unitarias y apuntan hacia afuera.
  - Cylinder + transform → AABB se mueve / escala correctamente.
  - `buildBrushMesh(cylinder)` produce mesh válido con N×2+top+bottom
    polígonos triangulados.
  - Prism Triangular → 5 caras (3 lat + 2 caps).
  - Prism Hexagonal → 8 caras (6 lat + 2 caps).
  - Cylinder ⊂ subtract test: cylinder dentro de box → box con
    "agujero" cilíndrico (resultado N convexos).

Validar: tests verdes.

### C — `makeSphereBrush` + tests

Crear `makeSphereBrush` (dodecaedro inscripto). 12 caras planas —
elegir orientación canónica de un dodecaedro regular escalado a
unit cell.

Tests (~5-7 cases):
- Default: 12 caras, AABB ≅ unit cube.
- Normales unitarias y apuntando hacia afuera.
- `isBrushValid(sphere)` → true.
- `buildBrushMesh(sphere)` produce mesh con 12 polígonos pentagonales
  triangulados.
- Sphere ∩ Box (overlap parcial) → brush válido.

### D — `makePyramidBrush` + `makeWedgeBrush` + tests

Crear `makePyramidBrush` (4 lat + 1 base) y `makeWedgeBrush` (5
caras). Geometría a mano (matrix multiplicación de coordenadas
canónicas).

Tests (~8-10 cases):
- Pyramid: 5 caras, vertice cima en `(0, 0.5, 0)`, base cuadrada.
- Pyramid: las 4 caras laterales convergen a la cima (normales
  apuntan hacia afuera de la pirámide).
- Wedge: 5 caras, vertices canónicos del wedge (ver decision #9).
- Wedge: el plano inclinado tiene normal `(0, +y, -z)` normalizada
  hacia arriba-adelante.
- Pyramid + Wedge en transformaciones rotadas / escaladas / etc.
- Subtract Pyramid de Box → resultado válido (no degenerado).

### E — UI: menu "Añadir Brush"

- Reemplazar `Archivo > Mapa > Añadir Box Brush` con un submenu
  `Archivo > Mapa > Añadir Brush >` con 7 entries:
  - Box (renombrar el handler existente para consistencia, o
    mantener `AddBoxBrush` como acción separada).
  - Cylinder.
  - Sphere.
  - Pyramid.
  - Wedge.
  - Prism Triangular.
  - Prism Hexagonal.

- `ProjectAction` enum: agregar `AddCylinderBrush`,
  `AddSphereBrush`, `AddPyramidBrush`, `AddWedgeBrush`,
  `AddPrismTriangularBrush`, `AddPrismHexagonalBrush`.

- Handlers en `EditorProjectActions.cpp`: factorizar el
  `handleAddBoxBrush` a un helper genérico `handleAddBrush(brush, tagPrefix)`
  y agregar 6 wrappers cortos.

- Posición del spawn: igual a Box — origen en `(0, 1, 0)`.

### F — Validación visual

- Lanzar editor.
- Spawn una de cada primitiva.
- Verificar que aparecen con la geometría correcta (cilindro
  redondo, esfera redondeada, pirámide afilada, wedge inclinado).
- Probar Subtract de Cylinder dentro de Box → debería hacer un
  agujero cilíndrico en la box.
- Verificar persistencia: save / close / reopen → primitivas
  siguen ahí (gracias al patrón genérico de planos del schema v10).

### G — Cierre + tag

- `docs/HITOS.md`: F2H14 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H14 cerrado, F2H13 a "anterior".
- `docs/DECISIONS.md`: entrada con las decisiones de geometría
  por primitiva (cylinder 16 segments, sphere = dodecaedro,
  pyramid cuadrada, wedge canónico) + rationale "primitivas como
  datos puros, no clases".
- Archivar `PLAN_HITO_F2H14.md` a `docs/archive/plans/`.
- Tag: `v1.5.0-fase2-hito14`.

---

## Suite

Cases nuevos esperados:
- `test_csg_primitives.cpp`: ~30-35 cases, ~80-130 asserts.

**Suite estimada: 495 → ~525 cases / 7715 → ~7850 asserts.**

---

## Riesgos

- **Performance del CSG con primitivas de N alto**: cylinder con
  32 segments tiene 34 caras. Subtract contra otro brush complejo
  es O(N_A × N_B²) → 32 × 1156 = ~37K ops. Debug build puede
  tardar 100-200 ms por op. Aceptable para v1; F2H17 (compilación
  unificada) absorbe en mesh estática al guardar.
- **Aproximación de la esfera**: 12 caras es claramente poliédrico,
  no esfera real. Documentar como expected. Si emerge feedback
  visual, agregar variante de 32 caras (geosfera de 1 subdivision)
  como hito futuro.
- **Vertices coplanares en pyramid**: la cima compartida por 4
  caras laterales podría generar tripletes que intersectan en el
  mismo punto. `isBrushValid` (post-fix de F2H13) debería manejarlo
  bien — la AABB sigue siendo no-degenerada.
- **Wedge en transformaciones rotadas**: el plano inclinado puede
  tener problemas numéricos si la rotación lo alinea con un eje
  cardinal (caras casi paralelas). Mitigación: tests con
  rotaciones extremas.

---

## Criterios de cierre

- [ ] `src/engine/world/csg/Primitives.{h,cpp}` con las 5 helpers.
- [ ] `tests/test_csg_primitives.cpp` ~30+ cases verde.
- [ ] Menu `Archivo > Mapa > Añadir Brush > [...]` con 7 entries
      activos.
- [ ] Validación visual: spawn de cada primitiva, ver geometría
      correcta, Subtract entre primitivas distintas funciona.
- [ ] Save / reopen preserva las primitivas (sin schema bump).
- [ ] Suite ~525 cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado.
- [ ] Tag `v1.5.0-fase2-hito14` pusheado.

---

## Refs canónicas

- "Real-Time Collision Detection" (Ericson) cap 5 — convex polyhedra.
- TrenchBroom source: `common/src/Model/BrushBuilder.cpp` para los
  generadores `createCylinder`, `createCone`, etc.
- Quake / Hammer Editor Tool 4: las primitivas que el dev espera
  (cilindro, esfera, pirámide, wedge) son las del workflow
  histórico de mapping.
