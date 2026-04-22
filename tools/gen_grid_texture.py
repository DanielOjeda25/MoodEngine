"""Genera una textura de grid para el Hito 3 del motor.

Salida: assets/textures/grid.png (256x256 RGBA). Cada celda de 32x32 pixeles
tiene un tono base, con lineas de 2px marcando los bordes de la celda y un
gradiente diagonal sutil para que al rotar el cubo se perciba orientacion.

Reproducible: se puede regenerar en cualquier momento ejecutando este script
desde la raiz del repo. Requiere Pillow (pip install pillow).
"""

from pathlib import Path

from PIL import Image, ImageDraw


SIZE = 256
CELL = 32
LINE_W = 2
BG = (60, 72, 90, 255)       # gris azulado
CELL_TINT = (85, 105, 130, 255)
LINE_COLOR = (220, 228, 240, 255)
ACCENT = (230, 150, 60, 255)  # marca cada 4 celdas para dar orientacion


def main() -> None:
    img = Image.new("RGBA", (SIZE, SIZE), BG)
    draw = ImageDraw.Draw(img)

    # Pintar celdas con un checkerboard suave.
    for y in range(0, SIZE, CELL):
        for x in range(0, SIZE, CELL):
            if ((x // CELL) + (y // CELL)) % 2 == 0:
                draw.rectangle([x, y, x + CELL - 1, y + CELL - 1], fill=CELL_TINT)

    # Lineas verticales y horizontales cada CELL.
    for i in range(0, SIZE + 1, CELL):
        draw.line([(i, 0), (i, SIZE)], fill=LINE_COLOR, width=LINE_W)
        draw.line([(0, i), (SIZE, i)], fill=LINE_COLOR, width=LINE_W)

    # Acento cada 4 celdas (sirve como "norte" al ver el cubo rotando).
    for i in range(0, SIZE + 1, CELL * 4):
        draw.line([(i, 0), (i, SIZE)], fill=ACCENT, width=LINE_W)
        draw.line([(0, i), (SIZE, i)], fill=ACCENT, width=LINE_W)

    out = Path(__file__).resolve().parent.parent / "assets" / "textures" / "grid.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
