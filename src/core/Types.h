#pragma once

// Aliases numericos cortos y explicitos para usar en todo el motor.
// Preferir estos sobre los tipos crudos cuando el tamano importa (buffers,
// serializacion, APIs graficas). Para contadores locales, int sigue siendo
// aceptable.

#include <cstddef>
#include <cstdint>

namespace Mood {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

static_assert(sizeof(f32) == 4, "f32 debe ser IEEE-754 de 32 bits");
static_assert(sizeof(f64) == 8, "f64 debe ser IEEE-754 de 64 bits");

} // namespace Mood
