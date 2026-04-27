// Tests del LightSystem (Hito 11). buildFrameData es la unidad pura: itera
// el Scene y devuelve un snapshot. bindUniforms se valida con un mock de
// IShader que registra las llamadas — cubre que el orden y los nombres de
// uniforms coinciden con `lit.frag`.

#include <doctest/doctest.h>

#include "engine/render/IShader.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/LightSystem.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Mood;

namespace {

// Mock simple: guarda el nombre del ultimo set de cada uniform y un contador
// global de llamadas. Suficiente para tests de "se llamo con esto".
class RecordingShader : public IShader {
public:
    void bind() const override {}
    void unbind() const override {}
    void setInt(std::string_view n, int v) override {
        ints[std::string(n)] = v; ++calls;
    }
    void setFloat(std::string_view n, float v) override {
        floats[std::string(n)] = v; ++calls;
    }
    void setVec3(std::string_view n, const glm::vec3& v) override {
        vec3s[std::string(n)] = v; ++calls;
    }
    void setVec4(std::string_view n, const glm::vec4& v) override {
        vec4s[std::string(n)] = v; ++calls;
    }
    void setMat4(std::string_view n, const glm::mat4& v) override {
        mat4s[std::string(n)] = v; ++calls;
    }

    std::unordered_map<std::string, int> ints;
    std::unordered_map<std::string, float> floats;
    std::unordered_map<std::string, glm::vec3> vec3s;
    std::unordered_map<std::string, glm::vec4> vec4s;
    std::unordered_map<std::string, glm::mat4> mat4s;
    int calls = 0;
};

} // namespace

TEST_CASE("LightSystem::buildFrameData con scene vacia no tiene luces") {
    Scene scene;
    LightSystem ls;
    auto data = ls.buildFrameData(scene);
    CHECK_FALSE(data.directional.enabled);
    CHECK(data.pointLights.empty());
}

TEST_CASE("LightSystem captura UNA Directional, ignora el resto") {
    // El orden de iteracion del registry de EnTT no es estable, asi que
    // no asumimos cual de las dos directionales gana — solo que se quedo
    // con UNA y no con ambas merged.
    Scene scene;
    LightSystem ls;

    Entity a = scene.createEntity("sun_a");
    LightComponent la{};
    la.type = LightComponent::Type::Directional;
    la.color = glm::vec3(1.0f, 0.0f, 0.0f);
    la.intensity = 0.7f;
    la.enabled = true;
    a.addComponent<LightComponent>(la);

    Entity b = scene.createEntity("sun_b");
    LightComponent lb{};
    lb.type = LightComponent::Type::Directional;
    lb.color = glm::vec3(0.0f, 1.0f, 0.0f);
    lb.intensity = 0.3f;
    lb.enabled = true;
    b.addComponent<LightComponent>(lb);

    auto data = ls.buildFrameData(scene);
    CHECK(data.directional.enabled);
    // El color es el de UNA de las dos (cualquiera): exactamente rojo o verde,
    // nunca un mix.
    const bool isA = (data.directional.color.r > 0.9f && data.directional.color.g < 0.1f);
    const bool isB = (data.directional.color.r < 0.1f && data.directional.color.g > 0.9f);
    CHECK((isA || isB));
    // Idem la intensidad: una de las dos, no la suma.
    const bool intIsA = std::abs(data.directional.intensity - 0.7f) < 1e-3f;
    const bool intIsB = std::abs(data.directional.intensity - 0.3f) < 1e-3f;
    CHECK((intIsA || intIsB));
}

TEST_CASE("LightSystem usa la posicion del TransformComponent para Point lights") {
    Scene scene;
    LightSystem ls;

    Entity p = scene.createEntity("p");
    auto& t = p.getComponent<TransformComponent>();
    t.position = glm::vec3(3.0f, 4.0f, -1.0f);
    LightComponent lc{};
    lc.type = LightComponent::Type::Point;
    lc.intensity = 2.0f;
    lc.radius = 8.0f;
    lc.enabled = true;
    p.addComponent<LightComponent>(lc);

    auto data = ls.buildFrameData(scene);
    REQUIRE(data.pointLights.size() == 1);
    CHECK(data.pointLights[0].position.x == doctest::Approx(3.0f));
    CHECK(data.pointLights[0].position.y == doctest::Approx(4.0f));
    CHECK(data.pointLights[0].position.z == doctest::Approx(-1.0f));
    CHECK(data.pointLights[0].intensity == doctest::Approx(2.0f));
    CHECK(data.pointLights[0].radius == doctest::Approx(8.0f));
}

TEST_CASE("LightSystem cap a k_MaxPointLights, ignora el exceso silencio") {
    Scene scene;
    LightSystem ls;
    const u32 N = k_MaxPointLights + 3;
    for (u32 i = 0; i < N; ++i) {
        Entity p = scene.createEntity("p" + std::to_string(i));
        LightComponent lc{};
        lc.type = LightComponent::Type::Point;
        lc.enabled = true;
        p.addComponent<LightComponent>(lc);
    }
    auto data = ls.buildFrameData(scene);
    CHECK(data.pointLights.size() == k_MaxPointLights);
}

TEST_CASE("LightSystem ignora las luces deshabilitadas") {
    Scene scene;
    LightSystem ls;

    Entity off = scene.createEntity("off");
    LightComponent lc{};
    lc.type = LightComponent::Type::Point;
    lc.enabled = false;
    off.addComponent<LightComponent>(lc);

    auto data = ls.buildFrameData(scene);
    CHECK(data.pointLights.empty());
}

TEST_CASE("LightSystem normaliza la direccion del Directional") {
    Scene scene;
    LightSystem ls;
    Entity d = scene.createEntity("d");
    LightComponent lc{};
    lc.type = LightComponent::Type::Directional;
    lc.direction = glm::vec3(2.0f, 0.0f, 0.0f); // no-normalizada
    lc.intensity = 1.0f;
    lc.enabled = true;
    d.addComponent<LightComponent>(lc);

    auto data = ls.buildFrameData(scene);
    REQUIRE(data.directional.enabled);
    CHECK(glm::length(data.directional.direction) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(data.directional.direction.x == doctest::Approx(1.0f));
}

TEST_CASE("LightSystem::bindUniforms sube los nombres esperados al shader") {
    Scene scene;
    LightSystem ls;

    // Setup: 1 directional + 2 point.
    Entity dir = scene.createEntity("dir");
    {
        LightComponent lc{};
        lc.type = LightComponent::Type::Directional;
        lc.color = glm::vec3(0.4f, 0.5f, 0.6f);
        lc.intensity = 0.8f;
        lc.direction = glm::vec3(0, -1, 0);
        lc.enabled = true;
        dir.addComponent<LightComponent>(lc);
    }
    Entity p0 = scene.createEntity("p0");
    {
        auto& t = p0.getComponent<TransformComponent>();
        t.position = glm::vec3(1, 2, 3);
        LightComponent lc{};
        lc.type = LightComponent::Type::Point;
        lc.intensity = 1.5f;
        lc.radius = 7.0f;
        lc.enabled = true;
        p0.addComponent<LightComponent>(lc);
    }
    Entity p1 = scene.createEntity("p1");
    {
        LightComponent lc{};
        lc.type = LightComponent::Type::Point;
        lc.enabled = true;
        p1.addComponent<LightComponent>(lc);
    }

    auto data = ls.buildFrameData(scene);
    REQUIRE(data.pointLights.size() == 2);

    RecordingShader sh;
    ls.bindUniforms(sh, data, glm::vec3(10, 5, -2));

    // Globales esperados.
    CHECK(sh.vec3s.count("uCameraPos") == 1);
    CHECK(sh.vec3s.count("uAmbient")   == 1);
    // Hito 17: uSpecularStrength / uShininess fueron removidos del
    // LightSystem (eran del Blinn-Phong; el PBR usa metallic-roughness).

    // Directional fields.
    CHECK(sh.vec3s.count("uDirectional.direction") == 1);
    CHECK(sh.vec3s.count("uDirectional.color") == 1);
    CHECK(sh.floats.count("uDirectional.intensity") == 1);
    CHECK(sh.ints.count("uDirectional.enabled") == 1);
    CHECK(sh.ints["uDirectional.enabled"] == 1);

    // Hito 18: las point lights ya NO se suben como uniform array
    // desde LightSystem::bindUniforms. Viven en un SSBO (binding 2)
    // que el EditorRenderPass actualiza por frame; la indexacion la
    // hace el LightGrid (binding 3 + 4). Verificamos que NO aparezcan
    // como uniforms.
    CHECK(sh.ints.count("uActivePointLights") == 0);
    CHECK(sh.vec3s.count("uPointLights[0].position") == 0);
    CHECK(sh.vec3s.count("uPointLights[1].position") == 0);

    // El frame data SI sigue trayendo las point lights — lo que
    // cambia es el path de upload a GPU, no la enumeracion CPU.
    CHECK(data.pointLights.size() == 2);
}
