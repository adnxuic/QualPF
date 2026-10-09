#pragma once

#include "qualpf/domain/result.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace qualpf::crystallography {
namespace detail {
struct SymmetryTableBuilder;
struct SpaceGroupRecord;
} // namespace detail

// Exact affine lattice operation on fractional COLUMN coordinates: R*x + t.
// Translation is an element of (Z/12Z)^3: this covers every retained setting.
// Equality and composition are modulo integral translations. No cell metric is
// implied: validity means an integral unimodular lattice map, not Euclidean
// orthogonality or membership in a particular finite space group.
class SymmetryOperation {
  public:
    using Rotation = std::array<std::int32_t, 9>; // row major
    using Translation = std::array<std::int32_t, 3>;
    static constexpr int translation_denominator = 12;
    static constexpr int rotation_bound = 32767;
    constexpr SymmetryOperation() = default; // identity
    [[nodiscard]] static Result<SymmetryOperation> create(Rotation rotation, Translation twelfths);
    [[nodiscard]] const Rotation &rotation() const noexcept { return rotation_; }
    [[nodiscard]] const Translation &translation_twelfths() const noexcept { return translation_; }
    [[nodiscard]] std::int64_t determinant() const noexcept;
    // a.compose(b) applies b first, then a. Range overflow is a stable error.
    [[nodiscard]] Result<SymmetryOperation> compose(const SymmetryOperation &b) const;
    [[nodiscard]] Result<SymmetryOperation> inverse() const;
    [[nodiscard]] Result<std::array<double, 3>> apply(std::array<double, 3> x,
                                                      bool wrap = true) const;
    [[nodiscard]] static Result<std::array<double, 3>> wrap_fractional(std::array<double, 3> x);
    bool operator==(const SymmetryOperation &) const = default;

  private:
    friend struct detail::SymmetryTableBuilder;
    constexpr SymmetryOperation(Rotation r, Translation t) : rotation_(r), translation_(t) {}
    Rotation rotation_{1, 0, 0, 0, 1, 0, 0, 0, 1};
    Translation translation_{};
};

struct SpaceGroupSetting {
    int id;                  // stable zero-based resource position, 0..550
    int index;               // one-based setting within international number
    int standard_id;         // zero-based registry id, not Fortran array index
    std::string_view choice; // includes axis/origin choice; "Unknown" is retained
    std::string_view to_setting;
    std::string_view to_standard; // descriptive source expressions, not transforms
};

class SpaceGroup {
  public:
    [[nodiscard]] int number() const noexcept;
    [[nodiscard]] std::string_view hermann_mauguin() const noexcept;
    [[nodiscard]] std::string_view hall() const noexcept;
    [[nodiscard]] std::string_view schoenflies() const noexcept;
    [[nodiscard]] std::array<std::string_view, 2> alternative_symbols() const noexcept;
    [[nodiscard]] const SpaceGroupSetting &setting() const noexcept;
    [[nodiscard]] int primitive_count() const noexcept;
    [[nodiscard]] int centering_count() const noexcept;
    // 0: acentric, 1: inversion at origin, 2: inversion away from origin.
    [[nodiscard]] int centrosymmetry() const noexcept;
    [[nodiscard]] std::span<const SymmetryOperation> operations() const noexcept;

  private:
    friend class SpaceGroupRegistry;
    explicit SpaceGroup(const detail::SpaceGroupRecord *record) : record_(record) {}
    const detail::SpaceGroupRecord *record_; // immutable static storage
};

enum class SymbolKind { hermann_mauguin, hall, either };

// Stateless facade over compile-time data: no I/O, initialization, environment,
// Qt, Fortran runtime, worker pool or mutable cache. Returned views never
// expire.
class SpaceGroupRegistry {
  public:
    static constexpr int group_count = 230;
    static constexpr int setting_count = 551;
    [[nodiscard]] Result<SpaceGroup> by_id(int id) const;
    [[nodiscard]] Result<std::vector<SpaceGroup>> settings(int number) const;
    [[nodiscard]] Result<SpaceGroup> by_number(int number,
                                               std::optional<int> setting = std::nullopt) const;
    [[nodiscard]] Result<SpaceGroup> find(std::string_view symbol,
                                          SymbolKind kind = SymbolKind::either) const;
};
} // namespace qualpf::crystallography
