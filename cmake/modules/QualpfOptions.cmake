# Opt-in contract harness; the product build requires Qt and C++20.
option(QUALPF_BUILD_CONTRACT_TESTS "Build model/state/transport contract tests" OFF)
option(QUALPF_BUILD_NUMERICS_TESTS "Build numerical foundation tests" OFF)
option(QUALPF_BUILD_CLASSIC_SEARCH_TESTS "Build classic Search-Match compatibility tests" OFF)
option(QUALPF_BUILD_SEARCH_INTEGRATION_TESTS "Build isolated application search integration tests" OFF)
option(QUALPF_BUILD_PRODUCT_TESTS "Build Qt product workflow probes" OFF)
if(QUALPF_BUILD_CONTRACT_TESTS OR QUALPF_BUILD_NUMERICS_TESTS OR
   QUALPF_BUILD_CLASSIC_SEARCH_TESTS OR QUALPF_BUILD_SEARCH_INTEGRATION_TESTS OR QUALPF_BUILD_PRODUCT_TESTS)
    enable_testing()
endif()
