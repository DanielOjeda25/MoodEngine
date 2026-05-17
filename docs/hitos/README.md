# Per-hito archives

Cada hito cerrado de la Fase 2 (desde F2H62 / AUDIT-1 en adelante) tiene su entry de detalle largo en este directorio como `F2H<N>.md`.

Convención establecida en **AUDIT-1** (2026-05-17) para combatir el crecimiento descontrolado de `HITOS.md` y `ESTADO_ACTUAL.md`:

- **`HITOS.md`** = índice maestro. 1 línea por hito (`F2H<N> — <título> — tag — fecha — [detalle](hitos/F2H<N>.md)`). Estado (`[x]`/`[ ]`) + marcadores de cierre de sub-fase.
- **`docs/hitos/F2H<N>.md`** = el detalle largo del hito (qué entregó, bloques A-X, decisiones, limitaciones, citas del dev). Inmutable post-cierre.
- **`ESTADO_ACTUAL.md`** = solo el **último** hito cerrado + pendientes. Sin "0bis" históricos acumulados (esos viven en los per-hito files).
- **`DECISIONS.md`** = sigue siendo append-only, se navega por búsqueda.

## Para qué sirve cada archivo

| Archivo | Frecuencia de lectura | Tamaño objetivo |
|---|---|---|
| `HITOS.md` | Alta (snapshot del proyecto) | ~200-400 líneas |
| `ESTADO_ACTUAL.md` | Alta (handoff entre sesiones) | ~100-200 líneas |
| `docs/hitos/F2H<N>.md` | Baja (cuando se necesita contexto histórico de un hito específico) | ~50-200 líneas por archivo |
| `DECISIONS.md` | Media (búsqueda de decisiones específicas) | crece sin freno (es OK) |

## Cómo redactar un `F2H<N>.md` nuevo

Template recomendado:

```markdown
# F2H<N> — <título corto del hito>

> **Tag**: `vX.Y.Z-fase2-hito<N>`
> **Fecha de cierre**: YYYY-MM-DD
> **Sub-fase**: 2.<N> — <nombre>
> **Validación del dev**: <cita verbatim>

## Qué entregó

<Bloques A-X breve>

## Decisiones clave

<Lista de decisiones — detalle en DECISIONS.md>

## Limitaciones v1

<Lista de lo que NO entró>

## Limitaciones tipográficas

<Algo digno de mencionar para la memoria operacional>

## Plan archivado

[`archive/plans/PLAN_HITO_F2H<N>.md`](../archive/plans/PLAN_HITO_F2H<N>.md)
```

## Backfill de hitos viejos

Los entries de F2H1 a F2H61 todavía viven inline en `HITOS.md` con su formato "fat" original. Se irán migrando a per-hito files en **AUDIT-2** (y siguientes) según vaya necesitándose contexto. No hay urgencia — el contenido ya está versionado en git history y los tags.
