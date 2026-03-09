cmake_minimum_required(VERSION 3.18)
project(tetrad_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings (platform-specific)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4 /EHsc)
endif()

# Dependencies via FetchContent
include(FetchContent)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
)

if(NOT SKBUILD)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(eigen Catch2)
else()
    FetchContent_MakeAvailable(eigen)
endif()

# Core library
add_library(tetrad_cpp STATIC
    src/graph/node.cpp
    src/graph/edge.cpp
    src/graph/graph.cpp
    src/data/knowledge.cpp
    src/data/data_set.cpp
    src/search/ind_test_fisher_z.cpp
    src/search/sepset_map.cpp
    src/search/fas.cpp
    src/search/meek_rules.cpp
    src/search/pc.cpp
    src/search/sem_bic_score.cpp
    src/search/fges.cpp
    src/search/fci_orient.cpp
    src/search/gfci.cpp
    src/search/grow_shrink_tree.cpp
    src/search/bes_permutation.cpp
    src/search/boss.cpp
    src/search/permutation_search.cpp
    src/search/star_fci.cpp
    src/search/boss_fci.cpp
    src/search/teyssier_scorer.cpp
    src/search/grasp.cpp
    src/search/grasp_fci.cpp
    src/util/choice_generator.cpp
    src/util/log_stream.cpp
)

target_include_directories(tetrad_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tetrad_cpp PUBLIC Eigen3::Eigen)
set_target_properties(tetrad_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Python bindings (only when building via pip install)
if(SKBUILD)
    find_package(Python 3.9
        REQUIRED COMPONENTS Interpreter Development.Module
        OPTIONAL_COMPONENTS Development.SABIModule
    )

    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR
    )
    list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
    find_package(nanobind CONFIG REQUIRED)

    nanobind_add_module(
        _tetrad_cpp
        STABLE_ABI
        NB_STATIC
        bindings/tetrad_bindings.cpp
    )

    target_link_libraries(_tetrad_cpp PRIVATE tetrad_cpp)
    target_include_directories(_tetrad_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    install(TARGETS _tetrad_cpp LIBRARY DESTINATION tetrad_port)
endif()

# Tests and example (standalone build only)
if(NOT SKBUILD)
    enable_testing()
    add_executable(tetrad_tests
        tests/test_node_edge.cpp
        tests/test_graph.cpp
        tests/test_choice_generator.cpp
        tests/test_fisher_z.cpp
        tests/test_fas.cpp
        tests/test_meek_rules.cpp
        tests/test_pc.cpp
        tests/test_knowledge.cpp
        tests/test_sem_bic_score.cpp
        tests/test_fges.cpp
        tests/test_gfci.cpp
        tests/test_boss.cpp
        tests/test_boss_fci.cpp
        tests/test_grasp.cpp
        tests/test_grasp_fci.cpp
    )
    target_link_libraries(tetrad_tests PRIVATE tetrad_cpp Catch2::Catch2WithMain)
    add_test(NAME tetrad_tests COMMAND tetrad_tests)

    add_executable(run_pc examples/run_pc.cpp)
    target_link_libraries(run_pc PRIVATE tetrad_cpp)
endif()
