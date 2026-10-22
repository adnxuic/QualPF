#pragma once
#include "qualpf/application/pfsm.h"
#include "qualpf/application/search_engine.h"
#include "pfsm_checks.h"

namespace qualpf::application {
Result<PreparedSearchResult> runPfsm(const SearchInput &, CandidateReader &, const ExecutionContext &, SearchProgress);
}
namespace qualpf::application::pfsm_detail {
std::shared_ptr<const PfsmProblem> prepareProblem(const SearchInput &, const PfsmRequest &,
                                                  const ExecutionContext &);
profile::ProfileTemplate expand(const ProductCard &, const PfsmProblem &, const ExecutionContext &);
profile::ProfileExpansionInput expansionSettings(const PfsmProblem &);
profile::ProfileExpansionInput expansionInput(const ProductCard &, const PfsmProblem &);
}
