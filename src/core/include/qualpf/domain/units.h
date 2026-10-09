#pragma once

#include <cstdint>
#include <limits>

namespace qualpf {

static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53,
              "Core transport values require IEEE binary64 double");

// Storage only: no implicit unit conversion and no diffraction mathematics here.
template<class Tag>
struct Quantity {
    double value = 0.0;
    explicit constexpr Quantity(double v = 0.0) noexcept : value(v) {}
    friend bool operator==(const Quantity&, const Quantity&) = default;
};
struct TwoThetaTag {};
struct AngularWidthTag {};
struct DSpacingTag {};
struct WavelengthTag {};
using TwoThetaDegrees = Quantity<TwoThetaTag>;
using AngularWidthDegrees = Quantity<AngularWidthTag>;
using DSpacingAngstrom = Quantity<DSpacingTag>;
using WavelengthAngstrom = Quantity<WavelengthTag>;

template<class Tag>
struct Identifier {
    std::uint64_t value = 0; // Zero is invalid; IDs are assigned by the owning importer/session.
    friend bool operator==(const Identifier&, const Identifier&) = default;
};
struct DatasetTag {};
struct PeakTag {};
struct ReflectionTag {};
using DatasetId = Identifier<DatasetTag>;
using PeakId = Identifier<PeakTag>;
using ReflectionId = Identifier<ReflectionTag>;

struct AngularRange {
    TwoThetaDegrees minimum;
    TwoThetaDegrees maximum;
};

enum class SourcePrecision { unknown, float32_abi, float64 };
enum class IntensityScale { unspecified, counts, relative };
enum class NumericalPolicy { unspecified, legacy_mixed_precision, explicit_float64 };

} // namespace qualpf
