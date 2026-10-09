#include "qualpf/application/domain_inputs.h"
#include "qualpf/domain/validation.h"
#include "qualpf/domain/radiation.h"
namespace qualpf::application {
Result<ReferenceCard> checkedReferenceCard(ReferenceCard card) {
    const auto issues = validate(card);
    return issues.empty() ? Result<ReferenceCard>{std::move(card)} : Result<ReferenceCard>{issues.front()};
}
Result<PeakSet> checkedPeakSet(PeakSet peaks) {
    const auto issues = validate(peaks);
    return issues.empty() ? Result<PeakSet>{std::move(peaks)} : Result<PeakSet>{issues.front()};
}
double defaultWavelength() { return radiation::default_wavelength_angstrom; }
double alpha2Wavelength() { return radiation::CU_KA2_WAVE; }
} // namespace qualpf::application
