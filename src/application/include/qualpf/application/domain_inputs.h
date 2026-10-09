#pragma once
#include "qualpf/application/product_session.h"
namespace qualpf::application {
Result<ReferenceCard> checkedReferenceCard(ReferenceCard);
Result<PeakSet> checkedPeakSet(PeakSet);
double defaultWavelength();
double alpha2Wavelength();
} // namespace qualpf::application
