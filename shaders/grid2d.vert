#version 450 core

// F2H28 Bloque E: fullscreen triangle SIN VBO (mismo patron que
// post_process.vert). Genera 3 vertices NDC desde gl_VertexID y pasa
// el NDC al frag para que compute la posicion world del pixel sobre
// el plano de la vista orto.
//
// gl_VertexID 0 -> NDC (-1,-1)
//             1 -> NDC ( 3,-1)
//             2 -> NDC (-1, 3)
// El triangulo cubre TODO el viewport; lo que cae fuera de (-1,1) lo
// recorta el rasterizador.

out vec2 vNdc;

void main() {
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                    (gl_VertexID == 2) ? 3.0 : -1.0);
    vNdc = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
