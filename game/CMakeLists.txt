# Game rules with no rendering: state, content loading, rules, shop simulation, saves.
# Shared by the game and the unit tests.
add_library(game_logic STATIC
    src/logic/Content.cpp
    src/logic/Encounter.cpp
    src/logic/GameState.cpp
    src/logic/Rules.cpp
    src/logic/SaveGame.cpp
    src/logic/Selling.cpp
    src/logic/ShopSim.cpp
)

target_include_directories(game_logic PUBLIC src)
target_link_libraries(game_logic PUBLIC jsoncpp_static)
fun_set_warnings(game_logic)

add_executable(AAAGame
    src/main.cpp
    src/Flow.cpp
    src/Visuals.cpp
    src/scenes/ChapterEndScene.cpp
    src/scenes/DaySummaryScene.cpp
    src/scenes/DialogueScene.cpp
    src/scenes/EncounterScene.cpp
    src/scenes/JournalScene.cpp
    src/scenes/PauseScene.cpp
    src/scenes/SellScene.cpp
    src/scenes/SupplierScene.cpp
    src/scenes/TitleScene.cpp
    src/scenes/WorldScene.cpp
)

target_link_libraries(AAAGame PRIVATE engine game_logic)
fun_set_warnings(AAAGame)

if(MINGW)
    # Bake in the GCC runtime so fewer MSYS2 DLLs are needed next to the exe.
    target_link_options(AAAGame PRIVATE -static-libgcc -static-libstdc++)
endif()

get_filename_component(FUN_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

# Copy every DLL the exe needs (raylib, glfw, ...) and the assets folder next to it,
# so it also runs outside the MSYS2 shell.
add_custom_command(TARGET AAAGame POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:AAAGame>
            -DSEARCH_DIR=${FUN_TOOLCHAIN_BIN}
            -DOBJDUMP=${CMAKE_OBJDUMP}
            -P ${PROJECT_SOURCE_DIR}/cmake/CopyRuntimeDeps.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:AAAGame>/assets
)

# `cmake --install build/release --prefix dist` produces a standalone, shareable folder.
install(TARGETS AAAGame
    RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*\\.dll"
        DIRECTORIES "${FUN_TOOLCHAIN_BIN}"
    RUNTIME DESTINATION .
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION assets)
