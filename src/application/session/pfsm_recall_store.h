#pragma once
#include "pfsm_workflow.h"
#include <cfenv>
#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace qualpf::application::pfsm_detail {
// Private builder until all workers join; publication exposes only const access.
// Slots are reserved in original-library order, independent of worker scheduling.
class PfsmRecallStore final {
    struct Slot { std::size_t offset = 0, capacity = 0, count = 0; bool reserved = false, ready = false; };
    std::shared_ptr<const PfsmProblem> problem_;
    std::shared_ptr<const std::vector<ProductCard>> universe_;
    NumericalPolicy policy_;
    int rounding_ = std::fegetround();
    unsigned denormals_ = denormals();
    std::vector<Slot> slots_;
    std::vector<profile::ProfileLine> lines_;
    static unsigned denormals() noexcept {
#if defined(__SSE2__) || defined(_M_X64)
        return _mm_getcsr() & 0x8040;
#else
        return 0;
#endif
    }
  public:
    static std::size_t retentionBudget(const PfsmProblem &problem, std::span<const ProductCard> universe) noexcept {
        // Small scans do not amortize persistent storage. Keep their original
        // path: the long-spectrum RSS controls exposed this fixed retention cost.
        constexpr std::size_t minimum = 4 * 1024 * 1024, maximum = 128 * 1024 * 1024;
        const auto components = problem.radiation.lines.size();
        std::size_t bytes = 0;
        for (const auto &card : universe) {
            if (card.d.empty()) continue;
            if (components > (minimum - bytes) / sizeof(profile::ProfileLine) / card.d.size()) return maximum;
            bytes += card.d.size() * components * sizeof(profile::ProfileLine);
            if (bytes >= minimum) return maximum;
        }
        return 0;
    }
    PfsmRecallStore(std::shared_ptr<const PfsmProblem> problem,
        std::shared_ptr<const std::vector<ProductCard>> universe, const ExecutionContext &context,
        std::size_t budget = 128 * 1024 * 1024)
        : problem_(std::move(problem)), universe_(std::move(universe)), policy_(context.numerical_policy) {
        if (budget < sizeof(*this) || universe_->size() > (budget - sizeof(*this)) / sizeof(Slot)) return;
        slots_.resize(universe_->size());
        if (bytes() > budget) { std::vector<Slot>().swap(slots_); return; }
        const auto available = (budget - bytes()) / sizeof(profile::ProfileLine);
        const auto radiation = problem_->radiation.lines.size();
        std::size_t count = 0;
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            checkpoint(context);
            const auto peaks = (*universe_)[i].d.size();
            if (radiation && peaks > (available - count) / radiation) continue;
            auto &slot = slots_[i];
            slot.offset = count; slot.capacity = peaks * radiation; slot.reserved = true;
            count += slot.capacity;
        }
        lines_.resize(count);
        if (bytes() > budget) { std::vector<Slot>().swap(slots_); std::vector<profile::ProfileLine>().swap(lines_); }
    }
    bool matches(const std::shared_ptr<const PfsmProblem> &problem,
        const std::shared_ptr<const std::vector<ProductCard>> &universe, const ExecutionContext &context) const noexcept {
        return problem_ == problem && universe_ == universe && policy_ == context.numerical_policy &&
            rounding_ == std::fegetround() && denormals_ == denormals();
    }
    bool reserved(std::size_t index) const noexcept { return index < slots_.size() && slots_[index].reserved; }
    std::optional<std::span<const profile::ProfileLine>> get(std::size_t index) const noexcept {
        if (index >= slots_.size() || !slots_[index].ready) return {};
        const auto &slot = slots_[index];
        return std::span<const profile::ProfileLine>(lines_).subspan(slot.offset, slot.count);
    }
    void store(std::size_t index, std::span<const profile::ProfileLine> value) {
        if (!reserved(index) || value.size() > slots_[index].capacity) return;
        auto &slot = slots_[index];
        std::copy(value.begin(), value.end(), lines_.begin() + slot.offset);
        slot.count = value.size(); slot.ready = true;
    }
    std::size_t bytes() const noexcept {
        if (slots_.capacity() == 0) return 0;
        return sizeof(*this) + slots_.capacity() * sizeof(Slot) + lines_.capacity() * sizeof(profile::ProfileLine);
    }
};
} // namespace qualpf::application::pfsm_detail
