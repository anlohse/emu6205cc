cmake_minimum_required(VERSION 3.16)
project(emu6502)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads (needed for clock and debugger)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# Include directories
# Expose 'include' directory to targets
include_directories(include)

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

# Public include directories for the library users
target_include_directories(emu6502_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link Threads
target_link_libraries(emu6502_lib PUBLIC Threads::Threads)

# ------------------------------------------------------------------------------
# Debugger (Windows Only)
# ------------------------------------------------------------------------------
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
    
    # Ensure resource compiler can find headers
    target_include_directories(emu6502_debugger PRIVATE debugger_src)
else()
    # Linux / TUI Debugger using ncurses
    find_package(Curses REQUIRED)
    include_directories(${CURSES_INCLUDE_DIR})

    set(DEBUGGER_SOURCES
        debugger_src/test_6502_linux.cpp
    )

    add_executable(emu6502_debugger 
        ${DEBUGGER_SOURCES}
    )

    target_link_libraries(emu6502_debugger PRIVATE emu6502_lib ${CURSES_LIBRARIES})
endif()

include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
FetchContent_MakeAvailable(doctest)

# ------------------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------------------
set(TEST_SOURCES
    test_src/testAdc.cpp
    test_src/testAsm.cpp
    test_src/testMain.cpp
)

add_executable(emu6502_test ${TEST_SOURCES})

target_link_libraries(emu6502_test PRIVATE emu6502_lib doctest::doctest)
