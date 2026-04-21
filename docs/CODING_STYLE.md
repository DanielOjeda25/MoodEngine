# Convenciones de código — MoodEngine

Resumen de la sección 7 del documento técnico de contexto. Para detalles, consultar ese documento.

## Nombres

| Elemento                              | Convención             | Ejemplo                        |
|---------------------------------------|------------------------|--------------------------------|
| Tipos (class, struct, enum, using)    | `PascalCase`           | `TransformComponent`           |
| Funciones libres y métodos            | `camelCase`            | `renderScene`                  |
| Variables locales y parámetros        | `camelCase`            | `int triangleCount`            |
| Miembros privados de clase            | `m_camelCase`          | `SDL_Window* m_window`         |
| Miembros públicos de structs de datos | `camelCase`            | `vec3 position`                |
| Constantes de compile-time            | `k_PascalCase`         | `constexpr int k_MaxEntities`  |
| Macros                                | `UPPER_SNAKE_CASE`     | `MOOD_ASSERT`                  |
| Namespace                             | `PascalCase`           | `Mood`, `Mood::Log`            |

Todo el motor vive bajo el namespace `Mood`.

## Archivos

- Un tipo principal por archivo. Excepción: tipos pequeños muy acoplados (ej. `Components.h`).
- Nombre de archivo = nombre del tipo principal.
- `#pragma once` en headers, no include guards.
- `.h` para headers, `.cpp` para implementación.

## Orden de includes

1. Header del propio módulo (solo en `.cpp`).
2. Headers del proyecto (`"core/..."`, `"engine/..."`).
3. Librerías externas (`<SDL.h>`, `<glad/glad.h>`, `<glm/...>`).
4. Headers estándar (`<vector>`, `<string>`).

Comillas `"..."` para includes del proyecto, `<...>` para externos.

## Comentarios

- En **español**, para acompañar el aprendizaje.
- Solo donde aportan valor: intención, no descripción.
- Doxygen ligero en APIs públicas del motor: `/// @brief ...`.

## Formateo

Archivo `.clang-format` en la raíz. Estilo base LLVM con ajustes:

- `IndentWidth: 4`
- `ColumnLimit: 120`
- `PointerAlignment: Left`
- `BreakBeforeBraces: Attach`

Formatear antes de commit: `clang-format -i <archivo>`.

## Otros principios

- `const` siempre que sea posible.
- Referencias `&` antes que punteros cuando no puede ser nulo.
- `std::unique_ptr` para ownership único, `std::shared_ptr` solo si realmente compartido.
- Forward declarations en headers para reducir tiempos de compilación.
- Nada de `new`/`delete` manual salvo casos muy específicos.
- RAII estricto: destructores cierran recursos.
