# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

project(
    NaturalehiaProteinLogic
    VERSION 0.3.0
    DESCRIPTION "Open C++ infrastructure for protein-level Boolean logic research"
    LANGUAGES CXX
)

option(
    NATURALEHIA_PROTEIN_LOGIC_BUILD_CLI
    "Build the protein logic command-line evaluator"
    ON
)
option(
    NATURALEHIA_PROTEIN_LOGIC_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors"
    OFF
)
option(
    NATURALEHIA_PROTEIN_LOGIC_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

include(GNUInstallDirs)

function(protein_logic_set_compile_policy target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(NATURALEHIA_PROTEIN_LOGIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Werror=return-type
        )
        if(NATURALEHIA_PROTEIN_LOGIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()

    if(NATURALEHIA_PROTEIN_LOGIC_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(
                ${target_name}
                PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
            )
            target_link_options(
                ${target_name}
                PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
            )
        else()
            message(FATAL_ERROR "The sanitizer preset requires GCC or Clang")
        endif()
    endif()
endfunction()

add_library(
    naturalehia_protein_logic
    STATIC
        src/equilibrium_acceptance.cpp
        src/equilibrium_model.cpp
        src/model.cpp
)
add_library(Naturalehia::ProteinLogic ALIAS naturalehia_protein_logic)

target_compile_features(naturalehia_protein_logic PUBLIC cxx_std_20)
target_include_directories(
    naturalehia_protein_logic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(
    naturalehia_protein_logic
    PROPERTIES
        CXX_EXTENSIONS OFF
        EXPORT_NAME ProteinLogic
        VERSION ${PROJECT_VERSION}
)
protein_logic_set_compile_policy(naturalehia_protein_logic)

if(NATURALEHIA_PROTEIN_LOGIC_BUILD_CLI)
    add_executable(naturalehia_protein_logic_cli apps/cli/main.cpp)
    target_link_libraries(
        naturalehia_protein_logic_cli
        PRIVATE Naturalehia::ProteinLogic
    )
    target_compile_features(naturalehia_protein_logic_cli PRIVATE cxx_std_20)
    set_target_properties(
        naturalehia_protein_logic_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-protein-logic"
    )
    protein_logic_set_compile_policy(naturalehia_protein_logic_cli)

    add_executable(
        naturalehia_protein_logic_equilibrium_cli
        apps/equilibrium_cli/main.cpp
    )
    target_link_libraries(
        naturalehia_protein_logic_equilibrium_cli
        PRIVATE Naturalehia::ProteinLogic
    )
    target_compile_features(
        naturalehia_protein_logic_equilibrium_cli
        PRIVATE cxx_std_20
    )
    set_target_properties(
        naturalehia_protein_logic_equilibrium_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-protein-logic-equilibrium"
    )
    protein_logic_set_compile_policy(
        naturalehia_protein_logic_equilibrium_cli
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(protein_logic_acceptance_tests tests/acceptance_tests.cpp)
    target_link_libraries(
        protein_logic_acceptance_tests
        PRIVATE Naturalehia::ProteinLogic
    )
    target_compile_features(protein_logic_acceptance_tests PRIVATE cxx_std_20)
    set_target_properties(
        protein_logic_acceptance_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    protein_logic_set_compile_policy(protein_logic_acceptance_tests)
    add_test(NAME protein_logic_acceptance_tests COMMAND protein_logic_acceptance_tests)

    add_executable(protein_logic_tests tests/model_tests.cpp)
    target_link_libraries(protein_logic_tests PRIVATE Naturalehia::ProteinLogic)
    target_compile_features(protein_logic_tests PRIVATE cxx_std_20)
    set_target_properties(protein_logic_tests PROPERTIES CXX_EXTENSIONS OFF)
    protein_logic_set_compile_policy(protein_logic_tests)
    add_test(NAME protein_logic_tests COMMAND protein_logic_tests)

    add_executable(
        protein_logic_equilibrium_tests
        tests/equilibrium_model_tests.cpp
    )
    target_link_libraries(
        protein_logic_equilibrium_tests
        PRIVATE Naturalehia::ProteinLogic
    )
    target_compile_features(
        protein_logic_equilibrium_tests
        PRIVATE cxx_std_20
    )
    set_target_properties(
        protein_logic_equilibrium_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    protein_logic_set_compile_policy(protein_logic_equilibrium_tests)
    add_test(
        NAME protein_logic_equilibrium_tests
        COMMAND protein_logic_equilibrium_tests
    )

    if(NATURALEHIA_PROTEIN_LOGIC_BUILD_CLI)
        add_test(
            NAME protein_logic_cli_nominal
            COMMAND naturalehia_protein_logic_cli
        )
        set_tests_properties(
            protein_logic_cli_nominal
            PROPERTIES PASS_REGULAR_EXPRESSION "endpoint_specification_match.true"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_region
            COMMAND naturalehia_protein_logic_equilibrium_cli region
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_region
            PROPERTIES PASS_REGULAR_EXPRESSION "passes_required_separation.true"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_independent_null
            COMMAND naturalehia_protein_logic_equilibrium_cli
                region --independent-null
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_independent_null
            PROPERTIES PASS_REGULAR_EXPRESSION "passes_threshold.false"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_surface
            COMMAND naturalehia_protein_logic_equilibrium_cli
                surface --a-points 2 --b-points 2
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_surface
            PROPERTIES PASS_REGULAR_EXPRESSION "surface_layout.input-a-fastest"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_ambiguous_singleton
            COMMAND naturalehia_protein_logic_equilibrium_cli
                surface --a-min 2 --a-max 10 --a-points 1
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_ambiguous_singleton
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_coupling_audit_unassessed
            COMMAND naturalehia_protein_logic_equilibrium_cli coupling-audit
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_coupling_audit_unassessed
            PROPERTIES PASS_REGULAR_EXPRESSION "acceptance_status.not_assessed"
        )
        set(coupling_audit_criteria
            coupling-audit
            --criteria-label illustrative-regression-v1
            --criteria-threshold 0.5
            --criteria-min-separation 0.3
            --criteria-max-intended-off 0.15
            --criteria-max-floor-imbalance 0.05
        )
        add_test(
            NAME protein_logic_equilibrium_cli_coupling_audit_declared_passes
            COMMAND naturalehia_protein_logic_equilibrium_cli
                ${coupling_audit_criteria}
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_coupling_audit_declared_passes
            PROPERTIES PASS_REGULAR_EXPRESSION "overall_outcome.declared.pass"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_coupling_audit_null_fails
            COMMAND naturalehia_protein_logic_equilibrium_cli
                ${coupling_audit_criteria}
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_coupling_audit_null_fails
            PROPERTIES PASS_REGULAR_EXPRESSION "overall_outcome.independent_null.fail"
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_partial_criteria
            COMMAND naturalehia_protein_logic_equilibrium_cli
                coupling-audit --criteria-label incomplete
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_partial_criteria
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_legacy_audit_threshold
            COMMAND naturalehia_protein_logic_equilibrium_cli
                coupling-audit --threshold 0.5
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_legacy_audit_threshold
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_legacy_audit_separation
            COMMAND naturalehia_protein_logic_equilibrium_cli
                coupling-audit --required-separation 0.3
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_legacy_audit_separation
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_degenerate_audit
            COMMAND naturalehia_protein_logic_equilibrium_cli
                coupling-audit --off-omega 1 --on-omega 1
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_degenerate_audit
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_formula_label
            COMMAND naturalehia_protein_logic_equilibrium_cli
                coupling-audit --criteria-label =formula
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_formula_label
            PROPERTIES WILL_FAIL TRUE
        )
        add_test(
            NAME protein_logic_equilibrium_cli_rejects_formula_unit
            COMMAND naturalehia_protein_logic_equilibrium_cli
                region --unit @formula
        )
        set_tests_properties(
            protein_logic_equilibrium_cli_rejects_formula_unit
            PROPERTIES WILL_FAIL TRUE
        )
    endif()
endif()

include(CMakePackageConfigHelpers)

install(
    TARGETS naturalehia_protein_logic
    EXPORT NaturalehiaProteinLogicTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(NATURALEHIA_PROTEIN_LOGIC_BUILD_CLI)
    install(
        TARGETS
            naturalehia_protein_logic_cli
            naturalehia_protein_logic_equilibrium_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT NaturalehiaProteinLogicTargets
    FILE NaturalehiaProteinLogicTargets.cmake
    NAMESPACE Naturalehia::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaProteinLogic
)

configure_package_config_file(
    cmake/NaturalehiaProteinLogicConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaProteinLogicConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaProteinLogic
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaProteinLogicConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaProteinLogicConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaProteinLogicConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaProteinLogic
)
install(
    FILES
        LICENSE
        README.md
        CONTRIBUTING.md
        CODE_OF_CONDUCT.md
        SECURITY.md
        THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(
    DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
    FILES_MATCHING PATTERN "*.md"
)
