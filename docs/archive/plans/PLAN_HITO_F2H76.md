# PLAN HITO F2H76 — Preferencias + temas visuales del editor

> **Estado**: borrador para aprobar con el dev.
> **Sub-fase**: 2.7 (UI/UX final + cierre Fase 2). Primer hito de la sub-fase.
> **Predecesor**: F2H75 (cloth, cierre Sub-fase 2.4).

## Qué siente el usuario

Abre **Editar → Preferencias** y encuentra, por primera vez, un lugar donde
ajustar el editor a su gusto. Lo primero que puede cambiar: el **tema visual**
(oscuro / claro / y alguno con más personalidad). Elige uno y el editor cambia
**al instante**; la próxima vez que lo abra, sigue con el tema elegido. Es el
primer paso de "el editor se siente mío" y deja la **casa de los ajustes** lista
para que después cuelguen ahí los atajos de teclado, la escala de UI, etc.

**Fuera de scope de este hito** (siguientes hitos de 2.7):
- Atajos de teclado configurables (hito propio — es el frente grande).
- Tutorial / onboarding in-app.
- Escala de UI, temas con colores 100% custom editables por el usuario (este
  hito trae un set de temas predefinidos, no un editor de paletas).
- El bump a `v2.0.0` (eso es el CIERRE de la sub-fase 2.7, no este hito).

## Estado de partida (auditoría)

- `UserSettings` (`src/core/UserSettings.{h,cpp}`) + `settings.json` en
  `%APPDATA%/MoodEngine/`: infra de load/save sólida, hoy solo guarda `language`.
- ImGui style: hardcodeado a `ImGui::StyleColorsDark()` al startup
  (`EditorApplication_Init.cpp:212`). Sin theme switching.
- Menú: hay `View → Language` + `Help → About`. No hay entrada de Preferencias.
- i18n en/es sólido (`I18n::T`).

## Bloques

### A — Extender `UserSettings` con el tema
- Agregar campo `theme` (string id, ej. `"dark"` default) al struct + a la
  serialización JSON. Getters/setters `theme()` / `setTheme()`, mismo patrón
  que `language()`. Back-compat: si el JSON no trae `theme`, default `"dark"`.

### B — Registro de temas (paletas definidas en código)
- `src/editor/ui/EditorThemes.{h,cpp}` (o similar): un set de temas built-in,
  cada uno una función que rellena `ImGuiStyle::Colors` (+ spacing/rounding si
  aporta). Set inicial:
  - **Dark** (el actual `StyleColorsDark`, baseline).
  - **Light** (`StyleColorsLight`).
  - **Midnight** (dark con tinte azulado, más contraste — "personalidad").
  - (opcional 4°: **Sepia**/cálido si el tiempo da.)
- API: `applyTheme(const std::string& id)` que aplica al `ImGui::GetStyle()`, +
  `availableThemes()` (lista de {id, i18nKey}) para poblar el combo.
- Themes con colores 100% custom editables por JSON = futuro (este hito son
  presets); el diseño deja la puerta abierta (un id → función).

### C — Aplicar el tema al startup + panel de Preferencias
- **Startup**: reemplazar el `StyleColorsDark()` hardcodeado por
  `applyTheme(UserSettings::theme())` en `EditorApplication_Init`.
- **Menú**: agregar **Editar → Preferencias…** (`MenuBar.cpp`) que abre el modal.
- **Modal "Preferencias"** (mismo patrón que About/Welcome): combo de Tema con
  **preview live** (al cambiar el combo, `applyTheme` inmediato para que el dev
  lo vea), y persiste a `settings.json` al confirmar/cambiar. Estructurado con
  espacio para tabs futuros (Tema / … ) aunque el v1 tenga solo Tema.
  - Decisión a confirmar con el dev: ¿centralizar también el selector de idioma
    acá (sacándolo de View → Language) o dejarlo donde está? (ver Dudas.)

### D — i18n
- Claves en/es: `editor.menu.edit.preferences`, `editor.modal.preferences.title`,
  `editor.preferences.theme`, `theme.dark` / `theme.light` / `theme.midnight`
  (+ sepia si entra). Mantener paridad en/es (lo chequea `test_i18n`).

### E — Cierre
- `docs/hitos/F2H76.md`, one-liner en `HITOS.md`, actualizar sección 0.1 de
  `ESTADO_ACTUAL.md`, archivar este plan. Tag `v1.67.0-fase2-hito76`.
- (El `v2.0.0` se reserva para el cierre de toda la sub-fase 2.7.)

## Riesgos / dudas
1. **¿Idioma se muda a Preferencias?** Tenerlo en dos lados es redundante. Mi
   sugerencia: mover `View → Language` a la nueva Preferencias (un solo lugar de
   ajustes). Lo confirmo con el dev antes de tocar el menú View.
2. **Persistencia inmediata vs al confirmar**: el combo aplica live; guardar a
   disco en cada cambio es barato (settings.json es chico). Voy con guardar al
   cambiar (sin botón "OK") salvo que el dev prefiera un OK/Cancel explícito.
3. **Player**: este hito tema-iza el EDITOR. El runtime del juego (MoodPlayer)
   tiene su propio look; no entra acá.
4. **Legibilidad de los temas**: validar runtime que todos los paneles se lean
   bien en Light (el editor fue diseñado en Dark; algún `PushStyleColor`
   hardcodeado —ej. botón Play/Stop— puede chocar). Ajuste menor si aparece.

## Estimación
~2-4h: A (0.5h) · B (1-1.5h, definir paletas) · C (1h) · D (0.5h) · E (0.5h).

## Orden propuesto
A → B → C → D → validación runtime con el dev (probar los temas) → E.
