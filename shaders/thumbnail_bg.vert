#version 450 core

// F3H14: fullscreen-triangle vert para el fondo gradient de los thumbnails
// de meshes. No requiere VBO — usa gl_VertexID + 3 vertices que cubren
// todo el clip space ([-1,3] x [-1,3]); el triangulo se recorta al
// viewport, lo que queda es exactamente el cuadrado [-1,1]^2 (full
// screen). UV emitida en [0,1] para el fragment.

out vec2 v_uv;

void main() {
    // Vertices del triangulo, codificados via gl_VertexID:
    //   id=0 -> (-1,-1)   bottom-left
    //   id=1 -> ( 3,-1)   bottom-right (off-screen)
    //   id=2 -> (-1, 3)   top-left     (off-screen)
    vec2 pos;
    if (gl_VertexID == 0)       pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1)  pos = vec2( 3.0, -1.0);
    else                         pos = vec2(-1.0,  3.0);

    gl_Position = vec4(pos, 0.0, 1.0);
    v_uv        = pos * 0.5 + 0.5;
}
