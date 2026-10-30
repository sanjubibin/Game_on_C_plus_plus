# fun_set_warnings(<target>): strict warnings for our own code (third-party headers are -isystem).
function(fun_set_warnings target)
    if(MSVC)
        set(warnings /W4 /permissive-)
        if(FUN_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    else()
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
        )
        if(FUN_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    endif()
    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
