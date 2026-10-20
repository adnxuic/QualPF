#include "qpf_project.h"
#include "qpf_archive.h"
#include "quantification.h"
#include "qualpf/domain/validation.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QUuid>
#include <QVariant>
#include <functional>
#include <bit>

namespace qualpf::infrastructure {
namespace {
using namespace qpf;
using namespace application;
const std::map<QString, QString> tables{
    {"manifest", "CREATE TABLE manifest (id INTEGER PRIMARY KEY CHECK(id=1), payload BLOB NOT NULL, sha256 "
                 "TEXT NOT NULL)"},
    {"objects", "CREATE TABLE objects (id TEXT PRIMARY KEY NOT NULL, kind TEXT NOT NULL, version INTEGER NOT "
                "NULL, payload BLOB NOT NULL, sha256 TEXT NOT NULL)"},
    {"blocks", "CREATE TABLE blocks (sha256 TEXT PRIMARY KEY NOT NULL, byte_length INTEGER NOT NULL, payload "
               "BLOB NOT NULL)"}};
struct Connection {
    QString name = "qualpf_project_" + QUuid::createUuid().toString(QUuid::Id128);
    std::thread::id owner = std::this_thread::get_id();
    Connection(const QString &path, bool readonly) {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        if (readonly)
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open()) {
            const auto error = db.lastError().text().toStdString();
            db = QSqlDatabase{};
            QSqlDatabase::removeDatabase(name);
            invalid("project.file", error, "project_open_failed");
        }
    }
    QSqlDatabase db() const {
        Q_ASSERT(owner == std::this_thread::get_id());
        return QSqlDatabase::database(name, false);
    }
    ~Connection() {
        Q_ASSERT(owner == std::this_thread::get_id());
        {
            auto d = db();
            d.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
};
void sql(QSqlQuery &q, const QString &statement) {
    if (!q.exec(statement))
        invalid("project.sqlite", q.lastError().text().toStdString(), "project_sql_failed");
}
void execute(QSqlQuery &q) {
    if (!q.exec())
        invalid("project.sqlite", q.lastError().text().toStdString(), "project_sql_failed");
}
void validateRecords(const ProductState &s) {
    take(validateWorkflowState(s.workflow));
    if (s.workflow.last_attempt && s.workflow.last_attempt->status == WorkflowStepStatus::running)
        invalid("project.workflow", "A running attempt cannot be restored");
    if (s.workflow.imported_pattern && !s.workflow.imported_pattern->two_theta.empty()) {
        const auto errors = validate(*s.workflow.imported_pattern);
        if (!errors.empty())
            throw Failure{errors.front()};
    }
    for (const auto &r : s.workflow.records) {
        ProjectSettings settings;
        settings.profile(r.workflow) = r.parameters;
        take(validateProjectSettings(settings));
        if (r.input) {
            const auto errors = validate(*r.input);
            if (!errors.empty())
                throw Failure{errors.front()};
        }
        if (r.status == WorkflowStepStatus::running)
            invalid("project.workflow", "A running step cannot be restored");
        if (r.status != WorkflowStepStatus::completed && r.status != WorkflowStepStatus::skipped)
            continue;
        if (!r.input || r.input->id != s.pattern.id || r.input->source_id != s.pattern.source_id)
            invalid("project.workflow.input");
        if (r.step == WorkflowStep::background && !s.pattern.background)
            invalid("project.workflow.background");
        if (r.step == WorkflowStep::initial_peaks && !s.preprocessing.initial_peak_usage)
            invalid("project.workflow.initial_peaks");
        if (r.step == WorkflowStep::fwhm && !s.preprocessing.fwhm_usage)
            invalid("project.workflow.fwhm");
        if (r.step == WorkflowStep::peaks && !s.peaks.valid &&
            !(r.manual && r.actual_parameters.contains("cleared") &&
              r.actual_parameters.at("cleared").toBool()))
            invalid("project.workflow.peaks");
        if (r.step == WorkflowStep::search && r.workflow == WorkflowId::pfsm && !s.pfsm)
            invalid("project.workflow.pfsm");
    }
}
void validatePowderProvenance(const ProductState &s) {
    if (!s.powderCifProvenance)
        return;
    const auto &p = *s.powderCifProvenance;
    const auto require = [](bool ok) {
        if (!ok)
            invalid("project.powder_cif_provenance", "Invalid frozen powder CIF source evidence",
                    "invalid_provenance");
    };
    require(p.source && p.source->source_id == s.pattern.source_id);
    const auto &raw = p.source->bytes;
    const auto n = raw.size();
    require(digest(QByteArray(raw.data(), qsizetype(raw.size()))) == p.source->sha256);
    const auto span = [&](const auto &v) { require(v.begin <= v.end && v.end <= n); };
    const auto field = [&](const auto &f) {
        span(f.span);
        require(f.location.offset == f.span.begin && f.location.line > 0 && f.location.line <= n + 1 &&
                f.location.column > 0 && f.location.column <= n + 1);
        if (f.row)
            require(*f.row <= n);
    };
    span(p.block_span);
    require(!p.block_id.empty() && p.block_index <= n && !p.axis_source.empty() &&
            !p.intensity_source.empty());
    require(p.axis_fields.size() == (p.axis_source == "generated" ? 2u : 1u));
    for (const auto &f : p.axis_fields)
        field(f);
    field(p.intensity_field);
    require(p.intensity_field.tag == p.intensity_source);
    require(p.radiation.origin && (*p.radiation.origin == io::powder_cif::RadiationOrigin::source_file ||
                                   *p.radiation.origin == io::powder_cif::RadiationOrigin::user_supplied));
    require(p.radiation.wavelength_bits.has_value());
    const auto wavelength = std::bit_cast<float>(*p.radiation.wavelength_bits);
    require(std::isfinite(wavelength) && wavelength > 0);
    if (p.radiation.kind)
        require(*p.radiation.kind == RadiationKind::x_ray || *p.radiation.kind == RadiationKind::neutron);
    require((*p.radiation.origin == io::powder_cif::RadiationOrigin::source_file) ==
            p.radiation.source_field.has_value());
    if (p.radiation.source_field)
        field(*p.radiation.source_field);
    for (const auto &notice : p.notices) {
        span(notice.span);
        require(notice.block_index <= n);
    }
}
Value quantitative(ProductHandle state, Archive &a) {
    const auto q = prepareQuantification(state, quantificationInputs(state->phases));
    Writer writer{a, "quantification", {}};
    writer.fields["method"] = q.method == QuantificationMethod::classic_rir ? "classic_rir" : "pfsm_rir";
    writer.fields["availability"] = q.availability == QuantificationAvailability::available ? "available"
                                    : q.availability == QuantificationAvailability::empty   ? "empty"
                                                                                            : "unavailable";
    writer("percentages", q.percentages());
    return putObject("quantification", std::move(writer));
}
void checkQuantitative(const Value &ref, ProductHandle state, Archive &a) {
    const auto [id, obj] = getObject(ref, a, "quantification");
    const auto q = prepareQuantification(state, quantificationInputs(state->phases));
    const auto method = q.method == QuantificationMethod::classic_rir ? "classic_rir" : "pfsm_rir";
    const auto availability = q.availability == QuantificationAvailability::available ? "available"
                              : q.availability == QuantificationAvailability::empty   ? "empty"
                                                                                      : "unavailable";
    if (obj->fields.at("method").as<std::string>() != method ||
        obj->fields.at("availability").as<std::string>() != availability ||
        decode<std::vector<double>>(obj->fields.at("percentages"), a, "quantification.percentages") !=
            q.percentages())
        invalid("project.quantification", "Saved quantification disagrees with frozen evidence");
    a.visiting.erase(id);
}
void validateGraph(const Archive &a, std::set<std::string> &objects, std::set<std::string> &blocks) {
    std::set<std::string> visiting;
    std::function<void(const Value &, unsigned)> visit;
    visit = [&](const Value &v, unsigned depth) {
        if (depth > a.budget.options.maximum_depth)
            invalid("project.reference", "Reference depth limit exceeded", "project_resource_limit");
        if (auto m = std::get_if<Value::Map>(&v.data)) {
            if (m->contains("$metadata")) {
                if (m->size() != 1)
                    invalid("project.metadata", "Invalid literal metadata wrapper");
            } else if (m->contains("$ref")) {
                const auto &id = v.at("$ref").as<std::string>();
                if (m->size() != 1 || visiting.contains(id))
                    invalid("project.reference", "Cyclic or invalid object reference");
                if (objects.contains(id))
                    return;
                auto found = a.objects.find(id);
                if (found == a.objects.end())
                    invalid("project.reference", "Dangling object reference");
                objects.insert(id);
                visiting.insert(id);
                visit(found->second.fields, depth + 1);
                visiting.erase(id);
            } else if (m->contains("$array")) {
                const auto &d = v.at("$array");
                for (const auto &b : d.at("blocks").as<Value::Array>()) {
                    const auto &hash = b.as<std::string>();
                    if (!a.blocks.contains(hash))
                        invalid("project.blocks", "Missing array block");
                    blocks.insert(hash);
                }
            } else
                for (const auto &[key, e] : *m)
                    visit(e, depth + 1);
        } else if (auto values = std::get_if<Value::Array>(&v.data))
            for (const auto &e : *values)
                visit(e, depth + 1);
    };
    visit(a.manifest, 0);
    // Preserve standalone optional domain objects, including their dependencies.
    for (const auto &[id, o] : a.objects)
        if (!a.decoded.contains(id))
            visit(reference(id), 0);
}
bool timestamp(const std::string &s) {
    return !s.empty() && s.back() == 'Z' &&
           QDateTime::fromString(QString::fromStdString(s), Qt::ISODateWithMs).isValid();
}
ProjectIdentity identity(const Value &m) {
    ProjectIdentity out{m.at("project_uuid").as<std::string>(), m.at("created_utc").as<std::string>()};
    const auto uuid = QUuid(QString::fromStdString(out.uuid));
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces).toStdString() != out.uuid ||
        !timestamp(out.created_utc) || !timestamp(m.at("saved_utc").as<std::string>()))
        invalid("project.manifest", "Invalid project identity or UTC timestamp");
    return out;
}
Archive load(const std::string &path, const ProjectReadOptions &options) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly))
        invalid("project.file", file.errorString().toStdString(), "project_read_failed");
    if (file.read(16) != QByteArray("SQLite format 3\0", 16))
        invalid("project.format", "Only QPF 1.x projects are supported; QXP projects cannot be opened",
                "unsupported_project_format");
    file.close();
    Archive a;
    a.budget.options = options;
    Connection conn(QString::fromStdString(path), true);
    QSqlQuery q(conn.db());
    sql(q, "PRAGMA query_only=ON");
    sql(q, "PRAGMA trusted_schema=OFF");
    if (!conn.db().transaction())
        invalid("project.sqlite", "Cannot start project read snapshot");
    sql(q, "PRAGMA application_id");
    if (!q.next() || q.value(0).toLongLong() != 0x51504650)
        invalid("project.format", "Not a QPF project", "unsupported_project_format");
    sql(q, "PRAGMA user_version");
    if (!q.next() || q.value(0).toLongLong() != 1)
        invalid("project.version", "Unsupported QPF storage version", "unsupported_project_version");
    sql(q, "SELECT name,type,sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'");
    std::set<QString> found;
    while (q.next()) {
        const auto name = q.value(0).toString();
        auto expected = tables.find(name);
        if (expected == tables.end() || q.value(1).toString() != "table" ||
            q.value(2).toString() != expected->second)
            invalid("project.schema", "Unsupported QPF database schema");
        found.insert(name);
    }
    if (found.size() != tables.size())
        invalid("project.schema", "Incomplete QPF schema");
    for (const auto &statement :
         {"SELECT count(*) FROM manifest WHERE typeof(payload)!='blob' OR typeof(sha256)!='text'",
          "SELECT count(*) FROM objects WHERE typeof(id)!='text' OR typeof(kind)!='text' OR "
          "typeof(version)!='integer' OR typeof(payload)!='blob' OR typeof(sha256)!='text'",
          "SELECT count(*) FROM blocks WHERE typeof(sha256)!='text' OR typeof(byte_length)!='integer' OR "
          "typeof(payload)!='blob'"}) {
        sql(q, QString::fromLatin1(statement));
        if (!q.next() || q.value(0).toLongLong() != 0)
            invalid("project.schema", "Incorrect SQLite storage class");
    }
    sql(q, "PRAGMA quick_check");
    if (!q.next() || q.value(0).toString() != "ok" || q.next())
        invalid("project.sqlite", "Damaged project container");
    sql(q, "SELECT id,length(payload),sha256 FROM manifest");
    if (!q.next() || q.value(0).toInt() != 1)
        invalid("project.manifest");
    a.budget.charge(q.value(1).toULongLong());
    const auto manifestHash = q.value(2).toString().toStdString();
    if (q.next())
        invalid("project.manifest");
    sql(q, "SELECT payload FROM manifest WHERE id=1");
    if (!q.next())
        invalid("project.manifest");
    const auto manifest = q.value(0).toByteArray();
    if (digest(manifest) != manifestHash)
        invalid("project.hash", "Manifest checksum mismatch", "project_checksum_failed");
    a.manifest = parse(manifest, a.budget);
    if (a.manifest.at("format").as<std::string>() != "qualpf.project" ||
        unsignedValue(a.manifest.at("major"), "project.version") != 1)
        invalid("project.version", "Unsupported QPF major version", "unsupported_project_version");
    unsignedValue(a.manifest.at("minor"), "project.version");
    identity(a.manifest);
    a.manifest.at("writer").as<std::string>();
    for (const auto &f : a.manifest.at("required_features").as<Value::Array>())
        if (f.as<std::string>() != "typed_arrays" && f.as<std::string>() != "frozen_analysis" &&
            f.as<std::string>() != "project_view")
            invalid("project.features", "Unsupported required QPF feature", "unsupported_project_feature");
    sql(q, "SELECT id,kind,version,length(payload),sha256 FROM objects");
    struct Header {
        std::string id, kind, hash;
        std::uint64_t version;
    };
    std::vector<Header> headers;
    while (q.next()) {
        a.budget.charge(sizeof(Header) + q.value(3).toULongLong());
        if (q.value(2).toLongLong() < 1)
            invalid("project.object.version");
        headers.push_back({q.value(0).toString().toStdString(), q.value(1).toString().toStdString(),
                           q.value(4).toString().toStdString(), q.value(2).toULongLong()});
    }
    QSqlQuery object(conn.db());
    object.prepare("SELECT payload FROM objects WHERE id=?");
    for (const auto &h : headers) {
        object.bindValue(0, QString::fromStdString(h.id));
        execute(object);
        if (!object.next())
            invalid("project.object");
        auto fields = parse(object.value(0).toByteArray(), a.budget);
        fields.as<Value::Map>();
        Object o{h.kind, h.version, std::move(fields)};
        if (objectDigest(h.id, o) != h.hash)
            invalid("project.hash", "Object checksum mismatch", "project_checksum_failed");
        a.objects.emplace(h.id, std::move(o));
    }
    sql(q, "SELECT sha256,byte_length,length(payload) FROM blocks");
    std::vector<std::pair<std::string, std::uint64_t>> blockHeaders;
    while (q.next()) {
        const auto n = q.value(1).toLongLong();
        if (n <= 0 || n > 4 * 1024 * 1024 || n != q.value(2).toLongLong())
            invalid("project.blocks", "Invalid block length");
        a.budget.charge(std::uint64_t(n) + sizeof(blockHeaders.front()));
        blockHeaders.emplace_back(q.value(0).toString().toStdString(), std::uint64_t(n));
    }
    QSqlQuery block(conn.db());
    block.prepare("SELECT payload FROM blocks WHERE sha256=?");
    for (const auto &[hash, n] : blockHeaders) {
        block.bindValue(0, QString::fromStdString(hash));
        execute(block);
        if (!block.next())
            invalid("project.blocks");
        auto b = block.value(0).toByteArray();
        if (std::uint64_t(b.size()) != n || digest(b) != hash)
            invalid("project.hash", "Data block checksum mismatch", "project_checksum_failed");
        a.blocks.emplace(hash, std::move(b));
    }
    return a;
}
void store(const QString &path, const Archive &a) {
    Connection conn(path, false);
    QSqlQuery q(conn.db());
    sql(q, "PRAGMA journal_mode=DELETE");
    sql(q, "PRAGMA synchronous=FULL");
    sql(q, "PRAGMA application_id=1364215376");
    sql(q, "PRAGMA user_version=1");
    if (!conn.db().transaction())
        invalid("project.sqlite", "Cannot start project transaction");
    for (const auto &[name, ddl] : tables)
        sql(q, ddl);
    q.prepare("INSERT INTO manifest VALUES (1,?,?)");
    auto manifest = cbor(a.manifest);
    q.addBindValue(manifest);
    q.addBindValue(QString::fromStdString(digest(manifest)));
    execute(q);
    q.prepare("INSERT INTO objects VALUES (?,?,?,?,?)");
    for (const auto &[id, o] : a.objects) {
        q.bindValue(0, QString::fromStdString(id));
        q.bindValue(1, QString::fromStdString(o.kind));
        q.bindValue(2, qulonglong(o.version));
        q.bindValue(3, cbor(o.fields));
        q.bindValue(4, QString::fromStdString(objectDigest(id, o)));
        execute(q);
    }
    q.prepare("INSERT INTO blocks VALUES (?,?,?)");
    for (const auto &[hash, b] : a.blocks) {
        q.bindValue(0, QString::fromStdString(hash));
        q.bindValue(1, qlonglong(b.size()));
        q.bindValue(2, b);
        execute(q);
    }
    if (!conn.db().commit())
        invalid("project.sqlite", "Cannot commit project transaction");
}
} // namespace
Result<ProjectDocument> QpfProjectReader::read(const std::string &path, SearchBackend backend,
                                               const ProjectReadOptions &options) const try {
    auto a = load(path, options);
    ProjectDocument out;
    out.identity = identity(a.manifest);
    const auto [rootId, root] = getObject(a.manifest.at("root"), a, "project");
    out.state = decode<ProductState>(root->fields.at("analysis"), a, "analysis");
    out.view = decode<ProjectViewState>(root->fields.at("view"), a, "view");
    a.visiting.erase(rootId);
    out.state.backend = backend;
    out.state.restored_project = true;
    if (!out.state.projectSettings)
        invalid("project.settings", "QPF requires all four workflow profiles");
    take(restorePfsmEvidence(out.state));
    take(validateProductState(out.state));
    validateRecords(out.state);
    validatePowderProvenance(out.state);
    take(validateProjectView(out.view));
    checkQuantitative(root->fields.at("quantification"), std::make_shared<const ProductState>(out.state), a);
    std::set<std::string> reachable, blocks;
    validateGraph(a, reachable, blocks);
    auto extensions = std::make_shared<ProjectExtensions>();
    extensions->manifest_fields = bytes(cbor(a.manifest));
    for (const auto &[id, o] : a.objects)
        extensions->objects.emplace(id, ProjectOpaqueObject{o.kind, o.version, bytes(cbor(o.fields))});
    for (const auto &[hash, b] : a.blocks)
        extensions->blocks.emplace(hash, bytes(b));
    extensions->decoded_objects.assign(a.decoded.begin(), a.decoded.end());
    out.extensions = std::move(extensions);
    return Result<ProjectDocument>{std::move(out)};
} catch (const Failure &f) {
    return Result<ProjectDocument>{f.error};
} catch (const std::bad_alloc &) {
    return Result<ProjectDocument>{Error{ErrorCode::invalid_input, "project.size",
                                         "Project memory budget exhausted", "project_resource_limit"}};
} catch (const std::exception &e) {
    return Result<ProjectDocument>{
        Error{ErrorCode::invalid_input, "project", e.what(), "project_read_failed"}};
}

Result<ProjectIdentity> QpfProjectWriter::writeAtomic(const std::string &path,
                                                      const ProjectSaveSnapshot &source) const try {
    take(validateProjectSaveSource(source.state));
    if (!source.view)
        invalid("project.view");
    take(validateProjectView(*source.view));
    validateRecords(*source.state);
    validatePowderProvenance(*source.state);
    Archive a;
    if (source.extensions) {
        if (!source.extensions->manifest_fields.empty())
            a.manifest = parse(bytes(source.extensions->manifest_fields), a.budget);
        for (const auto &[id, o] : source.extensions->objects)
            a.objects.emplace(id, Object{o.kind, o.version, parse(bytes(o.payload), a.budget)});
        for (const auto &[hash, b] : source.extensions->blocks)
            a.blocks.emplace(hash, bytes(b));
    }
    // Explicit API callers may not yet have project profiles; make this new file
    // self-contained with typed constructor defaults, never global preferences.
    auto state = *source.state;
    if (!state.projectSettings)
        state.projectSettings.emplace();
    Writer root{a, "project", {}};
    root("analysis", state);
    root("view", *source.view);
    root.fields["quantification"] = quantitative(source.state, a);
    const auto rootRef = putObject("project", std::move(root));
    auto manifest =
        std::holds_alternative<Value::Map>(a.manifest.data) ? a.manifest.as<Value::Map>() : Value::Map{};
    ProjectIdentity id = source.identity;
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
    if (id.uuid.empty())
        id.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    if (id.created_utc.empty())
        id.created_utc = now;
    manifest["format"] = "qualpf.project";
    manifest["major"] = std::uint64_t(1);
    if (!manifest.contains("minor"))
        manifest["minor"] = std::uint64_t(0);
    manifest["project_uuid"] = id.uuid;
    manifest["created_utc"] = id.created_utc;
    manifest["saved_utc"] = now;
    manifest["writer"] = std::string("QualPF/") + QUALPF_PROJECT_WRITER_VERSION;
    manifest["required_features"] = Value::Array{"typed_arrays", "frozen_analysis", "project_view"};
    manifest["root"] = rootRef;
    a.manifest = std::move(manifest);
    identity(a.manifest);
    if (source.extensions)
        a.decoded.insert(source.extensions->decoded_objects.begin(),
                         source.extensions->decoded_objects.end());
    std::set<std::string> reachable, blocks;
    validateGraph(a, reachable, blocks);
    std::erase_if(a.objects, [&](const auto &e) { return !reachable.contains(e.first); });
    std::erase_if(a.blocks, [&](const auto &e) { return !blocks.contains(e.first); });
    const auto parent = QFileInfo(QString::fromStdString(path)).absolutePath();
    QTemporaryFile temporary(QDir(parent).filePath(".qualpf-project-XXXXXX.tmp"));
    if (!temporary.open())
        invalid("project.file", temporary.errorString().toStdString(), "project_open_failed");
    const auto tempPath = temporary.fileName();
    temporary.close();
    store(tempPath, a);
    take(QpfProjectReader{}.read(tempPath.toStdString(), source.state->backend));
    QFile input(tempPath);
    QSaveFile output(QString::fromStdString(path));
    output.setDirectWriteFallback(false);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        invalid("project.file", output.errorString().toStdString(), "project_open_failed");
    while (!input.atEnd()) {
        auto chunk = input.read(1024 * 1024);
        if (input.error() != QFileDevice::NoError || chunk.isEmpty() || output.write(chunk) != chunk.size()) {
            output.cancelWriting();
            invalid("project.file", "Failed writing project", "project_save_failed");
        }
    }
    if (!output.commit())
        invalid("project.file", output.errorString().toStdString(), "project_save_failed");
    return Result<ProjectIdentity>{std::move(id)};
} catch (const Failure &f) {
    return Result<ProjectIdentity>{f.error};
} catch (const std::bad_alloc &) {
    return Result<ProjectIdentity>{Error{ErrorCode::invalid_input, "project.size",
                                         "Project memory budget exhausted", "project_resource_limit"}};
} catch (const std::exception &e) {
    return Result<ProjectIdentity>{
        Error{ErrorCode::invalid_input, "project", e.what(), "project_save_failed"}};
}
} // namespace qualpf::infrastructure
