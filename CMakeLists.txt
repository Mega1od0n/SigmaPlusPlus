cmake_minimum_required(VERSION 3.24)
project(SigmaPlusPlus CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ASMJIT_STATIC TRUE)
add_subdirectory(extern/asmjit)

add_executable(SigmaPlusPlus
        src/main.cpp
        src/bytecode.cpp  src/bytecode.h
        src/vm.cpp        src/vm.h
        src/runtime.cpp   src/runtime.h
        src/gc.cpp        src/gc.h
        src/jit.cpp       src/jit.h
        src/lexer.cpp     src/lexer.h
        src/ast.cpp       src/ast.h
        src/parser.cpp    src/parser.h
)

if (MSVC)
    target_compile_options(SigmaPlusPlus PRIVATE /W4)
else()
    target_compile_options(SigmaPlusPlus PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_include_directories(SigmaPlusPlus PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(SigmaPlusPlus SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/asmjit/src
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/jit.cpp PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
elseif (MSVC)
    set_source_files_properties(src/jit.cpp PROPERTIES COMPILE_OPTIONS "/wd4201")
endif()
target_link_libraries(SigmaPlusPlus asmjit::asmjit)
