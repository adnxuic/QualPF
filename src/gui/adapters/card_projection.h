#pragma once

#include <QString>

#include "gui/model/cardtype.h"
#include "qualpf/application/domain_inputs.h"

#include <span>

namespace qualpf::gui {

// Projects an already decoded card. No SQL, unit conversion, scoring or sorting.
[[nodiscard]] Result<ReferenceCard> copy_reference_card(
    const CardType&, std::string database_id, std::span<const ReflectionId> stable_ids,
    std::optional<WavelengthAngstrom> coordinate_wavelength);

} // namespace qualpf::gui
