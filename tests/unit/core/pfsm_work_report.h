#pragma once
#include "pfsm_work.h"
#include <cstdlib>
#include <iostream>
#include <optional>

struct PfsmWorkReport {
    qualpf::profile::detail::WorkCounters counters;
    std::optional<qualpf::profile::detail::MeasureWork> measurement;
    PfsmWorkReport() {
        if (std::getenv("QUALPF_PFSM_MEASURE_WORK"))
            measurement.emplace(counters);
    }
    ~PfsmWorkReport() {
        if (!measurement)
            return;
        std::cerr << "{\"paired_samples\":" << counters.paired_samples
                  << ",\"physical_columns\":" << counters.physical_columns
                  << ",\"template_preparations\":" << counters.template_preparations
                  << ",\"chebyshev_tables\":" << counters.chebyshev_tables
                  << ",\"observation_preparations\":" << counters.observation_preparations
                  << ",\"observation_reuses\":" << counters.observation_reuses
                  << ",\"formal_columns\":" << counters.formal_columns
                  << ",\"anchor_restores\":" << counters.anchor_restores
                  << ",\"nnls_solves\":" << counters.nnls_solves << "}\n";
    }
};
