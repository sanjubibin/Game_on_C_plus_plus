add_executable(logic_tests logic_tests.cpp)
target_link_libraries(logic_tests PRIVATE game_logic)
target_compile_definitions(logic_tests PRIVATE FUN_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
fun_set_warnings(logic_tests)

if(MINGW)
    target_link_options(logic_tests PRIVATE -static-libgcc -static-libstdc++)
endif()

add_test(NAME logic_tests COMMAND logic_tests)
