# PLAN F3H17 — Drag & drop con feedback visual

**Estado:** **A DEFINIR** (arrancar tras F3H16).
**Predecesor:** F3H16 (hover preview ampliada del Asset Browser).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Drag & drop con feedback visual".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 –  — ⬅ próximo: Drag & drop con feedback visual
F3H18 –  — Validador de assets rotos
F3H19 –  — Rename con cascada
```

---

## Norte

`PLAN_FASE3.md` declara:
> **F3H17 — Drag&drop con feedback visual.**
> Drag de asset al viewport: cursor cambia + drop zone destacada. Drop al Inspector: highlight del field compatible. Cancel con Esc.

**Mecánica del editor (cómo se siente):** el dev arrastra un mesh del Asset Browser. Inmediatamente:
- El cursor cambia (icono fantasma del thumb o cursor especial).
- El viewport perspectiva se "ilumina" sutilmente (borde brillante o tinte) — el dev sabe que ese es el drop target válido.
- Si el dev arrastra sobre el Inspector y hay un field compatible (ej. slot de material en MeshRenderer), ese field se destaca con borde de "drop OK".
- Suelta sobre un target válido → spawna entidad / asigna material / etc.
- Presiona Esc o suelta sobre área no válida → drag se cancela, nada cambia.

---

## Scope candidato

### Trabajo principal

1. **Drop zones destacadas**: cuando un drag está activo (ImGui::IsDragDropActive equivalente o flag global), iterar los viewports + Inspector + Hierarchy para resaltar los que aceptan el tipo del payload. Color overlay sutil (azul translúcido) o borde tipo "halo".

2. **Cursor feedback**: ImGui ya maneja el cursor durante drag&drop (muestra el contenido del `BeginDragDropSource`). Podemos enriquecerlo con icono "+" verde al estar sobre target válido, o "X" rojo al estar sobre área inválida.

3. **Cancel con Esc**: hook al keyboard durante drag. Si Esc presionado mientras drag activo → cancelar (ImGui::ClearDragDrop o equivalente).

4. **Highlight de Inspector slots compatibles**: en el `InspectorPanel`, cuando hay drag activo de un tipo (ej. MOOD_MESH_ASSET), los `Image`/`Button` de los slots compatibles (Mesh slot del MeshRenderer, etc) reciben un borde de "drop OK".

### Sites a tocar

- `EditorViewportPanel.cpp`: detectar drag activo + dibujar halo en el borde del viewport.
- `OrthoViewportPanel.cpp`: igual.
- `InspectorPanel_*.cpp`: en cada slot drag-target, condicionar el borde a `isDragActiveOfType()`.
- `HierarchyPanel.cpp`: hover de un entry durante drag muestra "soltar para asignar como hijo" o similar.
- `AssetBrowserPanel_Tabs.cpp`: el `BeginDragDropSource` ya existe — quizás enriquecer el preview drag (mostrar thumb mayor mientras se arrastra).

### Decisiones a tomar al arrancar

1. **Estilo del halo**: ¿borde sólido (tipo Windows Explorer drag-over), tinte interior (Unity 2022 drop overlay), o pulse animado?
2. **Inspector slot highlight**: ¿borde discrete (1-2 px), background tinted, o glow?
3. **Cancel con Esc**: ¿solo durante drag o también como atajo global "cancelar última acción"?
4. **Cobertura**: ¿solo Viewport + Inspector + Hierarchy, o también incluir Material Editor + Vehicle Editor + Item/Quest editors (que pueden aceptar drops)?

---

## Alternativas a F3H17

### B) F3H18 (Validador de assets rotos)

Saltar el drag&drop polish e ir al validador (panel que liste assets con refs muertas). Más impactante para proyectos grandes; menos pulido inmediato pero rescata productividad.

### C) Backlog UX (memoria `backlog-ux-gaps-editor`)

- Spawn de ForceField/Cloth desde "+ Crear Entidad".
- "Agregar sonido al activar mesh" workflow.

Items chicos que cierran fricciones pre-existentes.

---

## Recomendación

Yo (Claude) sugiero **opción A — Drag & drop con feedback visual**:
1. Sigue el orden del plan.
2. UX visible inmediato; el dev "siente" el editor más vivo.
3. Scope acotado (no diseña nada nuevo del schema, solo polish del drag flow que ya existe).

**Preguntas al dev cuando arranque F3H17:**
1. ¿Confirmás A o querés B/C?
2. ¿Cobertura: solo Viewport + Inspector + Hierarchy, o también Material/Vehicle/Item/Quest editors?
3. ¿Estilo del halo: borde sólido, tinte, o pulse animado?

---

## Lo que NO toca F3H17

- F3H18 (Validador): hito propio.
- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
- Async preview generation / GPU streaming: backlog.
