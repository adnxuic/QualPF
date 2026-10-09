#pragma once
#include "gui/state/search_session.h"
#include "qualpf/export/pattern_export.h"
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace phase24b {
template <class T> T scalar(std::istream &in) {
    T value{};
    in.read(reinterpret_cast<char *>(&value), sizeof(value));
    if (!in)
        throw std::runtime_error("Invalid frozen fixture");
    return value;
}
inline std::vector<double> vector(std::istream &in) {
    const auto size = scalar<std::uint32_t>(in);
    if (size > 1000000)
        throw std::runtime_error("Fixture too large");
    std::vector<double> value(size);
    in.read(reinterpret_cast<char *>(value.data()), size * sizeof(double));
    if (!in)
        throw std::runtime_error("Invalid frozen vector");
    return value;
}
inline qualpf::gui::SessionStateView load(std::istream &in, qualpf::exporting::PatternFormat &format) {
    using namespace qualpf;
    if (scalar<std::uint32_t>(in) != 0x24312026)
        throw std::runtime_error("Wrong frozen fixture magic");
    format = static_cast<qualpf::exporting::PatternFormat>(scalar<std::int32_t>(in));
    gui::SessionStateView state;
    state.projectMetadata["radtype"] = scalar<std::int32_t>(in);
    state.editing = scalar<std::int32_t>(in) != 0;
    auto &p = state.pattern;
    for (auto x : vector(in))
        p.two_theta.emplace_back(x);
    p.observed = vector(in);
    if (scalar<std::int32_t>(in)) {
        const auto first = scalar<std::int64_t>(in);
        p.background.emplace();
        if (first >= 0)
            p.background->window = SampleWindow{static_cast<std::size_t>(first)};
        p.background->values = vector(in);
    }
    if (scalar<std::int32_t>(in)) {
        const auto lo = scalar<double>(in), hi = scalar<double>(in);
        p.calculation_range = AngularRange{TwoThetaDegrees{lo}, TwoThetaDegrees{hi}};
    }
    const auto lines = scalar<std::uint32_t>(in);
    for (std::uint32_t i = 0; i < lines; ++i) {
        const auto wave = scalar<double>(in), weight = scalar<double>(in);
        p.radiation.lines.push_back({WavelengthAngstrom{wave}, weight});
    }
    const auto method = scalar<std::int32_t>(in);
    const auto points = scalar<std::uint32_t>(in);
    state.preprocessing.background = preprocessing::BackgroundOptions{};
    state.preprocessing.background->method = method == 0 ? preprocessing::BackgroundMethod::none
                                                       : preprocessing::BackgroundMethod::chebyshev;
    // Frozen transport records point count, which alone enters the approved
    // schema predicate. These test values are never background estimation inputs.
    if (points)
        state.preprocessing.background_points = std::vector<preprocessing::BackgroundPoint>(points);
    if (in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Trailing fixture bytes");
    return state;
}
inline void dump(std::ostream &out, const std::vector<double> &values) {
    const auto count = static_cast<std::uint32_t>(values.size());
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    out.write(reinterpret_cast<const char *>(values.data()), values.size() * sizeof(double));
}
} // namespace phase24b
