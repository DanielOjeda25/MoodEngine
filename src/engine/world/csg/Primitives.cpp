#include "engine/world/csg/Primitives.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/geometric.hpp>

#include <cmath>

namespace Mood::Csg {

namespace {

/// @brief Transforma un plano local a world. Misma logica que en
///        Brush.cpp::makeBoxBrush — duplicada aqui para no exponer
///        ese helper privado en el header. Si emergen mas
///        primitivas con esta necesidad, factorizar a un util
///        comun en BrushOps o un Plane helper.
Plane transformPlane(const Plane& localPlane,
                     const glm::mat4& worldFromLocal,
                     const glm::mat3& normalMatrix) {
    const glm::vec3 worldNormal =
        glm::normalize(normalMatrix * localPlane.normal);
    const glm::vec3 pointOnLocalPlane =
        -localPlane.distance * localPlane.normal;
    const glm::vec3 pointOnWorldPlane =
        glm::vec3(worldFromLocal * glm::vec4(pointOnLocalPlane, 1.0f));
    return planeFromPointAndNormal(pointOnWorldPlane, worldNormal);
}

/// @brief Genera un brush prismatico con N caras laterales radiales
///        + 2 caps top/bottom. Compartido entre cilindro y prisma.
///        Las caras laterales tienen normales `(cos θ, 0, sin θ)`
///        para θ en [0, 2π) en pasos de 2π/N.
///        El radio se ajusta para que los **vertices** del prisma
///        regular toquen el cilindro circumscripto de radio 0.5.
///        Eso significa que las caras laterales estan en distancia
///        `cos(π/N) * 0.5` del centro (= radio del incirculo).
Brush buildPrismaticBrush(const glm::mat4& worldFromLocal,
                            u32 sides,
                            u32 materialIndex) {
    Brush b;
    if (sides < 3) return b;  // brush degenerado, retorna vacio

    // Radio del incirculo: distancia desde el centro hasta la cara.
    // Para que los VERTICES del prisma regular toquen el cilindro
    // unitario (radio 0.5), el incirculo es 0.5 * cos(pi/N).
    const f32 inradius = 0.5f * std::cos(glm::pi<f32>() / static_cast<f32>(sides));

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(worldFromLocal));
    b.faces.reserve(sides + 2);

    // Caras laterales.
    for (u32 i = 0; i < sides; ++i) {
        const f32 theta = (2.0f * glm::pi<f32>() * static_cast<f32>(i))
                            / static_cast<f32>(sides);
        const glm::vec3 normal(std::cos(theta), 0.0f, std::sin(theta));
        const Plane localPlane{normal, -inradius};
        BrushFace face;
        face.plane = transformPlane(localPlane, worldFromLocal, normalMatrix);
        face.materialIndex = materialIndex;
        defaultTangentBasis(face.plane.normal, face.uAxis, face.vAxis);
        b.faces.push_back(face);
    }

    // Caps top/bottom.
    const Plane topLocal   {{0.0f,  1.0f, 0.0f}, -0.5f};
    const Plane bottomLocal{{0.0f, -1.0f, 0.0f}, -0.5f};
    BrushFace topFace, bottomFace;
    topFace.plane = transformPlane(topLocal, worldFromLocal, normalMatrix);
    topFace.materialIndex = materialIndex;
    defaultTangentBasis(topFace.plane.normal, topFace.uAxis, topFace.vAxis);
    bottomFace.plane = transformPlane(bottomLocal, worldFromLocal, normalMatrix);
    bottomFace.materialIndex = materialIndex;
    defaultTangentBasis(bottomFace.plane.normal, bottomFace.uAxis, bottomFace.vAxis);
    b.faces.push_back(topFace);
    b.faces.push_back(bottomFace);

    b.localAabb = computeBrushAabb(b);
    return b;
}

} // namespace

Brush makeCylinderBrush(const glm::mat4& worldFromLocal,
                         u32 segments,
                         u32 materialIndex) {
    return buildPrismaticBrush(worldFromLocal, segments, materialIndex);
}

Brush makePrismBrush(const glm::mat4& worldFromLocal,
                      u32 sides,
                      u32 materialIndex) {
    return buildPrismaticBrush(worldFromLocal, sides, materialIndex);
}

Brush makeSphereBrush(const glm::mat4& worldFromLocal,
                       u32 materialIndex) {
    // Dodecaedro regular inscripto en una esfera de radio 0.5.
    // 12 caras pentagonales. Las normales son los 12 ejes que
    // pasan por los centros de las caras del dodecaedro.
    //
    // Para construir el dodecaedro: usamos la simetria del icosaedro
    // dual. Los 12 vertices del icosaedro inscripto en esfera unidad
    // son las direcciones a las caras del dodecaedro. Coordenadas
    // canonicas del icosaedro:
    //   (0, ±1, ±φ), (±1, ±φ, 0), (±φ, 0, ±1)   con  φ = (1+√5)/2.
    // Normalizadas (longitud √(1+φ²)).
    constexpr f32 phi = 1.6180339887498949f;
    const f32 norm = std::sqrt(1.0f + phi * phi);
    const f32 c1 = 1.0f / norm;          // coord "1/norm"
    const f32 c2 = phi / norm;            // coord "phi/norm"

    const glm::vec3 directions[12] = {
        { 0.0f,  c1,  c2}, { 0.0f,  c1, -c2},
        { 0.0f, -c1,  c2}, { 0.0f, -c1, -c2},
        { c1,  c2,  0.0f}, { c1, -c2,  0.0f},
        {-c1,  c2,  0.0f}, {-c1, -c2,  0.0f},
        { c2,  0.0f,  c1}, { c2,  0.0f, -c1},
        {-c2,  0.0f,  c1}, {-c2,  0.0f, -c1},
    };

    // Para que el dodecaedro este inscripto en una esfera de radio
    // 0.5 (matcheando la unit cell [-0.5, +0.5]), el incirculo de
    // cada cara pentagonal esta a distancia 0.5 / cos(angulo).
    // Aproximacion suficiente para CSG: distance = 0.5 (las normales
    // ya son unitarias y apuntan a una esfera de radio 1; escalamos
    // a 0.5 con la convencion del local space).
    const f32 faceDistance = 0.5f;

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(worldFromLocal));
    Brush b;
    b.faces.reserve(12);
    for (const auto& n : directions) {
        const Plane localPlane{n, -faceDistance};
        BrushFace face;
        face.plane = transformPlane(localPlane, worldFromLocal, normalMatrix);
        face.materialIndex = materialIndex;
        defaultTangentBasis(face.plane.normal, face.uAxis, face.vAxis);
        b.faces.push_back(face);
    }

    b.localAabb = computeBrushAabb(b);
    return b;
}

Brush makePyramidBrush(const glm::mat4& worldFromLocal,
                        u32 materialIndex) {
    // Piramide cuadrada: cima en (0, +0.5, 0), base cuadrada en
    // y=-0.5 con vertices (±0.5, -0.5, ±0.5).
    //
    // Las 4 caras laterales tienen normales que apuntan hacia
    // afuera-arriba. Para la cara que da al lado +X (entre los
    // vertices (+0.5, -0.5, ±0.5) y la cima), la normal es:
    //   - dirige al exterior +X.
    //   - tiene componente +Y (arriba).
    // Calculo: el plano contiene (0, 0.5, 0), (0.5, -0.5, 0.5),
    // (0.5, -0.5, -0.5). Vectores en el plano: v1 = (0.5, -1, 0.5),
    // v2 = (0.5, -1, -0.5). normal = cross(v1, v2) = (1, 0.5, 0).
    // Normalizada: (1, 0.5, 0) / |...|.
    const f32 lateralLen = std::sqrt(1.0f + 0.25f);  // sqrt(1.25)
    const f32 lx = 1.0f / lateralLen;
    const f32 ly = 0.5f / lateralLen;

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(worldFromLocal));
    Brush b;
    b.faces.reserve(5);

    // Las 4 caras laterales.
    // Cara +X: normal (lx, ly, 0), distance tal que la cima
    // (0, 0.5, 0) cumple dot(n, p) + d = 0:
    //   dot((lx, ly, 0), (0, 0.5, 0)) + d = ly*0.5 + d = 0
    //   => d = -ly*0.5.
    const f32 lateralDist = -ly * 0.5f;
    const glm::vec3 lateralNormals[4] = {
        { lx,  ly,  0.0f},
        {-lx,  ly,  0.0f},
        { 0.0f, ly,  lx},
        { 0.0f, ly, -lx},
    };
    for (const auto& n : lateralNormals) {
        const Plane localPlane{n, lateralDist};
        BrushFace face;
        face.plane = transformPlane(localPlane, worldFromLocal, normalMatrix);
        face.materialIndex = materialIndex;
        defaultTangentBasis(face.plane.normal, face.uAxis, face.vAxis);
        b.faces.push_back(face);
    }

    // Base en y=-0.5, normal (0,-1,0).
    const Plane baseLocal{{0.0f, -1.0f, 0.0f}, -0.5f};
    BrushFace baseFace;
    baseFace.plane = transformPlane(baseLocal, worldFromLocal, normalMatrix);
    baseFace.materialIndex = materialIndex;
    defaultTangentBasis(baseFace.plane.normal, baseFace.uAxis, baseFace.vAxis);
    b.faces.push_back(baseFace);

    b.localAabb = computeBrushAabb(b);
    return b;
}

Brush makeWedgeBrush(const glm::mat4& worldFromLocal,
                      u32 materialIndex) {
    // Wedge / rampa con base cuadrada [-0.5, +0.5]^2 en X-Z.
    // Altura maxima en z=-0.5 (atras): la rampa llega a y=+0.5.
    // Altura minima en z=+0.5 (adelante): la rampa cae a y=-0.5
    // (al ras de la base).
    //
    // 5 planos:
    //  1. Lateral +X: normal (+1, 0, 0), distance -0.5.
    //  2. Lateral -X: normal (-1, 0, 0), distance -0.5.
    //  3. Base (-Y): normal (0, -1, 0), distance -0.5.
    //  4. Atras (-Z): normal (0, 0, -1), distance -0.5.
    //  5. Plano inclinado: contiene la arista (y=+0.5, z=-0.5) y la
    //     arista (y=-0.5, z=+0.5). El vector entre estas aristas
    //     en el plano YZ es dy=-1, dz=+1; la normal hacia
    //     arriba-adelante es (0, +1, +1) normalizado, y la
    //     distance se ajusta para que el plano contenga las
    //     aristas. Para el punto (0, 0.5, -0.5):
    //       dot((0, 1/sqrt(2), 1/sqrt(2)), (0, 0.5, -0.5))
    //       = 0.5/sqrt(2) - 0.5/sqrt(2) = 0
    //     El plano pasa por origen en YZ. distance = 0.

    const f32 invSqrt2 = 1.0f / std::sqrt(2.0f);
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(worldFromLocal));
    Brush b;
    b.faces.reserve(5);

    const Plane planes[5] = {
        {{ 1.0f,  0.0f,  0.0f}, -0.5f},        // +X
        {{-1.0f,  0.0f,  0.0f}, -0.5f},        // -X
        {{ 0.0f, -1.0f,  0.0f}, -0.5f},        // base
        {{ 0.0f,  0.0f, -1.0f}, -0.5f},        // atras
        {{ 0.0f, invSqrt2, invSqrt2}, 0.0f},   // inclinado
    };

    for (const auto& p : planes) {
        BrushFace face;
        face.plane = transformPlane(p, worldFromLocal, normalMatrix);
        face.materialIndex = materialIndex;
        b.faces.push_back(face);
    }

    b.localAabb = computeBrushAabb(b);
    return b;
}

} // namespace Mood::Csg
