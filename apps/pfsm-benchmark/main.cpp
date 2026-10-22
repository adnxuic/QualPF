// Normal identification ranks source-supported chemical candidates before truth
// comparison. Conditional quantification fits separately frozen identities and
// never reads weighed fractions or claims identification acceptance.
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "pfsm_build_revision.h"
#include "candidate_recall_diagnostics.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/export/quantification_export.h"
#include "qualpf/numerics/runtime.h"
#include "infrastructure/io/atomic_export_target.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// clang-format off
#include <psapi.h> // Requires Windows types above.
// clang-format on
#else
#include <sys/resource.h>
#endif
using namespace qualpf;
using namespace qualpf::application;
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
using Clock = std::chrono::steady_clock;
double seconds(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}
[[noreturn]] void fail(const QString &s) { throw std::runtime_error(s.toStdString()); }
template <class T> T take(Result<T> result) {
    if (!result)
        fail(QString::fromStdString(result.error().field + ": " + result.error().message));
    return std::move(result).value();
}
QString required(const QJsonObject &o, const char *key) {
    const auto s = o[key].toString();
    if (s.isEmpty())
        fail(QString("Missing string: ") + key);
    return s;
}
QString digest(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        fail("Cannot read " + path + ": " + f.errorString());
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f))
        fail("Cannot hash " + path);
    return QString::fromLatin1(h.result().toHex());
}
QJsonObject objectFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        fail("Cannot read " + path);
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !d.isObject())
        fail("Invalid JSON object: " + path);
    return d.object();
}
struct ChemicalEvidencePending : std::runtime_error {
    explicit ChemicalEvidencePending(const QString &reason) : std::runtime_error(reason.toStdString()) {}
};
struct Chemistry {
    bool ready = false;
    QString reason;
    CandidateQuery query;
    QJsonObject evidence, query_report;
};
Chemistry chemistry(const QJsonObject &item, const QDir &base, const QString &mode) {
    Chemistry out;
    out.query_report = {{"scope", mode == "none" ? "unconstrained" : "allowed_element_union"},
                        {"executed", false},
                        {"truth_based_filters", false},
                        {"subsequent_rounds", "retained initial candidate universe"}};
    if (mode == "none") {
        // Explicit supplemental runs ignore chemical fields, including unresolved
        // or malformed evidence. They never masquerade as primary acceptance.
        out.ready = true;
        out.evidence = {{"status", "not_requested"}, {"source_hashes_verified", false}};
        return out;
    }
    if (!item.contains("chemical_constraints")) {
        out.reason = "No source-supported chemical constraints were supplied";
        out.evidence = {{"status", "evidence_pending"}, {"source_hashes_verified", false}};
        return out;
    }
    if (!item["chemical_constraints"].isObject())
        fail("Chemical constraints must be an object");
    out.evidence = item["chemical_constraints"].toObject();
    out.evidence["source_hashes_verified"] = false;
    const auto status = required(out.evidence, "status");
    if (status == "evidence_pending" || status == "unsupported") {
        out.reason = "Chemical evidence is not supported: " + status;
        return out;
    }
    if (status != "supported")
        fail("Unknown chemical evidence status: " + status);
    if (required(out.evidence, "kind") != "allowed_elements")
        fail("Chemical constraint kind must be allowed_elements");
    if (required(out.evidence, "basis").trimmed().isEmpty())
        fail("Chemical constraint basis must be nonblank");
    // Neutral IUPAC symbols, independent of the narrower scattering registry.
    // Do not accept formula text, isotope/species labels, or SQL fragments.
    static const auto symbols = QStringLiteral("H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti "
                                               "V Cr Mn Fe Co Ni Cu Zn "
                                               "Ga Ge As Se Br Kr Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In "
                                               "Sn Sb Te I Xe Cs Ba La Ce "
                                               "Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu Hf Ta W Re Os Ir "
                                               "Pt Au Hg Tl Pb Bi Po At Rn "
                                               "Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr Rf Db "
                                               "Sg Bh Hs Mt Ds Rg Cn Nh Fl "
                                               "Mc Lv Ts Og")
                                    .split(' ');
    if (!out.evidence["elements"].isArray() || out.evidence["elements"].toArray().isEmpty())
        fail("Chemical elements must be a nonempty array");
    std::set<QString> unique;
    for (const auto &value : out.evidence["elements"].toArray()) {
        const auto symbol = value.toString();
        if (!value.isString() || !symbols.contains(symbol))
            fail("Invalid chemical element symbol: " + symbol);
        if (!unique.insert(symbol).second)
            fail("Duplicate chemical element symbol: " + symbol);
    }
    QStringList elements;
    for (const auto &symbol : unique)
        elements.append(symbol);
    out.evidence["elements"] = QJsonArray::fromStringList(elements);
    if (!out.evidence["evidence"].isArray() || out.evidence["evidence"].toArray().isEmpty())
        fail("Chemical constraints require source evidence");
    for (const auto &value : out.evidence["evidence"].toArray()) {
        if (!value.isObject())
            fail("Chemical source evidence must be an object");
        const auto source = value.toObject();
        const auto relative = required(source, "path");
        if (relative.trimmed().isEmpty() || !QDir::isRelativePath(relative))
            fail("Chemical evidence path must be relative to the manifest");
        const auto expected = required(source, "sha256");
        if (!QRegularExpression("^[0-9a-f]{64}$").match(expected).hasMatch())
            fail("Chemical evidence requires a lowercase SHA-256 digest");
        if (required(source, "locator").trimmed().isEmpty())
            fail("Chemical evidence locator must be nonblank");
        const auto path = base.filePath(relative);
        if (digest(path) != expected)
            fail("Chemical source hash mismatch: " + path);
    }
    out.ready = true;
    out.evidence["source_hashes_verified"] = true;
    // Existing any/JUST_OP means a nonempty per-card subset of this union,
    // rather than requiring every sample element in each individual phase.
    out.query.elementOperator = CandidateQuery::ElementOperator::any;
    out.query.elements = elements.join(" and ").toStdString();
    out.query_report["elements"] = QString::fromStdString(out.query.elements);
    out.query_report["element_operator"] = "any";
    out.query_report["semantics"] = "nonempty per-card subset of allowed elements";
    return out;
}
std::vector<std::string> profileSubset(const QString &dbFile, int limit) {
    std::vector<std::string> result;
    const auto cleanup = qScopeGuard([] { QSqlDatabase::removeDatabase("pfsm-benchmark-profile"); });
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "pfsm-benchmark-profile");
        db.setDatabaseName(dbFile);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open())
            fail(db.lastError().text());
        QSqlQuery q(db);
        if (!q.exec(QString("SELECT id FROM id ORDER BY id LIMIT %1").arg(limit)))
            fail(q.lastError().text());
        while (q.next())
            result.push_back(q.value(0).toString().toStdString());
        if (q.lastError().isValid())
            fail(q.lastError().text());
    }
    return result;
}
void save(const QString &path, const QJsonObject &o) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        fail("Cannot write " + path);
    const auto bytes = QJsonDocument(o).toJson(QJsonDocument::Indented);
    if (f.write(bytes) != bytes.size() || !f.commit())
        fail("Cannot commit " + path + ": " + f.errorString());
}
template <class T> QJsonArray array(const T &v) {
    QJsonArray a;
    for (auto x : v)
        a.append(QJsonValue(x));
    return a;
}
QJsonObject params(const profile::ProfileFitParameters &p) {
    return {{"scales", array(p.scales)}, {"shifts_degrees", array(p.shifts)}};
}
QJsonObject workStatistics(const profile::ProfileWorkStatistics &s) {
    return {{"input_builds",qint64(s.input_builds)}, {"input_buffer_growths",qint64(s.input_buffer_growths)},
            {"sample_buffers",qint64(s.sample_buffers)}, {"paired_samples",qint64(s.paired_samples)},
            {"physical_columns",qint64(s.physical_columns)}, {"nnls_solves",qint64(s.nnls_solves)},
            {"template_preparations",qint64(s.template_preparations)}, {"chebyshev_tables",qint64(s.chebyshev_tables)},
            {"input_worker_seconds",s.input_seconds}, {"reflection_worker_seconds",s.reflection_seconds},
            {"correlation_worker_seconds",s.correlation_seconds}, {"sampling_worker_seconds",s.sampling_seconds},
            {"nnls_worker_seconds",s.nnls_seconds}, {"template_worker_seconds",s.template_seconds}};
}
QJsonObject diagnostic(const profile::ProfileFitDiagnostics &d) {
    return {{"interpolated_iterations", d.interpolated_iterations},
            {"exact_final_verification", d.exact_final_verification},
            {"residual_evaluations", qint64(d.residual_evaluations)},
            {"difference_evaluations", qint64(d.difference_evaluations)},
            {"nnls_iterations", qint64(d.nnls_iterations)},
            {"starts", qint64(d.starts)},
            {"rank", qint64(d.rank)},
            {"rank_deficient", d.rank_deficient},
            {"kkt_checked", d.kkt_checked},
            {"kkt_violation", d.kkt_violation},
            {"optimality", d.optimality},
            {"at_bound", array(d.at_bound)},
            {"zero_templates", array(d.zero_templates)},
            {"unidentifiable_shifts", array(d.unidentifiable_shifts)}};
}
QString termination(profile::ProfileFitTermination t) {
    using T = profile::ProfileFitTermination;
    switch (t) {
    case T::fixed_shifts:
        return "fixed_shifts";
    case T::gradient:
        return "gradient";
    case T::cost:
        return "cost";
    case T::step:
        return "step";
    case T::cost_and_step:
        return "cost_and_step";
    case T::budget:
        return "budget";
    case T::no_progress:
        return "no_progress";
    case T::feasible_fallback:
        return "feasible_fallback";
    }
    return "unknown";
}
double peakBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS m{};
    m.cb = sizeof(m);
    return GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(m)) ? double(m.PeakWorkingSetSize) : 0;
#else
    rusage r{};
    return getrusage(RUSAGE_SELF, &r) == 0 ? double(r.ru_maxrss) * 1024 : 0;
#endif
}
QJsonObject widthJson(const preprocessing::GeometricFwhmResult &w) {
    QJsonArray peaks;
    for (const auto &p : w.candidates) {
        QJsonObject o{{"sample", qint64(p.sample_index)},
                      {"eligible", p.eligible},
                      {"rejection", preprocessing::fwhm_rejection_name(p.rejection)},
                      {"prominence", p.prominence}};
        if (p.width)
            o["width_degrees"] = p.width->value;
        if (p.sampling_density)
            o["samples_per_fwhm"] = *p.sampling_density;
        if (p.snr)
            o["snr"] = *p.snr;
        peaks.append(o);
    }
    QJsonObject out{{"candidates", peaks},
                    {"minimum_samples_per_fwhm", w.applied.minimum_samples_per_fwhm},
                    {"noise_source", QString::fromStdString(w.noise_source)}};
    if (w.default_fwhm)
        out["H0_degrees"] = w.default_fwhm->value;
    if (w.selected_candidate)
        out["selected_candidate"] = qint64(*w.selected_candidate);
    return out;
}
struct Loaded {
    ProductState state;
    std::shared_ptr<PfsmRequest> request = std::make_shared<PfsmRequest>();
};
// Lossless decimal input produced by the separately reviewable preparation tool.
// No resampling, clipping, normalization or implicit count inference occurs here.
Loaded load(const QJsonObject &item, const QDir &base) {
    Loaded out;
    out.state.algorithms = take(resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm}));
    auto &p = out.state.pattern;
    p.id = {1};
    p.source_id = required(item, "id").toStdString();
    p.processing = {false, item["kalpha2_subtracted"].toBool(false), item["intensity_scaled"].toBool(false)};
    if (!item.contains("intensity_scaled"))
        p.processing.intensity_scaled.reset();
    auto &r = *out.request;
    r.observations.emplace();
    auto &o = *r.observations;
    o.signal_source = required(item, "signal_source").toStdString();
    o.intensity_units = required(item, "intensity_units").toStdString();
    o.noise_source = required(item, "noise_source").toStdString();
    o.unclipped_signal = item["unclipped_signal"].toBool(false);
    for (const auto &v : item["radiation"].toArray()) {
        auto c = v.toObject();
        p.radiation.lines.push_back(
            {WavelengthAngstrom{c["wavelength_angstrom"].toDouble()}, c["weight"].toDouble()});
    }
    required(item, "radiation_source");
    const auto file = base.filePath(required(item, "data_file"));
    if (digest(file) != required(item, "data_sha256"))
        fail("Input hash mismatch: " + file);
    for (const auto &v : item["sources"].toArray()) {
        const auto source = v.toObject();
        const auto raw = base.filePath(required(source, "path"));
        if (digest(raw) != required(source, "sha256"))
            fail("Source hash mismatch: " + raw);
    }
    QFile data(file);
    if (!data.open(QIODevice::ReadOnly))
        fail("Cannot read " + file);
    const auto noise = required(item, "noise");
    const bool third = noise == "standard_deviation" || noise == "least_squares_weight";
    std::vector<double> uncertainty;
    while (!data.atEnd()) {
        auto line = QString::fromUtf8(data.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        const auto fields = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (fields.size() != (third ? 3 : 2))
            fail("Wrong column count: " + file);
        std::vector<double> values;
        for (const auto &field : fields) {
            bool ok = false;
            double x = field.toDouble(&ok);
            if (!ok || !std::isfinite(x))
                fail("Invalid numeric input: " + file);
            values.push_back(x);
        }
        p.two_theta.emplace_back(values[0]);
        p.observed.push_back(values[1]);
        if (third) {
            if (values[2] <= 0)
                fail("Nonpositive uncertainty/weight");
            uncertainty.push_back(noise == "least_squares_weight" ? 1 / values[2] : values[2]);
        }
    }
    if (noise == "least_squares_weight")
        o.noise = preprocessing::FwhmCovariance{std::move(uncertainty), {}};
    else if (noise == "standard_deviation")
        o.noise = preprocessing::FwhmStandardDeviations{std::move(uncertainty)};
    else if (noise == "raw_counts")
        o.noise = preprocessing::FwhmPoissonCounts{};
    else if (noise == "equal")
        o.equal_weights = true;
    else
        fail("Unsupported explicit noise model");
    if (item.contains("constant_background"))
        o.background = std::vector<double>(p.observed.size(), item["constant_background"].toDouble());
    if (item.contains("shift_bounds")) {
        auto b = item["shift_bounds"].toArray();
        if (b.size() != 2)
            fail("Expected two shift bounds");
        r.lower_shift = b[0].toDouble();
        r.upper_shift = b[1].toDouble();
    }
    return out;
}
struct Truth {
    std::vector<QString> phases;
    std::map<std::string, std::size_t> card_phase;
    Truth() = default;
    explicit Truth(const QJsonObject &item) {
        required(item, "truth_source");
        for (const auto &v : item["truth"].toArray()) {
            const auto p = v.toObject();
            const auto name = required(p, "phase");
            if (p["mapping_basis"].toObject().isEmpty())
                fail("Missing structural mapping basis");
            if (std::find(phases.begin(), phases.end(), name) != phases.end())
                fail("Duplicate truth phase");
            const auto ids = p["accepted_ids"].toArray();
            if (ids.empty())
                fail("Uncovered truth phase: " + name);
            for (const auto &id : ids)
                if (id.toString().isEmpty() ||
                    !card_phase.emplace(id.toString().toStdString(), phases.size()).second)
                    fail("Ambiguous/empty truth card mapping");
            phases.push_back(name);
        }
        if (phases.empty())
            fail("Missing truth phases");
    }
};
QString candidateRecallMode(PfsmCandidateRecallMode mode) {
    return mode == PfsmCandidateRecallMode::off ? "off" : mode == PfsmCandidateRecallMode::shadow ? "shadow" : "enabled";
}
QJsonArray candidateRecallIndices(const std::vector<std::size_t> &v) {
    QJsonArray out; for (auto i : v) out.append(qint64(i)); return out;
}
QJsonObject candidateRecallOutput(const QString &directory, int number, const PfsmAnalysis &a) {
    QJsonObject result{{"mode", candidateRecallMode(a.problem->candidate_recall.mode)}, {"version", 3},
        {"algorithm_version", "v2"},
        {"recall_budget", a.problem->candidate_recall.limits.recall_candidates},
        {"fit_budget", a.problem->candidate_recall.limits.local_shortlist},
        {"normalization_scope", a.normalization_over_full_pool ? "full_pool" : "formal_subset"},
        {"normalization_maximum", a.normalization_maximum}};
    result["resource_fallback"] = a.problem->candidate_recall.resource_fallback == PfsmResourceFallback::full_pool ? "full_pool" : "bounded_integral";
    if (a.problem->candidate_recall.limits != profile::PfsmCandidateRecallLimits{})
        result["limits"] = pfsmCandidateRecallLimitsJson(a.problem->candidate_recall.limits);
    if (a.problem->candidate_recall.mode == PfsmCandidateRecallMode::off) return result;
    QFile file(directory + QString("/fast-%1.jsonl").arg(number));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail("Cannot create fast evidence");
    QJsonArray attempts;
    for (std::size_t attempt = 0; attempt < a.candidate_recall_reports.size(); ++attempt) {
        const auto &r = a.candidate_recall_reports[attempt];
        QJsonObject report{{"attempt", qint64(attempt)}, {"generation", qint64(r.generation)},
            {"baseline_version", qint64(r.baseline_version)}, {"formal_baseline_Q", r.formal_baseline_q},
            {"local_baseline_q", r.local_baseline_q}, {"pool_size", qint64(r.pool_size)},
            {"grid_nodes", qint64(r.grid_nodes)}, {"grid_step", r.grid_step},
            {"discarded", r.discarded}, {"full_fallback", r.full_fallback},
            {"fallback_reason", QString::fromStdString(r.fallback_reason)},
            {"recalled_indices", candidateRecallIndices(r.recalled_indices)}, {"retained_indices", candidateRecallIndices(r.retained_indices)},
            {"top5_indices", candidateRecallIndices(r.top5_indices)}};
        if (r.recall_top5_retained) report["recall_top5_retained"] = *r.recall_top5_retained;
        if (r.final_top5_retained) report["final_top5_retained"] = *r.final_top5_retained;
        if (r.selection) report["v2"] = pfsmCandidateRecallSelectionJson(*r.selection);
        report["backend"] = r.backend == PfsmCandidateRecallBackend::v2 ? "v2" : "bounded_integral_relaxed";
        report["switch_reason"] = QString::fromStdString(r.switch_reason);
        if (r.integral) report["integral"] = pfsmIntegralRecallReportJson(*r.integral);
        attempts.append(report);
        for (const auto &e : r.candidates) {
            QJsonObject row{{"attempt", qint64(attempt)}, {"original_index", qint64(e.original_index)},
                {"id", QString::fromStdString((*a.universe)[e.original_index].id)},
                {"original_shift", e.original_shift}, {"residual_shift", e.residual_shift},
                {"recalled", e.recalled}, {"retained", e.retained}, {"nnls_iterations", qint64(e.nnls_iterations)},
                {"recall_reason", QString::fromStdString(e.recall_reason)},
                {"retention_reason", QString::fromStdString(e.retention_reason)},
                {"protection_reason", QString::fromStdString(e.protection_reason)}};
            if (e.original_rank) row["original_rank"] = qint64(*e.original_rank);
            if (e.residual_rank) row["residual_rank"] = qint64(*e.residual_rank);
            if (e.local_rank) row["local_rank"] = qint64(*e.local_rank);
            if (e.original_score) row["original_score"] = *e.original_score;
            if (e.residual_score) row["residual_score"] = *e.residual_score;
            if (e.local_q) row["local_q"] = *e.local_q;
            if (e.gain) row["gain"] = *e.gain;
            if (r.integral) {
                const auto found = std::lower_bound(r.integral->candidates.begin(), r.integral->candidates.end(), e.original_index,
                    [](const auto &v, auto index) { return v.original_index < index; });
                if (found != r.integral->candidates.end() && found->original_index == e.original_index)
                    row["integral"] = pfsmIntegralRecallCandidateJson(*found);
            }
            {
                const auto found = std::lower_bound(r.refinement_candidates.begin(), r.refinement_candidates.end(), e.original_index,
                    [](const auto &v, auto index) { return v.recall.original_index < index; });
                if (found != r.refinement_candidates.end() && found->recall.original_index == e.original_index)
                    row["v2"] = pfsmCandidateRecallCandidateJson(*found);
            }
            const auto bytes = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
            if (file.write(bytes) != bytes.size()) fail("Cannot write fast evidence");
        }
    }
    result["attempts"] = attempts;
    return result;
}
QJsonObject roundOutput(const QString &directory, int number, ProductHandle state, const Truth &truth,
                        const std::set<std::size_t> &accepted, double wall) {
    const auto &a = *state->pfsm;
    QFile file(directory + QString("/round-%1.jsonl").arg(number));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        fail("Cannot create ranking output");
    std::vector<int> ranks(truth.phases.size(), 0), phaseRanksKnown(truth.phases.size(), 0);
    std::set<std::string> seenPhases;
    QJsonArray top;
    int falseTop = 0;
    for (std::size_t k = 0; k < state->candidates.size(); ++k) {
        const auto &c = state->candidates[k];
        const auto &e = *c.pfsm;
        QJsonObject row{{"rank", qint64(k + 1)},
                        {"id", QString::fromStdString(c.id)},
                        {"formula", QString::fromStdString(c.chemicalFormula)},
                        {"name", QString::fromStdString(c.chemicalName)},
                        {"original_index", qint64(e.original_index)},
                        {"Q", e.q},
                        {"Rwp_net", e.rwp_net},
                        {"delta_Q", e.delta_q},
                        {"delta_Rwp_net", e.delta_rwp},
                        {"parameters", params(e.parameters)},
                        {"diagnostics", diagnostic(e.diagnostics)},
                        {"termination", termination(e.termination)}};
        const auto found = truth.card_phase.find(c.id);
        const std::string phaseKey = found == truth.card_phase.end()
                                         ? "unmapped-card:" + c.id
                                         : "mapped-phase:" + std::to_string(found->second);
        seenPhases.insert(phaseKey);
        if (found != truth.card_phase.end()) {
            row["truth_phase"] = truth.phases[found->second];
            if (ranks[found->second] == 0) {
                ranks[found->second] = int(k + 1);
                phaseRanksKnown[found->second] = int(seenPhases.size());
            }
        } else if (k < 5)
            ++falseTop;
        if (k < 5)
            top.append(row);
        const auto bytes = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
        if (file.write(bytes) != bytes.size())
            fail("Failed writing ranking");
    }
    file.close();
    QFile curves(directory + QString("/baseline-%1.tsv").arg(number));
    if (!curves.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        fail("Cannot write baseline curves");
    QTextStream stream(&curves);
    stream.setRealNumberPrecision(17);
    stream << "# 2theta observed background weight calculated residual\n";
    const auto &observations = *a.problem->observations;
    for (std::size_t i = 0; i < observations.coordinates.size(); ++i)
        stream << observations.coordinates[i] << '\t' << observations.observed[i] << '\t'
               << observations.background[i] << '\t' << observations.weights[i] << '\t'
               << a.baseline.calculated[i] << '\t' << a.baseline.residual[i] << '\n';
    stream.flush();
    if (stream.status() != QTextStream::Ok)
        fail("Failed writing baseline curves");
    QJsonArray phaseRanks;
    for (std::size_t p = 0; p < truth.phases.size(); ++p)
        phaseRanks.append(QJsonObject{{"phase", truth.phases[p]},
                                      {"accepted", accepted.contains(p)},
                                      {"card_rank", ranks[p] ? QJsonValue(ranks[p]) : QJsonValue()},
                                      {"phase_rank_known_mapping",
                                       phaseRanksKnown[p] ? QJsonValue(phaseRanksKnown[p]) : QJsonValue()}});
    const auto &p = a.performance;
    const auto recall = candidateRecallOutput(directory, number, a);
    QJsonObject report{{"round", number},
                       {"candidate_recall", recall}, {"fast_pfsm", recall},
                       {"baseline_Q", a.baseline.q},
                       {"baseline_Rwp_net", a.baseline.rwp_net},
                       {"baseline_parameters", params(a.baseline.parameters)},
                       {"baseline_diagnostics", diagnostic(a.baseline.diagnostics)},
                       {"baseline_termination", termination(a.baseline.termination)},
                       {"top5", top},
                       {"truth_card_ranks", phaseRanks},
                       {"false_cards_in_top5", falseTop},
                       {"wall_seconds", wall},
                       {"peak_process_resident_bytes", peakBytes()},
                       {"performance", QJsonObject{{"database_seconds", p.database_seconds},
                                                   {"expansion_worker_seconds", p.expansion_seconds},
                                                   {"solve_worker_seconds", p.solve_seconds},
                                                   {"sorting_seconds", p.sorting_seconds},
                                                   {"candidate_wall_seconds", p.candidate_wall_seconds},
                                                   {"candidate_recall_shared_seconds", p.candidate_recall_shared_seconds},
                                                   {"fast_shared_seconds", p.candidate_recall_shared_seconds},
                                                   {"candidate_recall_correlation_seconds", p.candidate_recall_correlation_seconds},
                                                   {"fast_recall_seconds", p.candidate_recall_correlation_seconds},
                                                   {"candidate_recall_local_seconds", p.candidate_recall_local_seconds},
                                                   {"fast_local_seconds", p.candidate_recall_local_seconds},
                                                   {"candidate_recall_initial_seconds", p.candidate_recall_initial_seconds},
                                                   {"fast_initial_seconds", p.candidate_recall_initial_seconds},
                                                   {"candidate_recall_review_seconds", p.candidate_recall_review_seconds},
                                                   {"fast_review_seconds", p.candidate_recall_review_seconds},
                                                   {"formal_reused", qint64(p.formal_reused)},
                                                   {"template_prepare_calls", qint64(p.templates.prepare_calls)},
                                                   {"template_cache_hits", qint64(p.templates.hits)},
                                                   {"template_cache_misses", qint64(p.templates.misses)},
                                                   {"template_expanded_hits", qint64(p.templates.expanded_hits)},
                                                   {"template_cache_bytes", qint64(p.templates.bytes)},
                                                   {"template_cache_peak_bytes", qint64(p.templates.peak_bytes)},
                                                   {"template_evictions", qint64(p.templates.evictions)},
                                                   {"template_prepare_seconds", p.templates.prepare_seconds},
                                                   {"template_budget_fallbacks", qint64(p.templates.budget_fallbacks)},
                                                   {"recall_cache_hits", qint64(p.recall_cache_hits)},
                                                   {"recall_cache_misses", qint64(p.recall_cache_misses)},
                                                   {"recall_cache_budget_fallbacks", qint64(p.recall_cache_budget_fallbacks)},
                                                   {"recall_cache_bytes", qint64(p.recall_cache_bytes)},
                                                   {"threads", p.threads},
                                                   {"evaluated", qint64(p.evaluated)},
                                                   {"residual_evaluations", qint64(p.residual_evaluations)},
                                                   {"baseline_rebuilds", qint64(p.baseline_rebuilds)},
                                                   {"baseline_refinement_fits",
                                                    qint64(p.baseline_refinement_fits)}}}};
    if (p.detailed_work) {
        report["search_diagnostics"] = QJsonObject{{"correlation",workStatistics(p.correlation_work)},
            {"initial",workStatistics(p.initial_work)}, {"review",workStatistics(p.review_work)},
            {"selection_seconds",p.recall_selection_seconds}};
    }
    if (p.database_read) {
        const auto &s=*p.database_read;
        report["database_read_diagnostics"] = QJsonObject{{"count_seconds",s.count_seconds},
            {"prepare_seconds",s.prepare_seconds}, {"execute_seconds",s.execute_seconds},
            {"fetch_seconds",s.fetch_seconds}, {"fields_seconds",s.fields_seconds},
            {"peaks_seconds",s.peaks_seconds}, {"materialize_seconds",s.materialize_seconds},
            {"row_query_executions",qint64(s.row_query_executions)}, {"cards",qint64(s.cards)},
            {"peaks",qint64(s.peaks)}, {"peak_text_bytes",qint64(s.peak_text_bytes)},
            {"peak_buffer_growths",qint64(s.peak_buffer_growths)}, {"forward_only",s.forward_only}};
    }
    if (a.baseline.rwp_observed)
        report["baseline_Rwp_obs"] = *a.baseline.rwp_observed;
    if (truth.phases.empty()) {
        report.remove("truth_card_ranks");
        report.remove("false_cards_in_top5");
    }
    return report;
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("qualpf_pfsm_benchmark");
    QCommandLineParser cli;
    cli.addHelpOption();
    cli.addOption(QCommandLineOption("search-diagnostics", "Measure read, recall and local work; not a speed measurement."));
    for (const auto &name : {"database", "manifest", "output", "threads", "case", "profile-candidates", "split",
                             "chemistry", "linear-algebra-cache-sizes", "fast-pfsm", "fast-pfsm-version",
                             "pfsm-candidate-recall", "pfsm-candidate-recall-version", "pfsm-resource-fallback"})
        cli.addOption(QCommandLineOption(name, QString("PFSM benchmark ") + name, name));
    cli.addOption(
        QCommandLineOption("prepare-only", "Validate observations/Geometric FWHM without searching."));
    cli.addOption(QCommandLineOption("quantify-selected",
        "Assess quantification conditional on frozen quantification_card_ids; never identification acceptance."));
    cli.addOption(QCommandLineOption("performance-only",
        "Time full candidate searches without truth; optionally replay performance_accept_ids."));
    cli.addOption(QCommandLineOption(
        "template-cache", "Use the bounded IL interpolation cache; verify final fits by direct IL."));
    cli.addOption(QCommandLineOption("full-round", "Formally evaluate the full pool without changing the frozen candidate recall mode."));
    cli.process(app);
    try {
        for (const auto &name : {"database", "manifest", "output"})
            if (!cli.isSet(name))
                fail(QString("Required --") + name);
        PfsmCandidateRecallConfiguration candidate_recall;
        const auto resourceFallback = cli.isSet("pfsm-resource-fallback") ? cli.value("pfsm-resource-fallback") : QString("bounded_integral");
        if (resourceFallback == "full_pool") candidate_recall.resource_fallback = PfsmResourceFallback::full_pool;
        else if (resourceFallback != "bounded_integral") fail("pfsm-resource-fallback must be bounded_integral or full_pool");
        const auto option = [&](const char *name, const char *legacy, const char *fallback) {
            if (cli.isSet(name) && cli.isSet(legacy) && cli.value(name) != cli.value(legacy))
                fail(QString("Conflicting %1 and %2 options").arg(name, legacy));
            return cli.isSet(name) ? cli.value(name) : cli.isSet(legacy) ? cli.value(legacy) : QString(fallback);
        };
        const auto mode = option("pfsm-candidate-recall", "fast-pfsm", "off");
        if (mode == "shadow") candidate_recall.mode = PfsmCandidateRecallMode::shadow;
        else if (mode == "enabled") candidate_recall.mode = PfsmCandidateRecallMode::enabled;
        else if (mode != "off") fail("pfsm-candidate-recall must be off, shadow or enabled");
        const auto candidateRecallVersion = option("pfsm-candidate-recall-version", "fast-pfsm-version", "v2");
        if (candidateRecallVersion != "v2")
            fail("Unsupported candidate recall version: only v2 is maintained; use the historical Git snapshot for v1");
        if (candidate_recall.mode != PfsmCandidateRecallMode::off && cli.isSet("quantify-selected"))
            fail("Candidate recall is not a selected-card quantification mode");
        QJsonObject algebraCache{{"mode", "automatic_cpu_detection"}};
        if (cli.isSet("linear-algebra-cache-sizes")) {
            const auto fields = cli.value("linear-algebra-cache-sizes").split(',');
            if (fields.size() != 3)
                fail("linear-algebra-cache-sizes requires L1,L2,L3 bytes");
            std::ptrdiff_t sizes[3];
            for (int i = 0; i < 3; ++i) {
                bool valid = false;
                const auto value = fields[i].toLongLong(&valid);
                if (!valid || value <= 0 || value > std::numeric_limits<std::ptrdiff_t>::max())
                    fail("Invalid linear-algebra-cache-sizes");
                sizes[i] = static_cast<std::ptrdiff_t>(value);
            }
            take(numerics::initializeLinearAlgebraCacheSizes({sizes[0], sizes[1], sizes[2]}));
            algebraCache = {{"mode", "explicit_process_start"}, {"l1_bytes", qint64(sizes[0])},
                            {"l2_bytes", qint64(sizes[1])}, {"l3_bytes", qint64(sizes[2])}};
        }
        bool ok = true;
        const int threads = cli.isSet("threads") ? cli.value("threads").toInt(&ok) : 0;
        if (!ok || threads < 0)
            fail("Invalid threads");
        const int limit = cli.isSet("profile-candidates") ? cli.value("profile-candidates").toInt(&ok) : 0;
        if (!ok || limit < 0 || (cli.isSet("profile-candidates") && limit == 0))
            fail("Invalid profile-candidates");
        const bool quantificationOnly = cli.isSet("quantify-selected");
        const bool performanceOnly = cli.isSet("performance-only");
        if (performanceOnly && (quantificationOnly || limit || cli.isSet("prepare-only")))
            fail("performance-only cannot be combined with quantify-selected, profiling or prepare-only");
        if (quantificationOnly && (limit || cli.isSet("prepare-only")))
            fail("quantify-selected cannot be combined with profiling or prepare-only");
        const QString split = cli.isSet("split") ? cli.value("split") : "all";
        if (split != "all" && split != "development" && split != "holdout")
            fail("Split must be all, development or holdout");
        const QString chemistryMode = cli.isSet("chemistry") ? cli.value("chemistry") : "evidence";
        if (chemistryMode != "evidence" && chemistryMode != "none")
            fail("Chemistry must be evidence or none");
        const QString acceptanceRole = limit || cli.isSet("prepare-only") || quantificationOnly || performanceOnly ? "not_acceptance"
                                       : chemistryMode == "evidence"     ? "primary"
                                                                          : "supplemental";
        const QString output = QFileInfo(cli.value("output")).absoluteFilePath();
        if (QFileInfo::exists(output))
            fail("Output must be a new directory; previous runs are preserved");
        if (!QDir().mkpath(output))
            fail("Cannot create output directory");
        const QString manifestPath = QFileInfo(cli.value("manifest")).absoluteFilePath();
        const QDir base = QFileInfo(manifestPath).absoluteDir();
        const auto manifest = objectFile(manifestPath);
        if (manifest["schema"] != "qualpf.pfsm.benchmark.v1")
            fail("Unknown manifest schema");
        const QString dbFile = QFileInfo(cli.value("database")).absoluteFilePath();
        if (!dbFile.endsWith(".sq"))
            fail("Database must name the .sq file");
        const QString dbBase = dbFile.chopped(3);
        QJsonObject hashes;
        for (const auto &suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
            const QString h = digest(dbBase + suffix);
            hashes[suffix] = h;
            if (manifest["database_hashes"].toObject()[suffix].toString() != h)
                fail(QString("Frozen database hash mismatch: ") + suffix);
        }
        QJsonArray versions;
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "pfsm-benchmark-metadata");
            db.setDatabaseName(dbFile);
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            if (!db.open())
                fail(db.lastError().text());
            QSqlQuery q(db);
            if (!q.exec("SELECT * FROM infodb"))
                fail(q.lastError().text());
            while (q.next()) {
                QJsonArray row;
                for (int i = 0; i < 5; ++i)
                    row.append(q.value(i).toString());
                versions.append(row);
            }
        }
        QSqlDatabase::removeDatabase("pfsm-benchmark-metadata");
        QJsonObject run{{"schema", "qualpf.pfsm.benchmark-result.v1"},
                        {"started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                        {"code_revision", QUALPF_PFSM_REVISION},
                        {"executable_sha256", digest(QCoreApplication::applicationFilePath())},
                        {"compiler", QUALPF_PFSM_COMPILER},
                        {"manifest_sha256", digest(manifestPath)},
                        {"manifest", manifest},
                        {"database_hashes", hashes},
                        {"database_version", versions},
                        {"mode", quantificationOnly          ? "conditional-selected-quantification"
                                 : performanceOnly           ? "performance-only"
                                 : limit                     ? "profiling"
                                 : cli.isSet("prepare-only") ? "preparation"
                                                             : "full-candidate-top5"},
                        {"template_cache", cli.isSet("template-cache")},
                        {"candidate_recall_mode", mode}, {"candidate_recall_version", candidateRecallVersion},
                        {"candidate_recall_resource_fallback", resourceFallback},
                        {"fast_pfsm_mode", mode}, {"fast_pfsm_version", candidateRecallVersion}, {"full_round", cli.isSet("full-round")},
                        {"linear_algebra_cache", algebraCache},
                        {"validation_split", split},
                        {"threads_requested", threads},
                        {"profile_candidates", limit},
                        {"chemistry_mode_requested", chemistryMode},
                        {"acceptance_role", acceptanceRole},
                        {"query", quantificationOnly ? "frozen phase identities; conditional quantification only"
                                                     : "per-case constraints; no truth-dependent filters"}};
        // Keep startup metadata without replacing a file that a reader may hold.
        save(output + "/run-start.json", run);
        std::signal(SIGINT, interrupt);
        std::signal(SIGTERM, interrupt);
        std::stop_source stop;
        std::jthread watcher([&](std::stop_token done) {
            while (!done.stop_requested()) {
                if (interrupted)
                    stop.request_stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        QJsonArray summaries;
        int failures = 0, executed = 0;
        for (const auto &value : manifest["cases"].toArray()) {
            const auto item = value.toObject();
            const QString id = required(item, "id");
            if (cli.isSet("case") && id != cli.value("case"))
                continue;
            if (split != "all" && item["validation_split"].toString() != split)
                continue;
            ++executed;
            if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch())
                fail("Unsafe case id");
            QJsonObject summary{{"id", id}, {"status", "not_run"}};
            summary["validation_split"] = item["validation_split"];
            summary["evaluation_family"] = item["evaluation_family"];
            summary["chemistry_mode_requested"] = chemistryMode;
            summary["acceptance_role"] = acceptanceRole;
            summary["query"] = QJsonObject{{"executed", false}, {"truth_based_filters", false}};
            if (chemistryMode == "evidence" && item.contains("chemical_constraints")) {
                auto declared = item["chemical_constraints"].toObject();
                declared["source_hashes_verified"] = false;
                summary["chemical_constraints"] = declared;
            }
            const QString directory = output + "/" + id;
            if (!QDir().mkpath(directory))
                fail("Cannot create case directory");
            QJsonArray rounds;
            const auto caseStart = Clock::now();
            try {
                if (item["status"] != "ready") {
                    summary["status"] = required(item, "status");
                    summary["reason"] = required(item, "reason");
                    ++failures;
                } else {
                    const auto chemical = chemistry(item, base, chemistryMode);
                    summary["chemical_constraints"] = chemical.evidence;
                    summary["query"] = chemical.query_report;
                    if (!chemical.ready && !cli.isSet("prepare-only"))
                        throw ChemicalEvidencePending(chemical.reason);
                    const Truth truth = performanceOnly ? Truth{} : Truth(item);
                    std::vector<std::string> replay;
                    if (performanceOnly && item.contains("performance_accept_ids")) {
                        if (!item["performance_accept_ids"].isArray())
                            fail("performance_accept_ids must be an ordered array");
                        for (const auto &v : item["performance_accept_ids"].toArray()) {
                            const auto id = v.toString().toStdString();
                            if (id.empty() || std::find(replay.begin(), replay.end(), id) != replay.end())
                                fail("Invalid/duplicate performance replay card ID");
                            replay.push_back(id);
                        }
                        summary["performance_accept_ids"] = item["performance_accept_ids"];
                    }
                    const auto importStart = Clock::now();
                    auto loaded = load(item, base);
                    summary["import_seconds"] = seconds(importStart);
                    loaded.request->threads = threads;
                    loaded.request->measure_search_work = cli.isSet("search-diagnostics");
                    loaded.request->candidate_recall = candidate_recall;
                    loaded.request->full_round = cli.isSet("full-round");
                    loaded.request->template_cache = cli.isSet("template-cache");
                    ProductSearchSession session(std::move(loaded.state));
                    SearchCoordinator coordinator(session);
                    ExecutionContext context;
                    context.numerical_policy = NumericalPolicy::explicit_float64;
                    context.cancellation = stop.get_token();
                    std::cout << id.toStdString() << ": prepare observations" << std::endl;
                    const auto preparationStart = Clock::now();
                    auto problem = take(preparePfsmProblem(session.snapshot(), *loaded.request, context));
                    summary["preflight_preparation_seconds"] = seconds(preparationStart);
                    save(directory + "/width.json", widthJson(problem->width));
                    summary["points"] = qint64(problem->observations->coordinates.size());
                    if (!problem->width.default_fwhm) {
                        summary["status"] = "no_eligible_geometric_fwhm";
                        ++failures;
                    } else if (cli.isSet("prepare-only")) {
                        summary["status"] = "prepared_not_evaluated";
                        summary["H0_degrees"] = problem->width.default_fwhm->value;
                    } else {
                        auto query = chemical.query;
                        auto queryReport = chemical.query_report;
                        std::vector<std::string> selectedIds;
                        if (quantificationOnly) {
                            for (const auto &v : item["quantification_card_ids"].toArray()) {
                                const auto id = v.toString().toStdString();
                                if (id.empty() || std::find(selectedIds.begin(), selectedIds.end(), id) != selectedIds.end() ||
                                    !truth.card_phase.contains(id)) fail("Invalid frozen quantification card ID");
                                selectedIds.push_back(id);
                            }
                            if (selectedIds.size() != truth.phases.size()) fail("Expected one frozen card per phase");
                            std::set<std::size_t> distinctPhases;
                            for (const auto &id : selectedIds) distinctPhases.insert(truth.card_phase.at(id));
                            if (distinctPhases.size() != truth.phases.size()) fail("Duplicate frozen phase identity");
                            query.preselectedIds = selectedIds;
                            queryReport["scope"] = "frozen_selected_cards_quantification";
                            queryReport["truth_based_filters"] = true;
                            queryReport["identity_basis"] = "Specified phase identities; no identification claim or mass-fraction fit constraints";
                        }
                        if (limit) {
                            // Chemical sources have been checked before even the
                            // diagnostic ID preselection reads candidate rows.
                            query.preselectedIds = profileSubset(dbFile, limit);
                            QJsonArray ids;
                            for (const auto &candidate : *query.preselectedIds)
                                ids.append(QString::fromStdString(candidate));
                            queryReport["profile_preselected_ids"] = ids;
                        }
                        infrastructure::SqlCandidateReaderFactory factory;
                        const ReferenceDatabase database{dbBase.toStdString(), 1};
                        auto guard = take(factory.protect(database));
                        std::set<std::size_t> accepted;
                        std::optional<ProductCard> chosen;
                        int number = 0;
                        while (true) {
                            const auto start = Clock::now();
                            auto task = take(coordinator.begin(database));
                            task.context.cancellation = stop.get_token();
                            SearchInput input;
                            input.source = task.source;
                            input.taskId = task.id;
                            input.databaseVersion = 1;
                            input.method = SearchMethod::pfsm;
                            if (number == 0) {
                                input.pfsm = loaded.request;
                                input.query = query;
                            } else {
                                auto next = std::make_shared<PfsmRequest>();
                                next->full_round = cli.isSet("full-round");
                                next->threads = threads;
                                next->measure_search_work = cli.isSet("search-diagnostics");
                                input.pfsm = next;
                                input.operation = SearchOperation::accept_phase;
                                input.phase = chosen;
                            }
                            auto reader = take(factory.open(database, task.id, task.context));
                            std::cout << id.toStdString() << ": round " << number << ", accepted "
                                      << accepted.size() << std::endl;
                            auto progressTime = Clock::now();
                            const auto progress = [&](std::size_t done, std::size_t total) {
                                if (seconds(progressTime) >= 15 || done == total) {
                                    std::cout << id.toStdString() << ": work " << done << '/' << total
                                              << std::endl;
                                    progressTime = Clock::now();
                                }
                            };
                            queryReport["executed"] = true;
                            summary["query"] = queryReport;
                            const auto searchStart = Clock::now();
                            auto result = SearchEngine(task.algorithms).run(input, *reader, task.context, progress);
                            const double searchSeconds = seconds(searchStart);
                            const auto publicationStart = Clock::now();
                            take(coordinator.complete(task.id, std::move(result)));
                            const double publicationSeconds = seconds(publicationStart);
                            const double resultAvailableSeconds = seconds(start);
                            auto state = session.snapshot();
                            const auto &a = *state->pfsm;
                            if (number == 0) {
                                QFile order(directory + "/candidate-order.txt");
                                if (!order.open(QIODevice::WriteOnly | QIODevice::NewOnly))
                                    fail("Cannot write candidate order");
                                std::vector<bool> covered(truth.phases.size());
                                for (const auto &c : *a.universe) {
                                    order.write(QByteArray::fromStdString(c.id) + "\n");
                                    auto t = truth.card_phase.find(c.id);
                                    if (t != truth.card_phase.end())
                                        covered[t->second] = true;
                                }
                                order.close();
                                summary["candidate_order_sha256"] =
                                    digest(directory + "/candidate-order.txt");
                                summary["candidate_count"] = qint64(a.universe->size());
                                if (!limit &&
                                    std::find(covered.begin(), covered.end(), false) != covered.end())
                                    fail("Frozen candidate universe does not cover all true phases");
                            }
                            const auto outputStart = Clock::now();
                            auto report = roundOutput(directory, number, state, truth, accepted, resultAvailableSeconds);
                            report["search_seconds"] = searchSeconds;
                            report["publication_seconds"] = publicationSeconds;
                            report["output_seconds"] = seconds(outputStart);
                            report["operation_to_result_seconds"] = resultAvailableSeconds;
                            // CLI serialization is measurable; native GUI drawing is not part of this runner.
                            report["display_seconds"] = QJsonValue();
                            save(directory + QString("/round-%1.json").arg(number), report);
                            rounds.append(report);
                            summary["rounds"] = rounds;
                            if (performanceOnly) {
                                if (number == static_cast<int>(replay.size())) {
                                    summary["status"] = "performance_not_acceptance";
                                    break;
                                }
                                const auto &id = replay[number];
                                const auto found = std::find_if(state->candidates.begin(), state->candidates.end(),
                                    [&](const auto &card) { return card.id == id; });
                                if (found == state->candidates.end())
                                    fail("Performance replay card not returned by the database");
                                chosen = *found;
                                ++number;
                                save(directory + QString("/summary-through-round-%1.json").arg(number - 1), summary);
                                continue;
                            }
                            if (quantificationOnly) {
                                if (number == static_cast<int>(selectedIds.size())) {
                                    const auto quantified = prepareQuantification(state);
                                    infrastructure::exporting::QtAtomicExportFactory exports;
                                    take(application::exporting::writeQuantificationCsv(exports,
                                        (directory + "/quantification.csv").toStdString(), quantified));
                                    summary["status"] = quantified.available() ? "conditional_quantification_available"
                                                                               : "conditional_quantification_unavailable";
                                    break;
                                }
                                const auto &id = selectedIds[number];
                                const auto found = std::find_if(state->candidates.begin(), state->candidates.end(),
                                    [&](const auto &card) { return card.id == id; });
                                if (found == state->candidates.end()) fail("Frozen quantification card not returned by the database");
                                chosen = *found;
                                accepted.insert(truth.card_phase.at(id));
                                ++number;
                                save(directory + QString("/summary-through-round-%1.json").arg(number - 1), summary);
                                continue;
                            }
                            if (limit) {
                                summary["status"] = "profiling_not_acceptance";
                                break;
                            }
                            if (accepted.size() == truth.phases.size()) {
                                summary["status"] = "passed_top5_assisted";
                                summary["remaining_max_delta_Rwp_net"] =
                                    state->candidates.empty() ? 0 : state->candidates.front().fom;
                                break;
                            }
                            chosen.reset();
                            for (std::size_t i = 0; i < std::min<std::size_t>(5, state->candidates.size());
                                 ++i) {
                                const auto &c = state->candidates[i];
                                const auto found = truth.card_phase.find(c.id);
                                if (found != truth.card_phase.end() && !accepted.contains(found->second)) {
                                    chosen = c;
                                    accepted.insert(found->second);
                                    break;
                                }
                            }
                            if (!chosen) {
                                summary["status"] = "failed_top5";
                                ++failures;
                                break;
                            }
                            ++number;
                            save(directory + QString("/summary-through-round-%1.json").arg(number - 1), summary);
                        }
                        if (performanceOnly) {
                            summary["replayed_phase_count"] = qint64(replay.size());
                        } else if (quantificationOnly) {
                            summary["specified_phase_count"] = qint64(accepted.size());
                        } else {
                            summary["identified_phases"] = qint64(accepted.size());
                            summary["true_phase_count"] = qint64(truth.phases.size());
                            summary["phase_recall"] = double(accepted.size()) / double(truth.phases.size());
                            QJsonArray identified;
                            for (auto index : accepted)
                                identified.append(truth.phases[index]);
                            summary["identified_phase_names"] = identified;
                            const auto weak = item["weak_phases"].toArray();
                            int weakAccepted = 0;
                            for (const auto &name : weak)
                                if (identified.contains(name))
                                    ++weakAccepted;
                            summary["weak_phase_recall"] =
                                weak.empty() ? QJsonValue() : QJsonValue(double(weakAccepted) / weak.size());
                        }
                    }
                }
            } catch (const ChemicalEvidencePending &e) {
                summary["status"] = "chemical_evidence_pending";
                summary["reason"] = e.what();
                ++failures;
            } catch (const std::exception &e) {
                summary["status"] = stop.stop_requested() ? "cancelled" : "error";
                summary["error"] = e.what();
                ++failures;
            }
            summary["rounds"] = rounds;
            summary["end_to_end_seconds"] = seconds(caseStart);
            summary["timing_scope"] = "CLI input validation/import, preflight preparation, searches, checked publication and output; GUI display unmeasured";
            save(directory + "/summary.json", summary);
            summaries.append(summary);
            std::cout << id.toStdString() << ": " << summary["status"].toString().toStdString() << std::endl;
            if (stop.stop_requested())
                break;
        }
        if (executed == 0)
            fail("No matching cases");
        run["cases"] = summaries;
        run["failures_or_unresolved"] = failures;
        run["peak_process_resident_bytes"] = peakBytes();
        save(output + "/run.json", run);
        return failures ? 2 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
