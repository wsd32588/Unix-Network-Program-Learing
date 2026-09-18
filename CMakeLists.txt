cmake_minimum_required(VERSION 4.2)
project(Native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(unp INTERFACE)
target_include_directories(unp INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_sources(unp INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES include/unp/sys/process.h
)
if(MSVC)
    add_compile_options(/std:c++latest)
endif()

if(WIN32)
    target_link_libraries(unp INTERFACE ws2_32)
endif()

add_executable(Native src/main.cpp)
target_link_libraries(Native PRIVATE unp)

target_compile_options(Native PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

include(CTest)

if(BUILD_TESTING)
    add_executable(unp_tests tests/test_unp.cpp)
    target_link_libraries(unp_tests PRIVATE unp)
    target_compile_options(unp_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )

    add_test(NAME unp_tests COMMAND unp_tests)

    add_executable(process_tests tests/test_process.cpp)
    target_link_libraries(process_tests PRIVATE unp)
    target_compile_options(process_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )

    add_test(NAME process_tests COMMAND process_tests)
endif()
