#include "../../support/classic_fixture.h"
#include "infrastructure/io/product_files.h"
#include "infrastructure/serialization/qpf_project.h"
#include "infrastructure/serialization/qpf_value.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QFileInfo>
#include <functional>
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/project_reference_operation.h"
#include <set>
#include <QDateTime>
#include <QUuid>
#include <bit>
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/initial_peak_search.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/workflow.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok)
        throw std::runtime_error("QPF line " + std::to_string(at.line()));
}
template <class T> T take(Result<T> r, std::source_location at = std::source_location::current()) {
    if (!r)
        throw std::runtime_error("line " + std::to_string(at.line()) + " " + r.error().field + ": " +
                                 r.error().message);
    return std::move(r).value();
}
static double il(double x, double q) {
    return std::pow(1 + 4 * (std::pow(2., 2. / 3) - 1) * std::pow((x - q) / .2, 2), -1.5);
}
static ProductState state() {
    ProductState s;
    s.projectSettings.emplace();
    s.pattern.id = {17};
    s.pattern.source_id = "independent-analytic-mixture";
    s.pattern.radiation.lines = {{WavelengthAngstrom{1.5406}, 1}};
    s.pattern.processing = {false, false, false};
    for (int i = 0; i <= 700; ++i) {
        const double x = 28 + .01 * i;
        s.pattern.two_theta.emplace_back(x);
        s.pattern.observed.push_back(7 + 100 * il(x, 30.061) + 30 * il(x, 32.964));
    }
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.original_kalpha2_subtracted = false;
    s.workflow.imported_pattern = std::make_shared<const Pattern>(s.pattern);
    s.projectSettings->profile(WorkflowId::preprocessing).background.method =
        preprocessing::BackgroundMethod::none;
    return s;
}
struct Reader : CandidateReader {
    int calls = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress) override {
        ++calls;
        std::vector<ProductCard> cards;
        for (const auto &[id, q] :
             std::vector<std::pair<std::string, double>>{{"strong", 30}, {"weak", 33}, {"other", 31}}) {
            ProductCard c;
            c.id = id;
            c.d = {1.5406 / (2 * std::sin(q * std::numbers::pi / 360))};
            c.intensity = {1};
            c.referenceRir = 2;
            c.rir = "2";
            cards.push_back(c);
        }
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
static application::ProjectSaveSnapshot snapshot(ProductHandle s) {
    return {std::move(s), std::make_shared<const ProjectViewState>(), {}, {}, 0};
}
static ProductState roundtrip(const ProductState &s) {
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + "/roundtrip.qpf").toStdString();
    take(QpfProjectWriter{}.writeAtomic(path, snapshot(std::make_shared<const ProductState>(s))));
    return take(QpfProjectReader{}.read(path, SearchBackend::cpp)).state;
}
static void preprocessingRoundtrip() {
    ProductSearchSession session(state());
    auto source = session.snapshot();
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "qpf1"};
    auto result = take(evaluateInitialPeakSearch(
        source, source->projectSettings->profile(WorkflowId::preprocessing).initial_peaks, context));
    auto next = *source;
    take(applyInitialPeakSearch(next, result));
    take(session.commit(source, std::move(next)));
    source = session.snapshot();
    auto fixed = source->projectSettings->profile(WorkflowId::preprocessing).fwhm;
    fixed.use_fixed = true;
    fixed.default_fwhm = AngularWidthDegrees{.2};
    auto width = take(
        evaluateConfiguredFwhm(source, ConfiguredFwhmRequest{source->pattern, fixed, {}, true}, context));
    next = *source;
    take(applyConfiguredFwhm(next, width));
    take(session.commit(source, std::move(next)));
    check(bool(session.snapshot()->preprocessing.initial_peak_usage));
    check(!session.snapshot()->workflow.records.empty());
    auto restored = roundtrip(*session.snapshot());
    check(bool(restored.preprocessing.initial_peak_usage));
    check(restored.preprocessing.initial_peak_usage->detection.candidates.size() ==
          session.snapshot()->preprocessing.initial_peak_usage->detection.candidates.size());
    check(restored.preprocessing.fwhm_usage->width.value == .2 && restored.preprocessing.fwhm_usage->current);
    check(restored.workflow.records.size() == session.snapshot()->workflow.records.size());
    check(reusableWorkflowStep(restored, WorkflowId::csm, WorkflowStep::background));
    ProductSearchSession opened(restored);
    for (const auto &r : restored.workflow.records)
        check(!opened.canUndo(r.id));
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + "/saved.qpf").toStdString();
    auto frozen = snapshot(session.snapshot());
    const auto id = take(QpfProjectWriter{}.writeAtomic(path, frozen));
    const auto prior = take(QtTextFiles{}.read(path));
    auto invalid = *session.snapshot();
    invalid.pattern.observed = {1};
    check(!QpfProjectWriter{}.writeAtomic(path, snapshot(std::make_shared<const ProductState>(invalid))));
    check(take(QtTextFiles{}.read(path)) == prior);
    frozen.identity = id;
    take(QpfProjectWriter{}.writeAtomic(path, frozen));
    check(take(QpfProjectReader{}.read(path, SearchBackend::cpp)).identity == id);
}

static void csmRoundtrip() {
    testing::ClassicFixture fixture;
    ProductState s;
    s.pattern = fixture.pattern;
    s.pattern.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    auto peaks = fixture.peaks;
    peaks.effective_wavelength = WavelengthAngstrom{1.54};
    s.peaks = take(productMeasuredPeaks(peaks));
    struct CsmReader : CandidateReader {
        ProductCard card;
        Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                              SearchProgress) override {
            return Result<std::vector<ProductCard>>{{card}};
        }
    } reader;
    reader.card.id = "A";
    reader.card.rir = "2";
    for (const auto &reflection : fixture.card.reflections) {
        reader.card.tth.push_back(reflection.two_theta->value);
        reader.card.intensity.push_back(reflection.intensity);
    }
    SearchInput input;
    input.source = std::make_shared<const ProductState>(s);
    input.parameters = fixture.parameters;
    input.databaseVersion = 9;
    input.databaseIdentity = "classic-library";
    auto result = take(SearchEngine{}.run(input, reader, fixture.context)).next;
    check(result.candidates.size() == 1 && result.candidates[0].fom == 1 && result.candidates[0].scale == 10);
    auto restored = roundtrip(result);
    check(restored.candidates[0].fom == 1 && restored.candidates[0].scale == 10 &&
          restored.candidates[0].tth == reader.card.tth);
    check(restored.candidates[0].algorithmSource &&
          restored.candidates[0].algorithmSource->score.algorithms == restored.algorithms);
    auto accept = take(
        phaseAcceptanceInput(std::make_shared<const ProductState>(restored), "A", fixture.parameters, true));
    check(bool(accept));
    accept->databaseIdentity = input.databaseIdentity;
    accept->databaseVersion = 20;
    auto selected = take(SearchEngine{}.run(*accept, reader, fixture.context)).next;
    restored = roundtrip(selected);
    check(restored.phases.size() == 1 && restored.peaks.intensity == selected.peaks.intensity &&
          restored.provenance.residualHistory.size() == selected.provenance.residualHistory.size());
}
static void changeObject(const std::string &, const std::string &,
                         const std::function<void(qpf::Value::Map &, const std::string &)> &, bool);
static void pfsm(bool recall, bool cache, bool integral = false, bool fullPolicy = false) {
    auto s = state();
    s.algorithms = take(resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm}));
    s.workflow.active = WorkflowId::pfsm;
    // Explicit, independently generated signal and background; no product
    // goldens.
    auto request = std::make_shared<PfsmRequest>();
    request->threads = 1;
    request->template_cache = cache;
    if (fullPolicy) request->candidate_recall.resource_fallback = PfsmResourceFallback::full_pool;
    request->lower_shift = -.12;
    request->upper_shift = .12;
    request->candidate_recall.mode = recall ? PfsmCandidateRecallMode::enabled : PfsmCandidateRecallMode::off;
    // Three cards exceed a deliberately configured one-card small-pool boundary.
    if (recall) request->candidate_recall.limits = {500,1,1,80,80};
    if (integral) {
        request->lower_shift = request->upper_shift = 0;
        FwhmUsage width;
        width.width = AngularWidthDegrees{.005}; width.origin = FwhmOrigin::fixed;
        width.algorithm = {"qualpf.fwhm.fixed",1}; width.dataset = s.pattern.id;
        width.source_id = s.pattern.source_id; width.current = width.manual = true;
        width.minimum_sampling_density = 6; request->supplied_width = width;
    }
    request->observations.emplace();
    auto &o = *request->observations;
    o.signal_source = "analytic";
    o.intensity_units = "arbitrary";
    o.noise_source = "equal weights";
    o.unclipped_signal = true;
    o.equal_weights = true;
    o.background = std::vector<double>(701, 7);
    auto source = std::make_shared<const ProductState>(s);
    SearchInput input;
    input.source = source;
    input.method = SearchMethod::pfsm;
    input.pfsm = request;
    input.databaseVersion = 19;
    input.databaseIdentity = "fixture:immutable-v1";
    ExecutionContext context{{}, NumericalPolicy::explicit_float64, "qpf1"};
    Reader reader;
    auto searched = take(SearchEngine{}.run(input, reader, context));
    check(searched.next.candidates.front().id == "strong");
    auto accepted = take(phaseAcceptanceInput(std::make_shared<const ProductState>(searched.next), "strong",
                                              SearchParameters{AngularWidthDegrees{0}}, true));
    check(bool(accepted));
    accepted->databaseVersion = 19;
    accepted->databaseIdentity = input.databaseIdentity;
    auto selected = take(SearchEngine{}.run(*accepted, reader, context)).next;
    selected = take(preparePfsmRirRevision(std::make_shared<const ProductState>(selected), "strong",
                                           PfsmRirRevision{3, "independent supplied RIR"}));
    QTemporaryDir savedDirectory;
    check(savedDirectory.isValid());
    const auto savedPath = (savedDirectory.path() + "/pfsm.qpf").toStdString();
    take(QpfProjectWriter{}.writeAtomic(savedPath, snapshot(std::make_shared<const ProductState>(selected))));
    auto fresh = take(QpfProjectReader{}.read(savedPath, SearchBackend::cpp)).state;
    check(fresh.pfsm && fresh.algorithms->spec.method() == SearchMethod::pfsm);
    check(fresh.pfsm->baseline.parameters.shifts == selected.pfsm->baseline.parameters.shifts);
    auto restored = roundtrip(selected);
    check(restored.pfsm->problem->candidate_recall.resource_fallback == request->candidate_recall.resource_fallback);
    if (integral && !fullPolicy) {
        check(restored.pfsm->candidate_recall_reports.back().backend == PfsmCandidateRecallBackend::bounded_integral_relaxed &&
            restored.pfsm->candidate_recall_reports.back().integral && !restored.pfsm->integral_recall_data);
        const auto &a = restored.pfsm->candidate_recall_reports.back().integral->candidates;
        const auto &b = selected.pfsm->candidate_recall_reports.back().integral->candidates;
        check(a.size() == b.size());
        for (std::size_t i=0;i<a.size();++i) check(a[i].relaxed.best.q == b[i].relaxed.best.q &&
            a[i].relaxed.best.shifts == b[i].relaxed.best.shifts && a[i].relaxed.evaluations == b[i].relaxed.evaluations);
    } else if (recall) {
        changeObject(savedPath, "pfsm_candidate_recall_report", [](auto &fields, const auto &) {
            fields.erase("backend"); fields.erase("switch_reason"); fields.erase("integral");
        }, true);
        changeObject(savedPath, "pfsm_candidate_recall_configuration", [](auto &fields, const auto &) { fields.erase("resource_fallback"); }, true);
        auto historical = take(QpfProjectReader{}.read(savedPath, SearchBackend::cpp)).state;
        check(historical.pfsm->problem->candidate_recall.resource_fallback == PfsmResourceFallback::bounded_integral);
        check(historical.pfsm->candidate_recall_reports.back().backend == PfsmCandidateRecallBackend::v2);
        if (fullPolicy) {
            // Historical v2 diagnostics stay unchanged on load. A subsequent
            // rescore uses the missing-field default on the frozen narrow problem.
            auto resumedInput = input;
            resumedInput.source = std::make_shared<const ProductState>(historical);
            resumedInput.operation = SearchOperation::rescore;
            auto workers = std::make_shared<PfsmRequest>(); workers->threads = 1;
            resumedInput.pfsm = workers;
            auto resumed = take(SearchEngine{}.run(resumedInput, reader, context)).next;
            check(resumed.pfsm->problem == historical.pfsm->problem && reader.calls == 1 &&
                resumed.pfsm->candidate_recall_reports.back().backend == PfsmCandidateRecallBackend::bounded_integral_relaxed &&
                resumed.pfsm->integral_recall_data);
        }
        changeObject(savedPath, "pfsm_candidate_recall_report", [](auto &fields, const auto &) { fields.erase("pool_size"); }, true);
        check(!QpfProjectReader{}.read(savedPath, SearchBackend::cpp));
    }
    check(restored.algorithms == restored.pfsm->problem->algorithms);
    check(restored.pfsm->baseline.calculated == selected.pfsm->baseline.calculated &&
          restored.pfsm->baseline.parameters.shifts == selected.pfsm->baseline.parameters.shifts);
    check(restored.pfsm->normalization_maximum == selected.pfsm->normalization_maximum &&
          restored.pfsm->rir_revisions == selected.pfsm->rir_revisions);
    check(prepareQuantification(std::make_shared<const ProductState>(restored)).available());
    check(prepareQuantification(std::make_shared<const ProductState>(restored)).percentages() ==
          prepareQuantification(std::make_shared<const ProductState>(selected)).percentages());
    auto resume = take(phaseAcceptanceInput(std::make_shared<const ProductState>(restored), "weak",
                                            SearchParameters{AngularWidthDegrees{0}}, true));
    check(bool(resume));
    const auto frozenDatabase =
        projectOperationDatabase(*resume, {"missing-reference-database", 200, "other-library"});
    check(frozenDatabase.identity == input.databaseIdentity);
    resume->databaseVersion = frozenDatabase.selectionVersion;
    resume->databaseIdentity = frozenDatabase.identity;
    auto continued = take(SearchEngine{}.run(*resume, reader, context));
    check(continued.next.phases.size() == 2 && reader.calls == 1);
    auto direct = take(phaseAcceptanceInput(std::make_shared<const ProductState>(selected), "weak",
                                            SearchParameters{AngularWidthDegrees{0}}, true));
    direct->databaseVersion = 19;
    direct->databaseIdentity = input.databaseIdentity;
    auto reference = take(SearchEngine{}.run(*direct, reader, context));
    check(continued.next.pfsm->baseline.parameters.shifts ==
              reference.next.pfsm->baseline.parameters.shifts &&
          continued.next.pfsm->baseline.q == reference.next.pfsm->baseline.q);
    resume->databaseIdentity = "another-library";
    auto denied = SearchEngine{}.run(*resume, reader, context);
    check(!denied && denied.error().reason == "project_database_mismatch" && reader.calls == 1);
    auto newRead = *resume;
    newRead.operation = SearchOperation::initial;
    check(projectOperationDatabase(newRead, {"new-database", 201, "different-library"}).identity ==
          "different-library");
    auto broken = selected;
    auto analysis = std::make_shared<PfsmAnalysis>(*broken.pfsm);
    analysis->baseline.calculated[100] += 1;
    broken.pfsm = analysis;
    check(!QpfProjectWriter{}.writeAtomic(savedPath, snapshot(std::make_shared<const ProductState>(broken))));
}

static void editDatabase(const std::string &path, const std::function<void(QSqlDatabase &)> &edit) {
    const auto name = "qpf_test_" + QUuid::createUuid().toString();
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(QString::fromStdString(path));
        check(db.open());
        edit(db);
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
static void changeManifest(const std::string &path, const std::function<void(qpf::Value::Map &)> &edit) {
    editDatabase(path, [&](QSqlDatabase &db) {
        QSqlQuery q(db);
        check(q.exec("SELECT payload FROM manifest"));
        check(q.next());
        qpf::Budget budget;
        auto value = qpf::parse(q.value(0).toByteArray(), budget);
        auto map = value.as<qpf::Value::Map>();
        edit(map);
        const auto bytes = qpf::cbor(map);
        q.prepare("UPDATE manifest SET payload=?,sha256=?");
        q.addBindValue(bytes);
        q.addBindValue(QString::fromStdString(qpf::digest(bytes)));
        check(q.exec());
    });
}
static void changeObject(const std::string &path, const std::string &kind,
                         const std::function<void(qpf::Value::Map &, const std::string &)> &edit,
                         bool checksum = true) {
    editDatabase(path, [&](QSqlDatabase &db) {
        QSqlQuery q(db);
        q.prepare("SELECT id,version,payload FROM objects WHERE kind=? LIMIT 1");
        q.addBindValue(QString::fromStdString(kind));
        check(q.exec() && q.next());
        const auto id = q.value(0).toString().toStdString();
        const auto version = q.value(1).toULongLong();
        qpf::Budget budget;
        auto map = qpf::parse(q.value(2).toByteArray(), budget).as<qpf::Value::Map>();
        edit(map, id);
        q.prepare(checksum ? "UPDATE objects SET payload=?,sha256=? WHERE id=?"
                           : "UPDATE objects SET payload=? WHERE id=?");
        q.addBindValue(qpf::cbor(map));
        if (checksum)
            q.addBindValue(QString::fromStdString(qpf::objectDigest(id, {kind, version, map})));
        q.addBindValue(QString::fromStdString(id));
        check(q.exec());
    });
}
static void protocolAndFailures() {
    // Independent protocol bytes: [uint64_max, int64_min, binary64 negative zero,
    // binary64 1, integer 1, undefined, null]. No product-generated expectations.
    qpf::Budget budget;
    const auto sample = QByteArray::fromHex(
        "871bffffffffffffffff3b7ffffffffffffffffb8000000000000000fb3ff000000000000001f7f6");
    const auto values = qpf::parse(sample, budget).as<qpf::Value::Array>();
    check(values[0].as<std::uint64_t>() == UINT64_MAX && values[1].as<std::int64_t>() == INT64_MIN);
    check(std::bit_cast<std::uint64_t>(values[2].as<double>()) == 0x8000000000000000ull);
    check(values[3].as<double>() == 1 && values[4].as<std::uint64_t>() == 1 &&
          std::holds_alternative<qpf::Undefined>(values[5].data) &&
          std::holds_alternative<std::nullptr_t>(values[6].data));
    check(qpf::cbor(qpf::Value(values)) == sample);
    const auto metadataMax = qpf::metadata(values[0]);
    check(std::get<std::uint64_t>(metadataMax.data) == UINT64_MAX &&
          qpf::metadata(metadataMax).as<std::uint64_t>() == UINT64_MAX);
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + QString::fromUtf8("/项目.qpf")).toStdString();
    auto s = state();
    s.pattern.id = {UINT64_MAX};
    s.pattern.observed[0] = -0.0;
    s.pattern.original_observed = s.pattern.observed;
    s.workflow.imported_pattern = std::make_shared<const Pattern>(s.pattern);
    s.comparisonPatterns = {s.pattern};
    s.projectMetadata = {{"wide", std::int64_t(9007199254740993ll)},
                         {"integer", std::int64_t(1)},
                         {"unsigned_max", UINT64_MAX},
                         {"floating", 1.0},
                         {"undefined", MetadataValue{}},
                         {"null", nullptr},
                         {"zero", -0.0}};
    s.projectMetadata["literal_reference"] =
        MetadataObject{{"$ref", "uninterpreted"}, {"$array", MetadataObject{{"any", nullptr}}}};
    auto frozen = snapshot(std::make_shared<const ProductState>(s));
    auto view = std::make_shared<ProjectViewState>();
    view->axis = "d_spacing";
    view->bounds = std::array{.1, 10., -1., 100.};
    view->result_order = {"B", "A"};
    view->layers["background"] = "visible";
    view->curves["observed"].pen.rgba = 0x12345678;
    frozen.view = view;
    auto write = [&] { return take(QpfProjectWriter{}.writeAtomic(path, frozen)); };
    const auto id = write();
    auto doc = take(QpfProjectReader{}.read(path, SearchBackend::cpp));
    check(doc.state.pattern.id.value == UINT64_MAX && std::signbit(doc.state.pattern.observed[0]) &&
          doc.state.comparisonPatterns.size() == 1 && doc.view == *view);
    check(doc.state.projectMetadata == s.projectMetadata &&
          std::signbit(doc.state.projectMetadata.at("zero").toDouble()));
    std::size_t blocks = 0;
    editDatabase(path, [&](QSqlDatabase &db) {
        QSqlQuery q(db);
        check(q.exec("SELECT count(*) FROM blocks"));
        check(q.next());
        blocks = q.value(0).toULongLong();
        check(q.exec("PRAGMA application_id"));
        check(q.next() && q.value(0).toLongLong() == 0x51504650);
    });
    check(blocks > 0 && blocks < 6);
    check(!QFileInfo::exists(QString::fromStdString(path) + "-wal") &&
          !QFileInfo::exists(QString::fromStdString(path) + "-journal"));
    changeManifest(path, [](auto &m) {
        m["future"] = qpf::Value::Array{qpf::Undefined{}, nullptr, std::uint64_t(UINT64_MAX), -0.0};
        m["minor"] = std::uint64_t(7);
    });
    changeObject(path, "pattern", [](auto &m, const auto &) {
        m["future"] = qpf::Value::Map{
            {"integer", std::uint64_t(UINT64_MAX)}, {"null", nullptr}, {"undefined", qpf::Undefined{}}};
    });
    doc = take(QpfProjectReader{}.read(path, SearchBackend::cpp));
    frozen.extensions = doc.extensions;
    frozen.identity = doc.identity;
    write();
    auto twice = take(QpfProjectReader{}.read(path, SearchBackend::cpp));
    check(twice.identity == id);
    qpf::Budget manifestBudget;
    auto savedManifest = qpf::parse(qpf::bytes(twice.extensions->manifest_fields), manifestBudget);
    check(savedManifest.as<qpf::Value::Map>().contains("future") &&
          qpf::unsignedValue(savedManifest.at("minor"), "minor") == 7);
    bool foundFuture = false;
    for (const auto &[key, o] : twice.extensions->objects)
        if (o.kind == "pattern") {
            qpf::Budget b;
            auto m = qpf::parse(qpf::bytes(o.payload), b).as<qpf::Value::Map>();
            foundFuture |= m.contains("future");
        }
    check(foundFuture);
    auto limited = QpfProjectReader{}.read(path, SearchBackend::cpp, {64, 64});
    check(!limited && limited.error().reason == "project_resource_limit");
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp, {2ull * 1024 * 1024 * 1024, 1}));
    const auto original = take(QtTextFiles{}.read(path));
    for (int v : {1, 2, 3}) {
        QFile f(QString::fromStdString(path));
        check(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(("{\"version\":" + std::to_string(v) + "}").c_str());
        f.close();
        auto old = QpfProjectReader{}.read(path, SearchBackend::cpp);
        check(!old && old.error().reason == "unsupported_project_format");
    }
    take(QtTextFiles{}.writeAtomic(path, original));
    changeManifest(path, [](auto &m) { m["required_features"] = qpf::Value::Array{"unknown_required"}; });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    changeManifest(path, [](auto &m) { m["major"] = std::uint64_t(2); });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    changeObject(
        path, "pattern", [](auto &m, const auto &) { m["source_id"] = "corrupted"; }, false);
    auto corrupt = QpfProjectReader{}.read(path, SearchBackend::cpp);
    check(!corrupt && corrupt.error().reason == "project_checksum_failed");
    write();
    changeObject(path, "product_state", [](auto &m, const auto &id) {
        m["pattern"] = qpf::Value::Map{{"$ref", id}};
    });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    changeObject(path, "product_state", [](auto &m, const auto &) {
        m["pattern"] = qpf::Value::Map{{"$ref", "missing"}};
    });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    changeObject(path, "pattern", [](auto &m, const auto &) {
        auto desc = m.at("observed").at("$array").template as<qpf::Value::Map>();
        desc["shape"] = qpf::Value::Array{std::uint64_t(1)};
        m["observed"] = qpf::Value::Map{{"$array", desc}};
    });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    editDatabase(path, [&](QSqlDatabase &db) {
        QSqlQuery q(db);
        check(q.exec("UPDATE blocks SET payload=zeroblob(byte_length)"));
    });
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
    write();
    QFile truncated(QString::fromStdString(path));
    check(truncated.open(QIODevice::ReadWrite));
    check(truncated.resize(120));
    truncated.close();
    check(!QpfProjectReader{}.read(path, SearchBackend::cpp));
}
static void workspaceTransactions() {
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + "/workspace.qpf").toStdString();
    Workspace workspace;
    take(saveProjectInput(workspace, QpfProjectWriter{}, path));
    check(!workspace.projectModified());
    const auto source = workspace.session().snapshot();
    auto task = take(workspace.searches().begin());
    auto view = *workspace.projectView();
    view.offset = 42;
    take(workspace.updateProjectView(view));
    check(workspace.session().snapshot() == source && workspace.projectModified());
    check(!saveProjectInput(workspace, QpfProjectWriter{}, path) &&
          !openProjectInput(workspace, QpfProjectReader{}, path));
    auto completed = workspace.searches().complete(
        task.id, Result<PreparedSearchResult>{PreparedSearchResult{source, task.id, 0, *source}});
    check(bool(completed));
    take(saveProjectInput(workspace, QpfProjectWriter{}, path));
    check(!workspace.projectModified());
    bool observed = false;
    auto subscription = workspace.session().subscribe([&](ProductHandle) {
        observed = true;
        check(workspace.projectView()->offset == 42);
    });
    view.offset = 7;
    take(workspace.updateProjectView(view));
    take(openProjectInput(workspace, QpfProjectReader{}, path));
    check(observed && workspace.projectView()->offset == 42 && !workspace.projectModified());
    struct FailingReader : ProjectSourceReader {
        Result<ProjectDocument> read(const std::string &, SearchBackend,
                                     const ProjectReadOptions &) const override {
            return Result<ProjectDocument>{Error{ErrorCode::invalid_input, "project", "controlled failure"}};
        }
    } failed;
    const auto before = workspace.session().snapshot();
    const auto beforeView = workspace.projectView();
    check(!openProjectInput(workspace, failed, path));
    check(workspace.session().snapshot() == before && workspace.projectView() == beforeView);
    workspace.session().beginPreprocessingEdit();
    check(!saveProjectInput(workspace, QpfProjectWriter{}, path));
    workspace.session().cancelPreprocessingEdit(before);
    workspace.searches().close();
    check(!openProjectInput(workspace, QpfProjectReader{}, path));

    struct Defaults : DefaultSettingsStore {
        mutable int reads = 0;
        bool unavailable = false;
        Result<ProjectSettings> load() const override {
            ++reads;
            if (unavailable)
                return Result<ProjectSettings>{Error{ErrorCode::missing_data, "defaults", "unavailable"}};
            return Result<ProjectSettings>{ProjectSettings{}};
        }
        Status save(const ProjectSettings &) override { return Status{std::monostate{}}; }
    } defaults;
    Workspace independent(SearchBackend::cpp, defaultAlgorithms(), &defaults);
    defaults.reads = 0;
    defaults.unavailable = true;
    take(openProjectInput(independent, QpfProjectReader{}, path));
    check(defaults.reads == 0);
    const auto independentSource = independent.session().snapshot();
    struct FailingWriter : ProjectTargetWriter {
        Result<ProjectIdentity> writeAtomic(const std::string &, const ProjectSaveSnapshot &) const override {
            return Result<ProjectIdentity>{
                Error{ErrorCode::invalid_input, "project", "controlled write failure"}};
        }
    } failingWriter;
    check(!saveProjectInput(independent, failingWriter, path) && !independent.projectModified() &&
          independent.session().snapshot() == independentSource);
    struct StaleReader : ProjectSourceReader {
        Workspace &workspace;
        explicit StaleReader(Workspace &w) : workspace(w) {}
        Result<ProjectDocument> read(const std::string &p, SearchBackend backend,
                                     const ProjectReadOptions &options) const override {
            auto loaded = QpfProjectReader{}.read(p, backend, options);
            auto view = *workspace.projectView();
            view.offset += 1;
            take(workspace.updateProjectView(view));
            return loaded;
        }
    } stale(independent);
    auto rejected = openProjectInput(independent, stale, path);
    check(!rejected && rejected.error().reason == "stale_project" &&
          independent.session().snapshot() == independentSource);
    auto exhaustedState = *independentSource;
    exhaustedState.revision = UINT64_MAX;
    ProductSearchSession exhausted(exhaustedState);
    const auto exhaustedSource = exhausted.snapshot();
    check(!exhausted.restoreProject(exhaustedSource, *independentSource) &&
          exhausted.snapshot() == exhaustedSource);
}

static void chunkAndThreadBoundaries() {
    auto s = state();
    constexpr std::size_t n = 4 * 1024 * 1024 / 8 + 3;
    s.pattern.two_theta.clear();
    s.pattern.observed.clear();
    for (std::size_t i = 0; i < n; ++i) {
        s.pattern.two_theta.emplace_back(20 + .0001 * double(i));
        s.pattern.observed.push_back(double(i % 16));
    }
    s.pattern.original_observed = s.pattern.observed;
    s.workflow.imported_pattern = std::make_shared<const Pattern>(s.pattern);
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + "/chunked.qpf").toStdString();
    take(QpfProjectWriter{}.writeAtomic(path, snapshot(std::make_shared<const ProductState>(s))));
    bool boundary = false;
    editDatabase(path, [&](QSqlDatabase &db) {
        QSqlQuery q(db);
        check(q.exec("SELECT max(byte_length) FROM blocks") && q.next());
        boundary = q.value(0).toInt() == 4 * 1024 * 1024;
    });
    check(boundary);
    bool workerOk = false;
    std::thread reader([&] {
        auto loaded = QpfProjectReader{}.read(path, SearchBackend::cpp);
        workerOk = loaded && loaded.value().state.pattern.observed == s.pattern.observed;
    });
    reader.join();
    check(workerOk);
    for (const auto &name : QSqlDatabase::connectionNames())
        check(!name.startsWith("qualpf_project_"));
}

static ProductState portableFixture() {
    ProductState s;
    s.projectSettings.emplace();
    s.pattern.id = {UINT64_MAX};
    s.pattern.source_id = "qpf-cross-platform";
    s.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{11}, TwoThetaDegrees{12}};
    s.pattern.observed = {-0.0, .25, 65536.125};
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.radiation.lines = {{WavelengthAngstrom{1.5}, 1}};
    s.pattern.source_precision = SourcePrecision::float64;
    s.projectMetadata = {{"wide", std::int64_t(9007199254740993ll)},
                         {"integer", std::int64_t(1)},
                         {"unsigned_max", UINT64_MAX},
                         {"floating", 1.0},
                         {"undefined", MetadataValue{}},
                         {"null", nullptr}};
    return s;
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    if (argc >= 3 && std::string(argv[1]) == "--write-fixture") {
        take(QpfProjectWriter{}.writeAtomic(
            argv[2], snapshot(std::make_shared<const ProductState>(portableFixture()))));
        return 0;
    }
    if (argc >= 3 &&
        (std::string(argv[1]) == "--read-fixture" || std::string(argv[1]) == "--resave-fixture")) {
        auto doc = take(QpfProjectReader{}.read(argv[2], SearchBackend::cpp));
        const auto expected = portableFixture();
        check(doc.state.pattern.id.value == UINT64_MAX &&
              doc.state.pattern.observed == expected.pattern.observed &&
              std::signbit(doc.state.pattern.observed[0]) &&
              doc.state.projectMetadata == expected.projectMetadata);
        if (std::string(argv[1]) == "--resave-fixture") {
            check(argc == 4);
            take(QpfProjectWriter{}.writeAtomic(argv[3], {std::make_shared<const ProductState>(doc.state),
                                                          std::make_shared<const ProjectViewState>(doc.view),
                                                          doc.identity, doc.extensions, 0}));
        }
        std::cout << "PASS: portable independent QPF fixture\n";
        return 0;
    }
    protocolAndFailures();
    workspaceTransactions();
    chunkAndThreadBoundaries();
    preprocessingRoundtrip();
    csmRoundtrip();
    pfsm(false, false);
    pfsm(true, true);
    pfsm(true, false, true);
    pfsm(true, false, true, true);
    std::cout << "PASS: QPF 1.0 workflow evidence, unknown fields, atomic save, "
                 "PFSM continuation and database admission\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
