// Smoke test de Lua + sol2 (Hito 8 Bloque 1).
// No prueba ScriptSystem ni bindings del motor todavia — solo que la
// infraestructura de sol2 esta disponible y ejecutable en el target de
// tests. Los tests reales vienen en el Bloque 7.

#include <doctest/doctest.h>

#include <sol/sol.hpp>

#include <string>

TEST_CASE("Lua + sol2: sol::state arranca y ejecuta script inline") {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // Ejecutar un script trivial. Si sol2 o Lua no estan bien linkeados,
    // esto no compila o crashea.
    sol::protected_function_result r = lua.safe_script(
        "x = 1 + 2", sol::script_pass_on_error);
    REQUIRE(r.valid());

    const int x = lua["x"];
    CHECK(x == 3);
}

TEST_CASE("Lua + sol2: funcion Lua llamable desde C++") {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    lua.safe_script(R"(
        function doble(n)
            return n * 2
        end
    )");

    sol::function f = lua["doble"];
    const int r = f(21);
    CHECK(r == 42);
}

TEST_CASE("Lua + sol2: error de sintaxis se reporta sin crashear") {
    sol::state lua;
    sol::protected_function_result r = lua.safe_script(
        "esto no es lua valido @#$", sol::script_pass_on_error);
    CHECK_FALSE(r.valid());
    // r.get<std::string>() contiene el mensaje de error; no lo asserteamos
    // porque el texto exacto depende del parser de Lua.
}
