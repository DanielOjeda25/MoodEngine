// Tests de JsonHelpers: round-trip de los tipos adaptados + el check de
// version de formato. Confirma que nlohmann_json + los adaptadores
// funcionan antes de que los serializers reales (mapa, proyecto) los usen.

#include <doctest/doctest.h>

#include "core/math/AABB.h"
#include "engine/serialization/JsonHelpers.h"

#include <glm/vec3.hpp>

#include <nlohmann/json.hpp>

#include <stdexcept>

using json = nlohmann::json;
using namespace Mood;

TEST_CASE("JsonHelpers: glm::vec3 round-trip") {
    const glm::vec3 original(1.5f, -2.25f, 3.75f);
    json j = original;
    CHECK(j.is_array());
    CHECK(j.size() == 3);

    const glm::vec3 decoded = j.get<glm::vec3>();
    CHECK(decoded.x == doctest::Approx(original.x));
    CHECK(decoded.y == doctest::Approx(original.y));
    CHECK(decoded.z == doctest::Approx(original.z));
}

TEST_CASE("JsonHelpers: AABB round-trip con min/max explicitos") {
    const AABB original{glm::vec3(-1.0f, 0.0f, -2.0f),
                        glm::vec3(3.0f, 5.0f, 1.0f)};
    json j = original;
    CHECK(j.contains("min"));
    CHECK(j.contains("max"));

    const AABB decoded = j.get<AABB>();
    CHECK(decoded.min == original.min);
    CHECK(decoded.max == original.max);
}

TEST_CASE("JsonHelpers: TileType serializa como string estable") {
    json j = TileType::SolidWall;
    CHECK(j.get<std::string>() == "solid_wall");

    json empty = TileType::Empty;
    CHECK(empty.get<std::string>() == "empty");

    // Round-trip: el string vuelve a ser el enum correcto.
    CHECK(j.get<TileType>() == TileType::SolidWall);
    CHECK(empty.get<TileType>() == TileType::Empty);
}

TEST_CASE("JsonHelpers: checkFormatVersion acepta igual o menor") {
    json ok = {{"version", k_MoodmapFormatVersion}};
    CHECK_NOTHROW(checkFormatVersion(ok, k_MoodmapFormatVersion, "moodmap"));

    json past = {{"version", k_MoodmapFormatVersion - 1}};
    // Formalmente ya soportado, aunque el serializer deba migrar.
    CHECK_NOTHROW(checkFormatVersion(past, k_MoodmapFormatVersion, "moodmap"));
}

TEST_CASE("JsonHelpers: checkFormatVersion rechaza futura") {
    json future = {{"version", k_MoodmapFormatVersion + 1}};
    CHECK_THROWS_AS(
        checkFormatVersion(future, k_MoodmapFormatVersion, "moodmap"),
        std::runtime_error);
}

TEST_CASE("JsonHelpers: checkFormatVersion rechaza falta de campo") {
    json missing = json::object();
    CHECK_THROWS_AS(
        checkFormatVersion(missing, k_MoodmapFormatVersion, "moodmap"),
        std::runtime_error);
}
