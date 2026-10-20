#pragma once
#include "qualpf/application/product_values.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

namespace qualpf::infrastructure {
inline application::MetadataValue metadataValue(const QJsonValue &value) {
    using namespace application;
    switch (value.type()) {
    case QJsonValue::Undefined: return {};
    case QJsonValue::Null: return MetadataValue{nullptr};
    case QJsonValue::Bool: return MetadataValue{value.toBool()};
    case QJsonValue::Double:
        if (value.toVariant().metaType().id() == QMetaType::LongLong)
            return MetadataValue{std::int64_t(value.toInteger())};
        return MetadataValue{value.toDouble()};
    case QJsonValue::String: return MetadataValue{value.toString().toStdString()};
    case QJsonValue::Array: {
        MetadataArray out;
        for (const auto &entry : value.toArray()) out.push_back(metadataValue(entry));
        return MetadataValue{std::move(out)};
    }
    case QJsonValue::Object: {
        MetadataObject out;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            out.emplace(it.key().toStdString(), metadataValue(it.value()));
        return MetadataValue{std::move(out)};
    }
    }
    return {};
}

inline QJsonValue jsonValue(const application::MetadataValue &value) {
    return std::visit([](const auto &entry) -> QJsonValue {
        using T = std::decay_t<decltype(entry)>;
        if constexpr (std::is_same_v<T, application::UndefinedValue>) return QJsonValue::Undefined;
        else if constexpr (std::is_same_v<T, std::nullptr_t>) return QJsonValue::Null;
        else if constexpr (std::is_same_v<T, std::string>) return QString::fromStdString(entry);
        else if constexpr (std::is_same_v<T, std::int64_t>) return QJsonValue(qint64(entry));
        // JSON has no full uint64 integer domain. This legacy/display bridge
        // emits exact decimal text; QPF serializes the native integer directly.
        else if constexpr (std::is_same_v<T, std::uint64_t>) return QString::number(qulonglong(entry));
        else if constexpr (std::is_same_v<T, application::MetadataArray>) {
            QJsonArray out;
            for (const auto &element : entry) out.append(jsonValue(element));
            return out;
        } else if constexpr (std::is_same_v<T, application::MetadataObject>) {
            QJsonObject out;
            for (const auto &[key, element] : entry)
                out.insert(QString::fromStdString(key), jsonValue(element));
            return out;
        } else return QJsonValue(entry);
    }, value.data);
}
} // namespace qualpf::infrastructure
