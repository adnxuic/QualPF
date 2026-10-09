#pragma once
#include "qualpf/application/search_engine.h"
#include <QStringList>
#include <QVector>
#include <QDebug>
namespace qualpf::gui {
// Qt boundary DTO. SearchEngine alone evaluates scores; SQL receives filters.
class SearchRequest {
  public:
    enum boolOperator { AND_OR_OP, ONLY_OP, JUST_OP };
    SearchRequest() { query_.decodeReflections = false; }
    explicit SearchRequest(const application::CandidateQuery &query)
        : query_(query), calcFom_(query.decodeReflections) {}
    void initialize() {
        query_.names.clear(); query_.subfiles.clear(); query_.elements.clear();
        query_.spaceGroups.clear(); query_.colors.clear();
        query_.cellMinimum.fill(-1); query_.cellMaximum.fill(-1);
        query_.calculatedDensityMinimum = query_.calculatedDensityMaximum = -1;
        query_.measuredDensityMinimum = query_.measuredDensityMaximum = -1;
        query_.wavelength = 0; query_.decodeReflections = false;
        SearchParameters defaults;
        parameters_ = defaults;
        parameters_.minimum_score = .35;
        parameters_.weight_position = parameters_.weight_intensity = parameters_.weight_phases = .50;
        parameters_.delta_two_theta.value = .08;
        calcFom_ = false;
    }
    application::CandidateQuery candidateQuery(application::CandidateQuery::Route route) const {
        auto value = query_; value.route = route; return value;
    }
    void setNames(const QString &s) { query_.names = s.toStdString(); }
    void setElements(QString s) {
        s.replace(" AND ", " and ", Qt::CaseInsensitive).replace(" OR ", " or ", Qt::CaseInsensitive).replace(" NOT ", " not ", Qt::CaseInsensitive);
        query_.elements = s.toStdString();
    }
    void setCellParameter(int i, double low, double high) { query_.cellMinimum[i]=low; query_.cellMaximum[i]=high; }
    void setDensityCalc(double low, double high) { query_.calculatedDensityMinimum=low; query_.calculatedDensityMaximum=high; }
    void setDensityMeas(double low, double high) { query_.measuredDensityMinimum=low; query_.measuredDensityMaximum=high; }
    void setBOperator(boolOperator op) { query_.elementOperator = application::CandidateQuery::ElementOperator(op); }
    void enableDeleted(bool value) { query_.excludeDeleted=value; }
    bool deletedEnabled() const { return query_.excludeDeleted; }
    void setPrintEnabled(bool value) { query_.print=value; }
    void setDecodeReflections(bool value) { query_.decodeReflections=value; }
    bool decodeReflections() const { return query_.decodeReflections; }
    void setWave(double value) { query_.wavelength=value; }
    double getWave() const { return query_.wavelength; }
    void setDValues(const QVector<double> &d, const QVector<double> &tolerance) {
        query_.d.assign(d.begin(),d.end()); query_.deltaD.assign(tolerance.begin(),tolerance.end());
        if (query_.print) { qInfo() << "d values:" << d; qInfo() << "delta values:" << tolerance; }
    }
    QVector<double> getDValues() const { return {query_.d.begin(),query_.d.end()}; }
    QVector<double> getDTol() const { return {query_.deltaD.begin(),query_.deltaD.end()}; }
    // SQL compilation belongs to the reader; the GUI request contains only criteria.
    void setSubfiles(const QStringList &values) { query_.subfiles.clear(); for (const auto &s : values) query_.subfiles.push_back(s.toStdString()); }
    void setCsysString(const QStringList &values) { query_.crystalSystems.clear(); for (const auto &s : values) query_.crystalSystems.push_back(s.toStdString()); }
    void setSpgString(const QStringList &values) { query_.spaceGroups.clear(); for (const auto &s : values) query_.spaceGroups.push_back(s.toStdString()); }
    void setColorString(const QStringList &values) { query_.colors.clear(); for (const auto &s : values) query_.colors.push_back(s.toStdString()); }
    void setIdEntry(const QStringList &values) { query_.ids.clear(); for (const auto &s : values) query_.ids.push_back(s.toStdString()); }
    void setMinFom(double value) { parameters_.minimum_score = value; }
    double getMinFom() const { return parameters_.minimum_score; }
    void setCalcFom(bool value) {
        calcFom_ = value;
        setDecodeReflections(value);
    }
    bool getCalcFom() const { return calcFom_; }
    void setWeight2thetaD(double value) { parameters_.weight_position = value; }
    double getWeight2thetaD() const { return parameters_.weight_position; }
    void setWeightIntensity(double value) { parameters_.weight_intensity = value; }
    double getWeightIntensity() const { return parameters_.weight_intensity; }
    void setWeightPhases(double value) { parameters_.weight_phases = value; }
    double getWeightPhases() const { return parameters_.weight_phases; }
    void setDelta2theta(double value) { parameters_.delta_two_theta.value = value; }
    double getDelta2theta() const { return parameters_.delta_two_theta.value; }

  private:
    application::CandidateQuery query_;
    SearchParameters parameters_ = [] {
        SearchParameters value;
        value.minimum_score = .35;
        value.weight_position = value.weight_intensity = value.weight_phases = .50;
        value.delta_two_theta.value = .08;
        return value;
    }();
    bool calcFom_ = false;
};
} // namespace qualpf::gui
