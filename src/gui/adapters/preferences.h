#pragma once
#include "qualpf/application/preferences.h"
#include <QDataStream>
#include <QIODevice>
#include <QVariant>
#include <algorithm>
#include <cstring>
#include <vector>

namespace qualpf::gui {
// Qt presentation transport. QSettings and its on-disk QVariant format remain in Infrastructure.
class Preferences {
  public:
    explicit Preferences(application::PreferenceStore &store) : store_(store) {}
    QVariant value(const QString &key, const QVariant &fallback = {}) const {
        const auto payload = store_.read(fullKey(key).toStdString(), encode(fallback));
        return decode(payload);
    }
    void setValue(const QString &key, const QVariant &value) { store_.write(fullKey(key).toStdString(), encode(value)); }
    bool contains(const QString &key) const { return store_.contains(fullKey(key).toStdString()); }
    void remove(const QString &key) { store_.remove(fullKey(key).toStdString()); }
    QString fileName() const { return QString::fromStdString(store_.fileName()); }
    void beginGroup(const QString &group) { scopes_.push_back({group}); }
    void endGroup() { scopes_.pop_back(); }
    int beginReadArray(const QString &name) {
        beginGroup(name);
        const int size = value("size", 0).toInt();
        scopes_.back().array = true;
        return size;
    }
    void beginWriteArray(const QString &name, int size = -1) {
        beginGroup(name);
        if (size >= 0) setValue("size", size);
        scopes_.back().array = true;
        scopes_.back().writeSize = size < 0;
    }
    void setArrayIndex(int index) {
        scopes_.back().index = index + 1;
        scopes_.back().maximum = std::max(scopes_.back().maximum, index + 1);
    }
    void endArray() {
        const auto scope = scopes_.back();
        scopes_.back().array = false;
        if (scope.writeSize) setValue("size", scope.maximum);
        endGroup();
    }
    static application::PreferencePayload encode(const QVariant &value) {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream << value;
        application::PreferencePayload out(static_cast<std::size_t>(bytes.size()));
        if (!out.empty()) std::memcpy(out.data(), bytes.constData(), out.size());
        return out;
    }
    static QVariant decode(const application::PreferencePayload &value) {
        QDataStream stream(QByteArray(reinterpret_cast<const char *>(value.data()), qsizetype(value.size())));
        stream.setVersion(QDataStream::Qt_6_0);
        QVariant out;
        stream >> out;
        return stream.status() == QDataStream::Ok ? out : QVariant{};
    }
  private:
    struct Scope { QString name; bool array = false, writeSize = false; int index = 0, maximum = 0; };
    QString fullKey(const QString &key) const {
        QString result;
        for (const auto &scope : scopes_) {
            result += scope.name + '/';
            if (scope.array) result += QString::number(scope.index) + '/';
        }
        return result + key;
    }
    application::PreferenceStore &store_;
    std::vector<Scope> scopes_;
};
} // namespace qualpf::gui
