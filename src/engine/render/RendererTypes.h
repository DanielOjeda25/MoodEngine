#pragma once

// Tipos comunes compartidos por la RHI (Render Hardware Interface).
// Mantener este header liviano: solo tipos POD y aliases, nada de logica.

#include "core/Types.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mood {

/// @brief Handle opaco de una textura visible desde la UI (p.ej. ImGui::Image).
///        En el backend OpenGL es el GLuint del color attachment; en Vulkan
///        seria un VkDescriptorSet. Se expone como void* para no filtrar el
///        tipo concreto del backend.
using TextureHandle = void*;

/// @brief Valores de limpieza al comenzar un frame u operacion de render.
struct ClearValues {
    glm::vec4 color{0.0f, 0.0f, 0.0f, 1.0f};
    f32 depth = 1.0f;
    bool clearColor = true;
    bool clearDepth = true;
};

/// @brief Descriptor de un atributo de vertice (para la creacion de mallas).
///        Por ahora asume tipo float. Cuando se necesiten enteros / normales
///        empacados (Hito 3+), se agrega un enum `VertexAttributeType`.
struct VertexAttribute {
    u32 location = 0;       ///< layout(location = X) en el vertex shader.
    u32 componentCount = 3; ///< 1..4 componentes float.
};

} // namespace Mood
