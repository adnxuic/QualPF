#include "qualpf/application/pfsm.h"
#include "pfsm_workflow.h"
#include "qualpf/search/rir.h"
#include "qualpf/application/configured_fwhm.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace qualpf::application {
namespace {
bool sameRadiation(const Radiation &a, const Radiation &b) {
    if (a.kind != b.kind || a.lines.size() != b.lines.size())
        return false;
    for (std::size_t i = 0; i < a.lines.size(); ++i)
        if (a.lines[i].wavelength != b.lines[i].wavelength ||
            a.lines[i].relative_weight != b.lines[i].relative_weight)
            return false;
    return true;
}
bool sameBackground(const std::optional<SampledSignal> &a, const std::optional<SampledSignal> &b) {
    return bool(a) == bool(b) && (!a || (a->values == b->values && bool(a->window) == bool(b->window) &&
                                         (!a->window || a->window->first == b->window->first)));
}
bool samePattern(const Pattern &a, const Pattern &b) {
    return a.id == b.id && a.source_id == b.source_id && a.two_theta == b.two_theta &&
           a.corrected_two_theta == b.corrected_two_theta && a.observed == b.observed &&
           a.original_observed == b.original_observed &&
           a.original_kalpha2_subtracted == b.original_kalpha2_subtracted &&
           sameRadiation(a.radiation, b.radiation) && sameBackground(a.background, b.background) &&
           a.weights == b.weights &&
           a.processing.background_subtracted == b.processing.background_subtracted &&
           a.processing.kalpha2_subtracted == b.processing.kalpha2_subtracted &&
           a.processing.intensity_scaled == b.processing.intensity_scaled &&
           a.intensity_scale == b.intensity_scale;
}
} // namespace
void invalidatePfsmAfterEdit(const ProductState &previous, ProductState &next) {
    if (previous.prepared_pfsm && next.prepared_pfsm == previous.prepared_pfsm &&
        (next.algorithms != previous.algorithms || !samePattern(previous.pattern, next.pattern) ||
         next.original_signal_provenance != previous.original_signal_provenance)) next.prepared_pfsm.reset();
    if (!previous.pfsm || next.pfsm != previous.pfsm)
        return;
    if (next.algorithms == previous.algorithms && samePattern(previous.pattern, next.pattern) &&
        next.original_signal_provenance == previous.original_signal_provenance)
        return;
    next.pfsm.reset();
    for (auto *cards : {&next.candidates, &next.phases})
        for (auto &card : *cards) {
            card.pfsm.reset();
            card.fomCalculated = false;
            card.scale = 0;
            card.algorithmSource.reset();
        }
}
Status validatePfsmState(const ProductState &state) {
    if (!state.pfsm)
        return Status{std::monostate{}};
    const auto error = [] {
        return Status{Error{ErrorCode::invalid_input, "pfsm.state",
                            "Inconsistent PFSM analysis; use a checked joint search transaction",
                            "invalid_pfsm_state"}};
    };
    const auto &analysis = *state.pfsm;
    if (!state.algorithms || state.algorithms->spec.method() != SearchMethod::pfsm || !analysis.problem ||
        !analysis.universe || !analysis.quantification_library ||
        analysis.quantification_library->universe != analysis.universe ||
        analysis.quantification_library->references.size() != analysis.universe->size() ||
        analysis.problem->algorithms != state.algorithms ||
        !analysis.problem->observations || !analysis.problem->physical_width ||
        analysis.problem->original_signal_provenance != state.original_signal_provenance ||
        !std::isfinite(analysis.normalization_maximum) || analysis.normalization_maximum < 0 ||
        analysis.selected.size() != state.phases.size() || !analysis.generation)
        return error();
    if (!validateFwhmUsage(*analysis.problem->physical_width))
        return error();
    const auto mode = analysis.problem->candidate_recall.mode;
    const auto version = analysis.problem->candidate_recall.version;
    const auto &limits = analysis.problem->candidate_recall.limits;
    if (!profile::validatePfsmCandidateRecallLimits(limits)) return error();
    if (version != PfsmCandidateRecallVersion::v2) return error();
    if (mode != PfsmCandidateRecallMode::off && mode != PfsmCandidateRecallMode::shadow && mode != PfsmCandidateRecallMode::enabled)
        return error();
    if (mode == PfsmCandidateRecallMode::off && (analysis.candidate_recall_data || !analysis.candidate_recall_reports.empty() ||
        !analysis.normalization_over_full_pool)) return error();
    if (mode != PfsmCandidateRecallMode::enabled && !analysis.normalization_over_full_pool) return error();
    if (analysis.candidate_recall_data && (analysis.candidate_recall_data->observations != analysis.problem->observations ||
        analysis.candidate_recall_data->width != analysis.problem->physical_width->width.value ||
        analysis.candidate_recall_data->lower_shift != analysis.problem->lower_shift ||
        analysis.candidate_recall_data->upper_shift != analysis.problem->upper_shift)) return error();
    std::set<std::size_t> universe;
    for (std::size_t i = 0; mode != PfsmCandidateRecallMode::off && i < analysis.universe->size(); ++i)
        if (std::none_of(analysis.selected.begin(), analysis.selected.end(),
            [&](const auto &p) { return p.identity == (*analysis.universe)[i].id; })) universe.insert(i);
    for (const auto &r : analysis.candidate_recall_reports) {
        if (r.algorithm_version != version || r.limits != limits) return error();
        if (r.generation != analysis.generation || r.pool_size != universe.size() ||
            !std::isfinite(r.formal_baseline_q) || r.formal_baseline_q < 0 ||
            !std::isfinite(r.local_baseline_q) || r.local_baseline_q < 0) return error();
        for (const auto *indices : {&r.recalled_indices, &r.retained_indices})
            if (!std::is_sorted(indices->begin(), indices->end()) ||
                std::adjacent_find(indices->begin(), indices->end()) != indices->end() ||
                std::any_of(indices->begin(), indices->end(), [&](auto i) { return !universe.contains(i); }))
                return error();
        if (!r.full_fallback && !std::includes(r.recalled_indices.begin(), r.recalled_indices.end(),
                            r.retained_indices.begin(), r.retained_indices.end())) return error();
        if (r.full_fallback) {
            if (r.fallback_reason.empty() || r.retained_indices.size() != universe.size())
                return error();
        } else if (!r.fallback_reason.empty()) return error();
        if (!r.full_fallback || !r.candidates.empty()) {
            if (r.candidates.size() != universe.size()) return error();
            std::set<std::size_t> seen;
            for (const auto &e : r.candidates) {
                if (!universe.contains(e.original_index) || !seen.insert(e.original_index).second ||
                    e.recalled != std::binary_search(r.recalled_indices.begin(), r.recalled_indices.end(), e.original_index) ||
                    e.retained != std::binary_search(r.retained_indices.begin(), r.retained_indices.end(), e.original_index))
                    return error();
                for (auto value : {e.original_score, e.residual_score, e.local_q, e.gain})
                    if (value && (!std::isfinite(*value) || *value < 0)) return error();
                if (!std::isfinite(e.original_shift) || !std::isfinite(e.residual_shift)) return error();
                if (e.recalled != !e.recall_reason.empty() || e.retained != !e.retention_reason.empty())
                    return error();
                for (auto rank : {e.original_rank, e.residual_rank, e.local_rank})
                    if (rank && (*rank == 0 || *rank > universe.size())) return error();
                if (!e.protection_reason.empty() && (!e.recalled || !e.retained)) return error();
            }
        }
        if (r.selection) {
            const auto &s = *r.selection;
            if (s.limits != limits) return error();
            if (s.retained != r.retained_indices || s.full_fallback != r.full_fallback ||
                s.fallback_reason != r.fallback_reason || !std::isfinite(s.g_star) || s.g_star < 1e-12 ||
                r.refinement_candidates.size() != r.recalled_indices.size()) return error();
            for (const auto *indices : {&s.initial_top,&s.local_top,&s.channel,&s.review,&s.protected_indices,&s.proposed})
                if (!std::is_sorted(indices->begin(),indices->end()) ||
                    std::adjacent_find(indices->begin(),indices->end()) != indices->end() ||
                    !std::includes(r.recalled_indices.begin(),r.recalled_indices.end(),indices->begin(),indices->end()))
                    return error();
            if (s.review_skipped != (s.review.size()>static_cast<std::size_t>(limits.review_limit)) ||
                (s.review_skipped && !s.full_fallback) ||
                (s.proposed.size()>static_cast<std::size_t>(limits.formal_limit) && !s.full_fallback) ||
                (!s.full_fallback && s.proposed != s.retained)) return error();
            for (std::size_t i=0;i<r.refinement_candidates.size();++i) {
                const auto &e=r.refinement_candidates[i];
                if (e.recall.original_index!=r.recalled_indices[i] || !e.recall.recalled ||
                    e.nnls_calls>profile::PfsmCandidateRecallPolicy::total_calls ||
                    (!e.reviewed && e.nnls_calls>profile::PfsmCandidateRecallPolicy::initial_calls) ||
                    e.seeds.size()>profile::PfsmCandidateRecallPolicy::seeds ||
                    e.starts.size()>e.seeds.size() || !std::isfinite(e.best.gain) || e.best.gain<0 ||
                    !std::isfinite(e.best.q) || e.best.q<0 || !std::isfinite(e.maximum_distortion)) return error();
                for (auto v : {e.initial_gain,e.initial_eta,e.eta,e.formal_gain})
                    if (v && !std::isfinite(*v)) return error();
                for (const auto &seed:e.starts) {
                    if (seed.steps.size()>profile::PfsmCandidateRecallPolicy::steps || !std::isfinite(seed.initial_shift)) return error();
                    for (const auto &step:seed.steps)
                        if (!std::isfinite(step.actual) || !std::isfinite(step.predicted) ||
                            !std::isfinite(step.distortion) || !std::isfinite(step.physical.gain) ||
                            !std::isfinite(step.physical.q) || (step.ratio && !std::isfinite(*step.ratio))) return error();
                }
            }
        } else if (!r.full_fallback || !r.refinement_candidates.empty()) return error();
    }
    if (mode != PfsmCandidateRecallMode::off && (analysis.candidate_recall_reports.empty() || analysis.candidate_recall_reports.back().discarded ||
        analysis.candidate_recall_reports.back().formal_baseline_q != analysis.baseline.q)) return error();
    if (mode == PfsmCandidateRecallMode::enabled && analysis.normalization_over_full_pool !=
        (analysis.candidate_recall_reports.back().retained_indices.size() == universe.size())) return error();
    for (const auto &[id, revision] : analysis.rir_revisions)
        if (!analysis.quantification_library->references.contains(id) ||
            !std::isfinite(revision.value) || revision.value <= 0 ||
            revision.source.find_first_not_of(" \t\r\n") == std::string::npos ||
            revision.radiation != PfsmRirRevision::Radiation::cu_kalpha)
            return error();
    const auto n = analysis.problem->observations->coordinates.size(), m = analysis.selected.size();
    const auto &base = analysis.baseline;
    if (!base.diagnostics.exact_final_verification || base.parameters.scales.size() != m ||
        base.parameters.shifts.size() != m || base.calculated.size() != n || base.residual.size() != n ||
        !std::isfinite(base.q) || base.q < 0 || !std::isfinite(base.rwp_net) || base.rwp_net < 0 ||
        !(base.denominator_net > 0))
        return error();
    for (std::size_t j = 0; j < m; ++j)
        if (analysis.selected[j].identity != state.phases[j].id ||
            !analysis.quantification_library->references.contains(state.phases[j].id) ||
            state.phases[j].scale != base.parameters.scales[j] || !std::isfinite(base.parameters.scales[j]) ||
            base.parameters.scales[j] < 0 || !std::isfinite(base.parameters.shifts[j]) ||
            base.parameters.shifts[j] < analysis.selected[j].lower_shift ||
            base.parameters.shifts[j] > analysis.selected[j].upper_shift)
            return error();
    for (const auto &card : state.candidates) {
        if (!card.pfsm || !card.fomCalculated)
            return error();
        const auto &e = *card.pfsm;
        if (!analysis.normalization_over_full_pool && !std::binary_search(
            analysis.candidate_recall_reports.back().retained_indices.begin(), analysis.candidate_recall_reports.back().retained_indices.end(),
            e.original_index)) return error();
        if (!e.diagnostics.exact_final_verification || e.generation != analysis.generation ||
            e.original_index >= analysis.universe->size() ||
            (*analysis.universe)[e.original_index].id != card.id || e.parameters.scales.size() != m + 1 ||
            e.parameters.shifts.size() != m + 1 || !std::isfinite(e.delta_rwp) || e.delta_rwp < 0 ||
            e.delta_rwp != card.fom || e.delta_rwp > analysis.normalization_maximum || e.q > base.q)
            return error();
    }
    return Status{std::monostate{}};
}
Status restorePfsmEvidence(ProductState &state) try {
    using namespace pfsm_detail;
    const auto checkProblem = [&](const PfsmProblem &p) {
        if (!p.algorithms || p.algorithms != state.algorithms || !p.observations || !p.physical_width ||
            p.lower_shift > p.upper_shift || !std::isfinite(p.lower_shift) || !std::isfinite(p.upper_shift))
            fail("project.pfsm.problem","Incomplete frozen PFSM problem");
        take(validateFwhmUsage(*p.physical_width));
        take(profile::validatePfsmCandidateRecallLimits(p.candidate_recall.limits));
        if(p.fit.ftol<=0 || p.fit.xtol<=0 || p.fit.gtol<=0 ||
           !std::isfinite(p.fit.ftol) || !std::isfinite(p.fit.xtol) || !std::isfinite(p.fit.gtol))
            fail("project.pfsm.fit","Invalid frozen fitting tolerances");
        const auto &o=*p.observations; const auto n=o.coordinates.size();
        if (n<3 || o.observed.size()!=n || o.background.size()!=n || o.weights.size()!=n)
            fail("project.pfsm.observations","Unaligned PFSM observations");
        for(std::size_t i=0;i<n;++i)
            if(!std::isfinite(o.coordinates[i]) || !std::isfinite(o.observed[i]) ||
               !std::isfinite(o.background[i]) || !std::isfinite(o.weights[i]) || o.weights[i]<0 ||
               (i && o.coordinates[i]<=o.coordinates[i-1]))
                fail("project.pfsm.observations","Invalid frozen PFSM observations");
    };
    if(state.prepared_pfsm) checkProblem(*state.prepared_pfsm);
    if(!state.pfsm) return Status{std::monostate{}};
    auto analysis=std::make_shared<PfsmAnalysis>(*state.pfsm);
    if(!analysis->problem || !analysis->universe || !analysis->quantification_library)
        fail("project.pfsm","Incomplete frozen PFSM analysis");
    checkProblem(*analysis->problem);
    auto library=std::make_shared<PfsmQuantificationLibrary>(*analysis->quantification_library);
    library->universe=analysis->universe;
    std::set<std::string> identities;
    for(const auto &card:*analysis->universe) {
        if(card.id.empty() || !identities.insert(card.id).second || card.d.size()!=card.intensity.size())
            fail("project.pfsm.library","Invalid frozen reference library");
        const auto found=library->references.find(card.id);
        if(found==library->references.end() || found->second.maximum_intensity!=take(search::rir_reference_maximum(card.intensity)) ||
           found->second.rir!=card.referenceRir || found->second.original_rir!=card.rir)
            fail("project.pfsm.quantification","Quantification reference disagrees with frozen library");
    }
    analysis->quantification_library=std::move(library);
    ExecutionContext context{{},NumericalPolicy::explicit_float64,"qxp3-restore"};
    for(auto &phase:analysis->selected) {
        const auto found=std::find_if(analysis->universe->begin(),analysis->universe->end(),[&](const auto &card){return card.id==phase.identity;});
        if(found==analysis->universe->end()) fail("project.pfsm.selected","Missing selected reference");
        phase.profile=expand(*found,*analysis->problem,context);
    }
    state.pfsm=analysis;
    take(validatePfsmState(state));
    profile::ProfileFitInput input; input.observations=analysis->problem->observations;
    input.phases=analysis->selected; input.options=analysis->problem->fit;
    take(profile::validateProfileFitResult(analysis->baseline,input));
    // Check the saved calculation against its frozen phases without optimizing.
    const auto &o=*input.observations;
    std::vector<double> calculated(o.coordinates.size(),0);
    for(std::size_t j=0;j<input.phases.size();++j) {
        const auto samples=take(input.phases[j].profile->evaluateExact(o.coordinates,analysis->baseline.parameters.shifts[j],false,context));
        for(std::size_t i=0;i<calculated.size();++i) calculated[i]+=analysis->baseline.parameters.scales[j]*samples.intensity[i];
    }
    double savedObjective=0;
    for(std::size_t i=0;i<calculated.size();++i) {
        const double residual=o.observed[i]-o.background[i]-calculated[i];
        const double weighted=std::sqrt(o.weights[i])*residual; savedObjective+=weighted*weighted;
        const double tolerance=512*std::numeric_limits<double>::epsilon()*std::max({1.,std::abs(o.observed[i]),std::abs(calculated[i])});
        if(std::abs(calculated[i]+o.background[i]-analysis->baseline.calculated[i])>tolerance ||
           std::abs(residual-analysis->baseline.residual[i])>tolerance)
            fail("project.pfsm.baseline","Saved joint baseline disagrees with frozen inputs");
    }
    const double objectiveTolerance=512*std::numeric_limits<double>::epsilon()*std::max(1.,analysis->baseline.denominator_net);
    if(std::abs(savedObjective-analysis->baseline.q)>objectiveTolerance)
        fail("project.pfsm.baseline","Saved objective disagrees with the joint residual");
    for(const auto &card:state.candidates) {
        const auto &e=*card.pfsm;
        if(e.delta_q!=analysis->baseline.q-e.q ||
           std::abs(e.rwp_net-std::sqrt(e.q/analysis->baseline.denominator_net))>512*std::numeric_limits<double>::epsilon()*std::max(1.,e.rwp_net))
            fail("project.pfsm.candidate","Candidate objective disagrees with the joint baseline");
        for(std::size_t j=0;j<e.parameters.scales.size();++j) {
            const auto lower=j<analysis->selected.size()?analysis->selected[j].lower_shift:analysis->problem->lower_shift;
            const auto upper=j<analysis->selected.size()?analysis->selected[j].upper_shift:analysis->problem->upper_shift;
            if(e.parameters.scales[j]<0 || e.parameters.shifts[j]<lower || e.parameters.shifts[j]>upper)
                fail("project.pfsm.candidate","Candidate parameters are outside the frozen bounds");
        }
    }
    return Status{std::monostate{}};
} catch(const pfsm_detail::Failure &failure) { return Status{failure.error}; }

Result<ProductState> preparePfsmRirRevision(ProductHandle source, const std::string &id,
                                           std::optional<PfsmRirRevision> revision) {
    const auto invalid = [](const char *reason, const char *message) {
        return Result<ProductState>{Error{ErrorCode::invalid_input, "quantification.rir", message, reason}};
    };
    if (!source || !source->pfsm || source->editing)
        return invalid("no_pfsm_analysis", "A current closed PFSM analysis is required");
    auto valid = validatePfsmState(*source);
    if (!valid) return Result<ProductState>{valid.error()};
    if (std::none_of(source->phases.begin(), source->phases.end(), [&](const auto &p) { return p.id == id; }))
        return invalid("phase_not_selected", "The phase is no longer selected");
    if (revision && (!std::isfinite(revision->value) || revision->value <= 0 ||
        revision->source.find_first_not_of(" \t\r\n") == std::string::npos ||
        revision->radiation != PfsmRirRevision::Radiation::cu_kalpha))
        return invalid("invalid_rir_revision", "A finite positive RIR, source and Cu Kalpha convention are required");
    auto next = *source;
    const auto found = source->pfsm->rir_revisions.find(id);
    if ((!revision && found == source->pfsm->rir_revisions.end()) ||
        (revision && found != source->pfsm->rir_revisions.end() && found->second == *revision))
        return Result<ProductState>{std::move(next)};
    auto analysis = std::make_shared<PfsmAnalysis>(*source->pfsm);
    if (revision) analysis->rir_revisions[id] = std::move(*revision);
    else analysis->rir_revisions.erase(id);
    next.pfsm = std::move(analysis);
    return Result<ProductState>{std::move(next)};
}
} // namespace qualpf::application
