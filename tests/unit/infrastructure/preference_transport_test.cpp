#include "gui/adapters/preferences.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/settings_migration.h"
#include <QCoreApplication>
#include <QSettings>
#include <QPen>
#include <QColor>
#include <stdexcept>
void opaquePreferenceChecks(const QString &root) {
    using qualpf::gui::Preferences;
    const auto check=[](bool ok) { if(!ok) throw std::runtime_error("Preference key/type/transport changed"); };
    QCoreApplication::setOrganizationName("QualPF.PreferenceTransport.Test");
    QCoreApplication::setApplicationName("qualpf");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root+"/transport");
    QPen pen(QColor(12,34,56,78),3.25,Qt::DashDotLine,Qt::RoundCap,Qt::BevelJoin);
    pen.setCosmetic(true);
    const QList<QPair<QString,QVariant>> values{{"PlotSettings/xAxisPen",pen},
        {"PlotSettings/backColor",QColor(7,11,13,17)},
        {"mainwindow/state",QByteArray::fromHex("00ff5a010080")},
        {"mainwindow/geometry",QByteArray::fromHex("00010000ff")},
        {"SearchOptions/maxEntries",QString("003000")},
        {"SearchOptions/minFom",.35},{"flags/visible",true}};
    QSettings source(root+"/transport-legacy.ini",QSettings::IniFormat), destination;
    for(const auto &[key,value]:values) source.setValue(key,value);
    source.sync();
    check(qualpf::infrastructure::migrateUserSettings(source,destination).completed);
    qualpf::infrastructure::QtPreferenceStore store;
    Preferences preferences(store);
    check(preferences.fileName()==destination.fileName());
    for(const auto &[key,value]:values) {
        const auto read=preferences.value(key);
        check(read.metaType()==value.metaType() && read==value);
        // Independently instantiated transports must agree on Qt_6_0 bytes.
        check(Preferences::encode(value)==qualpf::infrastructure::QtPreferenceStore::encode(value));
        preferences.setValue(key,value);
        QSettings actual;
        check(actual.value(key).metaType()==value.metaType() && actual.value(key)==value);
    }
    check(preferences.value("missing",pen).value<QPen>()==pen);
    preferences.beginWriteArray("qualxrecentfiles");
    for(int i=0;i<2;++i) {
        preferences.setArrayIndex(i);
        preferences.setValue("FileName",QString("file%1").arg(i));
        preferences.setValue("FileType","Project");
    }
    preferences.endArray();
    QSettings actual;
    check(actual.beginReadArray("qualxrecentfiles")==2);
    actual.setArrayIndex(1);
    check(actual.value("FileName").toString()=="file1");
    actual.endArray();
    check(preferences.beginReadArray("qualxrecentfiles")==2);
    preferences.setArrayIndex(0);check(preferences.value("FileName").toString()=="file0");
    preferences.endArray();
    check(actual.value("qualxrecentfiles/size").metaType().id()==QMetaType::Int);
    check(actual.value("qualxrecentfiles/2/FileType").toString()=="Project");
}
