# LUTs (Look-Up Tables) — color grading samples

Estos PNG son **256x16 RGBA8 lineal**, layout convención Unity URP / Unreal
(16 slices horizontales de 16x16 px que codifican el cubo RGB 16x16x16).

MoodEngine los aplica en `ColorGradingPass` entre bloom y tonemap.
Cargá uno desde el panel Inspector → Environment → **Color Grading** →
botón "..." para abrir file picker.

## LUTs shipped

| Archivo | Mood | Cuándo usar |
|---|---|---|
| `identity.png` | sin cambios | testing del algoritmo de lookup (lookup(c) == c). |
| `cinema_warm.png` | atardecer / sepia | escenas exteriores cálidas, westerns, post-apocalipsis. |
| `matrix_cool.png` | verde-cyan | sci-fi / cyberpunk / tech noir. |
| `noir_high_contrast.png` | blanco-negro con S-curve | thriller / drama / momentos dramáticos. |

## Generar más LUTs

### Opción 1 — Python (`tools/gen_luts.py`)

```bash
python tools/gen_luts.py assets/luts
```

Editá el script para sumar tu `color_func(r, g, b) -> (r, g, b)` y volvé a
correrlo. Cada función opera sobre el output de la LUT identidad.

### Opción 2 — DaVinci Resolve / Photoshop / Filmlight Baselight

1. Abrí `identity.png` en tu tool de cine favorito.
2. Aplicá curvas / color wheels / nodes hasta tener el look deseado.
3. Exportá como PNG 256x16 (mismas dimensiones). Si tu tool exporta en
   sRGB, fijate de que MoodEngine lo cargue como linear — la LUT NO es
   una imagen para mirar, es una tabla de mapeo.

### Opción 3 — Cualquier tool que exporte `.cube`

`.cube` (Adobe) es el formato estándar de la industria pero **no soportado
en F2H58 v1**. Si emerge necesidad, agregamos parser que convierta a
PNG 256x16 en runtime. Por ahora exportá desde tu tool a PNG directo.

## Convención del cubo RGB

```
              ┌─ slice 0 ─┬─ slice 1 ─┬─ ... ─┬─ slice 15 ─┐
   y=0 (G=0)  │  R varía  │  R varía  │       │  R varía   │
              │ B=0/15    │ B=1/15    │       │ B=15/15    │
   ...        │           │           │       │            │
   y=15 (G=1) │           │           │       │            │
              └───────────┴───────────┴───────┴────────────┘
              x=0..15      x=16..31    ...    x=240..255
```

Cada slice es un plano R×G a un valor de B fijo. El shader interpola
entre 2 slices adyacentes para reconstruir el valor de B intermedio.
