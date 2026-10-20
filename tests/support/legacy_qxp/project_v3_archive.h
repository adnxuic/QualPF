#pragma once
#include "infrastructure/serialization/metadata_values.h"
#include "project_v3_schema.h"
#include "infrastructure/serialization/workflow_settings.h"
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <limits>
#include <type_traits>
namespace qualpf::infrastructure::legacy_fixture::qxp3 {
struct Failure {
    Error error;
};
[[noreturn]] inline void invalid(const std::string &path, const char *message = "Invalid QXP v3 value") {
    throw Failure{{ErrorCode::invalid_input, path, message, "invalid_project"}};
}
template <class T> T take(Result<T> result) {
    if (!result)
        throw Failure{result.error()};
    return std::move(result).value();
}
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {
    using Value = T;
};
template <class T> struct Shared : std::false_type {};
template <class T> struct Shared<std::shared_ptr<T>> : std::true_type {
    using Value = std::remove_const_t<T>;
};
template <class T> struct Vector : std::false_type {};
template <class T> struct Vector<std::vector<T>> : std::true_type {
    using Value = T;
};
template <class T> struct Array : std::false_type {};
template <class T, std::size_t N> struct Array<std::array<T, N>> : std::true_type {
    using Value = T;
};
template <class T> struct Map : std::false_type {};
template <class T, class C> struct Map<std::map<std::string, T, C>> : std::true_type {
    using Value = T;
};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct ScalarWrapper : std::false_type {};
template <class T> struct ScalarWrapper<Quantity<T>> : std::true_type {};
template <class T> struct ScalarWrapper<Identifier<T>> : std::true_type {};
template <class T> struct Enum;
#define ENUM(T, MAX)                                                                                         \
    template <> struct Enum<T> {                                                                             \
        static constexpr int maximum = MAX;                                                                  \
    };
ENUM(RadiationKind, 2)
ENUM(IntensityScale, 2)
ENUM(SourcePrecision, 2) ENUM(NumericalPolicy, 2) ENUM(BackgroundMethod, 6) ENUM(SmoothingMethod, 1)
    ENUM(FwhmMethod, 1) ENUM(FwhmOrigin, 2) ENUM(FwhmNoiseState, 2) ENUM(FwhmNoiseModel, 4)
        ENUM(FwhmRejection, 14) ENUM(GeometricFwhmResult::State, 1) ENUM(FwhmEstimate::State, 1)
            ENUM(PeakCalibration::Source, 4) ENUM(PeakDetectionStage, 1) ENUM(PfsmSettings::Weighting, 1)
                ENUM(WorkflowId, 3) ENUM(WorkflowStep, 9) ENUM(WorkflowStepStatus, 6) ENUM(SearchMethod, 1)
                    ENUM(AnalysisPurpose, 1) ENUM(CandidateQuery::Route, 2)
                        ENUM(CandidateQuery::ElementOperator, 2) ENUM(ProfileFitTermination, 7)
                            ENUM(PfsmObservationRequest::Signal, 1) ENUM(PfsmCandidateRecallMode, 2)
                                ENUM(PfsmCandidateRecallVersion, 0) ENUM(PfsmRirRevision::Radiation, 0)
#undef ENUM
                                    struct Context {
    std::vector<AlgorithmHandle> algorithms;
};
template <class T> QJsonValue encode(const T &);
template <class T> T decode(const QJsonValue &, Context &, const std::string &);
struct Writer {
    QJsonObject object;
    template <class T> void operator()(const char *key, const T &value) { object[key] = encode(value); }
};
struct Reader {
    QJsonObject object;
    Context &context;
    std::string path;
    template <class T> void operator()(const char *key, T &value) {
        if (!object.contains(key))
            invalid(path + "." + key, "Missing required QXP v3 field");
        value = decode<T>(object[key], context, path + "." + key);
    }
};
template <class V, std::size_t I = 0>
V decodeVariant(int index, const QJsonValue &v, Context &c, const std::string &p) {
    if constexpr (I < std::variant_size_v<V>) {
        if (index == int(I))
            return V{std::in_place_index<I>, decode<std::variant_alternative_t<I, V>>(v, c, p)};
        return decodeVariant<V, I + 1>(index, v, c, p);
    } else
        invalid(p, "Invalid noise variant");
}
template <class T> QJsonValue encode(const T &value) {
    if constexpr (std::is_same_v<T, MetadataValue>)
        return jsonValue(value);
    else if constexpr (std::is_same_v<T, ProjectSettings>)
        return encodeWorkflowSettings(value);
    else if constexpr (std::is_same_v<T, AlgorithmHandle>) {
        if (!value)
            return QJsonValue::Null;
        // A saved identity must be resolvable without an in-process override.
        take(resolveAlgorithms(value->spec));
        Writer w;
        w("selections", value->spec.selections());
        w("method", value->spec.method());
        w("defaults", value->spec.defaults());
        return w.object;
    } else if constexpr (std::is_same_v<T, std::shared_ptr<const AnalysisSpectrum>>) {
        if (!value)
            return QJsonValue::Null;
        Writer w;
        w("pattern", value->pattern());
        w("purpose", value->purpose());
        return w.object;
    } else if constexpr (std::is_same_v<T, std::monostate>)
        return QJsonObject{};
    else if constexpr (std::is_same_v<T, std::string>)
        return QString::fromStdString(value);
    else if constexpr (std::is_same_v<T, bool>)
        return value;
    else if constexpr (std::is_integral_v<T>)
        return QString::number(value);
    else if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value))
            invalid("project.number", "Non-finite project number");
        if (value == 0 && std::signbit(value))
            return QStringLiteral("-0");
        return double(value);
    } else if constexpr (std::is_enum_v<T>)
        return int(value);
    else if constexpr (Optional<T>::value || Shared<T>::value)
        return value ? encode(*value) : QJsonValue(QJsonValue::Null);
    else if constexpr (Vector<T>::value || Array<T>::value) {
        QJsonArray a;
        for (const auto &v : value)
            a.append(encode(typename T::value_type(v)));
        return a;
    } else if constexpr (Map<T>::value) {
        QJsonObject o;
        for (const auto &[k, v] : value)
            o[QString::fromStdString(k)] = encode(v);
        return o;
    } else if constexpr (Variant<T>::value) {
        return QJsonObject{{"kind", int(value.index())},
                           {"value", std::visit([](const auto &v) { return encode(v); }, value)}};
    } else if constexpr (ScalarWrapper<T>::value)
        return encode(value.value);
    else {
        Writer w;
        Schema<T>::visit(w, value);
        return w.object;
    }
}
template <class T> T decode(const QJsonValue &value, Context &context, const std::string &path) {
    if constexpr (std::is_same_v<T, MetadataValue>)
        return metadataValue(value);
    else if constexpr (std::is_same_v<T, ProjectSettings>) {
        if (!value.isObject())
            invalid(path);
        return take(decodeWorkflowSettings(value.toObject()));
    } else if constexpr (std::is_same_v<T, AlgorithmHandle>) {
        if (value.isNull())
            return {};
        if (!value.isObject())
            invalid(path);
        Reader r{value.toObject(), context, path};
        AlgorithmSelections selections;
        SearchMethod method;
        AlgorithmDefaults defaults;
        r("selections", selections);
        r("method", method);
        r("defaults", defaults);
        AlgorithmSpec spec{selections, method, defaults};
        for (const auto &handle : context.algorithms)
            if (handle->spec == spec)
                return handle;
        auto handle = take(resolveAlgorithms(spec));
        context.algorithms.push_back(handle);
        return handle;
    } else if constexpr (std::is_same_v<T, std::shared_ptr<const AnalysisSpectrum>>) {
        if (value.isNull())
            return {};
        if (!value.isObject())
            invalid(path);
        Reader r{value.toObject(), context, path};
        Pattern pattern;
        AnalysisPurpose purpose;
        r("pattern", pattern);
        r("purpose", purpose);
        return std::make_shared<const AnalysisSpectrum>(
            take(AnalysisSpectrum::create(std::move(pattern), purpose)));
    } else if constexpr (std::is_same_v<T, std::monostate>) {
        if (!value.isObject())
            invalid(path);
        return {};
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!value.isString())
            invalid(path);
        return value.toString().toStdString();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!value.isBool())
            invalid(path);
        return value.toBool();
    } else if constexpr (std::is_integral_v<T>) {
        if (!value.isString())
            invalid(path);
        bool ok = false;
        if constexpr (std::is_signed_v<T>) {
            auto n = value.toString().toLongLong(&ok);
            if (!ok || n < std::numeric_limits<T>::min() || n > std::numeric_limits<T>::max())
                invalid(path);
            return T(n);
        } else {
            if (value.toString().startsWith('-'))
                invalid(path);
            auto n = value.toString().toULongLong(&ok);
            if (!ok || n > std::numeric_limits<T>::max())
                invalid(path);
            return T(n);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        if (value == QJsonValue(QStringLiteral("-0")))
            return T(-0.0);
        if (!value.isDouble() || !std::isfinite(value.toDouble()) || !std::isfinite(T(value.toDouble())))
            invalid(path);
        return T(value.toDouble());
    } else if constexpr (std::is_enum_v<T>) {
        if (!value.isDouble() || value.toDouble() != value.toInt(-1) || value.toInt(-1) < 0 ||
            value.toInt() > Enum<T>::maximum)
            invalid(path);
        return T(value.toInt());
    } else if constexpr (Optional<T>::value) {
        if (value.isNull())
            return {};
        return decode<typename Optional<T>::Value>(value, context, path);
    } else if constexpr (Shared<T>::value) {
        if (value.isNull())
            return {};
        return std::make_shared<const typename Shared<T>::Value>(
            decode<typename Shared<T>::Value>(value, context, path));
    } else if constexpr (Vector<T>::value || Array<T>::value) {
        if (!value.isArray())
            invalid(path);
        const auto a = value.toArray();
        T out;
        if constexpr (Vector<T>::value)
            out.resize(a.size());
        else if (a.size() != int(out.size()))
            invalid(path);
        for (qsizetype i = 0; i < a.size(); ++i)
            out[std::size_t(i)] =
                decode<typename T::value_type>(a[i], context, path + "[" + std::to_string(i) + "]");
        return out;
    } else if constexpr (Map<T>::value) {
        if (!value.isObject())
            invalid(path);
        T out;
        const auto o = value.toObject();
        for (auto i = o.begin(); i != o.end(); ++i)
            out.emplace(i.key().toStdString(), decode<typename Map<T>::Value>(
                                                   i.value(), context, path + "." + i.key().toStdString()));
        return out;
    } else if constexpr (Variant<T>::value) {
        if (!value.isObject())
            invalid(path);
        const auto o = value.toObject();
        const auto kind = o["kind"];
        if (!kind.isDouble() || kind.toDouble() != kind.toInt(-1))
            invalid(path);
        return decodeVariant<T>(kind.toInt(-1), o["value"], context, path);
    } else if constexpr (ScalarWrapper<T>::value) {
        T out;
        out.value = decode<decltype(out.value)>(value, context, path);
        return out;
    } else {
        if (!value.isObject())
            invalid(path);
        T out;
        Reader r{value.toObject(), context, path};
        Schema<T>::visit(r, out);
        return out;
    }
}
} // namespace qualpf::infrastructure::legacy_fixture::qxp3
