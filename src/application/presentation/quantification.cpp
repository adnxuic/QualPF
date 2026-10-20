#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/product_session.h"
#include "qualpf/application/pfsm.h"
#include <algorithm>
#include <cmath>
namespace qualpf::application {
std::vector<double> quantify(std::span<const QuantificationInput> phases) {
    return search::quantify_rir(phases);
}
std::vector<double> quantifyProduct(const ProductState &source, std::span<const QuantificationInput> inputs) {
    if (source.pfsm || (source.algorithms && source.algorithms->spec.method() == SearchMethod::pfsm))
        return prepareQuantification(std::make_shared<const ProductState>(source)).percentages();
    if (!source.algorithms || inputs.size() != source.phases.size() ||
        (source.provenance.legacyUnverified && source.algorithms->spec != AlgorithmSpec{})) return {};
    for (const auto &phase : source.phases)
        if (phase.algorithmSource && (!phase.algorithmSource->scale.algorithms ||
            !phase.algorithmSource->score.algorithms ||
            !sameScoreProducers(phase.algorithmSource->scale.algorithms->spec, source.algorithms->spec) ||
            !sameScoreProducers(phase.algorithmSource->score.algorithms->spec, source.algorithms->spec))) return {};
    return quantify(inputs);
}
std::vector<double> QuantificationResult::percentages() const {
    if (!available()) return {};
    std::vector<double> result;
    for (const auto &p : phases) {
        if (!p.weight_percent) return {};
        result.push_back(*p.weight_percent);
    }
    return result;
}
QuantificationResult prepareQuantification(ProductHandle source, std::span<const QuantificationInput> inputs) {
    QuantificationResult out;
    out.source = source;
    if (!source) {
        out.availability = QuantificationAvailability::unavailable;
        out.issues.push_back({{}, "missing_source", "Missing canonical analysis snapshot"});
        return out;
    }
    out.revision = source->revision;
    const bool pfsm = source->pfsm || (source->algorithms && source->algorithms->spec.method() == SearchMethod::pfsm);
    out.method = pfsm ? QuantificationMethod::pfsm_rir : QuantificationMethod::classic_rir;
    for (const auto &card : source->phases) {
        QuantificationPhase row;
        row.id = card.id;
        row.name = card.chemicalName;
        if (!card.mineralName.empty()) row.name += " [" + card.mineralName + "]";
        row.original_rir = card.rir;
        row.rir_source = "Reference database (original RIR provenance unverified)";
        row.scale = card.scale;
        out.phases.push_back(std::move(row));
    }
    out.qualifications = {"Mass fractions normalized over the selected crystalline phases; unidentified and amorphous material are excluded."};
    const auto issue = [&](std::string id, std::string reason, std::string message) {
        out.issues.push_back({std::move(id), std::move(reason), std::move(message)});
        out.availability = QuantificationAvailability::unavailable;
    };
    if (pfsm) {
        if (!source->pfsm) {
            if (!out.phases.empty()) issue({}, "no_pfsm_analysis", "Run PFSM before quantification");
            return out;
        }
        const auto &analysis = *source->pfsm;
        out.generation = analysis.generation;
        out.database_version = analysis.database_version;
        out.rwp_net = analysis.baseline.rwp_net;
        out.diagnostics = analysis.baseline.diagnostics;
        out.termination = analysis.baseline.termination;
        if (out.phases.empty()) return out;
        for (std::size_t i=0; i<out.phases.size() && i<analysis.baseline.parameters.scales.size(); ++i)
            if (!std::isfinite(analysis.baseline.parameters.scales[i]) || analysis.baseline.parameters.scales[i]<0)
                issue(out.phases[i].id, "invalid_scale", "Joint baseline scales must be finite and nonnegative");
        auto valid = validatePfsmState(*source);
        if (!valid) {
            issue({}, "invalid_pfsm_evidence", valid.error().message);
            return out;
        }
        if (source->algorithms->spec.selections().profile_expander != AlgorithmIdentity{"qualpf.profile.il.fixed", 1})
            issue({}, "unsupported_template_normalization", "RIR conversion requires the fixed peak-top IL convention");
        if (!search::profile_rir_supports_radiation(analysis.problem->radiation))
            issue({}, "unsupported_rir_radiation", "PFSM/RIR currently requires Cu Kalpha-compatible reference and observation radiation");
        const auto &d = analysis.baseline.diagnostics;
        if (!d.exact_final_verification || !d.kkt_checked || !std::isfinite(d.kkt_violation))
            issue({}, "unverified_joint_fit", "The selected joint baseline lacks final direct/KKT verification");
        if (d.rank_deficient || d.rank != out.phases.size())
            issue({}, "rank_deficient", "The selected templates cannot determine individual phase amounts");
        if (d.zero_templates.size() != out.phases.size())
            issue({}, "invalid_fit_diagnostics", "Joint template diagnostics are incomplete");
        out.qualifications.push_back("Reference-library RIR source conditions are unverified; Cu Kalpha compatibility is assumed. Preferred orientation, microabsorption and model mismatch can bias these semi-quantitative estimates.");
        if (!analysis.problem->radiation.kind)
            out.qualifications.push_back("Radiation kind was not explicitly recorded; the recorded Cu Kalpha wavelengths are used.");
        if (analysis.baseline.termination == profile::ProfileFitTermination::budget ||
            analysis.baseline.termination == profile::ProfileFitTermination::feasible_fallback ||
            analysis.baseline.termination == profile::ProfileFitTermination::no_progress)
            out.qualifications.push_back("The joint fit retained a verified feasible solution without a convergence guarantee.");
        std::vector<search::ProfileRirInput> converted;
        for (std::size_t i = 0; i < out.phases.size(); ++i) {
            auto &row = out.phases[i];
            const auto &reference = analysis.quantification_library->references.at(row.id);
            row.reference_maximum = reference.maximum_intensity;
            row.original_rir = reference.original_rir;
            row.rir = reference.rir;
            row.scale = analysis.baseline.parameters.scales[i];
            row.shift_degrees = analysis.baseline.parameters.shifts[i];
            if (const auto r = analysis.rir_revisions.find(row.id); r != analysis.rir_revisions.end()) {
                row.rir = r->second.value;
                row.rir_source = r->second.source;
                row.rir_revised = true;
            }
            if (!row.rir || !std::isfinite(*row.rir) || *row.rir <= 0)
                issue(row.id, reference.original_rir.empty() && !row.rir_revised ? "missing_rir" : "invalid_rir",
                      "A finite positive RIR is required for this selected phase");
            if (!std::isfinite(row.reference_maximum) || row.reference_maximum <= 0)
                issue(row.id, "invalid_reference_maximum", "The full frozen reference card has no positive intensity normalization");
            if (i < d.zero_templates.size() && d.zero_templates[i])
                issue(row.id, "zero_template", "The selected phase has a zero template on the observation grid");
            converted.push_back({row.scale, row.reference_maximum, row.rir});
        }
        if (!out.issues.empty()) return out;
        auto percentages = search::quantify_profile_rir(converted);
        if (!percentages) {
            const auto &error = percentages.error();
            const auto found = error.field.find('[');
            std::string id;
            if (found != std::string::npos)
                for (std::size_t i = 0; i < out.phases.size(); ++i)
                    if (error.field == "quantification.phase[" + std::to_string(i) + "]") id = out.phases[i].id;
            issue(id, error.reason, error.message);
            return out;
        }
        for (std::size_t i = 0; i < out.phases.size(); ++i) out.phases[i].weight_percent = percentages.value()[i];
    } else {
        if (out.phases.empty()) return out;
        if (inputs.size() != out.phases.size()) {
            issue({}, "invalid_classic_inputs", "Classical quantification inputs disagree with the snapshot");
            return out;
        }
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            if (inputs[i].calculated != source->phases[i].fomCalculated ||
                inputs[i].scale != source->phases[i].scale ||
                !std::ranges::equal(inputs[i].intensity, source->phases[i].intensity))
                issue(out.phases[i].id, "stale_classic_inputs", "Classical quantification inputs do not belong to this snapshot");
            out.phases[i].rir = inputs[i].rir;
            if (!inputs[i].rir || !std::isfinite(*inputs[i].rir) || *inputs[i].rir <= 0)
                issue(out.phases[i].id, "invalid_rir", "A finite positive RIR is required for this selected phase");
        }
        const auto percentages = quantifyProduct(*source, inputs);
        if (!out.issues.empty()) return out;
        if (percentages.size() != out.phases.size() ||
            std::any_of(percentages.begin(), percentages.end(), [](double value) { return !std::isfinite(value) || value < 0; })) {
            issue({}, "classic_quantification_unavailable", "Classical phase scores, RIR or algorithm provenance are unavailable");
            return out;
        }
        for (std::size_t i = 0; i < out.phases.size(); ++i) out.phases[i].weight_percent = percentages[i];
    }
    out.availability = QuantificationAvailability::available;
    return out;
}
} // namespace qualpf::application
