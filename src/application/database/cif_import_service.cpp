#include "qualpf/application/database/cif_import_service.h"
#include "cif_parser.h"

namespace qualpf::application {
namespace {
namespace cif = io::cif;
CifImportFailure storage_failure(const database::WriteError &error) {
    return {"database", error.code, error.source, error.block, error.field, error.reason, {}, {}, {}, {}};
}
CifImportFailure import_failure(const cif::ImportIssue &error) {
    return {"import",         error.code,       error.source,      error.block,
            error.field,      error.message,    error.core_reason, error.location,
            error.candidates, error.provenance, error.core_field};
}

// Activation is bounded to the exact approved original sources. Other inputs
// keep the existing strict policy; filenames and database IDs grant no policy.
// The importer still checks every A1 source/block/site/tensor prerequisite.
cif::ImportPolicies approved_policies(std::string_view hash) {
    cif::ImportPolicies policies;
    if (hash == "bf25e35b06c55d9f35dc51f8afc992b4654844f433c5192327beacf3bffb038b")
        policies.source_ueq = cif::SourceUeqPolicy::zincite_2300112_v1;
    if (hash == "5e58cacb9b858f84f751357ff80fe3c8d3aa1b843c93928287a2d8998aaae802" ||
        hash == "1efc18e93353e40fb525d068012b343fb0f439beab5a9a104e8c0aa3a651d8d8" ||
        hash == "7efd473b9c742b4b8636393bb596ff91edfd299933994a4ce85bb3b1fec51215")
        policies.formula_z = cif::FormulaZPolicy::source_metadata_v1;
    return policies;
}
} // namespace

CifBackendResult cif_database_backend(std::string_view setting) {
    if (setting.empty() || setting == "cpp")
        return CifDatabaseBackend::cpp;
    if (setting == "legacy")
        return CifDatabaseBackend::cpp; // Explicit canonical compatibility owner.
    return CifImportFailure{"configuration",
                            "invalid_cif_database_backend",
                            {},
                            {},
                            "QUALPF_CIF_DB_BACKEND",
                            "Expected cpp or legacy",
                            {},
                            {},
                            {},
                            {}};
}
std::optional<CifImportFailure> CifImportService::create(const std::string &base) {
    if (auto error = writer_.create(base))
        return storage_failure(*error);
    return {};
}

CifPreparation CifImportService::prepare_source(CifSourceReader &reader, const std::string &path,
                                                std::int32_t identity, cif::BlockSelection selection) {
    auto read = reader.read(path, cif::Limits{}.bytes);
    if (auto error = std::get_if<CifImportFailure>(&read))
        return *error;
    return prepare_bytes(std::get<std::string>(read), path, identity, std::move(selection));
}

CifPreparation CifImportService::prepare_bytes(std::string_view bytes, std::string source,
                                               std::int32_t identity, cif::BlockSelection selection) {
    ++counters_.parser_calls;
    auto syntax = cif::parse(bytes, std::move(source));
    if (const auto *error = std::get_if<cif::Error>(&syntax))
        return CifImportFailure{"syntax",
                                cif::error_name(error->code),
                                error->source,
                                error->block,
                                error->field,
                                error->reason,
                                {},
                                error->location,
                                {},
                                {}};
    auto &document = std::get<cif::Document>(syntax);
    ++counters_.importer_calls;
    auto imported = cif::import_cif(document, std::move(selection),
                                    approved_policies(cif::sha256(document.original_bytes)));
    if (const auto *error = std::get_if<cif::ImportIssue>(&imported))
        return import_failure(*error);
    auto &crystal = std::get<cif::ImportedCrystal>(imported);
    auto ticket = cif::save_selection(document, crystal);
    if (const auto *error = std::get_if<cif::ImportIssue>(&ticket))
        return import_failure(*error);
    return finish(std::move(crystal), identity, std::move(std::get<std::string>(ticket)));
}

CifPreparation CifImportService::reload(std::string_view ticket, std::int32_t identity) {
    auto imported = cif::reload_selection(ticket);
    if (const auto *error = std::get_if<cif::ImportIssue>(&imported))
        return import_failure(*error);
    const auto &p = std::get<cif::ImportedCrystal>(imported).provenance;
    const auto allowed = approved_policies(p.source_sha256);
    if ((!p.source_ueq_policy.empty() && allowed.source_ueq == cif::SourceUeqPolicy::strict) ||
        (!p.formula_z_policy.empty() && allowed.formula_z == cif::FormulaZPolicy::strict))
        return CifImportFailure{"reload", "unapproved_product_policy_source",
                                p.source, p.selected_block,
                                "policy", "Ticket policy is outside the approved product source scope",
                                {},       {},
                                {},       p};
    return finish(std::move(std::get<cif::ImportedCrystal>(imported)), identity, std::string(ticket));
}

CifPreparation CifImportService::finish(cif::ImportedCrystal imported, std::int32_t identity,
                                        std::string ticket) {
    ++counters_.card_pipeline_calls;
    auto calculated = CifCardService::from_imported(std::move(imported));
    if (const auto *error = std::get_if<CifCardIssue>(&calculated))
        return CifImportFailure{std::string(cif_card_stage_name(error->stage)),
                                error->code,
                                error->source,
                                error->block,
                                error->field,
                                error->message,
                                error->core_reason,
                                error->location,
                                error->candidates,
                                error->provenance,
                                error->core_field};
    ++counters_.completed_phase13_18_chains;
    auto card = std::move(std::get<CifCard>(calculated));
    auto record = cif_card_record(card, identity);
    if (const auto *error = std::get_if<database::WriteError>(&record))
        return storage_failure(*error);
    return PreparedCifImport(std::move(card), std::move(std::get<database::CifRecord>(record)),
                             std::move(ticket));
}

std::optional<CifImportFailure> CifImportService::commit(const PreparedCifImport &prepared,
                                                         std::stop_token stop) {
    if (stop.stop_requested())
        return CifImportFailure{"commit",
                                "cancelled",
                                prepared.record().source,
                                prepared.record().block,
                                "transaction",
                                "Cancelled before card transaction",
                                {},
                                {},
                                {},
                                {}};
    ++counters_.writer_calls;
    if (auto error = writer_.write(prepared.record()))
        return storage_failure(*error);
    ++counters_.committed_cards;
    return {};
}
} // namespace qualpf::application
