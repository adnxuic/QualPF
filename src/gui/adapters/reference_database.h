#pragma once
#include "qualpf/application/reference_database.h"
#include <QList>
#include <QString>
#include <QVector>
namespace qualpf::gui {
struct DatabaseEntry {
    bool inUse = false;
    QString name;
    int entries = 0;
    QString contentType;
    QString path;
};
struct CardDetails {
    // id table
    QString id, name, mineralName, chemicalFormula, spaceGroup, quality, rir;
    int     nrec = 0, nd = 0;
    QVector<double> dvalues;
    QVector<double> intensities;
    // info table
    QString authors, journal, journalVolume, pageStart, pageEnd, color, type;
    int     journalYear = 0, z = 0;
    double  crystalDensity = 0.0, volume = 0.0, density = 0.0;
    double  a = 0.0, b = 0.0, c = 0.0;
    double  alpha = 0.0, beta = 0.0, gamma = 0.0;
    double  muCuKa = 0.0;
    QVector<int> h, k, l, mul;
    bool    valid = false;
};



inline DatabaseEntry databaseView(const application::ReferenceDatabaseEntry &e) { return {e.inUse, QString::fromStdString(e.name), e.entries, QString::fromStdString(e.contentType), QString::fromStdString(e.path)}; }
inline std::vector<application::ReferenceDatabaseEntry> databaseValues(const QList<DatabaseEntry> &entries) { std::vector<application::ReferenceDatabaseEntry> out; for (const auto &e : entries) out.push_back({e.inUse,e.name.toStdString(),e.entries,e.contentType.toStdString(),e.path.toStdString()}); return out; }
inline QList<DatabaseEntry> databaseViews(const std::vector<application::ReferenceDatabaseEntry> &entries) { QList<DatabaseEntry> out; for (const auto &e : entries) out.append(databaseView(e)); return out; }
inline CardDetails cardDetailsView(const application::CardDetails &source) { CardDetails out;
    out.id = QString::fromStdString(source.id);
    out.name = QString::fromStdString(source.name);
    out.mineralName = QString::fromStdString(source.mineralName);
    out.chemicalFormula = QString::fromStdString(source.chemicalFormula);
    out.spaceGroup = QString::fromStdString(source.spaceGroup);
    out.quality = QString::fromStdString(source.quality);
    out.rir = QString::fromStdString(source.rir);
    out.authors = QString::fromStdString(source.authors);
    out.journal = QString::fromStdString(source.journal);
    out.journalVolume = QString::fromStdString(source.journalVolume);
    out.pageStart = QString::fromStdString(source.pageStart);
    out.pageEnd = QString::fromStdString(source.pageEnd);
    out.color = QString::fromStdString(source.color);
    out.type = QString::fromStdString(source.type);
    out.nrec = source.nrec;
    out.nd = source.nd;
    out.journalYear = source.journalYear;
    out.z = source.z;
    out.crystalDensity = source.crystalDensity;
    out.volume = source.volume;
    out.density = source.density;
    out.a = source.a;
    out.b = source.b;
    out.c = source.c;
    out.alpha = source.alpha;
    out.beta = source.beta;
    out.gamma = source.gamma;
    out.muCuKa = source.muCuKa;
    out.valid = source.valid;
    out.dvalues = QVector<double>(source.dvalues.begin(), source.dvalues.end());
    out.intensities = QVector<double>(source.intensities.begin(), source.intensities.end());
    out.h = QVector<int>(source.h.begin(), source.h.end());
    out.k = QVector<int>(source.k.begin(), source.k.end());
    out.l = QVector<int>(source.l.begin(), source.l.end());
    out.mul = QVector<int>(source.mul.begin(), source.mul.end());
    return out;
}
inline QList<QPair<QString,int>> namedCountViews(const application::NamedCounts &values) { QList<QPair<QString,int>> out; for (const auto &v : values) out.append({QString::fromStdString(v.first),v.second}); return out; }
} // namespace qualpf::gui
