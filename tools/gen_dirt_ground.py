"""F3H30: dirt_ground.png — terreno exterior tierra/polvo HL1-style.

Salida: assets/textures/library/dirt_ground.png (256x256 indexed).
Uso: todo el exterior (caminos, terreno general). Look organico,
sin patron geometrico, paleta marron/tan.

Receta: ruido grueso + small specks (piedrecitas) + parches de tono.
Sin lineas/juntas — la naturaleza no tiene grid.

Reproducible desde la raiz del repo: python tools/gen_dirt_ground.py
"""

from PIL import Image, ImageDraw
import numpy as np

from _texture_lib import (
    TEX_SIZE, gaussian_noise, library_path, make_tileable,
    quantize_to_palette, seeded_rng, smooth,
)


def main() -> None:
    rng_py, rng_np = seeded_rng(seed=20290532)

    # Base: ruido + smooth fuerte para "chunks" mas grandes.
    base = gaussian_noise(TEX_SIZE, mean=95.0, sigma=28.0, rng=rng_np)
    base = smooth(base, passes=3)

    # Tono marron tierra: R alto, G medio, B bajo.
    rgb = np.stack([
        base * 1.15,
        base * 0.85,
        base * 0.55,
    ], axis=-1)
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    img = Image.fromarray(rgb, mode="RGB")

    draw = ImageDraw.Draw(img)

    # Patches chicos y sutiles (zonas mas claras/oscuras de tierra).
    # F3H30 ajuste: radios mas chicos + menor cantidad + tono mas
    # cercano al base para que no parezcan "manchas grandes" sino
    # variacion natural del terreno.
    arr_patches = np.array(img, dtype=np.float32)
    for _ in range(rng_py.randint(5, 8)):
        cx = rng_py.randint(0, TEX_SIZE - 1)
        cy = rng_py.randint(0, TEX_SIZE - 1)
        r = rng_py.randint(10, 20)
        # Tono: variacion de ±20 sobre el base (95 mean), no extremos.
        delta = rng_py.randint(-25, 25)
        tone_r = max(0, min(255, int(110 + delta) + 15))
        tone_g = max(0, min(255, int(110 + delta) - 12))
        tone_b = max(0, min(255, int(110 + delta) - 45))
        # Blend con alpha en vez de fill solido para integrar.
        y_grid, x_grid = np.ogrid[:TEX_SIZE, :TEX_SIZE]
        dist = np.sqrt((x_grid - cx) ** 2 + (y_grid - cy) ** 2)
        mask = np.clip(1.0 - dist / r, 0.0, 1.0) * 0.55  # alpha max 0.55
        for c, target in enumerate([tone_r, tone_g, tone_b]):
            arr_patches[..., c] = (
                arr_patches[..., c] * (1.0 - mask) + target * mask
            )
    img = Image.fromarray(np.clip(arr_patches, 0, 255).astype(np.uint8),
                          mode="RGB")
    draw = ImageDraw.Draw(img)

    # Specks: 140-180 piedrecitas chiquitas (1-2 px). Mas densas que
    # antes — son lo que mas vende el look "tierra granulada".
    for _ in range(rng_py.randint(140, 180)):
        cx = rng_py.randint(0, TEX_SIZE - 1)
        cy = rng_py.randint(0, TEX_SIZE - 1)
        size = rng_py.choice([1, 1, 1, 1, 2, 2])
        # Piedras: gris claro o muy oscuro (50/50).
        if rng_py.random() < 0.5:
            tone = rng_py.randint(150, 200)
            color = (tone, tone, tone - 10)
        else:
            tone = rng_py.randint(25, 50)
            color = (tone, tone - 5, tone - 8)
        draw.rectangle((cx, cy, cx + size, cy + size), fill=color)

    # Sin smooth final — preserva la granulacion de las specks.

    img = make_tileable(img, blend=32)
    img = quantize_to_palette(img, n_colors=24)  # mas variacion tonal

    out = library_path("dirt_ground")
    img.save(out, "PNG")
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
