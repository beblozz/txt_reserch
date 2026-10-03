cmake_minimum_required(VERSION 3.10)
project(txt_search)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(txt_search
    src/main.cpp
    src/index.cpp
    src/tokenizer.cpp
    src/server.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(txt_search Threads::Threads)

if(WIN32)
    target_link_libraries(txt_search ws2_32)
endif()
