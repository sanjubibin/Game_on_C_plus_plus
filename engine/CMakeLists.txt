# Genre-agnostic engine layer: window, main loop, scene management.
add_library(engine STATIC
    src/Application.cpp
    src/SceneManager.cpp
    src/Ui.cpp
)

target_include_directories(engine PUBLIC include)
target_link_libraries(engine PUBLIC raylib)
fun_set_warnings(engine)
