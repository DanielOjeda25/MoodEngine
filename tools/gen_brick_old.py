"""F3H30: brick_old.png — ladrillo envejecido HL1-style.

Salida: assets/textures/library/brick_old.png (256x256 indexed).
Uso: fachadas de edificios, paredes "viejas" de exterior. Variante
sucia/envejecida del brick.png de F2H5 (no es replacement — coexisten).

Receta: aparejo ingles (offset cada fila) + ladrillos en 3 tonos
desgastados (terracota apagado, mas oscuro, casi gris) + mortar
oscuro con grietas + manchas de humedad/musgo en algunos ladrillos.

Reproducible desde la raiz del repo: python tools/gen_brick_old.py
"""

from PIL import Image, ImageDraw

from _texture_lib import (
    TEX_SIZE, library_path, make_tileable, quantize_to_palette,
    seeded_rng,
)


BRICK_W = 64
BRICK_H = 32
MORTAR = 3


def main() -> None:
    rng_py, _rng_np = seeded_rng(seed=20290534)

    # Mortar de fondo (oscuro, sucio).
    mortar_color = (52, 48, 42)
    img = Image.new("RGB", (TEX_SIZE, TEX_SIZE), mortar_color)
    draw = ImageDraw.Draw(img)

    # Tonos de ladrillo envejecido — 4 variantes para variedad.
    brick_palette = [
        (138, 72, 56),   # terracota standard
        (118, 62, 50),   # terracota oscuro
        (155, 90, 70),   # terracota claro
        (95, 70, 60),    # casi gris-marron (muy gastado)
        (110, 82, 65),   # neutro envejecido
    ]

    row_idx = 0
    for y in range(0, TEX_SIZE, BRICK_H):
        # Aparejo ingles: filas pares no offset, impares con +BRICK_W/2.
        offset = (row_idx % 2) * (BRICK_W // 2)
        col_idx = 0
        for x in range(-offset, TEX_SIZE, BRICK_W):
            # Color random del palette + jitter chico para que ningun
            # par de ladrillos sea identico.
            base_color = rng_py.choice(brick_palette)
            jitter = (
                rng_py.randint(-8, 8),
                rng_py.randint(-5, 5),
                rng_py.randint(-5, 5),
            )
            color = tuple(max(0, min(255, c + j))
                          for c, j in zip(base_color, jitter))
            x1 = x + MORTAR // 2
            y1 = y + MORTAR // 2
            x2 = x + BRICK_W - MORTAR // 2
            y2 = y + BRICK_H - MORTAR // 2
            draw.rectangle([x1, y1, x2, y2], fill=color)

            # 30% de los ladrillos tienen una "mancha" de humedad/edad
            # (parche oscuro dentro del ladrillo).
            if rng_py.random() < 0.3:
                mx = rng_py.randint(x1, x2 - 1)
                my = rng_py.randint(y1, y2 - 1)
                mr = rng_py.randint(3, 8)
                stain = tuple(max(0, c - rng_py.randint(20, 35))
                              for c in color)
                draw.ellipse((mx - mr, my - mr, mx + mr, my + mr),
                              fill=stain)
            col_idx += 1
        row_idx += 1

    # Grietas en el mortar (3-5 segmentos horizontales finos).
    for _ in range(rng_py.randint(3, 5)):
        y = rng_py.choice([BRICK_H, BRICK_H * 2, BRICK_H * 3,
                            BRICK_H * 4, BRICK_H * 5, BRICK_H * 6])
        x_start = rng_py.randint(0, TEX_SIZE // 2)
        x_end = rng_py.randint(x_start + 30, TEX_SIZE - 1)
        crack_color = (30, 28, 25)
        # Grieta serpenteante: 4 segmentos con jitter Y.
        segs = 4
        prev_x, prev_y = x_start, y
        for s in range(1, segs + 1):
            next_x = x_start + (x_end - x_start) * s // segs
            next_y = y + rng_py.randint(-2, 2)
            draw.line((prev_x, prev_y, next_x, next_y),
                       fill=crack_color, width=1)
            prev_x, prev_y = next_x, next_y

    # F3H30 ajuste: sin smooth final (lo dejaba borroso). Los ladrillos
    # quieren bordes nitidos entre piezas. La definicion del aparejo
    # ingles es lo que vende el look.

    # F3H30 ajuste: blend menor (6 vs 12) para preservar la nitidez
    # de los bordes y juntas entre ladrillos.
    img = make_tileable(img, blend=6)
    # Mas colores: brick necesita matiz para que la variacion tonal
    # entre ladrillos sea visible despues de quantizar.
    img = quantize_to_palette(img, n_colors=32)

    out = library_path("brick_old")
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
