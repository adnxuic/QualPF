#include "qualpf/profile/pfsm_candidate_recall.h"
#include "pfsm_internal.h"
#include <unsupported/Eigen/FFT>
#include <numeric>
#include <set>

namespace qualpf::profile {
using detail::Matrix;
using detail::Vector;
using Complex = std::complex<double>;
// Explicit backend: parent build flags cannot silently select FFTW or MKL.
using FastFft = Eigen::FFT<double, Eigen::internal::kissfft_impl<double>>;
struct PfsmCandidateRecallKernel {
    std::size_t nodes = 0, fft_size = 0;
    std::vector<Complex> profile, squared;
};
struct PfsmCandidateRecallLocalProblem {
    Matrix columns;
    Vector z, sqrt_weights;
    std::vector<double> coordinates;
};
namespace {
using detail::checkpoint;
[[noreturn]] void unavailable(const char *message) {
    throw detail::Failure{Error{ErrorCode::numerical_failure, "pfsm.fast", message}};
}
template <class T> T take(Result<T> r) {
    if (!r) throw detail::Failure{r.error()};
    return std::move(r).value();
}
bool tied(double a, double b) {
    return std::abs(a - b) <= 128 * std::numeric_limits<double>::epsilon() * std::max({1., std::abs(a), std::abs(b)});
}
double query(const PfsmCandidateRecallData &d, std::span<const double> v, double x) {
    const double t = (x - d.origin) / d.step;
    if (!std::isfinite(t) || t < 0 || t > double(v.size() - 1)) unavailable("Query outside fast grid");
    const auto j = std::min(static_cast<std::size_t>(t), v.size() - 2);
    const double a = t - double(j);
    const double result = (1 - a) * v[j] + a * v[j + 1];
    if (!std::isfinite(result)) unavailable("Nonfinite fast interpolation");
    return result;
}
void deposit(const PfsmCandidateRecallData &d, std::vector<double> &v, double x, double value) {
    const double t = (x - d.origin) / d.step;
    if (!std::isfinite(t) || t < 0 || t > double(v.size() - 1)) unavailable("Deposit outside fast grid");
    const auto j = std::min(static_cast<std::size_t>(t), v.size() - 2);
    const double a = t - double(j);
    v[j] += (1 - a) * value;
    v[j + 1] += a * value;
    if (!std::isfinite(v[j]) || !std::isfinite(v[j + 1])) unavailable("Fast grid overflow");
}
std::vector<double> convolve(const PfsmCandidateRecallKernel &k, std::span<const double> v, bool square,
                             const ExecutionContext &ctx) {
    checkpoint(ctx);
    FastFft fft;
    std::vector<double> padded(k.fft_size, 0), result;
    std::copy(v.begin(), v.end(), padded.begin());
    std::vector<Complex> spectrum;
    fft.fwd(spectrum, padded);
    const auto &kernel = square ? k.squared : k.profile;
    for (std::size_t i = 0; i < spectrum.size(); ++i) spectrum[i] *= kernel[i];
    checkpoint(ctx);
    fft.inv(result, spectrum);
    std::vector<double> out(k.nodes);
    for (std::size_t i = 0; i < k.nodes; ++i) {
        out[i] = result[i + k.nodes - 1];
        if (!std::isfinite(out[i])) unavailable("Nonfinite FFT convolution");
    }
    checkpoint(ctx);
    return out;
}
std::vector<ProfileLine> lines(const ProfileExpansionInput &input, const ExecutionContext &ctx) {
    auto out = take(expandProfileLines(input, ctx));
    std::stable_sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.position < b.position; });
    std::vector<ProfileLine> merged;
    for (const auto &line : out) {
        if (line.intensity == 0) continue;
        if (!merged.empty() && merged.back().position == line.position) {
            merged.back().intensity += line.intensity;
            if (!std::isfinite(merged.back().intensity)) unavailable("Merged line overflow");
        } else merged.push_back(line);
    }
    return merged;
}
std::vector<double> gridProfile(const PfsmCandidateRecallData &d, const ProfileExpansionInput &in,
                                 const ExecutionContext &ctx) {
    if (in.fwhm_degrees != d.width) unavailable("Fast profile width mismatch");
    std::vector<double> stick(d.weights.size(), 0);
    for (const auto &line : lines(in, ctx)) deposit(d, stick, line.position, line.intensity);
    return convolve(*d.kernel, stick, false, ctx);
}
Vector column(const PfsmCandidateRecallData &d, const PfsmCandidateRecallLocalProblem &local,
              std::span<const double> profile, double shift) {
    Vector out(local.z.size());
    for (Eigen::Index i = 0; i < out.size(); ++i)
        out[i] = local.sqrt_weights[i] * query(d, profile, local.coordinates[static_cast<std::size_t>(i)] - shift);
    if (!out.allFinite()) unavailable("Nonfinite local column");
    return out;
}
detail::LinearSolution solve(const Matrix &a, const Vector &z, const ExecutionContext &ctx) {
    detail::NnlsWorkspace workspace;
    workspace.resize(a.rows(), a.cols());
    return detail::nnls(a, z, 0, workspace, ctx);
}
template <class Value> std::vector<std::size_t> order(const std::vector<PfsmCandidateRecallEvidence> &e, Value value) {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < e.size(); ++i) if (value(e[i])) indices.push_back(i);
    std::stable_sort(indices.begin(), indices.end(), [&](auto a, auto b) {
        const double x = *value(e[a]), y = *value(e[b]);
        return x == y ? e[a].original_index < e[b].original_index : x > y;
    });
    return indices;
}
} // namespace

Result<std::shared_ptr<const PfsmCandidateRecallData>> preparePfsmCandidateRecallData(
    std::shared_ptr<const ProfileObservations> observations, double width, double lower, double upper,
    const ExecutionContext &ctx) try {
    checkpoint(ctx);
    if (ctx.numerical_policy != NumericalPolicy::explicit_float64 ||
        !observations || observations->coordinates.size() < 2 || !(width > 0) || !std::isfinite(width) ||
        !std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
        detail::fail("pfsm.fast", "Invalid fast observations, width or bounds");
    const auto &o = *observations;
    const auto n = o.coordinates.size();
    if (o.weights.size() != n || o.observed.size() != n || o.background.size() != n)
        detail::fail("pfsm.fast", "Unaligned fast observations");
    auto d = std::make_shared<PfsmCandidateRecallData>();
    d->observations = std::move(observations);
    d->width = width; d->step = width / 8; d->lower_shift = lower; d->upper_shift = upper;
    const auto [xmin, xmax] = std::minmax_element(o.coordinates.begin(), o.coordinates.end());
    const double margin = std::max(std::abs(lower), std::abs(upper));
    d->origin = std::min(0., *xmin) - margin - 2 * d->step;
    const double end = std::max(180., *xmax) + margin + 2 * d->step;
    const double count = std::ceil((end - d->origin) / d->step) + 1;
    if (!std::isfinite(count) || count < 2 || count > 131072) unavailable("Fast grid node limit");
    const auto m = static_cast<std::size_t>(count);
    d->weights.assign(m, 0); d->weighted_signal.assign(m, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if ((i & 255) == 0) checkpoint(ctx);
        if (!std::isfinite(o.coordinates[i]) || !std::isfinite(o.weights[i]) || o.weights[i] < 0 ||
            !std::isfinite(o.observed[i]) || !std::isfinite(o.background[i]))
            detail::fail("pfsm.fast", "Invalid fast sample");
        if (o.weights[i] == 0) continue;
        const double y = o.observed[i] - o.background[i];
        deposit(*d, d->weights, o.coordinates[i], o.weights[i]);
        deposit(*d, d->weighted_signal, o.coordinates[i], o.weights[i] * y);
        d->denominator += o.weights[i] * y * y;
    }
    if (!(d->denominator > 0) || !std::isfinite(d->denominator)) unavailable("Invalid fast normalization");
    const double intervals = (upper - lower) / (width / 4);
    if (!std::isfinite(intervals) || intervals > 4096) unavailable("Fast shift scan limit");
    d->scan = {lower, upper, std::clamp(0., lower, upper)};
    for (std::size_t j = 1; j <= static_cast<std::size_t>(intervals); ++j) {
        const double shift = lower + double(j) * (width / 4);
        if (shift < upper) d->scan.push_back(shift);
    }
    std::sort(d->scan.begin(), d->scan.end());
    d->scan.erase(std::unique(d->scan.begin(), d->scan.end()), d->scan.end());
    if (d->scan.size() > 4096) unavailable("Fast shift scan limit");
    auto k = std::make_shared<PfsmCandidateRecallKernel>();
    k->nodes = m; k->fft_size = 1;
    while (k->fft_size < 3 * m - 2) k->fft_size *= 2;
    std::vector<double> kernel(k->fft_size, 0), squared(k->fft_size, 0);
    const double alpha = 4 * (std::cbrt(4.) - 1);
    for (std::size_t j = 0; j < 2 * m - 1; ++j) {
        const double v = (double(j) - double(m - 1)) / 8;
        kernel[j] = std::pow(1 + alpha * v * v, -1.5);
        squared[j] = kernel[j] * kernel[j];
    }
    FastFft fft;
    fft.fwd(k->profile, kernel); fft.fwd(k->squared, squared);
    d->kernel = k;
    d->original_response = convolve(*k, d->weighted_signal, false, ctx);
    d->energy_response = convolve(*k, d->weights, true, ctx);
    return Result<std::shared_ptr<const PfsmCandidateRecallData>>{std::move(d)};
} catch (const detail::Failure &f) { return Result<std::shared_ptr<const PfsmCandidateRecallData>>{f.error}; }

Result<PfsmCandidateRecallRound> preparePfsmCandidateRecallRound(std::shared_ptr<const PfsmCandidateRecallData> data,
    std::span<const double> residual, std::span<const PfsmCandidateRecallSelected> selected, const ExecutionContext &ctx,
    bool prepareLocalDictionary) try {
    checkpoint(ctx);
    if (!data || !data->observations || !data->kernel || data->weights.size() < 2 ||
        residual.size() != data->observations->coordinates.size())
        detail::fail("pfsm.fast", "Invalid fast residual");
    const auto &d = *data;
    PfsmCandidateRecallRound out; out.data = std::move(data);
    std::vector<double> v(d.weights.size(), 0);
    for (std::size_t i = 0; i < residual.size(); ++i) {
        if (!std::isfinite(residual[i])) detail::fail("pfsm.fast", "Nonfinite fast residual");
        if (d.observations->weights[i] > 0)
            deposit(d, v, d.observations->coordinates[i], d.observations->weights[i] * residual[i]);
    }
    out.residual_response = convolve(*d.kernel, v, false, ctx);
    if (!prepareLocalDictionary) return Result<PfsmCandidateRecallRound>{std::move(out)};
    auto local = std::make_shared<PfsmCandidateRecallLocalProblem>();
    std::vector<std::size_t> occupied;
    for (std::size_t i = 0; i < d.weights.size(); ++i) if (d.weights[i] > 0) occupied.push_back(i);
    local->z.resize(static_cast<Eigen::Index>(occupied.size())); local->sqrt_weights.resize(local->z.size());
    const double norm = std::sqrt(d.denominator);
    for (std::size_t j = 0; j < occupied.size(); ++j) {
        const auto i = occupied[j];
        local->coordinates.push_back(d.origin + double(i) * d.step);
        local->sqrt_weights[static_cast<Eigen::Index>(j)] = std::sqrt(d.weights[i]) / norm;
        local->z[static_cast<Eigen::Index>(j)] = d.weighted_signal[i] / std::sqrt(d.weights[i]) / norm;
    }
    std::vector<Vector> columns;
    for (const auto &phase : selected) {
        if (!std::isfinite(phase.shift) || phase.shift < d.lower_shift || phase.shift > d.upper_shift)
            detail::fail("pfsm.fast", "Invalid selected shift");
        const auto p = gridProfile(d, phase.input, ctx);
        std::vector<double> shifts{std::max(d.lower_shift, phase.shift - d.width / 4), phase.shift,
                                   std::min(d.upper_shift, phase.shift + d.width / 4)};
        shifts.erase(std::unique(shifts.begin(), shifts.end()), shifts.end());
        for (auto shift : shifts) columns.push_back(column(d, *local, p, shift));
    }
    local->columns.resize(local->z.size(), static_cast<Eigen::Index>(columns.size()));
    for (std::size_t j = 0; j < columns.size(); ++j) local->columns.col(static_cast<Eigen::Index>(j)) = columns[j];
    out.baseline_q = solve(local->columns, local->z, ctx).residual.squaredNorm();
    out.local = std::move(local);
    return Result<PfsmCandidateRecallRound>{std::move(out)};
} catch (const detail::Failure &f) { return Result<PfsmCandidateRecallRound>{f.error}; }

Result<PfsmCandidateRecallEvidence> scorePfsmCandidateRecallCandidate(const PfsmCandidateRecallRound &round, const ProfileExpansionInput &input,
    std::size_t index, const ExecutionContext &ctx, bool collectSeeds) try {
    checkpoint(ctx);
    if (!round.data || !round.data->kernel || round.data->weights.size() < 2 ||
        round.residual_response.size() != round.data->weights.size())
        detail::fail("pfsm.fast", "Missing fast correlation round");
    const auto &d = *round.data;
    if (input.fwhm_degrees != d.width) unavailable("Fast candidate width mismatch");
    const auto reference = lines(input, ctx);
    PfsmCandidateRecallEvidence e; e.original_index = index;
    if (reference.empty()) unavailable("Zero fast candidate");
    std::vector<double> originalScores, residualScores;
    for (double shift : d.scan) {
        checkpoint(ctx);
        double original = 0, residual = 0, energy = 0;
        for (const auto &line : reference) {
            const double t = line.position + shift;
            original += line.intensity * query(d, d.original_response, t);
            residual += line.intensity * query(d, round.residual_response, t);
            energy += line.intensity * line.intensity * query(d, d.energy_response, t);
        }
        if (!(energy > 0) || !std::isfinite(energy)) unavailable("Unreliable fast energy");
        const auto score = [&](double c) {
            if (!std::isfinite(c)) unavailable("Nonfinite fast correlation");
            const double value = std::max(0., c) / std::sqrt(energy) / std::sqrt(d.denominator);
            if (!std::isfinite(value * value)) unavailable("Fast score overflow");
            return value * value;
        };
        const double a = score(original), b = score(residual);
        if (collectSeeds) { originalScores.push_back(a); residualScores.push_back(b); }
        if (!e.original_score || a > *e.original_score) { e.original_score = a; e.original_shift = shift; }
        if (!e.residual_score || b > *e.residual_score) { e.residual_score = b; e.residual_shift = shift; }
    }
    if (collectSeeds) {
        const auto extrema = [&](const std::vector<double> &scores) {
            std::vector<std::size_t> order;
            for (std::size_t i=0;i<scores.size();++i)
                if ((!i || scores[i]>=scores[i-1]) && (i+1==scores.size() || scores[i]>=scores[i+1])) order.push_back(i);
            std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return scores[a]>scores[b];});
            return order;
        };
        const auto original=extrema(originalScores), residual=extrema(residualScores);
        const auto seeds = [&](double first,double second) {
            std::vector<double> out{first};
            const auto add = [&](double value) {
                if (out.size()<3 && std::none_of(out.begin(),out.end(),[&](double old){return std::abs(old-value)<d.width/4;}))
                    out.push_back(value);
            };
            add(second);
            for(auto i:original) { add(d.scan[i]); if(out.size()==3)break; }
            if(out.size()<3) for(auto i:residual) { add(d.scan[i]); if(out.size()==3)break; }
            return out;
        };
        e.original_seeds=seeds(e.original_shift,e.residual_shift);
        e.residual_seeds=seeds(e.residual_shift,e.original_shift);
    }
    return Result<PfsmCandidateRecallEvidence>{std::move(e)};
} catch (const detail::Failure &f) { return Result<PfsmCandidateRecallEvidence>{f.error}; }

std::vector<std::size_t> selectPfsmRecallCandidates(std::vector<PfsmCandidateRecallEvidence> &e,
                                                 const PfsmCandidateRecallLimits &limits) {
    for (auto &row : e) {
        row.recalled = !row.protection_reason.empty();
        row.recall_reason = row.recalled ? "numerical_protection" : "";
        row.original_rank.reset(); row.residual_rank.reset();
    }
    const auto original = order(e, [](const auto &v) { return v.original_score; });
    const auto residual = order(e, [](const auto &v) { return v.residual_score; });
    for (std::size_t i = 0; i < original.size(); ++i) e[original[i]].original_rank = i + 1;
    for (std::size_t i = 0; i < residual.size(); ++i) e[residual[i]].residual_rank = i + 1;
    std::size_t a = 0, b = 0, count = 0;
    std::optional<double> cutA, cutB;
    const auto add = [&](const auto &indices, std::size_t &position, auto value, auto &cut, const char *reason) {
        while (position < indices.size() && e[indices[position]].recalled) ++position;
        if (position < indices.size()) {
            auto &row = e[indices[position++]]; row.recalled = true; ++count; cut = value(row);
            row.recall_reason = reason;
        }
    };
    while (count < static_cast<std::size_t>(limits.recall_candidates) && (a < original.size() || b < residual.size())) {
        add(original, a, [](const auto &v) { return v.original_score; }, cutA, "original_channel");
        if (count < static_cast<std::size_t>(limits.recall_candidates))
            add(residual, b, [](const auto &v) { return v.residual_score; }, cutB, "residual_channel");
    }
    for (auto &row : e) {
        const bool tieA = cutA && row.original_score && tied(*cutA, *row.original_score);
        const bool tieB = cutB && row.residual_score && tied(*cutB, *row.residual_score);
        if (!row.recalled && (tieA || tieB)) {
            row.recalled = true;
            row.recall_reason = tieA && tieB ? "both_channel_cutoff_ties" :
                tieA ? "original_cutoff_tie" : "residual_cutoff_tie";
        }
    }
    std::vector<std::size_t> out;
    for (const auto &row : e) if (row.recalled) out.push_back(row.original_index);
    std::sort(out.begin(), out.end()); return out;
}
std::vector<std::size_t> selectPfsmRecallAblationCandidates(std::vector<PfsmCandidateRecallEvidence> &e) {
    const auto ranked = order(e, [](const auto &v) { return v.recalled ? v.gain : std::optional<double>{}; });
    const auto count = std::min(pfsm_candidate_recall_fit_budget, ranked.size());
    for (auto &row : e) {
        row.retained = !row.protection_reason.empty(); row.local_rank.reset();
        row.retention_reason = row.retained ? "numerical_protection" : "";
    }
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        auto &row = e[ranked[i]]; row.local_rank = i + 1;
        if (!row.retained && (i < count || (count && tied(*row.gain, *e[ranked[count - 1]].gain)))) {
            row.retained = true;
            row.retention_reason = i < count ? "local_rank_budget" : "local_cutoff_tie";
        }
    }
    std::vector<std::size_t> out;
    for (const auto &row : e) if (row.retained) out.push_back(row.original_index);
    std::sort(out.begin(), out.end()); return out;
}

// Diagnostic ablation A: preserve every v1 dictionary proposal and its cold NNLS.
// Physical evaluation happens separately on the original observations.
Result<PfsmCandidateRecallBackprojectionResult> backprojectionProposals(const PfsmCandidateRecallRound &round,
    std::span<const PfsmCandidateRecallSelected> selected, const ProfileExpansionInput &input,
    const PfsmCandidateRecallEvidence &e, const ExecutionContext &ctx) try {
    checkpoint(ctx);
    if (!round.data || !round.local) detail::fail("pfsm.fast.v2", "Missing backprojection round");
    const auto &d = *round.data;
    const auto &local = *round.local;
    std::vector<std::vector<double>> groups;
    std::size_t oldColumns = 0;
    for (const auto &phase : selected) {
        std::vector<double> shifts{std::max(d.lower_shift, phase.shift-d.width/4), phase.shift,
                                  std::min(d.upper_shift, phase.shift+d.width/4)};
        shifts.erase(std::unique(shifts.begin(), shifts.end()), shifts.end());
        oldColumns += shifts.size(); groups.push_back(std::move(shifts));
    }
    if (oldColumns != static_cast<std::size_t>(local.columns.cols()))
        detail::fail("pfsm.fast.v2", "Backprojection phase dictionary mismatch");
    std::set<std::pair<double,double>> intervals;
    for (double shift : {e.original_shift,e.residual_shift}) {
        intervals.emplace(std::max(d.lower_shift,shift-d.width/4),shift);
        intervals.emplace(shift,std::min(d.upper_shift,shift+d.width/4));
    }
    const auto p = gridProfile(d,input,ctx);
    PfsmCandidateRecallBackprojectionResult out;
    for (auto [a,b] : intervals) {
        Matrix columns(local.z.size(),local.columns.cols()+(a==b?1:2));
        columns.leftCols(local.columns.cols())=local.columns;
        columns.col(local.columns.cols())=column(d,local,p,a);
        if(a!=b) columns.col(local.columns.cols()+1)=column(d,local,p,b);
        ++out.nnls_calls;
        const auto solution=solve(columns,local.z,ctx);
        PfsmCandidateRecallBackprojectionTrial trial;
        trial.linear_q=solution.residual.squaredNorm();
        trial.candidate_initial_shift=(a==e.original_shift || b==e.original_shift)?e.original_shift:e.residual_shift;
        trial.coefficients=detail::valuesOf(solution.scales);
        trial.column_shifts=groups;
        trial.column_shifts.push_back(a==b?std::vector<double>{a}:std::vector<double>{a,b});
        out.nnls_iterations+=solution.iterations;
        out.trials.push_back(std::move(trial));
    }
    return Result<PfsmCandidateRecallBackprojectionResult>{std::move(out)};
} catch(const detail::Failure &f) { return Result<PfsmCandidateRecallBackprojectionResult>{f.error}; }
} // namespace qualpf::profile
