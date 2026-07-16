#pragma once

#include <guiddef.h>
#include <string_view>

namespace foocrate {

inline constexpr std::string_view kComponentName{"FooCrate"};
inline constexpr std::string_view kComponentVersion{"1.0.2"};
inline constexpr std::string_view kComponentFileName{"foo_crate.dll"};
inline constexpr GUID kComponentGuid{
    0x5aeac28d,
    0xed01,
    0x4574,
    {0xa9, 0x9b, 0xf9, 0xa2, 0x44, 0xb5, 0xb8, 0x15},
};

} // namespace foocrate
