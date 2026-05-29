# PLAN F3H30 — Texture pack HL1-style procedural + asset browser recursivo + reset UV brush

**Estado:** ✅ **CERRADO** (cerrado 2026-05-29, mismo día que F3H29 — sub-fase 3.4 termina hoy 11/11).
**Predecesor:** F3H29 (Camera limits + polish massive del Inspector).

## Origen

Pedido del dev tras cerrar F3H29: *"creo que lo de las texturas es mas rápido, vamos por eso"*. Stub original de Sub-fase 3.4 era F3H30 = "HDRI dinámico + ciclo día/noche", pero el dev pivoteó a "texture pack" porque las 4 texturas legacy (`brick.png`, `grid.png`, `missing.png`, `particle_fire.png`) no alcanzaban para construir un mapa real.

Refinamiento mid-planeación: el dev pidió *"necesito las necesarias para crear un mapa"* → de la propuesta inicial de 8 texturas (concrete_wall/floor, dirt, metal, wood, gravel, tile, lab, brick_old) bajamos a **5 mínimas viables** quitando las que tenían sustituto vía variantes de las otras.

---

## Sub-fase 3.4 — CIERRE

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – ✅ Parenting jerárquico de transforms
F3H28 – ✅ Grupos + Map Tools como categorías del Properties Editor
F3H29 – ✅ Camera limits + polish play mode + audit traducciones (massive)
F3H30 – ✅ Texture pack HL1-style procedural ⬅ este hito (cierre Sub-fase 3.4)
```

---

## Norte

Pre-F3H30 `assets/textures/` tenía 4 PNGs (Fase 1: tests + UI placeholders). El dev no podía armar un mapa real sin diversidad visual de paredes/pisos/terreno. Comprar/buscar pack CC0 = dependencia externa + licencias + atribución; AI gen = no determinístico + pipeline complejo; hand-painted = trabajo manual escala lineal.

**Decisión**: extender el flow procedural Pillow que ya existía (`gen_brick_texture.py`, `gen_grid_texture.py`, etc.) con scripts adicionales que generan texturas HL1-style — paleta indexed limitada, 256×256, tileable seamless, look "sucio" intencional. Reproducibles desde la raíz del repo. Cero deps nuevas.

---

## Decisiones

**D1 — Set mínimo de 5 texturas (no 8).** El dev pidió *"las necesarias para crear un mapa"*. Pensando como mapper Hammer/HL1: con 5 materiales podés cubrir 80% de cualquier mapa básico (interior + exterior). Las descartadas (wood planks, gravel, tile_floor, lab_panel) son variantes que se pueden sustituir: cajas de madera → metal_panel; gravel → dirt_ground + tonal variation; tile floor → variante de concrete_floor; lab panel → metal_panel + manchas. Si emerge demanda, agregar como F3H31+ asset pack expansion.

Set elegido:
| # | Script | Uso |
|---|---|---|
| 1 | `gen_concrete_wall.py` | Paredes + techos interiores (uso #1) |
| 2 | `gen_concrete_floor.py` | Piso interior (variante más clara) |
| 3 | `gen_dirt_ground.py` | Exterior universal (terreno, paths) |
| 4 | `gen_metal_panel.py` | Puertas, paneles, detalle metálico |
| 5 | `gen_brick_old.py` | Variación visual de pared (alt al concrete) |

Alternativa descartada:
- **8 texturas (set propuesto inicial)**: el dev fue explícito en mínimo viable; agregar 3 más era scope creep para "cerremos definitivamente".

**D2 — Stack Pillow + numpy puro (sin opensimplex, sin scipy).** El look HL1 se logra con: (a) ruido gaussiano cuantizado, (b) paleta indexed 16-32 colores, (c) patches manuales de manchas/grietas/rivets con `ImageDraw`. Pillow + numpy cubren todo. Agregar `opensimplex` daría mejor noise orgánico pero es dep nueva — para el set mínimo, gaussian noise smooth-blurred alcanza.

Alternativa descartada:
- **opensimplex / scipy**: deps nuevas para el repo. Si emerge demanda de noise más sofisticado (perlin/simplex/curl), agregar después.
- **CC0 pack downscaleado** (Kenney, AmbientCG): trade-off en `feedback_no_reinventar_rueda` apuntaba a esto — pero el dev quería look HL1 indexed específico que los packs CC0 modernos no tienen out-of-the-box (son PBR realistas).

**D3 — Sub-carpeta `assets/textures/library/` + AssetBrowser recursivo.** Las 4 legacy quedan en `assets/textures/` flat por compat con paths persistidos en .moodmap viejos (`textures/brick.png`). Las nuevas viven en `assets/textures/library/` → logicalPath `textures/library/concrete_wall.png`. AssetBrowserPanel migrado de `directory_iterator` a `recursive_directory_iterator` (mismo patrón que meshes desde F2H26). El displayName usa path relativo (`library/concrete_wall.png`) para distinguir sub-packs.

Alternativa descartada:
- **Flat en `assets/textures/`**: el dev tendría 9 PNGs mezclados sin agrupación. Sub-carpeta da semántica clara "esto es el pack base de F3H30".

**D4 — Tileable via blend de bordes opuestos.** En `tools/_texture_lib.py::make_tileable()`. Para cada par (columna_izq[i], columna_der[i]) — y análogo filas top/bottom — mezclamos linealmente: en el borde mismo, ambos pixels se vuelven el promedio de los dos (matchan exactamente, no hay costura); a `blend`px de distancia volvemos al pixel original. Resultado seamless al tilear.

Alternativa descartada:
- **Offset + blend del Photoshop Offset Filter**: requiere `np.roll` para mover costuras al centro, suavizar, rotar de vuelta. Implementación inicial tenía bug en el blend (convex combination mal calculada). El approach actual (mezcla directa de bordes) es más simple y robusto.

**D5 — Reset buttons del UV brush undoable.** Pedido del dev: *"a la parte de UV le falta los botones de reset"*. Gap del F3H29: agregamos reset buttons (↺) a los sliders PBR de materiales, pero los del editor UV del brush (uv scale / uv rotation / uv offset) quedaron sin ellos. Implementación: helper lambda `uvResetButton` inline en `InspectorPanel_Brush.cpp` que captura `BrushUVSnapshot` pre, aplica default via `applyToScope`, pushea `EditBrushUVCommand` con snapshot post (undoable). Tooltip `editor.common.reset_default` reutilizado.

Defaults:
- `uvScale` → (1.0, 1.0)
- `uvRotation` → 0.0 rad (0°)
- `uvOffset` → (0.0, 0.0)

El checkbox `lockToWorld` no lleva reset (boolean — destildar = "default").

Alternativa descartada:
- **`detail::inspectorResetButton<T>`** (helper genérico): asume target per-entity. El UV brush es per-face / multi-face con `applyToScope` — necesita lógica custom que el helper no cubre.

**D6 — Botón header del AssetBrowser: "R" → ícono FA rotate.** Pedido del dev: *"el botón de recargar podemos cambiarlo por un botón de reload"*. Cambio puntual: `SmallButton("R")` → `SmallButton(ICON_FA_ROTATE "##reload_assets")`. Consistente con el resto del editor que usa iconos FA (gizmos, snap, reset). Tooltip preservado.

---

## Implementación (compacta)

**1. `tools/_texture_lib.py` NEW** (~100 LOC):
Helpers compartidos: `TEX_SIZE=256`, `library_path(name)`, `seeded_rng(seed)`, `make_tileable(img, blend)`, `quantize_to_palette(img, n_colors)`, `gaussian_noise(...)`, `smooth(...)`. Cada material script importa de aquí.

**2. 5 scripts en `tools/gen_*.py`** (~80 LOC c/u):
- `gen_concrete_wall.py` — ruido gaussiano gris-azulado + 6-10 manchas oscuras + 3-4 grietas verticales serpenteantes; quantize 16 colores; blend 32.
- `gen_concrete_floor.py` — base beige más clara + grid de juntas 64×64 + water spots circulares + patches de desgaste difuso; quantize 16; blend 24.
- `gen_dirt_ground.py` — base marrón + patches sutiles (alpha blend, no fill sólido) + 140-180 specks 1-2px (piedrecitas claras/oscuras); quantize 24; blend 32.
- `gen_metal_panel.py` — base gris-azulado + grid 128×128 con bisel claro/oscuro + 16 rivets (4 por panel) + 4-6 manchas de óxido; quantize 20; blend 12.
- `gen_brick_old.py` — aparejo inglés con offset alterno + 5 tonos terracota gastado + ladrillos individuales con jitter + 30% con manchas de humedad + 3-5 grietas en mortar; quantize 32; blend 6.

Outputs: `assets/textures/library/<name>.png`, 256×256 indexed PNG, 2-25 KB c/u.

**3. AssetBrowserPanel.cpp**:
- `directory_iterator(k_textureDir)` → `recursive_directory_iterator(k_textureDir)`.
- `e.displayName = entry.path().filename()` → `e.displayName = std::filesystem::relative(entry.path(), k_textureDir).generic_string()`.
- `SmallButton("R")` → `SmallButton(ICON_FA_ROTATE "##reload_assets")`.

**4. InspectorPanel_Brush.cpp**:
- Include `IconsFontAwesome6.h`.
- Helper `uvResetButton` lambda inline (~25 LOC).
- 3 invocaciones después de los 3 widgets de UV.

---

## Backlog post-F3H30

- **Asset pack expansion**: si emerge demanda → gen_wood_planks / gen_gravel / gen_tile_floor / gen_lab_panel + variantes Lambda-style.
- **opensimplex / perlin noise**: para materiales orgánicos sofisticados (rock con vetas, mud con flow patterns).
- **Normal maps generados procedurally**: hoy las texturas son solo albedo. Generar normal map por height map derivado del noise → da relieve PBR. Hito propio.
- **Variants del mismo material**: ej. `concrete_wall_a/b/c.png` con seed distinta para break visual de tiling en mapas grandes. Default actual = 1 variant por material.
- **Texture browser preview con tooltip ampliado** (similar a meshes F3H16): hover lento sobre una textura del library → preview 384×384 con metadata (resolución, paleta indexed N colores). Mejora QoL.

---

## Lo que NO toca F3H30

- HDRI dinámico + ciclo día/noche (era el stub original; queda para Fase 4 o post-Fase 3).
- Normal/Roughness/Metallic maps procedurales (solo albedo).
- Variant texture system (1 textura = 1 archivo, sin pseudo-randomization in-engine).
- AI gen pipeline (anti-`feedback_no_reinventar_rueda` + no determinístico).

---

## Cierre — checklist

- [x] D1-D6 documentadas
- [x] `tools/_texture_lib.py` + 5 scripts gen_*.py
- [x] 5 PNGs generados en `assets/textures/library/`
- [x] AssetBrowserPanel.cpp recursivo + ícono FA rotate
- [x] InspectorPanel_Brush.cpp reset buttons UV (3 widgets)
- [x] Build MoodEditor verde
- [x] Validación visual: texturas aparecen en Asset Browser, drag a brush asigna, tilea seamless
- [x] Validación visual: reset buttons UV funcionan + undoable
