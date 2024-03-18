cmake_minimum_required(VERSION 3.28)

project(lox_bytecode_interpreter CXX)

set(CMAKE_CXX_STANDARD_REQUIRED 23)

add_executable(interpreter)

add_subdirectory(src)

target_link_libraries(interpreter interpreter_lib)
target_sources(interpreter PRIVATE src/main.cpp)

install(TARGETS interpreter DESTINATION bin)
install(DIRECTORY examples DESTINATION .)