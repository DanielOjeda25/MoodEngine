"""F3H30: metal_panel.png — paneles metalicos HL1-style.

Salida: assets/textures/library/metal_panel.png (256x256 indexed).
Uso: puertas, paneles, vigas, paredes industriales. Patron 2x2 de
paneles cuadrados de 128x128 con bisel + rivets en las esquinas.

Receta: base gris-azulado uniforme + bisel claro/oscuro en los bordes
de cada panel + 4 rivets (puntos circulares) por panel + manchas de
oxido sutil.

Reproducible desde la raiz del repo: python tools/gen_metal_panel.py
"""

from PIL import Image, ImageDraw
import numpy as np

from _texture_lib import (
    TEX_SIZE, gaussian_noise, library_path, make_tileable,
    quantize_to_palette, seeded_rng, smooth,
)


def main() -> None:
    rng_py, rng_np = seeded_rng(seed=20290533)

    # Base: ruido suave + tono gris-azulado.
    base = gaussian_noise(TEX_SIZE, mean=125.0, sigma=10.0, rng=rng_np)
    base = smooth(base, passes=2)

    rgb = np.stack([
        base * 0.88,
        base * 0.92,
        base * 1.00,
    ], axis=-1)
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    img = Image.fromarray(rgb, mode="RGB")

    draw = ImageDraw.Draw(img)

    # Panel grid 128x128 (4 paneles 2x2).
    panel = 128

    # Bisel: linea oscura en bordes inferior/derecho de cada panel,
    # linea clara en bordes superior/izquierdo. Da efecto 3D embossed.
    dark = (50, 55, 65)
    light = (180, 185, 195)
    for px in range(0, TEX_SIZE, panel):
        for py in range(0, TEX_SIZE, panel):
            # Top/left lineas claras.
            draw.line((px, py, px + panel - 1, py), fill=light, width=2)
            draw.line((px, py, px, py + panel - 1), fill=light, width=2)
            # Bottom/right lineas oscuras.
            draw.line((px, py + panel - 2, px + panel - 1, py + panel - 2),
                       fill=dark, width=2)
            draw.line((px + panel - 2, py, px + panel - 2, py + panel - 1),
                       fill=dark, width=2)
            # Rivets en las 4 esquinas internas del panel (8px del borde).
            rivet_off = 12
            rivet_r = 4
            for rx, ry in [(px + rivet_off, py + rivet_off),
                            (px + panel - rivet_off, py + rivet_off),
                            (px + rivet_off, py + panel - rivet_off),
                            (px + panel - rivet_off, py + panel - rivet_off)]:
                # Rivet: outer dark + inner highlight.
                draw.ellipse((rx - rivet_r, ry - rivet_r,
                               rx + rivet_r, ry + rivet_r),
                              fill=(80, 85, 95))
                draw.ellipse((rx - 1, ry - 1, rx + 1, ry + 1),
                              fill=(190, 195, 200))

    # Manchas de oxido sutiles (4-6 patches marron-rojizo).
    for _ in range(rng_py.randint(4, 6)):
        cx = rng_py.randint(20, TEX_SIZE - 20)
        cy = rng_py.randint(20, TEX_SIZE - 20)
        r = rng_py.randint(6, 14)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r),
                      fill=(115, 75, 50))

    # Smooth muy ligero para suavizar bisel.
    arr = np.array(img, dtype=np.float32)
    for c in range(3):
        arr[..., c] = smooth(arr[..., c], passes=1)
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGB")

    # Blend bajo para preservar bisel/rivets nitidos.
    img = make_tileable(img, blend=12)
    img = quantize_to_palette(img, n_colors=20)

    out = library_path("metal_panel")
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
