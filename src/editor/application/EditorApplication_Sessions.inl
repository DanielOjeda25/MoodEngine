// F2H83: structs de sesión / state del editor que viven dentro de
// `EditorApplication` (refactor para mantener el header <800 LOC).
// Este archivo se incluye desde la sección `private:` de la clase —
// el preprocesador inserta los structs como tipos anidados de
// `EditorApplication` sin cambiar el resto del código (referencias
// `m_orthoDragSession.startPositions`, etc. siguen resolviendo igual).
//
// NO meter aquí métodos miembro ni state que necesite ser construido
// por la clase fuera del .inl — solo definiciones de tipo POD.

/// @brief F2H29 Bloque B: sesion de drag-edit en orto. Una sesion =
///        1 LMB-down sobre brush + drag (>4 px) + LMB-up. Captura
///        las posiciones iniciales de TODAS las entidades del
///        SelectionSet con TransformComponent al arrancar el drag,
///        para poder pushear un MultiEditTransformCommand con el
///        before/after preciso al cerrar. Solo 1 sesion activa a
///        la vez (un orto a la vez); los otros ortos se ignoran
///        mientras hay sesion activa.
struct OrthoDragSession {
    bool active = false;
    int  orthoIdx = -1; // 0=Top, 1=Front, 2=Side
    std::vector<std::pair<Entity, glm::vec3>> startPositions;
};

/// @brief F2H29 Bloque C: sesion de block tool en orto. Activada
///        cuando el LMB-down inicia el drag en EMPTY space (sin
///        brush bajo el cursor). Durante el drag dibuja un AABB
///        cyan via debugRenderer (visible en perspectiva 3D); al
///        soltar, si el rectangulo supera `m_hammerSnapStep` en
///        ambos ejes del view, materializa un Box brush con esas
///        dimensiones + altura default (`snap * 4`) sobre el eje
///        perpendicular.
struct OrthoBlockToolSession {
    bool active = false;
    int  orthoIdx = -1;
    // AABB del preview cyan (en world space) computado cada frame en el
    // run loop. EditorRenderPass lo re-encola en el debugRenderer antes
    // de cada renderOrthoView para que el preview sea visible en los 3
    // ortos + perspectiva. valid=false antes del primer frame de drag.
    glm::vec3 previewMin{0.0f};
    glm::vec3 previewMax{0.0f};
    bool      previewValid = false;
};

/// @brief F2H31 Bloque B: sesion de marquee select en orto. Activada
///        cuando el LMB-down inicia drag en EMPTY space + m_mapTool
///        == Select. Cada frame el panel reporta `dragState().ndcCur`
///        que se usa para dibujar el rectangulo amarillo (4 lineas
///        en world space sobre el plano del view, via debugRenderer).
///        Al soltar, hit-test cada entidad seleccionable: AABB world
///        proyectado a ndc; si CUALQUIER corner cae dentro del rect
///        del marquee, hit. Aplica modifiers Shift/Ctrl al SelectionSet.
struct OrthoMarqueeSession {
    bool active = false;
    int  orthoIdx = -1;
};

/// @brief F2H32 Bloque B: sesion del clip tool. Activa cuando
///        m_mapTool == Clip + el dev hace primer click en orto.
///        Captura p1; segundo click captura p2 -> plano definido.
///        Tecla T durante la sesion cycle keepMode. Enter confirma
///        (splitea brushes seleccionados); Esc cancela.
struct ClipToolSession {
    bool active = false;
    int  orthoIdx = -1;  // -1 hasta el primer click
    bool hasP1 = false;
    bool hasP2 = false;
    glm::vec3 p1World{0.0f};
    glm::vec3 p2World{0.0f};
    ClipKeepMode keepMode = ClipKeepMode::Front;
};

/// @brief F2H30 Bloque B: sesion de vertex/edge edit en orto.
///        Activa cuando el LMB-down en Vertex/Edge sub-mode
///        impacta un vertex/edge del brush active. Captura los
///        planos pre + lista de planos a mutar (3 para vertex,
///        2 para edge). Cada frame aplica delta_world a los
///        planos via `d_new = d_old - dot(n, delta_local)` con
///        `delta_local = R^-1 * delta_world` (inverso de la
///        rotacion del worldMatrix). Al soltar pushea
///        EditBrushGeometryCommand.
struct OrthoVertexEditSession {
    bool active = false;
    int  orthoIdx = -1;
    Entity brush;                    // entidad con BrushComponent
    std::string brushTag;             // para el command (robusto a remap)
    std::vector<Plane> planesBefore;  // snapshot de TODAS las caras
    std::vector<u32>   incidentPlanes; // que planos mutar
    glm::vec3 tfPosBefore{0.0f};      // snapshot del transform.position
    // Pivot inicial del vertex/edge en LOCAL space del brush. Para Vertex
    // es la posicion del vertex; para Edge es el midpoint del edge.
    // Usado para snap ABSOLUTO al grid:
    // pivotNew = round((pivotStart + delta_local) / snap) * snap.
    glm::vec3 pivotLocalStart{0.0f};
};

/// @brief F2H30 Bloque C: sesion del "pincel poligonal". Activado
///        con tecla `B` desde el workspace "Editor de mapas".
///        Cada click LMB en una orto agrega un punto (snappeado
///        al grid) al polígono. Enter cierra (valida convex CCW
///        + crea prisma); Esc cancela. Locked a 1 sola orto
///        (la primera donde se clickea) para mantener coplanaridad.
struct PolygonDrawSession {
    bool active = false;
    int  orthoIdx = -1;          // -1 hasta el primer click
    u32  axisIndex = 1;           // eje perpendicular (0=X, 1=Y, 2=Z)
    std::vector<glm::vec3> pointsWorld;
};

/// @brief F2H30 Bloque D: state del modal G/R/S estilo Blender. El
///        dev presiona `G`/`R`/`S` con un brush selecto (y mouse
///        sobre el viewport perspectivo, fuera de un text input);
///        eso captura el snapshot del Transform de cada entidad
///        seleccionada y arranca un drag virtual. Mover el cursor
///        actualiza Position (G) / Rotation (R) / Scale (S) en
///        vivo respecto al pivote = centroide del SelectionSet.
///        Click izq confirma (push MultiEditTransformCommand);
///        Esc cancela (revert al startValue de cada entidad).
///        Tecla X/Y/Z durante el modal lockea/destrabea el axis
///        constraint (alineado con Blender). Convive con el gizmo
///        de flechas — ambos terminan llamando al mismo command.
struct ModalShortcutEntry {
    Entity   entity;
    glm::vec3 startValue{0.0f};
};
struct ModalShortcutState {
    bool active = false;
    // Field se reusa de EditTransformCommand pero como int para evitar
    // pull del header desde EditorApplication.h. 0=Position, 1=Rotation,
    // 2=Scale. Mismo orden que el enum del command.
    int  field = 0;
    // axisLock: -1 = sin lock (libre), 0/1/2 = X/Y/Z.
    int  axisLock = -1;
    glm::vec2 mouseStart{0.0f};
    // Pivote en world. Para G es el delta del cursor en plano de
    // camara; para R/S es el centro respecto al cual rotamos/escalamos.
    glm::vec3 worldCenter{0.0f};
    std::vector<ModalShortcutEntry> entries;
};

/// @brief F2H30 Bloque D iter 3: state para detectar double-tap de
///        E / R. Single-tap E -> GizmoMode::Scale; double-tap E ->
///        modal Scale uniforme. Single-tap R -> GizmoMode::Rotate;
///        double-tap R -> modal Rotate libre. Window 0.4s
///        (default Windows double-click time).
struct GizmoKeyTapState {
    int   lastKey = -1;        // -1 = none, 'E', 'R'
    f32   lastPressTime = -1.0f;
};

// Modo del gizmo (Hito 13 Bloque 3-5). Cambia con W/E/R estilo Unity.
enum class GizmoMode : u8 { Translate = 0, Rotate = 1, Scale = 2 };

/// @brief Estado del drag del gizmo. Segun `mode`, `startValue` guarda:
///   Translate: Transform.position inicial (vec3).
///   Scale:     Transform.scale inicial (vec3).
///   Rotate:    Transform.rotationEuler inicial (vec3).
/// `startParam` es el parametro/angulo inicial sobre el eje (o anillo,
/// en Rotate).
struct GizmoDragState {
    bool active = false;
    int axis = -1;
    glm::vec3 startValue{0.0f};
    f32 startParam = 0.0f;
    // Hito 27: Field cual transform se esta editando (Position /
    // Rotation / Scale). Se setea al iniciar el drag y se usa al
    // soltar para construir el EditTransformCommand correcto.
    // 0 = Position, 1 = Rotation, 2 = Scale (matchea
    // EditTransformCommand::Field).
    u8 field = 0;
    // Entidad activa en el drag. Stack para que el undo funcione
    // aunque la seleccion cambie post-drag.
    Entity entity;
    // F2H23 polish iter 5: multi-edit del gizmo. Snapshot de las
    // entidades EXTRA del SelectionSet al iniciar el drag con sus
    // startValues del Field correspondiente. Cada frame del drag
    // aplica el mismo delta del active a todas. Vacio si solo hay
    // una entidad seleccionada (single-edit clasico).
    struct OtherStart {
        Entity entity;
        glm::vec3 startValue;
    };
    std::vector<OtherStart> otherStarts;

};
