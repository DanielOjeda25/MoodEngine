#pragma once

// Implementacion OpenGL de IShader. Compila vertex + fragment desde archivos
// de texto y cachea ubicaciones de uniforms para no consultarlas por frame.

#include "core/Types.h"
#include "engine/render/IShader.h"

#include <glad/gl.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

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
    void setVec2(std::string_view name, const glm::vec2& value) override;
    void setVec3(std::string_view name, const glm::vec3& value) override;
    void setVec4(std::string_view name, const glm::vec4& value) override;
    void setMat4(std::string_view name, const glm::mat4& value) override;

    /// @brief Si vertex.glsl o fragment.glsl cambiaron en disco desde la
    ///        ultima carga, recompila + relinkea atomicamente. Si la nueva
    ///        compilacion falla, loguea el error y mantiene el program
    ///        anterior funcionando (no rompe el render).
    /// @return true si se ejecuto algun recompile (con exito o error);
    ///         false si los archivos no cambiaron.
    bool tryReloadIfChanged();

    /// @brief Tick global del hot-reload: llamar una vez por frame desde el
    ///        main loop. Acumula dt y cada 500ms recorre todos los shaders
    ///        vivos pidiendoles que chequeen mtime + recompilen si cambio.
    /// @param dt Delta time del frame en segundos.
    static void tickHotReload(f32 dt);

private:
    /// @brief Devuelve la ubicacion del uniform, usando cache interna.
    GLint locationOf(std::string_view name);

    GLuint m_program = 0;

    // Hot-reload: paths originales + mtime de la ultima compilacion exitosa.
    std::string m_vertexPath;
    std::string m_fragmentPath;
    std::filesystem::file_time_type m_vertexMtime{};
    std::filesystem::file_time_type m_fragmentMtime{};

    // Cache: glGetUniformLocation no es gratis. La clave se guarda como
    // std::string porque unordered_map no permite buscar por string_view sin
    // configurar transparent hashing (C++20). En su lugar materializamos la
    // clave al insertar, lo cual ocurre una vez por uniform.
    std::unordered_map<std::string, GLint> m_uniformCache;

    // Registro de instancias vivas para el tick global de hot-reload.
    // Un solo contexto GL en el editor; sin mutex.
    static std::vector<OpenGLShader*> s_allShaders;
    static f32 s_throttleAccum;
};

} // namespace Mood
