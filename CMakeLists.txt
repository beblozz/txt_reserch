cmake_minimum_required(VERSION 3.10)
project(txt_search)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(search_core STATIC
    src/index.cpp
    src/tokenizer.cpp
)

add_executable(txt_search
    src/main.cpp
    src/server.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(txt_search search_core Threads::Threads)

if(WIN32)
    target_link_libraries(txt_search ws2_32)
endif()

# Чтобы .exe запускался без DLL от MinGW
if(MINGW)
    target_link_options(txt_search PRIVATE -static)
endif()

enable_testing()
add_executable(tests tests/tests.cpp)
target_link_libraries(tests search_core)
add_test(NAME tests COMMAND tests)
