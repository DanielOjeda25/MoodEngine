// F2H82 Bloque A: implementación del analyzer. Ver VehicleMeshAnalyzer.h +
// docs/conventions/vehiculos.md.

#include "engine/physics/vehicle/VehicleMeshAnalyzer.h"

#include "core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/common.hpp>  // glm::min/max para vec3

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <limits>
#include <vector>

namespace Mood::vehicle {

namespace {

std::string normalizeName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lc == ' ' || lc == '-' || lc == '.' || lc == '/' || lc == ':') lc = '_';
        out += lc;
    }
    return out;
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

std::vector<std::string> splitTokens(const std::string& norm) {
    std::vector<std::string> toks;
    std::string cur;
    for (char c : norm) {
        if (c == '_') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) toks.push_back(cur);
    return toks;
}

bool hasToken(const std::vector<std::string>& toks, const char* t) {
    return std::find(toks.begin(), toks.end(), std::string(t)) != toks.end();
}

WheelRole roleFromLonLat(int lon, int lat) {
    // lon: -1 front, +1 rear. lat: -1 left, +1 right.
    if (lon == 0 || lat == 0) return WheelRole::Unknown;
    if (lon < 0) return (lat < 0) ? WheelRole::FL : WheelRole::FR;
    return (lat < 0) ? WheelRole::RL : WheelRole::RR;
}

} // namespace

WheelRole wheelRoleFromName(const std::string& nodeName) {
    const std::string n = normalizeName(nodeName);
    const std::vector<std::string> toks = splitTokens(n);

    // ¿Candidato a rueda? keyword o patrón de neumático abreviado [fb]_t_[lr].
    const bool wheelKw = contains(n, "wheel") || contains(n, "rueda") ||
                         contains(n, "ruedra") || contains(n, "tire") ||
                         contains(n, "tyre") || contains(n, "rim") ||
                         contains(n, "llanta");
    const bool tireAbbrev = hasToken(toks, "t") &&
                            (hasToken(toks, "f") || hasToken(toks, "b")) &&
                            (hasToken(toks, "l") || hasToken(toks, "r"));
    if (!wheelKw && !tireAbbrev) return WheelRole::Unknown;

    // 1) Códigos de 2 letras como tokens (canónico FL/FR/RL/RR, Rockstar
    //    LF/RF/LR/RR, Unreal BL/BR). Alta confianza.
    struct Code { const char* tok; WheelRole role; };
    static const std::array<Code, 9> kCodes{{
        {"fl", WheelRole::FL}, {"lf", WheelRole::FL},
        {"fr", WheelRole::FR}, {"rf", WheelRole::FR},
        {"rl", WheelRole::RL}, {"lr", WheelRole::RL}, {"bl", WheelRole::RL},
        {"rr", WheelRole::RR}, {"br", WheelRole::RR},
    }};
    for (const auto& c : kCodes) {
        if (hasToken(toks, c.tok)) return c.role;
    }

    // 2) Palabras completas (multi-idioma).
    int lon = 0;  // -1 front, +1 rear
    int lat = 0;  // -1 left, +1 right
    if (contains(n, "front") || contains(n, "delant")) lon = -1;
    else if (contains(n, "rear") || contains(n, "back") || contains(n, "tras")) lon = +1;
    if (contains(n, "left") || contains(n, "izq")) lat = -1;
    else if (contains(n, "right") || contains(n, "der")) lat = +1;

    // 3) Tokens de una letra (caso `f_t_l`): lon de f/b, lat de l/r.
    if (lon == 0) {
        if (hasToken(toks, "f")) lon = -1;
        else if (hasToken(toks, "b")) lon = +1;
    }
    if (lat == 0) {
        if (hasToken(toks, "l")) lat = -1;
        else if (hasToken(toks, "r")) lat = +1;
    }

    return roleFromLonLat(lon, lat);
}

WheelRole wheelRoleFromPosition(const glm::vec3& wheelCenter,
                                const glm::vec3& modelCenter) {
    const float dx = wheelCenter.x - modelCenter.x;  // <0 izquierda
    const float dz = wheelCenter.z - modelCenter.z;  // >0 frente (+Z forward)
    const int lat = (dx < 0.0f) ? -1 : +1;
    const int lon = (dz > 0.0f) ? -1 : +1;  // frente = lon -1
    return roleFromLonLat(lon, lat);
}

namespace {

// Recorre los nodos acumulando la transform global y junta una MeshPart por
// nodo con geometría. `applyXform` solo para glTF/GLB (FBX trae vertices baked).
void collectParts(const aiScene* scene, const aiNode* node,
                  const aiMatrix4x4& parentXform, bool applyXform,
                  std::vector<MeshPart>& out) {
    const aiMatrix4x4 global = parentXform * node->mTransformation;
    if (node->mNumMeshes > 0) {
        glm::vec3 mn(std::numeric_limits<float>::max());
        glm::vec3 mx(-std::numeric_limits<float>::max());
        u32 vc = 0;
        for (u32 i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh* m = scene->mMeshes[node->mMeshes[i]];
            if (m == nullptr) continue;
            for (u32 v = 0; v < m->mNumVertices; ++v) {
                aiVector3D p = m->mVertices[v];
                if (applyXform) p = global * p;
                mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
                mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
                ++vc;
            }
        }
        if (vc > 0 && mn.x <= mx.x) {
            MeshPart part;
            part.nodeName = std::string(node->mName.C_Str(), node->mName.length);
            part.aabbMin = mn;
            part.aabbMax = mx;
            part.center = (mn + mx) * 0.5f;
            part.vertexCount = vc;
            out.push_back(std::move(part));
        }
    }
    for (u32 c = 0; c < node->mNumChildren; ++c) {
        collectParts(scene, node->mChildren[c], global, applyXform, out);
    }
}

f32 estimateWheelRadius(const MeshPart& p) {
    // La rueda rueda en el plano Y-Z: el radio ≈ mitad del mayor entre la
    // altura (Y) y el largo (Z). El ancho (X) es el grosor.
    const glm::vec3 s = p.size();
    return 0.5f * std::max(s.y, s.z);
}

} // namespace

VehicleAnalysis analyzeVehicleMesh(const std::string& filesystemPath) {
    VehicleAnalysis a;

    Assimp::Importer importer;
    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
    const u32 flags = aiProcess_Triangulate | aiProcess_GlobalScale;
    const aiScene* scene = importer.ReadFile(filesystemPath, flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
        a.error = importer.GetErrorString();
        if (a.error.empty()) a.error = "modelo sin geometría";
        Log::assets()->warn("VehicleMeshAnalyzer: fallo '{}' ({})",
                             filesystemPath, a.error);
        return a;
    }

    const auto toLower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string ext = toLower(std::filesystem::path(filesystemPath).extension().string());
    const bool applyXform = (ext == ".gltf" || ext == ".glb");

    std::vector<MeshPart> parts;
    collectParts(scene, scene->mRootNode, aiMatrix4x4{}, applyXform, parts);
    if (parts.empty()) {
        a.error = "sin partes con geometría";
        return a;
    }

    // AABB total.
    glm::vec3 omn(std::numeric_limits<float>::max());
    glm::vec3 omx(-std::numeric_limits<float>::max());
    for (const auto& p : parts) { omn = glm::min(omn, p.aabbMin); omx = glm::max(omx, p.aabbMax); }
    a.overallAabbMin = omn;
    a.overallAabbMax = omx;
    const glm::vec3 modelCenter = (omn + omx) * 0.5f;

    std::array<int, 4> wheelPartIdx{-1, -1, -1, -1};  // índice en `parts` por rol

    // --- 1) Name-first ---
    for (size_t i = 0; i < parts.size(); ++i) {
        const WheelRole role = wheelRoleFromName(parts[i].nodeName);
        if (role == WheelRole::Unknown) continue;
        const int ri = static_cast<int>(role);
        if (wheelPartIdx[ri] < 0) {
            wheelPartIdx[ri] = static_cast<int>(i);
        } else if (parts[i].vertexCount > parts[wheelPartIdx[ri]].vertexCount) {
            wheelPartIdx[ri] = static_cast<int>(i);  // ante conflicto, la más densa
        }
    }
    int byName = 0;
    for (int idx : wheelPartIdx) if (idx >= 0) ++byName;
    a.wheelsByName = (byName == 4);

    // --- 2) Geometry-fallback (si no salieron las 4 por nombre) ---
    if (byName < 4) {
        // Candidatos: partes "chicas" (volumen mucho menor que el modelo) en
        // la mitad inferior, no ya elegidas por nombre.
        const glm::vec3 modelSize = omx - omn;
        const float yMid = modelCenter.y;
        std::vector<int> used(parts.size(), 0);
        for (int idx : wheelPartIdx) if (idx >= 0) used[idx] = 1;
        std::vector<int> cands;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (used[i]) continue;
            const glm::vec3 s = parts[i].size();
            const bool small = s.x < modelSize.x * 0.5f && s.y < modelSize.y * 0.6f &&
                               s.z < modelSize.z * 0.5f;
            const bool lower = parts[i].center.y < yMid;
            if (small && lower) cands.push_back(static_cast<int>(i));
        }
        for (int ci : cands) {
            const WheelRole role = wheelRoleFromPosition(parts[ci].center, modelCenter);
            if (role == WheelRole::Unknown) continue;
            const int ri = static_cast<int>(role);
            if (wheelPartIdx[ri] < 0) wheelPartIdx[ri] = ci;
        }
    }

    // --- Poblar ruedas detectadas ---
    for (int ri = 0; ri < 4; ++ri) {
        const int idx = wheelPartIdx[ri];
        if (idx < 0) continue;
        DetectedWheel w;
        w.part = parts[idx];
        w.role = static_cast<WheelRole>(ri);
        w.radius = estimateWheelRadius(parts[idx]);
        w.width = parts[idx].size().x;
        w.byName = (wheelRoleFromName(parts[idx].nodeName) != WheelRole::Unknown);
        a.wheels[ri] = w;
        ++a.wheelsFound;
    }

    // --- Chasis = unión de partes NO elegidas como rueda ---
    {
        std::array<int, 4> chosen = wheelPartIdx;
        glm::vec3 cmn(std::numeric_limits<float>::max());
        glm::vec3 cmx(-std::numeric_limits<float>::max());
        bool any = false;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (std::find(chosen.begin(), chosen.end(), static_cast<int>(i)) != chosen.end())
                continue;
            cmn = glm::min(cmn, parts[i].aabbMin);
            cmx = glm::max(cmx, parts[i].aabbMax);
            any = true;
        }
        if (!any) { cmn = omn; cmx = omx; }  // todo eran ruedas (raro)
        a.chassisAabbMin = cmn;
        a.chassisAabbMax = cmx;
        a.chassisCenter = (cmn + cmx) * 0.5f;
        a.chassisHalfExtents = (cmx - cmn) * 0.5f;
        // CoM a la ALTURA promedio de los cubos de las ruedas — ahi
        // tipicamente queda la mayor masa del vehiculo (motor, transmision,
        // baterias) y un CoM bajo evita que vuelque en las curvas. Sin esto
        // el CoM queda a 0.9-1.0 m del piso en autos altos (Tesla Cybertruck,
        // camioneta) y el auto flipa con cualquier giro fuerte. Fallback:
        // si no hay ruedas detectadas, -0.7*halfY desde el centro del cuerpo
        // (≈ piso del chasis), tambien estable.
        f32 wheelSumY = 0.0f; int wheelN = 0;
        for (const auto& wh : a.wheels) {
            if (wh.role != WheelRole::Unknown) {
                wheelSumY += wh.part.center.y;
                ++wheelN;
            }
        }
        const f32 comY = (wheelN > 0)
            ? (wheelSumY / static_cast<f32>(wheelN))
            : (a.chassisCenter.y - 0.7f * a.chassisHalfExtents.y);
        a.centerOfMassLocal = glm::vec3(0.0f, comY - a.chassisCenter.y, 0.0f);
    }

    // --- Derivados de ruedas ---
    auto haveW = [&](WheelRole r) { return a.wheels[static_cast<int>(r)].role != WheelRole::Unknown; };
    if (haveW(WheelRole::FL) && haveW(WheelRole::FR)) {
        a.trackFront = std::abs(a.wheels[0].part.center.x - a.wheels[1].part.center.x);
    }
    if (haveW(WheelRole::RL) && haveW(WheelRole::RR)) {
        a.trackRear = std::abs(a.wheels[2].part.center.x - a.wheels[3].part.center.x);
    }
    {
        // Wheelbase: |Z promedio delantero - Z promedio trasero|.
        float frontZ = 0.0f; int fn = 0;
        float rearZ = 0.0f;  int rn = 0;
        if (haveW(WheelRole::FL)) { frontZ += a.wheels[0].part.center.z; ++fn; }
        if (haveW(WheelRole::FR)) { frontZ += a.wheels[1].part.center.z; ++fn; }
        if (haveW(WheelRole::RL)) { rearZ += a.wheels[2].part.center.z; ++rn; }
        if (haveW(WheelRole::RR)) { rearZ += a.wheels[3].part.center.z; ++rn; }
        if (fn > 0 && rn > 0) {
            a.wheelbase = std::abs(frontZ / fn - rearZ / rn);
            // Yaw sugerido: si los que el NOMBRE dice "front" están en -Z
            // respecto a los "rear", el modelo mira -Z → 180°.
            if (a.wheelsByName && (frontZ / fn) < (rearZ / rn)) {
                a.suggestedYawOffsetDeg = 180.0f;
            }
        }
        f32 radSum = 0.0f, widSum = 0.0f; int wn = 0;
        for (const auto& w : a.wheels) {
            if (w.role == WheelRole::Unknown) continue;
            radSum += w.radius; widSum += w.width; ++wn;
        }
        if (wn > 0) { a.wheelRadius = radSum / wn; a.wheelWidth = widSum / wn; }
    }

    a.ok = true;
    Log::assets()->info(
        "VehicleMeshAnalyzer: '{}' → {} partes, {} ruedas ({}), "
        "chasis {:.2f}x{:.2f}x{:.2f}m, track F/R {:.2f}/{:.2f}, wb {:.2f}, "
        "rueda r={:.2f}, yaw sug {:.0f}°",
        filesystemPath, parts.size(), a.wheelsFound,
        a.wheelsByName ? "por nombre" : "mixto/geometría",
        a.chassisHalfExtents.x * 2.0f, a.chassisHalfExtents.y * 2.0f,
        a.chassisHalfExtents.z * 2.0f, a.trackFront, a.trackRear, a.wheelbase,
        a.wheelRadius, a.suggestedYawOffsetDeg);
    return a;
}

} // namespace Mood::vehicle
