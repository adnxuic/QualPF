#include "qualpf/application/database/cif_card_service.h"
#include "cif_parser.h"
#include "qualpf/domain/radiation.h"

namespace qualpf::application {
namespace {
CifCardIssue calculation_issue(CifCardStage stage, const Error &error,
                               const io::cif::StructureMetadata &metadata) {
  return {
      stage, "core_calculation", metadata.source, metadata.block, error.field,
      {},    error.message,      error.field,     error.reason,   error.code,
      {}};
}
} // namespace

std::string_view cif_card_stage_name(CifCardStage stage) noexcept {
  switch (stage) {
  case CifCardStage::syntax:
    return "syntax";
  case CifCardStage::import:
    return "import";
  case CifCardStage::reflections:
    return "reflections";
  case CifCardStage::factors:
    return "factors";
  case CifCardStage::powder:
    return "powder";
  case CifCardStage::card:
    return "card";
  }
  return "unknown";
}

CifCardResult CifCardService::from_bytes(std::string_view bytes,
                                         std::string source,
                                         io::cif::Limits limits,
                                         io::cif::BlockSelection selection,
                                         io::cif::ImportPolicies policies) {
  auto document = io::cif::parse(bytes, std::move(source), limits);
  if (const auto *error = std::get_if<io::cif::Error>(&document))
    return CifCardIssue{CifCardStage::syntax,
                        std::string(io::cif::error_name(error->code)),
                        error->source,
                        error->block,
                        error->field,
                        error->location,
                        error->reason,
                        {},
                        {},
                        {},
                        {}};
  return from_document(std::get<io::cif::Document>(document),
                       std::move(selection), policies);
}
CifCardResult CifCardService::from_document(const io::cif::Document &document,
                                            io::cif::BlockSelection selection,
                                            io::cif::ImportPolicies policies) {
  auto imported = io::cif::import_cif(document, std::move(selection), policies);
  if (const auto *error = std::get_if<io::cif::ImportIssue>(&imported))
    return CifCardIssue{CifCardStage::import, error->code,
                        error->source,        error->block,
                        error->field,         error->location,
                        error->message,       error->core_field,
                        error->core_reason,   {},
                        error->candidates,    error->provenance};
  return from_imported(std::move(std::get<io::cif::ImportedCrystal>(imported)));
}

CifCardResult CifCardService::from_imported(io::cif::ImportedCrystal imported) {
  using namespace crystallography;
  const auto &structure = imported.structure;
  const auto &metadata = imported.metadata;
  ReflectionGenerationOptions generation;
  generation.wavelength = radiation::default_wavelength;
  generation.minimum_d = DSpacingAngstrom{.1};
  generation.minimum_two_theta = TwoThetaDegrees{1};
  generation.maximum_two_theta = TwoThetaDegrees{150};
  generation.friedel = FriedelPolicy::merge;
  auto reflections =
      generate_reflections(structure.cell(), structure.group(), generation);
  if (!reflections)
    return calculation_issue(CifCardStage::reflections, reflections.error(),
                             metadata);
  PowderIntensityOptions options;
  options.radiation.wavelength = generation.wavelength;
  auto factors = PowderStructureFactors::calculate(
      structure, reflections.value(), options.radiation);
  if (!factors)
    return calculation_issue(CifCardStage::factors, factors.error(), metadata);
  auto physical = powder_lines(reflections.value(), factors.value(), options);
  if (!physical)
    return calculation_issue(CifCardStage::powder, physical.error(), metadata);
  auto card = TheoreticalReferenceCard::create(
      structure, physical.value(),
      {imported.provenance.identity(), metadata.chemical_name,
       metadata.mineral_name, metadata.source},
      options.radiation, generation.friedel);
  if (!card)
    return calculation_issue(CifCardStage::card, card.error(), metadata);
  return CifCard{std::move(imported), std::move(card).value(),
                 reflections.value().reflections().size()};
}
} // namespace qualpf::application
