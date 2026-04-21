# GLAD — loader de OpenGL

Archivos generados con **glad2** para OpenGL 4.5 Core Profile. Todos se commitean al repo (decisión alineada con la sección 4.5 del documento técnico).

## Estructura

```
external/glad/
├── include/
│   ├── glad/
│   │   └── gl.h              <- header publico (incluir con <glad/gl.h>)
│   └── KHR/
│       └── khrplatform.h
└── src/
    └── gl.c                  <- fuentes del loader
```

## Uso en el motor

```c
#include <glad/gl.h>
// Tras crear el contexto OpenGL con SDL:
if (!gladLoaderLoadGL()) {
    // error: no se pudo cargar OpenGL
}
```

El target `glad` esta definido en `CMakeLists.txt` y se enlaza a `MoodEditor`.

## Regeneracion (solo si cambia la version de GL o se agregan extensiones)

Requiere Python 3.9+ con el paquete `glad2`:

```bash
pip install glad2
python -m glad --api gl:core=4.5 --out-path external/glad --quiet c --loader
```

Ajustar el `gl:core=4.5` si se quiere otra version o perfil. `--loader` incluye los loaders internos (`gladLoaderLoadGL`). Opcional: `--extensions=GL_EXT_foo,GL_ARB_bar` para limitar extensiones.

> No versionamos aca el CLI de `glad2`, pero la version que se uso por ultima vez se registra en `docs/DECISIONS.md`.
