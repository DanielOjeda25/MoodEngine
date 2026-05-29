#version 450 core

// F3H31: vertex shader del sky procedural Hosek-Wilkie.
//
// Render target: una face del cubemap procedural (offscreen FBO).
// Patron: fullscreen quad de 4 vertices (TRIANGLE_STRIP) en clip space,
// el fragment reconstruye la direccion en el cielo desde clip + invVP.
//
// El renderer setea `uInvViewProjection` por face — 6 invocaciones del
// draw, una por face del cubemap. La projection es 90° perspectiva
// cuadrada, las 6 views son lookAt desde (0,0,0) a +X/-X/+Y/-Y/+Z/-Z.

out vec3 vDir;

uniform mat4 uInvViewProjection;

void main() {
    // 4 vertices del fullscreen quad como TRIANGLE_STRIP.
    const vec2 positions[4] = vec2[4](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    vec2 pos = positions[gl_VertexID];

    // Reconstruir direccion mundo desde clip coords.
    // z = 1.0 → far plane, w = 1.0 → standard NDC.
    vec4 clip = vec4(pos, 1.0, 1.0);
    vec4 worldDir = uInvViewProjection * clip;
    vDir = worldDir.xyz;  // no normalizamos aca, el frag lo hace

    gl_Position = vec4(pos, 0.0, 1.0);
}
