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
option(DIMENGUARD_BUILD_PLUGIN "Build the Endstone plugin" ON)

if (DIMENGUARD_BUILD_PLUGIN AND UNIX)
    # Every C++ library crossing the plugin boundary must share the server's STL ABI.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options(-stdlib=libc++)
endif ()

function(dimenguard_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif ()
endfunction()

add_library(dimenguard_core STATIC
        src/region/region.cpp
        src/region/region_manager.cpp
)
target_include_directories(dimenguard_core PUBLIC include)
target_compile_features(dimenguard_core PUBLIC cxx_std_20)
dimenguard_warnings(dimenguard_core)

FetchContent_Declare(sqlite
        URL https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip
        URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032)
FetchContent_MakeAvailable(sqlite)
add_library(dimenguard_sqlite STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
target_include_directories(dimenguard_sqlite SYSTEM PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(dimenguard_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
find_package(Threads REQUIRED)
target_link_libraries(dimenguard_sqlite PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

add_library(dimenguard_services STATIC
        src/storage/sqlite_store.cpp
        src/service/region_service.cpp
        src/command/catalog.cpp
        src/i18n/translator.cpp)
target_link_libraries(dimenguard_services PUBLIC dimenguard_core dimenguard_sqlite)
dimenguard_warnings(dimenguard_services)

if (DIMENGUARD_BUILD_PLUGIN)
    # Use the small, hash-verified release package instead of repository history.
    FetchContent_Declare(nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
            URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
    FetchContent_Declare(endstone
            GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
            GIT_TAG 46eff9f125f52eac76472d84339ead8fbf51fcd2)
    FetchContent_MakeAvailable(endstone)
    # Keep strict warnings on our code without promoting SDK implementation warnings.
    set_property(TARGET endstone PROPERTY SYSTEM TRUE)
    configure_file(include/dimenguard/version.h.in generated/dimenguard/version.h @ONLY)
    endstone_add_plugin(dimenguard
            src/plugin.cpp
            src/command/command_handler.cpp
            src/i18n/messenger.cpp
            src/listener/protection_listener.cpp)
    target_include_directories(dimenguard PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(dimenguard PRIVATE dimenguard_services)
    dimenguard_warnings(dimenguard)
endif ()

if (BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
    add_executable(dimenguard_tests tests/region_test.cpp tests/storage_test.cpp tests/service_test.cpp
            tests/presentation_test.cpp tests/selection_test.cpp tests/coordinates_test.cpp tests/command_test.cpp)
    target_link_libraries(dimenguard_tests PRIVATE dimenguard_services GTest::gtest_main)
    dimenguard_warnings(dimenguard_tests)
    include(GoogleTest)
    gtest_discover_tests(dimenguard_tests)
    if (DIMENGUARD_BUILD_PLUGIN)
        add_executable(dimenguard_plugin_tests tests/plugin_test.cpp)
        target_link_libraries(dimenguard_plugin_tests PRIVATE dimenguard dimenguard_services endstone::endstone GTest::gtest_main)
        dimenguard_warnings(dimenguard_plugin_tests)
        gtest_discover_tests(dimenguard_plugin_tests)
    endif ()
endif ()
