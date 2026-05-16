"""Generate 256x16 PNG LUTs for MoodEngine color grading (F2H58 E).

Layout: 16 horizontal slices de 16x16 px que codifican el cubo RGB 16x16x16.
  - Eje X global (0..255): (sliceIdx * 16) + localR.
    - sliceIdx (0..15) -> componente B del color de entrada.
    - localR  (0..15)  -> componente R del color de entrada.
  - Eje Y (0..15) -> componente G del color de entrada.

Identity LUT: out_pixel(x,y) = (localR/15, y/15, sliceIdx/15) -> mapea
cada color de entrada a si mismo cuando el shader hace el lookup descrito
en color_grading.frag.

Las custom LUTs aplican una transformacion (color_func) sobre el output
identity para producir el grade.
"""

from PIL import Image
import os
import sys

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else "."
SIZE = 16  # cubo RGB 16x16x16
W, H = SIZE * SIZE, SIZE  # 256x16


def identity_color(r01, g01, b01):
    return (r01, g01, b01)


def cinema_warm(r01, g01, b01):
    """Sepia / atardecer.
    - Lift en rojos + amarillos.
    - Cyan/azul ligeramente atenuado.
    - Gamma suave 0.95 en luma para subir mediotonos.
    """
    r = min(1.0, r01 * 1.10 + 0.04)
    g = min(1.0, g01 * 1.02 + 0.02)
    b = max(0.0, b01 * 0.85 - 0.02)
    return (r, g, b)


def matrix_cool(r01, g01, b01):
    """Verde-cyan tipo Matrix.
    - Rojos atenuados.
    - Verde lift moderado.
    - Azul lift suave (mas cyan).
    """
    r = max(0.0, r01 * 0.75)
    g = min(1.0, g01 * 1.10 + 0.05)
    b = min(1.0, b01 * 0.90 + 0.08)
    return (r, g, b)


def noir_high_contrast(r01, g01, b01):
    """Alto contraste blanco-negro. Saturacion reducida + curve S."""
    # Luma weighted (BT.601).
    luma = 0.299 * r01 + 0.587 * g01 + 0.114 * b01
    # Desaturate: 70% gris + 30% color original.
    r = 0.7 * luma + 0.3 * r01
    g = 0.7 * luma + 0.3 * g01
    b = 0.7 * luma + 0.3 * b01
    # S-curve: pull blacks down, push whites up.
    def scurve(v):
        return v * v * (3.0 - 2.0 * v)  # smoothstep
    return (scurve(r), scurve(g), scurve(b))


def gen_lut(color_func, out_path):
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            slice_idx = x // SIZE
            local_r   = x % SIZE
            # Color de entrada en [0,1] segun layout.
            r01 = local_r   / (SIZE - 1)
            g01 = y         / (SIZE - 1)
            b01 = slice_idx / (SIZE - 1)
            # Identity sample primero -> color_func aplica grade.
            ident = identity_color(r01, g01, b01)
            graded = color_func(*ident)
            r = int(round(graded[0] * 255))
            g = int(round(graded[1] * 255))
            b = int(round(graded[2] * 255))
            px[x, y] = (r, g, b)
    img.save(out_path, "PNG")
    print(f"  wrote {out_path}")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"Generating LUTs into {OUT_DIR} ...")
    gen_lut(identity_color,    os.path.join(OUT_DIR, "identity.png"))
    gen_lut(cinema_warm,       os.path.join(OUT_DIR, "cinema_warm.png"))
    gen_lut(matrix_cool,       os.path.join(OUT_DIR, "matrix_cool.png"))
    gen_lut(noir_high_contrast, os.path.join(OUT_DIR, "noir_high_contrast.png"))
    print("Done.")


if __name__ == "__main__":
    main()
