#include "qualpf/application/import/powder_cif_provenance.h"
namespace qualpf::application {
PowderCifProvenance powderCifProvenance(const io::powder_cif::PowderCifImportSnapshot &s) {
    PowderCifProvenance p;
    p.source=s.source; p.fragment=s.source_fragment_without_data_block;
    p.block_id=s.selected_block.block_id; p.block_index=s.selected_block.block_index;
    p.block_span=s.selected_block.source_span; p.radiation=s.radiation;
    p.reversed=s.reversed; p.notices=s.notices;
    if (s.selected_block.selected_axis) {
        p.axis_source=s.selected_block.selected_axis->tag;
        p.axis_fields.push_back(s.selected_block.selected_axis->field);
    } else {
        p.axis_source="generated";
        p.axis_fields={*s.selected_block.generated_axis->minimum.field,*s.selected_block.generated_axis->increment.field};
    }
    p.intensity_source=s.selected_block.selected_intensity->tag;
    p.intensity_field=s.selected_block.selected_intensity->field;
    return p;
}
}
