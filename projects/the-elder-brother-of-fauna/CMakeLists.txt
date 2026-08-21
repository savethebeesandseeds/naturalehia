cmake_minimum_required(VERSION 3.24)

project(
    NaturalehiaFauna
    VERSION 0.1.0
    DESCRIPTION "The Elder Brother of Fauna animal and colony tracking engine"
    LANGUAGES CXX
)

option(NATURALEHIA_FAUNA_BUILD_CLI "Build the synthetic-data command-line application" ON)
option(NATURALEHIA_FAUNA_BUILD_BENCHMARKS "Build the dependency-free benchmark executables" OFF)
include(GNUInstallDirs)

function(fauna_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
    endif()
endfunction()

add_library(naturalehia_fauna STATIC
    engine/src/association.cpp
    engine/src/engine.cpp
    engine/src/synthetic_source.cpp
)
add_library(Naturalehia::Fauna ALIAS naturalehia_fauna)

target_compile_features(naturalehia_fauna PUBLIC cxx_std_20)
target_include_directories(
    naturalehia_fauna
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(naturalehia_fauna PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Fauna
    VERSION ${PROJECT_VERSION}
)
fauna_enable_warnings(naturalehia_fauna)

if(NATURALEHIA_FAUNA_BUILD_CLI)
    add_executable(naturalehia_fauna_cli apps/cli/main.cpp)
    target_link_libraries(naturalehia_fauna_cli PRIVATE Naturalehia::Fauna)
    target_compile_features(naturalehia_fauna_cli PRIVATE cxx_std_20)
    set_target_properties(
        naturalehia_fauna_cli
        PROPERTIES
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "naturalehia-fauna"
    )
    fauna_enable_warnings(naturalehia_fauna_cli)
endif()

if(NATURALEHIA_FAUNA_BUILD_BENCHMARKS)
    add_executable(
        fauna_association_benchmark
        benchmarks/association_benchmark.cpp
    )
    target_link_libraries(fauna_association_benchmark PRIVATE Naturalehia::Fauna)
    target_compile_features(fauna_association_benchmark PRIVATE cxx_std_20)
    set_target_properties(
        fauna_association_benchmark
        PROPERTIES CXX_EXTENSIONS OFF
    )
    fauna_enable_warnings(fauna_association_benchmark)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(fauna_tests tests/fauna_tests.cpp)
    target_link_libraries(fauna_tests PRIVATE Naturalehia::Fauna)
    target_compile_features(fauna_tests PRIVATE cxx_std_20)
    set_target_properties(fauna_tests PROPERTIES CXX_EXTENSIONS OFF)
    fauna_enable_warnings(fauna_tests)
    add_test(NAME fauna_tests COMMAND fauna_tests)

    add_executable(fauna_association_tests tests/association_tests.cpp)
    target_link_libraries(fauna_association_tests PRIVATE Naturalehia::Fauna)
    target_include_directories(
        fauna_association_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine/src
    )
    target_compile_features(fauna_association_tests PRIVATE cxx_std_20)
    set_target_properties(fauna_association_tests PROPERTIES CXX_EXTENSIONS OFF)
    fauna_enable_warnings(fauna_association_tests)
    add_test(NAME fauna_association_tests COMMAND fauna_association_tests)
endif()

include(CMakePackageConfigHelpers)
install(
    TARGETS naturalehia_fauna
    EXPORT NaturalehiaFaunaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(NATURALEHIA_FAUNA_BUILD_CLI)
    install(TARGETS naturalehia_fauna_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY engine/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT NaturalehiaFaunaTargets
    FILE NaturalehiaFaunaTargets.cmake
    NAMESPACE Naturalehia::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaFauna
)
configure_package_config_file(
    cmake/NaturalehiaFaunaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaFaunaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaFauna
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaFaunaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaFaunaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaFaunaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NaturalehiaFauna
)
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
