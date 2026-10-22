#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "classic_fixture.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <bit>
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location where = std::source_location::current()) {
    if (!v)
        throw std::runtime_error("CandidateReader contract failed at line " + std::to_string(where.line()));
}
static void file(const QString &path, const QStringList &statements) {
    const QString name = "fixture-writer";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        check(db.open());
        for (const auto &sql : statements) {
            QSqlQuery q(db);
            if (!q.exec(sql))
                throw std::runtime_error(q.lastError().text().toStdString());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
static void scoredStages(const QString &directory) {
    const auto base = directory + "/scored";
    file(base + ".sq", {"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup "
                        "TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER)",
                        "INSERT INTO id VALUES('20','first','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('10','second','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('30','deleted classic top','m','F','P1',' D ','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('40','deleted alternate top','m','F','P1',' D ','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('50','last','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('60','null quality','m','F','P1',NULL,'1',1,'2','100',1)"});
    file(base + ".sq.info", {"CREATE TABLE info(id TEXT)"});
    file(base + ".sq.infostat", {"CREATE TABLE unused(id TEXT)"});
    file(base + ".sq.search", {"CREATE TABLE top(id TEXT,n INTEGER,dval TEXT)",
                               "INSERT INTO top VALUES('20',1,'99')", "INSERT INTO top VALUES('10',1,'9.090909090909092')",
                               "INSERT INTO top VALUES('30',1,'9.090909090909092')", "INSERT INTO top VALUES('40',1,'99')",
                               "INSERT INTO top VALUES('50',1,'99')", "INSERT INTO top VALUES('60',1,'9.090909090909092')"});
    infrastructure::SqlReferenceRepository reader; check(reader.openDatabases(base));
    testing::ClassicFixture fixture; auto peaks = fixture.peaks; peaks.effective_wavelength = WavelengthAngstrom{1.54};
    ProductState state; state.pattern = fixture.pattern; state.peaks = productMeasuredPeaks(peaks).value();
    SearchInput input; input.operation = SearchOperation::scored_browse; input.threshold = -1;
    input.parameters = fixture.parameters; input.query.wavelength = 1.54; input.query.excludeDeleted = true;
    input.query.route = CandidateQuery::Route::stored_strongest; input.source = std::make_shared<const ProductState>(state);
    auto original = SearchEngine{}.run(input, reader, fixture.context);
    check(original && original.value().next.candidates.size() == 1 && original.value().next.candidates.front().id == "10" &&
          original.value().next.candidates.front().fomCalculated);
    // Fixed quality checking retains the SQL NULL/deleted predicate and database order.
    input.query.preselectedIds = std::vector<std::string>{"20","40","50"};
    auto preselected = SearchEngine{}.run(input, reader, fixture.context);
    check(preselected && preselected.value().next.candidates.size() == 2 &&
          preselected.value().next.candidates[0].id == "20" && preselected.value().next.candidates[1].id == "50");
    input.query.preselectedIds = std::vector<std::string>{};
    auto empty = SearchEngine{}.run(input, reader, fixture.context);
    check(empty && empty.value().next.candidates.empty());
    input.query.preselectedIds.reset(); input.query.excludeDeleted = false;
    auto included = SearchEngine{}.run(input, reader, fixture.context);
    check(included && included.value().next.candidates.size() == 3 &&
          included.value().next.candidates[0].id == "10" && included.value().next.candidates[1].id == "30" &&
          included.value().next.candidates[2].id == "60");
    const std::vector<double> d{3}, delta{.1}; const std::vector<DSpacingAngstrom> top{DSpacingAngstrom{3}};
    check(search::passes_stored_strongest(d, delta, top, fixture.context).value());
    check(search::accepts_record_quality("A", false, fixture.context).value() &&
          !search::accepts_record_quality(" D ", false, fixture.context).value());

}
static void pfsmPairs(const QString &directory) {
    const auto base=directory+"/pfsm";
    file(base+".sq",{"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER,nd INTEGER)",
        "INSERT INTO id VALUES('10','','','','','C','',1,'0.1,2,','900,100,',1,2)",
        "INSERT INTO id VALUES('20','','','','','C','',1,'2,bad,3,','100,1,20,',2,3)",
        "INSERT INTO id VALUES('30','','','','','C','',1,'2,,3,','100,1,20,',3,3)",
        "INSERT INTO id VALUES('40','','','','','C','',1,'2,3,','100,20,',4,3)",
        "INSERT INTO id VALUES('50','','','','','C','',1,NULL,NULL,5,0)",
        "INSERT INTO id VALUES('60','','','','','C',' 2.5 ',1,'2,','100,',6,1)",
        "INSERT INTO id VALUES('70','','','','','C','bad',1,'2,','100,',7,1)",
        "INSERT INTO id VALUES('80','','','','','C','',1,'2,','-0,',8,1)",
        "INSERT INTO id VALUES('90','','','','','C','',1,'2,','100,',9,2147483647)"});
    for(const auto &suffix:{".sq.info",".sq.infostat",".sq.search"})file(base+suffix,{"CREATE TABLE unused(id TEXT)"});
    infrastructure::SqlReferenceRepository reader;check(reader.openDatabases(base));
    CandidateQuery query;query.route=CandidateQuery::Route::constrained_browse;query.strictPairedReflections=true;
    query.preselectedIds=std::vector<std::string>{"10"};
    const ExecutionContext context{{},NumericalPolicy::explicit_float64,{}};
    auto result=reader.read(query,context,{});
    check(result&&result.value().size()==1&&result.value()[0].d==std::vector<double>({.1,2})&&
        result.value()[0].intensity==std::vector<double>({900,100})&&result.value()[0].tth.empty());
    check(!reader.readStatistics());
    query.measure_read=true;
    const auto measured=reader.read(query,context,{});
    check(measured && measured.value()[0].d==result.value()[0].d &&
          measured.value()[0].intensity==result.value()[0].intensity);
    const auto statistics=reader.readStatistics();
    check(statistics && statistics->forward_only && statistics->cards==1 && statistics->peaks==2 &&
          statistics->row_query_executions==1 && statistics->peak_text_bytes==14 &&
          statistics->peak_buffer_growths>0 && statistics->peaks_seconds>=0);
    query.measure_read=false;
    for(const auto *id:{"20","30","40","90"}) {
        query.preselectedIds=std::vector<std::string>{id};result=reader.read(query,context,{});
        check(!result&&result.error().message.find(id)!=std::string::npos);
    }
    query.strictPairedReflections=false;query.preselectedIds=std::vector<std::string>{"20"};
    result=reader.read(query,context,{});check(result&&result.value()[0].d.size()==2); // Retained Classical decoder.
    query.strictPairedReflections=true;query.preselectedIds=std::vector<std::string>{"50"};
    result=reader.read(query,context,{});check(result&&result.value()[0].d.empty()&&result.value()[0].intensity.empty());
    check(!result.value()[0].referenceRir);
    query.preselectedIds=std::vector<std::string>{"60"}; result=reader.read(query,context,{});
    check(result && result.value()[0].referenceRir==2.5 &&
          QString::fromStdString(result.value()[0].rir)==QStringLiteral(" 2.5 "));
    query.preselectedIds=std::vector<std::string>{"70"}; result=reader.read(query,context,{});
    check(result && !result.value()[0].referenceRir && result.value()[0].rir=="bad");
    query.preselectedIds=std::vector<std::string>{"80"};result=reader.read(query,context,{});
    check(result&&result.value()[0].intensity[0]==0&&std::signbit(result.value()[0].intensity[0]));
    std::stop_source stop;auto canceled=context;canceled.cancellation=stop.get_token();
    const auto stopped=reader.read(query,canceled,[&](auto,auto){stop.request_stop();});
    check(!stopped&&stopped.error().code==ErrorCode::cancelled);
}
static void numericRir(const QString &directory) {
    const auto base=directory+"/numeric-rir";
    file(base+".sq", {"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir REAL,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER,nd INTEGER)",
        "INSERT INTO id VALUES('1','','','','','C',1.2345678901234567,1,'2,','100,',1,1)"});
    for (const auto &suffix : {".sq.info",".sq.infostat",".sq.search"}) file(base+suffix,{"CREATE TABLE unused(id TEXT)"});
    infrastructure::SqlReferenceRepository reader; check(reader.openDatabases(base));
    CandidateQuery query; query.route=CandidateQuery::Route::constrained_browse;
    query.strictPairedReflections=true; query.preselectedIds=std::vector<std::string>{"1"};
    constexpr double expected=0x1.3c0ca428c59fbp+0; // Independently specified nearest binary64 to the SQL decimal.
    const ExecutionContext context{{},NumericalPolicy::explicit_float64,{}};
    const auto pfsm=reader.read(query,context,{});
    check(pfsm && pfsm.value()[0].referenceRir==expected);
    check(QString::fromStdString(pfsm.value()[0].rir)==QVariant(expected).toString());
    query.strictPairedReflections=false;
    const auto classic=reader.read(query,context,{});
    check(classic && !classic.value()[0].referenceRir && classic.value()[0].rir==pfsm.value()[0].rir);
}
static std::optional<std::vector<double>> priorPairedDecoder(const QByteArray &bytes,bool spacing) {
    if(bytes.trimmed().isEmpty())return std::vector<double>{};
    auto parts=QString::fromUtf8(bytes).split(',',Qt::KeepEmptyParts);
    if(!parts.empty()&&parts.back().trimmed().isEmpty())parts.removeLast();
    std::vector<double> values;
    for(const auto &part:parts) {
        bool ok=false;const auto v=part.toDouble(&ok);
        if(!ok||!std::isfinite(v)||(spacing?v<=0:v<0))return {};
        values.push_back(v);
    }
    return values;
}
static bool sameBits(const std::vector<double> &a,const std::vector<double> &b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(std::bit_cast<std::uint64_t>(a[i])!=std::bit_cast<std::uint64_t>(b[i]))return false;
    return true;
}
static void streamingDecoder(const QString &directory) {
    const auto base=directory+"/stream";
    file(base+".sq",{"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir REAL,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER,nd INTEGER)"});
    const std::vector<QByteArray> tokens={"", " ", "2", "2,", "2, \t", "2,,", ",", ",2", "2, ,3",
        "2,3", " 2.0000000000000004 ,3e-1,", "2,nan", "2,inf", "2,1e309", "2,1e-400", "2,-1", "-0", "0",
        "2\xC2\xA0", "\xC2\xA0", "2,\xC2\xA0", "2,\xff", "1.2345678901234567,+2E+0,", "1,234"};
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","stream-writer");db.setDatabaseName(base+".sq");check(db.open());
        for(std::size_t i=0;i<tokens.size();++i) {
            auto d=priorPairedDecoder(tokens[i],true);
            QSqlQuery q(db);q.prepare("INSERT INTO id VALUES(?,'','','','','C',1.2345678901234567,1,?,?,1,?)");
            q.addBindValue(QString::number(i));q.addBindValue(tokens[i]);q.addBindValue(tokens[i]);
            q.addBindValue(qint64(d?d->size():1));check(q.exec());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("stream-writer");
    for(const auto suffix:{".sq.info",".sq.infostat",".sq.search"})file(base+suffix,{"CREATE TABLE unused(id TEXT)"});
    infrastructure::SqlReferenceRepository reader;check(reader.openDatabases(base));
    CandidateQuery query;query.route=CandidateQuery::Route::constrained_browse;query.strictPairedReflections=true;
    const ExecutionContext context{{},NumericalPolicy::explicit_float64,{}};
    for(std::size_t i=0;i<tokens.size();++i) {
        query.preselectedIds=std::vector<std::string>{std::to_string(i)};
        const auto result=reader.read(query,context,{});
        const auto d=priorPairedDecoder(tokens[i],true),intensity=priorPairedDecoder(tokens[i],false);
        check(bool(result)==(bool(d)&&bool(intensity)));
        if(result)check(result.value().size()==1&&sameBits(result.value()[0].d,*d)&&sameBits(result.value()[0].intensity,*intensity));
    }
}
// Optional real-library differential audit; never a normal-test dependency or
// a product-generated golden. The preoptimization Qt decoder is kept above.
static void fullLibraryAudit(const QString &path) {
    const auto base=path.endsWith(".sq")?path.left(path.size()-3):path;
    infrastructure::SqlReferenceRepository reader;check(reader.openDatabases(base));
    CandidateQuery request;request.route=CandidateQuery::Route::constrained_browse;request.strictPairedReflections=true;
    const auto cards=reader.read(request,{{},NumericalPolicy::explicit_float64,{}},{});check(bool(cards));
    std::size_t rows=0,peaks=0;
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","audit-reader");db.setDatabaseName(base+".sq");
        db.setConnectOptions("QSQLITE_OPEN_READONLY");check(db.open());
        QSqlQuery query(db);query.setForwardOnly(true);
        check(query.exec("SELECT id,dvalue,intensita,nd FROM id"));
        while(query.next()) {
            check(rows<cards.value().size());const auto &card=cards.value()[rows++];
            auto d=priorPairedDecoder(query.value(1).toByteArray(),true);
            auto intensity=priorPairedDecoder(query.value(2).toByteArray(),false);
            check(d&&intensity&&card.id==query.value(0).toString().toStdString()&&
                  sameBits(card.d,*d)&&sameBits(card.intensity,*intensity)&&card.d.size()==query.value(3).toULongLong());
            peaks+=card.d.size();
        }
        check(!query.lastError().isValid()&&rows==cards.value().size());query.finish();db.close();
    }
    QSqlDatabase::removeDatabase("audit-reader");
    std::cout<<"PASS: full-library ordered bitwise paired decoder audit: "<<rows<<" cards, "<<peaks<<" peaks\n";
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid());
    numericRir(dir.path());
    streamingDecoder(dir.path());
    if(argc==3&&std::string(argv[1])=="--full-library")fullLibraryAudit(QString::fromLocal8Bit(argv[2]));
    const auto base = dir.path() + "/cards";
    file(base + ".sq", {"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup "
                        "TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER)",
                        "INSERT INTO id VALUES('10','first','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('20','second','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('30','deleted','m','F','P1',' D ','1',1,'2','100',1)"});
    file(base + ".sq.info", {"CREATE TABLE info(id TEXT)"});
    file(base + ".sq.infostat", {"CREATE TABLE unused(id TEXT)"});
    file(base + ".sq.search",
         {"CREATE TABLE top(id TEXT,n INTEGER,dval TEXT)", "INSERT INTO top VALUES('10',1,'3')",
          "INSERT INTO top VALUES('20',1,'9')", "INSERT INTO top VALUES('30',1,'3')"});
    qualpf::infrastructure::SqlReferenceRepository reader;
    check(reader.openDatabases(base));
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "repository-test"};
    CandidateQuery query;
    query.wavelength = 1.54;
    auto all = reader.read(query, context, {});
    check(bool(all) && all.value().size() == 3);
    check(all.value()[0].id == "10" && !all.value()[0].fomCalculated && all.value()[0].fom == 0 &&
          all.value()[0].tth.size() == 1);
    query.excludeDeleted = true;
    query.route = CandidateQuery::Route::stored_strongest;
    query.d = {3};
    query.deltaD = {.1};
    auto strongest = reader.read(query, context, {});
    check(bool(strongest) && strongest.value().size() == 1 && strongest.value()[0].id == "10");
    // Stored top controls filtering; decoding d=2 does not replace its saved d=3.
    auto indices = reader.storedTop(context, {});
    check(bool(indices) && indices.value().size() == 3 && indices.value()[0].id == "10" &&
          indices.value()[0].strongest.front().value == 3 && indices.value()[1].strongest.front().value == 9);
    SearchInput input;
    input.operation = SearchOperation::metadata_browse;
    input.source = std::make_shared<const ProductState>();
    input.query = query;
    auto planned = SearchEngine{}.run(input, reader, context);
    check(bool(planned) && planned.value().next.candidates.size() == 1 && planned.value().next.candidates[0].id == "10");
    input.query.d = {9};
    auto alternate = SearchEngine{}.run(input, reader, context);
    check(alternate && alternate.value().next.candidates.size() == 1 && alternate.value().next.candidates[0].id == "20");
    query.route = CandidateQuery::Route::constrained_browse;
    query.ids = {"20"};
    query.decodeReflections = false;
    auto metadata = reader.read(query, context, {});
    check(bool(metadata) && metadata.value().size() == 1 && metadata.value()[0].tth.empty() &&
          metadata.value()[0].d.empty());
    std::stop_source stop;
    context.cancellation = stop.get_token();
    auto cancelled = reader.read(query, context, [&](auto, auto) { stop.request_stop(); });
    check(!cancelled && cancelled.error().code == ErrorCode::cancelled);
    context.cancellation = {};
    query.elements = "Fe";
    query.ids.clear();
    // Missing chemical table is a failed query, never successful empty output.
    auto failed = reader.read(query, context, {});
    check(!failed);
    reader.closeDatabeses();
    scoredStages(dir.path());
    pfsmPairs(dir.path());
    std::cout << "PASS: owned unscored rows, saved strongest index, metadata browse, cancellation and SQL "
                 "failure\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
