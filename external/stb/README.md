# stb — Sean Barrett's single-header libraries

Headers public-domain commiteados al repo. Usados para carga y escritura de imagenes.

## Contenido

| Archivo | Version | Uso |
|---|---|---|
| `stb_image.h` | 2.30 | Lectura de PNG, JPG, BMP, TGA, GIF, PSD, HDR, PIC. |
| `stb_image_write.h` | 1.16 | Escritura de PNG, JPG, BMP, TGA, HDR. |

## Origen

Ambos archivos se obtuvieron directamente de https://github.com/nothings/stb (rama `master`). Adelantamos la entrada respecto del roadmap original (estaba planeada para Hito 5); la carga de texturas del cubo del Hito 3 la requiere antes.

## Uso

Son single-header: el codigo esta en el mismo `.h` envuelto por macros. Para que genere simbolos hay que definir `STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION` en **una sola** TU. Eso lo hace `src/engine/assets/stb_impl.cpp`.

Desde el resto del codigo:

```cpp
#include <stb_image.h>
// ...
int w, h, c;
stbi_uc* pixels = stbi_load("path.png", &w, &h, &c, 0);
// usar pixels...
stbi_image_free(pixels);
```

El target CMake `stb` es INTERFACE: solo expone los include paths, no compila nada por si mismo. Los simbolos viven en el `.obj` de `stb_impl.cpp`.

## Actualizacion

Para actualizar a una version nueva:

```bash
curl -sSL -o external/stb/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -sSL -o external/stb/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

Actualizar la tabla de versiones arriba y registrar el cambio en `docs/DECISIONS.md`.
