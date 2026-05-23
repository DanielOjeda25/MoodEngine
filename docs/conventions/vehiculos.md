# Convención de rigging de vehículos — MoodEngine

> **Propósito:** spec de cómo debe venir nombrado/orientado un modelo de vehículo
> para que el motor lo reconozca **sin adivinar**. Mismo rol que la doc de bones
> de Rockstar (RAGE) o el `.qc`/`.vehicle` de Source: el motor confía en la
> convención. Si el modelo la sigue, el importador (F2H82) lo arma directo; si
> no, cae a heurística por geometría + un paso de confirmación en el modal.
>
> **Establecida en F2H82 (2026-05-22)** alineando con Rockstar/Source/Unreal —
> no reinventamos, adoptamos.

---

## 1. Ejes y orientación

- **Forward = +Z**, **Up = +Y**, **Right = +X** (right-handed). Convención del
  engine (igual que Source / glTF-ish).
- Si el modelo fue exportado mirando para otro lado (-Z, ±X), **no se toca el
  asset**: se ajusta `meshYawOffsetDeg` en el `.moodvehicle` (0 / 90 / 180 / -90).
  Es como Rockstar/Source reconcilian la orientación del DCC.
- Escala: **1 unidad = 1 metro**. glTF ya viene en metros; FBX (cm) lo normaliza
  el loader (`aiProcess_GlobalScale`).

## 2. Nombres de nodos (canónicos)

El motor identifica las partes por el **nombre del nodo** (no del mesh-data).

| Parte | Nombre canónico |
|---|---|
| Rueda delantera izquierda | `wheel_FL` |
| Rueda delantera derecha | `wheel_FR` |
| Rueda trasera izquierda | `wheel_RL` |
| Rueda trasera derecha | `wheel_RR` |
| Chasis / carrocería | `chassis` (o cualquier nodo que **no** sea rueda) |

Orden fijo FL → FR → RL → RR (igual que `vehicle::WheelIndex`).

### Alias reconocidos (el importador los normaliza a los canónicos)

Para no obligar a renombrar modelos de Rockstar/Unreal/marketplaces, el analyzer
mapea estos alias (case-insensitive, match por substring/patrón):

| Canónico | Alias |
|---|---|
| `wheel_FL` | `wheel_lf`, `Wheel_FL`, `*front*left*`, `f_t_l`, `*delantera*izquierda*` |
| `wheel_FR` | `wheel_rf`, `*front*right*`, `f_t_r`, `*delantera*derecha*` |
| `wheel_RL` | `wheel_lr`, `*rear*left*`, `*back*left*`, `b_t_l`, `*trasera*izquierda*` |
| `wheel_RR` | `wheel_rr`, `*rear*right*`, `*back*right*`, `b_t_r`, `*trasera*derecha*` |

(Rockstar usa `lf/rf/lr/rr`; Unreal `FL/FR/BL/BR`; ambos cubiertos.)

## 3. Detección: **nombre primero, geometría después**

1. **Por nombre**: si los nodos matchean la convención o un alias → se confía en
   el nombre (modo "asset bien hecho", como un estudio).
2. **Fallback por geometría**: si no se identifican 4 ruedas por nombre, el
   analyzer las busca por posición — nodos chicos, Y bajo, cerca de las 4
   esquinas, tamaño similar — y clasifica FL/FR/RL/RR por el signo de X
   (izq/der) y Z (frente/atrás) del centroide.
3. **Confirmación**: el modal de importación muestra lo detectado y deja
   corregir antes de guardar. La heurística propone; el dev decide.

## 4. Qué se mide del modelo (auto) vs qué se pone a mano

- **De la malla (auto)**: dimensiones del chasis, radio/ancho de rueda, track,
  wheelbase (offset_z), centro de masa aproximado, yaw forward sugerido.
- **A mano (no está en la malla)**: peso, potencia/torque del motor, freno,
  fricción, gears. Se cargan vía **presets por tipo** (deportivo / sedán /
  camioneta / blindado) + ajuste fino. Igual que `handling.meta` (Rockstar) o
  el bloque `engine` del `.vehicle` (Source): valores que el diseñador tunea,
  nunca derivados de la geometría.

## 5. Config: `.moodvehicle` (JSON)

Data-driven, separado de la malla — mismo concepto que `.vehicle` (Source,
KeyValues) y `handling.meta` (Rockstar, XML). Schema v2; ver
`assets/vehicles/delorean/delorean_dmc12.moodvehicle` como referencia.

## 6. Fuera de alcance (v1)

- Vehículos != 4 ruedas (motos, tanques, camiones 6+). Abrir hito propio.
- Partes animables extra (puertas, capó, baúl) — Rockstar las nombra
  (`door_dside_f`, `bonnet`…); por ahora solo chasis + 4 ruedas.
