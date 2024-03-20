add_library(interpreter_lib 
    STATIC
    chunk.cpp
    value.cpp
    vm.cpp
    scanner.cpp
    compiler.cpp
    parser.cpp
    object.cpp
)

target_compile_features(interpreter_lib PUBLIC cxx_std_23)

target_include_directories(interpreter_lib PUBLIC .)

target_compile_definitions(interpreter_lib PRIVATE "$<$<CONFIG:Debug>:DEBUG_TRACE_EXECUTION>")

include(FetchContent)

set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL "")

FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20240116.1 # LTS 16/01/2024
)

FetchContent_MakeAvailable(abseil)

target_link_libraries(interpreter_lib absl::flat_hash_map)
