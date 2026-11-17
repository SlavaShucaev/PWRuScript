cmake_minimum_required(VERSION 3.16)
project(pwru_script LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# libarchive: пробуем новый интерфейс (CMake >= 3.17), иначе pkg-config
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.17")
    find_package(LibArchive REQUIRED)
    set(PWRU_ARCHIVE_LIB LibArchive::LibArchive)
    set(PWRU_ARCHIVE_INC "")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
    set(PWRU_ARCHIVE_LIB ${LIBARCHIVE_LIBRARIES})
    set(PWRU_ARCHIVE_INC ${LIBARCHIVE_INCLUDE_DIRS})
endif()

add_executable(pwru_script
    scr/main.cpp
    scr/locator.cpp
    scr/installer.cpp
)

target_include_directories(pwru_script PRIVATE scr ${PWRU_ARCHIVE_INC})
target_link_libraries(pwru_script PRIVATE
    CURL::libcurl
    Threads::Threads
    ${PWRU_ARCHIVE_LIB}
)

# Тесты чистой логики (суммы, параметр запуска Proton, старая правка exe): без сети и файлов игры.
#   cmake -DPWRU_BUILD_TESTS=ON .. && cmake --build . && ctest --output-on-failure
option(PWRU_BUILD_TESTS "Собрать тесты логики (pwru_tests)" OFF)
if(PWRU_BUILD_TESTS)
    enable_testing()
    add_executable(pwru_tests tests/test_logic.cpp)
    target_include_directories(pwru_tests PRIVATE scr)
    add_test(NAME pwru_tests COMMAND pwru_tests)
endif()
