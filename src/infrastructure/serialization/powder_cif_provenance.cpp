#include "powder_cif_provenance.h"
#include "binary32.h"
#include "sha256.h"
#include <QByteArray>
#include <QJsonArray>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qualpf::infrastructure {
using namespace application;
namespace {
using namespace io::powder_cif;
QString text(const std::string &s) { return QString::fromStdString(s); }
void require(bool ok) { if (!ok) throw std::invalid_argument("Invalid powder CIF provenance"); }
std::size_t count(QJsonValue v, std::size_t maximum) {
    require(v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= 0 &&
            v.toDouble() == std::floor(v.toDouble()) && v.toDouble() <= double(maximum));
    return std::size_t(v.toDouble());
}
SourceSpan span(const QJsonObject &o, std::size_t n) {
    SourceSpan out{count(o["begin"],n),count(o["end"],n)};
    require(out.begin<=out.end); return out;
}
QJsonObject field(const LocatedField &f) {
    QJsonObject out{{"tag",text(f.tag)},{"offset",double(f.location.offset)},
        {"line",double(f.location.line)},{"column",double(f.location.column)},
        {"begin",double(f.span.begin)},{"end",double(f.span.end)}};
    if (f.row) out["row"]=double(*f.row);
    return out;
}
LocatedField field(const QJsonValue &v, std::size_t n) {
    require(v.isObject()); const auto o=v.toObject();
    require(o["tag"].isString());
    LocatedField f; f.tag=o["tag"].toString().toStdString();
    f.location={count(o["offset"],n),count(o["line"],n+1),count(o["column"],n+1)};
    f.span=span(o,n);
    require(f.location.offset==f.span.begin && f.location.line>0 && f.location.column>0);
    if (o.contains("row")) f.row=count(o["row"],n);
    return f;
}
std::uint32_t bits(QJsonValue v) {
    require(v.isString() && v.toString().size()==8);
    bool ok=false; const auto out=v.toString().toUInt(&ok,16);
    require(ok && QString::number(out,16).rightJustified(8,'0')==v.toString());
    return out;
}
}
QJsonObject encodePowderCifProvenance(const PowderCifProvenance &p) {
    QJsonArray axis,notices;
    for (const auto &f : p.axis_fields) axis.append(field(f));
    for (const auto &n : p.notices)
        notices.append(QJsonObject{{"reason",text(n.reason)},{"tag",text(n.tag)},
            {"block_index",double(n.block_index)},{"begin",double(n.span.begin)},{"end",double(n.span.end)}});
    QJsonObject out{{"version",1},{"source_id",text(p.source->source_id)},
        {"source_sha256",text(p.source->sha256)},
        {"source_bytes_base64",QString::fromLatin1(QByteArray(p.source->bytes.data(),qsizetype(p.source->bytes.size())).toBase64())},
        {"fragment",p.fragment},{"selected_block_id",text(p.block_id)},
        {"selected_block_index",double(p.block_index)},
        {"block_span",QJsonObject{{"begin",double(p.block_span.begin)},{"end",double(p.block_span.end)}}},
        {"axis_source",text(p.axis_source)},{"intensity_source",text(p.intensity_source)},
        {"axis_fields",axis},{"intensity_field",field(p.intensity_field)},
        {"reversed",p.reversed},{"notices",notices}};
    if (p.radiation.origin)
        out["radiation_origin"]=*p.radiation.origin==io::powder_cif::RadiationOrigin::source_file ? "source_file" : "user_supplied";
    if (p.radiation.wavelength_bits)
        out["wavelength_bits"]=QString::number(*p.radiation.wavelength_bits,16).rightJustified(8,'0');
    if (p.radiation.kind) out["radiation_kind"]=int(*p.radiation.kind);
    if (p.radiation.source_field) out["wavelength_field"]=field(*p.radiation.source_field);
    return out;
}
Result<std::shared_ptr<const PowderCifProvenance>> decodePowderCifProvenance(const QJsonValue &value,const std::string &sourceId) {
    using Decoded = Result<std::shared_ptr<const PowderCifProvenance>>;
    if (value.isNull() || value.isUndefined()) return Decoded{std::shared_ptr<const PowderCifProvenance>{}};
    try {
        using namespace io::powder_cif;
        require(value.isObject()); const auto o=value.toObject();
        require(o["version"].isDouble() && o["version"].toDouble()==1 &&
            o["source_id"].isString() && o["source_id"].toString().toStdString()==sourceId &&
            o["source_bytes_base64"].isString() && o["source_sha256"].isString());
        const auto encoded=o["source_bytes_base64"].toString().toLatin1();
        const auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
        require(bool(decoded) && decoded.decoded.toBase64()==encoded);
        const std::string bytes(decoded.decoded.constData(),std::size_t(decoded.decoded.size()));
        require(source_sha256(bytes)==o["source_sha256"].toString().toStdString());
        auto p=std::make_shared<PowderCifProvenance>();
        p->source=std::make_shared<const SourceArtifact>(SourceArtifact{bytes,o["source_sha256"].toString().toStdString(),sourceId});
        const auto n=bytes.size();
        require(o["fragment"].isBool() && o["reversed"].isBool() && o["selected_block_id"].isString() &&
            o["block_span"].isObject() && o["axis_source"].isString() && o["intensity_source"].isString());
        p->fragment=o["fragment"].toBool(); p->reversed=o["reversed"].toBool();
        p->block_id=o["selected_block_id"].toString().toStdString(); require(!p->block_id.empty());
        p->block_index=count(o["selected_block_index"],n); p->block_span=span(o["block_span"].toObject(),n);
        p->axis_source=o["axis_source"].toString().toStdString(); p->intensity_source=o["intensity_source"].toString().toStdString();
        require(!p->axis_source.empty() && !p->intensity_source.empty() && o["axis_fields"].isArray());
        for (const auto f : o["axis_fields"].toArray()) p->axis_fields.push_back(field(f,n));
        require(p->axis_fields.size()==(p->axis_source=="generated" ? 2u : 1u));
        p->intensity_field=field(o["intensity_field"],n); require(p->intensity_field.tag==p->intensity_source);
        const auto origin=o["radiation_origin"].toString();
        require(origin=="source_file" || origin=="user_supplied");
        p->radiation.origin=origin=="source_file" ? RadiationOrigin::source_file : RadiationOrigin::user_supplied;
        p->radiation.wavelength_bits=bits(o["wavelength_bits"]);
        require(binary32::finite(*p->radiation.wavelength_bits) && binary32::less(0,*p->radiation.wavelength_bits));
        if (o.contains("radiation_kind")) {
            const auto k=count(o["radiation_kind"],1); p->radiation.kind=RadiationKind(k);
        }
        if (o.contains("wavelength_field")) p->radiation.source_field=field(o["wavelength_field"],n);
        require((origin=="source_file")==p->radiation.source_field.has_value());
        require(o["notices"].isArray());
        for (const auto v : o["notices"].toArray()) {
            require(v.isObject()); const auto notice=v.toObject();
            require(notice["reason"].isString() && notice["tag"].isString());
            p->notices.push_back({notice["reason"].toString().toStdString(),notice["tag"].toString().toStdString(),
                span(notice,n),count(notice["block_index"],n)});
        }
        require(encodePowderCifProvenance(*p)==o);
        return Decoded{std::shared_ptr<const PowderCifProvenance>(std::move(p))};
    } catch (const std::invalid_argument &e) {
        return Decoded{Error{ErrorCode::invalid_input,"project.powder_cif_provenance",e.what(),"invalid_provenance"}};
    }
}
} // namespace qualpf::infrastructure
