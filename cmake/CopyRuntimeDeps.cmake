# Script mode: cmake -DEXE=<exe> -DSEARCH_DIR=<dir> -DOBJDUMP=<objdump> -P CopyRuntimeDeps.cmake
# Copies every non-Windows DLL the exe needs (directly or transitively) from SEARCH_DIR next to it.
cmake_minimum_required(VERSION 3.25...4.4)

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXE}"
    RESOLVED_DEPENDENCIES_VAR deps
    UNRESOLVED_DEPENDENCIES_VAR missing
    DIRECTORIES "${SEARCH_DIR}"
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*\\.dll"
)

if(missing)
    message(WARNING "Unresolved runtime DLLs: ${missing}")
endif()

get_filename_component(exe_dir "${EXE}" DIRECTORY)
foreach(dll IN LISTS deps)
    file(COPY "${dll}" DESTINATION "${exe_dir}")
endforeach()
