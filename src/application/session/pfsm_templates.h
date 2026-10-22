#pragma once
#include "pfsm_workflow.h"
#include <atomic>

namespace qualpf::application::pfsm_detail {
class PfsmTemplateStore final {
    struct Slot {
        profile::PreparedProfileTemplateHandle prepared;
        std::size_t reservation = 0;
        PfsmTemplateStatistics statistics;
    };
    std::shared_ptr<const PfsmProblem> problem_;
    std::shared_ptr<const std::vector<ProductCard>> universe_;
    std::vector<Slot> slots_;
    std::size_t budget_, reserved_ = 0;
    std::atomic<std::size_t> bytes_{0}, peak_{0};
    std::atomic<std::size_t> budget_fallbacks_{0};
    static std::size_t slotBytes(const Slot &slot) {
        return slot.prepared ? slot.prepared->storageBytes() : 0;
    }
    void replaceBytes(std::size_t oldBytes, std::size_t newBytes) {
        const auto current = newBytes >= oldBytes ? bytes_.fetch_add(newBytes - oldBytes) + newBytes - oldBytes
                                                  : bytes_.fetch_sub(oldBytes - newBytes) - (oldBytes - newBytes);
        auto peak = peak_.load();
        while (current > peak && !peak_.compare_exchange_weak(peak, current)) {}
    }

  public:
    PfsmTemplateStore(std::shared_ptr<const PfsmProblem> problem,
                      std::shared_ptr<const std::vector<ProductCard>> universe,
                      std::size_t budget = 128 * 1024 * 1024,
                      std::shared_ptr<const PfsmTemplateStore> previous = {})
        : problem_(std::move(problem)), universe_(std::move(universe)), budget_(budget) {
        if (universe_->size() <= budget_ / sizeof(Slot)) {
            slots_.resize(universe_->size());
            reserved_ = slots_.capacity() * sizeof(Slot);
            if (reserved_ > budget_) {
                std::vector<Slot>().swap(slots_);
                reserved_ = 0;
            }
        }
        bytes_.store(reserved_);
        peak_.store(reserved_);
        if (previous && previous->problem_ == problem_ && previous->universe_ == universe_ &&
            previous->budget_ == budget_) {
            for (std::size_t i = 0; i < std::min(slots_.size(), previous->slots_.size()); ++i) {
                const auto &old = previous->slots_[i];
                if (old.reservation > budget_ - reserved_) continue;
                slots_[i] = old; slots_[i].statistics = {};
                reserved_ += old.reservation;
                replaceBytes(0, slotBytes(slots_[i]));
            }
        }
    }
    // Aliasing ownership keeps borrowed immutable inputs alive even if prepare()
    // replaces the private slot. Unreserved candidates use the original fallback.
    std::shared_ptr<const profile::ProfileExpansionInput> input(std::size_t index) const {
        if (index < slots_.size()) {
            const auto &slot = slots_[index];
            if (slot.prepared) return {slot.prepared, &slot.prepared->input()};
        }
        return std::make_shared<const profile::ProfileExpansionInput>(expansionInput((*universe_)[index], *problem_));
    }
    void reserve(std::span<const std::size_t> indices) {
        const auto settings = expansionSettings(*problem_);
        for (auto index : indices) {
            if (index >= slots_.size() || slots_[index].reservation)
                continue;
            const auto &card = (*universe_)[index];
            if (card.d.size() != card.intensity.size())
                fail("pfsm.reference", "Reference requires paired d/I records");
            auto size = profile::PreparedProfileTemplate::storageBoundForLineCount(settings, card.d.size(), true);
            if (size > budget_ - reserved_)
                size = profile::PreparedProfileTemplate::storageBoundForLineCount(settings, card.d.size(), false);
            if (size <= budget_ - reserved_) {
                slots_[index].reservation = size;
                reserved_ += size;
            }
        }
    }
    profile::PreparedProfileTemplateHandle prepare(std::size_t index, const profile::ProfileExpansionInput &input,
                                                   const ExecutionContext &context) {
        checkpoint(context);
        if (index >= slots_.size()) {
            ++budget_fallbacks_;
            return {};
        }
        auto &slot = slots_[index];
        if (!slot.reservation) {
            ++budget_fallbacks_;
            ++slot.statistics.misses;
            return {};
        }
        const bool matching = slot.prepared && slot.prepared->matches(input, context);
        if (matching && slot.prepared->profile()) {
            ++slot.statistics.hits;
            return slot.prepared;
        }
        ++slot.statistics.misses;
        ++slot.statistics.prepare_calls;
        if (matching)
            ++slot.statistics.expanded_hits;
        profile::ProfileWorkTimer timer(&profile::ProfileWorkStatistics::template_seconds);
        const auto begin = Clock::now();
        auto prepared = profile::PreparedProfileTemplate::prepare(input, context, slot.prepared);
        slot.statistics.prepare_seconds += elapsed(begin);
        if (!prepared) {
            if (prepared.error().code == ErrorCode::cancelled)
                throw Failure{prepared.error()};
            return {};
        }
        auto result = std::move(prepared).value();
        auto retained = result;
        if (retained->storageBytes() > slot.reservation)
            retained = result->expandedOnly();
        if (retained->storageBytes() > slot.reservation)
            retained.reset();
        const auto oldBytes = slotBytes(slot);
        const auto newBytes = retained ? retained->storageBytes() : 0;
        if (slot.prepared && (!matching || !retained))
            ++slot.statistics.evictions;
        slot.prepared = std::move(retained);
        replaceBytes(oldBytes, newBytes);
        return result;
    }
    PfsmTemplateStatistics statistics() const {
        PfsmTemplateStatistics result;
        for (const auto &slot : slots_) {
            result.prepare_calls += slot.statistics.prepare_calls;
            result.hits += slot.statistics.hits;
            result.misses += slot.statistics.misses;
            result.expanded_hits += slot.statistics.expanded_hits;
            result.evictions += slot.statistics.evictions;
            result.prepare_seconds += slot.statistics.prepare_seconds;
        }
        result.bytes = bytes_.load();
        result.peak_bytes = peak_.load();
        result.budget_fallbacks = budget_fallbacks_.load();
        return result;
    }
};
} // namespace qualpf::application::pfsm_detail
