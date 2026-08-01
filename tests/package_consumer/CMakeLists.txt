cmake_minimum_required(VERSION 3.24)

project(NaturalehiaPackageConsumer LANGUAGES CXX)

find_package(Naturalehia 0.1 CONFIG REQUIRED)

add_executable(naturalehia_package_consumer main.cpp)
target_link_libraries(naturalehia_package_consumer PRIVATE Naturalehia::core)
target_compile_features(naturalehia_package_consumer PRIVATE cxx_std_20)

enable_testing()
add_test(NAME naturalehia_package_consumer COMMAND naturalehia_package_consumer)
