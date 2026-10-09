// Oracle adapter: only the unchanged production Qt SQL writer is executed.
// Synthetic qualpf::testing::CifCrystalInfo inputs test storage, not crystallographic mathematics.
#include "tests/compatibility/support/cif_capture_writer.h"
#include "infrastructure/database/qt_sql/sql_schema_builder.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <cstdio>
#include <stdexcept>

namespace {
template <std::size_t N> void copy(char (&out)[N], const QString &text) {
    const auto bytes=text.toLatin1();
    if (bytes.size() >= static_cast<qsizetype>(N)) throw std::runtime_error("adapter string capacity");
    std::snprintf(out,N,"%s",bytes.constData());
}

qualpf::testing::CifCrystalInfo makeInfo(const QJsonObject &input) {
    qualpf::testing::CifCrystalInfo info;
    info.nat=2; info.zval=1; info.vol=125; info.dens=2.5f; info.mu=12.25f; info.rir=4.5f;
    info.cellpar[0]=info.cellpar[1]=info.cellpar[2]=5;
    info.cellpar[3]=info.cellpar[4]=info.cellpar[5]=90;
    copy(info.sform,QStringLiteral("O2 Si")); copy(info.spg_sym,QStringLiteral("P 1"));
    copy(info.crysys,QStringLiteral("Triclinic")); copy(info.subfile,QStringLiteral("inorganic"));
    copy(info.chem_name,input.value("name").toString(QStringLiteral("O'Brien native writer fixture")));
    copy(info.mineral_name,QStringLiteral("Writer oracle"));
    info.nelem=2; copy(info.specie_label[0],QStringLiteral("Si")); copy(info.specie_label[1],QStringLiteral("O"));
    auto d=input.value("d").toArray(); auto intensity=input.value("intensity").toArray();
    if (d.size()!=intensity.size() || d.size()>500) throw std::runtime_error("adapter row shape");
    info.nrefl=info.nrefl_print=static_cast<int>(d.size());
    for (int i=0;i<info.nrefl_print;++i) {
        info.refl_d[i]=static_cast<float>(d[i].toDouble());
        info.refl_ipct[i]=static_cast<float>(intensity[i].toDouble());
        info.refl_h[i]=i+1; info.refl_k[i]=-i; info.refl_l[i]=2*i; info.refl_mult[i]=2*(i+1);
    }
    return info;
}

QJsonObject observedInput(const QString &file,const qualpf::testing::CifCrystalInfo &info) {
    QJsonArray d,intensity,h,k,l,mul,cell,species;
    for(int i=0;i<info.nrefl_print;++i) {
        d.append(info.refl_d[i]); intensity.append(info.refl_ipct[i]);
        h.append(info.refl_h[i]);k.append(info.refl_k[i]);l.append(info.refl_l[i]);mul.append(info.refl_mult[i]);
    }
    for(float value:info.cellpar)cell.append(value);
    for(int i=0;i<info.nelem;++i)species.append(QString::fromLatin1(info.specie_label[i]));
    return {{"filename",file},{"nat",info.nat},{"z",info.zval},{"nrefl",info.nrefl},{"nrefl_print",info.nrefl_print},
            {"vol",info.vol},{"dens",info.dens},{"mu",info.mu},{"rir",info.rir},{"cell",cell},
            {"formula",QString::fromLatin1(info.sform)},{"group",QString::fromLatin1(info.spg_sym)},
            {"system",QString::fromLatin1(info.crysys)},{"subfile",QString::fromLatin1(info.subfile)},
            {"name",QString::fromLatin1(info.chem_name)},{"mineral",QString::fromLatin1(info.mineral_name)},
            {"species",species},{"d",d},{"intensity",intensity},{"h",h},{"k",k},{"l",l},{"mul",mul}};
}

QJsonValue count(const QSqlDatabase &db,const QString &table) {
    QSqlQuery query(db);
    if(!query.exec(QStringLiteral("SELECT count(*) FROM ")+table) || !query.next())
        return QJsonObject{{"error",query.lastError().text()}};
    return query.value(0).toInt();
}

QJsonObject externalCounts(const QString &base) {
    QJsonObject result;
    const QStringList suffixes={".sq",".sq.info",".sq.search"};
    const QStringList tables={"id","info","top"};
    for(int i=0;i<suffixes.size();++i) {
        const QString connection=QStringLiteral("phase19_observer_")+QString::number(i);
        {
            auto db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connection);
            db.setDatabaseName(base+suffixes[i]);db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            if(!db.open())result[tables[i]]=QJsonObject{{"error",db.lastError().text()}};
            else result[tables[i]]=count(db,tables[i]);
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
    return result;
}
}

int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3)return 2;
    QFile input(QString::fromLocal8Bit(argv[1]));
    if(!input.open(QIODevice::ReadOnly))return 3;
    QJsonParseError parseError;
    auto parsed=QJsonDocument::fromJson(input.readAll(),&parseError);
    if(parseError.error!=QJsonParseError::NoError || !parsed.isObject())return 4;
    auto request=parsed.object();
    const QString base=QString::fromLocal8Bit(argv[2]);
    qualpf::infrastructure::SqlSchemaBuilder db;
    if(!db.create(base,qualpf::infrastructure::SqlSchemaBuilder::DbType::CifFiles))return 5;
    QJsonArray observed,snapshots;
    const auto cards=request.value("cards").toArray();
    const QString fault=request.value("fault").toString();
    {
        qualpf::testing::CifCaptureWriter writer(&db);
        for(int index=0;index<cards.size();++index) {
            const auto card=cards[index].toObject();
            auto info=makeInfo(card);
            const QString filename=card.value("filename").toString();
            observed.append(observedInput(filename,info));
            if(index==0 && !fault.isEmpty()) {
                auto target=fault=="id"?db.mainDb():(fault=="info"?db.infoDb():db.searchDb());
                QSqlQuery q(target);
                if(!q.exec("CREATE TRIGGER phase19_fail BEFORE INSERT ON "+fault+
                           " BEGIN SELECT RAISE(ABORT, 'phase19 injected statement failure'); END"))return 6;
            }
            writer.onCifReady(filename,info);
            if(index==0 && !fault.isEmpty()) {
                auto target=fault=="id"?db.mainDb():(fault=="info"?db.infoDb():db.searchDb());
                QSqlQuery q(target);if(!q.exec("DROP TRIGGER phase19_fail"))return 7;
            }
            if(request.value("snapshot_after").toArray().contains(index+1))
                snapshots.append(QJsonObject{{"after_cards",index+1},{"external",externalCounts(base)},
                    {"internal",QJsonObject{{"id",count(db.mainDb(),"id")},{"info",count(db.infoDb(),"info")},{"top",count(db.searchDb(),"top")}}}});
        }
        if(request.value("finish").toBool(true)) {
            writer.onFinished(static_cast<int>(cards.size()));
            snapshots.append(QJsonObject{{"stage","finished"},{"external",externalCounts(base)}});
        }
    }
    db.close();
    snapshots.append(QJsonObject{{"stage","closed"},{"external",externalCounts(base)}});
    if(request.value("recreate").toBool(false)) {
        if(!db.create(base,qualpf::infrastructure::SqlSchemaBuilder::DbType::CifFiles))return 8;
        db.close();
        snapshots.append(QJsonObject{{"stage","recreated"},{"external",externalCounts(base)}});
    }
    QJsonObject result{{"inputs",observed},{"snapshots",snapshots}};
    std::printf("WRITER_JSON %s\n",QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    return 0;
}
