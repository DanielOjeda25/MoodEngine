# Performance baseline de MoodEngine

> **Generado en F2H2** (2026-05). Documento vivo. Cada hito de optimizacion
> (F2H3 frustum culling, F2H4 LOD, etc.) suma una columna comparativa.

## Como reproducir

1. Compilar en **Debug con `MOOD_PROFILE=ON`** (default):
   ```
   cmake --build build/debug --config Debug
   ```
2. Lanzar `build/debug/Debug/MoodEditor.exe` y abrir un proyecto vacio.
3. Toggle del HUD: **menu Ver > Performance**.
4. Spawn de la escena de stress: **menu Ayuda > Stress test poligonos > Spawn 10K / 100K / 500K / 1M tris (cubos)**.
5. Para ver Tracy:
   - Bajar `tracy-profiler.exe` de https://github.com/wolfpld/tracy/releases (mismo tag que el cliente: **v0.11.1**).
   - Abrirlo, conectar (`Ctrl+C` o boton "Connect" → localhost).
   - Las zonas `EditorApplication::*`, `SceneRenderer::*`, `PBR::staticPass`, etc. aparecen en vivo.

## Hardware baseline

| Item | Valor |
|---|---|
| GPU | _(rellenar — esperado: GTX 1660)_ |
| CPU | _(rellenar)_ |
| RAM | _(rellenar)_ |
| OS | Windows 11 Pro Education 26200 |
| Build | Debug / MSVC 14.44 |
| Tracy version | 0.11.1 |

## Resultados por escena de stress-test

> Todos los numeros tomados con viewport 1280x720, sin shadow demo activa
> (sombra desactivada). Rellenar al medir.

| Escena | Entities | Tris | Draw calls | FPS | Frame ms | Top 3 zonas Tracy (% del frame) |
|---|---|---|---|---|---|---|
| Empty (proyecto vacio) | _(?)_ | 0 | _(?)_ | _(?)_ | _(?)_ | _(?)_ |
| 10K tris (cubos) | ~833 | ~10K | _(?)_ | _(?)_ | _(?)_ | _(?)_ |
| 100K tris (cubos) | ~8333 | ~100K | _(?)_ | _(?)_ | _(?)_ | _(?)_ |
| 500K tris (cubos) | ~41666 | ~500K | _(?)_ | _(?)_ | _(?)_ | _(?)_ |
| 1M tris (cubos) | ~83333 | ~1M | _(?)_ | _(?)_ | _(?)_ | _(?)_ |

## Top 5 cuellos de botella identificados

> Ordenados por impacto (% del frame al cargar 100K-500K cubos). Rellenar
> con la captura Tracy. Cada item linkea a un hito siguiente que lo ataca.

1. **_(?)_** — % del frame, zona Tracy `_(?)_`. Ataque previsto: F2H3 (frustum culling) / F2H4 (LOD) / otro.
2. **_(?)_**
3. **_(?)_**
4. **_(?)_**
5. **_(?)_**

## Conclusiones

> Resumen de 2-3 lineas con la lectura del baseline:
> - Donde se cae el FPS bajo el umbral 60 / 30.
> - Que se ve mas inflado vs lo esperado.
> - Cual es el primer cuello (entrada a F2H3).

## Notas de medicion

- **Build mode**: el Debug de MSVC no esta optimizado — el `frame ms` real en
  Release con MOOD_PROFILE=OFF sera ~3-10x menor. El baseline Debug sirve para
  identificar **proporciones relativas** (que zona se lleva la torta), no para
  garantizar performance final.
- **Tracy on-demand**: el cliente solo activa instrumentacion al conectar el
  server; sin server conectado, la overhead es ~1-2% del frame. Conectado,
  ~5-8%. No es lo mismo medir con/sin server abierto.
- **Counters draw calls + tris**: el contador del IRenderer cubre PBR static
  + skinned + shadow pass. NO incluye skybox (1 call), particles (1 call
  instanced), debug renderer (1-2 calls). El delta es <5% del total para
  escenas con muchos meshes — relevante solo en escenas casi vacias.
