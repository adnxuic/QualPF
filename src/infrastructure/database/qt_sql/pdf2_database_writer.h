#pragma once

#include <QObject>
#include <QSqlQuery>
#include <QMap>
#include <QPair>
#include <QStringList>
#include "infrastructure/io/pdf2/pdf2reader.h"
#include "infrastructure/database/qt_sql/sql_schema_builder.h"

namespace qualpf::infrastructure {

// -----------------------------------------------------------------------
// Pdf2DatabaseWriter
//
// Receives Pdf2Card objects from Pdf2Reader and inserts them into the
// databases managed by SqlSchemaBuilder.
//
// Usage:
//   SqlSchemaBuilder db;
//   db.create("/path/to/mydb");
//
//   Pdf2Reader reader;
//   Pdf2DatabaseWriter populator(&db);
//
//   QObject::connect(&reader, &Pdf2Reader::cardReady,
//                    &populator, &Pdf2DatabaseWriter::onCardReady);
//   QObject::connect(&reader, &Pdf2Reader::finished,
//                    &populator, &Pdf2DatabaseWriter::onFinished);
//   reader.parse("/path/to/pdf2.dat");
// -----------------------------------------------------------------------
class Pdf2DatabaseWriter : public QObject
{
    Q_OBJECT

public:
    explicit Pdf2DatabaseWriter(SqlSchemaBuilder *db, QObject *parent = nullptr);
    bool succeeded() const noexcept { return !m_failed; }

public slots:
    void onCardReady(const Pdf2Card &card);
    void onFinished(int totalCards);

private:
    bool checked(bool success) { m_failed = m_failed || !success; return success; }
    void insertChemical(const Pdf2Card &card);
    void insertId(const Pdf2Card &card, const QVector<Pdf2Peak> &top6);
    void insertSubfiles(const Pdf2Card &card);
    void insertInfo(const Pdf2Card &card);
    void insertTop(const Pdf2Card &card, const QVector<Pdf2Peak> &top6);

    void populateSpgrStat();
    void accumulateInfoStat(const Pdf2Card &card);
    void populateInfoStat();
    void insertNumTable(QSqlDatabase &db, const QString &table,
                               const QMap<double,  QStringList> &data);
    void insertTxtTable(QSqlDatabase &db, const QString &table,
                               const QMap<QString, QStringList> &data);

    // Transaction helpers
    void beginTransaction();
    void commitTransaction();

    SqlSchemaBuilder *m_db;

    QSqlQuery m_chemQuery;    // prepared INSERT for chemical table
    QSqlQuery m_idQuery;      // prepared INSERT for id table
    QSqlQuery m_subQuery;     // prepared INSERT for subfiles table
    QSqlQuery m_infoQuery;    // prepared INSERT for info table
    QSqlQuery m_topQuery;     // prepared INSERT for top table (.sq.search)


    bool  m_failed        = false;
    bool  m_inTransaction = false;
    int   m_cardCount     = 0;
    int   m_rowCounter    = 0;  // sequential 'n' primary key for id table
    static constexpr int kTransactionSize = 500;

    // Accumulated data for spgrstat: key=(spacegroup, crystalSystemName), value=list of card ids
    QMap<QPair<QString,QString>, QStringList> m_spgrStat;

    // Accumulated data for infostat tables
    QMap<double,  QStringList> m_statA, m_statB, m_statC;
    QMap<double,  QStringList> m_statAlpha, m_statBeta, m_statGamma;
    QMap<double,  QStringList> m_statVol, m_statDens, m_statCdens;
    QMap<QString, QStringList> m_statColor, m_statSpgr, m_statType;
};

} // namespace qualpf::infrastructure
