# PLAN AUDIT-1 — Tech debt sprint inter-hitos

> **Estado**: en curso (2026-05-17).
> **Tag previsto**: `v1.49.1-audit-1`.
> **Origen**: pedido del dev tras cerrar F2H62. Convención industria (Naughty Dog / id Software / Unity) de "tech debt sprints" cada N features grandes. Acordado en conversación: arrancar acotado al primer AUDIT, expandir scope en los siguientes según resultado.

---

## 🎯 Objetivo

Bajar la deuda técnica acumulada en 62 hitos de Fase 2 **sin meter features nuevas**. Audit acotado a 1 día. Si el patrón funciona, se repite cada ~5 hitos grandes con scope ligeramente más amplio (AUDIT-2 sumará layer audit + pure helpers audit).

---

## 🧱 Bloques (scope acordado)

### Bloque A — Sizeometer + split del top 3 ofensores hard cap

- Script `tools/sizeometer.sh` (bash, multiplataforma) que reporta los 20 archivos `.cpp`/`.h` más grandes del repo (excluyendo `_deps/`, `build*/`, third-party).
- Reglas vigentes (memory rule `feedback_file_size_limit`): soft 500 LOC / hard 800 LOC.
- Identificar el top ofensores que pasan hard cap.
- **Splitear los 3 archivos top** que sobrepasen el hard cap, usando el patrón ya existente en el repo: archivos parciales sobre la misma clase (ej: `InspectorPanel_MeshRenderer.cpp` / `InspectorPanel_Transform.cpp` / etc, todos definiendo métodos de `InspectorPanel`).
- Soft cap (500-800) queda en la lista de "vigilar próximos audits", no se toca ahora.

### Bloque E — Slim de docs

Migración del modelo "fat entry" (HITOS.md con ~1000 palabras por hito) al modelo "índice + per-hito file":

- **HITOS.md**: convertir todos los entries existentes a **una línea** cada uno con formato:
  ```
  - [x] **F2H<N>** — <título corto> (<fecha>) — tag `v<X.Y.Z>-fase2-hito<N>` — [detalle](hitos/F2H<N>.md)
  ```
  Los marcadores de cierre de sub-fase se conservan.
- **`docs/hitos/F2H<N>.md`**: 1 archivo por hito existente, con el contenido detallado que hoy vive en HITOS.md. Inmutable post-creación (es histórico).
- **ESTADO_ACTUAL.md**: dejar solo la sección "0" del último hito cerrado + bullet list "pendientes del plan". Las secciones "0bis" históricas se borran (ya viven en HITOS.md como índice + en los archivos per-hito).
- **DECISIONS.md**: queda como está (append-only, se navega por búsqueda).
- **PLAN_HITO_F2H<N>.md** activo en `docs/`, archivado al cerrar (ya es el patrón).

### Bloque cierre — Reporte + tag

- `docs/audits/AUDIT_1.md` (1 página) con:
  - Top 20 LOC before / after.
  - Archivos spliteados.
  - Reducción de tamaño de HITOS.md + ESTADO_ACTUAL.md.
  - Próximos candidatos para AUDIT-2.
- Commit `audit(AUDIT-1)` + tag `v1.49.1-audit-1`.
- Plan archivado a `docs/archive/plans/PLAN_HITO_AUDIT_1.md`.

---

## NO entra en AUDIT-1

- **Layer dependency audit** (grep de includes cruzados al revés). → AUDIT-2.
- **Pure helpers audit** (identificar lógica entremezclada con GL/ImGui y extraer helpers testeables). → AUDIT-2.
- **Dead code audit** (funciones/clases sin referencias). → AUDIT-3 si emerge necesidad.
- **DOD refactor de hot paths** (frame loop con OO innecesario). → hito propio si el profiling lo justifica.
- **Tocar features**. Cero scope creep. Si emerge un bug durante el audit, se anota y se difiere a un fix commit aparte.

---

## Estimado

**1 día** total. Si Bloque A se va de scope (más de 3 archivos top ofensores con muchas dependencias acopladas), spliteamos solo el #1 y el resto agendamos.

---

## Convenciones que el AUDIT establece para futuros hitos

1. **Cierre de hito**: el detalle largo va al archivo `docs/hitos/F2H<N>.md`. HITOS.md solo gana 1 línea.
2. **ESTADO_ACTUAL.md**: cada cierre **reemplaza** la sección 0 (no se acumulan "0bis").
3. **Archivo nuevo o modificado >800 LOC**: split obligatorio antes de commitear el hito que lo genera.
