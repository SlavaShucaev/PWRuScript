cmake_minimum_required(VERSION 3.16)
project(PWRuScript LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pwru_script
    scr/main.cpp
    scr/locator.cpp
    scr/installer.cpp
)
