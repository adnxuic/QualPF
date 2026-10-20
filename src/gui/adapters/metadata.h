#pragma once
#include "qualpf/application/product_values.h"
#include <QVariant>
namespace qualpf::gui {
inline QVariant metadataVariant(const application::MetadataValue &value) {
    return std::visit([](const auto &v)->QVariant {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,std::string>) return QString::fromStdString(v);
        else if constexpr(std::is_same_v<T,std::int64_t>) return QVariant::fromValue(qlonglong(v));
        else if constexpr(std::is_same_v<T,std::uint64_t>) return QVariant::fromValue(qulonglong(v));
        else if constexpr(std::is_same_v<T,bool> || std::is_same_v<T,double>) return v;
        else if constexpr(std::is_same_v<T,application::MetadataArray>) {
            QVariantList out; for(const auto &e:v) out.append(metadataVariant(e)); return out;
        } else if constexpr(std::is_same_v<T,application::MetadataObject>) {
            QVariantMap out; for(const auto &[k,e]:v) out.insert(QString::fromStdString(k),metadataVariant(e)); return out;
        } else return {};
    },value.data);
}
} // namespace qualpf::gui
