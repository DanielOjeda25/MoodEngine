#version 450 core

// F3H31: prefilter del IBL specular usando GGX importance sampling.
//
// Toma un cubemap del cielo (procedural Hosek-Wilkie o HDRI bakeado) y
// produce un cubemap pre-filtrado segun roughness GGX. Los mips del
// output representan distintos roughness:
//   mip 0 (128x128) -> roughness 0.00 (mirror perfecto)
//   mip 1 (64x64)   -> roughness 0.25
//   mip 2 (32x32)   -> roughness 0.50
//   mip 3 (16x16)   -> roughness 0.75
//   mip 4 (8x8)     -> roughness 1.00 (totalmente difuso)
//
// El shader principal samplea este cubemap con `textureLod(prefilter,
// reflectDir, roughness * 4.0)` para obtener la reflexion adecuada.
//
// Algoritmo: GGX importance sampling con 1024 samples por texel.
// Implementacion escrita desde cero a partir del paper de Karis
// "Real Shading in UE4" (concepto publico, no codigo).

in vec3 vDir;

out vec4 outColor;

uniform samplerCube uEnvCubemap;
uniform float uRoughness;       // roughness del mip actual [0, 1]
uniform int uSampleCount;       // typical 1024

const float PI = 3.14159265358979323846;

// Hammersley 2D low-discrepancy sequence (Holger Dammertz, CC BY 3.0).
// Concepto matematico publico — usado por todos los engines AAA.
float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverseVdC(i));
}

// GGX importance sampling — devuelve un half-vector en tangent space.
vec3 importanceSampleGGX(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Spherical → cartesian (tangent space).
    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    // Tangent → world.
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    vec3 N = normalize(vDir);
    vec3 R = N;  // asumimos V = R = N para el prefilter (split-sum approximation)
    vec3 V = R;

    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < uint(uSampleCount); ++i) {
        vec2 xi = hammersley(i, uint(uSampleCount));
        vec3 H = importanceSampleGGX(xi, N, uRoughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += texture(uEnvCubemap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.001);
    outColor = vec4(prefilteredColor, 1.0);
}
