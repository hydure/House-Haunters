###############################
# Test suite (CTest driven).   #
###############################
#
# One executable per .cpp file in this directory. Each executable links the
# core game library (so it exercises real code, not a stub) and the shared
# hh_compile_options interface target (so it inherits the same warnings the
# game itself is built with).
#
# Run:
#   cd build
#   ctest --output-on-failure
#
# Or run an individual case:
#   ./build/tests/test_random
#
# The Python tests for tools/build_atlas.py are registered too, behind a
# Python3 lookup -- they're auto-skipped if Pillow isn't installed.
###############################

file(GLOB HH_TEST_SOURCES CONFIGURE_DEPENDS "test_*.cpp")

if(NOT TARGET ${PROJECT_NAME}_core)
    message(FATAL_ERROR "tests/ requires the ${PROJECT_NAME}_core library; "
                        "ensure src/ produced sources before this point.")
endif()

set(HH_TEST_TARGETS "")

foreach(TEST_SRC ${HH_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE
        ${PROJECT_NAME}_core
        hh_compile_options
        ${SFML_LIBRARIES})
    # Make the test executable see <test_harness.hpp> without forcing a
    # tests/-prefixed include in every file.
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    # Run from the build dir so "../resources/..." paths (engine/Paths.hpp)
    # resolve to the project's resources/ folder, the same way they do for
    # the HH binary (which is also emitted at ${CMAKE_BINARY_DIR}/).
    add_test(NAME ${TEST_NAME}
             COMMAND ${TEST_NAME}
             WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    message("-- Registering test: ${TEST_NAME}")
    list(APPEND HH_TEST_TARGETS ${TEST_NAME})
endforeach()

# Optional Python tests for tools/build_atlas.py. Skipped silently when no
# python3 interpreter is on PATH.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_build_atlas_py
             COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_build_atlas.py"
             WORKING_DIRECTORY "${HouseHaunters_SOURCE_DIR}")
    message("-- Registering test: test_build_atlas_py (Python)")

    add_test(NAME test_generate_embedded_resources_py
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/test_generate_embedded_resources.py"
             WORKING_DIRECTORY "${HouseHaunters_SOURCE_DIR}")
    message("-- Registering test: test_generate_embedded_resources_py (Python)")
endif()

# ---- Run every test on every build ---------------------------------------
# Custom target wired into ALL so a plain `cmake --build .` (which is
# what the IDE / _build_tests.bat / Ninja default does) re-runs the
# whole CTest suite once the test binaries are up to date. The target
# DEPENDS on each test executable so its inputs are correct; CTest then
# discovers them by name from CTestTestfile.cmake.
#
# To temporarily disable (e.g. for a CI tier that wants build-only):
#   -DHH_RUN_TESTS_ON_BUILD=OFF
option(HH_RUN_TESTS_ON_BUILD "Run the CTest suite as part of the default build" ON)
if(HH_RUN_TESTS_ON_BUILD AND HH_TEST_TARGETS)
    add_custom_target(hh_run_tests ALL
        COMMAND ${CMAKE_CTEST_COMMAND}
                --output-on-failure
                -C $<CONFIG>
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS ${HH_TEST_TARGETS}
        COMMENT "Running CTest suite (HH_RUN_TESTS_ON_BUILD=ON)")
endif()
