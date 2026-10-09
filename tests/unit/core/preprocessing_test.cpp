#include "../../../src/core/preprocessing/legacy_reduction.h"
#include "qualpf/preprocessing/legacy.h"
#include <gtest/gtest.h>
#include <limits>

using namespace qualpf;
using namespace qualpf::preprocessing;

TEST(Preprocessing, ReferenceFusedRoundingPreservesCancellationAndMidpoints) {
    using detail::cholesky_multiply_add;
    using detail::reference_fma;
    // Actual MinGW fmaf discrepancy found in the Gaussian SVD.
    EXPECT_EQ(reference_fma(0.52082163095474243f, -1.3887096643447876f, 0.f), -0.72327005863189697f);
    // Exact results, independently expressible as powers of two.
    EXPECT_EQ(reference_fma(1.f + 0x1p-23f, 1.f - 0x1p-23f, -1.f), -0x1p-46f);
    EXPECT_EQ(reference_fma(1.f, 0x1p-24f, 1.f), 1.f);
    EXPECT_EQ(reference_fma(1.f + 0x1p-23f, 0x1p-24f, 1.f), 1.f + 0x1p-23f);
    EXPECT_EQ(reference_fma(1.f - 0x1p-24f, 0x1p-24f, 1.f), 1.f);
    EXPECT_EQ(cholesky_multiply_add(1. + 0x1p-52, 1. - 0x1p-52, -1.), -0x1p-104);
    EXPECT_EQ(cholesky_multiply_add(1., 0x1p-53, 1.), 1.);
    EXPECT_EQ(cholesky_multiply_add(1. + 0x1p-52, 0x1p-53, 1.), 1. + 0x1p-52);
    EXPECT_EQ(cholesky_multiply_add(1. - 0x1p-53, 0x1p-53, 1.), 1.);
}

TEST(Preprocessing, MovingAveragePreservesEndsAndShortSignal) {
    const std::vector<double> y{1, 2, 9, 4, 5, 6, 7};
    SmoothingOptions o;
    o.method = SmoothingMethod::averaging;
    o.half_window = 2;
    auto r = smooth(y, o);
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value().front(), 1);
    EXPECT_EQ(r.value().back(), 7);
    EXPECT_DOUBLE_EQ(r.value()[1], double(12.f / 3));
    o.half_window = 10;
    r = smooth(y, o);
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value(), y);
}
TEST(Preprocessing, SavitzkyGolayUsesLegacyDerivativeUnits) {
    std::vector<double> y;
    for (int i = 0; i < 21; ++i)
        y.push_back(i * i);
    SmoothingOptions o;
    o.half_window = 3;
    o.polynomial_degree = 2;
    o.derivative = 2;
    auto r = smooth(y, o);
    ASSERT_TRUE(r);
    EXPECT_NEAR(r.value()[10], 1,
                1e-5); // coefficient of x^2, not the physical derivative 2
    o.derivative = 0;
    r = smooth(y, o);
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value().front(), y.front());
    EXPECT_EQ(r.value().back(), y.back());
}
TEST(Preprocessing, RejectsUnsafeWindowsAndNonfiniteInputs) {
    EXPECT_FALSE(smooth(std::vector<double>{1, 2, 3}, SmoothingOptions{}));
    EXPECT_FALSE(smooth(std::vector<double>{std::numeric_limits<double>::quiet_NaN()}, SmoothingOptions{}));
    SmoothingOptions o;
    o.method = SmoothingMethod::averaging;
    o.half_window = -1;
    EXPECT_FALSE(smooth(std::vector<double>{1, 2, 3}, o));
}
TEST(Preprocessing, SubtractionRequiresExplicitStateAndDoesNotRepeat) {
    Pattern p;
    p.id = {1};
    p.observed = {5, 10, 2};
    p.background = SampledSignal{{1, 3, 4}, SampleWindow{0}};
    EXPECT_FALSE(subtract_background(p));
    p.processing.background_subtracted = false;
    p.processing.intensity_scaled = false;
    auto r = subtract_background(p);
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value().observed, (std::vector<double>{4, 7, 0}));
    EXPECT_EQ(p.observed, (std::vector<double>{5, 10, 2}));
    auto twice = subtract_background(r.value());
    ASSERT_TRUE(twice);
    EXPECT_EQ(twice.value().observed, r.value().observed);
    p.background->window.reset();
    EXPECT_FALSE(subtract_background(p));
}
TEST(Preprocessing, UndefinedZeroRescaleIsNotSuccessfulEquivalence) {
    Pattern p;
    p.id = {1};
    p.observed = {2, 2};
    p.background = SampledSignal{{2, 2}, SampleWindow{0}};
    p.processing.background_subtracted = false;
    p.processing.intensity_scaled = true;
    auto r = subtract_background(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::numerical_failure);
}
TEST(Preprocessing, ExportPreservesResidualAndGeneratesLegacyTolerance) {
    PeakSet set;
    set.dataset = {1};
    set.effective_wavelength = WavelengthAngstrom{1.54056};
    Peak p;
    p.id = {1};
    p.two_theta = TwoThetaDegrees{30};
    auto d = legacy_d(p.two_theta, *set.effective_wavelength);
    ASSERT_TRUE(d);
    p.d_spacing = d.value();
    p.intensity = 23.123456789;
    p.original_intensity = 100;
    p.fwhm = AngularWidthDegrees{.2};
    set.peaks.push_back(p);
    auto r = export_peaks(set, AngularWidthDegrees{.15});
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value().peaks[0].intensity, p.intensity);
    EXPECT_EQ(r.value().peaks[0].original_intensity, p.original_intensity);
    EXPECT_FALSE(set.peaks[0].delta_d);
    EXPECT_GT(r.value().peaks[0].delta_d->value, 0);
    auto a = automatic_tolerance(set);
    ASSERT_TRUE(a);
    EXPECT_EQ(a.value(), double(1.3f * .2f));
    auto angle = legacy_two_theta(d.value(), *set.effective_wavelength);
    ASSERT_TRUE(angle);
    EXPECT_NEAR(angle.value().value, 30, 1e-5);
    set.peaks.clear();
    EXPECT_EQ(automatic_tolerance(set).value(), .5);
}
TEST(Preprocessing, RejectsGridCollapsedByFloatRounding) {
    Pattern p;
    p.id = {1};
    p.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{10 + 1e-10}};
    p.observed = {1, 2};
    EXPECT_FALSE(background(p, BackgroundOptions{}, WavelengthAngstrom{1.54056}));
    EXPECT_FALSE(find_peaks(p, PeakOptions{}, WavelengthAngstrom{1.54056}, AngularWidthDegrees{.15}));
}
TEST(Preprocessing, EmptyExportHasNoFilePayloadAndZeroHeightDetailsAreRejected) {
    PeakSet set;
    set.dataset = {1};
    EXPECT_EQ(export_peak_text(set, PeakTextFormat::details).value(), "");
    Peak p;
    p.id = {1};
    p.two_theta = TwoThetaDegrees{.5};
    p.d_spacing = DSpacingAngstrom{4};
    p.fwhm = AngularWidthDegrees{.2};
    set.peaks.push_back(p);
    EXPECT_EQ(export_peak_text(set, PeakTextFormat::two_theta).value(), "#2theta values\n.500000\n");
    EXPECT_FALSE(export_peak_text(set, PeakTextFormat::details));
}

TEST(Preprocessing, PartialExistingBackgroundFilterPreservesOutsideAndAlignment) {
    Pattern p;
    p.id = {1};
    for (int i = 0; i < 31; ++i) {
        p.two_theta.push_back(TwoThetaDegrees{10. + i});
        p.observed.push_back(1.);
    }
    p.background = SampledSignal{std::vector<double>(31, 2.), SampleWindow{0}};
    p.background->values[15] = 100.;
    p.background->values[0] = 3.141592653589793;
    BackgroundFilter f{18, 32, 5, 3};
    auto result = filter_background(p, f);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().window->first, 0u);
    EXPECT_LT(result.value().values[15], 100.);
    for (int i = 0; i < 31; ++i)
        if (i < 8 || i > 22)
            EXPECT_EQ(result.value().values[i], p.background->values[i]);
    p.background->window.reset();
    result = filter_background(p, f);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().reason, "background_alignment_unknown");
}
TEST(Preprocessing, NewPartialBruecknerRejectsUnassignedOutput) {
    Pattern p;
    p.id = {1};
    for (int i = 0; i < 101; ++i) {
        p.two_theta.push_back(TwoThetaDegrees{10. + i * .1});
        p.observed.push_back(10. + i % 7);
    }
    BackgroundOptions o;
    o.method = BackgroundMethod::bruckner;
    o.bruckner_range = AngularRange{TwoThetaDegrees{12}, TwoThetaDegrees{18}};
    auto result = background(p, o, WavelengthAngstrom{1.54056});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::undefined_preprocessing);
    EXPECT_EQ(result.error().reason, "partial_brueckner_unassigned_output");
    o.bruckner_range = AngularRange{p.two_theta.front(), p.two_theta.back()};
    EXPECT_TRUE(background(p, o, WavelengthAngstrom{1.54056}));
}
