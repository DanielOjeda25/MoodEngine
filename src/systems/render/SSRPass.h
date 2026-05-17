#pragma once

// SSRPass (F2H61 Bloque C): Screen-Space Reflections.
//
// Algoritmo: Morgan McGuire 2014, linear DDA en view-space (no Hi-Z).
// Detalle del flow del shader en `shaders/ssr.frag`. Lee G-buffer
// parcial del scene FB (color + depth + normal RT) y escribe el color
// final con el reflejo aditivo a un FB destino HDR.
//
// Mismo patron que ColorGradingPass: single draw call, sin FBs internos.
// El caller le pasa src + dst FBs HDR del mismo tamano + matrices proj
// y los parametros del marcheo.
//
// El SSR es opt-in (default OFF) via `EnvironmentComponent::ssrEnabled`.
// Sin reflejo, el sceneFb pasa intacto al pase siguiente -- cero
// regresion vs F2H60.

#include "core/Types.h"

#include <glm/mat4x4.hpp>
#include <glad/gl.h>

#include <memory>

namespace Mood {

class IShader;
class OpenGLFramebuffer;

class SSRPass {
public:
    SSRPass();
    ~SSRPass();

    SSRPass(const SSRPass&) = delete;
    SSRPass& operator=(const SSRPass&) = delete;

    /// @brief Aplica SSR: lee color base + G-buffer (depth + normal),
    ///        escribe `dst` (HDR color con reflejo aditivo).
    /// @param srcColor    Color HDR a leer + reflejar. Puede venir post-SSAO
    ///                     para que el reflejo herede el AO de la superficie.
    /// @param srcGbuffer  Scene FB con G-buffer (depth texture + normal RT).
    ///                     Debe tener `withNormalRT=true` y `format=HDR`.
    ///                     Tipicamente es el m_sceneFb original.
    /// @param dst         FB HDR destino. Mismo tamano que srcColor.
    /// @param proj        Matriz de proyeccion del frame.
    /// @param invProj     Inversa de la proyeccion.
    /// @param maxSteps    Pasos del ray marching (typical 16-128).
    /// @param thickness   Tolerancia view-space para considerar hit (0.01-1.0).
    /// @param stepSize    Tamano de cada step view-space (0.05-0.5).
    /// @param intensity   Blend factor del reflejo aditivo [0..1].
    /// @return true si se escribio a dst. false si se skipeo (intensity 0
    ///         o normal RT ausente -- caller deberia usar srcColor para el
    ///         pase siguiente).
    bool apply(OpenGLFramebuffer& srcColor,
               OpenGLFramebuffer& srcGbuffer,
               OpenGLFramebuffer& dst,
               const glm::mat4& proj, const glm::mat4& invProj,
               u32 maxSteps, f32 thickness, f32 stepSize, f32 intensity);

private:
    std::unique_ptr<IShader> m_shader;
    GLuint                   m_vao = 0;
};

} // namespace Mood
