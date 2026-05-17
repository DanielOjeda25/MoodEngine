# PLAN AUDIT-2 — Tech debt sprint (continuación de AUDIT-1)

> **Estado**: en curso (2026-05-17).
> **Tag previsto**: `v1.49.2-audit-2`.
> **Origen**: el dev pidió continuidad inmediata post-AUDIT-1. La cadencia "1 audit cada ~5 hitos" arranca en serio desde AUDIT-3 (post-F2H67 aprox).

---

## 🎯 Objetivo

Cerrar la deuda técnica restante del code base (4 archivos sobre hard cap + audit preventivo de capas + backfill de docs históricos). Acotado a 1 día como AUDIT-1.

---

## 🧱 Bloques

### Bloque A — Splits restantes sobre hard cap

Skipeo `test_scene_serializer.cpp` 934 LOC (test file, menos crítico).

- **A.1** `EditorApplication_RunInteractions.cpp` 1065 LOC → sub-split por sub-modo. Candidatos naturales: click-to-select 3D, click-to-select ortho, polygon brush, clip tool, drag-edit, block tool, vertex/edge edit, marquee.
- **A.2** `EditorRenderPass.cpp` 888 LOC → extraer cada pass (shadow / ssao / ssr / bloom / color grading / post-process) como helper file.
- **A.3** `EditorUI.h` 836 LOC → es un header. Estrategia: extraer la declaración de cada panel a forward-decls (los `#include` pesados van solo en `EditorUI.cpp`).
- **A.4** `EditorProjectActions_CreateEntity.cpp` 813 LOC → split por categoría de entity (mesh / primitive / light).

### Bloque B — Layer dependency audit

Preventivo: grep de includes cruzados al revés. Reglas vigentes:

- `core/` NO incluye nada de fuera de `core/` + std + glm.
- `engine/` NO incluye `editor/` ni `player/` ni `tests/`.
- `editor/` + `player/` PUEDEN incluir `engine/` y `core/`.
- `systems/` (audio / animation / etc.) NO incluye `editor/` ni `player/`.

Reporte simple. Idealmente cero violaciones.

### Bloque C — Backfill HITOS.md (F2H55-F2H61)

Migrar los 7 entries fat más recientes a `docs/hitos/F2H<N>.md`:
- F2H55 (bloom)
- F2H56 (SSAO)
- F2H57 (UX pivot SFM picker)
- F2H58 (color grading)
- F2H59 (primitivas + reorg UX)
- F2H60 (CSM + 5 iter polish)
- F2H61 (SSR)

Los anteriores (F2H1-F2H54) quedan inline en HITOS.md hasta que emerja dolor.

### Bloque D — Pure helpers audit mínimo

Identificar 1-3 funciones complejas en el code base (preferentemente en el nuevo `_RunInteractions.cpp`) que mezclan lógica de negocio con GL/ImGui/SDL y se pueden extraer como funciones puras testeables.

**Solo identificar**, no necesariamente extraer en este audit. Documentar candidatos en `AUDIT_2.md` para AUDIT-3.

### Cierre

`docs/audits/AUDIT_2.md` + commit `audit(AUDIT-2)` + tag `v1.49.2-audit-2`. Plan archivado a `docs/archive/plans/PLAN_HITO_AUDIT_2.md`.

---

## NO entra en AUDIT-2

- Splits de archivos en SOFT cap (21 archivos, ninguno crítico).
- Pure helpers refactor real (solo identificación — extracción a AUDIT-3+).
- Backfill de F2H1-F2H54 (inline en HITOS.md, no genera fricción operacional).
- Dead code audit.
- DOD refactor de hot paths.

---

## Estimado

~5-6h. Si Bloque A se va de scope, recortamos al #1 ofensor y los otros 3 quedan para AUDIT-3.
