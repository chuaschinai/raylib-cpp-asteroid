cmake_minimum_required(VERSION 3.5)

set(RAYLIB_DIR ${CMAKE_SOURCE_DIR}/raylib)

project(raylib-asteroid)

set(CMAKE_CXX_STANDARD 11)

include_directories(${RAYLIB_DIR}/include)

link_directories(${RAYLIB_DIR}/lib)

add_executable(Asteroid src/main.cpp)

# link raylibdll.lib
target_link_libraries(Asteroid libraylibdll.a winmm)

# copy Asteroid and raylib.dll to ./bin folder after build 
set_target_properties(Asteroid PROPERTIES WIN32_EXECUTABLE TRUE RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
add_custom_command(TARGET Asteroid POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${RAYLIB_DIR}/lib/raylib.dll
    ${CMAKE_SOURCE_DIR}/bin)

# copy .wav files to ./bin folder after build
file(GLOB WAV_FILES ${CMAKE_SOURCE_DIR}/src/*.wav)
foreach(file ${WAV_FILES})
    add_custom_command(TARGET Asteroid POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${file}
        ${CMAKE_SOURCE_DIR}/bin)
endforeach()
