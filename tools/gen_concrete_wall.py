"""F3H30: concrete_wall.png — pared de concreto gris HL1-style.

Salida: assets/textures/library/concrete_wall.png (256x256 indexed).
Uso: paredes y techos interiores. Variante mas oscura y "manchada"
que concrete_floor para diferenciar visualmente.

Look: base gris medio + ruido gaussiano fino + 6-10 manchas oscuras
random + 3-4 grietas finas verticales. Paleta 16 colores.

Reproducible desde la raiz del repo: python tools/gen_concrete_wall.py
"""

from PIL import ImageDraw

from _texture_lib import (
    TEX_SIZE, gaussian_noise, library_path, make_tileable,
    quantize_to_palette, seeded_rng, smooth,
)
from PIL import Image
import numpy as np


def main() -> None:
    rng_py, rng_np = seeded_rng(seed=20290530)

    # Base: ruido gaussiano centrado en gris medio.
    base = gaussian_noise(TEX_SIZE, mean=110.0, sigma=22.0, rng=rng_np)
    base = smooth(base, passes=1)

    # Convertir a RGB tirando a frio (gris levemente azulado).
    rgb = np.stack([
        base * 0.92,        # R
        base * 0.96,        # G
        base * 1.02,        # B (un poco mas blue para "frio")
    ], axis=-1)
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    img = Image.fromarray(rgb, mode="RGB")

    # Manchas oscuras (humedad/oxido en concreto).
    draw = ImageDraw.Draw(img)
    for _ in range(rng_py.randint(6, 10)):
        cx = rng_py.randint(0, TEX_SIZE - 1)
        cy = rng_py.randint(0, TEX_SIZE - 1)
        r = rng_py.randint(8, 24)
        tone = rng_py.randint(45, 75)
        # Mancha con ligera variacion RGB para no ser plana.
        color = (tone, tone + rng_py.randint(-3, 3), tone + rng_py.randint(-3, 6))
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)

    # Grietas verticales finas (1-2 px).
    for _ in range(rng_py.randint(3, 4)):
        x = rng_py.randint(10, TEX_SIZE - 10)
        y_start = rng_py.randint(0, TEX_SIZE // 2)
        y_end = rng_py.randint(TEX_SIZE // 2, TEX_SIZE - 1)
        # Grieta serpenteante: dividir en segmentos cortos con offset random.
        segments = 6
        prev_x, prev_y = x, y_start
        for s in range(1, segments + 1):
            next_y = y_start + (y_end - y_start) * s // segments
            next_x = prev_x + rng_py.randint(-3, 3)
            draw.line((prev_x, prev_y, next_x, next_y),
                       fill=(38, 40, 42), width=1)
            prev_x, prev_y = next_x, next_y

    # Aplicar smooth final ligero para "envejecer" los bordes de las manchas.
    arr = np.array(img, dtype=np.float32)
    for c in range(3):
        arr[..., c] = smooth(arr[..., c], passes=1)
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGB")

    # Tileable + cuantizar a paleta 16 (look HL1 indexed).
    img = make_tileable(img, blend=32)
    img = quantize_to_palette(img, n_colors=16)

    out = library_path("concrete_wall")
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
