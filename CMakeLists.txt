###########################
# CMake Build Environment #
###########################
#
# You can set the following environment variables:
#
# CMAKE_MODULE_PATH to the search path for cmake modules (e.g., FindSFML.cmake)
# CMAKE_BUILD_TYPE  to override the default build type. Valid options are
#                   'release', 'profile' and 'debug'.
# SFML_ROOT         to point at an SFML install when it isn't on the system
#                   default search path (e.g. on Windows).
#
#########################################

cmake_minimum_required(VERSION 3.16)

# Project identifier (no spaces -- CMake derives variables like
# ${HouseHaunters_SOURCE_DIR} and target names from this).
project(HouseHaunters CXX)

# MSVC debug-info layout: embed in .obj (/Z7) instead of writing a shared
# program database (/Zi). The shared PDB path can't always be opened by
# parallel cl.exe processes (MSBuild defaults /MP on, so even
# `cmake --build --parallel 1` can fail with C1041 when the source path
# contains spaces). /Z7 sidesteps the contention entirely. Done by
# rewriting the default CXX flag strings so it works on every CMake
# version, regardless of the CMP0141 policy state.
if(MSVC)
    foreach(_cfg "" _DEBUG _RELEASE _MINSIZEREL _RELWITHDEBINFO)
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS${_cfg} "${CMAKE_CXX_FLAGS${_cfg}}")
        string(REPLACE "/ZI" "/Z7" CMAKE_CXX_FLAGS${_cfg} "${CMAKE_CXX_FLAGS${_cfg}}")
    endforeach()
endif()

#####################
# Setup Environment #
#####################
# Include the directory holding custom CMake modules (e.g. FindSFML.cmake).
list(APPEND CMAKE_MODULE_PATH "$ENV{CMAKE_MODULE_PATH}" "${HouseHaunters_SOURCE_DIR}/cmake")

# Pick a build type if one was specified through the environment.
if((NOT CMAKE_BUILD_TYPE) AND (NOT "$ENV{CMAKE_BUILD_TYPE}" STREQUAL ""))
    set(CMAKE_BUILD_TYPE "$ENV{CMAKE_BUILD_TYPE}" CACHE STRING "" FORCE)
endif()

# Determine build type:
# 1) honor an explicit user/environment selection,
# 2) fall back to Debug for in-source builds,
# 3) otherwise infer it from the build directory name.
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" TYPE)
    message("-- Using user specified build type: ${TYPE}")
elseif(${HouseHaunters_SOURCE_DIR} STREQUAL ${HouseHaunters_BINARY_DIR})
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    message("-- In-source building. Build type set to: Debug")
else()
    get_filename_component(TYPE ${HouseHaunters_BINARY_DIR} NAME)
    string(TOUPPER "${TYPE}" TYPE)
    if(${TYPE} STREQUAL "RELEASE")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    elseif(${TYPE} STREQUAL "PROFILE")
        set(CMAKE_BUILD_TYPE Profile CACHE STRING "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    endif()
    message("-- Out-source building. Build type set to: ${TYPE}")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

# Collect sources. CONFIGURE_DEPENDS makes CMake re-glob when files appear
# or disappear so we don't have to remember to re-run cmake by hand.
file(GLOB_RECURSE SRC      CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB         EXECLIST CONFIGURE_DEPENDS "bin/*.cpp")

#################################################
#                                               #
# ADD CUSTOM LIBRARIES HERE.                    #
#                                               #
# Example BOOST:                                #
#  find_package(Boost REQUIRED)                 #
#################################################
#find_package(Boost REQUIRED)

#############
# Find SFML #
#############
# Two modes:
#  HH_STATIC_SFML=OFF (default) -- the bundled cmake/FindSFML.cmake module
#    exposes SFML_INCLUDE_DIR and SFML_LIBRARIES (dynamic .lib stubs). The
#    resulting HH.exe needs all four SFML DLLs at runtime.
#  HH_STATIC_SFML=ON  -- use the modern SFMLConfig.cmake (shipped with the
#    vendored SFML 2.6.2 under lib/cmake/SFML) and link the *static*
#    imported targets. The resulting HH.exe has no SFML DLL dependencies
#    AND no openal32.dll (because audio routes through miniaudio, not
#    SFML/Audio). Used to produce the single-file redistributable.
#
# NOTE: SFML's 'audio' component is intentionally NOT requested -- audio
# is now provided by hh::AudioPlayer (vendored miniaudio at
# vendor/miniaudio/miniaudio.h). Pulling sfml-audio back in would drag
# OpenAL Soft (LGPL) and force us to ship openal32.dll alongside the exe.
option(HH_STATIC_SFML "Statically link SFML into HH.exe (for redistribution)" OFF)

# The in-tree SFML package contains Windows binaries. On macOS and Linux,
# build the same pinned SFML 2.6.2 tag from source so a clean checkout does
# not require a separately installed SFML package. Callers can disable this
# and supply SFML_ROOT/SFML_DIR when building fully offline.
if(WIN32)
    set(_fetch_sfml_default OFF)
else()
    set(_fetch_sfml_default ON)
endif()
option(HH_FETCH_SFML "Fetch and build pinned SFML 2.6.2 from source" ${_fetch_sfml_default})

#####################################
# Embed resources/ into the binary  #
#####################################
# When ON, the entire resources/ directory is baked into the executable.
# Windows uses an RT_RCDATA .rc script; macOS and Linux use generated C++
# byte arrays. ResourceFS transparently serves bytes from disk (OFF) or the
# executable (ON). Combined with HH_STATIC_SFML this yields a native package
# with no separate resources/ directory or SFML shared libraries.
#
# Defaults ON whenever HH_STATIC_SFML is ON (the only reason to enable
# static SFML is to ship a single .exe; embedding resources is the other
# half of that goal). The two flags are otherwise independent -- if you
# want static SFML but on-disk resources (e.g. so you can iterate on
# assets without rebuilding) just pass
#     -DHH_STATIC_SFML=ON -DHH_EMBED_RESOURCES=OFF
# explicitly; the option() call below honors the cache override.
if(HH_STATIC_SFML)
    set(_embed_default ON)
else()
    set(_embed_default OFF)
endif()
option(HH_EMBED_RESOURCES "Bake resources/ into the native executable" ${_embed_default})

# Embedding requires running the Python generator at configure-and-build
# time. We prefer the vendored interpreter under vendor/python so the
# build doesn't depend on a system Python install (the repo already
# bundles one for test_build_atlas.py).
if(HH_EMBED_RESOURCES)
    set(_vendored_python "${HouseHaunters_SOURCE_DIR}/vendor/python/python.exe")
    if(EXISTS "${_vendored_python}")
        set(HH_PYTHON "${_vendored_python}")
    else()
        find_program(HH_PYTHON NAMES python3 python)
        if(NOT HH_PYTHON)
            message(FATAL_ERROR
                "HH_EMBED_RESOURCES needs Python and the vendored "
                "vendor/python/python.exe was not found. Either restore it "
                "or install Python and re-run cmake.")
        endif()
    endif()

    set(_gen_dir       "${HouseHaunters_BINARY_DIR}/generated")
    set(_gen_ids       "${_gen_dir}/embedded_resources_ids.h")
    set(_gen_rc        "${_gen_dir}/embedded_resources.rc")
    set(_gen_table     "${_gen_dir}/embedded_resources_table.cpp")
    set(_gen_portable  "${_gen_dir}/embedded_resources_portable.cpp")
    set(_gen_script    "${HouseHaunters_SOURCE_DIR}/tools/generate_embedded_resources.py")
    set(_resources_dir "${HouseHaunters_SOURCE_DIR}/resources")

    # Rebuild the generated files whenever the script changes or any
    # resource file is added / removed / modified. CONFIGURE_DEPENDS on
    # a separate glob re-runs cmake when the filesystem changes.
    file(GLOB_RECURSE _resource_files CONFIGURE_DEPENDS
        "${_resources_dir}/*")

    if(WIN32)
        set(_gen_outputs "${_gen_ids}" "${_gen_rc}" "${_gen_table}")
        set(_gen_mode windows)
    else()
        set(_gen_outputs "${_gen_portable}")
        set(_gen_mode portable)
    endif()

    add_custom_command(
        OUTPUT  ${_gen_outputs}
        COMMAND "${HH_PYTHON}" "${_gen_script}" "${_resources_dir}" "${_gen_dir}" "${_gen_mode}"
        DEPENDS "${_gen_script}" ${_resource_files}
        COMMENT "Generating ${_gen_mode} embedded resources (${_gen_dir})"
        VERBATIM)

    # Custom target so the generation hooks into the dependency DAG
    # cleanly -- ninja schedules it before anything that needs the
    # generated headers / sources.
    add_custom_target(hh_generate_embedded_resources
        DEPENDS ${_gen_outputs})

    # Mark the generated files as actual source files so CMake doesn't
    # complain "file not found" while collecting target sources.
    set_source_files_properties(${_gen_outputs}
        PROPERTIES GENERATED TRUE)
endif()

# When building for redistribution, we'd ideally also statically link the
# Microsoft C/C++ runtime (/MT) so HH.exe doesn't depend on MSVCP140.dll
# or VCRUNTIME140.dll. We can't, because the vendored SFML 2.6.2 static
# libraries under vendor/sfml-extract/SFML-2.6.2/lib were built with /MD.
# Linking them against /MT code raises a flood of LNK2038 RuntimeLibrary-
# mismatch errors plus LNK2005 stdlib duplicate-symbol errors. To switch
# to /MT we would need to rebuild SFML from source with
# -DSFML_USE_STATIC_STD_LIBS=ON and replace the vendored libs.
#
# Decision: ship with /MD. The VC++ 2015-2022 x64 Redistributable is
# present on essentially every modern Windows install (Win10 1909+ has it
# via Windows Update); README-PLAYERS.txt links the installer for the
# rare machine that needs it.

if(HH_FETCH_SFML)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "SFML audio is replaced by miniaudio" FORCE)
    set(SFML_BUILD_NETWORK ON CACHE BOOL "Build SFML network" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "Do not build SFML examples" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "Do not build SFML documentation" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "Do not build SFML tests" FORCE)
    FetchContent_Declare(sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(sfml)
    set(SFML_FOUND TRUE)
    set(SFML_LIBRARIES sfml-graphics sfml-window sfml-system sfml-network)
    set(SFML_INCLUDE_DIR "")
elseif(HH_STATIC_SFML)
    set(SFML_STATIC_LIBRARIES TRUE)
    # Point find_package(... CONFIG) at the vendored SFMLConfig.cmake.
    # Honor an existing SFML_DIR if the caller supplied one (e.g. a
    # system install); otherwise fall back to the vendored copy that
    # ships with this repo.
    if(NOT DEFINED SFML_DIR OR "${SFML_DIR}" STREQUAL "")
        if(DEFINED ENV{SFML_ROOT} AND NOT "$ENV{SFML_ROOT}" STREQUAL "")
            set(SFML_DIR "$ENV{SFML_ROOT}/lib/cmake/SFML" CACHE PATH "" FORCE)
        else()
            set(SFML_DIR "${HouseHaunters_SOURCE_DIR}/vendor/sfml-extract/SFML-2.6.2/lib/cmake/SFML"
                CACHE PATH "" FORCE)
        endif()
    endif()
    find_package(SFML 2 COMPONENTS graphics window system network CONFIG REQUIRED)
    # The modern config exposes namespaced imported targets. Surface them
    # under the same SFML_LIBRARIES / SFML_INCLUDE_DIR variables the rest
    # of this file already consumes so neither path below has to fork.
    set(SFML_LIBRARIES sfml-graphics sfml-window sfml-system sfml-network)
    # Imported targets carry INTERFACE_INCLUDE_DIRECTORIES, so no explicit
    # SFML_INCLUDE_DIR is strictly needed -- but we set one for parity.
    set(SFML_INCLUDE_DIR "${HouseHaunters_SOURCE_DIR}/vendor/sfml-extract/SFML-2.6.2/include")
else()
    # The bundled cmake/FindSFML.cmake module exposes SFML_INCLUDE_DIR and
    # SFML_LIBRARIES rather than modern imported targets, so we keep the
    # variable-based interface but apply it per-target below instead of via
    # the deprecated global include_directories / link_libraries calls.
    find_package(SFML 2 COMPONENTS graphics window system network)
endif()

if(NOT SFML_FOUND)
    message(FATAL_ERROR
        " * Make sure LIB includes the directory where the SFML libraries are installed.\n"
        " * Make sure INCLUDE includes the directory where the SFML header files are installed.\n"
        " * On Windows, set SFML_ROOT to the SFML install directory.")
endif()

##################################
# Shared compile options/warnings
##################################
# Single INTERFACE target that every executable and the core library links
# against. Adding a flag here once propagates to every consumer.
add_library(hh_compile_options INTERFACE)
target_compile_features(hh_compile_options INTERFACE cxx_std_14)
target_include_directories(hh_compile_options INTERFACE
    "${HouseHaunters_SOURCE_DIR}/include"
    "${HouseHaunters_SOURCE_DIR}/vendor/miniaudio"
    ${SFML_INCLUDE_DIR})
# Surface latent bugs early. Per-compiler so the build stays portable.
# /FS on MSVC: serialize PDB writes when multiple cl.exe processes share
# a single program database (required for parallel builds, otherwise the
# compiler raises C1041).
target_compile_options(hh_compile_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/FS>)

# Propagate HH_EMBED_RESOURCES so #if defined() guards in ResourceFS.cpp
# and EmbeddedResources_Win32.cpp flip on at compile time. Also expose
# the generated/ include dir so EmbeddedResources_Win32.cpp can pull in
# embedded_resources_ids.h indirectly via embedded_resources_table.cpp.
if(HH_EMBED_RESOURCES)
    target_compile_definitions(hh_compile_options INTERFACE HH_EMBED_RESOURCES)
    target_include_directories(hh_compile_options INTERFACE "${_gen_dir}")
endif()
# Profile build = Debug binary + a GCC-style sampling profiler hook. MSVC
# has no -pg equivalent; the option is only injected for compilers that
# understand it.
if(${CMAKE_BUILD_TYPE} STREQUAL "PROFILE")
    target_compile_options(hh_compile_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-pg>)
    target_link_options(hh_compile_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-pg>)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()

#######################
# Set Compile Targets #
#######################
# Core library: every .cpp under src/ goes here. Library target name is
# derived from PROJECT_NAME so it stays stable even if the checkout folder
# is renamed (or contains characters CMake doesn't accept in target names,
# such as the space in "House Haunters").
if(SRC)
    set(LIBNAME "${PROJECT_NAME}_core")
    # Fold the generated backend into the core library. Windows uses a
    # path->resource-ID table paired with EmbeddedResources_Win32.cpp;
    # macOS and Linux use one generated portable byte-array backend.
    if(HH_EMBED_RESOURCES)
        if(WIN32)
            list(APPEND SRC "${_gen_table}")
        else()
            list(APPEND SRC "${_gen_portable}")
        endif()
    endif()
    add_library(${LIBNAME} STATIC ${SRC})
    target_link_libraries(${LIBNAME} PUBLIC hh_compile_options ${SFML_LIBRARIES})
    # miniaudio is compiled into AudioPlayer.cpp and needs these native
    # threading, dynamic-loader, math, and Core Audio link dependencies.
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(${LIBNAME} PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
        if(APPLE)
            target_link_libraries(${LIBNAME} PUBLIC
                "-framework CoreFoundation"
                "-framework CoreAudio"
                "-framework AudioToolbox")
        else()
            target_link_libraries(${LIBNAME} PUBLIC m)
        endif()
    endif()
    if(HH_EMBED_RESOURCES)
        # Make sure the generator runs before the library compiles. The
        # The generated source already implies the dependency, but binding
        # the custom target makes it explicit and also picks up the Windows
        # .rc and header consumed by the executable below.
        add_dependencies(${LIBNAME} hh_generate_embedded_resources)
    endif()

    # miniaudio is a single-header library (~75k SLOC). Compiled with
    # /W4 it emits hundreds of harmless C4505 "unused static function"
    # warnings (because we disable several decoders/encoders via the
    # MA_NO_* knobs in src/engine/AudioPlayer.cpp). We can't fix those
    # without forking the header, so we silence /W4 for just that one
    # TU instead of leaking warning-suppression flags into game code.
    set(_audio_src "${HouseHaunters_SOURCE_DIR}/src/engine/AudioPlayer.cpp")
    if(EXISTS "${_audio_src}")
        if(MSVC)
            # /W3 + targeted /wd silences C4505 (unused static fn) and
            # the other categories miniaudio trips on its own internals.
            set_source_files_properties("${_audio_src}" PROPERTIES
                COMPILE_FLAGS "/W3 /wd4505 /wd4244 /wd4245 /wd4456 /wd4457 /wd4459 /wd4701 /wd4702 /wd4706")
        else()
            # GCC/Clang -- disable all warnings for this one upstream TU.
            set_source_files_properties("${_audio_src}" PROPERTIES
                COMPILE_FLAGS "-w")
        endif()
    endif()
endif()

# Executables: every .cpp under bin/ becomes its own executable.
foreach(EXEC ${EXECLIST})
    get_filename_component(EXECNAME ${EXEC} NAME_WE)
    set(_exec_sources ${EXEC})
    if(HH_EMBED_RESOURCES AND WIN32)
        # The generated .rc is compiled by MSVC's rc.exe and produces
        # an .res object that the linker folds into the executable's
        # resource section. It has to be on the EXECUTABLE target, not
        # the static library, because static .lib files don't carry
        # resource data.
        list(APPEND _exec_sources "${_gen_rc}")
    endif()
    if(APPLE)
        add_executable(${EXECNAME} MACOSX_BUNDLE ${_exec_sources})
        set_target_properties(${EXECNAME} PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "House Haunters"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.hydure.house-haunters"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
    else()
        add_executable(${EXECNAME} ${_exec_sources})
    endif()
    if(HH_EMBED_RESOURCES)
        add_dependencies(${EXECNAME} hh_generate_embedded_resources)
    endif()
    target_link_libraries(${EXECNAME} PRIVATE hh_compile_options ${SFML_LIBRARIES})
    if(SRC)
        target_link_libraries(${EXECNAME} PRIVATE ${LIBNAME})
    endif()
    message("-- Adding executable: ${EXECNAME}")
endforeach()

#######################
# Tests (CTest)       #
#######################
# Standard CMake idiom: BUILD_TESTING defaults ON when this project is the
# top-level build, OFF when consumed as a subdirectory. Setting it to OFF
# (e.g. -DBUILD_TESTING=OFF) skips the test suite entirely so distro
# builds don't have to compile or run them.
include(CTest)
if(BUILD_TESTING AND EXISTS "${HouseHaunters_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
