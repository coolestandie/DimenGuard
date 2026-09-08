cmake_minimum_required(VERSION 3.29)
project(dimenguard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    set(CMAKE_LINKER_TYPE LLD)
endif ()

include(CTest)
include(FetchContent)
option(DIMENGUARD_BUILD_PLUGIN "Build the Endstone plugin" OFF)

add_library(dimenguard_core STATIC
        src/region/region.cpp
        src/region/region_manager.cpp
)
target_include_directories(dimenguard_core PUBLIC include)
target_compile_features(dimenguard_core PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(dimenguard_core PRIVATE /W4 /WX /utf-8)
else ()
    target_compile_options(dimenguard_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

if (BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
    add_executable(dimenguard_tests tests/region_test.cpp)
    target_link_libraries(dimenguard_tests PRIVATE dimenguard_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(dimenguard_tests)
endif ()
