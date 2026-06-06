cmake_minimum_required(VERSION 3.20)
project(werewolf CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# --- Core engine library (domain-agnostic-ish core + werewolf domain, M0) ---
add_library(werewolf_core
    src/core/roles/role.cpp
    src/core/abilities/role_abilities.cpp
    src/core/board.cpp
    src/core/game_state.cpp
    src/flow/win_condition.cpp
    src/flow/game.cpp
    src/io/console_decision_provider.cpp
)
target_include_directories(werewolf_core PUBLIC src)
target_compile_features(werewolf_core PUBLIC cxx_std_20)

# --- Command-line app (M4) ---
add_executable(werewolf src/app/main.cpp)
target_link_libraries(werewolf PRIVATE werewolf_core)

# --- Tests (GoogleTest via FetchContent, BRD §13) ---
option(WEREWOLF_BUILD_TESTS "Build tests" ON)
if(WEREWOLF_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP true
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(core_test tests/core_test.cpp)
    target_link_libraries(core_test PRIVATE werewolf_core GTest::gtest_main)

    add_executable(flow_test tests/flow_test.cpp)
    target_link_libraries(flow_test PRIVATE werewolf_core GTest::gtest_main)

    add_executable(roles_test tests/roles_test.cpp)
    target_link_libraries(roles_test PRIVATE werewolf_core GTest::gtest_main)

    add_executable(sheriff_test tests/sheriff_test.cpp)
    target_link_libraries(sheriff_test PRIVATE werewolf_core GTest::gtest_main)

    add_executable(console_test tests/console_test.cpp)
    target_link_libraries(console_test PRIVATE werewolf_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(core_test)
    gtest_discover_tests(flow_test)
    gtest_discover_tests(roles_test)
    gtest_discover_tests(sheriff_test)
    gtest_discover_tests(console_test)
endif()
