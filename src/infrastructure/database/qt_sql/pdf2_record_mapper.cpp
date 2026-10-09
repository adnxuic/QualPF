#include "pdf2_record_mapper.h"
#include <QSet>
#include <algorithm>
namespace qualpf::infrastructure {
QString Pdf2RecordMapper::buildDvalString(const QVector<Pdf2Peak> &peaks, int &ndOut) {
    QString result;
    ndOut = 0;
    for (int i = 0; i < peaks.size(); ++i) {
        if (i > 0 && peaks[i].d == peaks[i - 1].d && peaks[i].intensity == peaks[i - 1].intensity)
            continue; // skip duplicate
        result += QString::asprintf("%f,", double(peaks[i].d));
        ++ndOut;
    }
    return result;
}

QString Pdf2RecordMapper::buildIvalString(const QVector<Pdf2Peak> &peaks) {
    QString result;
    for (int i = 0; i < peaks.size(); ++i) {
        if (i > 0 && peaks[i].d == peaks[i - 1].d && peaks[i].intensity == peaks[i - 1].intensity)
            continue;
        result += QString::asprintf("%f,", double(peaks[i].intensity));
    }
    return result;
}

QVector<Pdf2Peak> Pdf2RecordMapper::selectTopPeaks(const QVector<Pdf2Peak> &peaks, int n) {
    QVector<Pdf2Peak> unique;
    unique.reserve(peaks.size());
    for (int i = 0; i < peaks.size(); ++i) {
        if (i > 0 && peaks[i].d == peaks[i - 1].d && peaks[i].intensity == peaks[i - 1].intensity)
            continue;
        unique.append(peaks[i]);
    }

    // Sort by intensity descending — the single shared sort.
    // The d-sort is NOT applied here: each formatter (buildBestDval / buildTopDval)
    // applies it on its own subset after selecting the desired number of peaks.
    std::stable_sort(unique.begin(), unique.end(),
                     [](const Pdf2Peak &a, const Pdf2Peak &b) { return a.intensity > b.intensity; });

    unique.resize(qMin(unique.size(), n));
    return unique;
}

QString Pdf2RecordMapper::buildBestDval(const QVector<Pdf2Peak> &topPeaks) {
    // topPeaks is sorted by intensity — sort a local copy by d descending before formatting.
    QVector<Pdf2Peak> byD = topPeaks;
    std::sort(byD.begin(), byD.end(), [](const Pdf2Peak &a, const Pdf2Peak &b) { return a.d > b.d; });
    QString result;
    for (const Pdf2Peak &p : byD)
        result += QString::asprintf("%f,", double(p.d));
    return result;
}

QString Pdf2RecordMapper::buildTopDval(const QVector<Pdf2Peak> &topPeaks, int &nOut) {
    // topPeaks is sorted by intensity descending.
    // Store the d-values in that same intensity order — no d-sort.
    nOut = qMin(topPeaks.size(), kNbestd);
    QStringList parts;
    parts.reserve(nOut);
    for (int i = 0; i < nOut; ++i)
        parts.append(QString::asprintf("%f", double(topPeaks[i].d)));
    return parts.join(QLatin1Char(','));
}

QStringList Pdf2RecordMapper::extractElements(const QString &formula) {
    QString cleaned;
    cleaned.reserve(formula.size());

    for (int i = 0; i < formula.size(); ++i) {
        const QChar c = formula.at(i);
        const char a = c.toLatin1();

        if (c.isDigit() || a == '(' || a == ')' || a == '[' || a == ']' || a == ',' || a == '+' || a == '-' ||
            a == '.' || a == '!' || a == '/') {
            cleaned += QLatin1Char(' ');
            continue;
        }

        if (a == 'x' || a == 'z') {
            cleaned += QLatin1Char(' ');
            continue;
        }
        if (a == 'n') {
            const bool partOfSymbol = (i > 0 && formula.at(i - 1).isLetter());
            if (!partOfSymbol) {
                cleaned += QLatin1Char(' ');
                continue;
            }
        }

        cleaned += c;
    }

    const QStringList tokens = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QSet<QString> seen;
    QStringList result;
    for (const QString &tok : tokens) {
        if (!seen.contains(tok)) {
            seen.insert(tok);
            result.append(tok);
        }
    }
    return result;
}
} // namespace qualpf::infrastructure
