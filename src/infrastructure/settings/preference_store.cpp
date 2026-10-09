#include "infrastructure/settings/preference_store.h"
#include <QDataStream>
#include <QIODevice>
#include <QSettings>
#include <cstring>
namespace qualpf::infrastructure {
application::PreferencePayload QtPreferenceStore::encode(const QVariant &value) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << value;
    application::PreferencePayload out(static_cast<std::size_t>(bytes.size()));
    if (!out.empty()) std::memcpy(out.data(), bytes.constData(), out.size());
    return out;
}
QVariant QtPreferenceStore::decode(const application::PreferencePayload &value) {
    QDataStream stream(QByteArray(reinterpret_cast<const char *>(value.data()), qsizetype(value.size())));
    stream.setVersion(QDataStream::Qt_6_0);
    QVariant out;
    stream >> out;
    return stream.status() == QDataStream::Ok ? out : QVariant{};
}
application::PreferencePayload QtPreferenceStore::read(const std::string &key, const application::PreferencePayload &fallback) const {
    QSettings settings;
    return encode(settings.value(QString::fromStdString(key), decode(fallback)));
}
void QtPreferenceStore::write(const std::string &key, const application::PreferencePayload &value) {
    QSettings().setValue(QString::fromStdString(key), decode(value));
}
bool QtPreferenceStore::contains(const std::string &key) const { return QSettings().contains(QString::fromStdString(key)); }
void QtPreferenceStore::remove(const std::string &key) { QSettings().remove(QString::fromStdString(key)); }
std::string QtPreferenceStore::fileName() const { return QSettings().fileName().toStdString(); }
} // namespace qualpf::infrastructure
