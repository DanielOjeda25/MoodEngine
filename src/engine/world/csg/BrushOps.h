#pragma once

// Operaciones booleanas CSG entre brushes (F2H12). Approach destructivo
// estilo Hammer/TrenchBroom: las ops devuelven los brushes resultantes
// y el llamador (editor) reemplaza los inputs con el resultado.
//
// Algoritmo: plane clipping a mano (~300 LOC). Sin libs externas;
// reusa los tipos puros de F2H11 (Plane, Brush, BrushFace) y la
// tolerancia kPlaneEpsilon (1e-4f).
//
// Cardinalidad del resultado:
//   - 0 brushes:    el output es vacio (ej: A subtract B con B
//                   conteniendo a A; o A intersect B disjuntos).
//   - 1 brush:      caso simple (subtract donde B esta fuera de A;
//                   intersect convexo; union con uno conteniendo
//                   al otro).
//   - N > 1:        descomposicion convexa (A subtract B con B
//                   metido parcialmente; A union B con overlap).

#include "engine/world/csg/Brush.h"

#include <optional>
#include <vector>

namespace Mood::Csg {

/// @brief True si el brush tiene volumen real (>= 4 vertices unicos
///        producidos por intersect-de-tripletes filtrando puntos
///        fuera del brush). Usado para descartar brushes
///        degenerados que las ops booleanas pueden producir
///        (ej: A subtract B donde B contiene a A devuelve [] sin
///        crashear).
bool isBrushValid(const Brush& b);

/// @brief A subtract B: pedazos de A que estan fuera de B.
///        Algoritmo: para cada plano P_i de B, generar el brush
///        A_i = (A recortado por planos previos) ∪ {plano flipped
///        de P_i}, y conservar si tiene volumen. Reemplazar A con
///        (A recortado por P_i) en la siguiente iteracion.
///
///        Cardinalidad:
///        - 0 brushes:  B contiene completamente a A (o B == A).
///        - 1 brush:    A y B son disjuntos -> copia de A.
///        - N > 1:      B se mete parcialmente en A (descomposicion
///                      convexa de A \ B).
std::vector<Brush> subtract(const Brush& A, const Brush& B);

/// @brief A intersect B: el brush convexo formado por la
///        interseccion de los half-spaces de ambos. Combina las
///        caras de A y B y verifica via isBrushValid que tiene
///        volumen.
///
///        Devuelve nullopt si la interseccion es vacia (no se
///        tocan, o se tocan solo en una cara/arista/vertice
///        degenerado sin volumen).
std::optional<Brush> intersectOp(const Brush& A, const Brush& B);

/// @brief A union B: descomposicion convexa de A ∪ B.
///        Cardinalidad:
///        - 1 brush:    uno contiene al otro -> el grande.
///        - 2 brushes:  son disjuntos -> [A, B].
///        - N > 2:      overlap parcial -> (A subtract B) ∪ {B}.
std::vector<Brush> unionOp(const Brush& A, const Brush& B);

/// @brief F2H32 Bloque B: clip tool. Splittea el brush A por el plano
///        `clipPlane` (que se asume con normal y distance ya correctos
///        para el lado "front"; el "back" se obtiene negando el plano).
///
///        Algoritmo: copia el brush, agrega un BrushFace con el plano
///        correspondiente (intersect con el half-space). Validar con
///        isBrushValid; si degenera (plano no toca al brush), no
///        emitir esa parte.
///
///        `keepFront` true -> emite el lado positivo del plano.
///        `keepBack`  true -> emite el lado negativo.
///        Ambos true -> spawnea ambos como brushes separados.
///        Devuelve hasta 2 brushes en orden: [front, back] (ausentes si
///        no se piden o degeneran). Cardinalidad:
///        - 0: el plano no intersecta al brush en ninguna direccion.
///        - 1: solo un lado es valido (el plano tangencia o el brush
///             vive todo de un lado).
///        - 2: ambos lados validos.
std::vector<Brush> clipBrushByPlane(const Brush& A,
                                     const Plane& clipPlane,
                                     bool keepFront,
                                     bool keepBack);

} // namespace Mood::Csg
