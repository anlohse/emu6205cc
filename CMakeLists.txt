cmake_minimum_required(VERSION 3.16)
project(emu6502)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When this project is pulled in with add_subdirectory() -- as the NES emulator
# does -- only the library is wanted by default. Building the debugger and the
# 30M-instruction conformance suite as a side effect of someone else's build is
# rarely what they asked for.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(EMU6502_IS_TOP_LEVEL ON)
else()
    set(EMU6502_IS_TOP_LEVEL OFF)
endif()

option(EMU6502_BUILD_TESTS    "Build the emu6502 test suite"         ${EMU6502_IS_TOP_LEVEL})
option(EMU6502_BUILD_DEBUGGER "Build the emu6502 debugger front-end" ${EMU6502_IS_TOP_LEVEL})

# Find Threads (needed for clock and debugger)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

include(FetchContent)

# ------------------------------------------------------------------------------
# Common Shared Library
# ------------------------------------------------------------------------------
set(COMMON_SOURCES
    src/Asm.cpp
    src/emu_bus.cpp
    src/emuclock.cpp
    src/I6502Emulator.cpp
    src/Memory.cpp
    src/Processor.cpp
    src/unasm.cpp
    # Headers
    src/InstructionImpl.h
    src/Opcode.h
    src/parameters.h
)

add_library(emu6502_lib SHARED ${COMMON_SOURCES})
add_library(emu6502::emu6502 ALIAS emu6502_lib)

# MSVC emits no import library for a SHARED target unless something is exported.
# Exporting everything keeps the headers free of dllexport macros.
set_target_properties(emu6502_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Public include directories for the library users
target_include_directories(emu6502_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link Threads
target_link_libraries(emu6502_lib PUBLIC Threads::Threads)

# ------------------------------------------------------------------------------
# Klaus Dormann's 6502 functional test suite
# ------------------------------------------------------------------------------
# Downloaded at configure time rather than vendored: it is GPL-3.0-or-later,
# and this repository does not want to take on redistribution obligations for
# it. Nothing from it is linked or compiled -- the emulator only executes the
# prebuilt image as data, which does not make this project a derivative work.
#
#   https://github.com/Klaus2m5/6502_65C02_functional_tests
#   Copyright (C) 2012-2020 Klaus Dormann, GPL-3.0-or-later
#
# Turn EMU6502_FETCH_FUNCTIONAL_TESTS off to build without network access; the
# functional test then reports itself as skipped instead of failing.
#
# Resolved before the debugger and test targets, because both consume the path.
option(EMU6502_FETCH_FUNCTIONAL_TESTS
    "Download Klaus Dormann's 6502 functional tests (GPL-3.0) at configure time" ON)

set(EMU6502_TEST_DATA_DIR "" CACHE PATH
    "Directory containing 6502_functional_test.bin; overrides the download")

if(EMU6502_BUILD_TESTS OR EMU6502_BUILD_DEBUGGER)
    if(EMU6502_FETCH_FUNCTIONAL_TESTS AND NOT EMU6502_TEST_DATA_DIR)
        FetchContent_Declare(
            klaus_6502_tests
            GIT_REPOSITORY https://github.com/Klaus2m5/6502_65C02_functional_tests.git
            # Pinned: the success-trap address, instruction count and cycle count
            # asserted by testFunctional.cpp are properties of this exact image.
            GIT_TAG 7954e2dbb49c469ea286070bf46cdd71aeb29e4b
        )
        # The repository has no CMakeLists.txt, so this only populates the source
        # directory -- add_subdirectory() is not called and nothing is built.
        FetchContent_MakeAvailable(klaus_6502_tests)
        set(EMU6502_TEST_DATA_DIR "${klaus_6502_tests_SOURCE_DIR}/bin_files")
    endif()

    if(NOT EMU6502_TEST_DATA_DIR)
        message(STATUS
            "6502 functional test not available; that test case will be skipped. "
            "Enable EMU6502_FETCH_FUNCTIONAL_TESTS or set EMU6502_TEST_DATA_DIR.")
    endif()
endif()

# ------------------------------------------------------------------------------
# Debugger (Win32 GUI, or ncurses TUI elsewhere)
# ------------------------------------------------------------------------------
if(EMU6502_BUILD_DEBUGGER)
    if(WIN32)
        set(DEBUGGER_SOURCES
            debugger_src/test_6502.cpp
            debugger_src/resource.h
        )

        set(DEBUGGER_RC
            debugger_src/Resource.rc
        )

        set(DEBUGGER_MANIFEST
            debugger_src/Application.manifest
        )

        add_executable(emu6502_debugger WIN32
            ${DEBUGGER_SOURCES}
            ${DEBUGGER_RC}
            ${DEBUGGER_MANIFEST}
        )

        # Link against the common library and Windows Common Controls
        target_link_libraries(emu6502_debugger PRIVATE emu6502_lib comctl32)

        # Resource.rc already embeds Application.manifest as RT_MANIFEST. Letting the
        # linker generate one as well produces CVT1100 (duplicate resource) / LNK1123.
        if(MSVC)
            target_link_options(emu6502_debugger PRIVATE /MANIFEST:NO)
        endif()

        # Ensure resource compiler can find headers
        target_include_directories(emu6502_debugger PRIVATE debugger_src)
    else()
        # Linux / TUI Debugger using ncurses
        find_package(Curses REQUIRED)

        add_executable(emu6502_debugger
            debugger_src/test_6502_linux.cpp
        )

        target_include_directories(emu6502_debugger PRIVATE ${CURSES_INCLUDE_DIRS})
        target_link_libraries(emu6502_debugger PRIVATE emu6502_lib ${CURSES_LIBRARIES})
    endif()

    # Preload the functional test image on startup, when one is configured.
    if(EMU6502_TEST_DATA_DIR)
        target_compile_definitions(emu6502_debugger PRIVATE
            EMU6502_TEST_DATA_DIR="${EMU6502_TEST_DATA_DIR}"
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------------------
if(EMU6502_BUILD_TESTS)
    if(NOT TARGET doctest::doctest)
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.12
        )
        FetchContent_MakeAvailable(doctest)
    endif()

    enable_testing()

    set(TEST_SOURCES
        test_src/testAdc.cpp
        test_src/testAsm.cpp
        test_src/testCpu.cpp
        test_src/testFunctional.cpp
        test_src/testMain.cpp
    )

    add_executable(emu6502_test ${TEST_SOURCES})

    target_link_libraries(emu6502_test PRIVATE emu6502_lib doctest::doctest)

    # Baked in as an absolute path so the suite passes from any working directory.
    # When it is empty, testFunctional.cpp reports the functional test as skipped.
    if(EMU6502_TEST_DATA_DIR)
        target_compile_definitions(emu6502_test PRIVATE
            EMU6502_TEST_DATA_DIR="${EMU6502_TEST_DATA_DIR}"
        )
    endif()

    add_test(NAME emu6502_test COMMAND emu6502_test)
endif()
