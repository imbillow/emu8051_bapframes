cmake_minimum_required(VERSION 3.12)
project(emu8051)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSES ncurses REQUIRED)

add_subdirectory(libtrace)

add_executable(emu
        emu.c
        core.c
        disasm.c
        logicboard.c
        mainview.c
        memeditor.c
        opcodes.c
        options.c
        popups.c
        emu8051.h
        emulator.h
        trace.cpp trace.h)

target_link_libraries(emu PRIVATE trace ${NCURSES_LIBRARIES})
target_include_directories(emu PRIVATE libtrace)
