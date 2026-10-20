#include "infrastructure/database/qt_sql/sql_candidate_query.h"
#include <QDebug>

namespace qualpf::infrastructure {

SqlCandidateQuery::SqlCandidateQuery()
    : printEnabled(false),
    bOperator(AND_OR_OP),
    addDeleted(false)
{
    initialize();
}

SqlCandidateQuery::SqlCandidateQuery(const qualpf::application::CandidateQuery &q) : SqlCandidateQuery() {
    auto strings=[](const auto &values) { QStringList out; for (const auto &value:values) out.append(QString::fromStdString(value)); return out; };
    setNames(QString::fromStdString(q.names)); setElements(QString::fromStdString(q.elements));
    minimumSpecies_ = q.minimumSpecies; maximumSpecies_ = q.maximumSpecies;
    setSubfiles(strings(q.subfiles)); setCsysString(strings(q.crystalSystems)); setSpgString(strings(q.spaceGroups));
    setColorString(strings(q.colors)); setIdEntry(strings(q.ids));
    setBOperator(boolOperator(q.elementOperator)); enableDeleted(q.excludeDeleted); setPrintEnabled(q.print);
    for (int i=0;i<6;++i) setCellParameter(i,q.cellMinimum[i],q.cellMaximum[i]);
    setDensityCalc(q.calculatedDensityMinimum,q.calculatedDensityMaximum);
    setDensityMeas(q.measuredDensityMinimum,q.measuredDensityMaximum);
    setDValues(QVector<double>(q.d.begin(),q.d.end()),QVector<double>(q.deltaD.begin(),q.deltaD.end()));
    setWave(q.wavelength); setDecodeReflections(q.decodeReflections); buildQuery();
}
qualpf::application::CandidateQuery SqlCandidateQuery::candidateQuery(qualpf::application::CandidateQuery::Route route) const {
    qualpf::application::CandidateQuery q; q.route=route;
    auto strings=[](const QStringList &values) { std::vector<std::string> out; for (const auto &value:values) out.push_back(value.toStdString()); return out; };
    q.names=names.toStdString(); q.elements=elString.toStdString();
    q.minimumSpecies = minimumSpecies_; q.maximumSpecies = maximumSpecies_;
    q.subfiles=strings(subfiles); q.crystalSystems=strings(csysString); q.spaceGroups=strings(spgString);
    q.colors=strings(colorList); q.ids=strings(idEntry); q.elementOperator=decltype(q.elementOperator)(bOperator);
    q.excludeDeleted=addDeleted; q.print=printEnabled; q.decodeReflections=decodeReflections_;
    std::copy(std::begin(cellParMin),std::end(cellParMin),q.cellMinimum.begin());
    std::copy(std::begin(cellParMax),std::end(cellParMax),q.cellMaximum.begin());
    q.calculatedDensityMinimum=densCalcMin; q.calculatedDensityMaximum=densCalcMax;
    q.measuredDensityMinimum=densMeasMin; q.measuredDensityMaximum=densMeasMax;
    q.d.assign(dValues.begin(),dValues.end()); q.deltaD.assign(dTol.begin(),dTol.end()); q.wavelength=wave;
    return q;
}

void SqlCandidateQuery::initialize()
{
    minimumSpecies_ = maximumSpecies_ = 0;
    subfiles.clear();
    names.clear();
    subfiles.clear();
    elString.clear();
    spgString.clear();
    for (int i = 0; i < 6; i++) {
        cellParMin[i] = -1;
        cellParMax[i] = -1;
    }
    densCalcMin = densCalcMax = -1;
    densMeasMin = densMeasMax = -1;
    wave            = 0.0; // Defined metadata-only request; never decode angles without an explicit wavelength.
    decodeReflections_ = false;

    queryChemical.clear();
    queryCrySys.clear();
    querySymmetry.clear();
    queryCellPar.clear();
    queryDensity.clear();
    colorList.clear();
    queryColor.clear();
}

void SqlCandidateQuery::setSubfiles(const QStringList &newSubfiles)
{
    subfiles = newSubfiles;
}

QString SqlCandidateQuery::getChemicalQueryString() const
{
    return queryChemical;
}

QString SqlCandidateQuery::getSymmetryQueryString() const
{
    return querySymmetry;
}

QStringList SqlCandidateQuery::getQueryCellPar() const
{
    return queryCellPar;
}

QString SqlCandidateQuery::getQueryIdEntry() const
{
    return queryIdEntry;
}

bool SqlCandidateQuery::deletedEnabled() const
{
    return addDeleted;
}

QVector<double> SqlCandidateQuery::getDValues() const
{
    return dValues;
}

QVector<double> SqlCandidateQuery::getDTol() const
{
    return dTol;
}

void SqlCandidateQuery::setDecodeReflections(bool value) { decodeReflections_ = value; }
bool SqlCandidateQuery::decodeReflections() const { return decodeReflections_; }

void SqlCandidateQuery::setWave(double newWave)
{
    wave = newWave;
}

double SqlCandidateQuery::getWave() const
{
    return wave;
}

void SqlCandidateQuery::setDValues(const QVector<double> &newDValues, const QVector<double> &newDTol)
{
    dValues = newDValues;
    dTol = newDTol;

    if (printEnabled) {
        //Print d and delta values
        qInfo() << "d values: " << dValues;
        qInfo() << "delta values: " << dTol;
    }
}

void SqlCandidateQuery::enableDeleted(bool newAddDeleted)
{
    addDeleted = newAddDeleted;
}

void SqlCandidateQuery::setPrintEnabled(bool newPrintEnabled)
{
    printEnabled = newPrintEnabled;
}

void SqlCandidateQuery::setBOperator(boolOperator newBOperator)
{
    bOperator = newBOperator;
}

void SqlCandidateQuery::setSpgString(const QStringList &newSpgString)
{
    spgString = newSpgString;
}

void SqlCandidateQuery::setCellParameter(int index, double min, double max)
{
    cellParMin[index] = min;
    cellParMax[index] = max;
}

void SqlCandidateQuery::setColorString(const QStringList &colors)
{
    colorList = colors;
}

QString SqlCandidateQuery::getColorQueryString() const
{
    return queryColor;
}

void SqlCandidateQuery::setDensityCalc(double min, double max)
{
    densCalcMin = min;
    densCalcMax = max;
}

void SqlCandidateQuery::setDensityMeas(double min, double max)
{
    densMeasMin = min;
    densMeasMax = max;
}

QStringList SqlCandidateQuery::getQueryDensity() const
{
    return queryDensity;
}

void SqlCandidateQuery::setIdEntry(const QStringList &newIdEntry)
{
    idEntry = newIdEntry;
}

void SqlCandidateQuery::setCsysString(const QStringList &newCsysString)
{
    csysString = newCsysString;
}

void SqlCandidateQuery::setElements(const QString &newElements)
{
    elString = newElements;
    // buildQueryElementString() compares operators in lowercase ("and", "or", "not")
    elString.replace(" AND ", " and ", Qt::CaseInsensitive)
            .replace(" OR ",  " or ",  Qt::CaseInsensitive)
            .replace(" NOT ", " not ", Qt::CaseInsensitive);
}

void SqlCandidateQuery::setNames(const QString &newNames)
{
    names = newNames;
}

void SqlCandidateQuery::buildQuery()
{
    queryCellPar  = buildQueryCellParameters();
    queryDensity  = buildQueryDensity();
    queryColor    = buildQueryColor();
    if (printEnabled) {
        for (int i = 0; i < queryCellPar.size(); i++) {
            qInfo() << "Query cell parameter " << i << ": " << queryCellPar.at(i);
        }
    }

    querySymmetry = buildQuerySymmetry();
    if (!querySymmetry.isEmpty() && printEnabled) {
        qInfo() << "Query symmetry: " << querySymmetry;
    }

    queryChemical = buildQueryNameString();
    if (!queryChemical.isEmpty() && printEnabled) {
        qInfo() << "Query chemical name: " << queryChemical;
    }

    QString querySubfile = buildQuerySubfilesString();
    if (!querySubfile.isEmpty()) {
        if (!queryChemical.isEmpty()) queryChemical += " intersect ";
        queryChemical += querySubfile;
        if (printEnabled) qInfo() << "Query subfiles: " << queryChemical;
    }

    QString queryElements = buildQueryElementString();
    if (!queryElements.isEmpty()) {
        if (!queryChemical.isEmpty()) queryChemical += " intersect ";
        queryChemical += queryElements;
        if (printEnabled) qInfo() << "Query elements: " << queryChemical;
    }

    queryIdEntry = buildQueryIdEntry();
    if (minimumSpecies_ > 0 || maximumSpecies_ > 0) {
        QStringList bounds;
        if (minimumSpecies_ > 0) bounds << QString("count(distinct chemical_element) >= %1").arg(minimumSpecies_);
        if (maximumSpecies_ > 0) bounds << QString("count(distinct chemical_element) <= %1").arg(maximumSpecies_);
        if (!queryChemical.isEmpty()) queryChemical += " intersect ";
        queryChemical += "select id from chemical group by id having " + bounds.join(" and ");
    }
    if (!queryIdEntry.isEmpty() && printEnabled) {
        qInfo() << "Query id entry: " << queryIdEntry;
    }
}

QString SqlCandidateQuery::buildQueryNameString()
{
    //Example: Select id, name from id where (
    //(name like '%silicon%' and name like '%oxide%')
    //or (mineralname like '%silicon' and mineralname like '%oxide'))
    QString queryName;
    QStringList nameList = names.split(" ", Qt::SkipEmptyParts);
    if (nameList.size() > 0) {
        queryName = "Select id from id where ( (";

        for (int i = 0; i < nameList.size(); i++) {
            queryName += "name like '%" + nameList.at(i) + "%'";
            if (i != nameList.size()-1) queryName += " and ";
        }
        queryName += ") or (";
        for (int i = 0; i < nameList.size(); i++) {
            queryName += "mineralname like '%" + nameList.at(i) + "%'";
            if (i != nameList.size()-1) queryName += " and ";
        }
        queryName += ")";
        queryName += ")";
    }

    return queryName;
}

QString SqlCandidateQuery::buildQuerySubfilesString()
{
    QString queryString;
    if (subfiles.size() > 0) {
        queryString = QString("Select id from subfiles where subfile = '%1'").arg(subfiles.at(0));
        for (int i = 1; i < subfiles.size(); i++) {
            queryString += QString("and id in (Select id from subfiles where subfile = '%1')").arg(subfiles.at(i));
        }
    }

    return queryString;
}

QString SqlCandidateQuery::buildQueryElementString()
{
    QString queryElement;

    if (!elString.isEmpty()) {
        QStringList list = elString.split(" ");
        if (list.size() % 2 == 0) {
            qInfo() << "Something wrong happens";
            return queryElement;
        }
        QStringList elements(std::ceil(static_cast<double>(list.size()) / 2));

        switch (bOperator) {
        case AND_OR_OP:
        {            
            QStringList logicOper(elements.size()-1);
            int j = 0, k = 0;
            for (int i = 0; i < list.size(); i++) {
                if (i % 2 == 0) {
                    elements[j++] = list.at(i);
                } else {
                    logicOper[k++] = list.at(i);
                }
            }

            if (elements.size() == 1) {
                queryElement = "Select distinct(c0.id) from chemical as c0 where c0.chemical_element='"+elements.at(0)+"'";
            } else {
                bool allAnd = true;
                foreach(const QString& item, logicOper) {
                    if (item != "and") {
                        allAnd = false;
                        break;
                    }
                }
                if (allAnd) {
                    queryElement = "Select distinct(c0.id) from ";
                    for (int i = 0; i < elements.size(); i++) {
                        queryElement += QString("chemical as c%1").arg(i);
                        if (i != elements.size()-1) queryElement+=", ";
                    }
                    queryElement += " where ";
                    for (int i = 0; i < elements.size(); i++) {
                        queryElement += QString("c%1.chemical_element='%2'").arg(i).arg(elements.at(i));
                        if (i != elements.size()-1) {
                            queryElement+= QString(" and c%1.id=c%2.id and ").arg(i).arg(i+1);
                        }
                    }
                } else {
                    //Start query with first element
                    queryElement = QString("Select distinct(id) from chemical where chemical_element='%1'").arg(elements.at(0));

                    //Add elements with 'and' operator to query
                    for (int i = 0; i < logicOper.size(); i++) {
                        if (logicOper.at(i) == "and") {
                            queryElement += QString(" and id in (select distinct(id) from chemical where chemical_element='%1')").arg(elements.at(i+1));
                        }
                    }

                    //Add elements with 'or' operator to query
                    for (int i = 0; i < logicOper.size(); i++) {
                        if (logicOper.at(i) == "or") {
                            queryElement += QString(" union select distinct(id) from chemical where chemical_element='%1'").arg(elements.at(i+1));
                        }
                    }
                }
            }
            break;
        }
        case ONLY_OP:
        {
            //Example: Al and O and P and Si
            //Select distinct(c0.id) from chemical as c0, chemical as c1, chemical as c2, chemical as c3
            //where c0.chemical_element='Al' and c0.id=c1.id and
            //      c1.chemical_element='O' and c1.id=c2.id and
            //      c2.chemical_element='P' and c2.id=c3.id and
            //      c3.chemical_element='Si' and
            //      not exists (select * from chemical as c4 where c4.id=c0.id and
            //     ( c4.chemical_element!='Al' and
            //       c4.chemical_element!='O' and
            //       c4.chemical_element!='P' and
            //       c4.chemical_element!='Si'))        
            int j = 0;
            for (int i = 0; i < list.size(); i++) {
                if (i % 2 == 0) {
                    elements[j++] = list.at(i);
                }
            }
            queryElement = "Select distinct(c0.id) from chemical as c0";
            for (int i = 1; i < elements.size(); i++) {
                queryElement += QString(", chemical as c%1").arg(i);
            }
            queryElement += " where ";
            int lastEl = elements.size() - 1;
            for (int i = 0; i < lastEl; i++) {
                queryElement += QString("c%1.chemical_element='%2' and c%1.id=c%3.id and ").arg(i).arg(elements.at(i)).arg(i+1);
            }
            queryElement += QString("c%1.chemical_element='%2'").arg(lastEl).arg(elements.at(lastEl));
            queryElement += QString(" and not exists (select * from chemical as c%1 where c%1.id=c0.id and (").arg(elements.size());
            for (int i = 0; i < elements.size(); i++) {
                queryElement += QString(" c%1.chemical_element!='%2'").arg(elements.size()).arg(elements.at(i));
                if (i < lastEl) queryElement += " and ";
            }            
            queryElement += "))";
            break;
        }
        case JUST_OP:
            // Example: Al and P and O
            // Select distinct(C.id) from chemical as C where
            //    (C.chemical_element="Al" or
            //     C.chemical_element="P" or
            //     C.chemical_element="O")
            //     and C.id not in ( select id from chemical where
            //     chemical_element!="Al" and
            //     chemical_element!="P" and
            //     chemical_element!="O")
            int j = 0;
            for (int i = 0; i < list.size(); i++) {
                if (i % 2 == 0) {
                    elements[j++] = list.at(i);
                }
            }
            queryElement = "Select distinct(C.id) from chemical as C where (";
            int lastEl = elements.size() - 1;
            for (int i = 0; i < lastEl; i++) {
                queryElement += QString("C.chemical_element='%1' or ").arg(elements.at(i));
            }
            queryElement += QString("C.chemical_element='%1')").arg(elements.at(lastEl));
            queryElement += "and C.id not in ( select id from chemical where ";
            for (int i = 0; i < lastEl; i++) {
                queryElement += QString("chemical_element!='%1' and ").arg(elements.at(i));
            }
            queryElement += QString("chemical_element!='%1')").arg(elements.at(lastEl));
            break;
        }

    }

    return queryElement;
}

QString SqlCandidateQuery::buildQuerySymmetry()
{
    QString querySymm;

    if (spgString.size() != 0) {
        querySymm = "select ids,n from spgrstat where (";
        for (int i = 0; i < spgString.size(); i++) {
            querySymm += QString("trim(spacegroup) = '%1'").arg(spgString.at(i));
            if (i != spgString.size()-1) querySymm += " or ";
        }

        for (int i = 0; i < csysString.size(); i++) {
            querySymm += QString(" or type='%1'").arg(csysString.at(i));
        }

        querySymm += ")";

        return querySymm;
    }

    if (csysString.size() != 0) {
        querySymm = "select ids,n from stat_type where (";
        for (int i = 0; i < csysString.size(); i++) {
            querySymm += QString("trim(val) like '%1'").arg(csysString.at(i));
            if (i != csysString.size()-1) querySymm += " or ";
        }
        querySymm += ")";
    }

    return querySymm;
}

QStringList SqlCandidateQuery::buildQueryCellParameters()
{
    const QStringList cellParStr = {"a", "b", "c", "alpha", "beta", "gamma"};
    QStringList queryCell;

    for (int i = 0; i < 6; i++) {
        if (cellParMin[i] > -1 && cellParMax[i] > -1) {
            QString query = "select ids,n from stat_" + cellParStr.at(i) + " where (";
            query += QString("val >= %1 and val <= %2").arg(cellParMin[i]).arg(cellParMax[i]);
            query += ")";
            queryCell.append(query);
        }
    }

    return queryCell;
}

QString SqlCandidateQuery::buildQueryColor()
{
    if (colorList.isEmpty())
        return {};
    QString q = QStringLiteral("select ids,n from stat_color where (");
    for (int i = 0; i < colorList.size(); ++i) {
        q += QString("trim(val) like '%1'").arg(colorList.at(i));
        if (i != colorList.size() - 1) q += QStringLiteral(" or ");
    }
    q += QLatin1Char(')');
    return q;
}

QStringList SqlCandidateQuery::buildQueryDensity()
{
    QStringList queries;
    if (densCalcMin > -1 && densCalcMax > -1)
        queries << QString("select ids,n from stat_cdens where (val >= %1 and val <= %2)")
                       .arg(densCalcMin).arg(densCalcMax);
    if (densMeasMin > -1 && densMeasMax > -1)
        queries << QString("select ids,n from stat_dens where (val >= %1 and val <= %2)")
                       .arg(densMeasMin).arg(densMeasMax);
    return queries;
}

QString SqlCandidateQuery::buildQueryIdEntry()
{
    //Example: select id from id where id in (1000000, 1000017)
    QString queryId;
    if (idEntry.size() > 0) {
        queryId = "select id from id where id in (";
        for (int i = 0; i < idEntry.size(); i++) {
            queryId += idEntry.at(i);
            if (i != idEntry.size()-1) queryId += ", ";
        }
        queryId += ")";
    }

    return queryId;
}

}
