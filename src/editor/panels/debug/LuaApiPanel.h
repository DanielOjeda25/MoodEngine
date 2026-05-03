#pragma once

// Panel de referencia rapida de la API Lua expuesta por el motor
// (Hito 22 Bloque 4). Lista hardcoded — cada vez que se agrega un
// binding nuevo en `engine/scripting/LuaBindings.cpp` o
// `engine/game/GameOverlay/State`, sumar una linea aca.
//
// No genera codigo automatico — es doc humana, no introspeccion. Si
// el motor crece a 50+ bindings vale la pena agregar un test que
// valide que cada entry del panel existe en LuaBindings.

#include "editor/panels/IPanel.h"

namespace Mood {

class LuaApiPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Lua API"; }
};

} // namespace Mood
