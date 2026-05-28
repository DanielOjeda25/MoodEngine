# PLAN F3H22 — Properties Editor con icons laterales (Blender style)

**Estado:** **A DEFINIR** (arrancar tras F3H21).
**Predecesor:** F3H21 (viewport pro — numpad views + render modes).
**Origen:** insertado en reorden 2026-05-27 a pedido del dev al cerrar F3H21:
> *"creo que debemos hacer un cambio importante, como lo hace blender, que tiene un panel con los iconos, y ahi el icono de cada seccion, sea el de materiales, scripts, etc, esto se que es un hito mas grande pero podriamos mejorar exponencialmente esto"*

Estaba anotado en `backlog-ux-gaps-editor` como follow-up; promovido a hito propio + insertado antes de F3H23 (Profiler).

---

## Avance de Sub-fase 3.4 (post-F3H21, reorden 2026-05-27)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  ✅ Viewport pro: numpad views + 4 render modes
F3H22 –  — Properties Editor con icons laterales (Blender style) ⬅ próximo
F3H23 –  — Performance feedback: Profiler + Stats overlay
F3H24 –  — Comunicación al dev: Console + Toasts
F3H25 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

**Hoy:** el Inspector es un panel scrollable largo con todos los componentes de la entity seleccionada apilados verticalmente. Una entity con muchos componentes (Transform + MeshRenderer + Light + Audio + Animator + Vehicle + Physics + Inventory + Script + ...) requiere scrollear y abrir/cerrar headers para encontrar lo que el dev busca.

**Post-F3H22:** una **barra vertical de icons** al lateral izquierdo del Inspector con categorías clickeables (estilo Properties Editor de Blender / Details panel de Unreal). Click en un icon muestra SOLO esa categoría — el resto se oculta. Categoría activa destacada. Cada icon con tooltip i18n. Estado per-instalación: la categoría seleccionada se persiste en `UserSettings.editor.inspectorActiveCategory`.

**Mecánica del editor:** seleccionás una entity → ves la barra de icons; click en "Mesh" → ves solo MeshRenderer + materiales; click en "Physics" → ves solo Rigidbody + Collider; click en "Script" → ves solo ScriptComponent + exposed properties; etc.

---

## Scope candidato

### Categorías

Mapping inicial (a ajustar según los componentes reales del engine):

| Icon | Categoría | Componentes que muestra |
|---|---|---|
| `ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT` | **Object** | TransformComponent, TagComponent, VisGroupMembership |
| `ICON_FA_CUBE` | **Mesh** | MeshRendererComponent, BrushComponent |
| `ICON_FA_PALETTE` | **Materials** | Materials del MeshRenderer (vista expandida) |
| `ICON_FA_LIGHTBULB` | **Light** | LightComponent |
| `ICON_FA_PERSON_RUNNING` | **Animation** | AnimatorComponent |
| `ICON_FA_VOLUME_HIGH` | **Audio** | AudioSourceComponent, ListenerComponent |
| `ICON_FA_BOLT` | **Physics** | RigidBodyComponent, ColliderComponent, JointComponent, RagdollComponent, ClothComponent |
| `ICON_FA_WIND` | **VFX** | ParticleEmitterComponent, ForceFieldComponent, TriggerComponent |
| `ICON_FA_CODE` | **Script** | ScriptComponent, exposed properties |
| `ICON_FA_BOX_OPEN` | **Inventory** | InventoryComponent, ItemPickupComponent |
| `ICON_FA_CAR` | **Vehicle** | VehicleComponent |
| `ICON_FA_COMMENT` | **Dialog** | DialogComponent |
| `ICON_FA_LIST_CHECK` | **Quest** | QuestComponent |
| `ICON_FA_GLOBE` | **Environment** | EnvironmentComponent (singleton de la escena) |
| `ICON_FA_VIDEO` | **Camera** | CameraComponent |

15 categorías. Cada una visible en la barra SOLO si la entity tiene al menos 1 componente de esa categoría (no spammear icons grises sin contenido).

### Persistencia + UX

- `UserSettings.editor.inspectorActiveCategory` (string id de categoría, default "object").
- La categoría seleccionada se preserva al cambiar de entity (sticky).
- Si la entity nueva no tiene la categoría activa → fallback a la primera disponible.
- Botón "All" en la barra para volver al modo legacy (todo en una lista scrollable) — opt-in si el dev lo prefiere.

### Tests

- Categoría persistida roundtrip.
- Filtrado por categoría: dada una entity con N componentes, validar que solo los de la categoría activa se renderizan.
- Fallback al cambiar de entity (categoría activa no presente → primera disponible).

---

## Decisiones a tomar al arrancar

1. **¿Single categoría a la vez o multi-pin?** Blender = single (un icon activo). Unreal Details Panel = scroll vertical con todo abierto + filtro de texto. Opción intermedia: single con botón "All" para volver al modo legacy.
2. **¿Iconos FontAwesome existentes o pack custom?** FontAwesome ya está en el repo (15+ candidatos en `IconsFontAwesome6.h`). Custom pack sería trabajo extra de arte sin upside claro.
3. **¿Mostrar icons grises (categoría sin componentes) o ocultarlos?** Blender los muestra grises (icon visible pero deshabilitado). Unreal solo muestra los activos. Decisión UX.
4. **¿Posición de la barra de icons? Izquierda (Blender) o arriba (Unity / tabs)?** Blender es izquierda en columna; Unity es arriba en fila. Izquierda usa el lateral del panel sin reducir ancho útil del Inspector.
5. **¿Promover las 15+ categorías de una o por bloques?** Big-bang vs incremental. Big-bang permite testing completo en una sesión; incremental (5-7 categorías primero) reduce riesgo si algo se rompe.

---

## Lo que NO toca F3H22

- F3H23+ (Profiler / Console / Toasts / Crash recovery): hitos propios.
- Cambios al modelo de datos de componentes — solo se reorganiza el rendering del Inspector, no la ECS.
- Edición multi-entity per-categoría (multi-edit F3H8 sigue funcionando como hoy — no se cambia la mecánica, solo qué se muestra).
- Refactor de cada `InspectorPanel_*.cpp` individual (los 15+ archivos) — la categoría es un wrapper que decide qué llamar, los paneles internos no se tocan.
- Filtro de texto del Inspector (sería un nice-to-have separado, no es Blender-style).
- Drag & drop de categorías (Blender no lo tiene en Properties Editor).
