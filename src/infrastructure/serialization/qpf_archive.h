#pragma once
#include "qpf_tokens.h"
#include "qpf_value.h"
#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>

namespace qualpf::infrastructure::qpf {
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {
    using Type = T;
};
template <class T> struct Shared : std::false_type {};
template <class T> struct Shared<std::shared_ptr<T>> : std::true_type {
    using Type = std::remove_const_t<T>;
};
template <class T> struct Vector : std::false_type {};
template <class T> struct Vector<std::vector<T>> : std::true_type {
    using Type = T;
};
template <class T> struct Array : std::false_type {};
template <class T, std::size_t N> struct Array<std::array<T, N>> : std::true_type {
    using Type = T;
};
template <class T> struct Map : std::false_type {};
template <class T, class C> struct Map<std::map<std::string, T, C>> : std::true_type {};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
// Protocol identities follow alternative types rather than std::variant order.
template <class T> struct NoiseKind;
template <> struct NoiseKind<std::monostate> {
    static constexpr const char *value = "unknown";
};
template <> struct NoiseKind<FwhmStandardDeviations> {
    static constexpr const char *value = "standard_deviations";
};
template <> struct NoiseKind<FwhmPoissonCounts> {
    static constexpr const char *value = "poisson_counts";
};
template <> struct NoiseKind<FwhmPoissonRates> {
    static constexpr const char *value = "poisson_rates";
};
template <> struct NoiseKind<FwhmCovariance> {
    static constexpr const char *value = "covariance";
};

template <class T> struct Scalar {
    using Type = T;
    static constexpr bool wrapped = false;
};
template <class T> struct Scalar<Quantity<T>> {
    using Type = double;
    static constexpr bool wrapped = true;
};
template <class T> struct Scalar<Identifier<T>> {
    using Type = std::uint64_t;
    static constexpr bool wrapped = true;
};
template <class T> using Numeric = typename Scalar<T>::Type;
template <class T> Value encode(const T &, Archive &, const std::string &);
template <class T> T decode(const Value &, Archive &, const std::string &);

inline Value reference(const std::string &id) { return Value::Map{{"$ref", id}}; }
inline std::string objectId(const std::string &kind, const std::string &path) {
    return digest(cbor(Value::Array{kind, path}));
}
struct Writer {
    Archive &archive;
    std::string path;
    Value::Map fields;
    template <class T> void operator()(const char *key, const T &v) {
        fields.insert_or_assign(key, encode(v, archive, path + "." + key));
    }
};
struct Reader {
    Archive &archive;
    std::string path;
    const Value::Map &fields;
    template <class T> void operator()(const char *key, T &v) {
        auto found = fields.find(key);
        if (found == fields.end())
            invalid(path + "." + key, "Missing required QPF field");
        v = decode<T>(found->second, archive, path + "." + key);
    }
};
inline Value putObject(const std::string &kind, Writer w) {
    const auto id = objectId(kind, w.path);
    // Existing fields contain optional extensions; known fields always win.
    if (auto old = w.archive.objects.find(id); old != w.archive.objects.end()) {
        auto fields = old->second.fields.as<Value::Map>();
        for (auto &[key, v] : w.fields)
            fields.insert_or_assign(key, std::move(v));
        w.fields = std::move(fields);
    }
    w.archive.objects.insert_or_assign(id, Object{kind, 1, Value(std::move(w.fields))});
    w.archive.decoded.insert(id);
    return reference(id);
}
inline std::pair<std::string, const Object *> getObject(const Value &v, Archive &a, const std::string &kind) {
    const auto &ref = v.as<Value::Map>();
    if (ref.size() != 1 || !ref.contains("$ref"))
        invalid("project.reference");
    const auto id = v.at("$ref").as<std::string>();
    const auto found = a.objects.find(id);
    if (found == a.objects.end() || found->second.kind != kind || found->second.version != 1)
        invalid("project.reference", "Missing object or unsupported object type/version");
    if (!a.visiting.insert(id).second)
        invalid("project.reference", "Cyclic project reference");
    if (a.visiting.size() > a.budget.options.maximum_depth)
        invalid("project.reference", "Reference depth limit exceeded", "project_resource_limit");
    a.decoded.insert(id);
    return {id, &found->second};
}
template <class T> std::string elementKey(const T &v, std::size_t index) {
    if constexpr (requires { v.id; }) {
        if constexpr (std::is_same_v<std::decay_t<decltype(v.id)>, std::string>)
            return "id:" + v.id;
        else if constexpr (std::is_integral_v<std::decay_t<decltype(v.id)>>)
            return "id:" + std::to_string(v.id);
    }
    return std::to_string(index);
}
template <class T> std::string dtype() {
    using N = Numeric<T>;
    if constexpr (std::is_floating_point_v<N>)
        return sizeof(N) == 4 ? "f32" : "f64";
    else if constexpr (std::is_same_v<N, bool>)
        return "u8";
    else
        return std::string(std::is_signed_v<N> ? "i" : "u") + std::to_string(8 * sizeof(N));
}
template <class T> auto number(const T &v) {
    if constexpr (Scalar<T>::wrapped)
        return v.value;
    else
        return v;
}
template <class T>
using Bits =
    std::conditional_t<sizeof(T) == 8, std::uint64_t,
                       std::conditional_t<sizeof(T) == 4, std::uint32_t,
                                          std::conditional_t<sizeof(T) == 2, std::uint16_t, std::uint8_t>>>;
template <class T> void appendNumber(QByteArray &b, const T &v) {
    using N = Numeric<T>;
    N n = number(v);
    if constexpr (std::is_floating_point_v<N>)
        if (!std::isfinite(n))
            invalid("project.array", "Non-finite scientific value");
    const auto bits = std::bit_cast<Bits<N>>(n);
    for (std::size_t i = 0; i < sizeof(N); ++i)
        b.append(char((bits >> (i * 8)) & 255));
}
template <class T> T readNumber(const char *p) {
    using N = Numeric<T>;
    Bits<N> bits = 0;
    for (std::size_t i = 0; i < sizeof(N); ++i)
        bits |= Bits<N>(static_cast<unsigned char>(p[i])) << (i * 8);
    if constexpr (std::is_same_v<N, bool>)
        if (bits > 1)
            invalid("project.array", "Invalid boolean byte");
    const N n = std::bit_cast<N>(bits);
    if constexpr (std::is_floating_point_v<N>)
        if (!std::isfinite(n))
            invalid("project.array", "Non-finite scientific value");
    if constexpr (Scalar<T>::wrapped)
        return T{n};
    else
        return n;
}
template <class T> Value encodeNumbers(const T &v, Archive &a, const std::string &path) {
    constexpr qsizetype blockSize = 4 * 1024 * 1024;
    Value::Array chunks;
    QByteArray block;
    block.reserve(blockSize);
    auto flush = [&] {
        if (!block.isEmpty()) {
            auto hash = digest(block);
            a.blocks.try_emplace(hash, block);
            chunks.emplace_back(hash);
            block.clear();
        }
    };
    for (const auto &n : v) {
        if (block.size() == blockSize)
            flush();
        appendNumber(block, typename T::value_type(n));
    }
    flush();
    return Value::Map{{"$array", Value::Map{{"dtype", dtype<typename T::value_type>()},
                                            {"shape", Value::Array{std::uint64_t(v.size())}},
                                            {"byte_order", "little"},
                                            {"unit", arrayUnit(path)},
                                            {"blocks", std::move(chunks)}}}};
}
template <class T> T decodeNumbers(const Value &v, Archive &a, const std::string &path) {
    using E = typename T::value_type;
    constexpr auto width = sizeof(Numeric<E>);
    const auto &desc = v.at("$array");
    const auto &shape = desc.at("shape").as<Value::Array>();
    if (desc.at("dtype").as<std::string>() != dtype<E>() ||
        desc.at("byte_order").as<std::string>() != "little" ||
        desc.at("unit").as<std::string>() != arrayUnit(path) || shape.size() != 1)
        invalid(path, "Incorrect array descriptor");
    const auto count = unsignedValue(shape.front(), path);
    if (count > SIZE_MAX / std::max(sizeof(E), width))
        invalid(path, "Array length overflow");
    a.budget.charge(count * sizeof(E));
    T out;
    if constexpr (Vector<T>::value)
        out.resize(std::size_t(count));
    else if (out.size() != count)
        invalid(path, "Incorrect fixed array length");
    std::size_t done = 0;
    for (const auto &ref : desc.at("blocks").as<Value::Array>()) {
        const auto found = a.blocks.find(ref.as<std::string>());
        if (found == a.blocks.end() || found->second.isEmpty() || found->second.size() > 4 * 1024 * 1024 ||
            found->second.size() % width)
            invalid(path, "Invalid array block");
        const auto &b = found->second;
        if (std::uint64_t(b.size() / width) > count - done)
            invalid(path, "Array blocks exceed declared length");
        for (qsizetype i = 0; i < b.size(); i += width)
            out[done++] = readNumber<E>(b.constData() + i);
    }
    if (done != count)
        invalid(path, "Incomplete array data");
    return out;
}
template <class T> Value encode(const T &v, Archive &a, const std::string &p) {
    if constexpr (std::is_same_v<T, MetadataValue>)
        return Value::Map{{"$metadata", metadata(v)}};
    else if constexpr (std::is_same_v<T, MetadataObject> || std::is_same_v<T, MetadataArray>)
        return Value::Map{{"$metadata", metadata(MetadataValue(v))}};
    else if constexpr (std::is_same_v<T, AlgorithmHandle>) {
        if (!v)
            return nullptr;
        take(resolveAlgorithms(v->spec));
        Writer w{a, p, {}};
        w("selections", v->spec.selections());
        w("method", v->spec.method());
        w("defaults", v->spec.defaults());
        return putObject("algorithm_set", std::move(w));
    } else if constexpr (std::is_same_v<T, io::powder_cif::SourceArtifact>) {
        Writer w{a, p, {}};
        w("source_id", v.source_id);
        w("sha256", v.sha256);
        w("source_bytes", std::vector<std::uint8_t>(v.bytes.begin(), v.bytes.end()));
        return putObject("source_artifact", std::move(w));
    } else if constexpr (std::is_same_v<T, std::shared_ptr<const AnalysisSpectrum>>) {
        if (!v)
            return nullptr;
        Writer w{a, p, {}};
        w("pattern", v->pattern());
        w("purpose", v->purpose());
        return putObject("analysis_spectrum", std::move(w));
    } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>)
        return v;
    else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>)
            return std::int64_t(v);
        else
            return std::uint64_t(v);
    } else if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(v))
            invalid(p, "Non-finite scientific value");
        return double(v);
    } else if constexpr (std::is_enum_v<T>) {
        for (const auto &[value, token] : Tokens<T>::values)
            if (v == value)
                return token;
        invalid(p, "Unknown enumeration value");
    } else if constexpr (Optional<T>::value)
        return v ? encode(*v, a, p) : Value(nullptr);
    else if constexpr (Shared<T>::value) {
        if (!v)
            return nullptr;
        if constexpr (requires { Schema<typename Shared<T>::Type>::kind; }) {
            if (const auto found = a.shared.find(v.get()); found != a.shared.end())
                return reference(found->second);
            auto result = encode(*v, a, p);
            a.shared[v.get()] = result.at("$ref").template as<std::string>();
            return result;
        } else
            return encode(*v, a, p);
    } else if constexpr (Vector<T>::value || Array<T>::value) {
        if constexpr (std::is_arithmetic_v<Numeric<typename T::value_type>>)
            return encodeNumbers(v, a, p);
        else {
            Value::Array out;
            std::size_t i = 0;
            for (const auto &e : v)
                out.push_back(encode(e, a, p + "[" + elementKey(e, i++) + "]"));
            return out;
        }
    } else if constexpr (Map<T>::value) {
        Value::Map out;
        for (const auto &[k, e] : v)
            out.emplace(k, encode(e, a, p + "[" + k + "]"));
        return out;
    } else if constexpr (Variant<T>::value) {
        return std::visit(
            [&](const auto &e) -> Value {
                return Value::Map{{"kind", NoiseKind<std::decay_t<decltype(e)>>::value},
                                  {"value", encode(e, a, p + ".value")}};
            },
            v);
    } else if constexpr (std::is_same_v<T, std::monostate>)
        return Value::Map{};
    else if constexpr (Scalar<T>::wrapped)
        return encode(v.value, a, p);
    else {
        Writer w{a, p, {}};
        Schema<T>::visit(w, v);
        return putObject(Schema<T>::kind, std::move(w));
    }
}
template <class T, std::size_t I = 0>
T decodeVariant(const std::string &kind, const Value &v, Archive &a, const std::string &p) {
    if constexpr (I < std::variant_size_v<T>) {
        using Alternative = std::variant_alternative_t<I, T>;
        if (kind == NoiseKind<Alternative>::value)
            return T{std::in_place_index<I>, decode<Alternative>(v, a, p)};
        return decodeVariant<T, I + 1>(kind, v, a, p);
    } else
        invalid(p);
}
template <class T> T decode(const Value &v, Archive &a, const std::string &p) {
    if constexpr (std::is_same_v<T, MetadataValue>)
        return metadata(v.at("$metadata"));
    else if constexpr (std::is_same_v<T, MetadataObject> || std::is_same_v<T, MetadataArray>) {
        auto value = metadata(v.at("$metadata"));
        const auto *out = std::get_if<T>(&value.data);
        if (!out)
            invalid(p, "Incorrect metadata container type");
        return *out;
    } else if constexpr (std::is_same_v<T, AlgorithmHandle>) {
        if (std::holds_alternative<std::nullptr_t>(v.data))
            return {};
        auto [id, obj] = getObject(v, a, "algorithm_set");
        Reader r{a, p, obj->fields.template as<Value::Map>()};
        AlgorithmSelections selections;
        SearchMethod method;
        AlgorithmDefaults defaults;
        r("selections", selections);
        r("method", method);
        r("defaults", defaults);
        a.visiting.erase(id);
        AlgorithmSpec spec{selections, method, defaults};
        for (const auto &handle : a.algorithms)
            if (handle->spec == spec)
                return handle;
        auto handle = take(resolveAlgorithms(spec));
        a.algorithms.push_back(handle);
        return handle;
    } else if constexpr (std::is_same_v<T, io::powder_cif::SourceArtifact>) {
        auto [id, obj] = getObject(v, a, "source_artifact");
        Reader r{a, p, obj->fields.template as<Value::Map>()};
        T out;
        std::vector<std::uint8_t> raw;
        r("source_id", out.source_id);
        r("sha256", out.sha256);
        r("source_bytes", raw);
        out.bytes.assign(raw.begin(), raw.end());
        a.visiting.erase(id);
        return out;
    } else if constexpr (std::is_same_v<T, std::shared_ptr<const AnalysisSpectrum>>) {
        if (std::holds_alternative<std::nullptr_t>(v.data))
            return {};
        auto [id, obj] = getObject(v, a, "analysis_spectrum");
        Reader r{a, p, obj->fields.template as<Value::Map>()};
        Pattern pattern;
        AnalysisPurpose purpose;
        r("pattern", pattern);
        r("purpose", purpose);
        a.visiting.erase(id);
        return std::make_shared<const AnalysisSpectrum>(
            take(AnalysisSpectrum::create(std::move(pattern), purpose)));
    } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>)
        return v.as<T>(p);
    else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            std::int64_t n;
            if (auto value = std::get_if<std::int64_t>(&v.data))
                n = *value;
            else {
                const auto positive = unsignedValue(v, p);
                if (positive > std::uint64_t(std::numeric_limits<T>::max()))
                    invalid(p);
                n = std::int64_t(positive);
            }
            if (n < std::numeric_limits<T>::min() || n > std::numeric_limits<T>::max())
                invalid(p);
            return T(n);
        } else {
            const auto n = unsignedValue(v, p);
            if (n > std::numeric_limits<T>::max())
                invalid(p);
            return T(n);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        const auto n = v.as<double>(p);
        if (!std::isfinite(n) || !std::isfinite(T(n)))
            invalid(p);
        if constexpr (sizeof(T) < sizeof(double))
            if (double(T(n)) != n)
                invalid(p, "Scalar precision mismatch");
        return T(n);
    } else if constexpr (std::is_enum_v<T>) {
        const auto &token = v.as<std::string>(p);
        for (const auto &[e, name] : Tokens<T>::values)
            if (token == name)
                return e;
        invalid(p, "Unsupported scientific enum token");
    } else if constexpr (Optional<T>::value || Shared<T>::value) {
        if (std::holds_alternative<std::nullptr_t>(v.data))
            return {};
        if constexpr (Optional<T>::value)
            return decode<typename Optional<T>::Type>(v, a, p);
        else
            return std::make_shared<const typename Shared<T>::Type>(
                decode<typename Shared<T>::Type>(v, a, p));
    } else if constexpr (Vector<T>::value || Array<T>::value) {
        if constexpr (std::is_arithmetic_v<Numeric<typename T::value_type>>)
            return decodeNumbers<T>(v, a, p);
        else {
            const auto &values = v.as<Value::Array>(p);
            a.budget.charge(values.size() * sizeof(typename T::value_type));
            T out;
            if constexpr (Vector<T>::value)
                out.resize(values.size());
            else if (out.size() != values.size())
                invalid(p);
            for (std::size_t i = 0; i < values.size(); ++i)
                out[i] = decode<typename T::value_type>(values[i], a, p + "[" + std::to_string(i) + "]");
            return out;
        }
    } else if constexpr (Map<T>::value) {
        T out;
        for (const auto &[k, e] : v.as<Value::Map>(p))
            out.emplace(k, decode<typename T::mapped_type>(e, a, p + "[" + k + "]"));
        return out;
    } else if constexpr (Variant<T>::value) {
        return decodeVariant<T>(v.at("kind").as<std::string>(), v.at("value"), a, p + ".value");
    } else if constexpr (std::is_same_v<T, std::monostate>) {
        v.as<Value::Map>(p);
        return {};
    } else if constexpr (Scalar<T>::wrapped)
        return T{decode<Numeric<T>>(v, a, p)};
    else {
        auto [id, obj] = getObject(v, a, Schema<T>::kind);
        Reader r{a, p, obj->fields.template as<Value::Map>()};
        T out;
        Schema<T>::visit(r, out);
        a.visiting.erase(id);
        return out;
    }
}
} // namespace qualpf::infrastructure::qpf
