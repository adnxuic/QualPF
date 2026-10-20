# Eigen is an implementation detail; configuration never downloads dependencies.
find_package(Eigen3 3.4 REQUIRED NO_MODULE)
find_path(QUALPF_CORE_BOOST_INCLUDE_DIR boost/multiprecision/cpp_int.hpp
    HINTS ${Boost_INCLUDE_DIRS} "${QUALPF_POWDER_BOOST_INCLUDE_DIR}" "${BOOST_ROOT}" REQUIRED)
add_subdirectory(export)
add_library(qualpf_core STATIC
    crystallography/chemistry.cpp
    crystallography/composition.cpp
    crystallography/theoretical_card.cpp
    include/qualpf/crystallography/chemistry.h
    include/qualpf/crystallography/composition.h
    include/qualpf/crystallography/theoretical_card.h
    crystallography/powder_intensity.cpp
    include/qualpf/crystallography/powder_intensity.h
    crystallography/scattering.cpp
    include/qualpf/crystallography/scattering.h
    crystallography/structure_factor.cpp
    crystallography/phase_sincos.cpp
    crystallography/phase_sincos.h
    crystallography/rounded_transcendentals.cpp
    crystallography/rounded_transcendentals.h
    include/qualpf/crystallography/structure_factor.h
    crystallography/reflection.cpp
    include/qualpf/crystallography/reflection.h
    crystallography/symmetry.cpp
    include/qualpf/crystallography/symmetry.h
    crystallography/unit_cell.cpp
    include/qualpf/crystallography/unit_cell.h
    io/pattern_reader.cpp
    include/qualpf/io/pattern_reader.h
    domain/validation.cpp
    include/qualpf/domain/result.h
    include/qualpf/domain/units.h
    include/qualpf/domain/radiation.h
    include/qualpf/domain/analysis_spectrum.h
    preprocessing/analysis_signal.h
    include/qualpf/domain/pattern.h
    include/qualpf/domain/peaks.h
    include/qualpf/domain/reference_card.h
    include/qualpf/domain/search.h
    include/qualpf/domain/validation.h
    include/qualpf/migration/backends.h
    numerics/basic.cpp
    numerics/interpolation.cpp
    numerics/linear_algebra.cpp
    numerics/polynomial.cpp
    include/qualpf/numerics/basic.h
    include/qualpf/numerics/interpolation.h
    include/qualpf/numerics/linear_algebra.h
    include/qualpf/numerics/polynomial.h
    search/classic_score.cpp
    search/classic_candidates.cpp
    search/classic_residual.cpp
    search/rir.cpp
    include/qualpf/algorithms/identity.h
    include/qualpf/search/algorithms.h
    include/qualpf/search/rir.h
    include/qualpf/search/classic.h
    preprocessing/algorithms.cpp
    include/qualpf/preprocessing/algorithms.h
    include/qualpf/preprocessing/legacy.h
    preprocessing/legacy_internal.h
    preprocessing/legacy_reduction.h
    preprocessing/legacy_smoothing.cpp
    preprocessing/average_compatibility.cpp
    preprocessing/average_compatibility.h
    preprocessing/legacy_background.cpp
    preprocessing/kalpha2.cpp
    include/qualpf/preprocessing/kalpha2.h
    peaks/legacy_peaks.cpp
    peaks/geometric_fwhm.cpp
    peaks/geometric_fwhm_noise.cpp
    peaks/geometric_fwhm_noise.h
    include/qualpf/preprocessing/geometric_fwhm.h
    peaks/legacy_polynomial.cpp
    peaks/legacy_polynomial.h
    peaks/legacy_export.cpp
    profile/pfsm_candidate_recall_selection.cpp
    profile/pfsm_candidate_recall_refinement.cpp
    profile/pfsm_candidate_recall_correlation.cpp
    profile/pfsm_expansion.cpp
    profile/pfsm_simd.cpp
    profile/pfsm_nnls.cpp
    profile/pfsm_fitting.cpp
    profile/pfsm_trf.cpp
    numerics/runtime.cpp
    include/qualpf/numerics/runtime.h
)
add_library(qualpf::core ALIAS qualpf_core)
option(QUALPF_PFSM_AVX2 "Build independently dispatched PFSM AVX2 kernels" ON)
if(QUALPF_PFSM_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
    target_sources(qualpf_core PRIVATE profile/pfsm_avx2.cpp)
    set_source_files_properties(profile/pfsm_simd.cpp PROPERTIES COMPILE_DEFINITIONS QUALPF_PFSM_AVX2)
    if(MSVC)
        set_source_files_properties(profile/pfsm_avx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2;/fp:strict")
    else()
        set_source_files_properties(profile/pfsm_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mno-fma;-fno-fast-math;-ffp-contract=off")
    endif()
endif()
target_compile_features(qualpf_core PUBLIC cxx_std_20)
if(MSVC)
    set_source_files_properties(profile/pfsm_candidate_recall_selection.cpp profile/pfsm_candidate_recall_refinement.cpp profile/pfsm_candidate_recall_correlation.cpp profile/pfsm_expansion.cpp profile/pfsm_nnls.cpp profile/pfsm_fitting.cpp profile/pfsm_trf.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(profile/pfsm_candidate_recall_selection.cpp profile/pfsm_candidate_recall_refinement.cpp profile/pfsm_candidate_recall_correlation.cpp profile/pfsm_expansion.cpp profile/pfsm_nnls.cpp profile/pfsm_fitting.cpp profile/pfsm_trf.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
if(MSVC)
    set_source_files_properties(peaks/geometric_fwhm.cpp peaks/geometric_fwhm_noise.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(peaks/geometric_fwhm.cpp peaks/geometric_fwhm_noise.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
target_include_directories(qualpf_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(qualpf_core PRIVATE Eigen3::Eigen)
# No implicit Eigen/OpenMP worker pool, BLAS/MKL backend or new SIMD path in S4.
target_compile_definitions(qualpf_core PRIVATE EIGEN_DONT_PARALLELIZE EIGEN_DONT_VECTORIZE)
set_target_properties(qualpf_core PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
set_source_files_properties(crystallography/phase_sincos.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${QUALPF_CORE_BOOST_INCLUDE_DIR}")
set_property(SOURCE crystallography/rounded_transcendentals.cpp PROPERTY
    INCLUDE_DIRECTORIES "${QUALPF_CORE_BOOST_INCLUDE_DIR}")
# The source-derived mixed-precision contract requires separate operations.
# Do not allow FMA contraction or fast-math reassociation in the compatibility scorer.
if(MSVC)
    set_source_files_properties(crystallography/chemistry.cpp crystallography/composition.cpp crystallography/theoretical_card.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(crystallography/powder_intensity.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(crystallography/scattering.cpp crystallography/structure_factor.cpp crystallography/phase_sincos.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(crystallography/reflection.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(crystallography/symmetry.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(crystallography/unit_cell.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(preprocessing/kalpha2.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
    set_source_files_properties(io/pattern_reader.cpp search/classic_score.cpp preprocessing/legacy_smoothing.cpp preprocessing/legacy_background.cpp peaks/legacy_peaks.cpp peaks/legacy_polynomial.cpp peaks/legacy_export.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(crystallography/chemistry.cpp crystallography/composition.cpp crystallography/theoretical_card.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    set_source_files_properties(crystallography/powder_intensity.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    set_source_files_properties(crystallography/scattering.cpp crystallography/structure_factor.cpp crystallography/phase_sincos.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    set_source_files_properties(crystallography/reflection.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    set_source_files_properties(crystallography/symmetry.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    set_source_files_properties(crystallography/unit_cell.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
    set_source_files_properties(preprocessing/kalpha2.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
    set_source_files_properties(io/pattern_reader.cpp search/classic_score.cpp preprocessing/legacy_smoothing.cpp preprocessing/legacy_background.cpp peaks/legacy_peaks.cpp peaks/legacy_polynomial.cpp peaks/legacy_export.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
if(MSVC)
    set_source_files_properties(preprocessing/average_compatibility.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(preprocessing/average_compatibility.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()

# RIR retains the strict floating mode of its former Application producer.
if(MSVC)
    set_source_files_properties(search/rir.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(search/rir.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()

add_subdirectory(peaks/measurement)

# Pure parsing and domain conversion are Core modules.
add_subdirectory(io/cif)
add_subdirectory(io/powder_cif)

# Plot coordinate expressions keep the original GUI default floating-point mode.
add_library(qualpf_plot_coordinates STATIC plotting/display_coordinates.cpp)
target_compile_features(qualpf_plot_coordinates PUBLIC cxx_std_20)
target_include_directories(qualpf_plot_coordinates PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(qualpf_plot_coordinates PRIVATE qualpf_powder_cif)
set_target_properties(qualpf_plot_coordinates PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
