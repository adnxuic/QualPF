#pragma once
#include "infrastructure/io/pdf2/pdf2reader.h"
namespace qualpf::infrastructure {
class Pdf2RecordMapper {
  public:
    // Extracts unique chemical element symbols from a PDF-2 formula string.
    // Mirrors the splitinfochemical + eliminaduplicati logic from the original C code:
    //   - removes digits, brackets, punctuation, and standalone 'n'/'x'/'z' variables
    //   - splits on whitespace and deduplicates
    static QStringList extractElements(const QString &formula);

    // Builds a comma-separated "%f," string of d-values sorted by d descending,
    // with duplicate (d, intensity) pairs removed.
    static QString buildDvalString(const QVector<Pdf2Peak> &peaks, int &ndOut);

    // Builds a comma-separated "%f," string of intensities in the same order as buildDvalString.
    static QString buildIvalString(const QVector<Pdf2Peak> &peaks);

    // Deduplicates peaks, sorts by intensity descending, takes the top n,
    // then sorts those by d descending.  Single shared sort used by both
    // buildBestDval and buildTopDval.
    static QVector<Pdf2Peak> selectTopPeaks(const QVector<Pdf2Peak> &peaks, int n);

    // Formats the pre-selected peaks as a comma-separated "%f," string (with trailing comma).
    static QString buildBestDval(const QVector<Pdf2Peak> &topPeaks);

    // Formats the first min(kNbestd, size) of the pre-selected peaks as a
    // comma-separated "%f" string (no trailing comma).
    // nOut is set to the actual number of formatted peaks.
    static QString buildTopDval(const QVector<Pdf2Peak> &topPeaks, int &nOut);

  private:
    static constexpr int kNbestd = 3;
};
} // namespace qualpf::infrastructure
