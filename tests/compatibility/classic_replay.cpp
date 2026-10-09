// Offline C++ observation producer. It never captures or labels reference data.
// JSON inputs are encoded into this private text protocol by
// classic_reference.py.
#include "classic_observation.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::search;
using namespace qualpf::testing;

namespace {
std::size_t count() {
    std::size_t n = 0;
    if (!(std::cin >> n) || n > 1000000)
        throw std::runtime_error("Invalid replay count");
    return n;
}
std::optional<double> optional_number() {
    int present;
    double value;
    if (!(std::cin >> present >> value) || (present != 0 && present != 1))
        throw std::runtime_error("Invalid optional number");
    return present ? std::optional<double>{value} : std::nullopt;
}
void print_json_string(const std::string &s) {
    std::cout << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\')
            std::cout << '\\' << c;
        else if (c < 32)
            std::cout << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else
            std::cout << c;
    }
    std::cout << '"';
}
void print(const ClassicObservation &out) {
    std::cout << std::setprecision(17) << "{\"error\":";
    if (out.numerical.error)
        std::cout << static_cast<int>(*out.numerical.error);
    else
        std::cout << "null";
    std::cout << ",\"fields\":{";
    bool first = true;
    for (const auto &[key, values] : out.numerical.fields) {
        if (!first)
            std::cout << ',';
        first = false;
        print_json_string(key);
        std::cout << ":[";
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                std::cout << ',';
            std::cout << values[i];
        }
        std::cout << ']';
    }
    std::cout << "},\"discrete\":{";
    first = true;
    for (const auto &[key, values] : out.discrete) {
        if (!first)
            std::cout << ',';
        first = false;
        print_json_string(key);
        std::cout << ":[";
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                std::cout << ',';
            print_json_string(values[i]);
        }
        std::cout << ']';
    }
    std::cout << "}}\n";
}
void failure(ClassicObservation &out, const Error &error) {
    out.numerical.error = error.code;
    out.discrete["error.field"] = {error.field};
}
} // namespace

int main() {
    try {
        int version = 0;
        std::string operation;
        std::cin >> version >> operation;
        if (version != 1)
            throw std::runtime_error("Unknown replay protocol");
        Pattern pattern;
        PeakSet peaks;
        SearchParameters p;
        pattern.id = DatasetId{1};
        peaks.dataset = pattern.id;
        double low = 0, high = 0;
        std::cin >> low >> high;
        pattern.calculation_range = AngularRange{TwoThetaDegrees{low}, TwoThetaDegrees{high}};
        for (std::size_t i = 0, n = count(); i < n; ++i) {
            double x = 0, y = 0;
            std::cin >> x >> y;
            pattern.two_theta.push_back(TwoThetaDegrees{x});
            pattern.observed.push_back(y);
        }
        for (std::size_t i = 0, n = count(); i < n; ++i) {
            Peak peak;
            std::cin >> peak.id.value >> peak.two_theta.value >> peak.intensity;
            peak.original_intensity = optional_number();
            if (auto x = optional_number())
                peak.d_spacing = DSpacingAngstrom{*x};
            if (auto x = optional_number())
                peak.delta_d = DSpacingAngstrom{*x};
            if (auto x = optional_number())
                peak.fwhm = AngularWidthDegrees{*x};
            peak.integrated_intensity = optional_number();
            peaks.peaks.push_back(peak);
        }
        std::cin >> p.delta_two_theta.value >> p.weight_position >> p.weight_intensity >> p.weight_phases >>
            p.minimum_score >> p.maximum_entries >> p.include_deleted_cards >> p.filter_strongest >>
            p.strongest_count;
        double residual_scale = 1;
        std::cin >> residual_scale;
        std::vector<CandidateInput> cards;
        for (std::size_t i = 0, n = count(); i < n; ++i) {
            CandidateInput candidate;
            std::cin >> std::quoted(candidate.card.database_id) >> std::quoted(candidate.card.card_id) >>
                std::quoted(candidate.card.quality);
            candidate.previous_scale = optional_number();
            int top_present = 0;
            if (!(std::cin >> top_present) || (top_present != 0 && top_present != 1))
                throw std::runtime_error("Invalid top-index presence flag");
            if (top_present) {
                candidate.stored_top.emplace();
                for (std::size_t j = 0, m = count(); j < m; ++j) {
                    double d = 0;
                    std::cin >> d;
                    candidate.stored_top->push_back(DSpacingAngstrom{d});
                }
            }
            for (std::size_t j = 0, m = count(); j < m; ++j) {
                Reflection r;
                double angle = 0;
                std::cin >> r.id.value >> angle >> r.intensity;
                r.two_theta = TwoThetaDegrees{angle};
                candidate.card.reflections.push_back(r);
            }
            cards.push_back(std::move(candidate));
        }
        if (!std::cin || ((operation == "score" || operation == "residual") && cards.size() != 1))
            throw std::runtime_error("Malformed replay input or invalid single-card count");
        std::cin >> std::ws;
        if (!std::cin.eof())
            throw std::runtime_error("Trailing replay data");
        ExecutionContext context;
        context.numerical_policy = NumericalPolicy::legacy_mixed_precision;
        ClassicObservation out;
        if (operation == "score") {
            out = observe(score_classic({pattern, peaks, cards.front().card, p}, context));
        } else if (operation == "residual") {
            auto r = subtract_phase(peaks, cards.front().card, residual_scale, p);
            if (!r)
                failure(out, r.error());
            else {
                out.discrete["associations"] = {};
                out.discrete["remaining_ids"] = {};
                out.discrete["removed_ids"] = {};
                out.numerical.fields["quality"] = {};
                for (const auto &a : r.value().associations) {
                    out.discrete["associations"].push_back(
                        a.reflection_index ? std::to_string(*a.reflection_index) : "none");
                    out.numerical.fields["quality"].push_back(a.quality);
                }
                for (const auto &peak : r.value().peaks.peaks) {
                    const auto key = "peak." + std::to_string(peak.id.value) + ".";
                    out.discrete["remaining_ids"].push_back(std::to_string(peak.id.value));
                    out.numerical.fields[key + "two_theta"] = {peak.two_theta.value};
                    out.numerical.fields[key + "intensity"] = {peak.intensity};
                    out.numerical.fields[key + "original_intensity"] = {*peak.original_intensity};
                    if (peak.d_spacing)
                        out.numerical.fields[key + "d_spacing"] = {peak.d_spacing->value};
                    if (peak.delta_d)
                        out.numerical.fields[key + "delta_d"] = {peak.delta_d->value};
                    if (peak.fwhm)
                        out.numerical.fields[key + "fwhm"] = {peak.fwhm->value};
                    if (peak.integrated_intensity)
                        out.numerical.fields[key + "integrated_intensity"] = {*peak.integrated_intensity};
                }
                for (auto id : r.value().removed)
                    out.discrete["removed_ids"].push_back(std::to_string(id.value));
            }
        } else if (operation == "initial" || operation == "rescore") {
            auto r = evaluate_candidates(pattern, peaks, cards, p,
                                         operation == "initial" ? SearchPass::initial : SearchPass::rescore,
                                         context);
            if (!r)
                failure(out, r.error());
            else {
                out.discrete["prefilter"] = {};
                out.discrete["retained"] = {};
                out.discrete["evaluated"] = {};
                for (bool pass : r.value().prefilter_passed)
                    out.discrete["prefilter"].push_back(pass ? "true" : "false");
                for (const auto &c : r.value().evaluated) {
                    // Separate components, no ambiguous concatenated qualified identity.
                    out.discrete["evaluated"].insert(out.discrete["evaluated"].end(),
                                                     {c.database_id, c.card_id});
                    const auto prefix =
                        "evaluated." + std::to_string(out.discrete["evaluated"].size() / 2 - 1) + ".";
                    if (c.outcome)
                        observe_score(out, c.outcome.value(), prefix);
                    else
                        out.discrete[prefix + "error"] = {
                            std::to_string(static_cast<int>(c.outcome.error().code)),
                            c.outcome.error().field};
                }
                for (const auto &c : r.value().results.candidates) {
                    out.discrete["retained"].insert(out.discrete["retained"].end(),
                                                    {c.database_id, c.card_id});
                    observe_score(out, c.outcome.value(),
                                  "retained." + std::to_string(out.discrete["retained"].size() / 2 - 1) +
                                      ".");
                }
            }
        } else
            throw std::runtime_error("Unknown replay operation");
        print(out);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
