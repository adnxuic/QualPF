#include "qualpf/profile/pfsm_integral_screen.h"
#include "pfsm_internal.h"
#include <numeric>

namespace qualpf::profile {
namespace {
using namespace detail;
template<class T> T take(Result<T> r) { if (!r) throw Failure{r.error()}; return std::move(r).value(); }
double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : std::midpoint(v[v.size() / 2 - 1], v[v.size() / 2]);
}
void unique(std::vector<double> &v) { std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()), v.end()); }
std::optional<std::size_t> regionAt(const PfsmIntegralData &d, double x) {
    const auto it = std::lower_bound(d.regions.begin(), d.regions.end(), x,
        [](const auto &r, double t) { return r.second < t; });
    if (it == d.regions.end() || x < it->first) return {};
    return std::size_t(it - d.regions.begin());
}
const std::vector<double> &values(const PfsmIntegralColumns &c, bool fine) { return fine ? c.fine : c.coarse; }
struct Solver {
    const PfsmIntegralLayer &layer;
    Matrix a;
    Vector z;
    NnlsWorkspace workspace;
    std::vector<std::vector<double>> columns;
    std::size_t iterations = 0;
    Solver(const PfsmIntegralRound &round, bool fine, bool candidate)
        : layer(fine ? round.data->fine : round.data->coarse),
          a(Eigen::Index(layer.net.size()), Eigen::Index(round.selected_columns.size() + (candidate ? 1 : 0))),
          z(Eigen::Index(layer.net.size())) {
        for (const auto &c : round.selected_columns) columns.push_back(values(c, fine));
        if (candidate) columns.emplace_back(layer.net.size(), 0);
        for (std::size_t i = 0; i < layer.net.size(); ++i)
            z[Eigen::Index(i)] = std::sqrt(layer.weights[i]) * layer.net[i] / std::sqrt(layer.denominator);
        for (std::size_t j = 0; j < columns.size(); ++j) update(j);
        workspace.resize(a.rows(), a.cols());
    }
    void update(std::size_t j) {
        for (std::size_t i = 0; i < layer.net.size(); ++i)
            a(Eigen::Index(i), Eigen::Index(j)) = columns[j][i] * std::sqrt(layer.weights[i]) / std::sqrt(layer.denominator);
        if (!a.col(Eigen::Index(j)).allFinite()) fail("pfsm.integral.nnls", "Weighted integral column overflow", ErrorCode::numerical_failure);
    }
    double solve(const ProfileFitOptions &options, const ExecutionContext &context) {
        const auto result = nnls(a, z, options.maximum_nnls_iterations, workspace, context);
        iterations += result.iterations;
        double q = 0;
        for (std::size_t i = 0; i < layer.net.size(); ++i) {
            double calculated = 0;
            for (std::size_t j = 0; j < columns.size(); ++j) calculated += result.scales[Eigen::Index(j)] * columns[j][i];
            const double r = std::sqrt(layer.weights[i]) * (layer.net[i] - calculated);
            q += r * r;
        }
        if (!std::isfinite(q)) fail("pfsm.integral.nnls", "Integral objective overflow", ErrorCode::numerical_failure);
        return q;
    }
    double candidate(std::span<const double> p, const ProfileFitOptions &o, const ExecutionContext &c) {
        columns.back().assign(p.begin(), p.end()); update(columns.size() - 1); workspace.valid.back() = false;
        return solve(o, c);
    }
    std::size_t bytes() const {
        // Owned visible buffers plus conservative bounds for Eigen QR/SVD
        // internal work arrays (not a process-resident-memory measurement).
        const auto n = std::size_t(a.rows()), m = std::size_t(a.cols());
        return sizeof(double) * (10 * n * m + 12 * m * m + 12 * m + 4 * n) +
               m * (sizeof(Eigen::Index) + 2 * sizeof(bool));
    }
};
std::vector<ProfileLine> majorGroups(std::vector<ProfileLine> lines, const PfsmIntegralData &d, double lo, double hi) {
    std::stable_sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) { return a.position < b.position; });
    std::vector<ProfileLine> groups;
    for (std::size_t i = 0; i < lines.size();) {
        const double first = lines[i].position; const auto id = lines[i].identity;
        double strength = 0, moment = 0;
        do { strength += lines[i].intensity; moment += lines[i].intensity * (lines[i].position - first); ++i; }
        while (i < lines.size() && lines[i].position - first <= d.width / 2);
        if (!(strength > 0)) continue;
        const double center = first + moment / strength;
        if (!std::isfinite(strength) || !std::isfinite(center)) fail("pfsm.integral.groups", "Group overflow", ErrorCode::numerical_failure);
        if (std::any_of(d.regions.begin(), d.regions.end(), [&](const auto &r) { return center + lo <= r.second && center + hi >= r.first; }))
            groups.push_back({id, center, strength});
    }
    std::stable_sort(groups.begin(), groups.end(), [](const auto &a, const auto &b) { return a.intensity > b.intensity; });
    if (groups.size() > 24) groups.resize(24);
    return groups;
}
void support(const PfsmIntegralRound &r, std::span<const ProfileLine> lines, double shift,
    bool residual, PfsmSupportEvidence &best, const ExecutionContext &context) {
    if (lines.empty() || !(r.tau > 0)) return;
    const auto &positions = r.support_positions;
    const auto &response = residual ? r.residual_response : r.original_response;
    double total = 0, sum = 0, weight = 0, visible = 0, edge = 0;
    std::size_t count = 0;
    for (const auto &line : lines) {
        checkpoint(context);
        total += line.intensity / lines.front().intensity;
        const double t = line.position + shift;
        const auto region = regionAt(*r.data, t);
        if (!region || t < positions.front() || t > positions.back()) continue;
        const auto it = std::lower_bound(positions.begin(), positions.end(), t);
        const auto j = std::size_t(it - positions.begin());
        double numerator = response[j], norm = r.norm_response[j];
        if (*it != t) {
            if (!j || regionAt(*r.data, positions[j - 1]) != region || regionAt(*r.data, positions[j]) != region) continue;
            const double f = (t - positions[j - 1]) / (positions[j] - positions[j - 1]);
            numerator = std::lerp(response[j - 1], response[j], f); norm = std::lerp(r.norm_response[j - 1], r.norm_response[j], f);
        }
        if (!(norm > 0) || !std::isfinite(norm)) continue;
        const double z = numerator / std::sqrt(norm);
        if (!std::isfinite(z)) continue;
        const double positive = std::max(0., z);
        const double e = positive > r.tau ? 1 / (1 + r.tau / positive) : positive / (positive + r.tau);
        const double a = std::sqrt(line.intensity / lines.front().intensity);
        sum += a * e; weight += a; visible += line.intensity / lines.front().intensity; ++count;
        const auto &reg = r.data->regions[*region];
        if (t - reg.first <= r.data->width / 2 || reg.second - t <= r.data->width / 2) edge += a * e;
    }
    if (weight > 0 && (!best.score || sum / weight > *best.score || (sum / weight == *best.score && shift < best.shift))) {
        best.score = sum / weight; best.shift = shift; best.visible_groups = count;
        best.visible_fraction = visible / total; best.edge_fraction = sum > 0 ? edge / sum : 0;
        best.uncertain = count < 3 || best.visible_fraction < .5 || best.edge_fraction > .5;
    }
}
std::vector<double> refine(std::span<const double> shifts, std::span<const double> gains,
    const PfsmScreenEvidence &e) {
    struct Neighborhood { double lo, hi; std::vector<double> anchors; };
    std::vector<Neighborhood> selected;
    const auto interval = [&](std::size_t i) { return Neighborhood{shifts[i ? i - 1 : 0], shifts[std::min(i + 1, shifts.size() - 1)], {shifts[i]}}; };
    const auto [minimum, maximum] = std::minmax_element(gains.begin(), gains.end());
    if (*maximum - *minimum <= 128 * std::numeric_limits<double>::epsilon()) {
        for (double t : {shifts.front(), std::clamp(0., shifts.front(), shifts.back()), shifts.back()})
            selected.push_back(interval(std::size_t(std::lower_bound(shifts.begin(), shifts.end(), t) - shifts.begin())));
    } else {
        std::vector<std::size_t> order(shifts.size()); std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return gains[a] > gains[b]; });
        selected.push_back(interval(order.front()));
        for (auto i : order) {
            auto n = interval(i);
            if (n.hi <= selected.front().lo || n.lo >= selected.front().hi) { selected.push_back(std::move(n)); break; }
        }
        for (const auto *s : {&e.original, &e.residual}) if (s->score)
            selected.push_back(interval(std::size_t(std::lower_bound(shifts.begin(), shifts.end(), s->shift) - shifts.begin())));
    }
    std::sort(selected.begin(), selected.end(), [](const auto &a, const auto &b) { return a.lo < b.lo; });
    std::vector<Neighborhood> merged;
    for (auto n : selected) {
        if (merged.empty() || n.lo > merged.back().hi) merged.push_back(std::move(n));
        else { merged.back().hi = std::max(merged.back().hi, n.hi); merged.back().anchors.insert(merged.back().anchors.end(), n.anchors.begin(), n.anchors.end()); }
    }
    std::vector<double> out;
    for (const auto &n : merged) {
        auto points = n.anchors; points.push_back(n.lo); points.push_back(n.hi); unique(points);
        const auto missing = 8 - points.size();
        for (std::size_t k = 1; k <= missing; ++k) points.push_back(std::lerp(n.lo, n.hi, double(k) / double(missing + 1)));
        unique(points); out.insert(out.end(), points.begin(), points.end());
    }
    unique(out); return out;
}
}
Result<PfsmIntegralRound> preparePfsmIntegralRound(std::shared_ptr<const PfsmIntegralData> data,
    std::span<const ProfileExpansionInput> selected, std::span<const double> shifts,
    std::span<const double> residual, const ProfileFitOptions &options, const ExecutionContext &context) try {
    checkpoint(context);
    if (!data || !data->original || selected.size() != shifts.size() || residual.size() != data->original->coordinates.size())
        fail("pfsm.integral.round", "Misaligned integral round");
    PfsmIntegralRound round; round.data = std::move(data); round.initial = selected.empty();
    const auto &d = *round.data;
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (selected[i].fwhm_degrees != d.width) fail("pfsm.integral.round", "Selected width mismatch");
        const auto lines = take(expandProfileLines(selected[i], context));
        auto columns = take(integratePfsmScreenTemplate(d, lines, shifts[i], context));
        if (std::none_of(columns.fine.begin(), columns.fine.end(), [](double p) { return p > 0; }))
            fail("pfsm.integral.baseline", "Selected phase has no integral evidence", ErrorCode::numerical_failure);
        round.selected_columns.push_back(std::move(columns));
    }
    {
        Solver coarse(round, false, false), fine(round, true, false);
        round.coarse_baseline_q = coarse.solve(options, context); round.fine_baseline_q = fine.solve(options, context);
        round.nnls_iterations = coarse.iterations + fine.iterations;
        round.buffer_bytes = coarse.bytes() + fine.bytes();
    }
    std::vector<double> aggregated(d.fine.net.size());
    for (double r : residual) if (!std::isfinite(r)) fail("pfsm.integral.residual", "Nonfinite residual");
    for (const auto &cell : d.overlaps) aggregated[cell.bin] += cell.density * (cell.upper - cell.lower) * residual[cell.sample];
    for (std::size_t j = 0; j < aggregated.size(); ++j) if (d.fine.weights[j] > 0) aggregated[j] /= d.fine.weights[j];
    for (std::size_t k = 0; k < d.fine.net.size(); ++k) {
        checkpoint(context);
        const double t = std::midpoint(d.edges[k], d.edges[k + 1]);
        const auto p = take(integratePfsmScreenTemplate(d, std::vector<ProfileLine>{{0, t, 1}}, 0, context));
        double y = 0, r = 0, norm = 0;
        for (std::size_t j = 0; j < p.fine.size(); ++j) {
            y += d.fine.weights[j] * d.fine.net[j] * p.fine[j];
            r += d.fine.weights[j] * aggregated[j] * p.fine[j];
            norm += d.fine.weights[j] * p.fine[j] * p.fine[j];
        }
        round.support_positions.push_back(t); round.original_response.push_back(y);
        round.residual_response.push_back(round.initial ? y : r); round.norm_response.push_back(norm);
    }
    std::vector<double> z;
    for (std::size_t j = 0; j < round.support_positions.size(); ++j) {
        if (regionAt(d, round.support_positions[j]) && round.norm_response[j] > 0) {
            const double v = round.original_response[j] / std::sqrt(round.norm_response[j]);
            if (std::isfinite(v)) z.push_back(v);
        }
    }
    if (!z.empty()) {
        const double center = median(z); auto deviations = z;
        for (auto &v : deviations) v = std::abs(v - center);
        round.tau = 1.4826 * median(std::move(deviations));
        if (round.tau == 0) round.tau = vectorOf(z).stableNorm() / std::sqrt(double(z.size()));
        if (!std::isfinite(round.tau)) round.tau = 0; // Support unavailable; main scores remain valid.
    }
    std::size_t retained = (round.support_positions.capacity() + round.original_response.capacity() +
        round.residual_response.capacity() + round.norm_response.capacity()) * sizeof(double);
    for (const auto &p : round.selected_columns) retained += (p.fine.capacity() + p.coarse.capacity()) * sizeof(double);
    round.buffer_bytes += retained + 4 * d.fine.net.size() * sizeof(double);
    return Result<PfsmIntegralRound>{std::move(round)};
} catch (const Failure &e) { return Result<PfsmIntegralRound>{e.error}; }

Result<PfsmScreenEvidence> evaluatePfsmIntegralCandidate(const PfsmIntegralRound &round,
    const ProfileExpansionInput &input, double lo, double hi, const ProfileFitOptions &options,
    const ExecutionContext &context) try {
    checkpoint(context);
    if (!round.data || input.fwhm_degrees != round.data->width || !std::isfinite(lo) || !std::isfinite(hi) || lo > hi)
        fail("pfsm.integral.candidate", "Invalid candidate input");
    const auto lines = take(expandProfileLines(input, context)); // Invalid reference records still fail the operation.
    PfsmScreenEvidence e;
    e.coarse_shift = e.fine_shift = std::clamp(0., lo, hi);
    try {
        const auto &d = *round.data;
        auto major = majorGroups(lines, d, lo, hi);
        std::vector<double> shifts;
        for (std::size_t k = 0; k < 65; ++k) shifts.push_back(std::lerp(lo, hi, double(k) / 64));
        if (lo <= 0 && hi >= 0) shifts.push_back(0); unique(shifts);
        Solver coarse(round, false, true), fine(round, true, true);
        e.buffer_bytes = coarse.bytes() + fine.bytes() + (d.fine.net.size() + d.coarse.net.size()) * sizeof(double) +
            (3 * lines.capacity() + major.capacity()) * sizeof(ProfileLine) + 256 * sizeof(double);
        std::vector<double> gains;
        double bestCoarse = round.coarse_baseline_q, bestFine = round.fine_baseline_q;
        bool visible = false;
        const auto evaluate = [&](double shift, bool detailed) {
            checkpoint(context);
            ++e.scan_evaluations;
            if (detailed) ++e.fine_evaluations; else ++e.coarse_evaluations;
            const auto p = take(integratePfsmScreenTemplate(d, lines, shift, context));
            visible = visible || std::any_of(p.fine.begin(), p.fine.end(), [](double v) { return v > 0; });
            auto &solver = detailed ? fine : coarse;
            const double q = solver.candidate(values(p, detailed), options, context);
            e.nnls_iterations = coarse.iterations + fine.iterations;
            const double base = detailed ? round.fine_baseline_q : round.coarse_baseline_q;
            if (q > base + 128 * std::numeric_limits<double>::epsilon() * std::max(solver.layer.denominator, base))
                fail("pfsm.integral.gain", "Negative integral improvement", ErrorCode::numerical_failure);
            support(round, major, shift, false, e.original, context);
            if (round.initial) e.residual = e.original; else support(round, major, shift, true, e.residual, context);
            if (detailed) { if (q < bestFine) { bestFine = q; e.fine_shift = shift; } }
            else { if (q < bestCoarse) { bestCoarse = q; e.coarse_shift = shift; } gains.push_back((base - std::min(base, q)) / solver.layer.denominator); }
        };
        for (double shift : shifts) evaluate(shift, false);
        const auto fineShifts = refine(shifts, gains, e);
        if (fineShifts.size() > 32) fail("pfsm.integral.budget", "Integral refinement budget exceeded", ErrorCode::numerical_failure);
        for (double shift : fineShifts) evaluate(shift, true);
        e.gain = round.coarse_baseline_q - bestCoarse; e.normalized_gain = *e.gain / d.coarse.denominator;
        e.fine_gain = round.fine_baseline_q - bestFine; e.normalized_fine_gain = *e.fine_gain / d.fine.denominator;
        e.nnls_iterations = coarse.iterations + fine.iterations; e.integral_complete = true;
        if (!visible) e.uncertainty.push_back("integral_template_insufficient");
    } catch (const Failure &failure) {
        if (failure.error.code == ErrorCode::cancelled || failure.error.code == ErrorCode::invalid_input) throw;
        e.uncertainty.push_back("integral_numerical_failure");
    }
    return Result<PfsmScreenEvidence>{std::move(e)};
} catch (const Failure &e) { return Result<PfsmScreenEvidence>{e.error}; }

Result<PfsmScreenSelection> selectPfsmIntegralCandidates(std::span<PfsmScreenEvidence> rows,
    const PfsmIntegralOptions &options, const ExecutionContext &context) try {
    take(validatePfsmIntegralOptions(options)); checkpoint(context);
    for (auto &r : rows) {
        r.main_selected = r.coarse_selected = r.original_selected = r.residual_selected = false;
        r.retained = !r.uncertainty.empty();
        if (!r.integral_complete && r.uncertainty.empty()) fail("pfsm.integral.selection", "Unaccounted candidate evaluation");
    }
    const auto queue = [&](auto score, std::size_t budget, auto flag) {
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < rows.size(); ++i) if (const auto value = score(rows[i])) {
            if (!std::isfinite(*value) || *value < 0) fail("pfsm.integral.selection", "Invalid integral score");
            order.push_back(i);
        }
        if (order.empty()) return;
        std::sort(order.begin(), order.end(), [&](auto a, auto b) {
            const double x = *score(rows[a]), y = *score(rows[b]);
            return x != y ? x > y : rows[a].original_index < rows[b].original_index;
        });
        const double cutoff = *score(rows[order[std::min(budget, order.size()) - 1]]);
        for (auto i : order) if (*score(rows[i]) >= cutoff - 128 * std::numeric_limits<double>::epsilon()) {
            rows[i].*flag = true; rows[i].retained = true;
        }
    };
    queue([](const auto &r) { return r.normalized_fine_gain; }, options.fine_budget, &PfsmScreenEvidence::main_selected);
    queue([](const auto &r) { return r.normalized_gain; }, options.coarse_budget, &PfsmScreenEvidence::coarse_selected);
    queue([](const auto &r) { return r.original.score; }, options.original_budget, &PfsmScreenEvidence::original_selected);
    queue([](const auto &r) { return r.residual.score; }, options.residual_budget, &PfsmScreenEvidence::residual_selected);
    PfsmScreenSelection out;
    for (const auto &r : rows) if (r.retained) out.indices.push_back(r.original_index);
    std::sort(out.indices.begin(), out.indices.end());
    return Result<PfsmScreenSelection>{std::move(out)};
} catch (const Failure &e) { return Result<PfsmScreenSelection>{e.error}; }
} // namespace qualpf::profile
