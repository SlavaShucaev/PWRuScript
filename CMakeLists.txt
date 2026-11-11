cmake_minimum_required(VERSION 3.16)
project(PWRuScript LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(pwru_script
    scr/main.cpp
    scr/locator.cpp
    scr/installer.cpp
)

target_include_directories(pwru_script PRIVATE scr)
target_link_libraries(pwru_script PRIVATE CURL::libcurl Threads::Threads)
