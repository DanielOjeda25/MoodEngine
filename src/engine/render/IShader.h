#pragma once

// Interfaz abstracta de un shader program (vertex + fragment). La carga desde
// archivo y la compilacion la hace cada backend concreto.

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string_view>

namespace Mood {

class IShader {
public:
    virtual ~IShader() = default;

    /// @brief Activa el program para los proximos draw calls.
    virtual void bind() const = 0;

    /// @brief Desactiva el program.
    virtual void unbind() const = 0;

    // Setters de uniforms. Una variante por tipo; C++ no permite templates
    // virtuales. Se iran agregando mas cuando los hitos las pidan.
    virtual void setInt(std::string_view name, int value) = 0;
    virtual void setFloat(std::string_view name, float value) = 0;
    virtual void setVec3(std::string_view name, const glm::vec3& value) = 0;
    virtual void setVec4(std::string_view name, const glm::vec4& value) = 0;
    virtual void setMat4(std::string_view name, const glm::mat4& value) = 0;
};

} // namespace Mood
