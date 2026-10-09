#include "infrastructure/serialization/powder_cif_provenance.h"
#include "infrastructure/serialization/metadata_values.h"
#include "powder_source_reader.h"
#include "sha256.h"
#include <QFile>
#include <QFileInfo>
namespace qualpf::infrastructure {
application::MetadataObject PowderSourceReader::encodeProvenance(const application::PowderCifProvenance &p) const {
    return std::get<application::MetadataObject>(metadataValue(encodePowderCifProvenance(p)).data);
}
io::powder_cif::ImportResult<application::SourceBytes>
PowderSourceReader::read(const std::string &identity) const {
    using namespace io::powder_cif;
    const auto path = QFileInfo(QString::fromUtf8(identity.data(), static_cast<qsizetype>(identity.size())))
                          .absoluteFilePath();
    QFile file(path);
    auto failed = [&] {
        return ImportError{
            {ErrorCode::invalid_input, "pattern.cif", file.errorString().toStdString(), "read_failure"},
            std::make_shared<const SourceArtifact>(SourceArtifact{{}, source_sha256({}), path.toStdString()}),
            {},
            {},
            {},
            {},
            {}};
    };
    if (!file.open(QIODevice::ReadOnly))
        return failed();
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return failed();
    return application::SourceBytes{std::string(bytes.constData(), std::size_t(bytes.size())),
                                    path.toStdString()};
}
} // namespace qualpf::infrastructure
