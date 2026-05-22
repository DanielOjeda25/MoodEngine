# PLAN HITO F2H81 — Mini-player de animaciones (preview de clips)

> **Estado**: stub — a planear en detalle con el dev antes de implementar.
> **Sub-fase**: 2.7 (UI/UX final + cierre Fase 2). Sexto hito.
> **Predecesor**: F2H80 (miniaturas 3D — del que se difirió esto).

## Qué siente el usuario

En el Asset Browser (tab Animations) y/o al asignar un clip, ve una
**previsualización del clip reproduciéndose** sobre un personaje — no un ícono
estático. Reconoce "esto es un caminar / un salto / un idle" de un vistazo.

## Por qué es su propio hito (diferido de F2H80)

Un clip `anim_*.fbx` es solo keyframes (tracks con `boneIndex=-1` hasta
bindearse). Para verlo hay que: (1) montar el clip sobre un **skeleton**
compatible, (2) tener un **personaje de referencia** (mesh skinneado) para
posar, (3) **reproducir** la animación en un viewport de preview (no una textura
estática como las miniaturas de F2H80). Es un mini-reproductor con loop, no un
thumbnail.

## A definir con el dev (mecánicas)

- ¿Personaje de referencia fijo (un maniquí default) o el último skinned mesh
  usado? (Mixamo clips asumen rig humanoide.)
- ¿Preview animado en vivo (cuesta 1 draw/frame del panel) o "scrubbing"
  manual? F2H80 evitó animar grillas; acá es 1 solo preview con foco → animar
  está OK (igual que el Material Editor anima su esfera).
- ¿Dónde vive? (panel dedicado / hover-preview en el tab Animations / al
  seleccionar el clip).

## Piezas reutilizables

- `MeshThumbnailRenderer` (F2H80): FBO + setup PBR; extensible a un preview con
  skeleton + pose por frame.
- `AnimationSystem` / `Skeleton` / `AnimationClip` (Hito 19, F2H49 standalone
  clips).

## Sucesor planeado: F2H82 — mejorar el Inspector

El dev reportó que el Inspector "marea entre tantas opciones" (17 secciones de
componentes). Reducir carga visual / mejorar navegación: secciones poco usadas
colapsadas con memoria, agrupar/ordenar, búsqueda de componentes, íconos por
componente. (Auditar primero qué ya existe — F2H77 mostró que el editor suele
estar más pulido de lo que un reporte superficial sugiere: las secciones YA son
colapsables vía `CollapsingHeader`.)
