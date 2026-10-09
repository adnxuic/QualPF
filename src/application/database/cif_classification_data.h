#pragma once
#include <array>

namespace qualpf::application::detail {
// Compatibility classification data, not a general oxidation-state model.
// Exact third row of ox_numb(:,1:108) in Legacy elements.f90, SHA256:
// 961ce26b85afd9e74bcb7e5502c298ec72b364647cafebfe44db5a4adb301569
// Original table is unchanged. Entry i corresponds to atomic number i+1.
inline constexpr std::array<int, 108> common_oxidation_number{
    1, 0, 1, 2,  3, 4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, -1, 0, 1, 2, 3, 4, 5, 6, 4, 3,  3,
    2, 2, 2, 3,  4, 3, 6,  -1, 2,  1, 2, 3, 4, 5, 6, 7, 3,  3, 2, 1, 2, 3, 4, 3, 6, -1, 4,
    1, 2, 3, 3,  3, 3, 3,  3,  3,  3, 3, 3, 3, 3, 3, 3, 3,  4, 5, 6, 4, 4, 3, 2, 3, 2,  3,
    2, 3, 2, -1, 2, 1, 2,  3,  4,  5, 6, 5, 4, 3, 3, 3, 3,  3, 3, 3, 2, 3, 4, 5, 6, 7,  8};
} // namespace qualpf::application::detail
