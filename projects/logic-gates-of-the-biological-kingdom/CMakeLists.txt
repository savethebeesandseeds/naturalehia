# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

project(
    NaturalehiaProteinLogic
    VERSION 0.1.0
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

add_library(naturalehia_protein_logic STATIC src/model.cpp)
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
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(protein_logic_tests tests/model_tests.cpp)
    target_link_libraries(protein_logic_tests PRIVATE Naturalehia::ProteinLogic)
    target_compile_features(protein_logic_tests PRIVATE cxx_std_20)
    set_target_properties(protein_logic_tests PROPERTIES CXX_EXTENSIONS OFF)
    protein_logic_set_compile_policy(protein_logic_tests)
    add_test(NAME protein_logic_tests COMMAND protein_logic_tests)
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
        TARGETS naturalehia_protein_logic_cli
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
    FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
