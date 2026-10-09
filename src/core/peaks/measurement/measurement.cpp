#include "qualpf/peaks/measurement.h"
#include "../../preprocessing/legacy_internal.h"
#include "../legacy_polynomial.h"
#include <cfenv>

namespace qualpf::peaks {
namespace {
namespace old = preprocessing::detail;
using old::Vector;
Error invalid(const char *field, const char *reason) {
  return {ErrorCode::invalid_input, field,
          "Invalid inferred peak measurement input", reason};
}
Error undefined(const char *field, const char *reason) {
  return {ErrorCode::undefined_preprocessing, field,
          "Undefined native inferred measurement", reason};
}
bool finite32(double v) {
  return std::isfinite(v) && std::abs(v) <= std::numeric_limits<float>::max();
}
Status rounding() {
  if (std::fegetround() != FE_TONEAREST)
    return Status{invalid("measurement.rounding", "round_to_nearest_required")};
  return Status{std::monostate{}};
}
bool coordinate(double v) {
  return finite32(v) && float(v) > 0 && float(v) <= 180;
}
struct Input {
  Vector x, y, signal;
};
Result<Input> prepare(const Pattern &p, WavelengthAngstrom wave) {
  if (auto status = rounding(); !status)
    return Result<Input>{status.error()};
  if (!finite32(wave.value) || !(float(wave.value) > 0))
    return Result<Input>{
        invalid("measurement.wavelength", "invalid_wavelength")};
  auto x = old::grid(p);
  if (!x)
    return Result<Input>{x.error()};
  if (x.value().size() < 3 ||
      x.value().size() > std::size_t(std::numeric_limits<int>::max() - 4))
    return Result<Input>{invalid("measurement.pattern", "unsafe_sample_count")};
  auto y = old::narrow(p.observed);
  if (!y)
    return Result<Input>{y.error()};
  if (!p.background)
    return Result<Input>{
        Error{ErrorCode::missing_data, "measurement.background",
              "Supply the resolved native background explicitly",
              "background_unavailable"}};
  if (!p.background->window || p.background->window->first != 0 ||
      p.background->values.size() != p.observed.size())
    return Result<Input>{
        invalid("measurement.background", "dimension_or_alignment_mismatch")};
  auto b = old::narrow(p.background->values);
  if (!b)
    return Result<Input>{b.error()};
  Vector signal;
  for (std::size_t i = 0; i < y.value().size(); ++i) {
    const float v = y.value()[i] - b.value()[i];
    if (!std::isfinite(v))
      return Result<Input>{
          undefined("measurement.intensity", "nonfinite_subtraction")};
    signal.push_back(v);
  }
  return Result<Input>{
      Input{std::move(x.value()), std::move(y.value()), std::move(signal)}};
}
Status neighbors_valid(std::span<const NeighborPeak> peaks) {
  for (std::size_t i = 0; i < peaks.size(); ++i) {
    if (!coordinate(peaks[i].two_theta.value) ||
        (i &&
         float(peaks[i - 1].two_theta.value) > float(peaks[i].two_theta.value)))
      return Status{
          invalid("measurement.neighbors", "unordered_or_invalid_coordinates")};
    if (!finite32(peaks[i].fwhm.value) || !(float(peaks[i].fwhm.value) > 0))
      return Status{invalid("measurement.neighbors.fwhm", "invalid_fwhm")};
  }
  return Status{std::monostate{}};
}
// clocate's exact equal-coordinate behavior matters for native table lists.
std::size_t nearest_neighbor(std::span<const NeighborPeak> p, float q) {
  const auto x = [&](std::size_t i) { return float(p[i].two_theta.value); };
  if (q <= x(0))
    return 0;
  if (q >= x(p.size() - 1))
    return p.size() - 1;
  std::size_t lo = 0, hi = p.size() - 1;
  while (hi - lo > 1) {
    auto mid = (lo + hi) / 2;
    if (q >= x(mid))
      lo = mid;
    else
      hi = mid;
  }
  return q - x(lo) < x(hi) - q ? lo : hi;
}
Result<float> neighbor_width(float q, std::span<const NeighborPeak> p) {
  if (p.empty())
    return Result<float>{.2f};
  const auto w = [&](std::size_t i) { return float(p[i].fwhm.value); };
  if (p.size() == 1)
    return Result<float>{w(0)};
  auto at = nearest_neighbor(p, q);
  const bool left = q < float(p[at].two_theta.value);
  float width;
  if ((at == 0 && left) || (at == p.size() - 1 && !left))
    width = w(at);
  else
    width = (w(at) + w(left ? at - 1 : at + 1)) / 2.f;
  if (!std::isfinite(width) || width <= 0)
    return Result<float>{
        undefined("measurement.fwhm", "nonfinite_neighbor_width")};
  return Result<float>{width};
}
bool duplicate(std::span<const NeighborPeak> p, float x) {
  return std::any_of(p.begin(), p.end(), [&](const auto &a) {
    return std::abs(float(a.two_theta.value) - x) <=
           std::numeric_limits<float>::epsilon();
  });
}
Result<float> graph_position(const Input &in, float q,
                             std::span<const NeighborPeak> p) {
  int at = int(old::nearest(in.x, q));
  if (p.empty())
    return Result<float>{q};
  if (duplicate(p, in.x[at]))
    return Result<float>{
        invalid("measurement.coordinate", "duplicate_position")};
  int first = std::max(0, at - 3),
      last = std::min(int(in.x.size()) - 1, at + 3);
  auto scan = [&](int step, int end) {
    // Keep the native 1-based candidate/sentinel arithmetic explicitly.
    int found = 0;
    float high = in.y[at];
    for (int i = at + step; step > 0 ? i <= end : i >= end; i += step) {
      if (in.y[i] > high) {
        high = in.y[i];
        found = i + 1;
      } else if (in.y[i] < high && found > 0)
        break;
    }
    if (found == end + 1)
      found = 0;
    if (found > 0 && duplicate(p, in.x[found - 1]))
      found = -1;
    return found;
  };
  int right = scan(1, last), left = scan(-1, first), chosen = at;
  if (left + right == 0 || left + right == -1)
    chosen = at;
  else if (left > 0 && right <= 0)
    chosen = left - 1;
  else if (right > 0 && left <= 0)
    chosen = right - 1;
  else if (left > 0 && right > 0)
    chosen = right - (at + 1) > (at + 1) - left ? left - 1 : right - 1;
  else
    return Result<float>{
        invalid("measurement.coordinate", "duplicate_local_maxima")};
  return Result<float>{in.x[chosen]};
}
// Native indexx_sp is stable only for partitions shorter than sixteen entries.
// Preserve its row permutation: equal angles may carry different original d.
Result<std::vector<std::size_t>>
import_order(const std::vector<ImportedCoordinate> &entries) {
  using Index = std::ptrdiff_t;
  std::vector<std::size_t> order(entries.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  if (order.empty())
    return Result<std::vector<std::size_t>>{std::move(order)};
  auto key = [&](Index i) {
    return entries[order[std::size_t(i)]].two_theta.value;
  };
  auto exchange = [&](Index a, Index b) {
    if (key(b) < key(a))
      std::swap(order[std::size_t(a)], order[std::size_t(b)]);
  };
  std::vector<std::pair<Index, Index>> stack;
  Index left = 0, right = Index(order.size()) - 1;
  for (;;) {
    if (right - left < 15) {
      for (Index j = left + 1; j <= right; ++j) {
        const auto held = order[std::size_t(j)];
        const double value = entries[held].two_theta.value;
        Index i = j - 1;
        while (i >= left && key(i) > value) {
          order[std::size_t(i + 1)] = order[std::size_t(i)];
          --i;
        }
        order[std::size_t(i + 1)] = held;
      }
      if (stack.empty())
        break;
      const auto pending = stack.back();
      stack.pop_back();
      left = pending.first;
      right = pending.second;
    } else {
      std::swap(order[std::size_t((left + right) / 2)],
                order[std::size_t(left + 1)]);
      exchange(left, right);
      exchange(left + 1, right);
      exchange(left, left + 1);
      Index i = left + 1, j = right;
      const auto held = order[std::size_t(left + 1)];
      const double pivot = entries[held].two_theta.value;
      for (;;) {
        do {
          ++i;
        } while (key(i) < pivot);
        do {
          --j;
        } while (key(j) > pivot);
        if (j < i)
          break;
        std::swap(order[std::size_t(i)], order[std::size_t(j)]);
      }
      order[std::size_t(left + 1)] = order[std::size_t(j)];
      order[std::size_t(j)] = held;
      if (stack.size() == 25)
        return Result<std::vector<std::size_t>>{
            undefined("measurement.sort", "native_sort_stack_limit")};
      if (right - i + 1 >= j - left) {
        stack.emplace_back(i, right);
        right = j - 1;
      } else {
        stack.emplace_back(left, j - 1);
        left = i;
      }
    }
  }
  return Result<std::vector<std::size_t>>{std::move(order)};
}
Result<std::optional<float>> fitted_width(const Input &in,
                                          MeasurementTrace &t) {
  const auto first = t.sample > 4 ? t.sample - 4 : 0;
  const auto last = std::min(in.x.size() - 1, t.sample + 4);
  t.fit_first = first;
  t.fit_last = last;
  const auto count = last - first + 1;
  if (count < 3)
    return Result<std::optional<float>>{
        undefined("measurement.fit", "insufficient_fit_samples")};
  float mean = old::sum(std::span(in.x).subspan(first, count)) / float(count);
  float squares = 0, deviations = 0;
  for (std::size_t i = first; i <= last; ++i) {
    float d = in.x[i] - mean;
    squares += d * d;
    deviations += d;
  }
  const float variance =
      (squares - deviations * deviations / float(count)) / float(count - 1);
  if (!(variance > 0) || !std::isfinite(variance))
    return Result<std::optional<float>>{
        undefined("measurement.fit", "degenerate_fit_scale")};
  const float sd = std::sqrt(variance);
  t.mean = mean;
  t.sample_sd = sd;
  std::vector<std::array<float, 3>> matrix;
  Vector rhs;
  for (std::size_t i = first; i <= last; ++i) {
    if (!(in.y[i] > 0))
      return Result<std::optional<float>>{
          undefined("measurement.fit", "nonpositive_log_input")};
    const float x = (in.x[i] - mean) / sd;
    matrix.push_back({1.f, x, x * x});
    rhs.push_back(std::log(in.y[i]));
    t.fit_inputs.push_back({in.x[i], in.y[i], rhs.back(), x, x * x});
  }
  auto c = old::fit_log_quadratic(std::move(matrix), rhs);
  if (!std::all_of(c.begin(), c.end(),
                   [](float v) { return std::isfinite(v); }) ||
      c[2] == 0)
    return Result<std::optional<float>>{
        undefined("measurement.fit", "singular_or_nonfinite_fit")};
  t.coefficients = {c[0], c[1], c[2]};
  const float center = -(sd * c[1] / (2.f * c[2]) - mean);
  const float ratio = c[1] / (2.f * c[2]);
  const float height = std::exp(c[0] - c[2] * (ratio * ratio));
  if (!std::isfinite(center) || !std::isfinite(height))
    return Result<std::optional<float>>{
        undefined("measurement.fit", "nonfinite_fitted_position_or_height")};
  t.fitted_center = center;
  t.fitted_height = height;
  if (c[2] > 0) {
    t.used_fallback = true;
    return Result<std::optional<float>>{std::optional<float>{}};
  }
  const float width = sd * 2.35482f / std::sqrt(-2.f * c[2]);
  if (!std::isfinite(width) || width <= 0)
    return Result<std::optional<float>>{
        undefined("measurement.fwhm", "invalid_fitted_width")};
  t.fitted_width = width;
  return Result<std::optional<float>>{std::optional<float>{width}};
}
Result<MeasuredPeak> measure(const Input &in, float input, float position,
                             std::span<const NeighborPeak> neighbors,
                             bool graph, WavelengthAngstrom wave,
                             std::optional<DSpacingAngstrom> original_d = {}) {
  MeasurementTrace t;
  t.input_coordinate = input;
  t.initial_sample = old::nearest(in.x, input);
  t.sample = old::nearest(in.x, position);
  auto fit = fitted_width(in, t);
  if (!fit)
    return Result<MeasuredPeak>{fit.error()};
  float width = .2f;
  if (fit.value())
    width = *fit.value();
  else if (graph) {
    auto w = neighbor_width(position, neighbors);
    if (!w)
      return Result<MeasuredPeak>{w.error()};
    width = w.value();
  }
  const float lo = position - 2.f * width, hi = position + 2.f * width;
  if (!std::isfinite(lo) || !std::isfinite(hi))
    return Result<MeasuredPeak>{
        undefined("measurement.area", "unrepresentable_integration_window")};
  t.integration_first = old::nearest(in.x, lo);
  t.integration_last = old::nearest(in.x, hi);
  float area = 0;
  if (t.integration_first == t.integration_last)
    area = in.signal[t.integration_first];
  else {
    for (auto i = t.integration_first; i < t.integration_last; ++i) {
      const float term =
          (in.x[i + 1] - in.x[i]) * (in.signal[i + 1] + in.signal[i]);
      area += term;
      t.integration_steps.push_back({term, area});
    }
    area = .5f * area;
  }
  if (!std::isfinite(area))
    return Result<MeasuredPeak>{
        undefined("measurement.area", "nonfinite_integral")};
  auto d = preprocessing::legacy_d(TwoThetaDegrees{double(position)}, wave);
  if (!d)
    return Result<MeasuredPeak>{d.error()};
  if (original_d)
    d = Result<DSpacingAngstrom>{
        DSpacingAngstrom{double(float(original_d->value))}};
  const double intensity = in.signal[t.sample];
  return Result<MeasuredPeak>{
      MeasuredPeak{{TwoThetaDegrees{position}, d.value(), intensity, intensity,
                    AngularWidthDegrees{width}, double(area)},
                   t}};
}
} // namespace

Result<AngularWidthDegrees>
infer_table_width(TwoThetaDegrees q, std::span<const NeighborPeak> neighbors) {
  if (auto s = rounding(); !s)
    return Result<AngularWidthDegrees>{s.error()};
  if (!coordinate(q.value))
    return Result<AngularWidthDegrees>{
        invalid("measurement.coordinate", "invalid_coordinate")};
  if (auto s = neighbors_valid(neighbors); !s)
    return Result<AngularWidthDegrees>{s.error()};
  auto w = neighbor_width(float(q.value), neighbors);
  if (!w)
    return Result<AngularWidthDegrees>{w.error()};
  return Result<AngularWidthDegrees>{AngularWidthDegrees{w.value()}};
}
Result<MeasuredPeak> measure_graph_peak(const Pattern &p, TwoThetaDegrees q,
                                        std::span<const NeighborPeak> neighbors,
                                        WavelengthAngstrom wave) {
  if (auto s = rounding(); !s)
    return Result<MeasuredPeak>{s.error()};
  if (!coordinate(q.value))
    return Result<MeasuredPeak>{
        invalid("measurement.coordinate", "invalid_coordinate")};
  if (auto s = neighbors_valid(neighbors); !s)
    return Result<MeasuredPeak>{s.error()};
  auto in = prepare(p, wave);
  if (!in)
    return Result<MeasuredPeak>{in.error()};
  auto position = graph_position(in.value(), float(q.value), neighbors);
  if (!position)
    return Result<MeasuredPeak>{position.error()};
  return measure(in.value(), float(q.value), position.value(), neighbors, true,
                 wave);
}
Result<std::vector<MeasuredPeak>>
measure_imported_peaks(const Pattern &p,
                       std::span<const ImportedCoordinate> coordinates,
                       WavelengthAngstrom wave) {
  using Output = std::vector<MeasuredPeak>;
  auto in = prepare(p, wave);
  if (!in)
    return Result<Output>{in.error()};
  double minimum = p.calculation_range ? p.calculation_range->minimum.value
                                       : in.value().x.front();
  double maximum = p.calculation_range ? p.calculation_range->maximum.value
                                       : in.value().x.back();
  if (!finite32(minimum) || !finite32(maximum) ||
      !(float(minimum) < float(maximum)))
    return Result<Output>{invalid("measurement.range", "invalid_range")};
  std::vector<ImportedCoordinate> accepted;
  for (auto c : coordinates) {
    if (!finite32(c.two_theta.value))
      return Result<Output>{
          invalid("measurement.coordinate", "invalid_coordinate")};
    c.two_theta.value = float(c.two_theta.value);
    if (!(c.two_theta.value > float(minimum) &&
          c.two_theta.value < float(maximum)))
      continue;
    if (!coordinate(c.two_theta.value))
      return Result<Output>{
          invalid("measurement.coordinate", "invalid_coordinate")};
    if (c.original_d &&
        (!finite32(c.original_d->value) || !(float(c.original_d->value) > 0)))
      return Result<Output>{invalid("measurement.d", "invalid_d_spacing")};
    accepted.push_back(c);
  }
  auto order = import_order(accepted);
  if (!order)
    return Result<Output>{order.error()};
  auto unsorted = std::move(accepted);
  accepted.clear();
  accepted.reserve(unsorted.size());
  for (auto index : order.value())
    accepted.push_back(unsorted[index]);
  Output out;
  for (std::size_t i = 0; i < accepted.size(); ++i) {
    // Compare each original adjacent input, including a previously removed
    // one, as native peak_filterd does before deleting the marked rows.
    if (i && std::abs(float(accepted[i].two_theta.value) -
                      float(accepted[i - 1].two_theta.value)) <=
                 std::numeric_limits<float>::epsilon())
      continue;
    const auto &c = accepted[i];
    auto m = measure(in.value(), float(c.two_theta.value),
                     float(c.two_theta.value), {}, false, wave, c.original_d);
    if (!m)
      return Result<Output>{m.error()};
    out.push_back(std::move(m.value()));
  }
  if (out.empty())
    return Result<Output>{Error{
        ErrorCode::missing_data, "measurement.coordinates",
        "No coordinates strictly inside range", "no_accepted_coordinates"}};
  return Result<Output>{std::move(out)};
}
} // namespace qualpf::peaks
