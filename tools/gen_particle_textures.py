"""
gen_particle_textures.py — genera texturas de particulas RGBA para el
ParticleSystem (Hito 29).

Uso:
    py tools/gen_particle_textures.py

Salida:
    assets/textures/particle_fire.png — chispa redonda blanca con falloff
        radial alpha (smoothstep). Tamano 64x64. Util para fuego/sparks.
        El color final lo controla `colorStart`/`colorEnd` del emisor;
        la textura aporta la forma.
"""

from pathlib import Path

import numpy as np
from PIL import Image


def main() -> None:
    out_dir = Path("assets/textures")
    out_dir.mkdir(parents=True, exist_ok=True)

    size = 64
    yy, xx = np.indices((size, size), dtype=np.float32)
    cx = cy = (size - 1) * 0.5
    dx = (xx - cx) / cx
    dy = (yy - cy) / cy
    r = np.sqrt(dx * dx + dy * dy)  # 0 en el centro, 1 en el borde

    # Smoothstep: alpha=1 hasta r=0.4, fade lineal a 0 hasta r=1.
    fade = np.clip(1.0 - (r - 0.0) / 1.0, 0.0, 1.0)
    alpha = np.clip(fade * fade * (3.0 - 2.0 * fade), 0.0, 1.0)
    rgb = np.full((size, size, 3), 255, dtype=np.uint8)
    a8 = (alpha * 255.0 + 0.5).astype(np.uint8)
    rgba = np.dstack([rgb, a8])

    out = out_dir / "particle_fire.png"
    Image.fromarray(rgba, mode="RGBA").save(out, optimize=True)
    print(f"OK -> {out} ({size}x{size}, RGBA)")


if __name__ == "__main__":
    main()
