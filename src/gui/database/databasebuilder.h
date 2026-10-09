#pragma once

#include <QString>
#include <functional>
#include "qualpf/application/database_build.h"

class QWidget;

// -----------------------------------------------------------------------
// DatabaseBuilder
//
// Static helpers to build Qualx databases from different sources.
// Shared between the command-line (main.cpp) and the GUI dialogs so that
// the construction logic lives in exactly one place.
// -----------------------------------------------------------------------
class DatabaseBuilder
{
public:
    // Progress callback for PDF-2: (cardsProcessed, bytesRead, totalBytes).
    using ProgressFn = std::function<void(int, qint64, qint64)>;

    // Progress callback for CIF: (ok, skipped, errors, currentFilePath).
    using CifProgressFn = std::function<void(int, int, int, const QString &)>;

    // Counters returned by the CLI CIF overload.
    struct CifBuildStats {
        int ok      = 0;
        int skipped = 0;
        int errors  = 0;
        qualpf::application::CifBatchReport report;
    };

    // Actual production entry for configured batches and directory/GUI wrappers.
    // Default/cpp and exact legacy token use the accepted C++ compatibility owner.
    static qualpf::application::CifBatchReport buildCifSources(
        qualpf::application::DatabaseBuildUseCases &, const QString &basePath,
        const std::vector<qualpf::application::CifBatchRequest> &sources,
        qualpf::application::CifBatchOptions options = {},
        qualpf::application::CifBatchProgress progress = {});

    // ---- PDF-2 ---------------------------------------------------------

    // Builds a PDF-2 (NBS*AIDS83) database at basePath.
    static bool buildPdfDatabase(qualpf::application::DatabaseBuildUseCases &, const QString &basePath,
                                 const QString &pdf2FilePath,
                                 ProgressFn progress = nullptr);

    // Same, but shows a modal QProgressDialog owned by parent.
    static bool buildPdfDatabase(qualpf::application::DatabaseBuildUseCases &, const QString &basePath,
                                 const QString &pdf2FilePath,
                                 QWidget *parent,
                                 bool *outCancelled = nullptr);

    // ---- CIF -----------------------------------------------------------

    // Builds a CIF database at basePath from *.cif files in cifDir.
    // inorganicOnly: skip cards whose calculated subfile is not inorganic.
    // progress: called for each file; pass nullptr for no reporting.
    // stats: if non-null, filled with ok/skipped/error counts.
    static bool buildCifDatabase(qualpf::application::DatabaseBuildUseCases &, const QString &basePath,
                                 const QString &cifDir,
                                 bool recursive,
                                 bool inorganicOnly,
                                 CifProgressFn progress = nullptr,
                                 CifBuildStats *stats = nullptr);

    // Same, but shows a modal QProgressDialog owned by parent.
    // inorganicOnly is always false in this overload.
    static bool buildCifDatabase(qualpf::application::DatabaseBuildUseCases &, const QString &basePath,
                                 const QString &cifDir,
                                 bool recursive,
                                 QWidget *parent,
                                 bool *outCancelled = nullptr);

};
