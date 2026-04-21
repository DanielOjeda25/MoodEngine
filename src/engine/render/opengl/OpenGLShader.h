#pragma once

// Implementacion OpenGL de IShader. Compila vertex + fragment desde archivos
// de texto y cachea ubicaciones de uniforms para no consultarlas por frame.

#include "engine/render/IShader.h"

#include <glad/gl.h>

#include <string>
#include <unordered_map>

namespace Mood {

class OpenGLShader final : public IShader {
public:
    /// @brief Compila y linkea el program desde dos archivos GLSL.
    /// @param vertexPath Ruta al vertex shader.
    /// @param fragmentPath Ruta al fragment shader.
    /// @throws std::runtime_error si falla la compilacion o el linkeo.
    OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath);

    ~OpenGLShader() override;

    OpenGLShader(const OpenGLShader&) = delete;
    OpenGLShader& operator=(const OpenGLShader&) = delete;

    void bind() const override;
    void unbind() const override;

    void setInt(std::string_view name, int value) override;
    void setFloat(std::string_view name, float value) override;
    void setVec3(std::string_view name, const glm::vec3& value) override;
    void setVec4(std::string_view name, const glm::vec4& value) override;
    void setMat4(std::string_view name, const glm::mat4& value) override;

private:
    /// @brief Devuelve la ubicacion del uniform, usando cache interna.
    GLint locationOf(std::string_view name);

    GLuint m_program = 0;

    // Cache: glGetUniformLocation no es gratis. La clave se guarda como
    // std::string porque unordered_map no permite buscar por string_view sin
    // configurar transparent hashing (C++20). En su lugar materializamos la
    // clave al insertar, lo cual ocurre una vez por uniform.
    std::unordered_map<std::string, GLint> m_uniformCache;
};

} // namespace Mood
