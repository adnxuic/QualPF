include_guard(GLOBAL)

# Persistence identities are compatibility keys, not distribution publishers.
set(QUALPF_DISTRIBUTION_MANIFEST "${PROJECT_SOURCE_DIR}/deploy/distribution.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUALPF_DISTRIBUTION_MANIFEST}")
file(READ "${QUALPF_DISTRIBUTION_MANIFEST}" QUALPF_DISTRIBUTION_JSON)
foreach(key publisher license homepage support documentation description copyright)
    string(TOUPPER "${key}" upper_key)
    string(JSON QUALPF_${upper_key} GET "${QUALPF_DISTRIBUTION_JSON}" product ${key})
endforeach()
set(QUALPF_RELEASE_CONTACT_EMAIL "" CACHE STRING "Real public contact email required for DEB packaging")
option(QUALPF_DISTRIBUTION_AUDIT_ONLY "Build explicitly marked local audit packages without rights clearance" OFF)
