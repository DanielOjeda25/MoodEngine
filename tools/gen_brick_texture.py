"""Genera una textura de ladrillo de prueba (Hito 5 Bloque 5).

Salida: assets/textures/brick.png (256x256 RGBA). Patron de ladrillos de
64x32 px con offset en filas alternas (aparejo ingles). Dos tonos de rojo
para que el textureo sea visible sin parecer liso.

Sirve como segunda textura del mapa para validar texturas por tile; el
borde perimetral usa grid.png y la columna central usa este brick.

Reproducible desde la raiz del repo. Requiere Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw


SIZE = 256
BRICK_W = 64
BRICK_H = 32
MORTAR = 3
BRICK_A = (155, 68, 52, 255)
BRICK_B = (172, 82, 62, 255)
MORTAR_COLOR = (75, 68, 60, 255)


def main() -> None:
    img = Image.new("RGBA", (SIZE, SIZE), MORTAR_COLOR)
    draw = ImageDraw.Draw(img)

    row_idx = 0
    for y in range(0, SIZE, BRICK_H):
        offset = (row_idx % 2) * (BRICK_W // 2)
        col_idx = 0
        for x in range(-offset, SIZE, BRICK_W):
            color = BRICK_A if (row_idx + col_idx) % 2 == 0 else BRICK_B
            x1 = x + MORTAR // 2
            y1 = y + MORTAR // 2
            x2 = x + BRICK_W - MORTAR // 2
            y2 = y + BRICK_H - MORTAR // 2
            draw.rectangle([x1, y1, x2, y2], fill=color)
            col_idx += 1
        row_idx += 1

    out = Path(__file__).resolve().parent.parent / "assets" / "textures" / "brick.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
