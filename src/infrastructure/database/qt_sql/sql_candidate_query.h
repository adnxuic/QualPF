#ifndef QUALPF_SQL_CANDIDATE_QUERY_H
#define QUALPF_SQL_CANDIDATE_QUERY_H

#include <QStringList>
#include "qualpf/application/search_engine.h"



namespace qualpf::infrastructure {
class SqlCandidateQuery
{
public:
    enum boolOperator {AND_OR_OP, ONLY_OP, JUST_OP};

    SqlCandidateQuery();
    explicit SqlCandidateQuery(const qualpf::application::CandidateQuery &);
    qualpf::application::CandidateQuery candidateQuery(qualpf::application::CandidateQuery::Route) const;
    void initialize();

    void setNames(const QString &newNames);
    void setSubfiles(const QStringList &newSubfiles);
    void setElements(const QString &newElements);
    void setCsysString(const QStringList &newCsysString);
    void setSpgString(const QStringList &newSpgString);
    void setColorString(const QStringList &colors);
    void setCellParameter(int index, double min, double max);
    void setDensityCalc(double min, double max);
    void setDensityMeas(double min, double max);
    void setIdEntry(const QStringList &newIdEntry);
    void setPrintEnabled(bool newPrintEnabled);
    void setBOperator(boolOperator newBOperator);
    void enableDeleted(bool newAddDeleted);
    void setDValues(const QVector<double> &newDValues, const QVector<double> &newDTol);
    void setDecodeReflections(bool value);
    bool decodeReflections() const;

    void buildQuery();
    QString getChemicalQueryString() const;
    QString getSymmetryQueryString() const;
    QString getColorQueryString()    const;
    QStringList getQueryCellPar() const;
    QStringList getQueryDensity() const;
    QString getQueryIdEntry() const;
    bool deletedEnabled() const;
    QVector<double> getDValues() const;
    QVector<double> getDTol() const;
    void setWave(double newWave);

    double getWave() const;

private:
    QString queryChemical;
    QString queryCrySys;
    QString querySymmetry;
    QStringList queryCellPar;
    QString queryIdEntry;
    QString names;
    QStringList subfiles;
    QString elString;
    QStringList csysString;
    QStringList spgString;
    QStringList colorList;
    QString     queryColor;
    QStringList idEntry;
    double cellParMin[6], cellParMax[6];
    double densCalcMin, densCalcMax;
    double densMeasMin, densMeasMax;
    QStringList queryDensity;
    bool printEnabled;
    boolOperator bOperator;
    bool addDeleted;
    QVector<double> dValues, dTol;
    double wave;
    bool decodeReflections_ = false;
    int minimumSpecies_ = 0, maximumSpecies_ = 0;

    QString buildQueryNameString();
    QString buildQuerySubfilesString();
    QString buildQueryElementString();
    QString buildQuerySymmetry();
    QString buildQueryColor();
    QStringList buildQueryCellParameters();
    QStringList buildQueryDensity();
    QString buildQueryIdEntry();
};

} // namespace qualpf::infrastructure

#endif // QUALPF_SQL_CANDIDATE_QUERY_H
