cmake_minimum_required(VERSION 3.24)

project(
    Naturalehia
    VERSION 0.1.0
    DESCRIPTION "A source-agnostic animal and colony tracking engine"
    LANGUAGES CXX
)

option(NATURALEHIA_BUILD_CLI "Build the synthetic-data command-line application" ON)
include(GNUInstallDirs)

function(naturalehia_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
    endif()
endfunction()

add_library(naturalehia_core STATIC
    engine/src/engine.cpp
    engine/src/synthetic_source.cpp
)
add_library(Naturalehia::core ALIAS naturalehia_core)

target_compile_features(naturalehia_core PUBLIC cxx_std_20)
target_include_directories(
    naturalehia_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(naturalehia_core PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME core
    VERSION ${PROJECT_VERSION}
)
naturalehia_enable_warnings(naturalehia_core)

if(NATURALEHIA_BUILD_CLI)
    add_executable(naturalehia apps/naturalehia_cli/main.cpp)
    target_link_libraries(naturalehia PRIVATE Naturalehia::core)
    target_compile_features(naturalehia PRIVATE cxx_std_20)
    set_target_properties(naturalehia PROPERTIES CXX_EXTENSIONS OFF)
    naturalehia_enable_warnings(naturalehia)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(naturalehia_tests tests/naturalehia_tests.cpp)
    target_link_libraries(naturalehia_tests PRIVATE Naturalehia::core)
    target_compile_features(naturalehia_tests PRIVATE cxx_std_20)
    set_target_properties(naturalehia_tests PROPERTIES CXX_EXTENSIONS OFF)
    naturalehia_enable_warnings(naturalehia_tests)
    add_test(NAME naturalehia_tests COMMAND naturalehia_tests)
endif()

include(CMakePackageConfigHelpers)
install(
    TARGETS naturalehia_core
    EXPORT NaturalehiaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(NATURALEHIA_BUILD_CLI)
    install(TARGETS naturalehia RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY engine/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT NaturalehiaTargets
    FILE NaturalehiaTargets.cmake
    NAMESPACE Naturalehia::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Naturalehia
)
configure_package_config_file(
    cmake/NaturalehiaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Naturalehia
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/NaturalehiaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Naturalehia
)
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
