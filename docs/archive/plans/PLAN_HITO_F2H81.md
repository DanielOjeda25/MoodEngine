# PLAN HITO F2H81 — Preview de animaciones + Inspector más claro

> **Estado**: stub — a planear en detalle con el dev antes de implementar.
> **Sub-fase**: 2.7 (UI/UX final + cierre Fase 2). Sexto hito.
> **Predecesor**: F2H80 (miniaturas 3D).
> **Decisión del dev**: juntar las dos mejoras (animaciones + Inspector) en un
> solo hito.

## Qué siente el usuario

Dos mejoras de "claridad visual" del editor en un hito:

1. **Preview de animaciones**: en el Asset Browser (tab Animations) y/o al
   asignar un clip, ve la **animación reproduciéndose** sobre un personaje — no
   un ícono estático. Reconoce "esto es un caminar / salto / idle" de un vistazo.

2. **Inspector más claro**: hoy el Inspector "marea entre tantas opciones" (17
   secciones de componentes). Se reduce la carga visual y se mejora la
   navegación para que el dev encuentre lo que busca sin perderse.

## Parte 1 — Preview de animaciones

Un clip `anim_*.fbx` es solo keyframes (tracks con `boneIndex=-1` hasta
bindearse). Para verlo hay que: (1) montarlo sobre un **skeleton** compatible,
(2) tener un **personaje de referencia** (mesh skinneado), (3) **reproducirlo**
en un viewport de preview (no una textura estática como F2H80). Es un
mini-reproductor con loop.

**A definir con el dev (mecánicas):**
- Personaje de referencia: ¿maniquí default fijo, o el último skinned mesh
  usado? (Los clips Mixamo asumen rig humanoide.)
- ¿Preview animado en vivo (1 draw/frame del panel — OK acá, es 1 solo con foco,
  igual que la esfera del Material Editor) o scrubbing manual?
- ¿Dónde vive? (panel dedicado / hover-preview en el tab Animations / al
  seleccionar el clip).

**Piezas reutilizables**: `MeshThumbnailRenderer` (F2H80, FBO + setup PBR,
extensible a un preview con skeleton + pose por frame), `AnimationSystem` /
`Skeleton` / `AnimationClip` (Hito 19, F2H49 standalone clips).

## Parte 2 — Inspector más claro

**Auditar primero qué ya existe** (F2H77 mostró que el editor suele estar más
pulido de lo que un reporte superficial sugiere — ej. las secciones del
Inspector YA son colapsables vía `CollapsingHeader`). Candidatos (a confirmar
tras la auditoría):
- Secciones poco usadas **arrancan colapsadas** y recuerdan su estado.
- **Íconos por componente** en los headers para escanear con la vista (ya hay
  íconos en varios headers desde F2H37 — verificar cobertura).
- **Búsqueda/filtro** de componentes en Inspectores muy poblados.
- Agrupar/ordenar secciones por relevancia (Transform siempre arriba).
- Reducir ruido visual (separadores, padding) — sin romper el espaciado de F2H77.

## Cierre

`docs/hitos/F2H81.md`, one-liner en `HITOS.md`, sección 0.1 de `ESTADO_ACTUAL.md`,
nota en `DECISIONS.md`, archivar este plan. Tag `v1.72.0-fase2-hito81`.

## Pendientes de 2.7 después de F2H81 (camino a v2.0.0)

- **Atajos de teclado configurables** (era F2H42 del plan original; nunca se
  hizo — hoy hardcodeados): sistema de keybindings + UI + persistencia + presets.
- **Cierre Fase 2 + `v2.0.0`**: suite verde, docs al día, release notes, recap,
  planning Fase 3.
- Menores: Ctrl+S "guardar como", unificar Undo en Material/Item/Quest.
- **Tutorial in-app**: el dev lo difirió a post-Fase 2.
