"""
hdr_to_equirect_png.py — convierte un HDRI equirectangular a un solo PNG
LDR tonemapped + gamma corrected, listo para `SkyboxRenderer` modo
equirectangular.

Uso:
    py tools/hdr_to_equirect_png.py <input.hdr> <output.png> [--width 2048] [--exposure 1.0]

A diferencia de `hdr_to_cubemap.py`, NO genera 6 caras — el shader
sampler equirectangular evita los seams que aparecen en cubemaps al
unir las caras.
"""

import sys
from pathlib import Path

import imageio
import imageio.v3 as iio
import numpy as np
from PIL import Image

try:
    imageio.plugins.freeimage.download()
except Exception:
    pass


def tonemap_reinhard(rgb: np.ndarray) -> np.ndarray:
    return rgb / (1.0 + rgb)


def linear_to_srgb(rgb: np.ndarray) -> np.ndarray:
    a = 0.055
    out = np.where(rgb <= 0.0031308,
                   12.92 * rgb,
                   (1 + a) * np.power(np.maximum(rgb, 1e-8), 1.0 / 2.4) - a)
    return np.clip(out, 0.0, 1.0)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    width = 2048
    exposure = 1.0
    if "--width" in sys.argv:
        width = int(sys.argv[sys.argv.index("--width") + 1])
    if "--exposure" in sys.argv:
        exposure = float(sys.argv[sys.argv.index("--exposure") + 1])
    height = width // 2

    print(f"Cargando HDRI: {in_path}")
    hdr = iio.imread(in_path, plugin="HDR-FI").astype(np.float32)
    if hdr.shape[2] == 4:
        hdr = hdr[..., :3]
    print(f"  shape={hdr.shape} min={hdr.min():.3f} max={hdr.max():.3f}")

    # Resize a target_w x target_h con bilinear (PIL).
    src_h, src_w = hdr.shape[:2]
    if (src_w, src_h) != (width, height):
        print(f"Resampleando de {src_w}x{src_h} a {width}x{height}...")
        # PIL no soporta float multibanda directo; convertimos canal por canal
        # tras tonemap para mantener calidad razonable.
        pass

    # Exposure → tonemap → gamma. Operamos a resolucion fuente luego resampleamos.
    rgb_lin = hdr * exposure
    rgb_t = tonemap_reinhard(rgb_lin)
    rgb_srgb = linear_to_srgb(rgb_t)
    rgb_8 = (rgb_srgb * 255.0 + 0.5).astype(np.uint8)
    img = Image.fromarray(rgb_8)
    if (img.width, img.height) != (width, height):
        img = img.resize((width, height), Image.LANCZOS)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path, optimize=True)
    print(f"OK -> {out_path} ({out_path.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
