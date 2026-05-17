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
// Luces directas: directional (con shadow factor del Hito 16) + point
// lights via Forward+ tile-based culling (Hito 18). Las point lights
// viven en un SSBO global; un par de SSBOs adicionales describen el
// `LightGrid` (offset/count por tile + flat array de indices). El shader
// loopea SOLO las luces que afectan al tile del fragment, escalando
// linealmente con luces visibles en lugar de N_total.

#define PI 3.14159265359

in vec3  vColor;
in vec2  vUv;
in vec3  vWorldPos;
in vec3  vWorldNormal;
in vec3  vViewSpaceNormal;  // F2H61: view-space normal para el G-buffer (SSR).
in float vViewSpaceZ;   // F2H60: |view-space Z| del vert para seleccion de cascada.

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

// Hito 18: point lights migradas a SSBOs (Forward+).
//
// std430 layout para `PointLight`: cada `vec3` toma 16 bytes (alineamiento
// de vec4) sin que necesitemos struct padding. El CPU sube un buffer
// matcheando este layout exacto.
struct PointLight {
    vec3 position;
    float pad0;       // padding std430 implícito
    vec3 color;
    float intensity;
    float radius;
    float pad1;       // alinea el siguiente PointLight a 16 bytes
    float pad2;
    float pad3;
};
layout(std430, binding = 2) readonly buffer PointLightBuffer {
    PointLight uPointLights[];
};

// LightGrid (Forward+): por tile, donde empezar a leer y cuantas luces
// procesar.
struct TileLightList {
    uint offset;
    uint count;
};
layout(std430, binding = 3) readonly buffer TileBuffer {
    TileLightList uTiles[];
};
layout(std430, binding = 4) readonly buffer LightIndexBuffer {
    uint uLightIndices[];
};

// Tamaño del tile (matchea k_LightGridTileSize en C++).
uniform int  uTileSize;       // = 16
uniform int  uTilesX;
uniform int  uTilesY;

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
uniform float        uIblIntensity;      // Hito 18: multiplicador del IBL

// --- Shadow mapping (Hito 16, refactor F2H60 a CSM) ---
// uShadowMap pasa de sampler2DShadow (single map) a sampler2DArrayShadow
// (N layers). El frag elige la cascada `i` por `vViewSpaceZ` y samplea
// con coords `(uv.x, uv.y, layer, refDepth)`.
#define MAX_CSM_CASCADES 4
uniform int                  uShadowEnabled;
uniform sampler2DArrayShadow uShadowMap;     // unit 1
uniform float                uShadowBias;
uniform int                  uCascadeCount;                       // 1..MAX_CSM_CASCADES
uniform mat4                 uLightSpaces[MAX_CSM_CASCADES];
uniform float                uCascadeSplits[MAX_CSM_CASCADES + 1]; // view-space distances

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

// F2H60: sampling de UNA cascada del shadow map array. Recibe el worldPos
// del fragment + el indice de cascada (0..uCascadeCount-1). Computa
// lightSpacePos para esa cascada con `uLightSpaces[cascadeIdx]`, proyecta
// a NDC, mapea a [0,1], aplica bias, y samplea con PCF 3x3 sobre el layer
// correspondiente del 2D-array. Cada `texture()` con sampler2DArrayShadow
// hace 4 taps hardware-PCF, totalizando 9 * 4 = 36 muestras efectivas.
float sampleShadowCascade(vec3 worldPos, int cascadeIdx) {
    vec4 lightSpacePos = uLightSpaces[cascadeIdx] * vec4(worldPos, 1.0);
    vec3 proj = lightSpacePos.xyz / max(lightSpacePos.w, 1e-5);
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    // Bias escalado por indice de cascada: cascadas lejanas tienen texel
    // mas grande, necesitan mas bias para evitar acne. Crecimiento lineal
    // simple (x1, x2, x3, x4) — suficiente para evitar acne sin generar
    // peter-panning visible en la cascada 0.
    float biasScale = float(cascadeIdx + 1);
    float refDepth = proj.z - uShadowBias * biasScale;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0).xy);
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // sampler2DArrayShadow: coord (x, y, layer, refDepth).
            sum += texture(uShadowMap,
                            vec4(proj.xy + offset,
                                  float(cascadeIdx),
                                  refDepth));
        }
    }
    return sum / 9.0;
}

// F2H60: elige cascada por `vViewSpaceZ` y samplea. Si el fragment esta
// cerca del borde de la cascada actual (ultimo 10% del rango), blend con
// la cascada siguiente para evitar banding visible al cruzar fronteras.
// Si no hay cascada siguiente (en la ultima), no hay blend.
float sampleShadow(vec3 worldPos) {
    if (uShadowEnabled == 0) return 1.0;

    int cascadeIdx = uCascadeCount - 1;  // fallback: ultima cascada
    for (int i = 0; i < uCascadeCount; ++i) {
        if (vViewSpaceZ < uCascadeSplits[i + 1]) {
            cascadeIdx = i;
            break;
        }
    }
    float sNear = sampleShadowCascade(worldPos, cascadeIdx);

    // Blend con la siguiente cascada en el ultimo 15% del rango. Esto
    // evita el "salto" visual visible cuando un fragment cruza la
    // frontera entre cascada i y cascada i+1.
    if (cascadeIdx + 1 < uCascadeCount) {
        float splitNear = uCascadeSplits[cascadeIdx];
        float splitFar  = uCascadeSplits[cascadeIdx + 1];
        float range = max(splitFar - splitNear, 1e-4);
        float t = (vViewSpaceZ - splitNear) / range;  // 0..1 dentro de la cascada
        const float kBlendStart = 0.85;
        if (t > kBlendStart) {
            float w = (t - kBlendStart) / (1.0 - kBlendStart);  // 0..1
            float sFar = sampleShadowCascade(worldPos, cascadeIdx + 1);
            return mix(sNear, sFar, w);
        }
    }
    return sNear;
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

layout(location = 0) out vec4 FragColor;
// F2H61: G-buffer parcial -- view-space normal + flag de "pixel PBR".
// Si el FB no tiene location 1 attached (path LDR), el driver ignora
// esta escritura. Si SI lo tiene, el SSRPass lee desde aca.
//   rgb = normal en view-space (componentes [-1,1])
//   a   = 1.0 marca "este pixel fue escrito por un shader PBR"
// SSR descarta pixels con a < 0.5 (skybox/particles/debug no escriben
// location 1 y quedan con el clear value (0,0,0,0)).
layout(location = 1) out vec4 NormalRT;

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

        ambient = (kD_ibl * diffuseIBL + specularIBL) * ao * uIblIntensity;
    } else {
        ambient = uAmbient * albedo * ao;
    }

    // Directional + shadow.
    vec3 lo = vec3(0.0);
    if (uDirectional.enabled == 1) {
        vec3 L = normalize(-uDirectional.direction);
        vec3 radiance = uDirectional.color * uDirectional.intensity;
        float shadowF = sampleShadow(vWorldPos);  // F2H60: pasa worldPos para que el frag elija cascada CSM.
        lo += evalBrdf(N, V, L, radiance, albedo, metallic, roughness, F0)
              * shadowF;
    }

    // Point lights via Forward+ tile lookup. gl_FragCoord.xy esta en
    // pixeles (origen abajo-izquierda en GL); el LightGrid mapea (x,y)
    // -> tile a 16 pixeles. Solo iteramos las luces del tile actual.
    int tileX = clamp(int(gl_FragCoord.x) / uTileSize, 0, uTilesX - 1);
    int tileY = clamp(int(gl_FragCoord.y) / uTileSize, 0, uTilesY - 1);
    uint tileIdx = uint(tileY * uTilesX + tileX);
    TileLightList tile = uTiles[tileIdx];
    for (uint k = 0u; k < tile.count; ++k) {
        uint lightIdx = uLightIndices[tile.offset + k];
        PointLight pl = uPointLights[lightIdx];
        vec3 toL = pl.position - vWorldPos;
        float dist = length(toL);
        if (dist > pl.radius) continue;
        vec3 L = toL / max(dist, 1e-4);
        float t = 1.0 - clamp(dist / pl.radius, 0.0, 1.0);
        float attenuation = t * t;
        vec3 radiance = pl.color * pl.intensity * attenuation;
        lo += evalBrdf(N, V, L, radiance, albedo, metallic, roughness, F0);
    }

    vec3 lighting = ambient + lo;

    float dist = length(uCameraPos - vWorldPos);
    float fogF = computeFogFactor(dist);
    lighting = mix(lighting, uFogColor, fogF);

    FragColor = vec4(lighting, 1.0);

    // F2H61: emitir view-space normal al G-buffer. SSRPass lo lee. Si el
    // FB no tiene attachment en location 1 (path LDR / FB sin normal RT),
    // GL ignora silenciosamente esta escritura.
    NormalRT = vec4(normalize(vViewSpaceNormal), 1.0);
}
