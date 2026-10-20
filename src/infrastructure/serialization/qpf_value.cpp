#include "qpf_value.h"
#include <QCborStreamReader>
#include <QCborStreamWriter>
#include <QCryptographicHash>
#include <cmath>
#include <limits>

namespace qualpf::infrastructure::qpf {
namespace {
void write(QCborStreamWriter &w, const Value &v) {
    std::visit(
        [&](const auto &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Undefined>)
                w.append(QCborSimpleType::Undefined);
            else if constexpr (std::is_same_v<T, std::nullptr_t>)
                w.append(QCborSimpleType::Null);
            else if constexpr (std::is_same_v<T, std::int64_t>)
                w.append(qint64(x));
            else if constexpr (std::is_same_v<T, std::uint64_t>)
                w.append(quint64(x));
            else if constexpr (std::is_same_v<T, std::string>)
                w.append(QString::fromStdString(x));
            else if constexpr (std::is_same_v<T, Value::Array>) {
                w.startArray(quint64(x.size()));
                for (const auto &e : x)
                    write(w, e);
                w.endArray();
            } else if constexpr (std::is_same_v<T, Value::Map>) {
                w.startMap(quint64(x.size()));
                for (const auto &[k, e] : x) {
                    w.append(QString::fromStdString(k));
                    write(w, e);
                }
                w.endMap();
            } else
                w.append(x);
        },
        v.data);
}
Value read(QCborStreamReader &r, Budget &budget, unsigned depth) {
    if (depth > budget.options.maximum_depth)
        invalid("project.cbor", "CBOR nesting limit exceeded", "project_resource_limit");
    budget.charge(sizeof(Value));
    Value out;
    if (r.isUnsignedInteger()) {
        out = std::uint64_t(r.toUnsignedInteger());
        r.next();
    } else if (r.isNegativeInteger()) {
        const auto magnitude = quint64(r.toNegativeInteger());
        if (!magnitude || magnitude > (quint64(1) << 63))
            invalid("project.integer");
        out = -std::int64_t(magnitude - 1) - 1;
        r.next();
    } else if (r.isDouble()) {
        out = r.toDouble();
        r.next();
    } else if (r.isFloat() || r.isFloat16())
        invalid("project.float", "QPF requires binary64 scalar numbers");
    else if (r.isBool()) {
        out = r.toBool();
        r.next();
    } else if (r.isNull()) {
        out = nullptr;
        r.next();
    } else if (r.isUndefined()) {
        out = Undefined{};
        r.next();
    } else if (r.isString()) {
        if (!r.isLengthKnown())
            invalid("project.cbor", "Indefinite strings are not supported");
        budget.charge(r.length());
        QString result;
        if (!r.readAndAppendToString(result))
            invalid("project.cbor");
        out = result.toStdString();
    } else if (r.isByteArray()) {
        if (!r.isLengthKnown())
            invalid("project.cbor");
        budget.charge(r.length());
        QByteArray result;
        if (!r.readAndAppendToByteArray(result))
            invalid("project.cbor");
        out = result;
    } else if (r.isArray() || r.isMap()) {
        if (!r.isLengthKnown())
            invalid("project.cbor");
        const bool map = r.isMap();
        const auto count = r.length();
        if (count > (budget.options.maximum_decoded_bytes - budget.used) / sizeof(Value))
            invalid("project.cbor", "CBOR container exceeds budget", "project_resource_limit");
        if (!r.enterContainer())
            invalid("project.cbor");
        if (map) {
            Value::Map m;
            for (quint64 i = 0; i < count; ++i) {
                const auto key = read(r, budget, depth + 1).as<std::string>();
                auto item = read(r, budget, depth + 1);
                if (!m.emplace(key, std::move(item)).second)
                    invalid("project.cbor", "Duplicate map key");
            }
            out = std::move(m);
        } else {
            Value::Array a;
            a.reserve(std::size_t(count));
            for (quint64 i = 0; i < count; ++i)
                a.push_back(read(r, budget, depth + 1));
            out = std::move(a);
        }
        if (!r.leaveContainer())
            invalid("project.cbor");
    } else
        invalid("project.cbor", "Unsupported or malformed CBOR value");
    if (r.lastError() != QCborError::NoError)
        invalid("project.cbor", "Truncated or malformed CBOR");
    return out;
}
} // namespace
QByteArray cbor(const Value &v) {
    QByteArray b;
    QCborStreamWriter w(&b);
    write(w, v);
    return b;
}
Value parse(const QByteArray &b, Budget &budget) {
    QCborStreamReader r(b);
    auto out = read(r, budget, 0);
    if (r.currentOffset() != b.size() || r.hasNext())
        invalid("project.cbor", "Trailing CBOR data");
    return out;
}
std::string digest(const QByteArray &b) {
    return QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex().toStdString();
}
std::uint64_t unsignedValue(const Value &v, const std::string &path) {
    if (const auto *p = std::get_if<std::uint64_t>(&v.data))
        return *p;
    if (const auto *p = std::get_if<std::int64_t>(&v.data); p && *p >= 0)
        return std::uint64_t(*p);
    invalid(path, "Expected an unsigned integer");
}
Value metadata(const application::MetadataValue &v) {
    return std::visit(
        [](const auto &x) -> Value {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, application::UndefinedValue>)
                return Undefined{};
            else if constexpr (std::is_same_v<T, application::MetadataArray>) {
                Value::Array a;
                for (const auto &e : x)
                    a.push_back(metadata(e));
                return a;
            } else if constexpr (std::is_same_v<T, application::MetadataObject>) {
                Value::Map m;
                for (const auto &[k, e] : x)
                    m.emplace(k, metadata(e));
                return m;
            } else
                return x;
        },
        v.data);
}
application::MetadataValue metadata(const Value &v) {
    return std::visit(
        [](const auto &x) -> application::MetadataValue {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Undefined>)
                return {};
            else if constexpr (std::is_same_v<T, QByteArray>)
                invalid("project.metadata", "Byte arrays require an opaque extension");
            else if constexpr (std::is_same_v<T, std::uint64_t>) {
                return application::MetadataValue{x};
            } else if constexpr (std::is_same_v<T, Value::Array>) {
                application::MetadataArray a;
                for (const auto &e : x)
                    a.push_back(metadata(e));
                return a;
            } else if constexpr (std::is_same_v<T, Value::Map>) {
                application::MetadataObject m;
                for (const auto &[k, e] : x)
                    m.emplace(k, metadata(e));
                return m;
            } else
                return x;
        },
        v.data);
}
std::string objectDigest(const std::string &id, const Object &o) {
    return digest(cbor(Value::Array{Value(id), Value(o.kind), Value(o.version), o.fields}));
}
std::string arrayUnit(const std::string &path) {
    const auto key = path.substr(path.find_last_of('.') + 1);
    if (key == "two_theta" || key == "tth" || key == "corrected_two_theta" || key == "coordinates" ||
        key == "shifts" || key == "fwhm")
        return "degree";
    if (key == "d" || key == "delta_d" || key == "deltaD")
        return "angstrom";
    if (key == "observed" || key == "original_observed" || key == "background" || key == "calculated" ||
        key == "residual" || key == "intensity" || key == "intensity_orig" || key == "standard_deviations")
        return "intensity";
    if (key == "values") {
        if (path.find(".background.") != std::string::npos ||
            path.find(".calculated.") != std::string::npos ||
            path.find(".background_subtracted_signal.") != std::string::npos)
            return "intensity";
        return "contextual";
    }
    if (key == "weights")
        return "weight";
    if (key == "diagonal")
        return "squared_intensity";
    if (key == "counts")
        return "count";
    if (key == "seconds")
        return "second";
    if (key == "percentages")
        return "percent";
    return "dimensionless";
}
} // namespace qualpf::infrastructure::qpf
