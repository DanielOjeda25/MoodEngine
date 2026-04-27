#version 450 core

// Vertex shader skinneable (Hito 19). Igual estructura que `pbr.vert` pero
// aplica Linear Blend Skinning (LBS) con hasta 4 huesos por vertice antes
// de transformar al world space. La normal se transforma con el mismo
// promedio ponderado de matrices (aproximacion barata, valida mientras
// los huesos no apliquen scales no-uniformes — caso comun en personajes
// humanoides).
//
// El frag shader es el MISMO `pbr.frag` (recibe `vWorldPos`/`vWorldNormal`
// ya skinneados, no le importa de donde vinieron). Eso evita duplicar la
// logica PBR/IBL/shadows entre dos `.frag`.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;
layout(location = 4) in vec4 aBoneIds;     // floats; los casteamos a int
layout(location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpace;

// MAX_BONES debe matchear `Mood::k_maxBonesPerSkeleton` en C++. Subirlo
// requiere bumpear ambos.
const int MAX_BONES = 128;
uniform mat4 uBoneMatrices[MAX_BONES];

out vec3 vColor;
out vec2 vUv;
out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec4 vLightSpacePos;

void main() {
    vColor = aColor;
    vUv = aUv;

    // Suma de pesos. Si es ~0 (mesh sin esqueleto compartiendo el shader,
    // caso defensivo) usamos pos/normal originales para que el resultado
    // sea identico al `pbr.vert` no-skinneado. La rama no se toma para
    // personajes reales (suma=1 garantizado por el MeshLoader).
    float wSum = aBoneWeights.x + aBoneWeights.y + aBoneWeights.z + aBoneWeights.w;

    vec4 skinnedPos;
    vec3 skinnedNormal;

    if (wSum < 1e-4) {
        skinnedPos    = vec4(aPos, 1.0);
        skinnedNormal = aNormal;
    } else {
        ivec4 ids = ivec4(aBoneIds + 0.5);
        // Clampeo defensivo: id fuera de rango se cae a la matriz 0 (no
        // crashea). El MeshLoader ya garantiza ids validos pero por las
        // dudas si carga un .glb raro.
        ids = clamp(ids, ivec4(0), ivec4(MAX_BONES - 1));

        mat4 boneMat = uBoneMatrices[ids.x] * aBoneWeights.x
                     + uBoneMatrices[ids.y] * aBoneWeights.y
                     + uBoneMatrices[ids.z] * aBoneWeights.z
                     + uBoneMatrices[ids.w] * aBoneWeights.w;

        skinnedPos    = boneMat * vec4(aPos, 1.0);
        // Normal: mat3 del skinning matrix. Si los huesos tienen scales
        // uniformes (humanoides tipicos) basta. Para scales no-uniformes
        // habria que usar inverse-transpose por hueso — overkill aqui.
        skinnedNormal = mat3(boneMat) * aNormal;
    }

    vec4 worldPos4 = uModel * skinnedPos;
    vWorldPos = worldPos4.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vWorldNormal = normalize(normalMatrix * skinnedNormal);

    vLightSpacePos = uLightSpace * worldPos4;
    gl_Position = uProjection * uView * worldPos4;
}
