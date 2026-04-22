// Unica TU donde se generan los simbolos de las libs single-header de stb.
// Si este archivo se incluyera dos veces (en dos TUs), habria duplicate
// symbol en el linker.

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
