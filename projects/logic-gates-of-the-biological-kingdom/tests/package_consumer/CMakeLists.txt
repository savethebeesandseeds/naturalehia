# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

project(NaturalehiaProteinLogicPackageConsumer LANGUAGES CXX)

find_package(NaturalehiaProteinLogic REQUIRED CONFIG)

add_executable(naturalehia_protein_logic_package_consumer main.cpp)
target_link_libraries(
    naturalehia_protein_logic_package_consumer
    PRIVATE Naturalehia::ProteinLogic
)
target_compile_features(naturalehia_protein_logic_package_consumer PRIVATE cxx_std_20)

enable_testing()
add_test(
    NAME naturalehia_protein_logic_package_consumer
    COMMAND naturalehia_protein_logic_package_consumer
)
