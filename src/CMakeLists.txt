add_library(interpreter_lib 
    STATIC
    chunk.cpp
    value.cpp
    vm.cpp
    scanner.cpp
    compiler.cpp
    parser.cpp
)

target_compile_features(interpreter_lib PUBLIC cxx_std_23)

target_include_directories(interpreter_lib PUBLIC .)

target_compile_definitions(interpreter_lib PRIVATE "$<$<CONFIG:Debug>:DEBUG_TRACE_EXECUTION>")