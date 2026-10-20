#include "qualpf/application/export/quantification_export.h"
#include "qualpf/application/product_files.h"
#include <charconv>
#include <limits>
namespace qualpf::application::exporting {
namespace {
std::string number(std::optional<double> value) {
    if (!value) return {};
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), *value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    return std::string(buffer, result.ptr);
}
std::string quoted(const std::string &value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string out = "\"";
    for (char ch : value) { out += ch; if (ch == '"') out += ch; }
    return out + '"';
}
std::string termination(profile::ProfileFitTermination value) {
    switch (value) {
    case profile::ProfileFitTermination::gradient: return "gradient";
    case profile::ProfileFitTermination::cost: return "cost";
    case profile::ProfileFitTermination::step: return "step";
    case profile::ProfileFitTermination::cost_and_step: return "cost_and_step";
    case profile::ProfileFitTermination::fixed_shifts: return "fixed_shifts";
    case profile::ProfileFitTermination::budget: return "budget";
    case profile::ProfileFitTermination::feasible_fallback: return "feasible_fallback";
    case profile::ProfileFitTermination::no_progress: return "no_progress";
    }
    return "unknown";
}
} // namespace
Result<std::string> prepareQuantificationCsv(const QuantificationResult &q) {
    if (!q.source || q.revision != q.source->revision || q.phases.size() != q.source->phases.size())
        return Result<std::string>{Error{ErrorCode::stale_snapshot,"quantification.export",
            "Quantification and canonical snapshot disagree","invalid_quantification_source"}};
    if (q.phases.empty())
        return Result<std::string>{Error{ErrorCode::missing_data,"quantification.export",
            "Select phases before exporting quantification","no_selected_phases"}};
    std::string out = "method,phase_id,phase_name,weight_percent,scale,shift_degrees,effective_rir,rir_source,rir_radiation,reference_rir,reference_maximum,status,reason,joint_rwp_net,revision,generation,database_version,rank,rank_deficient,kkt_checked,kkt_violation,exact_final_verification,zero_template,termination,qualifications\r\n";
    std::string notes;
    for (const auto &note : q.qualifications) { if (!notes.empty()) notes += '\n'; notes += note; }
    const bool pfsm = q.method == QuantificationMethod::pfsm_rir;
    for (std::size_t i=0; i<q.phases.size(); ++i) {
        const auto &p=q.phases[i];
        if (p.id != q.source->phases[i].id || (q.available() && !p.weight_percent))
            return Result<std::string>{Error{ErrorCode::stale_snapshot,"quantification.export",
                "Quantification phase identities or availability disagree","invalid_quantification_rows"}};
        std::string reasons;
        for (const auto &issue : q.issues) {
            if (!reasons.empty()) reasons += '\n';
            reasons += (issue.phase_id.empty() ? "" : issue.phase_id + ": ") + issue.reason + ": " + issue.message;
        }
        const auto &d=q.diagnostics;
        const std::vector<std::string> fields = {pfsm ? "PFSM/RIR" : "CSM/RIR", p.id, p.name,
            number(q.available() ? p.weight_percent : std::nullopt), number(p.scale), number(p.shift_degrees),
            number(p.rir), p.rir_source, p.rir_revised ? "Cu Kalpha" : "Reference conditions unverified",
            p.original_rir, pfsm ? number(p.reference_maximum) : "", q.available() ? "available" : "unavailable",
            reasons, number(q.rwp_net), std::to_string(q.revision), std::to_string(q.generation),
            std::to_string(q.database_version), d ? std::to_string(d->rank) : "",
            d ? (d->rank_deficient ? "true" : "false") : "", d ? (d->kkt_checked ? "true" : "false") : "",
            d ? number(d->kkt_violation) : "", d ? (d->exact_final_verification ? "true" : "false") : "",
            d && i<d->zero_templates.size() ? (d->zero_templates[i] ? "true" : "false") : "",
            q.termination ? termination(*q.termination) : "", notes};
        for (std::size_t j=0; j<fields.size(); ++j) { if (j) out += ','; out += quoted(fields[j]); }
        out += "\r\n";
    }
    return Result<std::string>{std::move(out)};
}
Status write_quantification_csv_to(AtomicExportTarget &target, const QuantificationResult &q) {
    auto bytes=prepareQuantificationCsv(q);
    if (!bytes) return Status{bytes.error()};
    if (!target.open()) return Status{Error{ErrorCode::invalid_input,"quantification.export.open",
        target.error_message(),"path_or_open_failure"}};
    if (target.write(bytes.value()) != static_cast<std::int64_t>(bytes.value().size())) {
        const auto message=target.error_message(); target.cancel();
        return Status{Error{ErrorCode::invalid_input,"quantification.export.write",message,"write_failure"}};
    }
    if (!target.commit()) {
        const auto message=target.error_message(); target.cancel();
        return Status{Error{ErrorCode::invalid_input,"quantification.export.commit",message,"commit_failure"}};
    }
    return Status{std::monostate{}};
}
Status writeQuantificationCsv(const AtomicExportTargetFactory &factory, const std::string &path,
                              const QuantificationResult &q) {
    auto valid=prepareQuantificationCsv(q);
    if (!valid) return Status{valid.error()};
    auto target=factory.create(path);
    if (!target) return Status{target.error()};
    if (!target.value()) return Status{Error{ErrorCode::missing_data,"quantification.export","No atomic target","missing_target"}};
    return write_quantification_csv_to(*target.value(),q);
}
} // namespace qualpf::application::exporting
