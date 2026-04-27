#version 450 core

// Hito 17: PBR metallic-roughness con Cook-Torrance.
//
// BRDF: D * G * F / (4 * NdotV * NdotL)
//   D = distribucion de microfacetas GGX (Trowbridge-Reitz)
//   G = geometria Smith con Schlick-GGX (k para direct lighting)
//   F = Fresnel-Schlick (aproxima el espectro completo con 5to grado)
//
// Energy conservation:
//   kS = F
//   kD = (1 - kS) * (1 - metallic)   // metallics no tienen diffuse
//   F0 = mix(0.04, albedo, metallic) // dieléctrico estándar 4%
//
// Luces directas: directional (con shadow factor del Hito 16) + N point
// lights (sin sombras todavia). Sin IBL: el ambient es un escalar global
// — Bloque 3 lo reemplaza por irradiance/prefilter cubemaps.

#define MAX_POINT_LIGHTS 8
#define PI 3.14159265359

in vec3 vColor;
in vec2 vUv;
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec4 vLightSpacePos;

uniform vec3 uCameraPos;
uniform vec3 uAmbient;            // ambient placeholder hasta IBL (Bloque 3)

// --- Material (Hito 17) ---
// Texturas: el shader sample SOLO si el flag `uHas*` esta en 1; sino
// usa los multiplicadores escalares. Esto evita bindear texturas dummy
// para materiales sin map asignado.
uniform sampler2D uAlbedoMap;          // unit 0
uniform sampler2D uMetallicRoughness;  // unit 2 (G=rough, B=metal, glTF)
uniform sampler2D uAoMap;              // unit 3
uniform int   uHasAlbedoMap;
uniform int   uHasMetallicRoughness;
uniform int   uHasAoMap;
uniform vec3  uAlbedoTint;
uniform float uMetallicMult;
uniform float uRoughnessMult;
uniform float uAoMult;

// --- Lights (mismo layout que lit.frag para reusar LightSystem) ---
struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
    int  enabled;
};
uniform DirLight uDirectional;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};
uniform PointLight uPointLights[MAX_POINT_LIGHTS];
uniform int uActivePointLights;

// uSpecularStrength / uShininess del lit shader ya no se usan en PBR.

// --- IBL (Hito 17 Bloque 3) ---
// Tres mapas pre-bakeados ofreciendo el termino indirecto:
//   - irradiance: integral cosine-weighted (diffuse).
//   - prefilter: mip chain GGX-importance-sampled (specular).
//   - BRDF LUT: split-sum de Epic (scale, bias) en RG.
// Cuando uIblEnabled = 0, el shader usa `uAmbient` escalar como fallback.
uniform int          uIblEnabled;
uniform samplerCube  uIrradianceMap;     // unit 4
uniform samplerCube  uPrefilterMap;      // unit 5
uniform sampler2D    uBrdfLut;           // unit 6
uniform float        uPrefilterMaxLod;   // mipLevels - 1 del prefilter

// --- Shadow mapping (Hito 16) ---
uniform int             uShadowEnabled;
uniform sampler2DShadow uShadowMap;     // unit 1
uniform float           uShadowBias;

// --- Fog (Hito 15) ---
uniform int   uFogMode;
uniform vec3  uFogColor;
uniform float uFogDensity;
uniform float uFogStart;
uniform float uFogEnd;

// --------------------------------------------------------------------------
// Cook-Torrance helpers
// --------------------------------------------------------------------------

// GGX Trowbridge-Reitz: distribucion de microfacetas. `roughness` se eleva
// al cuadrado (alpha = r^2) — convencion Disney/glTF: rough=0 mirror,
// rough=1 lambertian.
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith con Schlick-GGX. `k` para direct lighting es (r+1)^2 / 8.
float geometrySchlickGGX(float NdotX, float k) {
    return NdotX / (NdotX * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

// Fresnel-Schlick: aproxima el espectro reflectivo con un polinomio de
// grado 5 sobre cos(theta_h).
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --------------------------------------------------------------------------
// Shadow + fog (mismas formulas que lit.frag — para no divergir).
// --------------------------------------------------------------------------

float sampleShadow(vec4 lightSpacePos) {
    if (uShadowEnabled == 0) return 1.0;
    vec3 proj = lightSpacePos.xyz / max(lightSpacePos.w, 1e-5);
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    float refDepth = proj.z - uShadowBias;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            sum += texture(uShadowMap, vec3(proj.xy + offset, refDepth));
        }
    }
    return sum / 9.0;
}

float computeFogFactor(float distance) {
    if (uFogMode == 0) return 0.0;
    else if (uFogMode == 1) {
        float denom = uFogEnd - uFogStart;
        if (denom <= 0.0) return 1.0;
        return clamp((distance - uFogStart) / denom, 0.0, 1.0);
    } else if (uFogMode == 2) {
        return 1.0 - exp(-uFogDensity * distance);
    } else {
        float dd = uFogDensity * distance;
        return 1.0 - exp(-(dd * dd));
    }
}

// --------------------------------------------------------------------------
// BRDF eval para una luz dada
// --------------------------------------------------------------------------

vec3 evalBrdf(vec3 N, vec3 V, vec3 L, vec3 lightRadiance,
              vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 1e-4;
    vec3  specular    = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * lightRadiance * NdotL;
}

// --------------------------------------------------------------------------

out vec4 FragColor;

void main() {
    // --- Sample del material ---
    vec3 albedo = uAlbedoTint;
    if (uHasAlbedoMap == 1) {
        albedo *= texture(uAlbedoMap, vUv).rgb;
    }
    albedo *= vColor;

    float metallic  = uMetallicMult;
    float roughness = uRoughnessMult;
    if (uHasMetallicRoughness == 1) {
        // glTF MR packed: G=rough, B=metal. R puede traer AO embebido.
        vec3 mr = texture(uMetallicRoughness, vUv).rgb;
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    // Roughness bajo (~0) genera highlights muy duros y aliasing en el
    // shader directo; clamping mínimo evita NaN al dividir por (denom)^2.
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic,  0.0,  1.0);

    float ao = uAoMult;
    if (uHasAoMap == 1) {
        ao *= texture(uAoMap, vUv).r;
    }

    // F0 dieléctrico estándar (4%). Para metales, se reemplaza por el
    // albedo (los metales toman su color del especular).
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    float NdotV = max(dot(N, V), 0.0);

    // Indirecta (Hito 17 Bloque 3): split-sum IBL.
    //   diffuseIBL  = irradiance(N) * (kD * albedo)
    //   specularIBL = prefilter(R, roughness) * (F0 * scale + bias)
    // Si el IBL no esta cargado (uIblEnabled=0), fallback al ambient escalar.
    vec3 ambient;
    if (uIblEnabled == 1) {
        // Fresnel con roughness: Lazarov 2013 - aproximacion para suavizar
        // el grazing en superficies rugosas. Sin esto los metales rugosos
        // se ven con bordes muy brillantes contra el cielo.
        vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0)
                     * pow(1.0 - NdotV, 5.0);
        vec3 kS_ibl = F;
        vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuseIBL = irradiance * albedo;

        vec3 R = reflect(-V, N);
        // textureLod sample del prefilter mip chain. roughness 0 -> mip 0
        // (mirror), roughness 1 -> mip max (totalmente difuso).
        vec3 prefiltered = textureLod(uPrefilterMap, R,
                                       roughness * uPrefilterMaxLod).rgb;
        vec2 envBrdf = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
        vec3 specularIBL = prefiltered * (F * envBrdf.x + envBrdf.y);

        ambient = (kD_ibl * diffuseIBL + specularIBL) * ao;
    } else {
        ambient = uAmbient * albedo * ao;
    }

    // Directional + shadow.
    vec3 lo = vec3(0.0);
    if (uDirectional.enabled == 1) {
        vec3 L = normalize(-uDirectional.direction);
        vec3 radiance = uDirectional.color * uDirectional.intensity;
        float shadowF = sampleShadow(vLightSpacePos);
        lo += evalBrdf(N, V, L, radiance, albedo, metallic, roughness, F0)
              * shadowF;
    }

    // Point lights con atenuacion smooth basada en radius (consistente
    // con LightSystem).
    int n = min(uActivePointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < n; ++i) {
        vec3 toL = uPointLights[i].position - vWorldPos;
        float dist = length(toL);
        if (dist > uPointLights[i].radius) continue;
        vec3 L = toL / max(dist, 1e-4);
        float t = 1.0 - clamp(dist / uPointLights[i].radius, 0.0, 1.0);
        float attenuation = t * t;
        vec3 radiance = uPointLights[i].color * uPointLights[i].intensity
                      * attenuation;
        lo += evalBrdf(N, V, L, radiance, albedo, metallic, roughness, F0);
    }

    vec3 lighting = ambient + lo;

    float dist = length(uCameraPos - vWorldPos);
    float fogF = computeFogFactor(dist);
    lighting = mix(lighting, uFogColor, fogF);

    FragColor = vec4(lighting, 1.0);
}
