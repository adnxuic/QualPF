#pragma once
#include "powder_cif_importer.h"

namespace qualpf::application {
// Import history associated with the current Pattern. Processing may change its
// samples; it never changes the immutable original source or its interpretation.
struct PowderCifProvenance {
    std::shared_ptr<const io::powder_cif::SourceArtifact> source;
    bool fragment = false;
    std::string block_id;
    std::size_t block_index = 0;
    io::powder_cif::SourceSpan block_span;
    std::string axis_source;
    std::string intensity_source;
    std::vector<io::powder_cif::LocatedField> axis_fields;
    io::powder_cif::LocatedField intensity_field;
    io::powder_cif::RadiationProvenance radiation;
    bool reversed = false;
    std::vector<io::powder_cif::Notice> notices;
};
PowderCifProvenance powderCifProvenance(const io::powder_cif::PowderCifImportSnapshot &);
} // namespace qualpf::application
