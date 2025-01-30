cmake_minimum_required(VERSION 3.31)

add_subdirectory("third_party/glad")
add_subdirectory("third_party/stb")
add_subdirectory("third_party/freetype")

file(GLOB_RECURSE SOURCE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_library(engine ${SOURCE})

# Make __FILE__ macro return a relative path.
foreach(f IN LISTS SOURCE)
    file(RELATIVE_PATH b ${CMAKE_SOURCE_DIR} ${f})
    set_source_files_properties(${f} PROPERTIES COMPILE_DEFINITIONS "__FILE__=\"${b}\"")
endforeach()

set_target_properties(engine
    PROPERTIES
    C_STANDARD "99"
    C_STANDARD_REQUIRED true
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    COMPILE_FLAGS "-Wall -Wextra -Wno-builtin-macro-redefined"
)

target_include_directories(engine
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/"
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/waddle/"
)

target_link_libraries(engine glfw glad m freetype stb)

if (EMSCRIPTEN)
    execute_process(COMMAND emcc --cflags OUTPUT_VARIABLE EM_CFLAGS)
    get_target_property(CFLAGS ${CMAKE_PROJECT_NAME} COMPILE_FLAGS)
    set_target_properties(engine
        PROPERTIES
        COMPILE_FLAGS "${CFLAGS} ${EM_CFLAGS}"
    )
    target_link_options(${CMAKE_PROJECT_NAME}
        PRIVATE "-sALLOW_MEMORY_GROWTH=1"
    )
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif ()
