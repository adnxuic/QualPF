#pragma once
#include "qualpf/profile/work_statistics.h"

namespace qualpf::profile::detail {
using WorkCounters = ProfileWorkStatistics;
inline void countWork(std::size_t WorkCounters::*field) {
    countProfileWork(field);
}
using MeasureWork = ProfileWorkMeasurement;
} // namespace qualpf::profile::detail
