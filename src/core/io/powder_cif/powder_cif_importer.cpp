#include "powder_cif_importer.h"
#include "binary32.h"
#include "cif_parser.h"
#include "cif_tokenizer.h"
#include "qualpf/domain/validation.h"
#include "sha256.h"
#include <algorithm>
#include <array>
#include <exception>
#include <string_view>

namespace qualpf::io::powder_cif {
namespace {
namespace f = binary32;
constexpr std::array<std::string_view, 6> axes = {
    "_pd_proc_d_spacing",        "_pd_proc_2theta_fixed",
    "_pd_proc_2theta_corrected", "_pd_meas_2theta_corrected",
    "_pd_meas_2theta_fixed",     "_pd_meas_2theta_scan"};
constexpr std::array<std::string_view, 4> intensities = {
    "_pd_proc_intensity_net", "_pd_proc_intensity_total",
    "_pd_meas_counts_total", "_pd_meas_intensity_total"};
constexpr std::string_view wave = "_diffrn_radiation_wavelength";
constexpr std::string_view minimum = "_pd_meas_2theta_range_min";
constexpr std::string_view increment = "_pd_meas_2theta_range_inc";
bool signature(std::string_view tag) {
  return std::find(axes.begin(), axes.end(), tag) != axes.end() ||
         std::find(intensities.begin(), intensities.end(), tag) !=
             intensities.end() ||
         tag == minimum || tag == increment;
}
cif::Location original_location(std::string_view bytes, std::size_t offset) {
  cif::Location at;
  for (std::size_t i = 0; i < offset && i < bytes.size(); ++i) {
    if (bytes[i] == '\r') {
      if (i + 1 < offset && bytes[i + 1] == '\n')
        ++i;
      ++at.line;
      at.column = 1;
    } else if (bytes[i] == '\n') {
      ++at.line;
      at.column = 1;
    } else
      ++at.column;
  }
  at.offset = offset;
  return at;
}
LocatedField located(std::string tag, const cif::Value &value,
                     std::optional<std::size_t> row = {}) {
  return {std::move(tag),
          value.location,
          {value.location.offset, value.location.offset + value.raw.size()},
          row};
}
LocatedField located(const cif::Tag &tag) {
  return {tag.name,
          tag.location,
          {tag.location.offset, tag.location.offset + tag.spelling.size()},
          {}};
}
ImportError error(const ParsedPowderSource &source,
                  const PowderPatternBlockCandidate &block, std::string reason,
                  std::vector<LocatedField> fields = {},
                  std::string field = "pattern.cif") {
  if (fields.empty())
    fields.push_back(
        {{},
         original_location(source.artifact->bytes, block.source_span.begin),
         block.source_span,
         {}});
  return {{qualpf::ErrorCode::invalid_input, std::move(field),
           "Powder CIF input rejected", std::move(reason)},
          source.artifact,
          block.block_id,
          block.block_index,
          std::move(fields),
          {},
          {}};
}
ColumnCandidate column(const cif::Block &block, std::string_view tag) {
  auto source = *block.column(tag);
  ColumnCandidate out;
  out.tag = tag;
  out.field = located(source.loop->tags[source.column]);
  out.status = "usable";
  for (std::size_t i = 0; i < source.loop->rows(); ++i)
    out.values.push_back(source.at(i));
  if (std::any_of(out.values.begin(), out.values.end(),
                  [](const auto &v) { return v.missing(); })) {
    out.status = "missing_column";
    return out;
  }
  for (const auto &value : out.values) {
    if (value.kind != cif::ValueKind::bare) {
      out.status = "quoted_column_number";
      return out;
    }
    try {
      auto bits = f::decode(value.text);
      if (!f::finite(bits)) {
        out.status = "nonfinite_decoded";
        return out;
      }
      out.bits.push_back(bits);
    } catch (const std::exception &failure) {
      out.status = failure.what();
      return out;
    }
  }
  return out;
}
ScalarField scalar(const cif::Block &block, std::string_view name) {
  ScalarField out;
  out.tag = name;
  for (const auto &item : block.items)
    if (item.tag.name == name) {
      out.field = located(item.tag);
      out.first = item.value;
      break;
    }
  if (!out.first)
    if (auto col = block.column(name)) {
      out.field = located(col->loop->tags[col->column]);
      out.first = col->at(0);
    }
  if (!out.first)
    return out;
  if (out.first->missing()) {
    out.status = "missing";
    return out;
  }
  try {
    auto bits = f::decode(out.first->text);
    out.status = f::finite(bits) ? "usable" : "nonfinite_decoded";
    if (out.status == "usable")
      out.bits = bits;
  } catch (const std::exception &failure) {
    out.status = failure.what();
  }
  return out;
}
ImportResult<PowderCifImportSnapshot> finish(const ParsedPowderSource &source,
                                             PowderPatternBlockCandidate block,
                                             RadiationProvenance radiation) {
  PowderCifImportSnapshot result;
  result.source = source.artifact;
  result.source_fragment_without_data_block =
      source.source_fragment_without_data_block;
  result.selected_block = block;
  result.radiation = radiation;
  result.raw_y = block.selected_intensity->bits;
  const bool d = block.selected_axis && block.selected_axis->tag == axes[0];
  try {
    for (std::size_t i = 0; i < block.row_count; ++i) {
      std::uint32_t x;
      if (!block.selected_axis) {
        x = f::add(*block.generated_axis->minimum.bits,
                   f::mul(f::index(i), *block.generated_axis->increment.bits));
      } else if (d) {
        auto original = block.selected_axis->bits[i];
        auto arg =
            f::divide(f::mul(0x3f000000, *radiation.wavelength_bits), original);
        if (!f::finite(arg) || !f::less(0, arg) || f::less(0x3f800000, arg))
          return error(source, block, "d_conversion_domain",
                       {located(block.selected_axis->tag,
                                block.selected_axis->values[i], i)});
        x = f::mul(f::mul(0x40000000, f::certifiedAsin(arg)), 0x42652ee0);
      } else
        x = block.selected_axis->bits[i];
      if (!f::finite(x))
        return error(source, block, "nonfinite_axis");
      result.x.push_back(x);
    }
  } catch (const std::exception &failure) {
    return error(
        source, block,
        failure.what()); // No numerical fallback or uncertified result.
  }
  result.reversed = d && f::less(result.x.back(), result.x.front());
  if (result.reversed) {
    std::reverse(result.x.begin(), result.x.end());
    std::reverse(result.raw_y.begin(), result.raw_y.end());
  }
  for (std::size_t i = 1; i < result.x.size(); ++i)
    if (!f::less(result.x[i - 1], result.x[i])) {
      auto fields =
          block.selected_axis
              ? std::vector<LocatedField>{located(
                    block.selected_axis->tag,
                    block.selected_axis
                        ->values[result.reversed ? result.x.size() - 1 - i : i],
                    i)}
              : std::vector<LocatedField>{
                    *block.generated_axis->minimum.field,
                    *block.generated_axis->increment.field};
      return error(source, block, "nonincreasing_axis", fields);
    }
  auto max =
      *std::max_element(result.raw_y.begin(), result.raw_y.end(), f::less);
  for (std::size_t i = 0; i < result.raw_y.size(); ++i) {
    auto scaled = f::divide(f::mul(0x447a0000, result.raw_y[i]), max);
    if (!f::finite(scaled))
      return error(
          source, block, "nonfinite_scaled_intensity",
          {located(
              block.selected_intensity->tag,
              block.selected_intensity
                  ->values[result.reversed ? result.raw_y.size() - 1 - i : i],
              i)});
    result.scaled_y.push_back(scaled);
  }
  result.selected_block.importability = CandidateState::valid;
  result.selected_block.completeness = "complete";
  result.notices = block.notices;
  return result;
}
PowderPatternBlockCandidate candidate(const ParsedPowderSource &source,
                                      std::size_t index) {
  const auto &block = source.document.blocks[index];
  PowderPatternBlockCandidate out;
  out.block_id = block.name;
  out.block_index = index;
  out.source_span = {block.location.offset,
                     index + 1 < source.document.blocks.size()
                         ? source.document.blocks[index + 1].location.offset
                         : source.artifact->bytes.size()};
  out.wavelength = scalar(block, wave);
  out.range_minimum = scalar(block, minimum);
  out.range_increment = scalar(block, increment);
  bool pattern = false;
  for (const auto &item : block.items)
    pattern |= signature(item.tag.name);
  for (const auto &loop : block.loops)
    for (const auto &tag : loop.tags)
      pattern |= signature(tag.name);
  if (!pattern) {
    out.notices.push_back({"no_pattern_signature", {}, out.source_span, index});
    return out;
  }
  out.completeness = "incomplete";
  out.importability = CandidateState::incomplete;
  auto reject = [&](std::string reason, std::vector<LocatedField> fields = {},
                    std::string field = "pattern.cif") {
    out.errors.push_back(error(source, out, std::move(reason),
                               std::move(fields), std::move(field)));
    out.importability = CandidateState::invalid;
  };
  for (auto tag : axes)
    if (block.column(tag))
      out.available_axis_candidates.push_back(column(block, tag));
  for (auto tag : intensities)
    if (block.column(tag))
      out.available_intensity_candidates.push_back(column(block, tag));
  auto select = [&](const auto &candidates) -> std::optional<ColumnCandidate> {
    for (const auto &col : candidates) {
      if (col.status == "missing_column") {
        out.notices.push_back(
            {"missing_column_fallback", col.tag, col.field.span, index});
        continue;
      }
      if (col.status != "usable") {
        auto fields = std::vector<LocatedField>{col.field};
        for (std::size_t row = 0; row < col.values.size(); ++row)
          fields.push_back(located(col.tag, col.values[row], row));
        reject(col.status, std::move(fields));
        return {};
      }
      return col;
    }
    return {};
  };
  out.selected_intensity = select(out.available_intensity_candidates);
  if (!out.selected_intensity) {
    if (out.errors.empty())
      out.notices.push_back({"missing_intensity", {}, out.source_span, index});
    return out;
  }
  const auto &y = *out.selected_intensity;
  out.row_count = y.bits.size();
  if (out.row_count < 2) {
    reject("insufficient_points", {y.field});
    return out;
  }
  auto max = *std::max_element(y.bits.begin(), y.bits.end(), f::less);
  if (!(max & 0x7fffffff)) {
    reject("zero_intensity_max", {y.field});
    return out;
  }
  for (std::size_t i = 0; i < y.bits.size(); ++i)
    if (!f::finite(f::divide(f::mul(0x447a0000, y.bits[i]), max))) {
      reject("nonfinite_scaled_intensity", {located(y.tag, y.values[i], i)});
      return out;
    }
  out.selected_axis = select(out.available_axis_candidates);
  if (!out.errors.empty())
    return out;
  const auto &w = out.wavelength;
  if (w.status != "absent" && w.status != "missing" && w.status != "usable") {
    reject(w.status, {*w.field, located(w.tag, *w.first)},
           "pattern.cif.wavelength");
    return out;
  }
  if (w.bits) {
    if (!f::less(0, *w.bits)) {
      reject("invalid_wavelength", {*w.field}, "pattern.cif.wavelength");
      return out;
    }
    out.wavelength_state = WavelengthState::SOURCE_WAVELENGTH;
  }
  if (out.selected_axis) {
    const auto &x = *out.selected_axis;
    if (x.bits.size() != y.bits.size()) {
      reject("mismatched_rows", {x.field, y.field});
      out.errors.back().axis_count = x.bits.size();
      out.errors.back().intensity_count = y.bits.size();
      return out;
    }
    if (x.tag == axes[0]) {
      for (std::size_t i = 0; i < x.bits.size(); ++i)
        if (!f::less(0, x.bits[i])) {
          reject("nonpositive_d", {located(x.tag, x.values[i], i)});
          return out;
        }
      if (!w.bits) {
        out.completeness = "complete_source_requires_resolution";
        out.importability = CandidateState::requires_wavelength;
        out.wavelength_state = WavelengthState::WAVELENGTH_REQUIRED;
        return out;
      }
    }
  } else {
    for (const auto *field : {&out.range_minimum, &out.range_increment})
      if (field->status != "absent" && field->status != "missing" &&
          field->status != "usable") {
        reject(field->status,
               {*field->field, located(field->tag, *field->first)});
        return out;
      }
    if (!out.range_minimum.bits || !out.range_increment.bits) {
      out.notices.push_back(
          {"generated_axis_incomplete", {}, out.source_span, index});
      return out;
    }
    if (!f::less(0, *out.range_increment.bits)) {
      reject("nonpositive_increment", {*out.range_increment.field});
      return out;
    }
    out.generated_axis =
        GeneratedAxis{out.range_minimum, out.range_increment, out.row_count};
  }
  RadiationProvenance provenance;
  if (w.bits)
    provenance = {RadiationOrigin::source_file, w.field, w.bits,
                  RadiationKind::x_ray};
  auto completed = finish(source, out, provenance);
  if (auto failure = std::get_if<ImportError>(&completed)) {
    out.errors.push_back(*failure);
    out.importability = CandidateState::invalid;
    return out;
  }
  out.completeness = "complete";
  out.importability = CandidateState::valid;
  return out;
}
} // namespace

ImportResult<ParsedPowderSource>
PowderCifSyntaxAdapter::parse(std::string bytes, std::string source_id) {
  auto artifact = std::make_shared<const SourceArtifact>(
      SourceArtifact{bytes, source_sha256(bytes), source_id});
  bool has_data = false, has_powder = false;
  cif::Tokenizer scan(bytes);
  for (;;) {
    auto next = scan.next();
    if (std::holds_alternative<cif::Error>(next))
      break;
    const auto &token = std::get<cif::Token>(next);
    if (token.kind == cif::TokenKind::end)
      break;
    has_data |= token.kind == cif::TokenKind::data;
    has_powder |= token.kind == cif::TokenKind::tag &&
                  signature(cif::ascii_lower(token.value.text));
  }
  const bool synthetic = !has_data && has_powder;
  const std::string prefix = synthetic ? "data___qualpf_powder_fragment\n" : "";
  cif::Limits limits;
  // The logical container consumes neither the source byte nor token allowance.
  if (synthetic && bytes.size() <= limits.bytes) {
    limits.bytes += prefix.size();
    ++limits.tokens;
  }
  auto parsed = cif::parse(prefix + bytes, source_id, limits);
  auto mapped = [&](cif::Location loc) {
    if (synthetic) {
      if (loc.offset >= prefix.size()) {
        loc.offset -= prefix.size();
        --loc.line;
      } else
        loc = {0, 1, 1};
    }
    return loc;
  };
  if (auto failure = std::get_if<cif::Error>(&parsed)) {
    auto at = mapped(failure->location);
    ImportError out{{qualpf::ErrorCode::invalid_input, "pattern.cif",
                     failure->reason, cif::error_name(failure->code)},
                    artifact,
                    failure->block,
                    {},
                    {{failure->field, at, {at.offset, at.offset}, {}}},
                    {},
                    {}};
    // Recover the partial row's span without changing the generic error
    // contract.
    cif::Tokenizer tokens(prefix + bytes, limits);
    std::size_t block_count = 0, columns = 0, count = 0, row_start = 0;
    bool in_loop = false, tags = false;
    for (;;) {
      auto next = tokens.next();
      if (std::holds_alternative<cif::Error>(next))
        break;
      const auto &t = std::get<cif::Token>(next);
      if (t.value.location.offset >= failure->location.offset ||
          t.kind == cif::TokenKind::end)
        break;
      if (t.kind == cif::TokenKind::data)
        ++block_count;
      if (t.kind == cif::TokenKind::loop) {
        in_loop = true;
        tags = true;
        columns = 0;
        count = 0;
      } else if (in_loop && tags && t.kind == cif::TokenKind::tag)
        ++columns;
      else if (in_loop && t.kind == cif::TokenKind::value) {
        tags = false;
        if (columns && count % columns == 0)
          row_start = mapped(t.value.location).offset;
        ++count;
      } else {
        in_loop = false;
      }
    }
    if (block_count)
      out.block_index = block_count - 1;
    if (failure->code == cif::ErrorCode::incomplete_loop_row && columns &&
        count % columns)
      out.locations.front().span = {row_start, at.offset};
    return out;
  }
  auto doc = std::get<cif::Document>(std::move(parsed));
  for (auto &block : doc.blocks) {
    block.location = mapped(block.location);
    for (auto &item : block.items) {
      item.tag.location = mapped(item.tag.location);
      item.value.location = mapped(item.value.location);
    }
    for (auto &loop : block.loops) {
      loop.location = mapped(loop.location);
      for (auto &tag : loop.tags)
        tag.location = mapped(tag.location);
      for (auto &value : loop.values)
        value.location = mapped(value.location);
    }
  }
  doc.original_bytes = bytes;
  return ParsedPowderSource{artifact, std::move(doc), synthetic};
}

ImportResult<PowderCifPreparation>
PowderCifImporter::prepare(std::string bytes, std::string source_id) {
  auto syntax =
      PowderCifSyntaxAdapter::parse(std::move(bytes), std::move(source_id));
  if (auto failure = std::get_if<ImportError>(&syntax))
    return *failure;
  PowderCifPreparation result;
  result.source_ = std::get<ParsedPowderSource>(std::move(syntax));
  // Semantics stop at the first complete source. Whole-file syntax already
  // passed.
  for (std::size_t i = 0; i < result.source_.document.blocks.size(); ++i) {
    auto block = candidate(result.source_, i);
    result.candidates_.push_back(block);
    if (block.importability == CandidateState::invalid)
      return block.errors.front();
    if (block.importability == CandidateState::no_pattern ||
        block.importability == CandidateState::incomplete)
      continue;
    result.selected_ = result.candidates_.size() - 1;
    if (block.importability == CandidateState::valid) {
      RadiationProvenance provenance;
      if (block.wavelength.bits)
        provenance = {RadiationOrigin::source_file, block.wavelength.field,
                      block.wavelength.bits, RadiationKind::x_ray};
      result.snapshot_ = std::get<PowderCifImportSnapshot>(
          finish(result.source_, block, provenance));
      for (const auto &c : result.candidates_)
        if (c.block_index != block.block_index)
          result.snapshot_->notices.insert(result.snapshot_->notices.end(),
                                           c.notices.begin(), c.notices.end());
    }
    return result;
  }
  PowderPatternBlockCandidate absent;
  absent.source_span = {0, result.source_.artifact->bytes.size()};
  auto failure = error(result.source_, absent, "no_importable_pattern");
  for (const auto &c : result.candidates_)
    for (const auto &n : c.notices)
      failure.locations.push_back(
          {n.tag,
           original_location(result.source_.artifact->bytes, n.span.begin),
           n.span,
           {}});
  failure.block_index.reset();
  return failure;
}

ImportResult<PowderCifImportSnapshot>
PowderCifImporter::resolve(const PowderCifPreparation &source,
                           UserSuppliedRadiation supplied) {
  const auto &block = source.selected();
  if (block.wavelength.bits)
    return *source.snapshot(); // Source first wave owns interpretation.
  if (!f::finite(supplied.wavelength_bits) ||
      !f::less(0, supplied.wavelength_bits))
    return error(source.source(), block, "invalid_wavelength", {},
                 "pattern.cif.wavelength");
  std::optional<RadiationKind> kind;
  if (supplied.kind)
    kind = *supplied.kind == UserSuppliedRadiation::Kind::neutron
               ? RadiationKind::neutron
               : RadiationKind::x_ray;
  auto completed = finish(
      source.source(), block,
      {RadiationOrigin::user_supplied, {}, supplied.wavelength_bits, kind});
  if (auto snapshot = std::get_if<PowderCifImportSnapshot>(&completed))
    for (const auto &c : source.candidates())
      if (c.block_index != block.block_index)
        snapshot->notices.insert(snapshot->notices.end(), c.notices.begin(),
                                 c.notices.end());
  return completed;
}

ImportResult<ImportedPowderPattern>
PowderCifImporter::to_pattern(PowderCifImportSnapshot snapshot, DatasetId id) {
  ImportedPowderPattern imported;
  auto &pattern = imported.pattern;
  pattern.id = id;
  pattern.source_id = snapshot.source->source_id;
  for (auto x : snapshot.x)
    pattern.two_theta.emplace_back(f::value(x));
  for (auto y : snapshot.scaled_y)
    pattern.observed.push_back(f::value(y));
  pattern.original_observed.emplace();
  for (auto y : snapshot.raw_y)
    pattern.original_observed->push_back(f::value(y));
  pattern.corrected_two_theta = pattern.two_theta;
  pattern.original_kalpha2_subtracted = false;
  pattern.processing = {false, false, true};
  pattern.intensity_scale = IntensityScale::relative;
  pattern.source_precision = SourcePrecision::float32_abi;
  if (!pattern.two_theta.empty())
    pattern.calculation_range =
        AngularRange{pattern.two_theta.front(), pattern.two_theta.back()};
  if (snapshot.radiation.wavelength_bits) {
    pattern.radiation.kind = snapshot.radiation.kind;
    pattern.radiation.lines.push_back(
        {WavelengthAngstrom{f::value(*snapshot.radiation.wavelength_bits)},
         1.0});
  }
  auto issues = validate(pattern);
  if (!issues.empty()) {
    ImportError failure{issues.front(),
                        snapshot.source,
                        snapshot.selected_block.block_id,
                        snapshot.selected_block.block_index,
                        {},
                        {},
                        {}};
    return failure;
  }
  imported.provenance = std::move(snapshot);
  return imported;
}
} // namespace qualpf::io::powder_cif
