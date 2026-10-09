#include "gui/database/databasebuilder.h"
#include <QObject>
#include <QApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QProgressDialog>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
using namespace qualpf::application;

bool DatabaseBuilder::buildPdfDatabase(DatabaseBuildUseCases &useCases, const QString &basePath,
                                       const QString &pdf2FilePath,
                                       ProgressFn progress)
{
    auto build = useCases.preparePdf(basePath.toStdString(), pdf2FilePath.toStdString());
    return build && build->run(std::move(progress));
}

bool DatabaseBuilder::buildPdfDatabase(DatabaseBuildUseCases &useCases, const QString &basePath,
                                       const QString &pdf2FilePath,
                                       QWidget *parent,
                                       bool *outCancelled)
{
    auto build = useCases.preparePdf(basePath.toStdString(), pdf2FilePath.toStdString());
    if (!build) return false;

    // Use total record count as progress bar maximum (each record is 80 bytes).
    const qint64 fileSize   = build->size();
    const int    totalSteps = (fileSize > 0) ? int(fileSize / 80) : 0;

    QProgressDialog dlg(QObject::tr("Building database, please wait…"),
                        QObject::tr("Cancel"), 0, totalSteps, parent);
    dlg.setWindowTitle(QObject::tr("Creating Database"));
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setMinimumDuration(0);
    dlg.setMinimumWidth(350);
    dlg.setValue(0);
    dlg.show();
    {
        // Give the window manager time to map and paint the dialog before
        // starting the blocking parse below.
        QEventLoop loop;
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // Track cancellation via a local flag instead of dlg.wasCanceled() after close:
    // QProgressDialog::closeEvent() may call cancel() which sets the internal flag,
    // making wasCanceled() return true even when the user never clicked Cancel.
    bool wasCancelled = false;

    auto reportProgress = [&](int cards, std::int64_t bytesRead, std::int64_t) {
        dlg.setValue(int(bytesRead / 80));
        dlg.setLabelText(QObject::tr("%1 cards processed…").arg(cards));
        QApplication::processEvents();
        if (dlg.wasCanceled() && !wasCancelled) {
            wasCancelled = true;
            build->cancel();
        }
    };

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool parsed = build->run(reportProgress);
    QApplication::restoreOverrideCursor();

    // Use hide() instead of close() to avoid triggering closeEvent() -> cancel()
    // which would corrupt the wasCanceled() state.
    dlg.hide();

    if (outCancelled)
        *outCancelled = wasCancelled;

    return parsed && !wasCancelled;
}

CifBatchReport DatabaseBuilder::buildCifSources(
    DatabaseBuildUseCases &useCases, const QString &basePath,
    const std::vector<CifBatchRequest> &sources,
    CifBatchOptions options,
    CifBatchProgress progress)
{
    return useCases.buildCif(basePath.toStdString(), sources, std::move(options), std::move(progress));
}

bool DatabaseBuilder::buildCifDatabase(DatabaseBuildUseCases &useCases, const QString &basePath,
                                       const QString &cifDir,
                                       bool recursive,
                                       bool inorganicOnly,
                                       CifProgressFn progress,
                                       CifBuildStats *stats)
{
    using namespace qualpf::application;
    const auto found = useCases.discoverCif(cifDir.toStdString(), recursive);
    CifBatchReport report;
    if (const auto *error = std::get_if<CifImportFailure>(&found)) {
        report.fatal = *error;
    } else {
        CifBatchOptions options;
        options.inorganic_only = inorganicOnly;
        report = buildCifSources(useCases, basePath, std::get<std::vector<CifBatchRequest>>(found), options,
            [progress](const std::string &path, const CifBatchReport &r) {
                if (progress) progress(static_cast<int>(r.committed), static_cast<int>(r.skipped), static_cast<int>(r.failures.size()), QString::fromStdString(path));
            });
    }
    if (report.fatal) qWarning().noquote() << QString::fromStdString(useCases.describe(*report.fatal));
    const bool success = report.succeeded();
    if (stats) {
        stats->ok = static_cast<int>(report.committed);
        stats->skipped = static_cast<int>(report.skipped);
        stats->errors = static_cast<int>(report.failures.size());
        stats->report = std::move(report);
    }
    return success;
}

bool DatabaseBuilder::buildCifDatabase(DatabaseBuildUseCases &useCases, const QString &basePath,
                                       const QString &cifDir,
                                       bool recursive,
                                       QWidget *parent,
                                       bool *outCancelled)
{
    using namespace qualpf::application;
    const auto found = useCases.discoverCif(cifDir.toStdString(), recursive);
    if (const auto *error = std::get_if<CifImportFailure>(&found)) {
        QMessageBox::critical(parent, QObject::tr("CIF database"), QString::fromStdString(useCases.describe(*error)));
        return false;
    }
    DatabaseBuildCancellation stop;
    QProgressDialog progress(QObject::tr("Building CIF database…"), QObject::tr("Cancel"), 0, 0, parent);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QObject::connect(&progress, &QProgressDialog::canceled, [&] { stop.cancel(); });
    CifBatchOptions options;
    options.stop = stop.token();
    options.select_block = [parent](const std::string &source, const auto &candidates) -> std::optional<std::string> {
        const auto path = QString::fromStdString(source);
        QDialog dialog(parent);
        dialog.setObjectName("cifBlockSelectionDialog");
        dialog.setWindowTitle(QObject::tr("Select a structure block — %1").arg(QFileInfo(path).fileName()));
        QVBoxLayout layout(&dialog);
        QTableWidget table(static_cast<int>(candidates.size()), 7, &dialog);
        table.setObjectName("cifBlockCandidates");
        table.setHorizontalHeaderLabels({QObject::tr("Block"), QObject::tr("Cell"), QObject::tr("Space group"), QObject::tr("Sites"), QObject::tr("Completeness"), QObject::tr("ADP"), QObject::tr("Warnings")});
        table.setSelectionBehavior(QAbstractItemView::SelectRows);
        table.setSelectionMode(QAbstractItemView::SingleSelection);
        table.setEditTriggers(QAbstractItemView::NoEditTriggers);
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto &c = candidates[i];
            QStringList cell, adp, notices;
            for (const auto &v : c.cell_parameters) cell << (v ? QString::fromStdString(v->raw) : QStringLiteral("?"));
            for (const auto &[key, n] : c.adp_summary) adp << QString::fromStdString(key) + ": " + QString::number(n);
            if (c.rejection) notices << QString::fromStdString(c.rejection->message);
            if (c.provenance) for (const auto &notice : c.provenance->notices) notices << QString::fromStdString(notice);
            const QStringList fields{QString::fromStdString(c.block_id), cell.join(", "), QString::fromStdString(c.normalized_identifier), QString::number(c.atom_site_count), QString::fromUtf8(cifClassificationName(c.classification).data()), adp.join(", "), notices.join("; ")};
            for (int j = 0; j < fields.size(); ++j) {
                auto *item = new QTableWidgetItem(fields[j]);
                item->setToolTip(fields[j]);
                if (!c.importable) item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
                table.setItem(static_cast<int>(i), j, item);
            }
        }
        table.resizeColumnsToContents();
        table.horizontalHeader()->setStretchLastSection(true);
        layout.addWidget(&table);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons.button(QDialogButtonBox::Ok)->setText(QObject::tr("Import selected block"));
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        QObject::connect(&table, &QTableWidget::itemSelectionChanged, [&] {
            const auto row = table.currentRow();
            buttons.button(QDialogButtonBox::Ok)->setEnabled(row >= 0 && !table.selectedItems().isEmpty() && candidates[static_cast<std::size_t>(row)].importable);
        });
        QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout.addWidget(&buttons);
        dialog.resize(1100, 400);
        if (dialog.exec() != QDialog::Accepted || table.currentRow() < 0) return {};
        return candidates[static_cast<std::size_t>(table.currentRow())].block_id;
    };
    const auto report = buildCifSources(useCases, basePath, std::get<std::vector<CifBatchRequest>>(found), options,
        [&](const std::string &, const CifBatchReport &r) {
            progress.setLabelText(QObject::tr("%1 cards committed (%2 rejected)…").arg(r.committed).arg(r.failures.size()));
            QApplication::processEvents();
        });
    progress.hide();
    if (outCancelled) *outCancelled = report.cancelled;
    if (report.fatal) QMessageBox::critical(parent, QObject::tr("CIF database"), QString::fromStdString(useCases.describe(*report.fatal)));
    else if (!report.failures.empty()) {
        QStringList details;
        for (const auto &failure : report.failures) details << QString::fromStdString(useCases.describe(failure));
        QMessageBox message(QMessageBox::Warning, QObject::tr("CIF database"), QObject::tr("%1 cards committed; %2 files rejected.").arg(report.committed).arg(report.failures.size()), QMessageBox::Ok, parent);
        message.setDetailedText(details.join("\n\n"));
        message.exec();
    }
    return report.succeeded();
}
