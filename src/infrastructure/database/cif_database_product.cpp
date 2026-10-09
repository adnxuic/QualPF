#include "cif_database_product.h"
#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>
#include <algorithm>

namespace qualpf::infrastructure {
using namespace application;
std::variant<std::vector<CifSourceRequest>, CifImportFailure>
cif_sources_in_directory(const QString &directory, bool recursive) {
    const QFileInfo root(directory);
    if (!root.isDir() || !root.isReadable())
        return CifImportFailure{"discovery",
                                "invalid_cif_directory",
                                directory.toStdString(),
                                {},
                                "directory",
                                "CIF directory does not exist or is not readable",
                                {},
                                {},
                                {},
                                {}};
    std::vector<CifSourceRequest> sources;
    QDirIterator iterator(directory, {QStringLiteral("*.cif")}, QDir::Files,
                          recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (iterator.hasNext()) {
        const auto path = iterator.next();
        sources.push_back({path, QFileInfo(path).baseName().toInt(), {}});
    }
    std::sort(sources.begin(), sources.end(), [](const auto &a, const auto &b) { return a.path < b.path; });
    return sources;
}

QString describe_cif_failure(const CifImportFailure &failure) {
    QStringList lines;
    lines << QString::fromStdString(failure.source + ": " + failure.code + " [" + failure.field + "] " +
                                    failure.message);
    if (!failure.core_reason.empty())
        lines << QString::fromStdString(failure.core_reason);
    for (const auto &candidate : failure.candidates) {
        QStringList cell, adp, warnings;
        for (const auto &value : candidate.cell_parameters)
            cell << (value ? QString::fromStdString(value->raw) : QStringLiteral("?"));
        for (const auto &[key, count] : candidate.adp_summary)
            adp << QString::fromStdString(key) + ":" + QString::number(count);
        if (candidate.rejection)
            warnings << QString::fromStdString(candidate.rejection->code + ": " +
                                               candidate.rejection->message);
        if (candidate.provenance)
            for (const auto &notice : candidate.provenance->notices)
                warnings << QString::fromStdString(notice);
        lines << QStringLiteral("block=%1; cell=%2; group=%3; sites=%4; completeness=%5; "
                                "importable=%6; ADP=%7; warnings=%8")
                     .arg(QString::fromStdString(candidate.block_id), cell.join(", "),
                          QString::fromStdString(candidate.normalized_identifier))
                     .arg(candidate.atom_site_count)
                     .arg(QString::fromUtf8(io::cif::classification_name(candidate.classification).data()))
                     .arg(candidate.importable)
                     .arg(adp.join(", "), warnings.join("; "));
    }
    return lines.join('\n');
}
} // namespace qualpf::infrastructure

namespace qualpf::infrastructure {
namespace {
CifProductReport qtReport(const application::CifBatchReport &r) {
    CifProductReport q;
    q.backend = r.backend;
    q.discovered = r.discovered;
    q.committed = r.committed;
    q.skipped = r.skipped;
    q.completed = r.completed;
    q.cancelled = r.cancelled;
    q.failures = r.failures;
    q.fatal = r.fatal;
    q.cpp = r.cpp;
    for (const auto &s : r.sources)
        q.sources.push_back(
            {QString::fromUtf8(s.path), s.identity, s.selected_block, s.source_sha256, s.structure_identity});
    return q;
}
} // namespace
CifProductReport build_cif_sources(const QString &base, const std::vector<CifSourceRequest> &requests,
                                   CifProductOptions options, CifProductProgress progress) {
    const auto backend = cif_database_backend_from_environment();
    if (const auto *error = std::get_if<application::CifImportFailure>(&backend)) {
        CifProductReport r;
        r.fatal = *error;
        return r;
    }
    std::vector<application::CifBatchRequest> inputs;
    inputs.reserve(requests.size());
    for (const auto &r : requests)
        inputs.push_back({r.path.toUtf8().toStdString(), r.identity, r.selection});
    application::CifBatchOptions policy;
    policy.inorganic_only = options.inorganic_only;
    policy.errors = options.errors;
    policy.stop = options.stop;
    if (options.select_block)
        policy.select_block = [select = std::move(options.select_block)](const std::string &path,
                                                                         const auto &blocks) {
            return select(QString::fromUtf8(path), blocks);
        };
    database::CifDatabaseWriter writer;
    FileCifSourceReader reader;
    const auto report = application::runCifBatch(
        writer, reader, base.toUtf8().toStdString(), inputs,
        std::get<application::CifDatabaseBackend>(backend), std::move(policy),
        progress ? application::CifBatchProgress(
                       [progress = std::move(progress)](const std::string &path, const auto &r) {
                           progress(QString::fromUtf8(path), qtReport(r));
                       })
                 : application::CifBatchProgress{},
        [](const auto &failure) { qWarning().noquote() << describe_cif_failure(failure); });
    return qtReport(report);
}
} // namespace qualpf::infrastructure
