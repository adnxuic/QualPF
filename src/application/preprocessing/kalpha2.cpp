#include "qualpf/application/kalpha2.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/preprocessing/kalpha2.h"
#include <limits>

namespace qualpf::application {
Kalpha2Backend selectKalpha2Backend(std::string_view dedicated, std::string_view preprocessing) {
    if (dedicated == "cpp") return Kalpha2Backend::cpp;
    if (dedicated == "legacy") return Kalpha2Backend::legacy;
    if (!dedicated.empty()) return Kalpha2Backend::invalid;
    return preprocessing == "legacy" ? Kalpha2Backend::legacy : Kalpha2Backend::cpp;
}
Result<std::vector<double>> stripKalpha2Cpp(const Pattern &pattern) { return preprocessing::strip_kalpha2(pattern); }
Result<std::vector<double>> stripKalpha2Compatible(const Pattern &pattern) {
    auto valid = preprocessing::validate_kalpha2(pattern);
    if (!valid) return Result<std::vector<double>>{valid.error()};
    if (pattern.observed.size() > std::size_t(std::numeric_limits<int>::max()))
        return Result<std::vector<double>>{Error{ErrorCode::invalid_input, "pattern", "Native size limit", "kalpha2_invalid_grid"}};
    auto result = preprocessing::strip_kalpha2(pattern);
    if (!result && result.error().reason == "kalpha2_nonfinite_result")
        return Result<std::vector<double>>{Error{ErrorCode::numerical_failure, "pattern", "Native stripping produced a nonfinite result", "kalpha2_nonfinite_result"}};
    return result;
}
Status stripKalpha2Operation(ProductSearchSession &session, const EnvironmentReader &environment) {
    const auto backend = selectKalpha2Backend(environment.environmentValue("QUALPF_KALPHA2_BACKEND"),
                                              environment.environmentValue("QUALPF_PREPROCESSING_BACKEND"));
    if (backend == Kalpha2Backend::invalid)
        return Status{Error{ErrorCode::invalid_input, "kalpha2.backend", "Unknown K-alpha2 backend. Choose cpp or legacy.", "invalid_kalpha2_backend"}};
    ProductPreprocessingEdit edit(session, false, backend == Kalpha2Backend::legacy ? stripKalpha2Compatible : stripKalpha2Cpp);
    PreprocessingRequest request;
    request.strip_kalpha2 = true;
    auto status = edit.preview(request);
    return status ? edit.commit() : status;
}
} // namespace qualpf::application
