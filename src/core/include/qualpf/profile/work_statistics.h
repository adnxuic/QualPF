#pragma once
#include <chrono>
#include <cstddef>

namespace qualpf::profile {
// Optional thread-local diagnostics. Counts describe these explicit work sites,
// not a process-wide allocator trace. No scientific evidence depends on them.
struct ProfileWorkStatistics {
    std::size_t paired_samples = 0, physical_columns = 0, template_preparations = 0;
    std::size_t chebyshev_tables = 0, observation_preparations = 0, observation_reuses = 0;
    std::size_t formal_columns = 0, anchor_restores = 0, nnls_solves = 0;
    std::size_t input_builds = 0, input_buffer_growths = 0, sample_buffers = 0;
    double input_seconds = 0, reflection_seconds = 0, correlation_seconds = 0;
    double sampling_seconds = 0, nnls_seconds = 0, template_seconds = 0;
    ProfileWorkStatistics &operator+=(const ProfileWorkStatistics &other) {
#define QUALPF_ADD_WORK(field) field += other.field
        QUALPF_ADD_WORK(paired_samples); QUALPF_ADD_WORK(physical_columns);
        QUALPF_ADD_WORK(template_preparations); QUALPF_ADD_WORK(chebyshev_tables);
        QUALPF_ADD_WORK(observation_preparations); QUALPF_ADD_WORK(observation_reuses);
        QUALPF_ADD_WORK(formal_columns); QUALPF_ADD_WORK(anchor_restores); QUALPF_ADD_WORK(nnls_solves);
        QUALPF_ADD_WORK(input_builds); QUALPF_ADD_WORK(input_buffer_growths); QUALPF_ADD_WORK(sample_buffers);
        QUALPF_ADD_WORK(input_seconds); QUALPF_ADD_WORK(reflection_seconds); QUALPF_ADD_WORK(correlation_seconds);
        QUALPF_ADD_WORK(sampling_seconds); QUALPF_ADD_WORK(nnls_seconds); QUALPF_ADD_WORK(template_seconds);
#undef QUALPF_ADD_WORK
        return *this;
    }
};
inline thread_local ProfileWorkStatistics *active_profile_work = nullptr;
inline thread_local bool profile_work_timing = false;
class ProfileWorkMeasurement {
  public:
    explicit ProfileWorkMeasurement(ProfileWorkStatistics &work, bool timing = false)
        : previous_(active_profile_work), timing_(profile_work_timing) {
        active_profile_work = &work;
        profile_work_timing = timing;
    }
    ~ProfileWorkMeasurement() { active_profile_work = previous_; profile_work_timing = timing_; }
    ProfileWorkMeasurement(const ProfileWorkMeasurement &) = delete;
    ProfileWorkMeasurement &operator=(const ProfileWorkMeasurement &) = delete;
  private:
    ProfileWorkStatistics *previous_;
    bool timing_;
};
inline void countProfileWork(std::size_t ProfileWorkStatistics::*field) {
    if (active_profile_work) ++(active_profile_work->*field);
}
class ProfileWorkTimer {
  public:
    explicit ProfileWorkTimer(double ProfileWorkStatistics::*field)
        : work_(profile_work_timing ? active_profile_work : nullptr), field_(field) {
        if (work_) start_ = Clock::now();
    }
    ~ProfileWorkTimer() {
        if (work_) work_->*field_ += std::chrono::duration<double>(Clock::now() - start_).count();
    }
  private:
    using Clock = std::chrono::steady_clock;
    ProfileWorkStatistics *work_;
    double ProfileWorkStatistics::*field_;
    Clock::time_point start_;
};
} // namespace qualpf::profile
