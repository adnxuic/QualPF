#pragma once
#include "qualpf/application/project_document.h"
#include <QByteArray>
#include <map>
#include <set>
#include <variant>

namespace qualpf::infrastructure::qpf {
struct Failure {
    Error error;
};
[[noreturn]] inline void invalid(const std::string &path, const std::string &message = "Invalid QPF value",
                                 const std::string &reason = "invalid_project") {
    throw Failure{{ErrorCode::invalid_input, path, message, reason}};
}
template <class T> T take(Result<T> r) {
    if (!r)
        throw Failure{r.error()};
    return std::move(r).value();
}
struct Undefined {};
struct Value {
    using Array = std::vector<Value>;
    using Map = std::map<std::string, Value, std::less<>>;
    using Storage = std::variant<Undefined, std::nullptr_t, bool, std::int64_t, std::uint64_t, double,
                                 std::string, QByteArray, Array, Map>;
    Storage data = Undefined{};
    Value() = default;
    Value(const char *s) : data(std::string(s)) {}
    template <class T> Value(T v) : data(std::move(v)) {}
    template <class T> const T &as(const std::string &path = "project") const {
        const auto *p = std::get_if<T>(&data);
        if (!p)
            invalid(path, "Incorrect CBOR type");
        return *p;
    }
    const Value &at(const std::string &key) const {
        const auto &m = as<Map>();
        auto i = m.find(key);
        if (i == m.end())
            invalid("project." + key, "Missing required QPF field");
        return i->second;
    }
};
struct Budget {
    application::ProjectReadOptions options;
    std::uint64_t used = 0;
    void charge(std::uint64_t n) {
        if (n > options.maximum_decoded_bytes - used)
            invalid("project.size", "Project exceeds the decoding budget", "project_resource_limit");
        used += n;
    }
};
QByteArray cbor(const Value &);
Value parse(const QByteArray &, Budget &);
std::string digest(const QByteArray &);
inline std::vector<std::uint8_t> bytes(const QByteArray &b) {
    return {reinterpret_cast<const std::uint8_t *>(b.constData()),
            reinterpret_cast<const std::uint8_t *>(b.constData()) + b.size()};
}
inline QByteArray bytes(const std::vector<std::uint8_t> &b) {
    return QByteArray(reinterpret_cast<const char *>(b.data()), qsizetype(b.size()));
}
std::uint64_t unsignedValue(const Value &, const std::string &);
Value metadata(const application::MetadataValue &);
application::MetadataValue metadata(const Value &);
// Table records are an owned private document, never a live database/session.
struct Object {
    std::string kind;
    std::uint64_t version = 1;
    Value fields;
};
struct Archive {
    Value manifest;
    std::map<std::string, Object, std::less<>> objects;
    std::map<std::string, QByteArray, std::less<>> blocks;
    Budget budget;
    std::set<std::string> visiting, decoded;
    std::vector<application::AlgorithmHandle> algorithms;
    std::map<const void *, std::string> shared;
};
std::string objectDigest(const std::string &id, const Object &);
std::string arrayUnit(const std::string &);
} // namespace qualpf::infrastructure::qpf
