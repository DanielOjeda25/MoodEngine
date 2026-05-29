"""F3H30: concrete_floor.png — piso de concreto interior HL1-style.

Salida: assets/textures/library/concrete_floor.png (256x256 indexed).
Uso: pisos interiores. Variante mas clara que concrete_wall + sin
grietas verticales (los pisos no tienen patron vertical) + lineas de
junta sutiles cada 64px (suelo de baldosas grandes pintadas).

Reproducible desde la raiz del repo: python tools/gen_concrete_floor.py
"""

from PIL import Image, ImageDraw
import numpy as np

from _texture_lib import (
    TEX_SIZE, gaussian_noise, library_path, make_tileable,
    quantize_to_palette, seeded_rng, smooth,
)


def main() -> None:
    rng_py, rng_np = seeded_rng(seed=20290531)

    # Base mas clara y menos contrastada que wall (suelos pulidos).
    base = gaussian_noise(TEX_SIZE, mean=135.0, sigma=18.0, rng=rng_np)
    base = smooth(base, passes=2)

    # Tono beige-gris (concreto pulido tira a calido).
    rgb = np.stack([
        base * 1.04,
        base * 1.00,
        base * 0.92,
    ], axis=-1)
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    img = Image.fromarray(rgb, mode="RGB")

    draw = ImageDraw.Draw(img)

    # Juntas grid 64x64: lineas de 1px tono mas oscuro que el base.
    # Convencion suelo industrial: division en panos de ~50-80cm.
    joint = (90, 88, 80)
    for i in range(0, TEX_SIZE, 64):
        draw.line((i, 0, i, TEX_SIZE - 1), fill=joint, width=1)
        draw.line((0, i, TEX_SIZE - 1, i), fill=joint, width=1)

    # Water spots: 8-12 manchas circulares pequenas mas oscuras.
    for _ in range(rng_py.randint(8, 12)):
        cx = rng_py.randint(0, TEX_SIZE - 1)
        cy = rng_py.randint(0, TEX_SIZE - 1)
        r = rng_py.randint(4, 12)
        tone = rng_py.randint(85, 110)
        color = (tone, tone - rng_py.randint(0, 8), tone - rng_py.randint(5, 15))
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)

    # Patches mas grandes y difusos (sombras de uso/desgaste).
    for _ in range(rng_py.randint(3, 5)):
        cx = rng_py.randint(0, TEX_SIZE - 1)
        cy = rng_py.randint(0, TEX_SIZE - 1)
        r = rng_py.randint(20, 40)
        tone = rng_py.randint(105, 125)
        color = (tone, tone, tone - 6)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)

    # Smooth final para integrar patches con el base.
    arr = np.array(img, dtype=np.float32)
    for c in range(3):
        arr[..., c] = smooth(arr[..., c], passes=1)
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGB")

    img = make_tileable(img, blend=24)
    img = quantize_to_palette(img, n_colors=16)

    out = library_path("concrete_floor")
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
