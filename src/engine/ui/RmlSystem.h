#pragma once

// RmlUi `SystemInterface` adapter (Hito 20). Expone reloj + logger del
// motor a RmlUi. Una unica instancia se registra al iniciar el editor
// con `Rml::SetSystemInterface(...)`.

#include "core/Types.h"

#include <RmlUi/Core/SystemInterface.h>

#include <chrono>

namespace Mood {

class RmlSystem : public Rml::SystemInterface {
public:
    RmlSystem();

    /// @brief Tiempo en segundos desde el ctor del adapter. RmlUi lo usa
    ///        para animaciones CSS y timers internos.
    double GetElapsedTime() override;

    /// @brief Rutea logs de RmlUi al canal `editor` con el nivel
    ///        equivalente. Devuelve `true` para que RmlUi siga (no
    ///        propaga errores fatales hacia arriba).
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

private:
    std::chrono::steady_clock::time_point m_start;
};

} // namespace Mood
