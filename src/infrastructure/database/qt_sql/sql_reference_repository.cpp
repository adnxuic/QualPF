#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/database/searchutil.h"
#include "infrastructure/diagnostics/scopedtimer.h"

#include "core/io/powder_cif/binary32.h"
#include "qualpf/search/algorithms.h"
#include <bit>
#include <cmath>
#include <QSqlQuery>
#include <QSqlError>
#include <QScopeGuard>
#include <QUuid>

namespace qualpf::infrastructure {



qualpf::Result<std::vector<qualpf::application::ProductCard>> SqlReferenceRepository::read(
    const qualpf::application::CandidateQuery &request, const qualpf::ExecutionContext &context,
    qualpf::application::SearchProgress progress)
{
    using namespace qualpf;
    if (!dbMain.onOwnerThread()) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::invalid_input,"database.thread","Reader requires its execution thread"}};
    if (m_reading) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::invalid_input,"database","Reader is already active","busy"}};
    m_reading=true;
    const auto cleanup=qScopeGuard([this]{m_reading=false; m_token={};});
    m_token=context.cancellation; m_decodeReflections=request.decodeReflections; m_readError.clear();
    m_strictPairedReflections=request.strictPairedReflections;
    if (m_token.stop_requested()) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::cancelled,"search","Search canceled"}};
    if (!dbMain.isOpen()) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::missing_data,"database","Database is not open"}};
    auto lease = referenceLease_ ? nullptr : infrastructure::DatabaseLease::read(basePath_);
    if (!referenceLease_ && !lease) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::invalid_input,"database","Database is being written","busy"}};
    SqlCandidateQuery builder(request);
    std::vector<application::ProductCard> cards;
    if (request.preselectedIds) {
        QStringList ids;
        for (const auto &id : *request.preselectedIds) ids.push_back(QString::fromStdString(id));
        int count = ids.size();
        applyRestraintsToIds(builder, ids, count);
        if (!ids.empty()) {
            QString quoted;
            for (const auto &id : ids) quoted += QLatin1Char('\'') + id + QLatin1String("',");
            quoted.chop(1);
            readRows(quoted, builder, count, cards, progress);
        }
    } else {
    switch (request.route) {
    case application::CandidateQuery::Route::constrained_browse: queryBrowse(builder,cards,progress); break;
    case application::CandidateQuery::Route::stored_strongest: queryStrongest(builder,cards,progress); break;
    case application::CandidateQuery::Route::matching: queryWithoutStrongest(builder,cards,progress); break;
    }
    }
    if (m_token.stop_requested()) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::cancelled,"search","Search canceled"}};
    if (!m_readError.isEmpty()) return Result<std::vector<application::ProductCard>>{Error{ErrorCode::missing_data,"database.query",m_readError.toStdString()}};
    return Result<std::vector<application::ProductCard>>{std::move(cards)};
}

SqlReferenceRepository::SqlReferenceRepository()
    : SqlReferenceRepository(QUuid::createUuid().toString(QUuid::Id128), {}) {}
SqlReferenceRepository::SqlReferenceRepository(QString scope, qualpf::infrastructure::ConnectionTrace trace,
                             std::unique_ptr<qualpf::infrastructure::DatabaseLease> lease)
    : dbMain(scope,"main",trace), dbInfo(scope,"info",trace), dbInfoStat(scope,"statistics",trace),
      dbSearch(scope,"search",trace), nStrongest(3), referenceLease_(std::move(lease)) {}

SqlReferenceRepository::~SqlReferenceRepository()
{
    closeDatabeses();
}

bool SqlReferenceRepository::openDatabases(const QString &path, std::stop_token cancellation)
{
    if (!dbMain.onOwnerThread()) return false;
    if (m_reading) {

        m_readError = "Cannot replace databases during a search";
        return false;
    }
    closeDatabeses();
    basePath_ = path;
    const auto open = [&](SqlConnection &db, const QString &file) {
        return !cancellation.stop_requested() && db.openDb(file) && !cancellation.stop_requested();
    };
    if (!open(dbMain,path+".sq") || !open(dbInfo,path+".sq.info") ||
        !open(dbInfoStat,path+".sq.infostat") || !open(dbSearch,path+".sq.search")) {
        closeDatabeses();
        return false;
    }
    return true;
}

void SqlReferenceRepository::closeDatabeses()
{
    if (m_reading) {

        m_readError = "Cannot close databases during a search";
        return;
    }
    dbMain.closeDb();
    dbInfo.closeDb();
    dbInfoStat.closeDb();
    dbSearch.closeDb();
}

CardDetails SqlReferenceRepository::queryCard(const QString &idCard) const
{
    CardDetails info;
    if (!dbMain.onOwnerThread()) return info;
    auto lease = referenceLease_ ? nullptr : qualpf::infrastructure::DatabaseLease::read(basePath_);
    if ((!referenceLease_ && !lease) || !dbMain.onOwnerThread()) return info;

    QSqlQuery q1(dbMain.db());
    q1.prepare("SELECT id, name, mineralname, chemical_formula, spacegroup, quality, rir, nrec, nd, "
               "dvalue, intensita FROM id WHERE id=" + idCard);
    if (q1.exec() && q1.first()) {
        info.id              = q1.value(0).toString();
        info.name            = q1.value(1).toString().trimmed();
        info.mineralName     = q1.value(2).toString().trimmed();
        info.chemicalFormula = q1.value(3).toString().trimmed();
        info.spaceGroup      = q1.value(4).toString().trimmed();
        info.quality         = q1.value(5).toString().trimmed();
        const double rirVal  = q1.value(6).toDouble();
        info.rir             = rirVal != 0.0 ? QString::number(rirVal, 'f', 4) : QString();
        info.nrec            = q1.value(7).toInt();
        info.nd              = q1.value(8).toInt();
        info.dvalues         = blobToDoubleVector(q1.value(9).toByteArray());
        info.intensities     = blobToDoubleVector(q1.value(10).toByteArray());
        info.valid           = true;
    }

    // The "info" table (a, b, c, alpha, beta, gamma, h, k, l, ...) lives in
    // dbInfo (.sq.info) for both database types.
    QSqlQuery q2(dbInfo.db());
    q2.prepare("SELECT authors, journal, journal_year, journal_volume, page_start, page_end, "
               "color, crystal_density, type, volume, density, z, a, b, c, alpha, beta, gamma, "
               "`mu(CuKa)`, h, k, l, mul FROM info WHERE id=" + idCard);
    if (q2.exec() && q2.first()) {
        info.authors        = q2.value(0).toString().trimmed();
        info.journal        = q2.value(1).toString().trimmed();
        info.journalYear    = q2.value(2).toInt();
        info.journalVolume  = q2.value(3).toString().trimmed();
        info.pageStart      = q2.value(4).toString().trimmed();
        info.pageEnd        = q2.value(5).toString().trimmed();
        info.color          = q2.value(6).toString().trimmed();
        info.crystalDensity = q2.value(7).toDouble();
        info.type           = q2.value(8).toString().trimmed();
        info.volume         = q2.value(9).toDouble();
        info.density        = q2.value(10).toDouble();
        info.z              = q2.value(11).toInt();
        info.a              = q2.value(12).toDouble();
        info.b              = q2.value(13).toDouble();
        info.c              = q2.value(14).toDouble();
        info.alpha          = q2.value(15).toDouble();
        info.beta           = q2.value(16).toDouble();
        info.gamma          = q2.value(17).toDouble();
        info.muCuKa         = q2.value(18).toDouble();

        auto parseInts = [](const QString &s) {
            QVector<int> v;
            for (const QString &t : s.split(QLatin1Char(','), Qt::SkipEmptyParts))
                v.append(t.trimmed().toInt());
            return v;
        };
        info.h   = parseInts(q2.value(19).toString());
        info.k   = parseInts(q2.value(20).toString());
        info.l   = parseInts(q2.value(21).toString());
        info.mul = parseInts(q2.value(22).toString());
    }

    return info;
}

// void SqlReferenceRepository::getCardInfo(const QString &idCard)
// {
//     QSqlQuery queryId(dbMain.db());
//     queryId.prepare("SELECT id, name, mineralname, chemical_formula, spacegroup, quality, rir, nrec, intensita, dvalue, nd FROM id WHERE id="+idCard);
//     if (queryId.exec()) {
//         if (queryId.first()) {
//             qInfo() << "QUERY ID" << "id: " << queryId.value(0).toString() << Qt::endl <<
//                 //                       "name: " << queryId.value(1).toString() << Qt::endl <<
//                 //                       "mineralname: " << queryId.value(2).toString() << Qt::endl <<
//                 //                       "chemical_formula: " << queryId.value(3).toString() << Qt::endl <<
//                 //                       "spacegroup: " << queryId.value(4).toString() << Qt::endl <<
//                 //                       "quality: " << queryId.value(5).toString() << Qt::endl <<
//                 //                       "RIR: " << queryId.value(6).toFloat() << Qt::endl <<
//                 //                       "nrec: " << queryId.value(7).toInt() << Qt::endl <<
//                 //                       "intensita: " << queryId.value(8).toString() << Qt::endl <<
//                 //                       "dvalue: " << queryId.value(9).toString() << Qt::endl <<
//                 "nd: " << queryId.value(10).toInt();
//         } else {
//             qInfo() << "ERROR: id non trovato";
//         }
//     }
// }

// void SqlReferenceRepository::getCardAdditionalInfo(const QString &idCard)
// {
//     QSqlQuery queryId(dbInfo.db());
//     queryId.prepare("SELECT id, authors, journal, journal_year, journal_volume, page_start, "
//                     "page_end, color, crystal_density, spacegroup, type, volume, density, z, "
//                     "rir, a, b, c, alpha, beta, gamma, h, k, l, mul, `mu(CuKa)` FROM info WHERE id="+idCard);
//     if (queryId.exec()) {
//         if (queryId.first()) {
//             qInfo() << "QUERY ADD. INFO: " << queryId.value(0).toString();
//         } else {
//             qInfo() << "ERROR: id non trovato";
//         }
//     }
// }

QVector<double> SqlReferenceRepository::blobToDoubleVector(const QByteArray &blob)
{
    QVector<double> result;
    QString str = QString::fromUtf8(blob);
    QStringList list = str.split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < list.size(); ++i) {
        bool ok = false;
        double val = list.at(i).toDouble(&ok);
        if (ok) result.append(val);
    }
    return result;
}

int SqlReferenceRepository::makeQueryCellPar(const QString &qString, QString &result)
{
    //qInfo() << "Start query cell parameter";
    int nData = 0;
    result.clear();

    QSqlQuery query = QSqlQuery(dbInfoStat.db());

    query.prepare(qString);
    if (query.exec()) {
        while (query.next()) {
            if (m_token.stop_requested()) break;
            if (result.isEmpty()) {
                result = query.value(0).toString();
            } else {
                result = result + "," + query.value(0).toString();
            }
            nData = nData + query.value(1).toInt();
        }
    }
    if (query.lastError().isValid()) m_readError = query.lastError().text();
    // qInfo() << "N: " << nData;
    // qInfo() << "End query cell parameter";
    return nData;
}

int SqlReferenceRepository::makeQueryCellParameters(const QStringList &qParList, QString &result)
{

    int nData = 0;
    result.clear();

    for (int i = 0; i < qParList.size(); i++) {
        qInfo() << "Start query cell parameter n. " << i;
        if (i == 0) {
            nData = makeQueryCellPar(qParList.at(i), result);
        } else {
            QString tmpResult;
            int tmpData = makeQueryCellPar(qParList.at(i), tmpResult);
            QStringList list1 = result.split(QLatin1Char(','), Qt::SkipEmptyParts);
            QStringList list2 = tmpResult.split(QLatin1Char(','), Qt::SkipEmptyParts);
            QStringList resultTmp;
            nData = stringInnerJoin(list1, list2, resultTmp);
            qInfo() << "Inner join: " << list1.size() << " - " << list2.size() << " -> " << nData;
            result = resultTmp.join(",");
        }
        //qInfo() << "ID: " << result;
        qInfo() << "N: " << nData;
        qInfo() << "End query cell parameter n. " << i;
    }

    return nData;
}

int SqlReferenceRepository::makeQuerySymmetry(const QString &qString, QString &result)
{
    qInfo() << "Start query symmetry";
    int ndata = 0;
    result.clear();

    QSqlQuery query;
    if (qString.contains("from spgrstat"))
        query = QSqlQuery(dbInfo.db());
    else
        query = QSqlQuery(dbInfoStat.db());

    query.prepare(qString);
    if (query.exec()) {
        while (query.next()) {
            if (m_token.stop_requested()) break;
            if (result.isEmpty()) {
                result = query.value(0).toString();
            } else {
                result = result + "," + query.value(0).toString();
            }
            ndata = ndata + query.value(1).toInt();
        }
    }
    if (query.lastError().isValid()) m_readError = query.lastError().text();
    //qInfo() << "Space groups: " << result;
    qInfo() << "N: " << ndata;
    qInfo() << "End query symmetry";
    return ndata;
}

void SqlReferenceRepository::readRows(const QString &idsString, const SqlCandidateQuery &builder, int count,
                              std::vector<qualpf::application::ProductCard> &cards, ProgressCallback progress, bool noFilter)
{
    if (idsString.isEmpty() && !noFilter) return;
    QSqlQuery query(dbMain.db());
    QString sql = "Select id, name, mineralname, chemical_formula, spacegroup, quality, rir, nrec, dvalue, intensita, n";
    if(m_strictPairedReflections)sql+=", nd";
    sql+=" from id";
    if (!idsString.isEmpty()) {
        if (builder.deletedEnabled()) sql += " where trim(quality)!='D' and id in (" + idsString + ")";
        else sql += " where id in (" + idsString + ")";
    } else if (builder.deletedEnabled()) sql += " where trim(quality)!='D'";
    const int actualCount = count > 0 ? count : dbMain.queryForCount(sql);
    query.prepare(sql);
    int row=0, last=-1;
    if (query.exec()) while (query.next()) {
        if (m_token.stop_requested()) break;
        ++row;
        const int percentage = actualCount > 0 ? static_cast<int>(100.0f*row/actualCount) : 0;
        if (progress && percentage != last) { last=percentage; progress(row,actualCount); }
        if (m_token.stop_requested()) break;
        qualpf::application::ProductCard card;
        card.id=query.value(0).toString().toStdString();
        card.chemicalName=query.value(1).toString().toStdString();
        card.mineralName=query.value(2).toString().toStdString();
        card.chemicalFormula=query.value(3).toString().toStdString();
        card.spaceGroup=query.value(4).toString().toStdString();
        card.quality=query.value(5).toString().toStdString();
        card.rir=query.value(6).toString().toStdString();
        if (m_strictPairedReflections) {
            bool validRir = false;
            const auto raw = query.value(6);
            const auto type = raw.metaType().id();
            const bool numeric = type == QMetaType::Double || type == QMetaType::Float ||
                type == QMetaType::Int || type == QMetaType::UInt ||
                type == QMetaType::LongLong || type == QMetaType::ULongLong;
            // Preserve stored binary64 numbers; the legacy display string can
            // contain fewer digits. Text still uses the existing Qt boundary.
            const auto value = numeric ? raw.toDouble(&validRir) : raw.toString().trimmed().toDouble(&validRir);
            if (validRir) card.referenceRir = value;
        }
        if (m_decodeReflections) {
            if (m_strictPairedReflections) {
                const auto decode=[](const QByteArray &bytes,std::vector<double> &values,bool spacing) {
                    if(bytes.trimmed().isEmpty())return true; // Legal nd=0 gives an explicit zero template.
                    auto parts=QString::fromUtf8(bytes).split(',',Qt::KeepEmptyParts);
                    // Database writers conventionally append one terminal comma.
                    if(!parts.empty()&&parts.back().trimmed().isEmpty())parts.removeLast();
                    for(const auto &part:parts) {
                        bool ok=false;const double value=part.toDouble(&ok);
                        if(!ok||!std::isfinite(value)||(spacing?value<=0:value<0))return false;
                        values.push_back(value);
                    }
                    return true;
                };
                bool validCount=false;
                const auto declaredCount=query.value(11).toString().toULongLong(&validCount);
                if(!validCount||!decode(query.value(8).toByteArray(),card.d,true)||!decode(query.value(9).toByteArray(),card.intensity,false)||
                    card.d.size()!=card.intensity.size()||declaredCount!=card.d.size()) {
                    m_readError=QStringLiteral("Invalid paired d/I reflections for PFSM card ")+query.value(0).toString();
                    break;
                }
            } else {
            const auto d=blobToDoubleVector(query.value(8).toByteArray());
            const auto intensity=blobToDoubleVector(query.value(9).toByteArray());
            card.d.assign(d.begin(),d.end()); card.intensity.assign(intensity.begin(),intensity.end());
            }
            if (builder.getWave()>0) for (double value:card.d) {
                const auto angle=qualpf::io::powder_cif::binary32::certifiedAsin64(std::bit_cast<std::uint64_t>(0.5*builder.getWave()/value));
                const double tth=2.0*std::bit_cast<double>(angle)*(180/3.14159265358979323846);
                if (!std::isnan(tth)) card.tth.push_back(tth);
            }
        }
        cards.push_back(std::move(card));
    }
    if (query.lastError().isValid()) m_readError=query.lastError().text();
}








int SqlReferenceRepository::stringInnerJoin(const QStringList &list1, const QStringList &list2, QStringList &result)
{
    foreach(QString str, list1) {
        if (list2.contains(str)) {
            result.append(str);
        }
    }
    return result.size();
}

// QVector<double> SqlReferenceRepository::extractNumbers(const QString &input, int n, int m)
// {
//     // n: Number of numbers in the string
//     // m: How many numbers you want to extract

//     QVector<double> result;
//     // Split the string into a list using comma as separator
//     QStringList numberStrings = input.split(',', Qt::SkipEmptyParts);

//     // Check: if n does not match the actual number of elements, adjust n
//     //if (n > numberStrings.size())
//     //    n = numberStrings.size();

//     // Extract up to m numbers, but not more than those available
//     int count = qMin(m, n);

//     for (int i = 0; i < count; ++i) {
//         bool ok;
//         double number = numberStrings[i].toDouble(&ok);
//         if (ok)
//             result.append(number);
//     }
//     return result;
// }

void SqlReferenceRepository::queryBrowse(const SqlCandidateQuery &builder, std::vector<qualpf::application::ProductCard> &cards, ProgressCallback progress)
{
    // Browse and matching use the same restraint intersections. Keep the existing
    // empty Classical browse behavior. PFSM explicitly requests the complete
    // paired library when no restraints are supplied.
    if (!m_strictPairedReflections && builder.getQueryCellPar().isEmpty() && builder.getQueryDensity().isEmpty() &&
        builder.getSymmetryQueryString().isEmpty() && builder.getColorQueryString().isEmpty() &&
        builder.getChemicalQueryString().isEmpty() && builder.getQueryIdEntry().isEmpty()) return;
    queryWithoutStrongest(builder, cards, std::move(progress));
}

void SqlReferenceRepository::applyRestraintsToIds(const SqlCandidateQuery &builder,
                                          QStringList &ids, int &count)
{
    if (ids.isEmpty()) return;

    // Every enabled statistical restraint participates, including an empty result.
    // Reuse the query builder's existing calculated/measured table semantics.
    const auto intersect = [&](const QString &matches) {
        const auto allowed = matches.split(QLatin1Char(','), Qt::SkipEmptyParts);
        QStringList filtered;
        stringInnerJoin(ids, allowed, filtered);
        ids = std::move(filtered);
        count = ids.size();
    };
    const auto statistical = builder.getQueryCellPar() + builder.getQueryDensity();
    if (!statistical.isEmpty()) {
        QString matches;
        makeQueryCellParameters(statistical, matches);
        intersect(matches);
        if (ids.isEmpty()) return;
    }
    for (const auto &query : {builder.getSymmetryQueryString(), builder.getColorQueryString()}) {
        if (query.isEmpty()) continue;
        QString matches;
        makeQuerySymmetry(query, matches);
        intersect(matches);
        if (ids.isEmpty()) return;
    }

    // --- Chemical / name / subfile / element restraints ---
    QString queryChemical = builder.getChemicalQueryString();
    const QString entryIds = builder.getQueryIdEntry();
    if (!entryIds.isEmpty())
        queryChemical = queryChemical.isEmpty() ? entryIds : entryIds + " INTERSECT " + queryChemical;
    if (!queryChemical.isEmpty()) {
        // Build quoted id list for INTERSECT clause
        QString quotedIds;
        for (const QString &id : std::as_const(ids))
            quotedIds.append(QLatin1Char('\'') + id + QLatin1String("',"));
        quotedIds.chop(1);

        const QString combined = queryChemical
            + QStringLiteral(" INTERSECT SELECT id FROM id WHERE id IN (") + quotedIds + QLatin1Char(')');

        QSqlQuery q(dbMain.db());
        q.prepare(combined);
        if (q.exec()) {
            ids.clear();
            while (q.next())
                ids.append(q.value(0).toString());
            count = ids.size();
        }
        if (q.lastError().isValid()) m_readError = q.lastError().text();
    }
}







Result<std::vector<application::StoredCandidateIndex>> SqlReferenceRepository::storedTopValues(
    const ExecutionContext &context, application::SearchProgress progress) {
    using Return = Result<std::vector<application::StoredCandidateIndex>>;
    QSqlQuery query(dbSearch.db());
    if (!query.exec("SELECT id, n, dval FROM top"))
        return Return{Error{ErrorCode::missing_data, "database.query", query.lastError().text().toStdString()}};
    std::vector<application::StoredCandidateIndex> result;
    std::size_t scanned = 0;
    while (query.next()) {
        if (context.cancellation.stop_requested())
            return Return{Error{ErrorCode::cancelled, "search", "Search canceled"}};
        application::StoredCandidateIndex index;
        index.id = query.value(0).toString().toStdString();
        const auto values = SearchUtil::extractNumbers(query.value(2).toString(), query.value(1).toInt(), 3);
        for (double d : values) index.strongest.push_back(DSpacingAngstrom{d});
        result.push_back(std::move(index));
        if (++scanned % 500 == 0 && progress) progress(0, 0);
    }
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    if (query.lastError().isValid())
        return Return{Error{ErrorCode::missing_data, "database.query", query.lastError().text().toStdString()}};
    return Return{std::move(result)};
}
Result<std::vector<application::StoredCandidateIndex>> SqlReferenceRepository::storedTop(
    const ExecutionContext &context, application::SearchProgress progress) {
    using Return = Result<std::vector<application::StoredCandidateIndex>>;
    if (!dbMain.onOwnerThread()) return Return{Error{ErrorCode::invalid_input, "database.thread", "Reader requires its execution thread"}};
    if (m_reading) return Return{Error{ErrorCode::invalid_input, "database", "Reader is already active", "busy"}};
    if (context.cancellation.stop_requested()) return Return{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    if (!dbMain.isOpen()) return Return{Error{ErrorCode::missing_data, "database", "Database is not open"}};
    auto lease = referenceLease_ ? nullptr : infrastructure::DatabaseLease::read(basePath_);
    if (!referenceLease_ && !lease) return Return{Error{ErrorCode::invalid_input, "database", "Database is being written", "busy"}};
    m_reading = true;
    const auto cleanup = qScopeGuard([this] { m_reading = false; });
    return storedTopValues(context, std::move(progress));
}

void SqlReferenceRepository::queryStrongest(const SqlCandidateQuery &builder, std::vector<qualpf::application::ProductCard> &acceptedCards,
                                        ProgressCallback progress)
{
    ScopedTimer timer("QualxDbManager::makeQueryStrongest->makeQueryInfoIdsWithFom");

    auto indexes = storedTopValues({m_token, qualpf::NumericalPolicy::legacy_mixed_precision, "stored-index"}, progress);
    if (!indexes) { m_readError = QString::fromStdString(indexes.error().message); return; }
    QStringList idList;
    const auto d = builder.getDValues(), delta = builder.getDTol();
    for (const auto &index : indexes.value()) {
        auto accepted = qualpf::search::passes_stored_strongest({d.constData(), std::size_t(d.size())},
            {delta.constData(), std::size_t(delta.size())}, index.strongest,
            {m_token, qualpf::NumericalPolicy::legacy_mixed_precision, "stored-index"});
        if (!accepted) return;
        if (accepted.value()) idList.append(QString::fromStdString(index.id));
    }
    qInfo() << "Number of strongest matches:" << idList.size();

    // Apply restraints from builder (cell params, symmetry, chemical, names…)
    int count = idList.size();
    applyRestraintsToIds(builder, idList, count);
    qInfo() << "Number of candidates after restraints:" << count;

    if (idList.isEmpty()) return;

    // Build quoted idsString for makeQueryInfoIdsWithFom
    QString idsString;
    for (const QString &id : std::as_const(idList))
        idsString.append(QLatin1Char('\'') + id + QLatin1String("',"));
    idsString.chop(1);

    readRows(idsString, builder, count, acceptedCards, progress);
}

void SqlReferenceRepository::queryWithoutStrongest(const SqlCandidateQuery &builder,
                                               std::vector<qualpf::application::ProductCard> &acceptedCards,
                                               ProgressCallback progress)
{
    ScopedTimer timer("QualxDbManager::makeQueryWithoutStrongest");

    const bool hasRestraints = !builder.getQueryCellPar().isEmpty()
                            || !builder.getSymmetryQueryString().isEmpty()
                            || !builder.getColorQueryString().isEmpty()
                            || !builder.getQueryDensity().isEmpty()
                            || !builder.getChemicalQueryString().isEmpty()
                            || !builder.getQueryIdEntry().isEmpty();

    if (hasRestraints) {
        // Populate the candidate list with all card IDs, then filter via restraints
        QString fetchQuery = QStringLiteral("SELECT id FROM id");
        if (builder.deletedEnabled())
            fetchQuery += QStringLiteral(" WHERE trim(quality)!='D'");

        QSqlQuery q(dbMain.db());
        q.prepare(fetchQuery);
        QStringList idList;
        if (q.exec()) {
            int n = 0;
            while (q.next()) {
                if (m_token.stop_requested()) {
                    qInfo() << "Search canceled while fetching candidate ids at" << n;
                    break;
                }
                idList.append(q.value(0).toString());
                if (++n % 500 == 0)
                    if (progress) progress(0, 0);
            }
        }
        if (q.lastError().isValid()) m_readError = q.lastError().text();
        qInfo() << "makeQueryWithoutStrongest: total candidates =" << idList.size();

        int count = idList.size();
        applyRestraintsToIds(builder, idList, count);
        qInfo() << "makeQueryWithoutStrongest: after restraints =" << count;

        if (idList.isEmpty()) return;

        QString idsString;
        for (const QString &id : std::as_const(idList))
            idsString.append(QLatin1Char('\'') + id + QLatin1String("',"));
        idsString.chop(1);

        readRows(idsString, builder, count, acceptedCards, progress);
    } else {
        // No restraints: query all cards directly (idsString empty → no IN filter)
        readRows(QString(), builder, 0, acceptedCards, progress, /*noFilter=*/true);
    }

    qInfo() << "makeQueryWithoutStrongest: accepted =" << acceptedCards.size();
}

QList<QPair<QString,int>> SqlReferenceRepository::querySpaceGroups() const
{
    QList<QPair<QString,int>> result;
    auto lease = referenceLease_ ? nullptr : qualpf::infrastructure::DatabaseLease::read(basePath_);
    if ((!referenceLease_ && !lease) || !dbMain.onOwnerThread()) return result;
    if (!dbInfoStat.isOpen())
        return result;
    QSqlQuery q(dbInfoStat.db());
    q.prepare(QStringLiteral("SELECT val, n FROM stat_spgr ORDER BY n DESC"));
    if (q.exec()) {
        while (q.next())
            result.append({ q.value(0).toString(), q.value(1).toInt() });
    }
    return result;
}

QList<QPair<QString,int>> SqlReferenceRepository::queryColors() const
{
    QList<QPair<QString,int>> result;
    auto lease = referenceLease_ ? nullptr : qualpf::infrastructure::DatabaseLease::read(basePath_);
    if ((!referenceLease_ && !lease) || !dbMain.onOwnerThread()) return result;
    if (!dbInfoStat.isOpen())
        return result;
    QSqlQuery q(dbInfoStat.db());
    q.prepare(QStringLiteral("SELECT val, n FROM stat_color ORDER BY n DESC"));
    if (q.exec()) {
        while (q.next())
            result.append({ q.value(0).toString(), q.value(1).toInt() });
    }
    return result;
}

void SqlReferenceRepository::getInfo(int &ncard, QString &type)
{
    auto lease = referenceLease_ ? nullptr : qualpf::infrastructure::DatabaseLease::read(basePath_);
    if ((!referenceLease_ && !lease) || !dbMain.onOwnerThread()) return;
    QSqlQuery queryInfo(dbMain.db());
    queryInfo.prepare("SELECT ncard, type FROM infodb");

    if (queryInfo.exec()) {
        queryInfo.first();
        ncard = queryInfo.value(0).toInt();
        type = queryInfo.value(1).toString();
    }
}


}

namespace qualpf::infrastructure {
Result<application::ProductCard> SqlReferenceRepository::findById(std::string_view id, double wave, const ExecutionContext &context) {
    auto cancelled = [] { return Result<application::ProductCard>{Error{ErrorCode::cancelled, "search", "Search canceled"}}; };
    if (context.cancellation.stop_requested()) return cancelled();
    const auto info = queryCard(QString::fromUtf8(id.data(), qsizetype(id.size())));
    if (context.cancellation.stop_requested()) return cancelled();
    if (!info.valid) return Result<application::ProductCard>{Error{ErrorCode::missing_data,"search.card","Card was not found","card_not_found"}};
    application::ProductCard card;
    card.id=info.id.toStdString(); card.chemicalName=info.name.toStdString(); card.mineralName=info.mineralName.toStdString();
    card.chemicalFormula=info.chemicalFormula.toStdString(); card.quality=info.quality.toStdString();
    card.rir=info.rir.toStdString(); card.spaceGroup=info.spaceGroup.toStdString();
    card.d.assign(info.dvalues.begin(),info.dvalues.end());
    card.intensity.assign(info.intensities.begin(),info.intensities.end());
    // The ID-load contract retains all d/intensity values and omits only NaN
    // angles, exactly as CardType::setD/tthvalue_safe did before the refactor.
    for (double d : card.d) {
        const auto word = qualpf::io::powder_cif::binary32::certifiedAsin64(std::bit_cast<std::uint64_t>(0.5 * wave / d));
        const double angle = 2.0 * std::bit_cast<double>(word) * (180.0 / 3.14159265358979323846);
        if (!std::isnan(angle)) card.tth.push_back(angle);
    }
    if (context.cancellation.stop_requested()) return cancelled();
    return Result<application::ProductCard>{std::move(card)};
}
}

namespace qualpf::infrastructure {
bool SqlReferenceRepository::open(const std::string &path) {
    const bool ok = openDatabases(QString::fromStdString(path));
    if (!ok) qWarning("AppState: failed to open database: %s", path.c_str());
    return ok;
}
std::pair<int, std::string> SqlReferenceRepository::information() { int count; QString type; getInfo(count, type); return {count, type.toStdString()}; }
application::CardDetails SqlReferenceRepository::details(const std::string &id) const {
    const auto source = queryCard(QString::fromStdString(id)); application::CardDetails out;
    out.id = source.id.toStdString();
    out.name = source.name.toStdString();
    out.mineralName = source.mineralName.toStdString();
    out.chemicalFormula = source.chemicalFormula.toStdString();
    out.spaceGroup = source.spaceGroup.toStdString();
    out.quality = source.quality.toStdString();
    out.rir = source.rir.toStdString();
    out.authors = source.authors.toStdString();
    out.journal = source.journal.toStdString();
    out.journalVolume = source.journalVolume.toStdString();
    out.pageStart = source.pageStart.toStdString();
    out.pageEnd = source.pageEnd.toStdString();
    out.color = source.color.toStdString();
    out.type = source.type.toStdString();
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
    out.dvalues.assign(source.dvalues.begin(), source.dvalues.end());
    out.intensities.assign(source.intensities.begin(), source.intensities.end());
    out.h.assign(source.h.begin(), source.h.end());
    out.k.assign(source.k.begin(), source.k.end());
    out.l.assign(source.l.begin(), source.l.end());
    out.mul.assign(source.mul.begin(), source.mul.end());
    return out;
}
application::NamedCounts SqlReferenceRepository::spaceGroups() const { application::NamedCounts out; for (const auto &v : querySpaceGroups()) out.emplace_back(v.first.toStdString(), v.second); return out; }
application::NamedCounts SqlReferenceRepository::colors() const { application::NamedCounts out; for (const auto &v : queryColors()) out.emplace_back(v.first.toStdString(), v.second); return out; }
} // namespace qualpf::infrastructure
