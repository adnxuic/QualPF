#pragma once

#include "qualpf/application/database/cif_record.h"
#include "qualpf/application/database/cif_card_service.h"

namespace qualpf::application {

using CifRecordResult = std::variant<database::CifRecord, database::WriteError>;

// Adapts one complete calculation, with an explicit database identity supplied
// by the file-discovery owner. Does not parse filenames or modify card physics.
// Zero is a valid legacy identity; duplicates are rejected by the atomic writer.
[[nodiscard]] CifRecordResult cif_card_record(const CifCard &calculated, std::int32_t identity);

} // namespace qualpf::application
