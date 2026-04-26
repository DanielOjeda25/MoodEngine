"""Genera un cubemap de cielo simple para el Hito 15 del motor.

Salida: 6 PNG de 256x256 RGBA en assets/skyboxes/sky_day/:
    px.png  -X positivo (derecha)
    nx.png  -X negativo (izquierda)
    py.png  +Y (cenit)
    ny.png  -Y (suelo)
    pz.png  +Z (frente)
    nz.png  -Z (atras)

Estilo: gradient vertical azul oscuro -> azul claro hacia el horizonte
en las 4 caras laterales, azul muy oscuro casi uniforme arriba (cenit),
marron oscuro en el piso (cuando el camera-forward apunta hacia abajo).
Un acento sutil naranja-rosa en el horizonte de la cara `+Z` simula un
atardecer ligero que ayuda a notar la rotacion (sino el sky se ve
identico desde cualquier yaw).

Reproducible: regenerable con `python tools/gen_sky_cubemap.py` desde la
raiz del repo. Requiere Pillow (pip install pillow).
"""

from pathlib import Path

from PIL import Image


SIZE = 256
OUT_DIR = Path("assets/skyboxes/sky_day")


# Paleta del cielo:
#   - cenit:       (12, 22, 50)   azul casi negro
#   - mid:         (35, 80, 140)  azul medio
#   - horizonte:   (180, 195, 215) azul-blanquecino
#   - suelo:       (35, 25, 15)   marron muy oscuro (sub-horizonte)
#   - atardecer:   (220, 130, 90) naranja-rosa para acento en `+Z`


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def vertical_gradient(top_color, bottom_color, accent_color=None):
    """Devuelve una imagen SIZExSIZE con gradient vertical.

    top_color en y=0, bottom_color en y=SIZE-1. Si `accent_color`,
    suma una banda mas calida en los ultimos 30% pixeles (horizon glow).
    """
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255))
    px = img.load()
    for y in range(SIZE):
        t = y / (SIZE - 1)
        c = lerp(top_color, bottom_color, t)
        if accent_color is not None and t > 0.65:
            # Curva suave del accent para que no se note un corte duro.
            local = (t - 0.65) / 0.35  # 0..1 en el ultimo 35%
            local = local ** 2          # pegar al horizonte
            c = lerp(c, accent_color, local * 0.45)  # mezcla parcial
        for x in range(SIZE):
            px[x, y] = (c[0], c[1], c[2], 255)
    return img


def solid(color):
    return Image.new("RGBA", (SIZE, SIZE), (color[0], color[1], color[2], 255))


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    cenit = (12, 22, 50)
    mid = (35, 80, 140)
    horizon = (180, 195, 215)
    floor = (35, 25, 15)
    sunset = (220, 130, 90)

    # 4 caras laterales: gradient cenit -> horizonte. Solo `+Z` lleva
    # acento sunset; las demas, gradient plano para que el sol se note.
    side_plain = vertical_gradient(cenit, horizon)
    side_sunset = vertical_gradient(cenit, horizon, accent_color=sunset)

    # Cara superior (+Y): casi uniforme azul oscuro con leve viñeteo
    # hacia los bordes (esquinas mas claras donde se conecta con las
    # laterales). Implementacion simple: lerp del centro al cenit oscuro.
    top = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255))
    tp = top.load()
    cx = (SIZE - 1) / 2.0
    cy = (SIZE - 1) / 2.0
    max_d = (cx ** 2 + cy ** 2) ** 0.5
    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            t = d / max_d  # 0 centro, 1 esquina
            c = lerp(cenit, mid, t * 0.6)
            tp[x, y] = (c[0], c[1], c[2], 255)

    # Cara inferior (-Y): solido marron oscuro. Suficiente para evitar el
    # "fondo magenta" del missing y dar sensacion de suelo.
    bottom = solid(floor)

    # Mapa convencional de OpenGL cubemap: 6 caras nombradas px/nx/py/ny/pz/nz.
    # `pz` lleva el acento sunset para "marcar" un punto cardinal.
    faces = {
        "px.png": side_plain,
        "nx.png": side_plain,
        "py.png": top,
        "ny.png": bottom,
        "pz.png": side_sunset,
        "nz.png": side_plain,
    }

    for name, img in faces.items():
        path = OUT_DIR / name
        img.save(path)
        print(f"escrito: {path}")


if __name__ == "__main__":
    main()
