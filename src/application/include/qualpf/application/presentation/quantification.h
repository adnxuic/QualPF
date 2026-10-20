#pragma once
#include "qualpf/application/product_session.h"
#include "qualpf/search/rir.h"
#include <span>
namespace qualpf::application {
using QuantificationInput = search::RirInput;
enum class QuantificationMethod { classic_rir, pfsm_rir };
enum class QuantificationAvailability { empty, available, unavailable };
struct QuantificationPhase {
    std::string id, name, original_rir, rir_source;
    std::optional<double> weight_percent, rir, shift_degrees;
    double scale = 0, reference_maximum = 0;
    bool rir_revised = false;
};
struct QuantificationIssue {
    std::string phase_id, reason, message;
};
struct QuantificationResult {
    ProductHandle source;
    QuantificationMethod method = QuantificationMethod::classic_rir;
    QuantificationAvailability availability = QuantificationAvailability::empty;
    std::vector<QuantificationPhase> phases;
    std::vector<QuantificationIssue> issues;
    std::vector<std::string> qualifications;
    std::optional<profile::ProfileFitDiagnostics> diagnostics;
    std::optional<profile::ProfileFitTermination> termination;
    std::optional<double> rwp_net;
    std::uint64_t revision = 0, generation = 0, database_version = 0;
    bool available() const { return availability == QuantificationAvailability::available; }
    std::vector<double> percentages() const;
};
// Classical callers supply their unchanged boundary-parsed RIR inputs. PFSM
// ignores those inputs and consumes only frozen references and joint evidence.
QuantificationResult prepareQuantification(ProductHandle, std::span<const QuantificationInput> = {});
std::vector<double> quantify(std::span<const QuantificationInput>);
std::vector<double> quantifyProduct(const ProductState &, std::span<const QuantificationInput>);
} // namespace qualpf::application
