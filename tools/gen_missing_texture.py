"""Genera la textura de fallback "missing" del motor (Hito 5 Bloque 1).

Salida: assets/textures/missing.png (256x256 RGBA). Patron chequered rosa
(#FF00FF) y negro en bloques de 128x128 (estilo Source engine): 4 cuadrantes
que dan reconocimiento inmediato de que el asset esperado no se pudo cargar.

El AssetManager (Bloque 2) devuelve esta textura cuando stbi_load falla o el
path logico no existe, en vez de crashear o dibujar un cubo negro liso.

Reproducible: regenerar en cualquier momento ejecutando este script desde la
raiz del repo. Requiere Pillow (pip install pillow).
"""

from pathlib import Path

from PIL import Image, ImageDraw


SIZE = 256
BLOCK = 128               # 2x2 bloques grandes
MAGENTA = (255, 0, 255, 255)
BLACK = (0, 0, 0, 255)


def main() -> None:
    img = Image.new("RGBA", (SIZE, SIZE), BLACK)
    draw = ImageDraw.Draw(img)

    for by in range(0, SIZE, BLOCK):
        for bx in range(0, SIZE, BLOCK):
            if ((bx // BLOCK) + (by // BLOCK)) % 2 == 0:
                draw.rectangle([bx, by, bx + BLOCK - 1, by + BLOCK - 1], fill=MAGENTA)

    out = Path(__file__).resolve().parent.parent / "assets" / "textures" / "missing.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
