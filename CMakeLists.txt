cmake_minimum_required(VERSION 4.2)
project(Native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(UNP_HEADERS
    include/unp.h
    include/unp/address.h
    include/unp/detail/socket_error.h
    include/unp/fs/file.h
    include/unp/io.h
    include/unp/operations.h
    include/unp/socket.h
)

set(PROCESS_COMPAT_HEADERS
    include/unp/detail/linux_process.h
    include/unp/detail/windows_process.h
    include/unp/sys/exec.h
    include/unp/sys/process.h
)

add_library(unp INTERFACE)
target_include_directories(unp INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(unp INTERFACE cxx_std_23)
target_sources(unp INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES ${UNP_HEADERS}
)

add_library(process_compat INTERFACE)
target_include_directories(process_compat INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_features(process_compat INTERFACE cxx_std_23)
target_sources(process_compat INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES ${PROCESS_COMPAT_HEADERS}
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
    function(add_header_self_containment_target TARGET_NAME LIBRARY_TARGET)
        set(HEADER_CHECK_SOURCES)
        set(HEADER_CHECK_DIRECTORY
            ${CMAKE_CURRENT_BINARY_DIR}/header_checks/${TARGET_NAME}
        )
        file(MAKE_DIRECTORY ${HEADER_CHECK_DIRECTORY})

        foreach(HEADER_PATH IN LISTS ARGN)
            file(RELATIVE_PATH HEADER_INCLUDE_PATH
                ${PROJECT_SOURCE_DIR}/include
                ${PROJECT_SOURCE_DIR}/${HEADER_PATH}
            )
            string(MAKE_C_IDENTIFIER ${HEADER_INCLUDE_PATH} HEADER_ID)
            set(HEADER_CHECK_SOURCE
                ${HEADER_CHECK_DIRECTORY}/${HEADER_ID}.cpp
            )
            file(GENERATE
                OUTPUT ${HEADER_CHECK_SOURCE}
                CONTENT "#include \"${HEADER_INCLUDE_PATH}\"\n"
            )
            list(APPEND HEADER_CHECK_SOURCES ${HEADER_CHECK_SOURCE})
        endforeach()

        add_library(${TARGET_NAME} OBJECT ${HEADER_CHECK_SOURCES})
        target_link_libraries(${TARGET_NAME} PRIVATE ${LIBRARY_TARGET})
        target_compile_options(${TARGET_NAME} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
        )
    endfunction()

    add_header_self_containment_target(
        unp_header_self_containment
        unp
        ${UNP_HEADERS}
    )
    add_header_self_containment_target(
        process_compat_header_self_containment
        process_compat
        ${PROCESS_COMPAT_HEADERS}
    )

    add_executable(unp_tests tests/test_unp.cpp)
    target_link_libraries(unp_tests PRIVATE unp)
    target_compile_options(unp_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    add_dependencies(unp_tests unp_header_self_containment)

    add_test(NAME unp_tests COMMAND unp_tests)

    add_executable(process_tests tests/test_process.cpp)
    target_link_libraries(process_tests PRIVATE process_compat)
    target_compile_options(process_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    add_dependencies(
        process_tests
        process_compat_header_self_containment
    )

    add_test(NAME process_tests COMMAND process_tests)
endif()
