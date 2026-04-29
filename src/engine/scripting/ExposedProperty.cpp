#include "engine/scripting/ExposedProperty.h"

namespace Mood {

ExposedType typeOf(const ExposedValue& v) {
    return std::visit([](auto&& arg) -> ExposedType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, f32>)         return ExposedType::Number;
        else if constexpr (std::is_same_v<T, bool>)   return ExposedType::Bool;
        else if constexpr (std::is_same_v<T, std::string>) return ExposedType::String;
        else if constexpr (std::is_same_v<T, glm::vec3>)   return ExposedType::Vec3;
        else                                           return ExposedType::Number;
    }, v);
}

} // namespace Mood
