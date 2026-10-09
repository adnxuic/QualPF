#include "gui/adapters/card_projection.h"

#include <QByteArray>
#include <utility>

namespace qualpf::gui {

Result<ReferenceCard> copy_reference_card(const CardType& source, std::string database_id,
                                         std::span<const ReflectionId> stable_ids,
                                         std::optional<WavelengthAngstrom> coordinate_wavelength)
{
    const auto d = source.getD();
    const auto tth = source.getTth();
    const auto intensity = source.getIntensity();
    if (d.size() != intensity.size() || (!tth.isEmpty() && tth.size() != intensity.size()) ||
        stable_ids.size() != static_cast<std::size_t>(intensity.size()))
        return Result<ReferenceCard>{Error{ErrorCode::invalid_input, "card.reflections", "Card column/ID lengths disagree"}};
    ReferenceCard output;
    output.database_id = std::move(database_id);
    output.card_id = source.getId().toUtf8().toStdString();
    output.chemical_name = source.getChemicalName().toUtf8().toStdString();
    output.chemical_formula = source.getChemicalFormula().toUtf8().toStdString();
    output.mineral_name = source.getMineralName().toUtf8().toStdString();
    output.space_group = source.getSpaceGroup().toUtf8().toStdString();
    output.quality = source.getQuality().toUtf8().toStdString();
    output.effective_wavelength = coordinate_wavelength;
    output.source_precision = SourcePrecision::float64;
    const auto rir_text = source.getRIR().trimmed();
    if (!rir_text.isEmpty()) {
        bool valid = false;
        const auto rir = rir_text.toDouble(&valid);
        if (!valid) return Result<ReferenceCard>{Error{ErrorCode::invalid_input, "card.rir", "RIR text is not a number"}};
        output.rir = rir;
    }
    for (qsizetype i = 0; i < intensity.size(); ++i) {
        Reflection reflection;
        reflection.id = stable_ids[static_cast<std::size_t>(i)];
        reflection.d_spacing = DSpacingAngstrom{d[i]};
        if (!tth.isEmpty()) reflection.two_theta = TwoThetaDegrees{tth[i]};
        reflection.intensity = intensity[i];
        output.reflections.push_back(std::move(reflection));
    }
    return application::checkedReferenceCard(std::move(output));
}

} // namespace qualpf::gui
