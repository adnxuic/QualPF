#include "numerical_regression.h"

#include <gtest/gtest.h>

#include <limits>

using namespace qualpf;
using namespace qualpf::testing;

namespace {
NumericalRegressionCase synthetic_case() {
    NumericalRegressionCase reference;
    reference.id = "synthetic-comparator-contract-only";
    reference.evidence = EvidenceState::reviewed;
    reference.environment = {"synthetic",
                             "synthetic",
                             "none",
                             "synthetic",
                             "binary64",
                             "serial",
                             {{"synthetic-input", std::string(64, '0'), "test-only synthetic metadata"}}};
    reference.expected = NumericalObservation{{{"solution", {0, 2}}}, {1, 0}, 2, std::nullopt};
    reference.tolerances.emplace("solution",
                                 FieldTolerance{1e-12, 1e-10, "dimensionless", "synthetic contract tolerance"});
    return reference;
}
} // namespace

TEST(NumericalRegression, MissingReferenceCannotPassAsAnEmptyOrZeroResult) {
    NumericalRegressionCase reference;
    EXPECT_EQ(compare_numerical_reference(reference, {}).error().code, ErrorCode::missing_data);
    reference.expected = NumericalObservation{};
    EXPECT_FALSE(compare_numerical_reference(reference, {}));
    reference.evidence = EvidenceState::reviewed;
    EXPECT_FALSE(compare_numerical_reference(reference, {}));
}
TEST(NumericalRegression, ChecksMixedToleranceAndExactDiscreteOutputs) {
    auto reference = synthetic_case();
    auto actual = *reference.expected;
    actual.fields.at("solution")[0] = 0.5e-12;
    actual.fields.at("solution")[1] += 1e-10;
    EXPECT_TRUE(compare_numerical_reference(reference, actual));
    actual.indices = {0, 1};
    EXPECT_FALSE(compare_numerical_reference(reference, actual));
    actual = *reference.expected;
    actual.rank = 1;
    EXPECT_FALSE(compare_numerical_reference(reference, actual));
    actual = *reference.expected;
    actual.fields.at("solution")[0] = 2e-12;
    EXPECT_FALSE(compare_numerical_reference(reference, actual));
}
TEST(NumericalRegression, RejectsNonfiniteMissingFieldsAndUnfrozenTolerance) {
    auto reference = synthetic_case();
    auto actual = *reference.expected;
    actual.fields.at("solution")[0] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(compare_numerical_reference(reference, actual));
    actual = *reference.expected;
    actual.fields.at("solution").pop_back();
    EXPECT_FALSE(compare_numerical_reference(reference, actual));
    reference.tolerances.clear();
    EXPECT_FALSE(compare_numerical_reference(reference, *reference.expected));
}
