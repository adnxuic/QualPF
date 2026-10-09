#ifndef QUALPF_SQL_REFERENCE_REPOSITORY_H
#define QUALPF_SQL_REFERENCE_REPOSITORY_H

#include <QString>
#include <QVector>
#include "infrastructure/database/qt_sql/sql_connection.h"
#include "infrastructure/database/qt_sql/sql_candidate_query.h"
#include "qualpf/application/search_engine.h"
#include "qualpf/application/reference_database.h"
#include "infrastructure/database/database_access.h"
#include <QList>
#include <QPair>
#include <QString>
#include <functional>



namespace qualpf::infrastructure {
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



class SqlReferenceRepository : public qualpf::application::ReferenceRepository
{
public:
    using ProgressCallback = std::function<void(int current, int total)>;

    SqlReferenceRepository();
    SqlReferenceRepository(QString scope, qualpf::infrastructure::ConnectionTrace trace,
                   std::unique_ptr<qualpf::infrastructure::DatabaseLease> lease = {});
    ~SqlReferenceRepository();
    bool openDatabases(const QString &path, std::stop_token cancellation = {});
    void closeDatabeses();
    bool open(const std::string &path) override;
    void close() override { closeDatabeses(); }
    std::pair<int, std::string> information() override;
    application::CardDetails details(const std::string &) const override;
    application::NamedCounts spaceGroups() const override;
    application::NamedCounts colors() const override;
    qualpf::Result<std::vector<qualpf::application::ProductCard>> read(
        const qualpf::application::CandidateQuery &, const qualpf::ExecutionContext &,
        qualpf::application::SearchProgress) override;
    qualpf::Result<qualpf::application::ProductCard> findById(std::string_view, double, const qualpf::ExecutionContext &) override;
    void getInfo(int &ncard, QString &type);
    // void getCardInfo(const QString &idCard);
    // void getCardAdditionalInfo(const QString &idCard);
    CardDetails queryCard(const QString &idCard) const;
    QList<QPair<QString,int>> querySpaceGroups() const;
    QList<QPair<QString,int>> queryColors() const;
private:
    void queryBrowse(const SqlCandidateQuery &, std::vector<qualpf::application::ProductCard> &, ProgressCallback);
    void queryStrongest(const SqlCandidateQuery &, std::vector<qualpf::application::ProductCard> &, ProgressCallback);
    void queryWithoutStrongest(const SqlCandidateQuery &, std::vector<qualpf::application::ProductCard> &, ProgressCallback);
    SqlConnection dbMain;
    SqlConnection dbInfo;
    SqlConnection dbInfoStat;
    SqlConnection dbSearch;
    int nStrongest;
    std::stop_token m_token;
    bool m_decodeReflections=true;
    QString m_readError;
    bool m_reading = false;
    QString basePath_;
    std::unique_ptr<qualpf::infrastructure::DatabaseLease> referenceLease_;

    static QVector<double> blobToDoubleVector(const QByteArray &blob);

    void applyRestraintsToIds(const SqlCandidateQuery &builder, QStringList &ids, int &count);
    int  makeQueryCellPar(const QString &qString, QString &result);
    int  makeQueryCellParameters(const QStringList &qParList, QString &result);
    int  makeQuerySymmetry(const QString &qString, QString &result);
    void readRows(const QString &idsString, const SqlCandidateQuery &builder, int count, std::vector<qualpf::application::ProductCard> &acceptedCards, ProgressCallback progress=nullptr, bool noFilter=false);
    int stringInnerJoin(const QStringList &list1, const QStringList &list2, QStringList &result);
    //QVector<double> extractNumbers(const QString& input, int n, int m);
};

} // namespace qualpf::infrastructure

#endif // QUALPF_SQL_REFERENCE_REPOSITORY_H
