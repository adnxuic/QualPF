#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <utility>
#include <vector>
#include <limits>
#include <string_view>

namespace qualpf::application {

// Undefined is distinct from explicit JSON null. Integer payloads must not make
// a lossy round trip through binary64 when a project carries unknown metadata.
struct UndefinedValue { friend bool operator==(UndefinedValue, UndefinedValue) = default; };
struct MetadataValue;
using MetadataObject = std::map<std::string, MetadataValue, std::less<>>;
using MetadataArray = std::vector<MetadataValue>;
struct MetadataValue {
    using Storage = std::variant<UndefinedValue, std::nullptr_t, bool, std::int64_t,
                                 double, std::string, MetadataArray, MetadataObject>;
    Storage data = UndefinedValue{};
    MetadataValue() = default;
    MetadataValue(const char *value) : data(std::string(value)) {}
    MetadataValue(int value) : data(std::int64_t(value)) {}
    template<class T> MetadataValue(T value) : data(std::move(value)) {}
    MetadataValue(std::initializer_list<MetadataObject::value_type> values) : data(MetadataObject(values)) {}
    MetadataValue &operator[](std::string_view key) { return std::get<MetadataObject>(data)[std::string(key)]; }
    const MetadataValue &operator[](std::string_view key) const {
        if (auto object = std::get_if<MetadataObject>(&data)) {
            auto i = object->find(key); if (i != object->end()) return i->second;
        }
        static const MetadataValue missing; return missing;
    }
    bool toBool(bool fallback = false) const { auto p = std::get_if<bool>(&data); return p ? *p : fallback; }
    double toDouble(double fallback = 0) const {
        if (auto p = std::get_if<double>(&data)) return *p;
        if (auto p = std::get_if<std::int64_t>(&data)) return double(*p);
        return fallback;
    }
    int toInt(int fallback = 0) const {
        if (auto p = std::get_if<std::int64_t>(&data)) {
            if (*p >= std::numeric_limits<int>::min() && *p <= std::numeric_limits<int>::max()) return int(*p);
        }
        if (auto p = std::get_if<double>(&data)) {
            if (*p >= std::numeric_limits<int>::min() && *p <= std::numeric_limits<int>::max() && double(int(*p)) == *p) return int(*p);
        }
        return fallback;
    }
    std::string toString() const { auto p = std::get_if<std::string>(&data); return p ? *p : std::string{}; }
    std::size_t size() const {
        if (auto p=std::get_if<MetadataObject>(&data)) return p->size();
        if (auto p=std::get_if<MetadataArray>(&data)) return p->size();
        return 0;
    }
    bool empty() const { return size()==0; }
    bool contains(std::string_view key) const {
        auto p=std::get_if<MetadataObject>(&data); return p && p->contains(key);
    }
    void remove(std::string_view key) { std::get<MetadataObject>(data).erase(std::string(key)); }
    MetadataArray toArray() const { auto p = std::get_if<MetadataArray>(&data); return p ? *p : MetadataArray{}; }
    MetadataObject toObject() const { auto p = std::get_if<MetadataObject>(&data); return p ? *p : MetadataObject{}; }
    friend bool operator==(const MetadataValue &, const MetadataValue &) = default;
};

struct ProductCard {
    std::string id, chemicalName, chemicalFormula, mineralName, quality, rir, spaceGroup;
    double fom = 0, fomPeakPos = 0, fomIntensity = 0, scale = 0;
    bool fomCalculated = false;
    std::vector<double> d, tth, intensity;
    friend bool operator==(const ProductCard &, const ProductCard &) = default;
};

struct ProductPeaks {
    std::vector<double> d, deltaD, tth, intensity, intensityOrig, fwhm;
    std::optional<std::vector<double>> integratedIntensity;
    double wave = 0;
    bool valid = false;
    void clear() { *this = {}; }
    friend bool operator==(const ProductPeaks &, const ProductPeaks &) = default;
};

} // namespace qualpf::application
