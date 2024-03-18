add_library(interpreter_lib 
    STATIC
    chunk.cpp
    value.cpp
)

target_compile_features(interpreter_lib PUBLIC cxx_std_23)

target_include_directories(interpreter_lib PUBLIC .)