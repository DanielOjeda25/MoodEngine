#version 450 core

// F3H31: convolucion de irradiance para IBL diffuse.
//
// Toma un cubemap del cielo (procedural Hosek-Wilkie o HDRI bakeado) y
// produce un cubemap de irradiance: cada texel almacena el integral
// hemisferico (cosine-weighted) del cubemap para ese normal.
//
// Algoritmo: Riemann sum sobre la hemisferia. Pasos de phi y theta
// uniformes (no Monte Carlo) — 4096 samples (~125 phi x 32 theta) son
// suficientes para irradiance baja-frecuencia.
//
// Implementacion escrita desde cero a partir del paper de Karis
// "Real Shading in UE4" (concepto publico, no codigo).
//
// Render target: una face del cubemap irradiance (32x32 RGB16F).
// Patron: 6 invocaciones del draw, una por face, con `uFaceView`
// matrix lookAt distinto.

in vec3 vDir;

out vec4 outColor;

uniform samplerCube uEnvCubemap;

const float PI = 3.14159265358979323846;

void main() {
    vec3 N = normalize(vDir);

    // TBN basis local al normal.
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);

    // Sampling uniforme: 125 phi (azimuth) x 32 theta (zenith) = 4000 samples.
    // Mas alto que el LearnOpenGL default (180x90) pero menos costo (32 vs 90
    // en theta). Trade-off bueno para 32x32 output.
    const float phiStep   = 2.0 * PI / 125.0;
    const float thetaStep = 0.5 * PI / 32.0;

    int numSamples = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += phiStep) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += thetaStep) {
            // Spherical → tangent space.
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));
            // Tangent → world.
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            // cos(theta) * sin(theta) viene del integrando hemisferico:
            //   integral_hemisphere L(w) cos(theta) dw
            // dw = sin(theta) dtheta dphi en spherical.
            irradiance += texture(uEnvCubemap, sampleVec).rgb
                        * cos(theta) * sin(theta);
            numSamples++;
        }
    }

    // PI normaliza el integrando hemisferico (lambert BRDF).
    irradiance = PI * irradiance / float(numSamples);

    outColor = vec4(irradiance, 1.0);
}
