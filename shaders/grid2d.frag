#version 450 core

// F2H28 Bloque E: grid 2D estilo Valve Hammer Editor para los
// viewports ortograficos del workspace "Editor de mapas". Pintado
// como fullscreen quad ANTES del wireframe pass (depth write off
// para no tapar la geometria) sobre fondo negro.
//
// Computa la posicion world del pixel proyectada al plano de la
// camara (en coords right/up, locales al view) desde:
//   worldXY = panOffset + ndc * (halfW, halfH)
// con halfH = worldHeight/2 y halfW = halfH * aspect.
//
// Dibuja dos jerarquias de lineas:
//   - Menor: cada `uSnapStep` unidades (default 16) -> gris oscuro.
//   - Mayor: cada `uSnapStep * 8` unidades            -> naranja Valve.
//
// Antialiasing con `fwidth` -> grosor estable a cualquier zoom (las
// lineas no se engordan al hacer zoom out ni desaparecen al zoom in).

in vec2 vNdc;
out vec4 FragColor;

uniform vec2  uPanOffset;   // (right, up) world offset del centro del view
uniform float uWorldHeight; // altura del frustum orto en world units
uniform float uAspect;      // panelW / panelH
uniform float uSnapStep;    // unidades world entre lineas menores
uniform vec3  uBgColor;     // fondo (negro)
uniform vec3  uMinorColor;  // gris oscuro
uniform vec3  uMajorColor;  // naranja Valve #F58220

// Devuelve 1.0 sobre la linea (mod(coord, spacing) ~ 0) con AA, 0.0
// entre lineas. La derivada `fwidth(u)` da el ancho de un pixel en
// unidades de `u` -> grosor de ~1 pixel sin importar el zoom.
float gridLine1d(float coord, float spacing) {
    float u = coord / spacing;
    float d = max(fwidth(u), 1e-6);
    return 1.0 - smoothstep(0.0, d, abs(fract(u - 0.5) - 0.5));
}

void main() {
    float halfH = uWorldHeight * 0.5;
    float halfW = halfH * uAspect;
    vec2 worldXY = uPanOffset + vNdc * vec2(halfW, halfH);

    float minorSnap = uSnapStep;
    float majorSnap = uSnapStep * 8.0;

    // Cada eje contribuye su linea por separado; un pixel sobre la
    // interseccion de dos lineas tiene `max(x,y) = 1` en ambos.
    float minor = max(gridLine1d(worldXY.x, minorSnap),
                       gridLine1d(worldXY.y, minorSnap));
    float major = max(gridLine1d(worldXY.x, majorSnap),
                       gridLine1d(worldXY.y, majorSnap));

    vec3 col = uBgColor;
    col = mix(col, uMinorColor, minor);
    col = mix(col, uMajorColor, major);  // mayor encima del menor

    FragColor = vec4(col, 1.0);
}
